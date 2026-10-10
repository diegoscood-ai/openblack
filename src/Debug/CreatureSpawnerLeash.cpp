/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <imgui.h>

#include "Camera/Camera.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/LeashRope.h"
#include "Creature/LeashRules.h"
#include "Creature/LeashTie.h"
#include "CreatureSpawner.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/LeashHandTie.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureLeash;
using openblack::ecs::components::CreatureLeashAttitude;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Mesh;
using openblack::ecs::components::Transform;
using openblack::ecs::components::Tree;
namespace leash = openblack::creature_leash;

namespace
{
constexpr float k_BarWidth = 160.0f;
/// How far round the creature to look for something to tie the leash to
constexpr float k_TieSearch = 400.0f;

/// The nearest thing with a mesh, other than creatures, within reach of a point
std::optional<entt::entity> NearestObject(const ecs::Registry& registry, const glm::vec3& point)
{
	std::optional<entt::entity> nearest;
	float best = k_TieSearch;
	std::as_const(registry).Each<const Mesh, const Transform>(
	    [&](entt::entity entity, const Mesh& /*mesh*/, const Transform& at) {
		    if (registry.AllOf<Creature>(entity))
		    {
			    return;
		    }
		    if (const auto distance = glm::distance(at.position, point); distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	return nearest;
}

/// Another creature than this one, the nearest
std::optional<entt::entity> OtherCreature(const ecs::Registry& registry, entt::entity self, const glm::vec3& point)
{
	std::optional<entt::entity> nearest;
	float best = std::numeric_limits<float>::max();
	std::as_const(registry).Each<const Creature, const Transform>(
	    [&](entt::entity entity, const Creature& /*creature*/, const Transform& at) {
		    if (entity == self)
		    {
			    return;
		    }
		    if (const auto distance = glm::distance(at.position, point); distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	return nearest;
}

/// The nearest plain thing with a mesh within 400 of the creature, other than the creature itself: leash posts, spell
/// icons, script highlights and one-off spell seeds are left out. A debug stand-in for the hand's own pick, the nearest
/// thing within 5 of where the hand acts
std::optional<entt::entity> NearestTieTarget(const ecs::Registry& registry, entt::entity self, const glm::vec3& point)
{
	std::optional<entt::entity> nearest;
	float best = k_TieSearch;
	std::as_const(registry).Each<const Mesh, const Transform>(
	    [&](entt::entity entity, const Mesh& /*mesh*/, const Transform& at) {
		    if (entity == self || ecs::leash_tie::KindOf(entity) != leash::TieTargetKind::Object)
		    {
			    return;
		    }
		    if (const auto distance = glm::distance(at.position, point); distance < best)
		    {
			    best = distance;
			    nearest = entity;
		    }
	    });
	return nearest;
}

const char* TieText(leash::HandTie tie)
{
	switch (tie)
	{
	case leash::HandTie::Nothing:
		return "nothing";
	case leash::HandTie::Tap:
		return "a tap, as a single click";
	case leash::HandTie::Tie:
		return "tie the leash to it";
	case leash::HandTie::Untie:
		return "untie the leash back to the hand";
	}
	return "nothing";
}

const char* KindText(leash::TieTargetKind kind)
{
	switch (kind)
	{
	case leash::TieTargetKind::Object:
		return "a thing";
	case leash::TieTargetKind::LeashPost:
		return "a temple's leash post";
	case leash::TieTargetKind::SpellIcon:
		return "a spell icon";
	case leash::TieTargetKind::ScriptHighlight:
		return "a script highlight";
	case leash::TieTargetKind::OneOffSpellSeed:
		return "a one-off spell seed";
	}
	return "a thing";
}

/// What the hand's double click and tap would do to a target, for the player
void DrawHandTieDecision(const ecs::systems::LeashSystemInterface& leashes, PlayerNames player, const char* what,
                         entt::entity target)
{
	if (target == entt::null)
	{
		ImGui::TextDisabled("%s: none", what);
		return;
	}
	const auto kind = ecs::leash_tie::KindOf(target);
	const auto tie = leash::DecideHandTie(ecs::leash_tie::CheckDoubleClick(leashes, player, target));
	const auto tap = leash::DecideLeashTapOnObject(ecs::leash_tie::CheckTap(leashes, player), kind);
	ImGui::Text("%s: entity %u, %s", what, entt::to_integral(target), KindText(kind));
	ImGui::BulletText("Double click: %s", TieText(tie));
	ImGui::BulletText("Tap: %s", tap == leash::LeashTap::ActOnObject ? "the creature acts on it" : "the hand's own tap");
}

/// The hand's tie (ecs::leash_tie): the rules' decision for the thing last clicked and for the nearest thing, and the
/// tie and untie packets sent as the hand's double click sends them, carried out at the next turn
void DrawHandTie(const ecs::Registry& registry, const ecs::systems::LeashSystemInterface& leashes, entt::entity entity,
                 const glm::vec3& position)
{
	const auto player = registry.Get<const Creature>(entity).owner;
	const auto clicked =
	    Locator::handSystem::has_value() ? Locator::handSystem::value().GetClickedObject() : entt::entity {entt::null};
	DrawHandTieDecision(leashes, player, "Last clicked", clicked);
	const auto nearest = NearestTieTarget(registry, entity, position);
	DrawHandTieDecision(leashes, player, "Nearest thing", nearest.value_or(entt::null));
	ImGui::TextDisabled("(the thing under the hand is the hand's to pick: the last clicked one stands for it)");

	ImGui::BeginDisabled(!nearest.has_value());
	if (ImGui::Button("Tie to nearest (packet)") && nearest.has_value())
	{
		game_packets::Push({.type = game_packets::Type::LeashTie, .object = *nearest, .player = player});
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Untie (packet)"))
	{
		game_packets::Push({.type = game_packets::Type::LeashTie, .object = entt::null, .player = player});
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Sends the hand's untie: the leash comes off and goes back on in the hand at the next turn");
	}
	ImGui::TextDisabled("Pending packets: %zu", game_packets::Pending());
}

/// When two creatures leashed together last warmed or cooled to each other, by the stamp kept on the one leashed to,
/// and when they next will
void DrawAttitudeStep(const char* whose, const CreatureLeashAttitude* attitude, uint32_t turn)
{
	const auto last = attitude != nullptr ? attitude->lastStep : 0u;
	const auto lastText = last == 0 ? std::string("never") : fmt::format("turn {}", last);
	if (leash::AttitudeStepDue(last, turn))
	{
		ImGui::Text("%s: last step %s, the next is due now (turn %u)", whose, lastText.c_str(), turn);
	}
	else
	{
		ImGui::Text("%s: last step %s, the next at turn %u (now %u)", whose, lastText.c_str(),
		            last + leash::k_AttitudeTurns + 1, turn);
	}
}

/// The area the creature is kept within this turn and its walk back into it: the point and how near, how far it is,
/// and the speed, the arrival and the restart distance the walk back takes
void DrawKeptArea(const ecs::Registry& registry, entt::entity entity)
{
	const auto* state = registry.TryGet<const CreatureLeash>(entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	const auto* body = registry.TryGet<const Creature>(entity);
	if (state == nullptr || transform == nullptr || body == nullptr)
	{
		ImGui::TextDisabled("Not kept anywhere");
		return;
	}
	const auto radius = state->confinementRadius;
	const auto& point = state->confinementCentre;
	const bool leashed = state->worn.has_value();
	const bool confined = leash::IsConfined(radius, leashed, !leashed || state->worn->works);
	ImGui::Text("Point (%.1f, %.1f), radius %.1f%s", point.x, point.z, radius,
	            confined ? "" : (radius > 0.0f ? ", the leash does not work" : ", not kept"));
	const auto distance = gutils::GetDistanceInMetres(transform->position, point);
	ImGui::Text("Distance %.1f: %s", distance, confined && distance > radius ? "strayed" : "within");
	const auto height = creature_morph::k_HeightAtSizeOne * body->size;
	ImGui::Text("Walk back: walk + %.3f, back within %.1f, sent again past %.1f",
	            radius > 0.0f ? leash::WalkBackHurry(distance, radius) : 0.0f, leash::WalkBackArrival(height, radius),
	            leash::WalkBackRestartDistance(radius));
	if (state->returning)
	{
		ImGui::Text("Walking back to (%.1f, %.1f), %.1f from the point", state->returningTo.x, state->returningTo.z,
		            gutils::GetDistanceInMetres(state->returningTo, point));
	}
	else
	{
		ImGui::TextUnformatted("Not walking back");
	}
}

/// Readouts of the leash's rules for this creature: the tied lengths, the attitude step and the young creature's home
void DrawLeashRules(const ecs::Registry& registry, const ecs::systems::LeashSystemInterface& leashes, entt::entity entity)
{
	const auto* state = registry.TryGet<const CreatureLeash>(entity);
	const auto tiedTo = state != nullptr && state->worn.has_value() ? state->worn->tiedTo : std::nullopt;
	const bool tied = tiedTo.has_value() && registry.Valid(*tiedTo);

	// The tied lengths, and which rule gave them
	if (tied)
	{
		const auto& rope = state->worn->rope;
		ImGui::Text("Tied lengths: full %.2f, slack %.2f, by %s", static_cast<double>(rope.maxLength),
		            static_cast<double>(rope.slackLength),
		            registry.AllOf<Tree>(*tiedTo) ? "the tree rule (6 x height, at most 40)"
		                                          : "the other rule (1.5 x distance, 180 to 360)");
	}
	else
	{
		ImGui::TextDisabled("Tied lengths: not tied");
	}

	// The attitude steps: the stamp of the creature it is tied to, and its own as the one others are tied to
	const auto turn = game_clock::Turn();
	if (tied && registry.AllOf<Creature>(*tiedTo))
	{
		DrawAttitudeStep("With the creature it is tied to", registry.TryGet<const CreatureLeashAttitude>(*tiedTo), turn);
	}
	DrawAttitudeStep("Creatures tied to it", registry.TryGet<const CreatureLeashAttitude>(entity), turn);

	// The young creature's home: whether it applies, what keeps it from applying, the point and the radius
	const auto keeping = leashes.HomeKeepingOf(entity);
	const bool hasHome = state != nullptr && state->home.has_value();
	std::vector<std::string> fails;
	if (keeping.leashed)
	{
		fails.emplace_back("it wears a leash");
	}
	if (keeping.developmentPhase >= leash::k_YoungUntilPhase)
	{
		fails.push_back(
		    fmt::format("development phase {} (below {} needed)", keeping.developmentPhase, leash::k_YoungUntilPhase));
	}
	if (!keeping.localPlayers)
	{
		fails.emplace_back("not the local player's");
	}
	if (keeping.computerPlayer)
	{
		fails.emplace_back("a computer player's");
	}
	if (keeping.multiplayer)
	{
		fails.emplace_back("a multiplayer game");
	}
	if (keeping.landNumber != leash::k_YoungHomeLand)
	{
		fails.push_back(fmt::format("land {} ({} needed)", keeping.landNumber, leash::k_YoungHomeLand));
	}
	if (!hasHome)
	{
		fails.emplace_back("no home kept");
	}
	ImGui::Text("Kept at home while young: %s", fails.empty() ? "applies" : "does not apply");
	for (const auto& fail : fails)
	{
		ImGui::BulletText("%s", fail.c_str());
	}
	ImGui::TextDisabled("(the computer-player test is never failed: no player kind is kept yet)");
	if (hasHome)
	{
		ImGui::Text("Home (%.1f, %.1f), radius %.1f", static_cast<double>(state->home->x), static_cast<double>(state->home->z),
		            static_cast<double>(leash::k_YoungHomeRadius));
	}
}
} // namespace

void CreatureSpawner::DrawLeash(entt::entity entity) noexcept
{
	if (!Locator::leashSystem::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& leashes = Locator::leashSystem::value();
	const auto* transform = std::as_const(registry).TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return;
	}
	ImGui::SeparatorText("Leash");

	// Who the creature belongs to, and whether it is the one creature its owner can lead
	const auto& body = std::as_const(registry).Get<const Creature>(entity);
	const auto ownerName = [](PlayerNames owner) {
		return owner == PlayerNames::NEUTRAL ? std::string("Neutral") : fmt::format("Player {}", static_cast<int>(owner) + 1);
	};
	ImGui::SetNextItemWidth(k_BarWidth);
	if (ImGui::BeginCombo("Owner", ownerName(body.owner).c_str()))
	{
		for (size_t i = 0; i < static_cast<size_t>(PlayerNames::_COUNT); ++i)
		{
			const auto owner = static_cast<PlayerNames>(i);
			if (ImGui::Selectable(ownerName(owner).c_str(), owner == body.owner))
			{
				leashes.SetOwner(entity, owner);
			}
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	bool leashable = leashes.IsLeashable(entity);
	ImGui::BeginDisabled(!leash::CanLead(body.owner));
	if (ImGui::Checkbox("Leashable", &leashable))
	{
		leashes.SetLeashable(entity, leashable);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
	{
		ImGui::SetTooltip("The one creature its owner can lead; making it so stops their other creature being it");
	}
	if (const auto refused = leashes.LastRefusal(body.owner))
	{
		ImGui::TextDisabled("Last refused: player %d, creature %u: %s", static_cast<int>(refused->player) + 1,
		                    entt::to_integral(refused->creature), leash::Describe(refused->why));
	}

	// Which leashes it knows, as the scripts grant them
	ImGui::TextUnformatted("Knows:");
	for (const auto type : leash::k_Types)
	{
		ImGui::SameLine();
		bool known = leashes.Knows(entity, type);
		if (ImGui::Checkbox(leash::Name(type), &known))
		{
			leashes.SetKnown(entity, type, known);
		}
	}
	if (ImGui::Button("Grant all"))
	{
		for (const auto type : leash::k_Types)
		{
			leashes.SetKnown(entity, type, true);
		}
	}

	// Putting each leash on, held in the hand
	for (const auto type : leash::k_Types)
	{
		ImGui::SameLine();
		ImGui::BeginDisabled(!leashes.Knows(entity, type) || !leashes.Knows(entity, LeashType::Rope));
		if (ImGui::Button(fmt::format("Put on {}", leash::Name(type)).c_str()))
		{
			if (leashes.IsLeashed(entity))
			{
				leashes.ChangeType(entity, type);
			}
			else
			{
				leashes.PutOn(entity, type);
			}
		}
		ImGui::EndDisabled();
	}

	const auto* state = std::as_const(registry).TryGet<const CreatureLeash>(entity);
	const bool leashed = leashes.IsLeashed(entity);
	ImGui::BeginDisabled(!leashed);
	if (ImGui::Button("Take off"))
	{
		leashes.TakeOff(entity);
	}
	ImGui::SameLine();
	if (ImGui::Button("Tie to nearest thing"))
	{
		if (const auto object = NearestObject(registry, transform->position))
		{
			leashes.TieTo(entity, *object);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Tie to other creature"))
	{
		if (const auto other = OtherCreature(registry, entity, transform->position))
		{
			leashes.TieTo(entity, *other);
		}
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(!leashes.TiedTo(entity).has_value());
	if (ImGui::Button("Untie to hand"))
	{
		leashes.UntieToHand(entity);
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	if (state != nullptr && state->worn.has_value())
	{
		const auto& worn = *state->worn;
		bool works = worn.works;
		if (ImGui::Checkbox("Leash works", &works))
		{
			leashes.SetWorks(entity, works);
		}
		ImGui::Text("%s leash, %s", leash::Name(worn.type),
		            worn.tiedTo.has_value() ? fmt::format("tied to entity {}", static_cast<uint32_t>(*worn.tiedTo)).c_str()
		                                    : "held in the hand");
		ImGui::Text("Tension");
		ImGui::SameLine();
		ImGui::ProgressBar(worn.rope.tension, ImVec2(k_BarWidth, 0.0f), fmt::format("{:.2f}", worn.rope.tension).c_str());
		ImGui::Text("Length %.1f (rest %.1f, full %.1f)", glm::distance(worn.rope.start, worn.rope.end), worn.rope.slackLength,
		            worn.rope.maxLength);
	}
	else
	{
		ImGui::Text("No leash on (picked: %s)", state != nullptr ? leash::Name(state->selected) : "none");
	}
	if (state != nullptr)
	{
		ImGui::Text("Led: %s, pull %.2f",
		            state->control == CreatureLeash::Control::Led ? "walking where the leash takes it" : "not controlling",
		            state->pull);
		// Nothing in the game tugs a leash; this tugs the one in the hand as the original's tug would
		ImGui::SameLine();
		ImGui::BeginDisabled(!leashed || leashes.TiedTo(entity).has_value());
		if (ImGui::Button("Tug"))
		{
			leashes.Tug(entity);
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Tugs the leash held in the hand: not led yet, it is pulled away from its plan; it sets off for "
			                  "the hand. Nothing in the game sends a tug");
		}
		std::string pulledAway;
		for (size_t d = 0; d < state->pulls.counts.size(); ++d)
		{
			if (state->pulls.counts.at(d) > 0)
			{
				pulledAway += fmt::format(" {} {}", creature_desires::Name(static_cast<creature_desires::Desire>(d)),
				                          state->pulls.counts.at(d));
			}
		}
		ImGui::Text("Pulled away from:%s", pulledAway.empty() ? " nothing yet" : pulledAway.c_str());
		if (state->confinementRadius > 0.0f)
		{
			ImGui::Text("Kept within %.1f of (%.0f, %.0f)%s", state->confinementRadius, state->confinementCentre.x,
			            state->confinementCentre.z, state->returning ? ", returning" : "");
		}
	}
	ImGui::Text("Free of home: %s", leashes.FreeOfHome(entity) ? "yes" : "no");
	if (ImGui::Button("Keep at home"))
	{
		leashes.ConfineToHome(entity, leash::k_HomeConfinement);
	}
	ImGui::SameLine();
	if (ImGui::Button("Let roam"))
	{
		leashes.ClearConfinement(entity);
	}

	// What the leash tells the mind, for the learning to come
	if (const auto* mind = std::as_const(registry).TryGet<const CreatureMindState>(entity))
	{
		const auto& hooks = mind->leash;
		const auto& dominant = mind->dominantDesire;
		const auto msPerTurn = game_clock::MsPerTurn();
		const uint32_t turnsPerSecond = msPerTurn > 0 ? 1000 / msPerTurn : 0;
		const auto turnsLeft = creature_spell_mind::CheatTurnsLeft(dominant, static_cast<float>(turnsPerSecond));
		const auto secondsLeft = turnsPerSecond > 0 ? static_cast<double>(turnsLeft) / turnsPerSecond : 0.0;
		const auto dominantText =
		    dominant.desire.has_value()
		        ? fmt::format("{} dominant, {:.1f} s left", creature_desires::Name(*dominant.desire), secondsLeft)
		        : std::string("no dominant desire");
		ImGui::Text("Mind: %s%s, miracles count x%u, %zu shown, %zu to act on", dominantText.c_str(),
		            hooks.learningInHand ? ", copies in hand" : "", hooks.miracleSightingWeight, hooks.shown.size(),
		            hooks.actOn.size());
	}

	// The original's rules for the tie, the attitude step and the young creature's home, as they stand
	if (ImGui::TreeNode("Leash rules"))
	{
		DrawLeashRules(std::as_const(registry), leashes, entity);
		ImGui::TreePop();
	}
	// Where it is kept this turn, how far it strayed and its walk back
	if (ImGui::TreeNode("Kept area"))
	{
		DrawKeptArea(std::as_const(registry), entity);
		ImGui::TreePop();
	}
	// The hand's tie: what a double click or a tap would do, and the tie packets
	if (ImGui::TreeNode("Tie with the hand"))
	{
		DrawHandTie(std::as_const(registry), leashes, entity, transform->position);
		ImGui::TreePop();
	}

	ImGui::Checkbox("Show rope points", &_showRope);
	if (_showRope)
	{
		DrawRope(entity);
	}
}

void CreatureSpawner::DrawRope(entt::entity entity) noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* state = std::as_const(registry).TryGet<const CreatureLeash>(entity);
	if (state == nullptr || !state->worn.has_value() || !state->worn->ropeStarted)
	{
		return;
	}
	const auto& rope = state->worn->rope;
	const auto& camera = Locator::camera::value();
	const auto display = ImGui::GetIO().DisplaySize;
	const glm::vec4 viewport {0.0f, 0.0f, display.x, display.y};
	auto* drawList = ImGui::GetBackgroundDrawList();
	const auto project = [&](const glm::vec3& world) -> std::optional<ImVec2> {
		glm::vec3 screen;
		if (!camera.ProjectWorldToScreen(world, viewport, screen))
		{
			return std::nullopt;
		}
		return ImVec2(screen.x, screen.y);
	};
	// Each segment from white, slack, to red, stretched by a tenth or more
	for (size_t i = 1; i < leash_rope::k_PointCount; ++i)
	{
		const auto from = project(leash_rope::Point(rope, i - 1));
		const auto to = project(leash_rope::Point(rope, i));
		if (!from || !to)
		{
			continue;
		}
		// a mass keeps the stretch of the segment after it: this segment's is the one of the mass it starts at (the first
		// segment starts at the rope's first point, which keeps none)
		const auto stretch = i >= 2 ? rope.nodes.at(i - 2).stretch : 0.0f;
		const auto heat = static_cast<int>(std::clamp(stretch * 10.0f, 0.0f, 1.0f) * 255.0f);
		drawList->AddLine(*from, *to, IM_COL32(255, 255 - heat, 255 - heat, 255), 1.5f);
		drawList->AddCircleFilled(*to, 2.0f, IM_COL32(255, 220, 60, 255));
	}
	if (const auto start = project(rope.start))
	{
		drawList->AddCircle(*start, 6.0f, IM_COL32(80, 200, 255, 255), 12, 2.0f);
		drawList->AddText(ImVec2(start->x + 8.0f, start->y), IM_COL32(255, 255, 255, 255),
		                  fmt::format("tension {:.2f}", rope.tension).c_str());
	}
}
