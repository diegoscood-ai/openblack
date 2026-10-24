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

#include <array>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureDrink.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/CreatureSpells.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::components
{
struct CreatureMindState;
struct Transform;
}
namespace openblack::creature_mind_tables
{
struct Tables;
}
namespace openblack::creature_mind_model
{
struct Learnt;
enum class TreeKind : uint8_t;
}
namespace openblack::ecs::systems
{
class MagicSystemInterface;
}

/// What the creature mind system's files share, and its tests read: how a species' desires start, finding food, water,
/// things to pick up and things to hurl at about a creature, and what a creature knows of things. Every scan reads the
/// registry without changing it, passes over things that are going, and on a tie takes the lower entity.
namespace openblack::ecs::systems::mind_detail
{
[[nodiscard]] std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> SetupFor(CreatureType species);
[[nodiscard]] std::optional<float> FoodValueOf(entt::entity entity);
/// Whether it is night, as the creatures see it
[[nodiscard]] bool IsNight();
/// Whether the creature can eat a thing, as the game's eating plans ask it (creature_eating::CanBeEaten): an object it
/// can pick up, a mobile static that is not a toy, never a creature
[[nodiscard]] bool CanBeEaten(const ecs::Registry& registry, entt::entity self, entt::entity thing);
/// The food within reach of a point the creature would most like to eat, and where it is: anything it can eat (CanBeEaten);
/// weigh says how useful each is, 0.1 for food nothing is known about
[[nodiscard]] std::optional<std::pair<entt::entity, glm::vec2>>
NearestFood(const ecs::Registry& registry, entt::entity self, glm::vec2 from, const std::function<float(entt::entity)>& weigh);
/// Where a creature drinks from the sea: by the nearest water point that the land marks, else at the nearest coast of
/// its map of the land. None when it finds no place where it can stand to drink.
[[nodiscard]] std::optional<creature_drink::Place> DrinkPlaceOf(entt::entity creature);
/// Whether a thing is the kind an action is done to
[[nodiscard]] bool Accepts(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity entity,
                           entt::entity self);
/// Whether the creature can steal the thing (see creature_steal::IsStealable)
[[nodiscard]] bool CanSteal(const ecs::Registry& registry, entt::entity creature, entt::entity thing);
/// A thing found about a point
struct Found
{
	entt::entity entity;
	glm::vec2 point;
	float distance;
};
/// The things of a kind about a point, nearest first, then by entity
[[nodiscard]] std::vector<Found> Gather(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity self,
                                        glm::vec2 from);
/// How much an animal catches a creature's eye: every bird, and the vulture, as much as a dove; the rest as an animal
[[nodiscard]] creature_look::Interest LookInterestOf(AnimalInfo type);
/// Everything on the land a creature might look at, and where it looks to see it
[[nodiscard]] std::vector<creature_look::Candidate> GatherCandidates(const ecs::Registry& registry);
/// A belief's OnFire value: 0 when the thing burns, 1 when it does not
[[nodiscard]] constexpr uint32_t OnFireValue(bool onFire)
{
	return onFire ? 0 : 1;
}
/// Whether a town has anything in it for compassion to help: one of its villagers or abodes that is not going, one of its
/// fields, or the totem of its town centre
[[nodiscard]] bool TownHasMembers(const ecs::Registry& registry, entt::entity town);
/// A town's members, by the same rules: its villagers and abodes that are not going, its fields, the totem of its town
/// centre
[[nodiscard]] std::vector<entt::entity> TownMembers(const ecs::Registry& registry, entt::entity town);
/// What a creature knows of a thing, as far as the world here tells it
[[nodiscard]] std::optional<creature_tree::Belief> BeliefOf(const ecs::Registry& registry, entt::entity entity,
                                                            entt::entity self);
/// How useful the creature has learnt a thing is for a desire, by one of the desire's two trees (as what a plan is for,
/// or as what it is done to) and by what it knows of the thing; the planner's usual usefulness when it knows nothing of
/// it
[[nodiscard]] float Usefulness(const creature_mind_model::Learnt& learnt, creature_mind_model::TreeKind kind,
                               creature_desires::Desire desire, const std::optional<creature_tree::Belief>& belief);
/// The desire that a creature most wants to do about another creature, weighed as the planner weighs a thing to act on:
/// each desire that is activated, not held back and looks for a goal, and that can be about a creature, by its strength,
/// what the creature has learnt of the other and how far away the other is. None when no desire scores. The desire to
/// impress is about towns, never a creature; anger only about a creature it may be angry with.
[[nodiscard]] std::optional<creature_desires::Desire> DesireAboutCreature(const ecs::Registry& registry, entt::entity creature,
                                                                          const components::CreatureMindState& mind,
                                                                          entt::entity other,
                                                                          const creature_mind_tables::Tables& tables);
/// How useful the creature has learnt a food is to eat, 0.1 when nothing is known
[[nodiscard]] float FoodUsefulness(const ecs::Registry& registry, const components::CreatureMindState& mind, entt::entity self,
                                   entt::entity food);
/// Tells the creature's hands what to do with a thing, by the agenda's object order this turn
void Order(const ecs::Registry& registry, entt::entity creature, const creature_mind::Commands& commands);
/// The action of the game's table the creature carries out: its plan's, or the one what its idle mind does counts as;
/// none while it does nothing that counts as one
[[nodiscard]] std::optional<uint32_t> CurrentAction(const components::CreatureMindState& mind,
                                                    const creature_mind_tables::Tables* tables);
/// The plan the creature holds, as others see it: the planner's (a forced one and the leash's too); while it does
/// nothing the planner chose, what its idle mind does, as a plan of the row it counts as, for that row's desire, done
/// to the step's thing (itself when the step names none); none once a plan has ended and before the next
[[nodiscard]] std::optional<creature_planner::Plan> HeldPlan(const components::CreatureMindState& mind,
                                                             const creature_mind_tables::Tables* tables);
/// The species' floor, which no desire is set below: the least its desires are lessened to by its actions, or wanted
/// while another is made dominant; 0 for anything but a creature or without the game's constants
[[nodiscard]] float DesireFloorOf(const ecs::Registry& registry, entt::entity creature);
/// Having done an action, the desire it satisfies is less, by the game's action table, and its body pays for it
void Satisfied(entt::entity creature, creature_desires::Desires& desires, std::string_view action);
/// While the land's CreatureCurse script runs, the size, strength and alignment it keeps of the creature it found, read
/// from its globals OriginalSizeOfMyCreature (a height, so the size is it over the height at size one),
/// OriginalStrengthOfMyCreature and OriginalAlignmentOfMyCreature, 0 for any it does not have; none while it does not
/// run. scriptRuns says whether a script of that name runs, global gives a global's value by name.
[[nodiscard]] std::optional<creature_spells::SavedBody>
CurseValuesToSave(const std::function<bool(std::string_view)>& scriptRuns,
                  const std::function<std::optional<float>(std::string_view)>& global);

// Casting miracles (CreatureMindCasting.cpp): the creature mind system's members of the same names call these
/// The miracle an action of the game's table casts, by its magic type from the first to the last a creature casts;
/// none for any other action
[[nodiscard]] std::optional<uint32_t> CastMagicOf(uint32_t action);
/// How often a creature has seen a miracle and how often it needs to have seen it to learn it, none without its
/// learning
[[nodiscard]] std::optional<std::pair<float, float>> MiracleSightings(const components::CreatureMindState& mind,
                                                                      const creature_mind_tables::Tables& tables,
                                                                      CreatureType species, MagicType magic);
/// Whether the body of the creature can pay for a miracle of a type; not without the game's constants
[[nodiscard]] bool CanPayForMiracle(entt::entity creature, MagicType type);
/// Whether the creature may try the miracle an action casts: seen at least half the times it needs to learn it, able
/// to pay for it, and for a power-up grown past its seventh stage. The spell to amuse another creature asks only for a
/// spell on creatures that it knows and can pay for.
[[nodiscard]] bool MayCast(entt::entity creature, const components::CreatureMindState& mind,
                           const creature_mind_tables::Tables* tables, uint32_t action, bool powerUp);
/// What the creature casts for an action, the gesture it draws first, its height and itself. With the mind's tables it
/// also finds, for a fireball, the clear area near the creature that it casts from, and for the spell it casts to amuse
/// another creature, the spells it may pick from
[[nodiscard]] std::optional<creature_plan_actions::CastInfo> CastInfoFor(entt::entity creature, uint32_t action,
                                                                         const creature_mind_tables::Tables* tables = nullptr);
/// The spells on creatures that the creature knows and can pay for, in the order of their types, with their gestures:
/// what it may cast to amuse another creature
[[nodiscard]] std::vector<creature_mind::AmusingSpell>
AmusingSpells(entt::entity creature, const components::CreatureMindState& mind, const creature_mind_tables::Tables& tables);
/// The middle of the nearest area about a point, so wide, whose cells the creature may stand in, if any; with
/// `nothingLiving`, no living thing may stand in it either
[[nodiscard]] std::optional<glm::vec2> FindClearArea(const ecs::Registry& registry, entt::entity creature, glm::vec2 point,
                                                     float width, bool nothingLiving = false);
/// Where a creature casts water on itself: the nearest clear area about where it stands, 1.7 times its height wide
[[nodiscard]] std::optional<glm::vec2> ClearSpotToCastOnItself(const ecs::Registry& registry, entt::entity creature);
/// Going near, getting away from or turning to face an object begins (taking its first turn), and each turn it goes
/// on; animating says whether the body plays an action. Kept in the creature's components::CreatureCasting
void StartSubMove(entt::entity creature, const creature_mind::Movement& movement, float seconds);
void StepSubMove(entt::entity creature, bool animating);
/// How the creature's going near, getting away or turning is going: running while it does none
[[nodiscard]] creature_mind::SubMove SubMoveOf(entt::entity creature);
/// The creature tries a miracle at a thing: it lets go of what it held; in a fight it pays stamina first, and can't
/// without enough; short of the sightings it needs it fizzles, which counts as one more and is shown later; without the
/// energy for it, or at something the miracle can't be cast at, it wants food above all. Whether it was cast
bool TryMiracle(entt::entity creature, components::CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                MagicSystemInterface& magic, MagicType type, entt::entity target);
/// The creature tries a miracle at a place, as a try at a thing goes but with no stamina to pay in a fight, and the
/// fizzle first, before it lets go of what it held. A fizzle is shown about the thing of the last try at a thing that
/// fizzled. Without the energy for it, or at a place the miracle can't be cast at, it wants food above all. Whether it
/// was cast
bool TryMiracleAt(entt::entity creature, components::CreatureMindState& mind, const creature_mind_tables::Tables& tables,
                  MagicSystemInterface& magic, MagicType type, glm::vec3 place);
/// Whether a thing standing in a cell of the land keeps it from being clear for a creature: a temple's heart always; a
/// tree, a field or a thing the creature must go round (route_plan_world::CreatureMustAvoid) by the circles it keeps
/// clear; with `livingBlocks`, also any villager, animal or creature there
[[nodiscard]] bool CellBlocked(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z,
                               bool livingBlocks = false);
/// The way a creature's body faces, as a body's yaw (0 along -z, a quarter turn along +x). A heading turns the other way
/// round; before the creature first moves, it is read from the body's rotation as the locomotion reads it then.
[[nodiscard]] float BodyYaw(const ecs::Registry& registry, entt::entity creature, const components::Transform& at);
/// The kind of belief a creature keeps about a thing, by the game's belief types; none for the things it keeps none
/// about (CreatureMindBeliefs.cpp). Only the memory of what it came across uses it: what it thinks of a thing is
/// BeliefOf's
[[nodiscard]] std::optional<uint32_t> BeliefKindOf(const ecs::Registry& registry, entt::entity thing);
} // namespace openblack::ecs::systems::mind_detail
