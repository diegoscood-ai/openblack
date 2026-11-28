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

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// The shepherds (docs/wiki/beings/villager-jobs.md#shepherds): a villager sent for food may herd one of its town's
// flocks that no one herds. It walks to the flock, then up to its leader, joins it and looks it over; it takes the
// flock to drink at its home's water and brings it back, and lets it go. The states 91..99, 190..192 and 228 share
// one exit (ExitShepherding).

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// Turns a shepherd looks its flock over once it walks up to the leader
inline constexpr int16_t k_LookOverTurns = 50;
/// Turns the flock stays at the water
inline constexpr int16_t k_TurnsAtWater = 900;
/// The shepherd's short waits: before it decides again, and before it goes home
inline constexpr int16_t k_ShortWait = 20;
/// The flocks a villager weighs are scored by their distance up to this, in metres
inline constexpr float k_FlockDistanceLimit = 300.0f;
/// How far the shepherd looks for water when its home has none, in metres
inline constexpr float k_ShepherdWaterRadius = 400.0f;
/// The shepherds read their season from the normal shepherd's row of the jobs table
inline constexpr JobInfo k_ShepherdJob = JobInfo::NormalShepherd;
/// Above this the town wants food badly enough to take a flock's last animal
inline constexpr float k_HungryTown = 0.5f;
/// The animal the shepherd picks to lead to the store is nearer than this, in metres
inline constexpr float k_PickAnimalReach = 100.0f;
/// The shepherd walks on to the store only while the animal it leads is nearer than this, in metres
inline constexpr float k_LeadReach = 6.0f;
/// The animal is taken once it is nearer than this at the store's door, in metres
inline constexpr float k_TakeReach = 4.0f;
/// The members of the flock an animal is led away in keep this near, in metres
inline constexpr uint16_t k_LedFlockDistance = 4;
/// The turns the shepherd waits at the store's door before it takes the animal
inline constexpr int16_t k_WaitAtDoor = 5;
/// Further from the store's door than this the shepherd walks a quarter of the way at a time, in metres
inline constexpr float k_FarFromStore = 100.0f;
/// Further than this (and no further than the above) it walks this far at a time; nearer, straight to the door
inline constexpr float k_NearStore = 30.0f;
inline constexpr float k_StepNearStore = 20.0f;

/// A flock's score for the villager: GetDistanceModifier(distance, 300)
[[nodiscard]] float FlockScore(float distance);

/// What the shepherd does once it has looked its flock over
enum class FlockDecision : uint8_t
{
	/// The flock is down to the shepherd alone: it lets it go
	Release,
	/// Its town wants food: it leads an animal to the store
	Slaughter,
	/// It takes the flock to drink
	MoveToWater,
};
/// `members` counts the shepherd. Fewer than two: release; the town's food desire at its trigger or above with more
/// than two members, or with the desire above 0.5: slaughter; else water
[[nodiscard]] FlockDecision DecideWithFlock(std::size_t members, float foodDesire, float trigger);

/// How the shepherd walks to the store's door with the animal it leads, by its distance from the door
enum class StoreLeg : uint8_t
{
	/// A quarter of the way
	Quarter,
	/// 20 m
	Step,
	/// Straight to the door, then the slaughter
	Door,
};
[[nodiscard]] StoreLeg StoreLegFor(float distanceToDoor);
/// The food a slaughtered animal gives: its food value truncated when its kind is meat or vegetable food, else none
[[nodiscard]] int16_t SlaughterFood(float foodValue, FoodType foodType);

// ---- the finder and the job --------------------------------------------------------------------------------------

/// The town's flocks from the newest, those without a shepherd, the strictly best FlockScore above 0 of the distance
/// from the flock's place to the villager, and its score
[[nodiscard]] entt::entity FindFlockWithoutShepherd(entt::entity town, entt::entity villager, float& score);
/// The town's flocks from the newest, the first with no shepherd that has members
[[nodiscard]] entt::entity FirstFreeFlock(entt::entity town);
/// No flock given -> the town's first free flock (none -> 0); another's flock -> 0; the villager points at the flock
/// (out of any flock it is in), shepherds it and ShepherdGotoFlock
uint32_t VillagerBecomesShepherd(entt::entity villager, entt::entity flock);
/// The town's first free flock: out of the flock it is in, pointing at it and shepherding it, 1; none -> 0
uint32_t ShepherdLookForFlock(entt::entity villager);
/// No flock or one with no leader -> ShepherdLookForFlock, 0; the shepherds' season -> walk to the leader's place, then
/// 191, 1; else 0
uint32_t ShepherdGotoFlock(entt::entity villager);

// ---- the states (LivingActionSystem.cpp k_VillagerStateTable) ---------------------------------------------------

/// 91 SHEPHERD_LOOK_FOR_FLOCK
uint32_t ShepherdLookForFlockState(components::LivingAction& action);
/// 92 SHEPHERD_MOVE_FLOCK_TO_WATER: its home's water (else looked for within 400 m); the flock's place kept and moved
/// there, 900 turns then 94, the walk there into 96 at its flock's newest member's pace; no home, flock or water -> 192, 0
uint32_t ShepherdMoveFlockToWater(components::LivingAction& action);
/// 93 SHEPHERD_MOVE_FLOCK_TO_FOOD: 1, nothing else
uint32_t ShepherdMoveFlockToFood(components::LivingAction& action);
/// 94 SHEPHERD_MOVE_FLOCK_BACK: the flock's place back to where it was kept, the walk there into 192 at the newest
/// member's pace, 1; no flock -> 0
uint32_t ShepherdMoveFlockBack(components::LivingAction& action);
/// 95 SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK: one turn off its wait; at 0 DecideWithFlock: 192 (0), 190 or 92
uint32_t ShepherdDecideWhatToDoWithFlock(components::LivingAction& action);
/// 96 SHEPHERD_WAIT_FOR_FLOCK: one turn off its wait; at 0 the state it waits for (then none), else a wait of 20 and 95
uint32_t ShepherdWaitForFlock(components::LivingAction& action);
/// 97 SHEPHERD_SLAUGHTER_ANIMAL: one turn off its wait; at 0 with a flock, the animal within 4 m: out of the flock,
/// its food in the shepherd's hands, the animal walks to the shepherd and dies there, and the shepherd waits for the
/// store's drop-off (32) through 96; a wait of 20 either way. 1
uint32_t ShepherdSlaughterAnimal(components::LivingAction& action);
/// 98 SHEPHERD_FETCH_STRAY: 1, nothing else
uint32_t ShepherdFetchStray(components::LivingAction& action);
/// 99 SHEPHERD_GOTO_FLOCK
uint32_t ShepherdGotoFlockState(components::LivingAction& action);
/// 191 SHEPHERD_TAKES_CONTROL_OF_FLOCK: the shepherds' season and a flock -> into the flock, a wait of 50 and the walk
/// to the leader it had, into 95, 1; else 0
uint32_t ShepherdTakesControlOfFlock(components::LivingAction& action);
/// 190 SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER: the flock's animal nearest within 100 m and a working store: the shepherd
/// leaves the flock (no longer its shepherd), the animal goes into a flock of its own that the shepherd joins and
/// shepherds, its members 4 m apart and following it, a wait of 20 and 228, 1; else a wait of 20 and 95, 0
uint32_t ShepherdTakeAnimalForSlaughter(components::LivingAction& action);
/// 228 SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER: one turn off its wait, nothing more until it is out; then with no animal
/// left a wait of 20 and 95, 0; the animal within 6 m and a working store: a leg towards the store's door
/// (StoreLegFor, into 228 again, the last into 97 after a wait of 5) at the animal's pace, 1; no animal near: it turns
/// to its flock's newest member and waits 20, 0
uint32_t ShepherdCheckAnimalForSlaughter(components::LivingAction& action);
/// 192 SHEPHERD_RELEASES_CONTROL_OF_FLOCK: a flock -> a wait of 20, then GO_HOME, through 96, 1; else 0
uint32_t ShepherdReleasesControlOfFlock(components::LivingAction& action);
/// The exit of every shepherd state: the same exit -> 1; else the flock's place back to where it was kept, the shepherd
/// out of it and no longer its shepherd, and no flock. 1
uint32_t ExitShepherding(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager
