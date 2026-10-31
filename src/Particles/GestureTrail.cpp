/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GestureTrail.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

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

float gesture_trail::ShapeScale(float shapeLength)
{
	return shapeLength * 0.01f;
}

float gesture_trail::IdealShare(float age, float start, float timeToIdeal)
{
	return std::min(age - start, timeToIdeal) / timeToIdeal;
}

float gesture_trail::EasedIdealShare(float share, float interpGain)
{
	return share + ((3.0f - 2.0f * share) * share * share - share) * interpGain;
}

uint8_t gesture_trail::RevealAlpha(float along, float share, float peak)
{
	float alpha = 0.0f;
	if (along < share)
	{
		alpha += peak - peak * (along / share);
	}
	if (1.0f - along < share)
	{
		alpha += peak - peak * ((1.0f - along) / share);
	}
	return static_cast<uint8_t>(static_cast<int>(std::clamp(alpha, 0.0f, 255.0f)));
}

float gesture_trail::WiggleStrength(float age, float phaseSpeed, float dispersalTime, float shrinkTime)
{
	float wiggle = (std::cos(age * phaseSpeed) + 1.0f) * 0.5f;
	if (age - dispersalTime >= 0.0f)
	{
		wiggle *= std::clamp(1.0f - (age - dispersalTime) / shrinkTime, 0.0f, 1.0f);
	}
	return wiggle;
}

std::optional<float> gesture_trail::CollectionPulseAlpha(float age, int pulse)
{
	if (const float h = age - k_PulseStart; h > 0.0f)
	{
		const float f = std::clamp(1.0f - h / k_PulseLength, 0.0f, 1.0f);
		return static_cast<float>(static_cast<uint8_t>(static_cast<int>(static_cast<float>(pulse) * f)));
	}
	return std::nullopt;
}

float gesture_chain::LinkStep(float emitted, const glm::vec3& lastEmitted, const glm::vec3& head, float minEmitDist,
                              size_t count, float dieAge, float dt, bool emitting)
{
	const float maxStep = static_cast<float>(count) / dieAge * dt;
	float step = emitted == 0.0f ? 1.0f : glm::distance(lastEmitted, head) / minEmitDist;
	if (!(maxStep > step))
	{
		step = maxStep;
	}
	if (!emitting)
	{
		step = 0.0f;
	}
	return step;
}

bool gesture_chain::HeadGone(bool emitting, float age, float stopTime)
{
	return !emitting && age - stopTime > k_HeadLinger;
}
