/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The Villager handler of ECS/Effects/Reactions: applies a reaction to a villager of a cell, by reaction type (the
// ported ones: REACT_TO_FIRE in VillagerFire.cpp, REACT_TO_TELEPORT in VillagerTeleport.cpp; the rest reach no
// villager yet).

// Also the villager's side of a reaction's end that the reaction states share (the reaction it follows and its
// object: kept by VillagerFire.cpp and VillagerTeleport.cpp for their reaction).
//
// The types of the villager type table go through the original's per-villager logic with one shared slot
// (components::LivingReaction: the reaction and its object): the body that applies a reaction to a villager
// (availability, the score, the records, the switch rule), AddReaction, ProcessReaction and the reaction's
// shut-down. Registered: 7 REACT_TO_FOOD and 12 REACT_TO_WOOD (VillagerResourceReactions.h), and the miracles a
// villager runs from or stops to watch: 3 FLEE_FROM_SPELL (it runs ten metres, straight away from a still miracle or
// across the way of a moving one, then watches it once it is beyond the near distance and not coming at it, and gives
// up beyond the far one), 21 LOOK_AT_NICE_SPELL and 37 REACT_TO_IMPRESSIVE_SPELL (it turns to watch, and gives up
// beyond the far distance), and 9 REACT_TO_FLYING_OBJECT, a thing thrown from the hand (it runs from it as from a
// miracle when it is nearer than it flies in two seconds, else it points at it until it is no longer in the air), and
// 30 REACT_TO_BREEDER, a breeder disciple's call (ECS/Villager/VillagerBreeding.cpp), 28 FLEE_FROM_PREDATOR, a
// predator about (it runs from it, watches it, or hides from it in a building of its town and looks out until it is
// safe, VillagerCover.cpp), and 6 REACT_TO_CREATURE, a creature about (it inspects it, respects it or runs from it by
// its town's attitude, VillagerCreature.cpp). The other miracle types (10, 20, 13 and 23) keep their maps and code: they are
// only scored as "the current reaction" when a slot type meets them, and the slot and those maps are never held at once.

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::effects::reactions
{
struct Reaction;
enum class CellWalk : uint8_t;
} // namespace openblack::ecs::effects::reactions

namespace openblack::ecs::villager_reactions
{
/// SetLivingReactionHandler(Villager, ...) and SetLivingShutDownHandler(Villager, ...) (the slot holders' side of a
/// reaction's shut-down), once the game's reactions exist (before any map load)
void RegisterHandlers();
/// RegisterHandlers and the join order back to 0: at every map load (villager_fire::Clear, villager_teleport::Clear)
void Register();

/// SetTopState for the reactions' states: the villager core's. The walk a change leaves is kept (MOVE_TO_POS's exit
/// only lets go of the circle it was hugging), to go on with if the villager comes back to it
uint32_t SetTopState(entt::entity villager, VillagerStates state);
/// SetTopState of the stored state's resume state (field0x20); 0x2E -> a raw store of 163 in index 0; then a raw store
/// of 0 in index 2
void PopFromPrevious(entt::entity villager);
/// PopFromPrevious, then SetTopState(163) if the final state is a reactive state (field0xb8)
void ResetStateAfterReacting(entt::entity villager);
/// ResetStateAfterReacting, then StopReacting if still reacting
void StopReactingAndSetState(entt::entity villager);
/// It follows a reaction (the fire's, the teleport's, the shield's, the mourning's or a slot type's)
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The reaction's record gets the turn, the reaction and its object are cleared (the miracle maps and the slot)
void StopReacting(entt::entity villager);
/// The validate slot of the state table for the reaction rows (201, 202, 251, 215-218, 220, 6-30, 140-146; run each
/// turn for the TOP and the stored state, before the state itself). PopFromPrevious when the reaction's object is
/// none or not available, or when the reaction's info row says whetherReactionFinishesIfInitiatorInHand and the
/// object is in the hand. The original returns nothing; this returns false when it popped. The signature is the
/// table's `validate` column (ECS/Villager/VillagerStateTable.h); LivingActionSystem::VillagerCallValidate calls it
/// for every row with no validate of its own whose original validate is this one (VillagerStateFunctions.h).
bool ReactionValidate(components::LivingAction& action);
// ---- the slot types ---------------------------------------------------------------------------------------------

/// One row of the villager type table for a slot type (the villager's functions for that type)
struct SlotType
{
	bool sameTypeSwitch {false}; ///< effects::reactions::SameTypeSwitch
	/// What starting to react calls (initiator, reaction): the type's Setup*
	void (*setup)(entt::entity villager, entt::entity object, uint32_t reaction) {nullptr};
	/// The score's priority (reaction, other): 0 = don't
	uint8_t (*priority)(entt::entity villager, uint32_t reaction, uint32_t other) {nullptr};
	/// ProcessReaction's limit (object, type, distance)
	uint32_t (*turnsToReact)(entt::entity villager, entt::entity object, uint8_t type, float distance) {nullptr};
	/// The turns before reacting again (initiator, type, distance)
	uint32_t (*turnsBeforeAgain)(entt::entity villager, entt::entity object, uint8_t type, float distance) {nullptr};
};
/// The Villager handler of effects::reactions (SetLivingReactionHandler): applies a reaction to one villager of a cell
/// at the spread's distance d. A slot type -> its per-villager body; else, with no slot held, the miracle dispatch as
/// before (fire / teleport / shield / death); a miracle type meeting a slot holder -> the switch rule first; a reaction
/// that only impresses -> the villager is impressed, takes nothing up, and ends the cell's walk. Public for the tests
effects::reactions::CellWalk HandleReaction(entt::entity villager, const effects::reactions::Reaction& reaction,
                                            float distance);
/// The row of a type, nullptr when it is not a slot type
[[nodiscard]] const SlotType* SlotTypeOf(openblack::Reaction type);
/// The villager follows a slot reaction
[[nodiscard]] bool SlotHeld(entt::entity villager);
/// The slot's reaction (0 none)
[[nodiscard]] uint32_t SlotReaction(entt::entity villager);
/// The slot's object (entt::null none)
[[nodiscard]] entt::entity SlotObject(entt::entity villager);
/// The setups store the object, after AddReaction
void SetSlotObject(entt::entity villager, entt::entity object);
/// The reaction a villager follows now: its object and its kind, whatever keeps it (the slot or a miracle's map)
struct FollowedReaction
{
	entt::entity object {entt::null};
	openblack::Reaction kind {openblack::Reaction::None};
};
/// The reaction a villager follows now (a null object and no kind when it follows none). A set-up reads it before
/// AddReaction, which drops it
[[nodiscard]] FollowedReaction FollowedReactionOf(entt::entity villager);
/// How much a reaction impresses a villager as it takes it up: a reaction with a player gives belief in that player to
/// the villager's town (its initiator's impressive value, a miracle's, a missionary's, a scaffold's or an object's, by
/// the villager's share of its town; with `draw` the belief
/// rises above the villager), moves the player's alignment by the reaction's own amount, and tires the town of that
/// kind of reaction. Nothing for a villager with no town or a reaction with no player. (pending) a creature's own value
void UpdateHowImpressed(entt::entity villager, uint32_t reaction, bool draw);
/// What a scaffold's site's town wants of the building, which a scaffold's reaction is multiplied by: its raw desire for
/// abodes when the site builds an abode, for civic buildings otherwise; 1 without a site. (openblack) a site whose
/// building is gone, which the original would not survive, wants 0
[[nodiscard]] float ScaffoldSiteWant(entt::entity scaffold);
/// (reaction, state): how impressed it is (UpdateHowImpressed, drawn), then: no reaction yet -> StorePreviousState;
/// SetTopState(state); dancing -> out of the dance (TODO(dance)); the reaction is stored (a miracle map the villager
/// held is dropped here without refreshing its record, as the original only unlinks on a switch); the follower link
void AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state);
/// The same for the one setup whose object must be the villager's before its state changes (a creature's, whose
/// inspecting clip looks at it): the object is set first, on the reaction the villager follows if it follows one, and
/// kept unless the state change stopped the villager reacting
void AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state, entt::entity object);
/// For a slot holder (villager::ProcessReaction calls it before ProcessState): the reaction gone -> StopReacting; the
/// object none or not available -> StopReactingAndSetState; turns since its record (signed) > the type's
/// turnsToReact(object, type, the distance to the object) -> StopReactingAndSetState. Nothing for the miracle maps
/// (unchanged)
void ProcessSlotReaction(entt::entity villager);
/// The reaction score for a villager: 0 unless the villager info's isReacting[type] and d <= maxReactionDistance;
/// only then the type's priority (reaction, other) and effects::reactions::Score. The miracle types score with their
/// own priority functions (VillagerFire / VillagerTeleport / VillagerShield / VillagerMourning)
[[nodiscard]] uint32_t ScoreOf(entt::entity villager, uint32_t reaction, openblack::Reaction type, float distance,
                               uint32_t other);

/// The reaction rows' exit function: the circle hug reset, and StopReacting unless `next` is a reactive state. Always 1
/// (it may leave)
uint32_t ExitReaction(components::LivingAction& action, VillagerStates next);

// ---- the miracles it runs from or watches -----------------------------------------------------------------------

/// Running from a miracle: the table's priority for it, plus up to 100 more the nearer the miracle is
/// (effects::flee::FleeFromSpellPriority on the fast distance from the villager to it)
[[nodiscard]] uint8_t FleeFromSpellPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// Watching a nice or impressive miracle: 0 for the villager that made it, else the table's priority of a nice
/// miracle (for the impressive ones too)
[[nodiscard]] uint8_t LookAtNiceSpellPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// 6 FLEEING_FROM_OBJECT_REACTION. The object or the reaction gone, or beyond the reaction's
/// maxDistanceToRunAwayFromObject: it stops reacting and goes back to what it was doing. Beyond
/// minDistanceToRunAwayFromObject and the object not coming at it: 30, facing it. Else it runs ten metres (two game
/// draws for a moving miracle or a thing that flies) and watches from there (30). Always 1
uint32_t Fleeing(components::LivingAction& action);
/// 7 LOOKING_AT_OBJECT_REACTION and 30 FLEEING_AND_LOOKING_AT_OBJECT_REACTION. The object or the reaction gone, or
/// beyond maxDistanceToRunAwayFromObject: it stops reacting and goes back to what it was doing; else one turning step
/// towards it. Always 1
uint32_t Watching(components::LivingAction& action);
/// Beyond the reaction's minDistanceToRunAwayFromObject from the object and the object not coming at it: `lookState`,
/// facing it. Else it runs ten metres from it (two game draws for a thing that moves) and goes on in `fleeState` from
/// there; a point off the map: it stays
void FleeFromObjectIfComingTowardsMe(entt::entity villager, entt::entity object, openblack::Reaction type,
                                     VillagerStates lookState, VillagerStates fleeState);

// ---- a predator about -------------------------------------------------------------------------------------------

/// Running from a predator (28): effects::flee::VillagerFleeFromPredatorPriority on the villager and the predator
[[nodiscard]] uint8_t FleeFromPredatorPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// How long it reacts to a predator: forever while it hides or looks whether it is safe, else the Living's turns
/// (effects::flee::VillagerTurnsToReactToPredator)
[[nodiscard]] uint32_t TurnsToReactToPredator(entt::entity villager, entt::entity object, uint8_t type, float distance);
/// How long before it reacts to a predator again (effects::flee::VillagerTurnsBeforeReactingAgainToPredator); the
/// distance measured from the villager to the predator, not the spread's
[[nodiscard]] uint32_t TurnsBeforeReactingAgainToPredator(entt::entity villager, entt::entity object, uint8_t type,
                                                          float distance);
/// 231 FLEEING_FROM_PREDATOR_REACTION. The predator or the reaction gone, or beyond maxDistanceToRunAwayFromObject:
/// it stops reacting. Something else than an animal: nothing (0). Back from hiding with its building still there, it
/// goes back in; chased (the predator fast and coming at it), or else one draw in six, it hides in a building of its
/// town nearby; failing that it runs from the predator if it is coming at it, else watches it (both 7)
uint32_t FleeingFromPredator(components::LivingAction& action);

// ---- a thing thrown from the hand -------------------------------------------------------------------------------

/// Noticing a thrown thing: 0 for a villager amazed by a magic shield when the thing is not strictly inside the
/// shield's radius, and 0 while the villager flies or lands itself; else the table's priority. (pending) a dancing
/// villager notices it only when it comes at it; no villager dances in openblack
[[nodiscard]] uint8_t ReactToFlyingObjectPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// REACT_TO_MAGIC_SHIELD_DESTROYED: always its row's priority
[[nodiscard]] uint8_t ReactToMagicShieldDestroyedPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// 162 POINT_AT_FLYING_OBJECT_REACTION. The thing out of the physics: nothing. Nearer to the reaction's thing than it
/// flies in two seconds: 6, it runs. Else one turning step towards it, and once it is no longer really in the air the
/// villager stops reacting and goes back to what it was doing. Always 1
uint32_t PointAtFlyingObject(components::LivingAction& action);

// ---- only impressed --------------------------------------------------------------------------------------------

/// Whether a villager available for a reaction is only impressed by it, taking nothing up: the type is one that only
/// impresses (effects::reactions::BeliefButNotReaction), or the villager cannot be reached (villager::IsReachable:
/// not available, at home, held back from a change of state, or hiding in a building)
[[nodiscard]] bool IsAvailableForBeliefButNotReaction(bool beliefButNotReaction, bool reachable);
/// How long before a villager reacts to a burning thing in the hand again: the standard turns when the thing is at
/// least the reaction's minDistanceToRunAwayFromObject away and goes no faster than 3 m a second, else 0
[[nodiscard]] uint32_t BurningObjectTurnsBeforeReactingAgain(float distance, float minDistanceToRunAway, float speed,
                                                             uint32_t standard);
/// A walker's speed (a villager's, an animal's, the creature's) in metres a second, from its speed a turn in metres: ten
/// turns a second, as every walker of the original converts it
[[nodiscard]] float WalkerSpeedInMetresPerSecond(float metresPerTurn);
} // namespace openblack::ecs::villager_reactions
