/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// The town's emergency (Town.cpp of runblack.exe W120; spec dev\documentacion\edificios\repair_spec.md §7, the villager
// side dev\documentacion\aldeanos\V11_spec.md §6): a storage pit or a town centre knocked below its functional
// threshold (Abode::ReduceLife 0x405E3E), one of them on fire or the town attacked while in the emergency
// (ProcessTownEmergency) start it; the housed villagers are called at once to congregate in the town (242) and the
// worship stops for its 1200 turns (GTownInfo +0x110). Town::IsInStateOfEmergency 0x747970 is
// town_queries::IsInStateOfEmergency. Fields: components::Town::emergencyStartTurn (+0xF1C), aggressorTurn (+0xEB0),
// savedWorshipPercentage (+0xEC4); the worship percentage (+0x5C0) through worship::percentage.
// The attack refresh needs UpdateAggressor's record, written by effects::ApplyEffect and the physical shield.

namespace openblack::ecs::town_emergency
{
/// CallAllVillagersToTownEmergency(Town&) 0x747890 (a free function of Town.cpp): each abode of +0x754 (next +0x9C),
/// each inhabitant of +0xA0 (next +0xE4): the state table's +0x50 of its GetFinalState (vt +0xB04) answers != 0 ->
/// SetTopState(242 GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY) (vt +0x8E8). The per-villager part is the Personas
/// session's villager::CallToTownEmergency. Only housed villagers are called
void CallAllVillagersToTownEmergency(entt::entity town);
/// Town::SetInStateOfEmergency 0x7479A0: +0xF1C == 0 -> CallAllVillagersToTownEmergency; then +0xF1C = the turn (also
/// when it was set: the start is refreshed, the villagers are not called again)
void SetInStateOfEmergency(entt::entity town);
/// Town::ProcessTownEmergency 0x7477A0 (Town::Process step 15, 0x747444), in the .cpp: while in the emergency the
/// attack of this turn refreshes it and a worship percentage != 0 is saved in +0xEC4 and set to 0; else the storage pit
/// or the town centre on fire starts it; else the saved percentage comes back (when the current one is 0) and +0xEC4 =
/// +0xF1C = 0
void ProcessTownEmergency(entt::entity town);
/// Town::UpdateAggressor(EffectValues&, float) 0x73C9B0, its record only (what ProcessTownEmergency's refresh while
/// attacked reads): +0xEAC = the effect's GetCausedPlayer 0x525910 (`causedPlayer`), the neutral player without one
/// (0x73C9C7, g_game +0x205A5B), +0xEB0 = the turn (0x73CA82 / 0x73CA98). Called by Object::ApplyEffect 0x637B7D
/// (effects::ApplyEffect). (not ported) the per-player aggression slot fn_0073E0F0 (town + n x 0x80 + 0x9F4, plus
/// GTownInfo +0xAC when 0, x +0xEB8 for the town's own player else +0xEB4, which then x 0.9), the guidance
/// TownAttackSFX 0x71B7C0 and the creature part 0x73CAA3..
void UpdateAggressor(entt::entity town, std::optional<PlayerNames> causedPlayer);
} // namespace openblack::ecs::town_emergency
