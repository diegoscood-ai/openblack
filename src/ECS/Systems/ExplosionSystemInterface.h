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

#include <glm/vec3.hpp>

#include "Blast/DustPuff.h"

namespace openblack::ecs::systems
{

/// What explosions leave after them besides their particles: the heap of rubble on the land with its puff of dust, and the
/// camera shaking as they land near it
class ExplosionSystemInterface
{
public:
	virtual ~ExplosionSystemInterface() = default;

	/// A heap of rubble at a point, turned at random, with its puff of dust
	virtual void AddRubble(const glm::vec3& centre, float yaw) = 0;
	/// A puff of smoke or dust of a size at a point, its kind's colour unless given one (0xRRGGBB): the dust a tree taking
	/// root again throws up, the smoke a dead body leaves as it goes
	virtual void AddSmoke(const glm::vec3& centre, float size, dust_puff::Kind kind,
	                      std::optional<uint32_t> colour = std::nullopt) = 0;
	/// The camera shakes while within a radius of a point, from a strength down to none over the seconds (cut to whole
	/// milliseconds), only up and down when asked: a particle effect's shake
	virtual void AddShake(const glm::vec3& position, float radius, float strength, float seconds, bool verticalOnly) = 0;
	/// Whether any shake is going on, near the camera or not
	[[nodiscard]] virtual bool IsShaking() const = 0;
	/// Once a frame of the game's time: the rubble lies and fades (an uprooted tree's crater too, which is the same mark)
	virtual void Update(float milliseconds) = 0;
	/// A new land: no rubble
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
