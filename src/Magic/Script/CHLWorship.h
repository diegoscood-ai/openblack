/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// The CHL natives of the miracles' supply (worship sites, prayer power, town spells, dispensers, one-shots; research
// sources.md §8). CHLApi.cpp forwards to them; each pops its arguments in the handler's order.

namespace openblack::magic::script
{
void GameSetMana();             ///< 355 GAME_SET_MANA 0x6FE800 (site or a thing with one, chants)
void GetMana();                 ///< 422 GET_MANA 0x6FE8C0 (site)
void SetMagicInObject();        ///< 386 SET_MAGIC_IN_OBJECT 0x6FF0B0 (town, magic, on)
void SetCanBuildWorshipsite();  ///< 376 SET_CAN_BUILD_WORSHIPSITE 0x6FEC40 (town or citadel, on)
void IsSpellCharging();         ///< 330 IS_SPELL_CHARGING 0x70CB80 (player)
void IsThatSpellCharging();     ///< 331 IS_THAT_SPELL_CHARGING 0x70CBD0 (player, magic)
void ClearPlayerSpellCharging(); ///< 423 CLEAR_PLAYER_SPELL_CHARGING 0x70CD80 (player)
void GetSpellIconInTemple();    ///< 453 GET_SPELL_ICON_IN_TEMPLE 0x6F3590 (citadel, magic)
void GetTownWorshipDeaths();    ///< 410 GET_TOWN_WORSHIP_DEATHS 0x6FF640 (town)
void SetMagicProperties();      ///< 356 SET_MAGIC_PROPERTIES 0x70CC30 (dispenser, magic, seconds)

/// SET_ACTIVE 0x6FD720 / SET_TIMER_TIME 0x711280 on a spell dispenser (their other objects stay CHLApi's): true when
/// the object was one
bool SetDispenserActive(entt::entity object, bool active);
bool SetDispenserTimerTime(entt::entity object, float seconds);

/// CREATE / CREATE_WITH_ANGLE_AND_SCALE (GScript 0x6F1010) of the SCRIPT_OBJECT_TYPEs ONE_SHOT_SPELL (30:
/// OneOffSpellSeed::Create(pos, seed, -1, 1)), ONE_SHOT_SPELL_IN_HAND (31: CreateSpellIntoHand(the local status, seed,
/// -1, 1)) and SPELL_DISPENSER (36: SpellDispenser::Create(pos, abode info, no town, angle, scale)); entt::null
entt::entity CreateOneShotSpell(uint32_t seed, const glm::vec3& position);
entt::entity CreateOneShotSpellInHand(uint32_t seed);
entt::entity CreateSpellDispenser(uint32_t abodeInfo, const glm::vec3& position, float yAngle, float scale);
} // namespace openblack::magic::script
