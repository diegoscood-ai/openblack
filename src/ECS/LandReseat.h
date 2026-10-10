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

#include <bit>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/Events/LandEvents.h"

namespace openblack
{
class EventManager;
}

namespace openblack::ecs
{
class Registry;
}

/// What happens to the things standing on the land when its altitudes change (the vortex's and the temple's
/// flattening). The game keeps no re-seat step: the villagers, the animals and the wood and food piles are drawn on
/// the ground under them every frame, so they follow the new land at once, and everything else (buildings, trees,
/// features, fields, rocks, pots at rest) stays where it was. openblack stores the height in the Transform, so the
/// things that follow are moved here, keeping their height above the ground (docs/bw1-notes/vortex.md, "After a land
/// change")
namespace openblack::ecs::land_reseat
{
/// The new height of a thing at y over a ground that went from groundBefore to groundAfter: the same height above the
/// ground, groundAfter + (y - groundBefore). Nothing when the ground did not move (the two are bitwise equal), so a
/// thing on land that did not change keeps its exact y
[[nodiscard]] constexpr std::optional<float> ReseatedHeight(float y, float groundBefore, float groundAfter)
{
	if (std::bit_cast<uint32_t>(groundBefore) == std::bit_cast<uint32_t>(groundAfter))
	{
		return std::nullopt;
	}
	const float aboveGround = y - groundBefore;
	return groundAfter + aboveGround;
}

/// Whether the ground under a point (its cell's triangles, read from the cell's four corners) reads a corner of the
/// box, both ends included
[[nodiscard]] bool GroundReadsCorners(glm::vec2 xz, glm::ivec2 minCorner, glm::ivec2 maxCorner);

/// The things that follow the land: villagers, animals and the wood and food piles (pots with a sink offset)
[[nodiscard]] bool FollowsTheLand(const Registry& registry, entt::entity thing);

/// The ground (groundAt) under every thing that follows the land, is on it (not unavailable, not carried by a
/// particle system, not off the ground by `offTheGround`) and whose ground reads a corner of the box. Things on the
/// move are taken too: the original draws every one on the ground under it, and their next step sets their height
/// from the ground anyway.
/// In the order of the villagers, then the animals, then the piles, each in its registry order
[[nodiscard]] std::vector<events::GroundBefore> RecordGrounds(Registry& registry, glm::ivec2 minCorner, glm::ivec2 maxCorner,
                                                              const std::function<float(glm::vec2)>& groundAt,
                                                              const std::function<bool(entt::entity)>& offTheGround);

/// Moves each recorded thing to the same height above the new ground (groundAt): a villager's or an animal's y, a
/// pile's sink base (and its y, base + sink offset). Things that went away, or no longer follow the land, are skipped.
/// Their x and z stay, so they stay in the same map cell
/// The number of things moved
size_t Reseat(Registry& registry, std::span<const events::GroundBefore> grounds,
              const std::function<float(glm::vec2)>& groundAt);

/// RecordGrounds over the game's registry and land (the land's GetHeightAt, the height the villagers, animals and
/// piles take when they are made or step); a thing in the hand or flying in the physics is off the ground. Empty
/// without a registry or a land
[[nodiscard]] std::vector<events::GroundBefore> RecordGroundsUnder(glm::ivec2 minCorner, glm::ivec2 maxCorner);

/// Re-seats what a LandAltitudesChanged recorded, over the game's registry and land
void OnLandAltitudesChanged(const events::LandAltitudesChanged& event);

void AddLandReseatEventHandlers(EventManager& manager);
} // namespace openblack::ecs::land_reseat
