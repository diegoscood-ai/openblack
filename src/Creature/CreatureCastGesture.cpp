/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastGesture.h"

#include "Common/X87Precision.h"

using namespace openblack;

namespace x87 = game_math::x87;

glm::vec3 creature_cast_gesture::PointOf(const glm::vec3& shapePoint, float radius, float heading, const glm::vec3& head)
{
	// The shape's z counts down from the top
	const float up = x87::Store(x87::Sub(1.0, shapePoint.z));
	// Two radii ahead of the head
	const double twoRadii = x87::Add(radius, radius);
	const float aheadX = x87::Store(x87::Mul(x87::Sin(heading), twoRadii));
	const float aheadZ = x87::Store(-x87::Mul(x87::Cos(heading), twoRadii));
	const float originX = x87::Store(x87::Add(aheadX, head.x));
	const float originY = head.y;
	const float originZ = x87::Store(x87::Add(aheadZ, head.z));
	// Across to its left, a quarter turn from the way it faces
	const float size = x87::Store(x87::Mul(radius, k_SizeInRadii));
	const double across = x87::Sub(heading, k_QuarterTurn);
	const float acrossX = x87::Store(x87::Mul(x87::Sin(across), size));
	const double acrossZ = -x87::Mul(x87::Cos(across), size);
	const float height = x87::Store(x87::Mul(size, up));
	const float x = x87::Store(x87::Add(x87::Store(x87::Mul(acrossX, shapePoint.x)), originX));
	const float y = x87::Store(x87::Add(0.0, originY));
	const double z = x87::Add(x87::Mul(acrossZ, shapePoint.x), originZ);
	return {x87::Store(x87::Add(x, 0.0)), x87::Store(x87::Add(y, height)), x87::Store(x87::Add(z, 0.0))};
}

float creature_cast_gesture::ShareOfATurn(float lengthRatio)
{
	return 1.0f / (lengthRatio * k_TurnsForTheCircle);
}

creature_cast_gesture::Outcome creature_cast_gesture::Turn(float& progress, uint32_t& turns, uint32_t gesture, bool pointing,
                                                           float share)
{
	if (progress == 0.0f && (!pointing || turns < k_HoldTurns))
	{
		return Outcome::Waiting;
	}
	if (gesture > k_LastGesture)
	{
		return Outcome::Failed;
	}
	progress = share + progress;
	if (1.0f <= progress)
	{
		progress = 1.0f;
	}
	if (progress < 1.0f)
	{
		turns = 0;
	}
	if (1.0f <= progress && turns > k_HoldTurns)
	{
		return Outcome::Done;
	}
	return Outcome::Drawing;
}
