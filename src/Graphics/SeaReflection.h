/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>

#include <glm/mat4x4.hpp>

namespace openblack::graphics::sea_reflection
{

/// Where a villager is, for the sea's reflection of it
struct VillagerPlace
{
	/// Flying or tumbling in the physics
	bool inPhysics {false};
	/// Held in the hand
	bool inHand {false};
	/// The hand is drawn in the reflection this frame
	bool handReflected {false};
};

/// Whether the sea reflects a villager. Those on the land never are: only one moving in the physics, or one held in the
/// hand while the hand itself is reflected, which carries it along.
[[nodiscard]] constexpr bool ReflectsVillager(const VillagerPlace& place) noexcept
{
	return place.inPhysics || (place.inHand && place.handReflected);
}

/// The bones an object is mirrored with: the pose it is drawn with this frame, when it has one bone for every bone of
/// its mesh, as an animated object's reflection takes its animation's frame; else its mesh's own bones
[[nodiscard]] constexpr std::span<const glm::mat4> ReflectedBones(std::span<const glm::mat4> meshBones,
                                                                  std::span<const glm::mat4> drawnPose) noexcept
{
	return !drawnPose.empty() && drawnPose.size() == meshBones.size() ? drawnPose : meshBones;
}

} // namespace openblack::graphics::sea_reflection
