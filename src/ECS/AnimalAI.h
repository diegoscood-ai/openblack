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
#include <glm/mat3x3.hpp>

namespace openblack::ecs::animal_ai
{

/// The animals' per-turn AI like the original (docs/bw1-notes/animals.md; research dev\tmp_dis\animals\grazing_ai.md,
/// hand_death.md): Animal::ProcessState (0x417EE0) and the states of g_AnimalStateTable (0xD12108). The grazing
/// species (sheep, tortoise, cow, horse, pig) wander in their herd, graze, sleep and breed; the predators (lion, tiger,
/// leopard, wolf; ECS/AnimalPredators.cpp) also stalk, chase, pounce on and eat other animals; the hand, the physics and
/// death work for every ground species. The birds' flight is not done yet.

/// g_AnimalStateTable's states: 0..30 are LivingStates, 31..52 the animal ones
enum class AnimalState : uint8_t
{
	Invalid = 0,
	MoveToPos = 1,
	InScript = 4,
	FleeingFromObjectReaction = 6,
	LookingAtObjectReaction = 7,
	Flying = 10,
	Landed = 11,
	SetDying = 13,
	Dying = 14,
	Dead = 15,
	Drowning = 16,
	Downed = 17,
	BeingEaten = 18,
	WaitForAnimation = 23,
	FleeingAndLookingAtObjectReaction = 30,
	InHand = 24,
	MoveInFlock = 27,
	StartWander = 31,
	Wander = 32,
	Eat = 33,
	SeekSleep = 34,
	Sleeps = 35,
	SeekEnvironment = 36,
	StandardAction = 37,
	StartToEat = 38,
	FinishEating = 39,
	TargetPounce = 40,
	HuntingMoveToPos = 41,
	MoveToPosAndLookAround = 42,
	DecideWhatToDo = 43,
	SpecialMoveToPos = 44,
	FollowFlock = 45,
	LandOnObject = 46,
	LandAtPos = 47,
	InteractDecideWhatToDo = 48,
	FleeingFromPredatorReaction = 49,
	GivesBirth = 50,
	HideInLair = 51,
	SeekFood = 52,
};

/// One game turn for every animal (Living::ProcessLiving -> Animal::ProcessState), after the villagers.
/// `visualTime`: GGameInfo::GetVisualTime in hours (the big cats go to bed after 22:00, wolves hunt after 23:00)
void ProcessAnimalsTurn(float visualTime);

/// Reaction::CreateReaction(predator, 28) at a predator's construction (fn_0041FD30 0x41FD5C): the flee-from-predator
/// reaction is spread once, then, to the animals within 25 m (the per-turn re-spreading is off in the shipped game)
void SpreadPredatorReaction(entt::entity predator);

/// The top state (DECIDE_WHAT_TO_DO before the animal's first turn)
[[nodiscard]] AnimalState TopState(entt::entity entity);
/// +0xB4 bits 4-5: how it lay after its last landing (0 on its feet, 1 on its right side, 2 on its left, 3 none)
[[nodiscard]] uint16_t LandType(entt::entity entity);

/// Animal::ValidForPlaceInHand (0x419B40): GAnimalInfo.playerCanPickUp
[[nodiscard]] bool ValidForPlaceInHand(entt::entity entity);
/// Animal::InterfaceSetInMagicHand (0x419B60): leaves its flock for one of its own, IN_HAND
void PlaceInHand(entt::entity entity);
/// Living::InitialisePhysicsFromHand (0x5EFD80): every drop and throw is physics, FLYING
void InitialisePhysics(entt::entity entity);
/// Animal::EndPhysics (0x5F0D80) at rest: the landType from the body's right row, LANDED or dying. `rotation` is
/// the body's (openblack columns = LHMatrix rows).
void EndPhysics(entt::entity entity, const glm::mat3& rotation);
/// openblack's hand puts things down at once (the original drops them into physics): lands on its feet
void PutDown(entt::entity entity);
/// Animal::DestroyedByEffect (0x41B1B0) -> Living::SetDying (0x5EC390), when an effect took its last life. Nothing
/// while it flies: EndPhysics does it at rest.
void DestroyedByEffect(entt::entity entity);
/// The animal goes (sunk, deleted): off its flock's list
void Forget(entt::entity entity);

/// Test hooks, once per turn (ECS/AnimalDebugHooks.cpp)
void RunDebugHooks(uint32_t turn);

} // namespace openblack::ecs::animal_ai
