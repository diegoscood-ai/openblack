/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpQueryFacts.h"

#include <cstddef>

#include <glm/vec2.hpp>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/FishShoals.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/TerrainMaterial.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;
using help_query::Texts;

namespace
{

constexpr Texts k_None {.first = 0, .last = 0};
/// The player the help answers: the local one
constexpr auto k_LocalPlayer = PlayerNames::PLAYER_ONE;

template <typename Row>
Texts TextsOfRow(const Row& row)
{
	return {.first = static_cast<uint32_t>(row.helpStartEnum), .last = static_cast<uint32_t>(row.helpEndEnum)};
}

/// A row of a table by an index, or none past its end
template <typename Rows, typename Index>
std::optional<Texts> RowTexts(const Rows& rows, Index index)
{
	const auto i = static_cast<size_t>(index);
	if (i >= rows.size())
	{
		return std::nullopt;
	}
	return TextsOfRow(rows[i]);
}

/// Whose a thing is: a creature's owner, a villager's town's, else the owner of the town it belongs to
std::optional<PlayerNames> PlayerOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		return creature->owner;
	}
	if (registry.AllOf<Villager>(thing))
	{
		return ecs::villager::GetPlayerOf(thing);
	}
	const auto town = ecs::town_queries::GetTown(thing);
	if (town == entt::null || !registry.Valid(town))
	{
		return std::nullopt;
	}
	const auto* data = registry.TryGet<const Town>(town);
	return data != nullptr ? std::optional(data->owner) : std::nullopt;
}

bool IsLocalPlayers(std::optional<PlayerNames> player)
{
	return player.has_value() && *player == k_LocalPlayer;
}

/// The texts of the thing's kind, from its row in the info tables
Texts KindTexts(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();
	if (const auto* row = ecs::fire::traits::InfoOf(thing); row != nullptr)
	{
		return TextsOfRow(*row);
	}
	if (const auto* reward = registry.TryGet<const Reward>(thing))
	{
		return RowTexts(info.reward, reward->type).value_or(k_None);
	}
	if (const auto* highlight = registry.TryGet<const ScriptHighlight>(thing))
	{
		return RowTexts(info.scriptHighlight, highlight->infoIndex).value_or(k_None);
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(thing))
	{
		// A worship site is its tribe's
		const auto tribe = static_cast<size_t>(site->tribe);
		if (tribe < info.tribe.size())
		{
			return RowTexts(info.worshipSite, info.tribe.at(tribe).worshipSiteInfo).value_or(k_None);
		}
		return k_None;
	}
	if (const auto* totem = registry.TryGet<const VillageTotem>(thing))
	{
		// The totem stands in the town centre, of the centre's tribe
		if (const auto* centre = ecs::abodes::InfoOf(totem->townCentre); centre != nullptr)
		{
			return RowTexts(info.totemStatue, centre->tribeType).value_or(k_None);
		}
		return k_None;
	}
	if (registry.AllOf<FishFarm>(thing))
	{
		return TextsOfRow(info.fishFarm);
	}
	// TODO(fireflies): a firefly is told by the third single map fixed row, once fireflies are things of their own
	return k_None;
}

/// The text of what a villager is doing, by the state it works towards
uint32_t ActivityText(entt::entity villager)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return 0;
	}
	const auto state = Locator::livingActionSystem::value().VillagerGetFinalState(*action);
	return ecs::villager::state_info::QueryText(ecs::villager::state_info::StateInfo(state));
}

/// A villager's work as a disciple, none when it is not one
std::optional<VillagerDisciple> DiscipleOf(const Villager& villager)
{
	if ((villager.flags & Villager::k_FlagDisciple) == 0)
	{
		return std::nullopt;
	}
	return static_cast<VillagerDisciple>(villager.discipleType);
}

/// Is a pile of food in a finished storage pit
bool InFinishedStore(entt::entity pile)
{
	const auto& registry = Locator::entitiesRegistry::value();
	bool found = false;
	registry.Each<const StoragePit>([&found, pile](entt::entity store, const StoragePit& pit) {
		if (pit.foodPile == pile)
		{
			found = found || ecs::abodes::GetPercentBuilt(store) >= 1.0f;
		}
	});
	return found;
}

} // namespace

Texts ecs::help_query_facts::TextsOf(entt::entity thing)
{
	if (!Locator::infoConstants::has_value())
	{
		return k_None;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value();

	// TODO(town-desire-flags): a flag flying a town's need is told by its need's row, once towns make their flags
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		return help_query::VillagerTexts(ActivityText(thing), DiscipleOf(*villager), KindTexts(thing));
	}
	if (const auto* creature = registry.TryGet<const Creature>(thing))
	{
		const auto row = creature::InfoRow(creature->species);
		if (row >= info.creature.size())
		{
			return k_None;
		}
		return help_query::CreatureTexts(TextsOfRow(info.creature.at(row)), info.creature.at(row).row,
		                                 IsLocalPlayers(creature->owner));
	}
	if (const auto* pot = registry.TryGet<const Pot>(thing))
	{
		const auto row = static_cast<size_t>(pot->type);
		if (row < info.pot.size() && info.pot.at(row).potType == PotType::PileFood)
		{
			return help_query::FoodPileTexts(InFinishedStore(thing), KindTexts(thing));
		}
	}
	if (const auto* seed = registry.TryGet<const OneOffSpellSeed>(thing))
	{
		const auto miracle = RowTexts(info.spellSeed, seed->seedType).value_or(k_None);
		const auto object = RowTexts(info.mobileObject, MobileObjectInfo::OneOffSpellSeed).value_or(k_None);
		return help_query::SeedTexts(miracle, object);
	}
	if (const auto* spell = registry.TryGet<const Spell>(thing))
	{
		if (static_cast<size_t>(spell->magicType) < info.magicEffect.size())
		{
			return TextsOfRow(magic::GetMagicEffectInfo(info, spell->magicType));
		}
		return k_None;
	}
	// TODO(temple-entrance): the temple's entrance is told by help_query::k_TempleEntranceTexts, once it is a thing
	if (const auto* feature = registry.TryGet<const Feature>(thing))
	{
		return help_query::FeatureTexts(feature->type, abodes::GetPercentBuilt(thing), KindTexts(thing));
	}
	return KindTexts(thing);
}

help_query::Thing ecs::help_query_facts::ThingOf(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	help_query::Thing facts {.texts = TextsOf(thing)};

	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		const auto disciple = DiscipleOf(*villager);
		facts.person = help_query::Person {
		    .scriptControlled = registry.AllOf<ScriptControlled>(thing),
		    .workingDisciple = disciple.has_value() && villager::DiscipleHeldAtJob(static_cast<uint8_t>(*disciple)),
		    .dead = villager::IsDead(thing),
		    .child = villager->lifeStage == Villager::LifeStage::Child,
		    .vagrant = town_villagers::IsVagrant(thing),
		    .players = IsLocalPlayers(PlayerOf(thing)),
		};
		return facts;
	}
	if (const auto* abode = abodes::InfoOf(thing); abode != nullptr)
	{
		facts.othersWonder = abode->abodeType == AbodeType::Wonder && !IsLocalPlayers(PlayerOf(thing));
		if (abode->abodeType == AbodeType::TownCentre)
		{
			facts.role = help_query::Role::Town;
		}
		return facts;
	}
	if (registry.AllOf<VillageTotem>(thing))
	{
		facts.role = help_query::Role::Town;
	}
	return facts;
}

help_query::Ground ecs::help_query_facts::GroundAt(std::optional<glm::vec3> point)
{
	help_query::Ground ground;
	if (!point.has_value() || !Locator::terrainSystem::has_value())
	{
		return ground;
	}
	const auto coords = map_coords::FromWorld(*point);
	// The game takes a point at the map's very corner, on the ground, for no point at all
	if (coords.x == 0 && coords.z == 0 && coords.altitude == 0.0f)
	{
		return ground;
	}
	ground.hasPoint = true;

	// The point as the map keeps it, at its height over the sea
	const auto world = map_coords::ToWorld(coords);
	ground.altitude = world.y;
	ground.fishFarm = ecs::FindFishFarmAt(world).has_value();

	const glm::u16vec2 cell {map_coords::CellX(coords), map_coords::CellZ(coords)};
	ground.onLand = terrain_material::HasLandBlock(cell);
	const auto material = terrain_material::AtCell(cell);
	if (Locator::infoConstants::has_value())
	{
		ground.material = RowTexts(Locator::infoConstants::value().terrainMaterial, material).value_or(k_None);
	}
	return ground;
}
