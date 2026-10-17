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

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreaturePlanActions.h"
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
[[nodiscard]] std::optional<creature_mind::Wants::WaterSpot> NearestWater(glm::vec2 from);
/// The nearest thing a creature could pick up, and where it is
[[nodiscard]] std::optional<std::pair<entt::entity, glm::vec2>> NearestObject(const ecs::Registry& registry, glm::vec2 from);
[[nodiscard]] std::optional<glm::vec2> NearestHurlTarget(const ecs::Registry& registry, glm::vec2 from);
/// Whether a thing is the kind an action is done to
[[nodiscard]] bool Accepts(const ecs::Registry& registry, creature_plan_actions::Target target, entt::entity entity,
                           entt::entity self);
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
/// How useful the creature has learnt a food is to eat, 0.1 when nothing is known
[[nodiscard]] float FoodUsefulness(const ecs::Registry& registry, const components::CreatureMindState& mind, entt::entity self,
                                   entt::entity food);
/// Tells the creature's hands what to do with a thing, by the agenda's object order this turn
void Order(const ecs::Registry& registry, entt::entity creature, const creature_mind::Commands& commands);
/// The action of the game's table the creature carries out: its plan's, or the one what its idle mind does counts as;
/// none while it does nothing that counts as one
[[nodiscard]] std::optional<uint32_t> CurrentAction(const components::CreatureMindState& mind,
                                                    const creature_mind_tables::Tables* tables);
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
/// Whether the creature may try the miracle an action casts: seen at least half the times it needs to learn it, able
/// to pay for it, and for a power-up grown past its seventh stage
[[nodiscard]] bool MayCast(entt::entity creature, const components::CreatureMindState& mind,
                           const creature_mind_tables::Tables* tables, uint32_t action, bool powerUp);
/// What the creature casts for an action, the gesture it draws first, its height and itself
[[nodiscard]] std::optional<creature_plan_actions::CastInfo> CastInfoFor(entt::entity creature, uint32_t action);
/// The middle of the nearest area about a point, so wide, whose cells the creature may stand in, if any
[[nodiscard]] std::optional<glm::vec2> FindClearArea(const ecs::Registry& registry, entt::entity creature, glm::vec2 point,
                                                     float width);
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
/// Whether a thing standing in a cell of the land keeps it from being clear for a creature to get away to: the trees
/// and fields always by the circles they keep clear, anything else fixed when the creature must go round it (a
/// building, feature or static thing when as tall as a tenth of the creature or burning, a dead tree only burning), and
/// nothing that moves
[[nodiscard]] bool CellBlocked(const ecs::Registry& registry, entt::entity creature, int32_t x, int32_t z);
/// The way a creature's body faces, as a body's yaw (0 along -z, a quarter turn along +x). A heading turns the other way
/// round; before the creature first moves, it is read from the body's rotation as the locomotion reads it then.
[[nodiscard]] float BodyYaw(const ecs::Registry& registry, entt::entity creature, const components::Transform& at);
} // namespace openblack::ecs::systems::mind_detail
