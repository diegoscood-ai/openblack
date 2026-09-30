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

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/AnimalAI.h"
#include "ECS/Map.h"
#include "Enums.h"

namespace openblack
{
struct GAnimalInfo;
}

namespace openblack::ecs::components
{
struct Animal;
struct AnimalBrain;
struct Flock;
struct Transform;
} // namespace openblack::ecs::components

/// The animal AI's internals shared by ECS/AnimalAI.cpp (the turn, the grazers, the hand and death) and
/// ECS/AnimalPredators.cpp (the predators' decisions and their hunting).
namespace openblack::ecs::animal_ai::detail
{

constexpr float k_MapCoordsPerMetre = 6553.6f;
constexpr int32_t k_Circle = 2048;
/// the returns of the original's "did something" tests (ReactToAnimalNeeds, KeepLeaderWithinDomain...)
constexpr int k_Started = 0x23;
constexpr int k_Nothing = 0x24;
/// Living::GetNumTurnsToDieOver (0x5EC3E0)
constexpr int16_t k_TurnsToDieOver = 600;

struct Context
{
	entt::entity entity;
	components::Animal& animal;
	components::AnimalBrain& brain;
	components::Transform& transform;
	const GAnimalInfo& info;
};

/// the predators' code (dev\tmp_dis\animals\predator_ai.md §0): Tiger and Leopard run the Lion code (the tiger has its
/// own lair), the Wolf its own decide, needs and lair
enum class Hunter
{
	None,
	Cat,
	Tiger,
	Wolf,
};

/// GGameInfo::GetVisualTime (hours) of this turn and g_game+0x205A40, the game turn
extern float g_VisualTime;
extern uint32_t g_Turn;

const GAnimalInfo& InfoOf(const components::Animal& animal);
bool IsGrazer(AnimalInfo type);
Hunter HunterOf(AnimalInfo type);

/// GUtils::Spiral (0x74D7E0): every caller starts with dir 1, count 1, tests its own cell, then adds each step (whole
/// 10 m cells, the sub-cell offset kept): (-1, 0), (0, -1), (+1, 0) x2, (0, +1) x2, (-1, 0) x3...
struct Spiral
{
	int dir {1};
	int count {1};
	glm::ivec2 Next()
	{
		if (--count == 0)
		{
			++dir;
			count = dir / 2;
		}
		static constexpr glm::ivec2 k_Steps[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
		return k_Steps[dir & 3];
	}
};

// MapCoords and angles (2048 per circle)
glm::ivec2 Step(uint16_t angle, uint32_t speed);
/// GUtils::GetAngleFromDXDZ (0x74D200) on metres (converted to MapCoords) and on raw MapCoords / whole units
uint16_t AngleOf(glm::vec2 d);
uint16_t AngleOfMapCoords(int32_t dx, int32_t dz);
int32_t AngleDiff(uint16_t a, uint16_t b);
float Metres(uint32_t speed);
glm::vec2 Xz(const components::Transform& transform);
MapInterface::CellId CellOf(glm::vec2 p);
void FaceAngle(components::Transform& transform, uint16_t angle);

// the land
bool InBounds(glm::vec2 p);
/// Object::Collide(collideType) (0x6033B0): off the map collides with everything; type 1 with the water cells
bool Collides(glm::vec2 p, uint32_t collideType);
glm::vec2 SquarePos(glm::vec2 c, float size);
/// Living::CalcRandomPos (0x5ED080)
glm::vec2 CalcRandomPos(const Context& ctx, glm::vec2 c, float rMin, float rMax);
/// Animal::IsPosValidForTurnAngle (0x41B210): outside both of its turning circles
bool IsPosValidForTurnAngle(const Context& ctx, glm::vec2 p);
/// Flock::SetDomainCentrePos (0x52FC20): also the leader's move goal
void SetDomainCentre(components::Flock& flock, glm::vec3 position);
bool Available(entt::entity entity);

// the flock
components::Flock* FlockOf(const components::Animal& animal);
entt::entity LeaderOf(const components::Flock& flock);
glm::vec2 FlockPos(const Context& ctx);
bool PosWithinDomain(const Context& ctx, glm::vec2 p);
bool IsLeader(const Context& ctx);

// Living::SetDying (vt+0x6A4) and the hook the spells use (AnimalAI.h SetDeathCallback)
void SetDying(entt::entity entity, components::AnimalBrain& brain);
void Delete(entt::entity entity);
extern DeathCallback g_DeathCallback;

// states and moving
void SetTopState(entt::entity entity, components::AnimalBrain& brain, AnimalState state);
void SetTopState(Context& ctx, AnimalState state);
void PlayAnimThenSetState(Context& ctx, AnimalState state);
void SetSpeed(Context& ctx, uint32_t speed);
uint32_t SpeedDefault(const Context& ctx);
void SetupMoveToPos(Context& ctx, glm::vec2 p, AnimalState final);
/// MobileWallHug::SetupMobileMoveToPos (0x60AAD0, one argument): the goal, InitStepsXZ, ARRIVED or STEP_THROUGH
void SetupMobileMoveToPos(Context& ctx, glm::vec2 p);
/// MobileWallHug::MoveTo for ARRIVED / FINAL_STEP / STEP_THROUGH: 6 or 7 when it stepped (same / new map cell), 0xA
/// when it got there, 0 otherwise
int MoveTo(Context& ctx);
constexpr uint8_t k_MoveArrived = 1;
constexpr uint8_t k_MoveFinalStep = 4;
constexpr uint8_t k_MoveWander = 5;
constexpr uint8_t k_MoveStepThrough = 0xB;
bool MoveBy(Context& ctx, glm::ivec2 step);
void SetTowardsAngle(Context& ctx, uint16_t target, float distance);
int CheckNeeds(Context& ctx);
int KeepLeaderWithinDomain(Context& ctx);
int KeepFlockMemberWithinFlockArea(Context& ctx);
void StartWander(Context& ctx);
/// Animal::LookForFlocksInSpiral (0x41A690) with merge: the bigger flock of the same species and player keeps everyone
void LookForFlocksInSpiral(Context& ctx, float radius, bool merge);
/// Living::GetAge (0x5ECAF0): (turn - birth) / 1500 turns per year
[[nodiscard]] uint32_t AgeOf(const components::AnimalBrain& brain);

// the reactions (ECS/AnimalFlee.cpp)
void ProcessReaction(Context& ctx);
/// the reactions whose initiator was deleted go
void PruneReactions();
/// the states whose exit function is Animal::ExitReaction (0x41B170)
bool IsReactionState(uint8_t state);
/// Animal::ExitReaction: leaving the reaction states the reaction is dropped (the state kept)
void ExitReaction(components::AnimalBrain& brain, uint8_t next);
void FleeingFromPredatorReaction(Context& ctx);
void GotoFoodReaction(Context& ctx);
void ArrivesAtFoodReaction(Context& ctx);
void FleeingFromObjectReaction(Context& ctx);
void FleeingAndLookingReaction(Context& ctx);
components::AnimalBrain* BrainOf(entt::entity entity);

// the birds (ECS/AnimalBirds.cpp)
bool IsBird(AnimalInfo type);
void BirdDecideWhatToDo(Context& ctx);
void BirdStartWander(Context& ctx);
int BirdReactToAnimalNeeds(Context& ctx);
void SpecialMoveToPos(Context& ctx);
void FollowFlock(Context& ctx);
void BirdDying(Context& ctx);
/// GetTimeToBank / GetBankAngle (vt+0xBDC / +0xBE0): Dove 2 s / 0.5 rad, the spell birds 0.5 s; 0 for the ground ones
float TimeToBank(AnimalInfo type);
float BankAngle(AnimalInfo type);

// the predators (ECS/AnimalPredators.cpp)
void SetRunToFinalDest(Context& ctx);
/// the villagers a predator caught: DOWNED, BEING_EATEN 300 turns, dead
void ProcessDownedVillagers();
void PredatorDecideWhatToDo(Context& ctx);
int PredatorReactToAnimalNeeds(Context& ctx);
void HuntingMoveToPos(Context& ctx);
void TargetPounce(Context& ctx);
void BeingEaten(Context& ctx);
void HideInLair(Context& ctx);
void CalculeLairPos(Context& ctx);

} // namespace openblack::ecs::animal_ai::detail
