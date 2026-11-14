/*******************************************************************************
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

#include <entt/fwd.hpp>

#include "ECS/Components/Sprite.h"

namespace openblack::ecs::archetypes
{

class CameraBookmarkArchetype
{
public:
	static std::array<entt::entity, 8> CreateAll();
	/// A bookmark's marker (index 0 to 7): its own cell (24 + index) of misc0.raw, with its alpha misc0a.raw, blended
	/// (the misc material's mode 6) and tinted yellow
	[[nodiscard]] static components::Sprite MarkerSprite(uint8_t index);
};

} // namespace openblack::ecs::archetypes
