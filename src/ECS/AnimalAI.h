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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::animal_ai
{

/// The animals' per-turn AI like the original (docs/bw1-notes/animals.md; research dev\tmp_dis\animals\grazing_ai.md,
/// hand_death.md): Animal::ProcessState (0x417EE0) and the states of g_AnimalStateTable (0xD12108). The grazing
/// species (sheep, tortoise, cow, horse, pig) wander in their herd, graze, sleep and breed; the predators (lion, tiger,
/// leopard, wolf; ECS/AnimalPredators.cpp) also stalk, chase, pounce on and eat other animals; the hand, the physics and
/// death work for every ground species; the birds (ECS/AnimalBirds.cpp) fly in flocks and never land.

/// g_AnimalStateTable's states: 0..30 are LivingStates, 31..52 the animal ones
enum class AnimalState : uint8_t
{
	Invalid = 0,
	MoveToPos = 1,
	InScript = 4,
	FleeingFromObjectReaction = 6,
	GotoFoodReaction = 19,
	ArrivesAtFoodReaction = 20,
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

/// Pot::SetupReaction (0x66D660): a pot's reaction (food 7: the hungry grazers within 35 m come and eat 50 of it),
/// once until Pot::RemoveReaction (picked up, emptied, deleted). Called for the map's CREATE_POT, a pot the hand puts
/// down and the hand's new piles.
void SetupPotReaction(entt::entity pot);
void RemovePotReaction(entt::entity pot);
/// the map is unloaded
void ClearReactions();
/// Reaction 9 (Object::InitialisePhysicsFromHand 0x637412): anything the hand throws or drops offers itself once to the
/// predators within 25 m, which flee when it comes fast enough
void SpreadFlyingObjectReaction(entt::entity object);

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
void EndPhysics(entt::entity entity, const glm::mat3& rotation, const glm::mat3& turnStartRotation);
/// Object::EndPhysics (0x6375A0): the object's own reactions end (the animals reacting to it stop, their state kept)
void EndReactionsOf(entt::entity object);
/// openblack's hand puts things down at once (the original drops them into physics): lands on its feet
void PutDown(entt::entity entity);
/// Animal::DestroyedByEffect (0x41B1B0) -> Living::SetDying (0x5EC390), when an effect took its last life. Nothing
/// while it flies: EndPhysics does it at rest.
void DestroyedByEffect(entt::entity entity);
/// The animal goes (sunk, deleted): off its flock's list
void Forget(entt::entity entity);

// ---- for the spells and scripts (ECS/AnimalApi.cpp; the Flock miracle, session Milagros) ----

/// AnimalArchetype::Create (fn_00419D10 with a flock, fn_00419C20 without; age 0 = the random one) and the player
/// that owns it (Animal::player; -1 none). entt::null for the species the original doesn't make.
entt::entity CreateAnimal(const glm::vec3& position, AnimalInfo type, entt::entity flock, uint32_t age, int32_t player);
/// Living::SetupMoveToPos: the info's move state towards the point (the birds at that altitude over the land), then
/// `final`
void MoveTo(entt::entity entity, glm::vec2 position, float altitude, AnimalState final);
/// Living::SetTopState: the state with its clip
void SetState(entt::entity entity, AnimalState state);
/// Living::SetState(0, state) (vt+0x938): the top state only, the clip stays
void SetStateRaw(entt::entity entity, AnimalState state);
/// the SpellWolf's final destination (+0x148): it runs there (SetRunToFinalDest) and dies
void SetFinalDestination(entt::entity entity, glm::vec2 position);
/// the final state (vt+0x860 GetDestPos): the goal of its move, x / altitude over the land / z
[[nodiscard]] std::optional<glm::vec3> Destination(entt::entity entity);
/// Living::SetDying (vt+0x6A4) calls this first, once per death
using DeathCallback = std::function<void(entt::entity)>;
void SetDeathCallback(DeathCallback callback);
/// killed (SetDying: its dying and dead clips, the corpse) or gone at once (off its flock, out of the physics, deleted)
void Kill(entt::entity entity);
void Remove(entt::entity entity);
/// fn_0041AA00 (GScript::ReleaseScriptThingIntoTheGame 0x70F6B1): the last script reference of a script-controlled
/// animal has gone (ECS/ScriptHeld.h): a flock of its own if it has none, then dying if dead, else
/// INTERACT_DECIDE_WHAT_TO_DO (ECS/AnimalScript.cpp)
void ReleaseFromScript(entt::entity entity);
/// per-instance opacity (components::Alpha, the alpha-blended pass); 1 takes it off
void SetAlpha(entt::entity entity, float alpha);

/// the Dove class (crows, doves, swallows, pigeons, seagulls, bats and the spell ones)
[[nodiscard]] bool IsFlyingSpecies(AnimalInfo type);

/// Test hooks, once per turn (ECS/AnimalDebugHooks.cpp)
void RunDebugHooks(uint32_t turn);

} // namespace openblack::ecs::animal_ai
