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

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

/// The facing of a villager or an animal set by a script. The yaw itself is villager::SetYAngle
/// (ECS/Villager/VillagerCore.h) and animal_ai::SetYAngle (ECS/AnimalAI.h).
namespace openblack::ecs
{
struct PhysicsEntry;
}

namespace openblack::ecs::living
{

/// The yaw of a villager or an animal; nothing for another thing
void SetYAngle(entt::entity entity, float radians);
/// The script's focus on a villager or an animal: SetYAngle(the angle from its position to the target). An instant
/// snap: nothing keeps the focus, the next step or LookAtPos overwrites it. False when the entity is not a villager or
/// an animal
bool SetFocus(entt::entity entity, const glm::vec3& target);

/// OVERRIDE_STATE_ANIMATION 068: the clip count of its check. (inferred) the pack's count is the same 441
inline constexpr int32_t k_ForcedClipCount = 441;
/// clip <= 0 or >= 441 is "Invalid animation forced" (logged; it goes on)
[[nodiscard]] constexpr bool IsInvalidForcedClip(int32_t clip)
{
	return clip <= 0 || clip >= k_ForcedClipCount;
}
/// The clip's pack entry, entry 0 out of [0, count)
[[nodiscard]] constexpr int32_t ForcedClipIndex(int32_t clip)
{
	return clip >= 0 && clip < k_ForcedClipCount ? clip : 0;
}

/// GET_PROPERTY Speed:
/// - any object in the physics (not only a villager or animal) with its PhysicsEntry: the length of its velocity
///   ((z z + y y) + x x, square root, all 24 bits);
/// - a villager, animal or creature in the physics without one: its speed in metres, with no dead test;
/// - else a dead one 0, otherwise its speed in metres.
/// A creature's speed in metres is the one its kind is made with (CreatureMadeSpeedInMetres); openblack keeps no dead
/// creature. nullopt when no speed is known: anything else out of the physics or without a PhysicsEntry ((pending) the
/// other classes' speeds, the weather's)
[[nodiscard]] std::optional<float> SpeedProperty(entt::entity entity);
/// SpeedProperty with the physics state given: `inPhysics` whether it is in the physics, `po` its PhysicsEntry (null:
/// none)
[[nodiscard]] std::optional<float> SpeedProperty(entt::entity entity, bool inPhysics, const PhysicsEntry* po);
/// A creature's GET_PROPERTY Speed out of the physics: the default speed of its kind's info, in whole map units, kept
/// to 0..65535 as it is stored when the creature is made, in metres. Nothing changes it afterwards: the creature's pace
/// is its body's own, and SET_PROPERTY Speed asks for a fraction of its top speed instead
[[nodiscard]] float CreatureMadeSpeedInMetres(int32_t wholeSpeed);
/// GET_PROPERTY Age: a villager's or animal's age as a float; nullopt for another thing (the caller says "Not used on
/// non living objects")
[[nodiscard]] std::optional<float> AgeProperty(entt::entity entity);

} // namespace openblack::ecs::living
