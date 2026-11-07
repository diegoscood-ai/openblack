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
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The town's belief: the per-player block of components::TownBelief, its per-turn fold (a step of the town's process)
// and the once-per-turn pass over every town (a row of the game turn). Adding belief is
// ecs::town_stores::AddToBelief.

namespace openblack
{
class EventManager;
} // namespace openblack

namespace openblack::state_hash
{
class Hasher;
} // namespace openblack::state_hash

namespace openblack::ecs::components
{
struct Town;
struct TownBelief;
} // namespace openblack::ecs::components

namespace openblack::ecs::town_belief
{
/// The 8 player slots of every row; PlayerNames::NEUTRAL = 7
constexpr size_t k_Players = 8;
/// The REACTION count of the boredom multipliers
constexpr size_t k_Reactions = 41;
/// The TOWN_DESIRE count of the thresholds
constexpr size_t k_Desires = 17;
/// Init: the cap of every player
constexpr float k_InitialCap = 10.0f;
/// Init: every boredom multiplier
constexpr float k_InitialBoredom = 1.0f;
/// The fold's boredom ceiling: a sum below it is stored, otherwise the multiplier stays
constexpr float k_BoredomCeiling = 1.0f;
/// ProcessOncePerTurn runs on turn % 10 == 0
constexpr uint32_t k_ProcessEvery = 10;
/// The belief made an int amount for the belief sprites (DrawBelief, the fold)
constexpr double k_AmountScale = 10000.0;
/// ReduceBelief's draw threshold (a double)
constexpr double k_ReduceDrawThreshold = 0.005;
/// The believers ProcessOncePerTurn needs to say anything
constexpr float k_ToolTipMinimum = 0.001f;
/// The believers of the tooltip and the floating number
constexpr float k_ToolTipScale = 1000.0f;
/// HELP_TEXT: the "believers gained" tooltip, with the count as %3.0f
constexpr uint32_t k_ToolTipBelieversGained = 0xEE1;
/// The floating number of the believers gained: yellow (0xAARRGGBB), written four wide
constexpr uint32_t k_BelieversGainedColour = 0xFFFFFF00;
constexpr int k_BelieversGainedWidth = 4;
/// The colour of a DrawBelief without a player
constexpr uint32_t k_NoPlayerColour = 0xFFFFFFFF;
/// The start of RelativeBelief's best difference
constexpr float k_RelativeStart = -9999.0f;
/// RivalRecentRatio's start (the smallest "recent" belief of a rival that counts)
constexpr float k_RivalRecentStart = 0.008f;

// ---- Small methods --------------------------------------------------------------------------------------------------

/// Init: belief, pending, the reduce accumulator and lastAddedTurn 0, the cap 10; belief[neutral] = the town's
/// beliefInNeutralPlayer (raw, no clamp); every boredom multiplier 1.0; the desire thresholds from the town desire
/// info. recent and addedThisPeriod are NOT reset. Callers: the town constructor (before beliefInNeutralPlayer is set,
/// so belief[neutral] is 0 until the first fold) and SetTownEmpty
void Init(components::Town& town);
/// desireThreshold[d] = the town desire info's desireAffectsBeliefAfter (0 without the info)
void ResetDesireThresholds(components::TownBelief& belief);
/// belief[n] = cap[n] < v ? cap[n] : v. No lower clamp
void SetBelief(components::TownBelief& belief, PlayerNames player, float value);
/// belief[n] (0 for a slot out of range: openblack's guard)
[[nodiscard]] float GetBeliefInPlayer(const components::TownBelief& belief, PlayerNames player);
/// The same for a town entity (0 without one)
[[nodiscard]] float GetBeliefInPlayer(entt::entity town, PlayerNames player);
/// cap[P]
void SetCap(components::TownBelief& belief, PlayerNames player, float cap);
[[nodiscard]] float GetCap(const components::TownBelief& belief, PlayerNames player);
/// addedThisPeriod[n]
[[nodiscard]] float GetAddedThisPeriod(const components::TownBelief& belief, PlayerNames player);
/// added[n] == 0 ? 0 : added[n] / (belief[n] x 10). No caller in the original
[[nodiscard]] float GetAddedThisPeriodRatio(const components::TownBelief& belief, PlayerNames player);
/// SET_TOWN_BELIEF: the neutral player -> beliefInNeutralPlayer = f; then SetBelief(P, f)
void SetBeliefInPlayer(components::Town& town, PlayerNames player, float value);
/// The take-town cheat (OBJECT_RELATIVE_BELIEF): taken by the neutral player, every one of `players` (the players in
/// the game and the neutral one) is believed in 0; else the one of them believed in most (strictly more, from 0) is
/// believed in half that, then `player` in that most plus `amount`. Each through SetBelief (capped). The owner is left
/// as it is: the next fold hands the town over
void PlayerTakeTownCheat(components::TownBelief& belief, PlayerNames player, float amount,
                         std::span<const PlayerNames> players);
/// The players the take-town cheat weighs: each player that has an entity on the land (`onLand`, in any order, with
/// repeats) and the neutral player, in the players' order. A player nobody plays on the land has no entity and is left
/// out
[[nodiscard]] std::vector<PlayerNames> PlayersOnLandAndNeutral(std::span<const PlayerNames> onLand);
/// SET_PLAYER_BELIEF: the belief in `player` becomes the belief in the town's owner times `share` (SetBelief, capped)
void SetBeliefRelativeToOwner(components::TownBelief& belief, PlayerNames owner, PlayerNames player, float share);
/// belief[n] -= r (no clamp); with a town centre reduceAcc[n] += r and over 0.005 DrawBelief(-reduceAcc[n], centre, P)
/// then reduceAcc[n] = 0. That draw has a negative amount and so never shows anything
void ReduceBelief(entt::entity town, PlayerNames player, float r);
/// b = boredom[reaction] + f; below 0 -> 0
void AddToBoredomMultiplier(components::TownBelief& belief, size_t reaction, float f);
/// A town -> boredom[reaction], none -> 1.0
[[nodiscard]] float GetBoredomMultiplier(entt::entity town, size_t reaction);
/// The largest belief[i], i != n, from 0, strict >
[[nodiscard]] float GetMaxBeliefMeNotIncluded(const components::TownBelief& belief, PlayerNames player);
/// thing's player == P -> the belief of the slot i != P with the largest belief[i] - belief[P] (from -9999, strict >);
/// else belief[thing's player]. It returns belief[i] itself, not the difference
[[nodiscard]] float RelativeBelief(const components::TownBelief& belief, PlayerNames player, PlayerNames thingPlayer);
/// (Used by the computer player) over the slots 0..5 not P and not allied, the largest recent[i] above 0.008 (strict
/// >); none or belief[P] == 0 -> 0; else x = 2 belief[i] / belief[P] and min(x^2, 1). `allied`, whether P is allied to
/// the slot (ecs::alliances::IsAllied), may be empty
[[nodiscard]] float RivalRecentRatio(const components::TownBelief& belief, PlayerNames player,
                                     const std::function<bool(PlayerNames)>& allied = {});

/// (openblack) The state hash's "town_belief" part: every town, newest first, its entity then its belief, its pending
/// and its recent belief per player and its boredom per reaction, so that two runs whose towns believe differently
/// differ (the "transform" kind of parts never sees it)
void RegisterStateHash();
/// What the part folds for one town
void HashTown(state_hash::Hasher& h, uint32_t entity, const components::TownBelief& belief);
/// (Used by the computer player, through the town's belief attack importance) the largest belief of the active players
/// != P that P is not allied to (from 0, strict >) / belief[P]; 0 when belief[P] == 0. "Active" = the slots 0..6
/// flagged active; (approximate) openblack keeps no active flag: the 7 non-neutral slots. `allied` as above
[[nodiscard]] float BeliefRatioVsRivals(const components::TownBelief& belief, PlayerNames player,
                                        const std::function<bool(PlayerNames)>& allied = {});

// ---- The turn -------------------------------------------------------------------------------------------------------

/// Once per turn per town, a step of the town's process: pending x beliefScale into belief and addedThisPeriod; the
/// boredom; the desires that cost the owner belief; the neutral belief pinned to beliefInNeutralPlayer; recent x 0.997;
/// the conversion to the strongest player
void Fold(entt::entity town);
/// A row of the game turn: on turn % 10 == 0 only, every town's addedThisPeriod back to 0, the local player's sum over
/// 0.001 -> the "believers gained" tooltip with sum x 1000 (help::tooltips::Force) and the yellow floating number of
/// the same count at the land under the cursor (HandSystemInterface::GetHoverLandPoint), then CheckLosingBelief for the local
/// player. Each player's summed belief goes to its GameStats (game_stats::WorldBelief). Game.cpp calls it every turn
void ProcessOncePerTurn();
/// The "losing belief" help sprite; nothing on land 1
void CheckLosingBelief(PlayerNames player);
/// Whether the local player hears a town taken over: won by the local player from another
[[nodiscard]] bool HearsTakeOver(PlayerNames previous, PlayerNames next, PlayerNames local);
/// A town changing hands, in the original's order (nothing when it is the player's already): the take-over sound
/// (HearsTakeOver); its worship share 0; its disciples held at their jobs freed; out of its old owner
/// (RemoveTownFromPlayer); Town::owner = player, at the tail of the new owner's list; into the new player's citadel (a
/// worship site where the town may have one); its buildings given to the new owner; every magic type it holds enabled
/// for the new owner; its centre's totem given to the new owner (VillageTotemSystemInterface::
/// TownChangedPlayer); its centre's spell icons given to the new owner (worship::town_centre::SetPlayer); the
/// population to the new owner's GameStats (game_stats::TownTakenOver); a REACTION 31 from the town centre (the town
/// without one) of the new owner; for a player other than the neutral one the town fireworks and the player's symbol
/// fountain at the centre; the influence drawn afresh. (pending) the nearest towns
void TakeOverTown(PlayerNames player, entt::entity town);
/// A town leaving its owner, as a change of hands and a town's deletion both do it: its empty countdown 0; every magic
/// type it holds no longer enabled for the owner; out of its worship site, whose building site, when it is in the town's
/// list, is deleted, and whose builders from the town decide what to do next; out of the owner's temple heart's
/// building site (LeaveHeartSite); each of its buildings off the owner (a built wonder's power goes)
void RemoveTownFromPlayer(entt::entity town);
/// A town leaving its owner leaves the owner's temple heart too, when the heart has a building site: the town's
/// builders there stop and decide what to do next, and when a site of the town's list is the heart's, the heart's site
/// leaves that list (it is not deleted). Part of a change of hands and of a town's deletion
void LeaveHeartSite(PlayerNames owner, entt::entity town);
/// The belief part of emptying a town (a step of the town's process): Init; SetBelief(i, beliefInNeutralPlayer) for
/// the 8 slots; an owner other than the neutral player -> TakeOverTown(neutral, town). (pending) not called yet:
/// nobody starts the empty countdown
void SetTownEmpty(entt::entity town);

/// The lost-town scale: SET_LOST_TOWN_SCALE (land script); 1.0 when the land balance is initialised. Kept by
/// land_balance (LostTownScale / SetLostTownScale), reset with it when a map loads
[[nodiscard]] float LostTownScale();

// ---- DrawBelief and the belief sprites ------------------------------------------------------------------------------

/// v = f x 10000 truncated; v <= 0 -> nothing; the point is thing's ground point + its height (uninitialised without a
/// thing in the original, openblack 0); the debug "B n" value spinner is not ported (off in a normal game); colour =
/// P's player colour, else 0xFFFFFFFF; then QueueBeliefSprite
void DrawBelief(float f, entt::entity thing, std::optional<PlayerNames> player);

/// One entry of the belief-sprite queue (the original's first field, an uninitialised local, is left out)
struct BeliefSprite
{
	glm::vec3 position {};
	int32_t amount {0};
	uint32_t colour {0};
};
/// Pushed when the queue holds fewer than 400 (the original's other condition is a constant flag that is always set).
/// The original's `thing` argument is never read
void QueueBeliefSprite(const glm::vec3& position, int32_t amount, uint32_t colour);
/// The belief sprite rule (Particles/Rules/Belief.cpp) takes every entry each turn, the LAST one first (LIFO)
[[nodiscard]] std::optional<BeliefSprite> PopBeliefSprite();
[[nodiscard]] size_t BeliefSpriteCount();

namespace detail
{
/// The help sprites the belief asks for: LosingBelief (list 10), GeneralBad (list 13), GeneralGood (list 14)
enum class HelpSprite
{
	LosingBelief,
	GeneralBad,
	GeneralGood,
};
/// Test hook: the belief-sprite queue emptied
void ClearForTests();
} // namespace detail

/// The game's handlers of the belief events: events::TownBeliefHelp plays the town's help sprite,
/// events::TownBeliefToolTip shows the tooltip. Added once when the game starts
void AddBeliefEventHandlers(EventManager& manager);
} // namespace openblack::ecs::town_belief
