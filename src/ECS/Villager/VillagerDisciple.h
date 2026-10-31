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
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/DiscipleRules.h"
#include "Enums.h"

// The disciples: the disciple table, SetVillagerDisciple, DiscipleDecideWhatToDo and the state 221
// DISCIPLE_NOTHING_TO_DO. A disciple has the villager flag k_FlagDisciple, with its VILLAGER_DISCIPLE in
// Villager::discipleType; another flag marks a disciple follower. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the disciple table (13 records) -----------------------------------------------------------------------------

/// One record of the disciple table, as the original has it (7 dwords)
struct DiscipleInfo
{
	uint32_t startState;         ///< A VILLAGER_STATES byte: the state set when the disciple lands, 0: inspect
	uint32_t createsJobReaction; ///< != 0: DecideWhatToDo creates the reaction 0x18
	uint32_t marker;             ///< The value given to the 3D object when it becomes a disciple
	uint32_t heldAtJob;          ///< == 1: held at its job (read by the hunger and periodic checks, among others)
	uint32_t fetchesWood;        ///< != 0: interested in wood objects
	int32_t townDesire;          ///< The TOWN_DESIRE_INFO it serves, -1 none
	uint32_t movesIntoTown;      ///< != 0: on landing it checks moving into the town
};

/// The number of records (SetVillagerDisciple accepts 0 <= disciple < 13)
inline constexpr size_t k_DiscipleInfoCount = 13;

/// The disciple's record. (guard) Past the table: record 0 (the original never indexes past 12: SetVillagerDisciple
/// keeps the type in 0..12)
[[nodiscard]] const DiscipleInfo& GetDiscipleInfo(uint8_t disciple);
/// The start state, read as a byte: the state the disciple starts in
[[nodiscard]] VillagerStates DiscipleStartState(uint8_t disciple);
/// The reaction field != 0: 1, 2, 3, 4, 6, 8
[[nodiscard]] bool DiscipleCreatesJobReaction(uint8_t disciple);
/// The value SetVillagerDisciple gives the 3D object (0 for 0, 11, 12). (inferred) a disciple marker
[[nodiscard]] uint32_t DiscipleMarker(uint8_t disciple);
/// == 1: a working disciple held at its job (1..6, 8, 9); 0 lives normally (0, 7, 10, 11, 12)
[[nodiscard]] bool DiscipleHeldAtJob(uint8_t disciple);
/// != 0: FORESTER, BUILDER and CRAFTSMAN fetch wood
[[nodiscard]] bool DiscipleFetchesWood(uint8_t disciple);
/// The town desire the disciple serves, -1 none (FOOD 0, WOOD 1, 8, 9)
[[nodiscard]] int32_t DiscipleTownDesire(uint8_t disciple);
/// != 0: dropped on another town's object, it moves into that town
[[nodiscard]] bool DiscipleMovesIntoTown(uint8_t disciple);

// ---- the disciple ------------------------------------------------------------------------------------------------

/// Outside 0..12 -> 0, nothing done. Else the town's TownStats counts move from the old type to the new; a disciple: the
/// disciple flag set and the follower flag cleared, (pending) the 3D object's marker, the type stored; 0: both flags
/// cleared, (pending) the marker cleared, the type 0. 1. `thing` and `h` are not read
uint32_t SetVillagerDisciple(entt::entity villager, entt::entity thing, VillagerDisciple disciple, int32_t h);
/// One case per disciple type 1..12 (any other -> the fallback). 1 when the disciple found something to do
uint32_t DiscipleDecideWhatToDo(entt::entity villager);

// ---- 221 DISCIPLE_NOTHING_TO_DO ----------------------------------------------------------------------------------

/// FindDisciplePrayerPos -> the countdown reset, SetupMoveToWithHug(p, 221); 1. 0 when there is no point
uint32_t SetDiscipleNothingToDo(entt::entity villager);
/// No town -> 0; no town centre -> out = the town's position; else out = the centre + GetPosFromAngle(
/// Get3DAngleFromXZ(centre, me) + GameFloatRand(pi / 2) - pi / 4, GameFloatRand(4) + the centre's Get2DRadius). 1
uint32_t FindDisciplePrayerPos(entt::entity villager, glm::ivec2& out);
/// 221: the town's build pulse -> countdown = GameRand(10); countdown - 1 (signed 16 bits) > 0 -> 1; at 0 the clip
/// not done -> countdown = 1; else DiscipleDecideWhatToDo == 0 -> countdown = 300. Always 1
uint32_t DiscipleNothingToDo(components::LivingAction& action);
/// 221's entry: a town with a centre -> look at the centre. Always 1
uint32_t EnterDiscipleNothingToDo(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The breeder's job check (VillagerBreeding.cpp): pregnant -> GO_HOME, 1. No town, or its town too far over full -> 0.
/// Else a spot out from the door of its town's nearest home within 300 m (along the footpaths) or, with none, by its
/// town's food drop-off; it walks there into BREEDER_DISCIPLE, lets go of its target and keeps the spot as its wander
/// area; 1
uint32_t SetupBreederDisciple(entt::entity villager);

// ---- the hand's disciples ----------------------------------------------------------------------------------------
// A villager in the player's hand takes up, each turn, the work of what it is held near (ECS/DiscipleRules.h); put down
// gently it sets about that work.

/// 24 IN_HAND's entry: discipleOnPickUp = its disciple, a missionary's none (disciple::AtPickUp). Always 1
uint32_t EnterInHand(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The things round the villager that draw it, the most interesting first: those whose kind has a villager interact
/// desire, in the nine map cells of a spiral from its own, by the gap between their edges (disciple::Interest)
[[nodiscard]] std::vector<entt::entity> DiscipleThingsNear(entt::entity villager);
/// IN_HAND's turn in the player's hand (the in-hand flag): the first thing round it that offers it work it can take
/// makes it that thing's disciple, else it goes back to discipleOnPickUp (disciple::ChooseWhileHeld). A trader keeps the
/// town of the storage pit it was held by
void DiscipleInHand(entt::entity villager);
/// What the choice reads of the held villager and the hand: the hand's player and its own, its sex and age, and whether
/// it is refused all work but worship (a script holds it, it isn't available, a skeleton, dead or dying, or playing
/// football)
[[nodiscard]] disciple::HeldFacts HeldFactsOf(entt::entity villager);
/// Thrown from the hand with dontReplant: one of no player, or of the hand's player, is no disciple any more
void DiscipleThrownFromHand(entt::entity villager, bool dontReplant);
/// Let go of by the hand into a flight, or with no body: back to discipleOnPickUp
void DiscipleFlownFromHand(entt::entity villager);
/// Put down gently by the local player's hand: a disciple other than discipleOnPickUp, the guidance's disciple remark
void DisciplePutDownByHand(entt::entity villager);
/// LANDED's first turn, put down gently as a disciple: the hand's player's creature is told the disciple deed of its type
/// (creature_watching::DiscipleDeed), whatever the type was at the pick-up
void ShowDiscipleToCreature(entt::entity villager);
/// LANDED's first turn, put down gently as a disciple: a disciple other than discipleOnPickUp moves the hand's player's
/// alignment by what its town wants of the work (disciple::AlignmentForMaking, alignment::UpdateForDisciple)
void DiscipleTouchDown(entt::entity villager);
/// LANDED after its clip, put down gently as a disciple: nothing round it -> 0. Else the player's GameStats count a
/// disciple other than discipleOnPickUp; a kind that moves into town (DiscipleMovesIntoTown) put by a thing of another
/// town moves into it (CheckMoveIntoTown) and takes on its tribe; then it sets about the work: its start state, or for start
/// state 0 an inspection of the first thing (INSPECT_OBJECT, walking up to it when it is farther than 5 m from its edge); a
/// start state openblack has not ported yet -> DECIDE_WHAT_TO_DO. 1
uint32_t DiscipleLanded(entt::entity villager);
/// 233 INSPECT_OBJECT: facing the target (LookAtPos step 0x80), then once the clip is done a builder sets up the
/// building of a target under construction (SetupBuildingObjectForBuilding); otherwise DECIDE_WHAT_TO_DO. Always 1
uint32_t InspectObject(components::LivingAction& action);
/// 233's validate: a target no longer available is let go of
bool ClosestObjectValidate(components::LivingAction& action);
/// A town changing hands: every disciple living in its abodes that is held at its job is no disciple any more, then its
/// missionaries are deleted (DeleteAllMissionaries)
void ResetAllDiscipleStates(entt::entity town);
} // namespace openblack::ecs::villager
