/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Tints a whole mesh like the world.foliage plants (not in the original; the mod tints the crop fields by their
/// growth): its texels turn grey and take the ground colour (grey 0.5 = the ground itself), mixed back towards their
/// own colour by `own`; dissolve = it fades (components::Alpha) as a screen pattern of growing crosses instead of
/// alpha blending. Travels to vs_object in the w of the third instance column (RenderingSystem), where piles keep
/// their sink offset.
struct MeshTint
{
	glm::vec3 ground {0.5f}; ///< 0..1
	float own {1.0f};        ///< 0: grey x ground, 1: the texture's own colour
	bool dissolve {false};
};

} // namespace openblack::ecs::components
