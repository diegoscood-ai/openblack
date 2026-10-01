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

/// Living +0xD0 (Living::SetSpecularColor 0x417480 / GetSpecularColor 0x417490; Object's are no-ops): a colour added
/// to the land light's specular when the villager or animal is drawn (fn_0051B3D0 / Animal::Draw 0x51C4D6 ->
/// fn_0080BEC0 -> fn_0080BF10, per channel with saturation, before the haze). 0 = none: the component goes. The heal
/// chakra (UR_HealSpellChakra fn_006A0E30) makes its targets glow with it.
struct SpecularColour
{
	glm::u8vec3 colour {0, 0, 0}; ///< r, g, b
};

} // namespace openblack::ecs::components
