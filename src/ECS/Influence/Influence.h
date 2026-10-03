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

#include "Enums.h"

namespace openblack
{
struct GInfluenceInfo;
}

// A player's influence (Influence.cpp of the original): where the player can act and cast. Sources: the citadel and the
// player's towns (a radius each: inside it the influence is 1) and the influence rings (a gradient). Research:
// dev\tmp_dis\miracles\influence.md; wiki: docs/bw1-notes/magic.md ("Influencia").
//   Influence.cpp            the queries and the land's globals
//   InfluenceSources.cpp     town (Town::Process part) and citadel radii
//   InfluenceRings.cpp       InfluenceRing
//   InfluenceDebugHooks.cpp  OPENBLACK_TEST_INFLUENCE

namespace openblack::influence
{
/// INFL_CALC_TYPE: only the virtual influence reads it (not ported). The hand's in-influence test passes Interface.
enum class CalcType : int32_t
{
	Default = 0,
	Interface = 1,
};

/// Influence::CalculatePlayerInfluence 0x5CD170 (pos, player, 0, type, allies), -1..1; > 0 = in the player's
/// influence. The cast rules (fn_005FB5D0) and the hand (fn_005D1120) test `> 0` with allies on.
[[nodiscard]] float CalculatePlayerInfluence(PlayerNames player, const glm::vec3& position,
                                             CalcType type = CalcType::Default, bool includeAllies = true);
[[nodiscard]] float CalculatePlayerInfluence(entt::entity player, const glm::vec3& position,
                                             CalcType type = CalcType::Default, bool includeAllies = true);
/// Influence::CalculatePlayerRawInfluence 0x5CD230: citadel + towns + rings, clamped to -1..1 (an anti ring of the
/// player covering the point gives 0)
[[nodiscard]] float CalculatePlayerRawInfluence(PlayerNames player, const glm::vec3& position);
/// Influence::IsInPlayerRawInfluence 0x5CD460
[[nodiscard]] bool IsInPlayerRawInfluence(PlayerNames player, const glm::vec3& position);
/// Influence::IsInAntiInfluence 0x5CD490: inside an anti-influence ring of that player
[[nodiscard]] bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position);
[[nodiscard]] bool IsInAntiInfluence(entt::entity player, const glm::vec3& position);
/// Influence::CalculateInfluenceOnRange 0x5CD560 (GInfluenceInfo): 1 up to 0.4 r, 0.8 -> 0 up to 0.6 r, 0.2 -> 0 up to r
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius);
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius, const GInfluenceInfo& info);

// ---- rings (InfluenceRings.cpp) ----

/// InfluenceRing::Create 0x5CD9D0: a fixed ring (CREATE_INFLUENCE_RING, INFLUENCE_POSITION, shields)
entt::entity CreateRing(const glm::vec3& position, PlayerNames player, float radius, bool anti);
/// fn_005CD990 -> fn_005CD800: a ring that follows an object (INFLUENCE_OBJECT); entt::null if the object is not valid
entt::entity CreateRingOnObject(entt::entity object, PlayerNames player, float radius, bool anti);
/// InfluenceRing::ToBeDeleted 0x5CD8A0
void DeleteRing(entt::entity ring);
/// InfluenceRing::ProcessRings 0x5CDB90: attached rings follow their object, or go with it
void ProcessRings();

// ---- towns and citadel (InfluenceSources.cpp) ----

/// The influence part of Town::Process 0x747380 for every town (GetBaseInfluence 0x73FD40 + Abode::GetInfluence
/// 0x4072A0 of its abodes, x townInfluenceMultiplier)
void ProcessTowns();
/// Town +0x5C8 of a town entity (0 if it has no influence yet)
[[nodiscard]] float TownRadius(entt::entity town);
/// Citadel::GetInfluence 0x464090 of a temple entity
[[nodiscard]] float CitadelRadius(entt::entity temple);

// ---- the player's influence power (GPlayer +0x8C; InfluenceSources.cpp) ----

/// GPlayer::CalculateInfluencePower 0x64AD00: +0x8C = Citadel::GetInfluence of the player's citadel (+0xA48, with its
/// heart +0x30), + each town's +0x5C8 (fn_0073FBC0, the town list +0xA50), + the radius +0x38 of every influence ring of
/// the player (g_game+0x205C4C, anti rings too), each a float sum; stored and returned. The rest of the function (+0x90
/// = (+0x90 + +0x8C) x 0.5 and the GameStats history +0xAC) has no reader in openblack and is not ported.
float CalculateInfluencePower(PlayerNames player);
/// GPlayer::Process 0x64971D for every player and the neutral one (GetNextPlayerAndNeutral, GPlayer::ProcessPlayers
/// 0x649B1D), after its alignment: MagicLoop's slot 3
void CalculateInfluencePowers();
/// GPlayer +0x8C as the last CalculateInfluencePower left it (0 before the first one of the land)
[[nodiscard]] float InfluencePower(PlayerNames player);
/// fn_0064B700: the sum (from 0, in float) of +0x8C over GGame::GetNextActivePlayerAndNeutral 0x550930 (the players of
/// type +0x8E0 != 0, then the neutral one), divided by the player's own +0x8C (fdivr 0x64B74F); 0 when the own is 0
/// (fcom 0; test ah, 0x40). (inferido) a player is active when the land made it (magic::players::EntityOf): openblack
/// keeps +0x8E0 only for the human one
[[nodiscard]] float InfluencePowerRatio(PlayerNames player);

/// What the original does in GGame::ProcessTurn at the influence slot: InfluenceRing::ProcessRings, plus the towns'
/// influence (Town::Process, run in the towns' own loop in the original)
void ProcessTurn();

// ---- the influence circles and the hand that crosses them (InfluenceCircles.cpp) ----

/// fn_0x005e5cd0 0x5E61A1..0x5E6230, once a frame while the game is not paused (g_game+0x14 & 4): the hand's point goes
/// to fn_00827820, which compares it against the circles GGame::Update3DInfluence 0x5552A0 keeps (one per citadel and per
/// town with influence) and, for each player whose "the hand is inside" changed since the last frame and who has a
/// circle whose edge the hand crossed since the previous call (fn_008277B0), makes a ripple at the crossing; then
/// G_HandThroughInfluence_01 (InGame 52) plays once, 3D at the hand
void ProcessHandCrossing(const glm::vec3& handPosition);

/// fn_00827820 without the sound: true when it set [0xEB9A6C] (a crossing made a ripple)
[[nodiscard]] bool HandCrossedInfluence(const glm::vec3& handPosition);

namespace detail
{
/// For the tests only: the statics of fn_00827820 ([0xEB9A48], [0xEB9A68], [0xEA9EF0]) back to the process start;
/// nothing in the original clears them
void ResetHandCrossing();
} // namespace detail

// ---- the land's globals (GGame fields) ----

/// g_game+0x205A08, SET_LAND_NUMBER (0 = no story land): Game::GetMapScriptGlobals().landNumber
[[nodiscard]] int32_t LandNumber();
/// g_game+0x250078 / +0x25007C, SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER (1 before each map
/// script): Game::GetMapScriptGlobals()
[[nodiscard]] float TownInfluenceMultiplier();
[[nodiscard]] float PlayerInfluenceMultiplier();
/// g_game+0x14 & 0x2000, set at start from the registry value "GatheringFlag" (start_system 0x6433B1): every player
/// has influence 1 everywhere. Only a test hook or a mod sets it.
void SetInfluenceEverywhere(bool on);
[[nodiscard]] bool IsInfluenceEverywhere();

/// OPENBLACK_TEST_INFLUENCE / OPENBLACK_INFLUENCE_EVERYWHERE (InfluenceDebugHooks.cpp); call once per turn
void RunDebugHooks();
} // namespace openblack::influence
