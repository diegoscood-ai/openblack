/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDeeds.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFriendship.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureWatching.h"
#include "CreatureSpawner.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureClearAreaReadout.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreaturePartner.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using creature_desires::Desire;
using openblack::ecs::components::ClearCellBlock;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLearningOff;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureReaction;
using openblack::ecs::components::CreatureReactionsOff;
using openblack::ecs::components::StoragePit;
using openblack::ecs::components::Transform;

namespace
{
std::string ActionName(const creature_mind_tables::Tables* tables, uint32_t action)
{
	return tables != nullptr && action < tables->actions.size() ? tables->actions[action].name : fmt::format("#{}", action);
}

/// The other creature nearest to the creature, if there is one
std::optional<entt::entity> NearestOtherCreature(const ecs::Registry& registry, entt::entity creature)
{
	const auto* at = registry.TryGet<const Transform>(creature);
	if (at == nullptr)
	{
		return std::nullopt;
	}
	std::optional<entt::entity> nearest;
	float nearestSquared = std::numeric_limits<float>::max();
	registry.Each<const Creature, const Transform>([&](entt::entity other, const Creature&, const Transform& otherAt) {
		const float dx = otherAt.position.x - at->position.x;
		const float dz = otherAt.position.z - at->position.z;
		if (other != creature && (dx * dx) + (dz * dz) < nearestSquared)
		{
			nearestSquared = (dx * dx) + (dz * dz);
			nearest = other;
		}
	});
	return nearest;
}

/// The creature casts a spell to amuse the other creature nearest to it, as a plan for the desire the game's row of it
/// serves, whether or not that creature seems friendly
std::string AmuseNearest(const ecs::Registry& registry, ecs::systems::CreatureMindSystemInterface& minds, entt::entity creature)
{
	constexpr std::string_view k_Amusing = "CastAmusingSpellOnCreature";
	const auto other = NearestOtherCreature(registry, creature);
	const auto* tables = minds.GetTables();
	const auto action = tables != nullptr ? creature_mind_tables::FindAction(*tables, k_Amusing) : std::nullopt;
	if (!other.has_value() || !action.has_value() || tables->actions[*action].desire >= creature_desires::k_DesireCount)
	{
		return !other.has_value() ? "No other creature" : "The game's tables have no such row";
	}
	const bool forced = minds.ForceActivity(
	    creature,
	    {.desire = static_cast<Desire>(tables->actions[*action].desire), .action = k_Amusing, .actionObject = *other});
	return forced ? fmt::format("Amusing creature {}", entt::to_integral(*other))
	              : std::string("It could not take up the spell to amuse");
}

/// The creature casts the town's shield at the built storage pit nearest to it, as a plan about that pit: the debug
/// tools' cast of the shield goes to the shield's own row, and a script's plan is about nothing, which idles
std::string CastTownShield(const ecs::Registry& registry, ecs::systems::CreatureMindSystemInterface& minds,
                           entt::entity creature)
{
	const auto* at = registry.TryGet<const Transform>(creature);
	if (at == nullptr)
	{
		return "It is nowhere";
	}
	std::optional<entt::entity> nearest;
	float nearestSquared = std::numeric_limits<float>::max();
	registry.Each<const StoragePit, const Transform>([&](entt::entity pit, const StoragePit&, const Transform& pitAt) {
		const float dx = pitAt.position.x - at->position.x;
		const float dz = pitAt.position.z - at->position.z;
		if (ecs::abodes::IsBuilt(pit) && (dx * dx) + (dz * dz) < nearestSquared)
		{
			nearestSquared = (dx * dx) + (dz * dz);
			nearest = pit;
		}
	});
	if (!nearest.has_value())
	{
		return "No built storage pit";
	}
	const bool forced = minds.ForceActivity(
	    creature,
	    {.desire = Desire::ObeyPlayer, .activityObject = *nearest, .action = "CastShieldAroundTown", .actionObject = *nearest});
	return forced ? fmt::format("Casting the town's shield at pit {}", entt::to_integral(*nearest))
	              : std::string("It could not take up the town's shield");
}

/// Why a cell of a clear-area search was not clear, in words
const char* BlockText(ClearCellBlock why)
{
	switch (why)
	{
	case ClearCellBlock::NotLand:
		return "not land it may stand on";
	case ClearCellBlock::TempleHeart:
		return "a temple's heart";
	case ClearCellBlock::Shape:
		return "the shape it keeps clear";
	case ClearCellBlock::Living:
		return "a living thing";
	}
	return "?";
}

/// The kind of thing that keeps a cell from being clear, by its components
const char* ThingKind(const ecs::Registry& registry, entt::entity thing)
{
	namespace c = openblack::ecs::components;
	if (thing == entt::null || !registry.Valid(thing))
	{
		return "nothing";
	}
	const std::array<std::pair<bool, const char*>, 16> kinds {{
	    {registry.AllOf<c::Temple>(thing), "temple"},
	    {registry.AllOf<c::DeadTree>(thing), "dead tree"},
	    {registry.AllOf<c::Tree>(thing), "tree"},
	    {registry.AllOf<c::Field>(thing), "field"},
	    {registry.AllOf<c::Abode>(thing), "abode"},
	    {registry.AllOf<c::Feature>(thing), "feature"},
	    {registry.AllOf<c::AnimatedStatic>(thing), "animated static"},
	    {registry.AllOf<c::MobileStatic>(thing), "mobile static"},
	    {registry.AllOf<c::Fragment>(thing), "fragment"},
	    {registry.AllOf<c::BigForest>(thing), "big forest"},
	    {registry.AllOf<c::MobileObject>(thing), "mobile object"},
	    {registry.AllOf<c::StreetLantern>(thing), "street lantern"},
	    {registry.AllOf<c::Pot>(thing), "pot"},
	    {registry.AllOf<c::Villager>(thing), "villager"},
	    {registry.AllOf<c::Animal>(thing), "animal"},
	    {registry.AllOf<c::Creature>(thing), "creature"},
	}};
	const auto kind = std::ranges::find_if(kinds, [](const auto& entry) { return entry.first; });
	return kind != kinds.end() ? kind->second : "thing";
}
} // namespace

void CreatureSpawner::DrawLearning(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* mind = Find<CreatureMindState>(registry, entity);
	if (mind == nullptr || !Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();
	const auto* tables = minds.GetTables();

	ImGui::SeparatorText("Learning mind");
	if (!mind->learnt.has_value() || !mind->desires.has_value())
	{
		ImGui::TextUnformatted("The mind hasn't thought yet");
		return;
	}
	auto& learnt = *mind->learnt;
	const auto& desires = *mind->desires;
	ImGui::Text("Name \"%s\", stage %u, attitude to the player %+.2f", creature_spawner::Narrow(learnt.name).c_str(),
	            mind->developmentPhase, static_cast<double>(mind->attitudeToPlayer));
	if (learnt.file != nullptr)
	{
		ImGui::Text("From a version %u mind file saved as \"%s\" by \"%s\"", learnt.file->version,
		            learnt.file->SavedAsText().c_str(), creature_spawner::Narrow(learnt.file->ProfileText()).c_str());
	}

	if (ImGui::TreeNodeEx("Mind files", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button("Clear learning"))
		{
			minds.ClearLearning(entity);
			_lastMindFile = "Learning cleared";
		}
		DrawMindFiles(true);
		ImGui::TreePop();
	}

	{
		// The trainer strokes it for each thing it does to the kind of thing picked, and slaps it for anything else
		constexpr std::array<std::pair<const char*, std::optional<uint32_t>>, 6> k_Trainers {{
		    {"No trainer", std::nullopt},
		    {"Reward villagers", creature_tree::belief_types::k_Villager},
		    {"Reward objects", creature_tree::belief_types::k_Other},
		    {"Reward trees", creature_tree::belief_types::k_Tree},
		    {"Reward creatures", creature_tree::belief_types::k_Creature},
		    {"Reward abodes", creature_tree::belief_types::k_Abode},
		}};
		const auto current =
		    std::ranges::find(k_Trainers, mind->trainer, &std::pair<const char*, std::optional<uint32_t>>::second);
		if (ImGui::BeginCombo("Trainer", current != k_Trainers.end() ? current->first : "Custom"))
		{
			for (const auto& [label, kind] : k_Trainers)
			{
				if (ImGui::Selectable(label, mind->trainer == kind))
				{
					mind->trainer = kind;
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Strokes the creature for each thing it does to that kind of thing and slaps it for anything "
			                  "else, as soon as it is done");
		}
	}
	ImGui::TextUnformatted("Feedback");
	ImGui::SameLine();
	if (ImGui::SmallButton("Stroke"))
	{
		minds.ReceiveFeedback(entity, 1.0f);
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Slap"))
	{
		minds.ReceiveFeedback(entity, -1.0f);
	}

	// Where a script holds its head: a point (SET_FOCUS) or a thing (SET_FOCUS_ON_OBJECT)
	if (mind->scriptFocus.has_value())
	{
		ImGui::Text("Script focus: the point (%.1f, %.1f, %.1f)", static_cast<double>(mind->scriptFocus->x),
		            static_cast<double>(mind->scriptFocus->y), static_cast<double>(mind->scriptFocus->z));
	}
	else if (mind->scriptFocusThing.has_value())
	{
		ImGui::Text("Script focus: entity %u", entt::to_integral(*mind->scriptFocusThing));
	}
	if (ImGui::TreeNodeEx("Planner", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const auto& planner = mind->planner;
		if (planner.current.has_value())
		{
			ImGui::Text("Carrying out %s for %s, priority %.1f", ActionName(tables, planner.current->action).c_str(),
			            creature_desires::Name(planner.current->desire).data(), static_cast<double>(planner.current->priority));
			// What the plan acts on: a thing, or the creature itself (as once an eating plan lets go of what it went
			// to); a plan that names nothing is done on the spot, to itself
			const auto& object = planner.current->object;
			if (!object.has_value())
			{
				ImGui::TextUnformatted("Acting on: itself (on the spot)");
			}
			else if (*object == static_cast<uint32_t>(entt::to_integral(entity)))
			{
				ImGui::TextUnformatted("Acting on: itself");
			}
			else
			{
				ImGui::Text("Acting on: thing %u", *object);
			}
		}
		else
		{
			ImGui::TextUnformatted("No plan: doing as it pleases with nothing better to do");
		}
		// What a script's GET_TARGET_OBJECT gives: the plan's thing, or the last plan's once it stopped
		if (const auto target = creature_planner::TargetObject(planner); target.has_value())
		{
			ImGui::Text("A script's target object: %u", static_cast<unsigned>(*target));
		}
		else
		{
			ImGui::TextUnformatted("A script's target object: none");
		}
		if (ImGui::BeginTable("plans", 5, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Desire");
			ImGui::TableSetupColumn("Value");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("Done to (goal, to act on)");
			ImGui::TableSetupColumn("Priority");
			ImGui::TableHeadersRow();
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& plan = planner.best.at(d);
				if (!plan.has_value())
				{
					continue;
				}
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(creature_desires::Name(plan->desire).data());
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", static_cast<double>(desires.desires.at(d).value));
				ImGui::TableNextColumn();
				ImGui::Text("%s (%.4f)", ActionName(tables, plan->action).c_str(), static_cast<double>(plan->actionPriority));
				ImGui::TableNextColumn();
				if (plan->object.has_value())
				{
					ImGui::Text("entity %u (%.2f, %.4f)", *plan->object, static_cast<double>(plan->goalUsefulness),
					            static_cast<double>(plan->objectUsefulness));
				}
				else
				{
					ImGui::Text("itself (%.2f)", static_cast<double>(plan->goalUsefulness));
				}
				ImGui::TableNextColumn();
				ImGui::Text("%.1f", static_cast<double>(plan->priority));
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}

	// The last search for a clear area, recorded only while the switch gives the creature the readout
	if (ImGui::TreeNode("Clear-area search"))
	{
		const auto* readout = std::as_const(registry).TryGet<const ecs::components::CreatureClearAreaReadout>(entity);
		bool recording = readout != nullptr;
		if (ImGui::Checkbox("Record its clear-area searches", &recording))
		{
			if (recording)
			{
				registry.AssignOrReplaceState<ecs::components::CreatureClearAreaReadout>(entity);
			}
			else
			{
				registry.RemoveState<ecs::components::CreatureClearAreaReadout>(entity);
			}
			readout = std::as_const(registry).TryGet<const ecs::components::CreatureClearAreaReadout>(entity);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Keeps the last search: the area's width, the point it looked about, what it found and "
			                  "what kept each cell of the window from being clear");
		}
		if (readout != nullptr && readout->last.has_value())
		{
			const auto& last = *readout->last;
			ImGui::Text("Turn %u: %.2f wide about (%.2f, %.2f)%s", last.turn, static_cast<double>(last.width),
			            static_cast<double>(last.point.x), static_cast<double>(last.point.y),
			            last.nothingLiving ? ", nothing living in it" : "");
			if (last.found.has_value())
			{
				ImGui::Text("Found: (%.4f, %.4f)", static_cast<double>(last.found->x), static_cast<double>(last.found->y));
			}
			else
			{
				ImGui::TextUnformatted("Found: none");
			}
			if (last.result != last.found)
			{
				ImGui::TextUnformatted("Taken: none, outside the area that keeps it");
			}
			ImGui::Text("Cells not clear: %zu", last.blocked.size());
			constexpr ImGuiTableFlags k_Flags =
			    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders;
			if (!last.blocked.empty() &&
			    ImGui::BeginTable("clear area cells", 4, k_Flags, ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 12.0f)))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn("Cell");
				ImGui::TableSetupColumn("Why");
				ImGui::TableSetupColumn("Entity");
				ImGui::TableSetupColumn("Kind");
				ImGui::TableHeadersRow();
				const auto& view = std::as_const(registry);
				for (const auto& [cell, blocker] : last.blocked)
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%d, %d", cell.x, cell.y);
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(BlockText(blocker.why));
					ImGui::TableNextColumn();
					if (blocker.by != entt::null)
					{
						ImGui::Text("%u", entt::to_integral(blocker.by));
					}
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(blocker.by != entt::null ? ThingKind(view, blocker.by) : "");
				}
				ImGui::EndTable();
			}
		}
		else if (readout != nullptr)
		{
			ImGui::TextUnformatted("No search since recording began");
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Desires and likes"))
	{
		for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
		{
			const auto& state = desires.desires.at(d);
			if (!state.activated)
			{
				continue;
			}
			const auto desire = static_cast<Desire>(d);
			const auto likes = creature_learning::LikesLevel(desire, state, learnt.initialThresholds.at(d));
			ImGui::Text("%-18s %.2f / %.2f, grows in %.0f s%s, likes it %s", creature_desires::Name(desire).data(),
			            static_cast<double>(state.value), static_cast<double>(state.max),
			            static_cast<double>(state.increaseSeconds), state.suppressedTurns > 0 ? ", held back" : "",
			            creature_learning::LikesWords(likes).data());
		}
		ImGui::Text("Wish to impress the villagers who need it: %.3f", static_cast<double>(desires.impressVillagers));
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Opinions of actions"))
	{
		for (size_t a = 0; a < learnt.opinions.size(); ++a)
		{
			if (learnt.opinions[a] != 0.0f)
			{
				ImGui::Text("%-32s %+.2f", ActionName(tables, static_cast<uint32_t>(a)).c_str(),
				            static_cast<double>(learnt.opinions[a]));
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Decision trees"))
	{
		for (size_t k = 0; k < creature_mind_model::k_TreeKinds; ++k)
		{
			for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
			{
				const auto& episodes = learnt.episodes.at(k).at(d);
				if (episodes.empty())
				{
					continue;
				}
				const auto label = fmt::format(
				    "{} - {}: {} examples##tree{}_{}", creature_desires::Name(static_cast<Desire>(d)),
				    k == static_cast<size_t>(creature_mind_model::TreeKind::Goal) ? "what a plan is for" : "what to act on",
				    episodes.size(), k, d);
				if (ImGui::TreeNode(label.c_str()))
				{
					for (const auto& line : creature_tree::Describe(learnt.trees.at(k).at(d)))
					{
						ImGui::TextUnformatted(line.c_str());
					}
					// what a mind file keeps with each example besides the thing and the feedback
					for (const auto& episode : episodes)
					{
						ImGui::Text("Example %+.2f: lesson kind %d, action %d, at (%d, %d)",
						            static_cast<double>(episode.feedback), episode.saved[0], episode.saved[1], episode.saved[2],
						            episode.saved[3]);
					}
					ImGui::TreePop();
				}
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNodeEx("Recent actions", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (mind->showDesireOnHold)
		{
			ImGui::TextUnformatted("Held: shows what it wants once it has turned to the player");
		}
		for (const auto& context : learnt.contexts | std::views::reverse)
		{
			const auto priority = creature_learning::LearningPriority(context);
			// A thing it knows of, or itself when the plan was done on the spot
			const bool onItself = creature_learning::OnItself(context, static_cast<uint32_t>(entt::to_integral(entity)));
			const char* on = context.belief.has_value() ? " on a " : (onItself ? " on itself" : "");
			ImGui::Text("%s for %s%s%s: %s, credit %.2f%s%s%s", ActionName(tables, context.action).c_str(),
			            creature_desires::Name(context.desire).data(), on,
			            context.belief.has_value() ? creature_tree::BeliefName(context.belief->type) : "",
			            context.running ? "running" : fmt::format("{:.0f} s ago", context.secondsSince).c_str(),
			            static_cast<double>(priority),
			            context.credited.has_value() ? fmt::format(", fed back {:+.2f}", *context.credited).c_str() : "",
			            context.ordered ? ", forced on it" : "",
			            context.source != creature_learning::k_NoTellingSource
			                ? fmt::format(", why: source {}", context.source).c_str()
			                : "");
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Lessons about things"))
	{
		// What the last feedback for each recent action taught about the thing it was for and the thing it was done to
		const auto lesson = [](const std::optional<float>& feedback) {
			return feedback.has_value() ? fmt::format("{:+.2f}", *feedback) : std::string("nothing");
		};
		for (const auto& context : learnt.contexts | std::views::reverse)
		{
			if (context.taught.has_value())
			{
				ImGui::Text("%s for %s: what it was for %s, what it was done to %s", ActionName(tables, context.action).c_str(),
				            creature_desires::Name(context.desire).data(), lesson(context.taught->goal).c_str(),
				            lesson(context.taught->actOn).c_str());
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Attitudes"))
	{
		ImGui::Text("To the player %+.2f, average feedback %+.2f", static_cast<double>(mind->attitudeToPlayer),
		            static_cast<double>(mind->averageFeedback));
		for (const auto& attitude : learnt.creatures)
		{
			ImGui::Text("Creature %u: nice %+.2f, impressive %.2f, attention %.2f", attitude.creature,
			            static_cast<double>(attitude.howNice), static_cast<double>(attitude.howImpressive),
			            static_cast<double>(attitude.attention));
		}
		// Its joint action with another creature, if any, and the land's rule after a forced action
		const auto& lookup = std::as_const(registry);
		if (const auto* link = lookup.TryGet<const ecs::components::CreaturePartner>(entity))
		{
			ImGui::Text("Partner: %u (agenda %u, %s)%s", static_cast<unsigned>(entt::to_integral(link->partner)), link->agenda,
			            link->agendaRunning ? "running" : "ended",
			            lookup.AllOf<ecs::components::CreatureDemobilised>(entity) ? ", mind stilled" : "");
		}
		else
		{
			ImGui::Text("Partner: none");
		}
		ImGui::Text("Partners always agree: %s", lookup.Context().partnersAlwaysAgree ? "yes" : "no");
		// How much it misses its friends, by its feelings; not yet one of its desires' sources
		ImGui::Text("Misses its friends %.2f (not read by its desires yet)",
		            static_cast<double>(creature_friendship::MissedFriends(learnt.creatures)));
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Beliefs"))
	{
		// The things it has come across, by kind, and how far its eyes have swept the cells about it
		ImGui::Text("%zu things; sweep ring %u, %u cells left", mind->beliefs.Size(), mind->sweep.ring, mind->sweep.left);
		for (uint32_t kind = 0; kind < creature_object_beliefs::k_KindCount; ++kind)
		{
			if (const auto count = mind->beliefs.Count(kind); count != 0)
			{
				ImGui::Text("%s: %zu of %u", creature_tree::BeliefName(kind), count,
				            creature_object_beliefs::k_Capacity.at(kind));
			}
		}
		// The last moving thing it came across, which its look on the way to a far point takes for two seconds
		ImGui::Text("Last moving thing seen: (%.1f, %.1f), height %.1f, turn %u, weight %.1f",
		            static_cast<double>(mind->movingSeen.at.x), static_cast<double>(mind->movingSeen.at.y),
		            static_cast<double>(mind->movingSeen.height), mind->movingSeen.turn,
		            static_cast<double>(mind->movingSeen.weight));
		// The sources that seeing things again stirs, as they stand now
		if (mind->desires.has_value())
		{
			for (const auto source :
			     {creature_object_beliefs::k_SightingSources.at(0), creature_object_beliefs::k_SightingSources.at(1),
			      creature_object_beliefs::k_SightingSources.at(2), creature_object_beliefs::k_ToySightingSource})
			{
				const auto desire = creature_desires::DesireOfSource(*mind->desires, source);
				const auto value =
				    desire.has_value() ? creature_desires::SourceValue((*mind->desires)[*desire], source) : std::nullopt;
				ImGui::Text("Source %u from sightings: %.2f", source, static_cast<double>(value.value_or(0.0f)));
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Reactions"))
	{
		// What a script's CREATURE_REACTION does: the creature takes no notice of reactions while it is off
		bool off = std::as_const(registry).AllOf<CreatureReactionsOff>(entity);
		if (ImGui::Checkbox("Reactions off", &off))
		{
			if (off)
			{
				registry.AssignState<CreatureReactionsOff>(entity);
			}
			else
			{
				registry.RemoveState<CreatureReactionsOff>(entity);
			}
		}
		// What a script's creature made from another is: strokes and slaps only stop what it does
		bool learningOff = std::as_const(registry).AllOf<CreatureLearningOff>(entity);
		if (ImGui::Checkbox("Learns nothing from strokes and slaps", &learningOff))
		{
			if (learningOff)
			{
				registry.AssignState<CreatureLearningOff>(entity);
			}
			else
			{
				registry.RemoveState<CreatureLearningOff>(entity);
			}
		}
		const auto* reaction = std::as_const(registry).TryGet<const CreatureReaction>(entity);
		if (reaction != nullptr && reaction->sawFlying.has_value())
		{
			ImGui::Text("Last took up a flying thing at reaction turn %u", *reaction->sawFlying);
		}
		else
		{
			ImGui::TextUnformatted("Has taken up no flying thing");
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Amusing another creature"))
	{
		if (const auto other = NearestOtherCreature(std::as_const(registry), entity); other.has_value())
		{
			ImGui::Text("Nearest other creature %u: %s", entt::to_integral(*other),
			            minds.SeemsFriendly(entity, *other) ? "seems friendly" : "does not seem friendly");
		}
		else
		{
			ImGui::TextUnformatted("No other creature");
		}
		if (ImGui::SmallButton("Amuse the nearest other creature with a spell"))
		{
			_lastAmusing = AmuseNearest(std::as_const(registry), minds, entity);
		}
		if (!_lastAmusing.empty())
		{
			ImGui::TextUnformatted(_lastAmusing.c_str());
		}
		ImGui::TreePop();
	}

	if (tables != nullptr && ImGui::TreeNode("Skills, miracles and copying"))
	{
		const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
		const auto& knowledge = learnt.knowledge;
		for (size_t i = 0; i < tables->skills.size() && i < knowledge.skillsSeen.size(); ++i)
		{
			ImGui::Text("%s %-20s seen %u", knowledge.skillsKnown.at(i) ? "[known]" : "[     ]", tables->skills[i].name.c_str(),
			            knowledge.skillsSeen[i].count);
		}
		for (size_t i = 0; i < tables->miracles.size() && i < knowledge.miraclesSeen.size(); ++i)
		{
			if (knowledge.miraclesKnown.at(i) || knowledge.miraclesSeen[i].count > 0)
			{
				ImGui::Text("%s %-28s seen %u", knowledge.miraclesKnown.at(i) ? "[known]" : "[     ]",
				            tables->miracles[i].name.c_str(), knowledge.miraclesSeen[i].count);
			}
		}
		const auto pick = [](const char* label, int& index, const auto& rows) {
			if (rows.empty())
			{
				return;
			}
			index = std::clamp(index, 0, static_cast<int>(rows.size()) - 1);
			if (ImGui::BeginCombo(label, rows.at(static_cast<size_t>(index)).name.c_str()))
			{
				for (size_t i = 0; i < rows.size(); ++i)
				{
					if (ImGui::Selectable(fmt::format("{}##{}", rows[i].name, i).c_str(), static_cast<int>(i) == index))
					{
						index = static_cast<int>(i);
					}
				}
				ImGui::EndCombo();
			}
		};
		const auto* creature = std::as_const(registry).TryGet<const Creature>(entity);
		if (transform != nullptr && creature != nullptr)
		{
			// Only the player's own creatures watch what the player does
			const auto owner = creature->owner;
			pick("Skill", _skill, tables->skills);
			ImGui::SameLine();
			if (ImGui::SmallButton("Show it##skill"))
			{
				minds.SeeSkill(transform->position, static_cast<size_t>(_skill));
			}
			pick("Miracle", _miracle, tables->miracles);
			ImGui::SameLine();
			if (ImGui::SmallButton("Cast it near##miracle"))
			{
				minds.SeeMiracle(transform->position, static_cast<size_t>(_miracle));
			}
			pick("Player's deed", _deed, tables->mimics);
			ImGui::SameLine();
			if (ImGui::SmallButton("Do it near##deed"))
			{
				minds.PlayerDid(owner, static_cast<size_t>(_deed), transform->position + glm::vec3(20.0f, 0.0f, 0.0f),
				                std::nullopt);
			}
			// the deed's rank, then the game's own report of it (the deed event, as the game's sites send it), done to
			// the creature itself so that it sees it: by hand, or with the miracle picked above
			if (static_cast<size_t>(_deed) < tables->mimics.size())
			{
				const auto& rule = tables->mimics.at(static_cast<size_t>(_deed));
				ImGui::Text("Rank %.2f%s", rule.chance, rule.needsLearningLeash ? ", only on the learning leash" : "");
			}
			if (ImGui::SmallButton("Report it to the creature##deed"))
			{
				ecs::creature_mimic::Consider(owner, static_cast<creature_watching::Deed>(_deed), entity);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Report it done with the miracle##deed"))
			{
				ecs::creature_mimic::Consider(owner, static_cast<creature_watching::Deed>(_deed), entity,
				                              static_cast<MagicType>(_miracle));
			}
		}
		if (learnt.mimicry.has_value())
		{
			ImGui::Text("Copying \"%s\": %s, %u steps left", tables->mimics.at(learnt.mimicry->rule).name.c_str(),
			            creature_watching::Name(learnt.mimicry->stage), learnt.mimicry->stepsLeft);
		}
		else
		{
			ImGui::TextUnformatted("Copying nothing");
		}
		ImGui::TreePop();
	}

	// The script natives on creatures, any creature's last call each; read only while open
	if (ImGui::TreeNode("Script natives (last calls)"))
	{
		using ecs::systems::CreatureNative;
		constexpr std::array<std::pair<CreatureNative, const char*>, ecs::systems::k_CreatureNativeCount> k_Natives {{
		    {CreatureNative::TurnOffAllDesires, "CREATURE_TURN_OFF_ALL_DESIRES"},
		    {CreatureNative::ForceFinish, "CREATURE_FORCE_FINISH"},
		    {CreatureNative::ArsePosition, "GET_ARSE_POSITION"},
		    {CreatureNative::BellyPosition, "GET_BELLY_POSITION"},
		    {CreatureNative::PosValidForCreature, "POS_VALID_FOR_CREATURE"},
		    {CreatureNative::MoveGameThing, "MOVE_GAME_THING"},
		    {CreatureNative::DoAction, "CREATURE_DO_ACTION"},
		}};
		for (const auto& [native, name] : k_Natives)
		{
			const auto last = minds.LastNative(native);
			if (!last.has_value())
			{
				ImGui::Text("%s: not called", name);
				continue;
			}
			const bool self = last->object == entt::to_integral(entity);
			ImGui::Text("%s x%u: thing %u%s, (%.1f, %.1f, %.1f), %s: %.*s", name, last->calls, last->object,
			            self ? " (this creature)" : "", static_cast<double>(last->point.x), static_cast<double>(last->point.y),
			            static_cast<double>(last->point.z), last->done ? "done" : "not done",
			            static_cast<int>(last->note.size()), last->note.data());
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Casting for a town"))
	{
		if (ImGui::SmallButton("Cast the town's shield at the nearest built pit"))
		{
			_lastTownCast = CastTownShield(std::as_const(registry), minds, entity);
		}
		if (!_lastTownCast.empty())
		{
			ImGui::TextUnformatted(_lastTownCast.c_str());
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Emergencies"))
	{
		// Badly hurt, on fire or with a fire in its town, it looks at no new emergency and takes up no reaction until it
		// stops what it does
		ImGui::Text("Emergency running: %s", mind->emergencyRunning ? "yes" : "no");
		if (Locator::fireSystem::has_value() && ImGui::SmallButton("Set it on fire"))
		{
			Locator::fireSystem::value().SetOnFire(entity, 0.0f);
		}
		ImGui::TreePop();
	}

	ImGui::TextUnformatted("Thoughts:");
	for (const auto& thought : learnt.thoughts | std::views::reverse)
	{
		ImGui::BulletText("%s", thought.c_str());
	}
}
