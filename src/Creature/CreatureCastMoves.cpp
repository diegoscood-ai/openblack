/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCastMoves.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x3.hpp>

#include "3D/ObjectMatrix.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureMorph.h"

using namespace openblack;
using namespace openblack::creature_cast_moves;

namespace
{
/// Walking up to something much lower than this share of its own height, the creature keeps clear of it by most of its
/// own radius less; up to that height less and less, and past it by this much
constexpr float k_LowThingShare = 0.8f;
constexpr float k_LowThingKeep = 0.7f;
constexpr float k_TallerThingKeep = 0.3f;
constexpr float k_TallThingKeep = 0.4f;
/// A tree is walked up to within this share of its radius, at most this close
constexpr float k_TreeShare = 0.1f;
constexpr float k_TreeMost = 0.25f;
/// A thing longer than this many times its width keeps a row of circles clear
constexpr float k_LongThing = 1.4f;

// The game's 3 by 3 products, for row vectors: m[r][c] is row r's cell c (affine's convention). Each cell is a sum of
// three float products, added in one of two orders
float FirstOrder(const glm::mat3& a, const glm::mat3& b, int r, int c)
{
	return ((a[r][2] * b[2][c]) + (a[r][1] * b[1][c])) + (a[r][0] * b[0][c]);
}
float SecondOrder(const glm::mat3& a, const glm::mat3& b, int r, int c)
{
	return ((a[r][0] * b[0][c]) + (a[r][2] * b[2][c])) + (a[r][1] * b[1][c]);
}

/// a times b as the game multiplies its matrices: the first column in the first order, the other two in the second
glm::mat3 Product(const glm::mat3& a, const glm::mat3& b)
{
	glm::mat3 m;
	for (int r = 0; r < 3; ++r)
	{
		m[r][0] = FirstOrder(a, b, r, 0);
		m[r][1] = SecondOrder(a, b, r, 1);
		m[r][2] = SecondOrder(a, b, r, 2);
	}
	return m;
}

/// A bone's rest rotation times its keyframe's: every cell in the first order
glm::mat3 RestTimesTurn(const glm::mat3& a, const glm::mat3& b)
{
	glm::mat3 m;
	for (int r = 0; r < 3; ++r)
	{
		for (int c = 0; c < 3; ++c)
		{
			m[r][c] = FirstOrder(a, b, r, c);
		}
	}
	return m;
}

/// That times the parent's inverse rest rotation: the first two rows as Product, the third in the first order
glm::mat3 TimesParentInverse(const glm::mat3& a, const glm::mat3& b)
{
	glm::mat3 m = Product(a, b);
	for (int c = 0; c < 3; ++c)
	{
		m[2][c] = FirstOrder(a, b, 2, c);
	}
	return m;
}

/// A point t moved as a bone under a parent turned by b and placed at p: x in the first order, y and z in the second,
/// then the parent's place added
glm::vec3 Moved(const glm::vec3& t, const glm::mat3& b, const glm::vec3& p)
{
	return {((t[2] * b[2][0]) + (t[1] * b[1][0])) + (t[0] * b[0][0]) + p.x,
	        ((t[0] * b[0][1]) + (t[2] * b[2][1])) + (t[1] * b[1][1]) + p.y,
	        ((t[0] * b[0][2]) + (t[2] * b[2][2])) + (t[1] * b[1][2]) + p.z};
}

/// The inverse of a rotation, as the game inverts its matrices
glm::mat3 InverseRows(const glm::mat3& m)
{
	return glm::mat3(affine::Inverse(glm::mat4x3(m)));
}

/// Where each bone is in a list of the bones an animation turns or moves, or nothing for those it doesn't
std::vector<std::optional<size_t>> Slots(std::span<const uint32_t> joints, size_t count)
{
	std::vector<std::optional<size_t>> slots(count);
	for (size_t slot = 0; slot < joints.size(); ++slot)
	{
		if (joints[slot] < count)
		{
			slots[joints[slot]] = slot;
		}
	}
	return slots;
}
} // namespace

float creature_cast_moves::BoneReach(const skeletal_animation::Animation& stand, std::span<const uint32_t> parents,
                                     std::span<const glm::mat4> restLocals)
{
	const auto count = std::min(parents.size(), restLocals.size());
	if (count == 0 || stand.frames.empty())
	{
		return 0.0f;
	}
	const auto parentOf = [&parents](size_t bone) -> std::optional<size_t> {
		const auto parent = parents[bone];
		return parent != skeletal_animation::k_NoParent && parent < bone ? std::optional<size_t>(parent) : std::nullopt;
	};
	const glm::mat3 identity(1.0f);

	// The rest bones under their parents, back to relative to their parents, then under their parents again
	std::vector<glm::mat3> composed(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto parent = parentOf(i);
		composed[i] = Product(glm::mat3(restLocals[i]), parent ? composed[*parent] : identity);
	}
	std::vector<glm::mat3> relative(count);
	for (size_t i = count; i-- > 0;)
	{
		const auto parent = parentOf(i);
		relative[i] = Product(composed[i], InverseRows(parent ? composed[*parent] : identity));
	}
	std::vector<glm::mat3> rest(count);
	std::vector<glm::mat3> inverseRest(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto parent = parentOf(i);
		rest[i] = Product(relative[i], parent ? rest[*parent] : identity);
		inverseRest[i] = InverseRows(rest[i]);
	}

	// The first keyframe, each bone under its parent, the root at the origin
	const auto rotated = Slots(stand.rotatedJoints, count);
	const auto translated = Slots(stand.translatedJoints, count);
	const auto& frame = stand.frames.front();
	std::vector<glm::mat3> rotations(count);
	std::vector<glm::vec3> positions(count);
	for (size_t i = 0; i < count; ++i)
	{
		// a bone the stand does not turn keeps no turn: not the game's default frame (BoneReach)
		glm::mat3 turn = identity;
		if (const auto slot = rotated[i]; slot && *slot < frame.eulerAngles.size())
		{
			const auto& angles = frame.eulerAngles[*slot];
			float y = 0.0f;
			float x = 0.0f;
			float z = 0.0f;
			affine::DecomposeYXZ(affine::RotationYXZ(angles.y, angles.x, angles.z), y, x, z);
			turn = affine::RotationYXZ(y, x, z);
			affine::NormaliseRows(turn);
		}
		glm::mat3 local = RestTimesTurn(rest[i], turn);
		const auto parent = parentOf(i);
		if (i != 0 && parent)
		{
			local = TimesParentInverse(local, inverseRest[*parent]);
		}
		glm::vec3 translation(0.0f);
		if (const auto slot = translated[i]; slot && *slot < frame.translations.size())
		{
			translation = frame.translations[*slot];
		}
		rotations[i] = Product(local, parent ? rotations[*parent] : identity);
		positions[i] =
		    Moved(translation, parent ? rotations[*parent] : identity, parent ? positions[*parent] : glm::vec3(0.0f));
	}
	return creature_morph::Reach(positions);
}

float creature_cast_moves::RoutePlanRadius(float radius, float height, bool tree, float creatureHeight, float creatureRadius)
{
	if (tree)
	{
		return std::min(k_TreeShare * radius, k_TreeMost);
	}
	const float low = creatureHeight * k_LowThingShare;
	const float keep = height <= low ? k_LowThingKeep - k_TallerThingKeep * height / low : k_TallThingKeep;
	return radius - keep * creatureRadius;
}

bool creature_cast_moves::Arrived(float distance, float creatureRadius, float routeRadius, float keep)
{
	return distance < k_ArrivalMargin * (creatureRadius + routeRadius + keep);
}

float creature_cast_moves::GetAwayDistance(float keep, std::optional<float> thingRadius)
{
	return keep + (thingRadius.has_value() ? *thingRadius * k_GetAwayRadii : 0.0f);
}

glm::vec3 creature_cast_moves::GetAwayPoint(const glm::vec3& creature, const glm::vec3& thing, float distance)
{
	const auto away = creature - thing;
	const float flat = glm::length(glm::vec2(away.x, away.z));
	glm::vec3 direction {0.0f};
	if (!(flat > 0.0001f))
	{
		direction = {1.0f, 0.0f, 0.0f};
	}
	else if (const float length = glm::length(away); length > 0.0f)
	{
		direction = away / length;
	}
	return creature + direction * distance;
}

std::optional<glm::vec2> creature_cast_moves::FindClearArea(glm::vec2 point, float width, const ClearCell& clear)
{
	const auto centreX = static_cast<int32_t>(std::floor(point.x / k_CellSize));
	const auto centreZ = static_cast<int32_t>(std::floor(point.y / k_CellSize));
	const int32_t originX = centreX - k_ClearAreaCells / 2;
	const int32_t originZ = centreZ - k_ClearAreaCells / 2;
	std::vector<bool> open(static_cast<size_t>(k_ClearAreaCells * k_ClearAreaCells));
	for (int32_t z = 0; z < k_ClearAreaCells; ++z)
	{
		for (int32_t x = 0; x < k_ClearAreaCells; ++x)
		{
			open.at(static_cast<size_t>(x + z * k_ClearAreaCells)) = clear(originX + x, originZ + z);
		}
	}
	// A square of cells as wide as the area, rounded up to whole cells
	const auto whole = static_cast<int32_t>(width);
	const int32_t cells = static_cast<int32_t>(width / k_CellSize) + (whole % static_cast<int32_t>(k_CellSize) != 0 ? 1 : 0);
	const float half = width * 0.5f;
	const auto inDisc = [&](int32_t i, int32_t j) {
		const float dx = half - (static_cast<float>(i) * k_CellSize + k_CellSize * 0.5f);
		const float dz = half - (static_cast<float>(j) * k_CellSize + k_CellSize * 0.5f);
		return dx * dx + dz * dz <= 0.25f * width * width;
	};
	std::optional<glm::vec2> best;
	float nearest = std::numeric_limits<float>::max();
	for (int32_t a = 0; a < k_ClearAreaCells - cells; ++a)
	{
		for (int32_t b = 0; b < k_ClearAreaCells - cells; ++b)
		{
			bool fits = true;
			for (int32_t i = 0; i < cells && fits; ++i)
			{
				for (int32_t j = 0; j < cells && fits; ++j)
				{
					fits = !inDisc(i, j) || open.at(static_cast<size_t>(a + i + (b + j) * k_ClearAreaCells));
				}
			}
			if (!fits)
			{
				continue;
			}
			const glm::vec2 candidate {static_cast<float>(originX + a) * k_CellSize + half,
			                           static_cast<float>(originZ + b) * k_CellSize + half};
			const float distance = glm::distance(candidate, point);
			if (distance < nearest)
			{
				nearest = distance;
				best = candidate;
			}
		}
	}
	return best;
}

bool creature_cast_moves::Facing(glm::vec2 creature, float heading, glm::vec2 point)
{
	const auto offset = point - creature;
	if (glm::length(offset) < k_OnTopDistance)
	{
		return true;
	}
	const float off = creature_locomotion::WrapAngle(creature_locomotion::HeadingOf(offset) - heading);
	return !(std::abs(off) > k_FacingRadians);
}

std::vector<CollideCircle> creature_cast_moves::CollideCircles(glm::vec2 halfSize, glm::vec2 centre, const glm::mat2& turn)
{
	const auto half = glm::max(halfSize, glm::vec2(1.0f));
	const float longer = std::max(half.x, half.y);
	const float shorter = std::min(half.x, half.y);
	if (!(longer / shorter > k_LongThing))
	{
		return {{.centre = centre, .radius = longer}};
	}
	// A row of circles as wide as its shorter side, spread evenly along its longer
	const auto count = static_cast<int32_t>(longer / shorter) + 1;
	const float spacing = 2.0f * longer / static_cast<float>(count);
	std::vector<CollideCircle> circles;
	circles.reserve(static_cast<size_t>(count));
	for (int32_t i = 0; i < count; ++i)
	{
		const float along = (static_cast<float>(i) + 0.5f) * spacing - longer;
		const glm::vec2 local = half.x > half.y ? glm::vec2(along, 0.0f) : glm::vec2(0.0f, along);
		circles.push_back({.centre = centre + turn * local, .radius = shorter});
	}
	return circles;
}

bool creature_cast_moves::BlocksCell(const CollideCircle& circle, int32_t x, int32_t z)
{
	const glm::vec2 middle {static_cast<float>(x) * k_CellSize + k_CellSize * 0.5f,
	                        static_cast<float>(z) * k_CellSize + k_CellSize * 0.5f};
	return glm::distance(middle, circle.centre) - circle.radius < k_ClearOfThings;
}
