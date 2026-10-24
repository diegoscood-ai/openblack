/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectOnScreen.h"

namespace openblack::ecs::draw_list
{

bool OnScreen(const OnScreenInputs& object, const OnScreenView& view)
{
	if (object.dontDraw)
	{
		return true;
	}

	// X, Y and Z, the depth, in the box test's own summing order
	const glm::vec3 clip = affine::ToClipForBoxTest(view.worldToClipping, object.boxCentreWorld);
	const float radius = object.radius;

	// Written as "not at or beyond", so that a NaN depth is off the screen
	if (!(clip.z + radius >= view.nearClip))
	{
		return false;
	}

	// The eye inside the sphere round the object's origin (not round the box centre): x, y and z summed in that order,
	// and only a strictly larger r^2 counts
	const glm::vec3 offset = object.origin - view.eye;
	const float distanceSquared = (offset.x * offset.x + offset.y * offset.y) + offset.z * offset.z;
	if (radius * radius > distanceSquared)
	{
		return true;
	}

	const float inverseDepth = 1.0f / clip.z;
	const auto width = static_cast<float>(view.screen.x);
	const auto height = static_cast<float>(view.screen.y);
	const float sx = (clip.x * inverseDepth + 1.0f) * (width * 0.5f);
	const float sy = (1.0f - clip.y * inverseDepth) * (height * 0.5f);
	const float discRadius = ((((radius * view.nearClip) * inverseDepth) * width) * 0.5f) / view.nearHalfWidth;

	// The left and top edges are written as "not at or beyond 0", so that a NaN is off the screen; the right and bottom
	// ones as "beyond", so that a NaN stays on it
	if (!(discRadius + sx >= 0.0f) || sx - discRadius > width || !(discRadius + sy >= 0.0f))
	{
		return false;
	}
	return !(sy - discRadius > height);
}

bool ActiveAfterDraw(bool onScreen, bool isHuman, bool isComplex)
{
	return onScreen || isHuman || isComplex;
}

} // namespace openblack::ecs::draw_list
