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

#include <entt/entity/entity.hpp>

namespace openblack::ecs
{
class Registry;
}

/// The arm of a creature that holds a thing, posed by the hold animation's last frame over the body.
/// docs/wiki/creature/pose-and-animation.md#which-pose-each-reader-takes
namespace openblack::ecs::creature_hold_pose
{

/// The hand that holds a thing and shows the hold pose: its bone, and whether it is the left hand
struct HoldingHand
{
	uint32_t bone {0};
	bool leftHand {false};
};

/// The hand of a creature that shows the hold pose: the hand that holds a thing, unless the body plays an action whose
/// animation moves the held thing itself (creature_object_actions::ShowsHoldPose), or reaches for a thing it catches
[[nodiscard]] std::optional<HoldingHand> HoldingHandOf(const Registry& registry, entt::entity creature);

} // namespace openblack::ecs::creature_hold_pose
