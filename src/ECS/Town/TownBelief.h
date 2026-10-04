/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The town's belief, GBelief (Belief.cpp of runblack.exe W120): the block at Town +0x798 (components::TownBelief), its
// per-turn fold fn_004383D0 (Town::Process step 19) and GBelief::ProcessOncePerTurn 0x4380B0 (GGame::ProcessTurn row
// 27). Spec: dev\documentacion\edificios\belief_spec.md. GBelief::AddToBelief 0x437EB0 is
// ecs::town_stores::AddToBelief.

namespace openblack::ecs::components
{
struct Town;
struct TownBelief;
} // namespace openblack::ecs::components

namespace openblack::ecs::town_belief
{
/// The 8 player slots of every row (GPlayer +0xB5, GPlayer::GetPlayerNumber 0x64A790); PlayerNames::NEUTRAL = 7
constexpr size_t k_Players = 8;
/// The REACTION count of BoredomMultiplier (+0xE8, rep stosd 0x29 at 0x437E40)
constexpr size_t k_Reactions = 41;
/// The TOWN_DESIRE count of the thresholds (+0x18C, fn_00437E50 0xDA296C .. 0xDA32FC step 0x90)
constexpr size_t k_Desires = 17;
/// Init 0x437DD0: the cap of every player, 10.0 (0x41200000 at 0x437DE1)
constexpr float k_InitialCap = 10.0f;
/// Init 0x437DD0: every boredom multiplier, 1.0 (0x437E30..0x437E40)
constexpr float k_InitialBoredom = 1.0f;
/// The fold's boredom ceiling (0x4384CF fcom 1.0): a sum below it is stored, otherwise the multiplier stays
constexpr float k_BoredomCeiling = 1.0f;
/// ProcessOncePerTurn 0x4380B0: turn % 10 (0x4380C0 div 0xA)
constexpr uint32_t k_ProcessEvery = 10;
/// [0x8C58A4] = 10000: the belief made an int amount for the belief sprites (DrawBelief 0x438806, the fold 0x438449)
constexpr double k_AmountScale = 10000.0;
/// [0x8C5890] = 0.005 (a double): ReduceBelief 0x43800B's draw threshold
constexpr double k_ReduceDrawThreshold = 0.005;
/// [0x8AA3B0] = 0.001: the believers ProcessOncePerTurn needs to say anything (0x43819C..0x4381AB)
constexpr float k_ToolTipMinimum = 0.001f;
/// 1000 (0x4381B1): the believers of the tooltip and the floating number
constexpr float k_ToolTipScale = 1000.0f;
/// HELP_TEXT 0xEE1 "Conseguidos %3.0f Creyentes" (ToolTips::ForceToolTips 0x4381C9)
constexpr uint32_t k_ToolTipBelieversGained = 0xEE1;
/// fn_0069B550 0x69B565: the belief-sprite queue takes no entry once it holds 400 ([0xD4ED30] >= 400)
constexpr size_t k_MaxBeliefSprites = 400;
/// The colour of a DrawBelief without a player (0x4388E5: 0xFFFFFFFF)
constexpr uint32_t k_NoPlayerColour = 0xFFFFFFFF;
/// 0xC61C3C00 = -9999: the start of fn_00438910's best difference
constexpr float k_RelativeStart = -9999.0f;
/// 0x3C03126F = 0.008: fn_00438A40's start (the smallest "recent" belief of a rival that counts)
constexpr float k_RivalRecentStart = 0.008f;

// ---- Small methods (spec §2) ----------------------------------------------------------------------------------------

/// GBelief::Init 0x437DD0 (Town*): belief, pending, the reduce accumulator and lastAddedTurn 0, the cap 10 (0x437DDE..
/// 0x437DF4); belief[neutral] = Town +0x5D8 (fn_0073E4B0, raw, no clamp: 0x437DF6..0x437E2C); boredom[0..40] = 1.0
/// (0x437E30..0x437E40); the desire thresholds from GTownDesireInfo +0x3C (fn_00437E50 0x437E44). recent (+0x28) and
/// addedThisPeriod (+0x88) are NOT reset. Callers: the Town ctor 0x739523 (before +0x5D8 is set, so belief[neutral] is
/// 0 until the first fold) and Town::SetTownEmpty 0x74108E
void Init(components::Town& town);
/// fn_00437E50: desireThreshold[d] = GTownDesireInfo[d] +0x3C desireAffectsBeliefAfter (0 without the info)
void ResetDesireThresholds(components::TownBelief& belief);
/// GBelief::SetBelief 0x4387D0 (n, v): belief[n] = cap[n] < v ? cap[n] : v (0x4387D4 fcomp; test ah, 1). No lower clamp
void SetBelief(components::TownBelief& belief, PlayerNames player, float value);
/// GBelief::GetBeliefInPlayer 0x437E70 / 0x437E90 (n / P): belief[n] (0 for a slot out of range: openblack's guard)
[[nodiscard]] float GetBeliefInPlayer(const components::TownBelief& belief, PlayerNames player);
/// Town::GetBeliefInPlayer 0x73BAB0 of a town entity (0 without one)
[[nodiscard]] float GetBeliefInPlayer(entt::entity town, PlayerNames player);
/// GBelief::SetBeliefInPlayerCap 0x438A00 / GetCap 0x438A20 (P): cap[P.number]
void SetCap(components::TownBelief& belief, PlayerNames player, float cap);
[[nodiscard]] float GetCap(const components::TownBelief& belief, PlayerNames player);
/// 0x438060 (and 0x437E80, no direct caller): addedThisPeriod[n]
[[nodiscard]] float GetAddedThisPeriod(const components::TownBelief& belief, PlayerNames player);
/// 0x438070 (no direct caller, §8 Q7): added[n] == 0 ? 0 : added[n] / (belief[n] x 10)
[[nodiscard]] float GetAddedThisPeriodRatio(const components::TownBelief& belief, PlayerNames player);
/// Town::SetBeliefInPlayer 0x73BA70 (P, f) (SET_TOWN_BELIEF 0x71542B): the neutral player -> Town +0x5D8 = f
/// (0x73BA87); then SetBelief(P.number, f)
void SetBeliefInPlayer(components::Town& town, PlayerNames player, float value);
/// GBelief::ReduceBelief 0x437FD0 (P, town, r): belief[n] -= r (0x437FE3..0x437FEB, no clamp); with a town centre
/// (+0x9A4) reduceAcc[n] += r and over 0.005 DrawBelief(-reduceAcc[n], centre, P) then reduceAcc[n] = 0 (0x437FEF..
/// 0x438041). That draw has a negative amount and so never shows anything (DrawBelief 0x438817)
void ReduceBelief(entt::entity town, PlayerNames player, float r);
/// GBelief::AddToBoredomMultiplier 0x438790 (reaction, f): b = boredom[reaction +0x24] + f; < 0 -> 0 (0x4387A2)
void AddToBoredomMultiplier(components::TownBelief& belief, size_t reaction, float f);
/// GameThingWithPos::GetBoredomMultiplier 0x56FE70: a town -> boredom[reaction], none -> 1.0
[[nodiscard]] float GetBoredomMultiplier(entt::entity town, size_t reaction);
/// GBelief::GetMaxBeliefMeNotIncluded 0x4389B0 (n): the largest belief[i], i != n, from 0, strict >
[[nodiscard]] float GetMaxBeliefMeNotIncluded(const components::TownBelief& belief, PlayerNames player);
/// fn_00438910 (P, thing): thing's player == P -> the belief of the slot i != P with the largest belief[i] - belief[P]
/// (from -9999, strict >); else belief[thing's player]. It returns belief[i] itself, not the difference (0x438970..
/// 0x438975 fstp of GetBeliefInPlayer(i), returned at 0x438983)
[[nodiscard]] float RelativeBelief(const components::TownBelief& belief, PlayerNames player, PlayerNames thingPlayer);
/// fn_00438A40 (P) (the computer player 0x66ABD8): over the slots 0..5 not P and not allied, the largest recent[i]
/// above 0.008 (strict >); none or belief[P] == 0 -> 0; else x = 2 belief[i] / belief[P] and min(x^2, 1)
/// (0x438AC9..0x438AFD). `allied` (pending: openblack has no alliances) may be empty
[[nodiscard]] float RivalRecentRatio(const components::TownBelief& belief, PlayerNames player,
                                     const std::function<bool(PlayerNames)>& allied = {});
/// fn_00438B20 (P) (Town::ShuffleVillagersAroundAbodes 0x741945): the largest belief of the active non-allied players
/// != P (from 0, strict >) / belief[P]; 0 when belief[P] == 0. "Active" = GetNextActivePlayer 0x5508D0 (the slots
/// 0..6 with +0x8E0 != 0); (approximate) openblack keeps no active flag: the 7 non-neutral slots. `allied` as above
[[nodiscard]] float BeliefRatioVsRivals(const components::TownBelief& belief, PlayerNames player,
                                        const std::function<bool(PlayerNames)>& allied = {});

// ---- The turn (spec §3, §4) -----------------------------------------------------------------------------------------

/// fn_004383D0 (this = &town +0x798, town; 0x4383D0, 0x3A0 bytes), Town::Process step 19 (0x7474FF..0x747506), once
/// per turn per town: pending x Town +0x5DC into belief and addedThisPeriod; the boredom; the desires that cost the
/// owner belief; the neutral belief pinned to +0x5D8; recent x 0.997; the conversion to the strongest player
void Fold(entt::entity town);
/// GBelief::ProcessOncePerTurn 0x4380B0 (static), GGame::ProcessTurn row 27 (0x54E6DF): on turn % 10 == 0 only, every
/// town's addedThisPeriod back to 0, the local player's sum over 0.001 -> ToolTips::ForceToolTips(0xEE1, sum x 1000)
/// (help::tooltips::Force) and the yellow floating number (pending), then fn_00438340 for the local player.
/// (pending, Motor's row 27) nothing calls it yet: apply_belief.py --motor wires it once Motor's GameLogicLoop leaves
/// its 0x54E6DF comment; until then addedThisPeriod is never reset and the tooltip / LosingBelief sprite never fire
void ProcessOncePerTurn();
/// fn_00438340 (P) 0x438340: the "losing belief" help sprite (spec §4.1); nothing on land 1 (g_game +0x205A08)
void CheckLosingBelief(PlayerNames player);
/// GPlayer::TakeOverTown fn_00649810 (newP, town): Town::owner (SetPlayer fn_0073A8F0 0x73A904) and +0xF20 = 0
/// (fn_0073A7D0 0x73A7F7). (pending) the rest of fn_00649810 / fn_0073A7D0: sound 0xCD (0x6498C9), SetWorship-
/// Percentage(0) 0x6498DC, fn_0073A9C0, +0x98C / +0x5D0 = 0, the worship site and citadel unlinked, the magic types,
/// the population into the new owner's GameStats +0x68 (0x649910), Reaction 0x1F (0x649937), the code after
/// 0x64995E; the list move fn_0064C090 (the tail of the new owner's list, next = 0 at 0x64C0C6) is town_process'
/// ProcessPlayers (approximate: TownsOf orders by Town::id)
void TakeOverTown(PlayerNames player, entt::entity town);
/// Town::SetTownEmpty 0x741080's belief part (Town::Process step 22, 0x747559): Init; SetBelief(i, +0x5D8) for the 8
/// slots; an owner other than the neutral player -> TakeOverTown(neutral, town). (pending, V12) not called yet: nobody
/// starts the empty countdown (+0xF20)
void SetTownEmpty(entt::entity town);

/// The lost-town scale [0xBF33F0]: SET_LOST_TOWN_SCALE (land script case 104, 0x717E85); 1.0 in GLandBalance::Init
/// 0x5E28A9. Kept by land_balance (LostTownScale / SetLostTownScale), reset with it when a map loads
[[nodiscard]] float LostTownScale();

// ---- DrawBelief and the belief sprites (spec §5) --------------------------------------------------------------------

/// GBelief::DrawBelief 0x438800 (f, thing, P; `ret 0xC`): v = ftol(f x 10000); v <= 0 -> nothing (0x438815); the point
/// is thing's GetLHPoint + GetHeight (vt +0x42C) (0x438827..0x438864; uninitialised without a thing in the original,
/// openblack 0); the debug "B n" ValueSpinner of g_game +0x14 bit 0x4000 is not ported (§8 Q3: off in a normal game);
/// colour = P's GetPlayerColour, else 0xFFFFFFFF (0x4388DD..0x4388EA); fn_0069B550(thing, &pos, v, colour) 0x4388F5
void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player);

/// One entry of the belief-sprite queue at 0xD4ED00 (24 bytes; the first dword, an uninitialised local, is left out)
struct BeliefSprite
{
	glm::vec3 position {};
	int32_t amount {0};
	uint32_t colour {0};
};
/// fn_0069B550 (thing, pos, amount, colour): pushed when [0xC02A08] == 1 (a constant 1, §8 Q4) and the queue holds
/// fewer than 400 (0x69B550..0x69B56B). `thing` is never read
void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour);
/// UR_BeliefSprite::ModifyAtomCollection 0x69B750 (SF_BeliefSprite): takes the LAST entry (0x69B781, LIFO). (pending)
/// openblack has no SF_BeliefSprite rule yet: nobody drains the queue, so nothing is drawn
[[nodiscard]] std::optional<BeliefSprite> PopBeliefSprite();
[[nodiscard]] size_t BeliefSpriteCount();

namespace detail
{
/// The help sprites the belief asks for: fn_0071CCC0 (LosingBelief, list 10), fn_0071CD70 (GeneralBad, list 13),
/// fn_0071CDF0 (GeneralGood, list 14)
enum class HelpSprite
{
	LosingBelief,
	GeneralBad,
	GeneralGood,
};
/// Test hook: when set it stands in for audio::guidance (the town the sprite is about)
using HelpSink = std::function<void(HelpSprite, entt::entity)>;
void SetHelpSinkForTests(HelpSink sink);
/// Test hook: when set it stands in for help::tooltips::Force (the text and the value)
using ToolTipSink = std::function<void(uint32_t, float)>;
void SetToolTipSinkForTests(ToolTipSink sink);
/// Test hook: the belief-sprite queue emptied
void ClearForTests();
} // namespace detail
} // namespace openblack::ecs::town_belief
