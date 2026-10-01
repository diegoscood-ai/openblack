/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Graphics/GraphicsHandle.h"

namespace openblack::ecs::components
{
/// Drawn as LH3DSprite::Draw 0x840530 mode A with angle 0 and no origin (Renderer.cpp drawSprite): the entity's
/// Transform gives the position and the half width / half height (scale x / y); Transform::rotation is ignored (mode A
/// only has the +0x14 roll)
struct Sprite
{
	graphics::TextureHandle texture;
	glm::vec2 uvMin;
	glm::vec2 uvExtent;
	glm::vec4 tint;
	/// Additive (glows, the default) or normal alpha blending with a premultiplied tint (e.g. dust).
	bool additive {true};
};

} // namespace openblack::ecs::components
