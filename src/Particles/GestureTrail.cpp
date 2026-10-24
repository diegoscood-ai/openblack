/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTrail.h"

#include "Magic/Gestures/GestureShapes.h"

using namespace openblack::particles;

float gesture_trail::SheetStrength(float age, float lifetime)
{
	double share = static_cast<double>(age) / lifetime;
	share = share <= 0.0 ? 0.0 : (share < 1.0 ? share : 1.0);
	const double centred = share + share - 1.0;
	return static_cast<float>(1.0 - (centred * centred));
}

std::vector<glm::vec3> gesture_trail::SheetPoints(const magic::gestures::Path& ideal)
{
	// The share between two points is kept in single precision, and each point's share is its number times it
	constexpr float k_Share = 1.0f / static_cast<float>(k_SheetPoints - 1);
	std::vector<glm::vec3> points;
	points.reserve(static_cast<size_t>(k_SheetPoints));
	for (int i = 0; i < k_SheetPoints; ++i)
	{
		points.push_back(ideal.At(static_cast<float>(i) * k_Share));
	}
	return points;
}
