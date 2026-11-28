/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureKiss.h"

#include <glm/geometric.hpp>

using namespace openblack;

glm::vec3 creature_kiss::PointAt(const KissPoints& points, float share, float size)
{
	// From the low point towards the high point by the share, then at the creature's size
	return (points.low + ((points.high - points.low) * share)) * size;
}

std::optional<float> creature_kiss::KissShare(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize)
{
	const auto otherHigh = PointAt(other, 1.0f, otherSize);
	const auto ownHigh = PointAt(own, 1.0f, ownSize);
	const bool ownIsHigher = ownHigh.y > otherHigh.y;
	// The two high points as high and low, and the lowest point the taller of the two kisses at
	const auto& taller = ownIsHigher ? own : other;
	const float tallerSize = ownIsHigher ? ownSize : otherSize;
	const auto high = ownIsHigher ? ownHigh : otherHigh;
	const auto low = ownIsHigher ? otherHigh : ownHigh;
	const auto bottom = PointAt(taller, 0.0f, tallerSize);
	if (bottom.y > low.y)
	{
		return std::nullopt;
	}
	if (ownIsHigher)
	{
		return (otherHigh.y - bottom.y) / (high.y - bottom.y);
	}
	return 1.0f;
}

float creature_kiss::KissingDistance(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize)
{
	const auto share = KissShare(own, ownSize, other, otherSize);
	if (!share.has_value())
	{
		return 0.0f;
	}
	return -PointAt(own, *share, ownSize).z;
}

float creature_kiss::MeanKissingDistance(const KissPoints& own, float ownSize, const KissPoints& other, float otherSize)
{
	return (KissingDistance(own, ownSize, other, otherSize) + KissingDistance(other, otherSize, own, ownSize)) * 0.5f;
}

std::pair<glm::vec2, glm::vec2> creature_kiss::KissPlaces(glm::vec2 self, glm::vec2 other, float distance)
{
	const auto middle = (self + other) * 0.5f;
	auto offset = middle - self;
	const float length = glm::length(offset);
	offset = length > 0.0f ? offset * (distance / length) : glm::vec2(0.0f);
	return {middle - offset, middle + offset};
}
