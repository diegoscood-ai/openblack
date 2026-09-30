/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The miracle dispensers (SpellDispenser.cpp 0x7226C0..0x723070): an Abode that makes a one-shot orb of its magic
// above itself, and another one `period` turns after the last was taken. Land 1's come from the challenge script's
// GiveSpellDispenserReward (CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER), SET_MAGIC_PROPERTIES, SET_ACTIVE,
// SET_TIMER_TIME); Land 3 and 5 have CREATE_SPELL_DISPENSER in their land script.

namespace openblack::worship::dispenser
{
/// SpellDispenser::Create 0x7228D0 (pos, abode info, town, y angle, scale): the abode (AbodeArchetype), the ctor's
/// period (the abode info's timeEachMobileObjectTakesToProduce, 300; 0 -> inactive) and magic 0, and
/// CallVirtualFunctionsForCreation 0x7227D0 (its effect, particle type 0x90, on the land under it). townId -1: the
/// town nearest (GetPlayer(0)'s first town in the original, fn_00723010).
entt::entity Create(const glm::vec3& position, AbodeInfo type, int townId, float yAngle, float scale);

/// fn_00723030: active; activating makes an orb at once (CreateOneOffSpellSeed)
void SetActive(entt::entity dispenser, bool active);
/// SET_MAGIC_PROPERTIES 0x70CC30 (the magic, seconds): seconds > 0 -> period = seconds x turns per second, else the
/// abode info's; a period of 0 deactivates it
void SetMagicProperties(entt::entity dispenser, MagicType magic, float seconds);
/// SET_TIMER_TIME 0x711280 on a dispenser: period = seconds x turns per second when above 0
void SetTimerTime(entt::entity dispenser, float seconds);
/// The magic and the period in turns (the map script's CREATE_SPELL_DISPENSER, case 90: the float is turns)
void SetMagicAndPeriod(entt::entity dispenser, MagicType magic, uint32_t periodTurns);

/// SpellDispenser::Process 0x722A70 (every turn): while its orb exists and touches it (vt 0x6B8 IsTouching, 0.001) it
/// waits; else, functional (IsActive), with a magic, built and repaired: every `period` turns an orb
void ProcessTurn();
/// SpellDispenser::CreateOneOffSpellSeed 0x722B80: the magic's seed and level (fn_0072B010), at the dispenser's
/// position + 1.2 x its height (fn_00722B30), OneOffSpellSeed::Create(pos, seed, pu, 1), spot visual 9
entt::entity CreateOneOffSpellSeed(entt::entity dispenser);

/// SpellSeed::ApplyThisToObject on a dispenser (fn_00728C50 / fn_00728C80): a seed not cast yet (+0x98 == 0) given to
/// it becomes a one-shot there (its chants are lost) and the seed goes; spot visual 0x1B. 1 when it was.
bool ApplySeed(entt::entity dispenser, entt::entity seed);
[[nodiscard]] bool IsDispenser(entt::entity entity);
} // namespace openblack::worship::dispenser
