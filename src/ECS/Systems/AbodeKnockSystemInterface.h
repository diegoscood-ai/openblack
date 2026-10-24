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

#include <chrono>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// One little person of the read-out, as it is drawn this frame
struct KnockReadoutPerson
{
	/// The world point it is drawn at, a sprite in the plane of the screen
	glm::vec3 position;
	/// Its half size
	float size;
	/// 0xAARRGGBB
	uint32_t colour;
};

/// The hand knocking on a town's buildings: everyone at home comes out, a house knocks back with a sound and the
/// hand's tap, and every drawn building of the town shows for a few seconds how many people live in it
class AbodeKnockSystemInterface
{
public:
	virtual ~AbodeKnockSystemInterface() = default;

	/// The hand taps a building at a point, as the tap reaches it with the next turn; `ownHand` when it is this
	/// computer's player's hand, which then plays its tap. Whether the building took the tap: always, as the original.
	virtual bool Tap(entt::entity abode, glm::vec3 handPoint, bool ownHand) = 0;
	/// Each frame the landscape is drawn: the read-out's time runs down by the frame's game time (none while paused),
	/// and the camera's right is kept for the rows
	virtual void Update(std::chrono::milliseconds frame, const glm::vec3& cameraRight) = 0;

	/// The town last knocked on: null for none, or for a building without a town
	[[nodiscard]] virtual entt::entity GetReadoutTown() const = 0;
	/// How big the read-out's markers are now, from nothing to full
	[[nodiscard]] virtual float GetReadoutScale() const = 0;
	/// The little people drawn over a building that is drawn this frame, in the order they are drawn; none when it
	/// shows no row
	[[nodiscard]] virtual std::vector<KnockReadoutPerson> PeopleOver(entt::entity abode) const = 0;
};

} // namespace openblack::ecs::systems
