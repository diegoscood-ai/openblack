/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureSpells.h"
#include "CreatureSpawner.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Transform.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreaturePhysiologySystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::debug::gui;
using openblack::ecs::components::Creature;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::CreatureMorph;
using openblack::ecs::components::CreatureNeeds;
using openblack::ecs::components::Transform;

namespace
{
/// The speeds the body's time can run at, to watch a creature grow and get hungry
constexpr std::array<float, 5> k_TimeScales {1.0f, 10.0f, 100.0f, 1000.0f, 3600.0f};
/// Food is dropped this far in front of the creature for its size
constexpr float k_FoodAhead = 6.0f;
/// The food dropped for a creature to eat: a pot of food, worth as much as the food in it, here a meal for a grown up
/// creature
constexpr auto k_FoodPot = PotInfo::FoodPot;
constexpr int32_t k_FoodAmount = 800;

constexpr float k_BarWidth = 160.0f;

std::string_view RestName(CreatureNeeds::Rest rest)
{
	constexpr std::array<std::string_view, 4> k_Names {"awake", "asleep", "resting", "out cold"};
	return k_Names.at(static_cast<size_t>(rest));
}

/// The action the body goes by, by its row and name in the game's action table
std::string ActionLabel(std::optional<uint32_t> action)
{
	if (!action.has_value())
	{
		return "none";
	}
	if (Locator::infoConstants::has_value() && *action < Locator::infoConstants::value().creatureAction.size())
	{
		const auto& name = Locator::infoConstants::value().creatureAction.at(*action).name;
		return fmt::format("{} {}", *action, std::string_view(name.data(), strnlen(name.data(), name.size())));
	}
	return std::to_string(*action);
}

/// A bar of a value from low to high, with a slider beside it to set it
void Bar(const char* label, float& value, float low, float high, const char* tooltip)
{
	const auto fraction = high > low ? (value - low) / (high - low) : 0.0f;
	ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(k_BarWidth, 0.0f), fmt::format("{:.3f}", value).c_str());
	ImGui::SameLine();
	ImGui::PushID(label);
	ImGui::SetNextItemWidth(90.0f);
	ImGui::SliderFloat("##set", &value, low, high, "set");
	ImGui::PopID();
	ImGui::SameLine();
	ImGui::TextUnformatted(label);
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", tooltip);
	}
}
} // namespace

void CreatureSpawner::DrawBody(entt::entity entity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* creature = Find<Creature>(registry, entity);
	auto* needs = Find<CreatureNeeds>(registry, entity);
	auto* transform = Find<Transform>(registry, entity);
	if (creature == nullptr || needs == nullptr || transform == nullptr)
	{
		return;
	}

	ImGui::SeparatorText("Body and needs");
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		const auto scale = physiology.GetTimeScale();
		ImGui::TextUnformatted("Body time");
		for (const auto option : k_TimeScales)
		{
			ImGui::SameLine();
			if (ImGui::RadioButton(fmt::format("x{:.0f}", option).c_str(), scale == option))
			{
				physiology.SetTimeScale(option);
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Game turns of every creature's body that pass each game turn, to watch them grow and tire");
		}
		bool fainting = physiology.IsFaintingEnabled();
		if (ImGui::Checkbox("Creatures faint", &fainting))
		{
			physiology.SetFaintingEnabled(fainting);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Grown up creatures owned by a player faint when exhausted, starved or out of life");
		}
	}

	auto& body = needs->needs;
	ImGui::Text("Age %u (game hours), %u turns, %u meals, %s for %u turns%s", body.age, body.turns, body.meals,
	            RestName(needs->rest).data(), needs->restTurns, needs->rested ? ", rested" : "");
	ImGui::TextUnformatted(needs->moving ? "On the move: tiring" : "Standing still: not tiring");
	ImGui::Text("Action %s: asleep for energy %s, for growth %s%s", ActionLabel(needs->action).c_str(),
	            creature_physiology::SlowsEnergy(needs->action) ? "yes" : "no",
	            creature_physiology::GrowsAsleep(needs->action) ? "yes" : "no", needs->sitting ? "; sitting, resting" : "");
	if (Locator::creaturePhysiologySystem::has_value())
	{
		if (const auto turns = Locator::creaturePhysiologySystem::value().LongEnoughAsleepOf(entity))
		{
			ImGui::Text("Nearly rested, it has slept long enough after %.0f turns", static_cast<double>(*turns));
		}
	}
	Bar("Energy", body.energy, 0.0f, std::max(1.0f, creature->size),
	    "Hunger is 1 less energy; a big meal fills it up to its size");
	Bar("Exhaustion", body.exhaustion, 0.0f, 1.0f, "Slowed to its slow speed from 0.8, faints at 1");
	Bar("Thirst", body.dehydration, 0.0f, 1.0f, "Fills in about 83 minutes; drinking clears it");
	Bar("Poo", body.poo, 0.0f, 1.0f, "Builds up from meals");
	Bar("Life", body.life, 0.0f, 1.0f, "Heals while asleep");
	Bar("Warmth", body.warmth, -1.0f, 1.0f, "Cold below 0, hot above, from the temperature against its species' comfort");
	Bar("Itchiness", body.itchiness, 0.0f, 1.0f, "Only the itchy spell makes a creature itch");
	Bar("Fatness", creature->fatness, 0.0f, 1.0f, "Burnt while hungry, put on by overeating");
	if (const auto* morph = std::as_const(registry).TryGet<const CreatureMorph>(entity))
	{
		ImGui::SameLine();
		ImGui::Text("(shown %.2f)", static_cast<double>(morph->shownFatness));
	}
	Bar("Strength", creature->strength, 0.0f, 1.0f, "Gained from hard work");
	const auto oldSize = creature->size;
	Bar("Size", creature->size, creature_morph::k_MinScale, creature_physiology::k_MaxGrownSize,
	    "Grows from stage 3, moving or not, fast while young; each turn it grows it is kept at most 2");
	if (creature->size != oldSize)
	{
		creature->size = creature_morph::ClampScale(creature->size);
		transform->scale = glm::vec3(ecs::archetypes::CreatureArchetype::DrawnScale(creature->species, creature->size));
		registry.SetDirty();
	}
	if (Locator::creaturePhysiologySystem::has_value())
	{
		auto& physiology = Locator::creaturePhysiologySystem::value();
		if (const auto growth = physiology.GrowthOf(entity))
		{
			const auto* mind = std::as_const(registry).TryGet<const CreatureMindState>(entity);
			const bool grows = mind == nullptr || mind->developmentPhase >= creature_physiology::k_GrowingPhase;
			ImGui::Text("Size %.4f, growth %.3g a turn%s", static_cast<double>(creature->size), static_cast<double>(*growth),
			            grows ? "" : " (not yet: below stage 3)");
			ImGui::SameLine();
			if (ImGui::SmallButton("Grow now"))
			{
				physiology.GrowNow(entity);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("One turn's growth at once, at any stage, then kept at most 2");
			}
		}
	}
	{
		// the size spells' targets, from the creature's own limits or the defaults, and a cast of each on it
		const auto& lookup = std::as_const(registry);
		const auto* own = lookup.TryGet<const ecs::components::CreatureSizeLimits>(entity);
		const auto limits = own != nullptr ? own->limits : creature_spells::SizeLimits {};
		const auto* spells = lookup.TryGet<const ecs::components::CreatureSpells>(entity);
		const float before =
		    spells != nullptr ? creature_spells::SizeBeforeSpells(spells->spells, creature->size) : creature->size;
		ImGui::Text("Size spells: big to %.2f, small to %.2f (%s)",
		            static_cast<double>(creature_spells::SizeTarget(creature_spells::Spell::Big, before, limits)),
		            static_cast<double>(creature_spells::SizeTarget(creature_spells::Spell::Small, before, limits)),
		            own != nullptr ? "a script's limits" : "the default limits");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Big takes it to its largest size, small to its smallest (%.2f and %.2f), after %.0f s",
			                  static_cast<double>(limits.largest), static_cast<double>(limits.smallest),
			                  static_cast<double>(creature_spells::k_StartDelaySeconds));
		}
		if (Locator::magicSystem::has_value() && Locator::infoConstants::has_value())
		{
			const auto cast = [&](MagicType type, const char* name) {
				const auto& info = Locator::infoConstants::value();
				const magic::SpellCastData data {.magnitude = 1.0f,
				                                 .chants = magic::GetMagicEffectInfo(info, type).initialChants,
				                                 .duration = magic::GetTimerWhenPlayerCasting(info, type),
				                                 .maxObjectsToCreate = -1};
				const psys::ProcessInfo process {.handPos = transform->position + glm::vec3(0.0f, 15.0f, 0.0f),
				                                 .direction = glm::vec3(0.0f, -10.0f, 0.0f)};
				const auto spell = Locator::magicSystem::value().CastOnObject(type, creature->owner, entity, data, process);
				_lastNeed = fmt::format("{} spell: {}", name, spell != entt::null ? "cast" : "could not start");
			};
			if (ImGui::SmallButton("Cast big"))
			{
				cast(MagicType::CreatureSpellBig, "Big");
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Cast small"))
			{
				cast(MagicType::CreatureSpellSmall, "Small");
			}
		}
	}
	{
		// a script's SCALE and HEIGHT sets, as a script makes them
		if (ImGui::SmallButton("Set scale 2"))
		{
			ecs::player_creature::SetCreatureScale(registry, entity, 2.0f);
			registry.SetDirty();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("SET SCALE: its size 2, drawn at 2 (the Guide's scale)");
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Set height 15"))
		{
			ecs::player_creature::SetCreatureHeight(registry, entity, 15.0f);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("SET HEIGHT: its size 15 / 15 = 1; the drawn body waits for its next resize");
		}
	}
	if (ImGui::SmallButton("Fill up"))
	{
		body.energy = 1.0f;
		body.exhaustion = 0.0f;
		body.dehydration = 0.0f;
		body.poo = 0.0f;
		body.life = 1.0f;
		body.warmth = 0.0f;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Starve"))
	{
		body.energy = 0.05f;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Tire out"))
	{
		body.exhaustion = 0.85f;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Exhaust"))
	{
		body.exhaustion = 1.0f;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Faints at its next turn if grown up, its player is not the computer and no script controls it");
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Parch"))
	{
		body.dehydration = 0.9f;
	}
	ImGui::SameLine();
	if (ImGui::SmallButton("Bloat"))
	{
		body.poo = 0.9f;
	}

	if (!Locator::creatureMindSystem::has_value())
	{
		return;
	}
	auto& minds = Locator::creatureMindSystem::value();
	ImGui::TextUnformatted("See to it now");
	const auto report = [this](std::string_view what, bool done) {
		_lastNeed = fmt::format("{}: {}", what, done ? "started" : "nothing to do it with");
	};
	if (ImGui::Button("Sleep"))
	{
		report("Sleep", minds.Sleep(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Wake"))
	{
		minds.Wake(entity);
	}
	ImGui::SameLine();
	if (ImGui::Button("Eat"))
	{
		report("Eat the nearest food", minds.Eat(entity, std::nullopt));
	}
	ImGui::SameLine();
	if (ImGui::Button("Drink"))
	{
		report("Drink at the nearest water", minds.Drink(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Poo"))
	{
		report("Poo", minds.Poo(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Puke"))
	{
		report("Puke", minds.Puke(entity));
	}
	ImGui::SameLine();
	if (ImGui::Button("Faint"))
	{
		report("Faint", minds.Faint(entity));
	}
	if (ImGui::Button("Drop food in front") && Locator::infoConstants::has_value())
	{
		const auto at = creature_spawner::PointAround(transform->position, transform->rotation, 0.0f,
		                                              k_FoodAhead * std::max(creature->size, 1.0f));
		ecs::archetypes::PotArchetype::Create(at, 0.0f, k_FoodPot, k_FoodAmount);
		_lastNeed = fmt::format("Dropped a pot of {} food", k_FoodAmount);
	}
	if (!_lastNeed.empty())
	{
		ImGui::TextUnformatted(_lastNeed.c_str());
	}
}
