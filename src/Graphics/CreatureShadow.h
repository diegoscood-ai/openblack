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

#include <glm/vec3.hpp>

#include "Graphics/ShadowMath.h"

namespace openblack::graphics
{

/// A creature's shadow (wiki: docs/bw1-notes/rendering.md, "Projected shadows"): an entry of graphics::shadow_list
/// made like the hand's, its silhouette rasterized on the CPU into a 32 x 32 texture and drawn over the land blocks and
/// the objects it touches. The light is the model light, kept at least 45 degrees up and no nearer across the ground
/// than three of the creature's radii. The shadow fades out between 50 and 80 of those radii from the camera, and only
/// the odd subrows are written, as for the hand.
struct CreatureShadow
{
	/// Camera distance from the ground beneath the creature, in its radii, where the shadow starts fading and where it
	/// has gone
	static constexpr float k_FadeStart = shadow_math::k_FadeFull;
	static constexpr float k_FadeEnd = shadow_math::k_FadeGone;
	/// The light is pulled in to no nearer than this many of the creature's radii across the ground
	static constexpr float k_LightDistance = shadow_math::k_CreatureRadii;
	/// A light nearer than this across the ground is moved aside first, so that it has a side to be on (compared as a
	/// double)
	static constexpr double k_LightNudge = shadow_math::k_CreatureNear;
	/// The texture's side, in texels
	static constexpr int k_Texels = shadow_math::k_Texels;
	/// Only the odd subrows of the silhouette are written, which makes it lighter, as for the hand
	static constexpr bool k_HalfRows = true;

	/// The shadow's opacity, 0 to 255, at a camera distance from the ground beneath a creature of a radius (the mesh's
	/// half diagonal times the creature's scale), once the land blocks round it have passed shadow_math::Fade's test
	[[nodiscard]] static uint8_t Alpha(float cameraDistance, float radius);
	/// Where the shadow is cast from: the light, brought round to at least 45 degrees above the creature and, when it is
	/// nearer than k_LightDistance radii across the ground, out to that distance
	[[nodiscard]] static glm::vec3 LightPoint(const glm::vec3& light, const glm::vec3& centre, float radius);
};

} // namespace openblack::graphics
