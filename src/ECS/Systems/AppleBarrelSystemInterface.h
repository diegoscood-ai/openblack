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
#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The apple barrel with its worm that the land draws beside the local player's storage pit nearest their temple, at
/// the end of the land's object draw (Scenery/AppleBarrelRules.h). Draw state only: no entity and nothing the game's
/// turns read
class AppleBarrelSystemInterface
{
public:
	/// What the land's draw draws this frame
	struct Drawn
	{
		/// The storage pit it stands beside
		entt::entity store {entt::null};
		/// Where the barrel stands, on the land, unturned and at its own size
		glm::vec3 barrel {0.0f};
		/// Where the worm sits, unturned and at its own size; it takes the land's light where the barrel stands
		glm::vec3 worm {0.0f};
		/// The worm's place in its clip, in milliseconds
		uint32_t wormPlace {0};
	};

	virtual ~AppleBarrelSystemInterface() = default;

	/// Each frame the land is drawn, after its objects: the barrel goes to the pit that carries it, and while one does
	/// the worm's clip plays on by the frame's game milliseconds
	virtual void Draw(uint32_t frameGameMs) = 0;

	/// The barrel and the worm the last drawn frame drew, none while no pit carries them. There is one worm for the
	/// whole game, so from land to land it carries on in its clip where it was
	[[nodiscard]] virtual const std::optional<Drawn>& GetDrawn() const = 0;
};

} // namespace openblack::ecs::systems
