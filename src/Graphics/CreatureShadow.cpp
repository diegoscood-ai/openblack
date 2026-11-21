/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureShadow.h"

namespace openblack::graphics
{

uint8_t CreatureShadow::Alpha(float cameraDistance, float radius)
{
	// The complex update's alpha over the fade's ramp, from a full base alpha
	return static_cast<uint8_t>(shadow_math::AlphaComplex(shadow_math::FadeOfRadii(cameraDistance / radius), 255));
}

glm::vec3 CreatureShadow::LightPoint(const glm::vec3& light, const glm::vec3& centre, float radius)
{
	// The radius already carries the scale: times 1 leaves it as it is
	return shadow_math::LightCreature(centre, light, radius, 1.0f);
}

} // namespace openblack::graphics
