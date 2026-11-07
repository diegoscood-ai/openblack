/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScriptMagic.h"

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "ECS/MapCells.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Worship/Citadel.h"
#include "Worship/FireFlyReward.h"
#include "Worship/SpellDispenser.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/TownMagic.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// The script's "x,z" as a point on the land (altitude 0)
glm::vec3 LandPoint(const glm::vec3& position)
{
	return ToWorld(glm::vec3(position.x, 0.0f, position.z));
}
} // namespace

std::optional<entt::entity> script::CreateOneShotSpell(const glm::vec3& position, const std::string& seed)
{
	// the seed by name (none: 30, which Create refuses)
	const int seedType = GetSpellSeedFromText(Locator::infoConstants::value(), seed).value_or(k_SpellSeedNotFound);
	const auto orb = one_off::Create(LandPoint(position), static_cast<SpellSeedType>(seedType), -1, 1.0f);
	if (orb == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "CREATE_ONE_SHOT_SPELL: no spell seed {}", seed);
		return std::nullopt;
	}
	return orb;
}

std::optional<entt::entity> script::CreateOneShotSpellPu(const glm::vec3& position, const std::string& magic)
{
	// GetInfoFromText, 0 < magic < 42, then the orb of the first seed with that magic type, at its level
	const auto found = GetInfoFromText(Locator::infoConstants::value(), magic);
	if (!found || *found <= 0 || *found >= static_cast<int>(k_MagicTypeCount))
	{
		return std::nullopt;
	}
	return one_off::CreateFor(LandPoint(position), static_cast<MagicType>(*found));
}

void script::CreateTownSpell(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const auto found = GetSpellSeedFromText(tables, seed);
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null || !found || *found < 0 || *found >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int seedType = *found;
	const auto& seedInfo = GetSpellSeedInfo(tables, static_cast<SpellSeedType>(seedType));
	worship::town::AddMagicTypesHeld(town, seedInfo.magicTypes[0]);
	if (const auto centre = worship::town::TownCentreOf(town); centre != entt::null)
	{
		worship::town_centre::AddSpell(centre, static_cast<SpellSeedType>(seedType));
	}
}

void script::CreateNewTownSpell(int32_t townId, const std::string& magic)
{
	const auto& tables = Locator::infoConstants::value();
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null)
	{
		return;
	}
	const auto found = GetInfoFromText(tables, magic);
	if (!found || *found >= static_cast<int>(k_MagicTypeCount) || *found <= 0)
	{
		return;
	}
	const int type = *found;
	const auto seed = GetFirstSpellSeedForMagicType(tables, static_cast<MagicType>(type));
	const auto base =
	    static_cast<int>(seed) >= 0 ? MagicTypeForPowerUpLevel(GetSpellSeedInfo(tables, seed), -1) : MagicType::None;
	worship::town::AddMagicTypesHeld(town, static_cast<MagicType>(type));
	if (!worship::town::IsMagicTypeHeld(town, base))
	{
		worship::town::AddMagicTypesHeld(town, base);
	}
}

void script::CreatePlannedSpellIcon(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const auto found = GetSpellSeedFromText(tables, seed);
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null || !found || *found < 0 || *found >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int seedType = *found;
	worship::town::AddMagicTypesHeld(town, GetSpellSeedInfo(tables, static_cast<SpellSeedType>(seedType)).magicTypes[0]);
}

std::optional<entt::entity> script::CreateWorshipSite(PlayerNames player, Tribe tribe)
{
	// the player's citadel and its heart, for the tribe
	const auto citadel = worship::citadel::Of(player);
	if (citadel == entt::null || tribe == Tribe::NONE)
	{
		return std::nullopt;
	}
	return worship::citadel::CreateBuiltWorshipSite(citadel, tribe);
}

entt::entity script::DispenserTownFor(int32_t townId, const glm::vec3& position)
{
	const auto town = ecs::town_queries::FindTownWithID(static_cast<uint32_t>(townId));
	if (town != entt::null)
	{
		return town;
	}
	return ecs::map_cells::FindNearestTownInList(map_coords::FromMetres(glm::vec2(position.x, position.z)));
}

entt::entity script::CreateSpellDispenser(int32_t townId, const glm::vec3& position, AbodeInfo abode, const std::string& magic,
                                          float yAngle, float scale, float period)
{
	// the land point, the magic by name (none: 42, passed on as it is), the town by its number, else the nearest
	const int type = GetInfoFromText(Locator::infoConstants::value(), magic).value_or(k_MagicTypeNotFound);
	const auto dispenser =
	    worship::dispenser::Create(LandPoint(position), abode, DispenserTownFor(townId, position), yAngle, scale);
	if (dispenser == entt::null)
	{
		return entt::null;
	}
	// the magic, an orb at once, the period truncated to turns (0 deactivates it)
	worship::dispenser::SetMagicAndPeriod(dispenser, static_cast<MagicType>(type), static_cast<uint32_t>(period));
	return dispenser;
}

void script::FireFlySpellRewardProb(const std::string& magic, float probability)
{
	worship::fire_fly::SetRewardProbability(
	    static_cast<MagicType>(GetInfoFromText(Locator::infoConstants::value(), magic).value_or(k_MagicTypeNotFound)),
	    probability);
}
