/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraBookmarkArchetype.h"

#include <stdexcept>

#include <entt/core/hashed_string.hpp>

#include "3D/FrameAnim.h"
#include "ECS/Components/CameraBookmark.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Registry.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
constexpr int k_FirstCell = 24;
constexpr auto k_Texture = entt::hashed_string("raw/misc0");
constexpr auto k_Alpha = entt::hashed_string("raw/misc0a");
} // namespace

Sprite CameraBookmarkArchetype::MarkerSprite(uint8_t index)
{
	// one cell of the 8 x 8 sheet, its colours with their alpha, blended, in the yellow of the original's markers
	return {
	    .texture = k_Texture.value(),
	    .uvMin = graphics::frame_anim::SpriteCellUv(k_FirstCell + index)[0],
	    .uvExtent = glm::vec2 {1.0f / 8.0f, 1.0f / 8.0f},
	    .tint = glm::vec4 {1.0f, 1.0f, 0.0f, 1.0f},
	    .additive = false,
	    .alpha = k_Alpha.value(),
	};
}

std::array<entt::entity, 8> CameraBookmarkArchetype::CreateAll()
{
	auto& registry = Locator::entitiesRegistry::value();
	// the alpha sheet is required, as it always was; without the colour sheet the markers are made all the same, and the
	// sprite draw leaves out a sprite whose texture the cache does not have
	if (!Locator::resources::value().GetTextures().Contains(k_Alpha.value()))
	{
		throw std::runtime_error("Failed to get Camera Bookmark sprite: misc0a");
	}

	auto result = std::array<entt::entity, 8>();

	registry.Create(result.begin(), result.end());

	// TODO (#749) use std::views::enumerate
	for (uint8_t i = 0; auto entity : result)
	{
		// cells 24..31 of the misc sheet (row 3), one per bookmark, tinted yellow: the original's cells and colour
		registry.Assign<Sprite>(entity, MarkerSprite(i));
		++i;
		registry.Assign<CameraBookmark>(entity, i);
	}

	return result;
}
