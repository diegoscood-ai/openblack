/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFootprints.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/ObjectMatrix.h"
#include "Common/X87Precision.h"

namespace openblack::creature_footprints
{
namespace
{
/// A print's corners about its middle, for a size of 1, before it is turned. The picture points along +x.
constexpr std::array<glm::vec2, 4> k_Corners = {glm::vec2(-k_HalfSide, k_HalfSide), glm::vec2(k_HalfSide, k_HalfSide),
                                                glm::vec2(k_HalfSide, -k_HalfSide), glm::vec2(-k_HalfSide, -k_HalfSide)};
/// Where the corners fall in the print's cell, for the right foot and for the left, flipped top to bottom
constexpr std::array<std::array<glm::vec2, 4>, 2> k_CellUvs = {{
    {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)},
    {glm::vec2(0.0f, 1.0f), glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 0.0f)},
}};
} // namespace

SpeciesPrint PrintOf(CreatureType species, bool aprilFools)
{
	const auto row = std::min(creature::InfoRow(species), k_SpeciesPrints.size() - 1);
	auto print = k_SpeciesPrints.at(row);
	if (aprilFools)
	{
		print.cell = k_SmileyCell;
	}
	return print;
}

Foot FootOf(const glm::mat4& world)
{
	// The game's matrices are the transposes of these. Its yaw, of a turn about y then x then z, comes from what the
	// bone's z axis has become.
	// The game's octant arc tangent of that axis as it is, not made one long, stored as a float
	const auto z = glm::vec3(world[2]);
	return {.position = glm::vec3(world[3]), .yaw = static_cast<float>(affine::ArcTanOctant(z.z, -z.x))};
}

float PrintSize(float creatureSize, const SpeciesPrint& print)
{
	// The game rounds the size times 4 to a float, then multiplies by the species' scale
	const float sized = creatureSize * k_SizePerCreatureSize;
	return sized * print.scale;
}

Footprint MakeFootprint(const glm::vec3& foot, float angle, float size, uint8_t cell, bool left, const AltitudeAt& altitudeAt)
{
	// The turn's cosine and sine as the game's floating point unit gives them, kept as floats
	const float cosine = game_math::x87::Store(game_math::x87::Cos(static_cast<double>(angle)));
	const float sine = game_math::x87::Store(game_math::x87::Sin(static_cast<double>(angle)));
	const float negativeSine = -sine;
	// The game turns each corner about the foot with one matrix: the turn, then a move that keeps the foot in place. The
	// move is the foot less the turned foot. Each step rounds to a float, so a corner far from the map's origin can be
	// some thousandths of a unit away from a turn of its offset alone.
	const float moveX = foot.x + (((cosine * -foot.x) + (negativeSine * -foot.z)) + 0.0f);
	const float moveZ = foot.z + (((sine * -foot.x) + (cosine * -foot.z)) + 0.0f);
	const auto origin = glm::vec2(static_cast<float>(cell) / static_cast<float>(k_CellsPerRow), 0.0f);
	const auto& cellUvs = k_CellUvs.at(left ? 1 : 0);
	Footprint print;
	for (size_t i = 0; i < print.corners.size(); ++i)
	{
		const float cornerX = (size * k_Corners.at(i).x) + foot.x;
		const float cornerZ = (size * k_Corners.at(i).y) + foot.z;
		const float x = (((cosine * cornerX) + (negativeSine * cornerZ)) + 0.0f) + moveX;
		const float z = (((cosine * cornerZ) + (sine * cornerX)) + 0.0f) + moveZ;
		print.corners.at(i) = glm::vec3(x, altitudeAt(x, z) + k_Lift, z);
		print.uvs.at(i) = origin + (cellUvs.at(i) / static_cast<float>(k_CellsPerRow));
	}
	return print;
}

bool Add(Trail& trail, const Footprint& print)
{
	if (trail.prints.size() >= k_Capacity)
	{
		return false;
	}
	trail.prints.push_back(print);
	return true;
}

void Fade(Trail& trail, float elapsedMs)
{
	trail.sinceFadeMs += elapsedMs;
	if (trail.sinceFadeMs < k_FadeIntervalMs)
	{
		return;
	}
	const auto fade = trail.sinceFadeMs * k_FadePerMs;
	trail.sinceFadeMs = 0.0f;
	for (auto& print : trail.prints)
	{
		// Kept in whole steps, the fraction dropped
		print.alpha = static_cast<uint8_t>(std::max(static_cast<float>(print.alpha) - fade, 0.0f));
	}
	std::erase_if(trail.prints, [](const Footprint& print) { return print.alpha == 0; });
}
} // namespace openblack::creature_footprints
