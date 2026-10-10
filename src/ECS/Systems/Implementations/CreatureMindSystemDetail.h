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
}
namespace openblack::creature_mind_tables
{
struct Tables;
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
/// The food within reach of a point the creature would most like to eat, and where it is; weigh says how useful each
/// is, 0.1 for food nothing is known about
[[nodiscard]] std::optional<std::pair<entt::entity, glm::vec2>> NearestFood(const ecs::Registry& registry, glm::vec2 from,
                                                                            const std::function<float(entt::entity)>& weigh);
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
} // namespace openblack::ecs::systems::mind_detail
