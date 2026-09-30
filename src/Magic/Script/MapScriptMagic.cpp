/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScriptMagic.h"

#include <spdlog/spdlog.h>

#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Worship/Citadel.h"
#include "Worship/FireFlyReward.h"
#include "Worship/SpellDispenser.h"
#include "Worship/TownMagic.h"
#include "Worship/TownCentreSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// GSetup::GetScriptPos 0x718250: the script's "x,z" as a MapCoords on the land (altitude 0)
glm::vec3 LandPoint(const glm::vec3& position)
{
	return ToWorld(glm::vec3(position.x, 0.0f, position.z));
}
} // namespace

void script::CreateOneShotSpell(const glm::vec3& position, const std::string& seed)
{
	// fn_00715150 case 83: fn_0072B170 (the seed by name; 30 = none, which Create refuses)
	const int seedType = GetSpellSeedFromText(Locator::infoConstants::value(), seed);
	if (one_off::Create(LandPoint(position), static_cast<SpellSeedType>(seedType), -1, 1.0f) == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "CREATE_ONE_SHOT_SPELL: no spell seed {}", seed);
	}
}

void script::CreateOneShotSpellPu(const glm::vec3& position, const std::string& magic)
{
	// fn_00715150 case 84: GetInfoFromText, 0 < magic < 42, GetFirstSpellSeedForMagicType in 0..29, and its level
	const auto& tables = Locator::infoConstants::value();
	const int type = GetInfoFromText(tables, magic);
	if (type <= 0 || type >= static_cast<int>(k_MagicTypeCount))
	{
		return;
	}
	const auto seedType = GetFirstSpellSeedForMagicType(tables, static_cast<MagicType>(type));
	const auto index = static_cast<int>(seedType);
	if (index <= -1 || index >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int powerUp = GetPowerUpFromMagicType(GetSpellSeedInfo(tables, seedType), static_cast<MagicType>(type));
	one_off::Create(LandPoint(position), seedType, powerUp, 1.0f);
}

void script::CreateTownSpell(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const int seedType = GetSpellSeedFromText(tables, seed); // fn_0072B170
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId)); // GGame::FindTownWithID
	if (town == entt::null || seedType < 0 || seedType >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
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
	const int type = GetInfoFromText(tables, magic);
	if (type >= static_cast<int>(k_MagicTypeCount) || type <= 0)
	{
		return;
	}
	const auto seed = GetFirstSpellSeedForMagicType(tables, static_cast<MagicType>(type));
	const auto base = static_cast<int>(seed) >= 0 ? GetMagicTypeFromPULevel(GetSpellSeedInfo(tables, seed), -1)
	                                                : MagicType::None;
	worship::town::AddMagicTypesHeld(town, static_cast<MagicType>(type));
	if (!worship::town::IsMagicTypeHeld(town, base))
	{
		worship::town::AddMagicTypesHeld(town, base);
	}
}

void script::CreatePlannedSpellIcon(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const int seedType = GetSpellSeedFromText(tables, seed);
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null || seedType < 0 || seedType >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	worship::town::AddMagicTypesHeld(town, GetSpellSeedInfo(tables, static_cast<SpellSeedType>(seedType)).magicTypes[0]);
}

void script::CreateWorshipSite(PlayerNames player, Tribe tribe)
{
	// GPlayer::GetPlayerFromText, player +0xA48 the citadel and its heart (+0x30), GTribeInfo::GetTribeFromText
	const auto citadel = worship::citadel::Of(player);
	if (citadel == entt::null || tribe == Tribe::NONE)
	{
		return;
	}
	worship::citadel::CreateBuiltWorshipSite(citadel, tribe);
}

void script::CreateSpellDispenser(int32_t townId, const glm::vec3& position, AbodeInfo abode, const std::string& magic,
                                  float yAngle, float scale, float period)
{
	// GSetup::GetScriptPos, GMagicInfo::GetInfoFromText, GGame::FindTownWithID (none: the nearest, 0x552FF0)
	const int type = GetInfoFromText(Locator::infoConstants::value(), magic);
	const auto dispenser = worship::dispenser::Create(LandPoint(position), abode, townId, yAngle, scale);
	if (dispenser == entt::null)
	{
		return;
	}
	// +0xD4 = magic, fn_00723030(1), +0xC8 = ftol(period) (turns), 0 -> fn_00723030(0)
	worship::dispenser::SetMagicAndPeriod(dispenser, static_cast<MagicType>(type), static_cast<uint32_t>(period));
}

void script::FireFlySpellRewardProb(const std::string& magic, float probability)
{
	worship::fire_fly::SetRewardProbability(
	    static_cast<MagicType>(GetInfoFromText(Locator::infoConstants::value(), magic)), probability);
}

