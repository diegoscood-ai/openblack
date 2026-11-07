/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootballGeometry.h"

#include <cmath>

namespace geometry = openblack::ecs::football_geometry;
namespace affine = openblack::affine;

namespace
{
/// The determinant's floor
constexpr float k_MinDeterminant = 1e-10f;
/// A heading is only taken over this length
constexpr float k_ShortestHeading = 0.0001f;
} // namespace

affine::AffineMatrix geometry::PitchMatrix(const glm::vec3& point, float yAngle, float scale)
{
	affine::AffineMatrix out;
	auto& m = out.m;
	m.fill(0.0f);
	if (yAngle == 0.0f)
	{
		// the scale on the diagonal (the identity at scale 1)
		m[0] = scale;
		m[4] = scale;
		m[8] = scale;
	}
	else
	{
		// the turn's sine and cosine are taken at full precision and rounded once, by the product with the scale; the
		// sums with 0 are the original's
		const double c = std::cos(static_cast<double>(yAngle));
		const double s = std::sin(static_cast<double>(yAngle));
		const float cs = static_cast<float>(c * static_cast<double>(scale));
		const float ss = static_cast<float>(s * static_cast<double>(scale));
		m[4] = scale;
		m[0] = 0.0f + cs;
		m[6] = 0.0f - ss;
		m[2] = 0.0f + ss;
		m[8] = cs - 0.0f;
	}
	m[9] = point.x;
	m[10] = point.y;
	m[11] = point.z;
	return out;
}

affine::AffineMatrix geometry::InverseInPlace(const affine::AffineMatrix& a)
{
	const auto& b = a.m;
	const float t1 = b[1] * b[5] - b[2] * b[4];
	const float t2 = b[2] * b[7] - b[1] * b[8];
	const float t0 = b[8] * b[4] - b[5] * b[7];
	float det = (t1 * b[6] + t2 * b[3]) + t0 * b[0];
	if (std::fabs(det) < k_MinDeterminant)
	{
		det = det < 0.0f ? -k_MinDeterminant : k_MinDeterminant;
	}
	const float inv = 1.0f / det;
	affine::AffineMatrix out;
	auto& r = out.m;
	r[0] = t0 * inv;
	r[3] = (b[6] * b[5] - b[3] * b[8]) * inv;
	r[6] = (b[3] * b[7] - b[6] * b[4]) * inv;
	r[1] = t2 * inv;
	r[4] = (b[8] * b[0] - b[6] * b[2]) * inv;
	r[7] = (b[6] * b[1] - b[7] * b[0]) * inv;
	r[2] = t1 * inv;
	r[5] = (b[2] * b[3] - b[5] * b[0]) * inv;
	r[8] = (b[4] * b[0] - b[1] * b[3]) * inv;
	r[9] = -((r[6] * b[11] + r[3] * b[10]) + r[0] * b[9]);
	r[10] = -((r[7] * b[11] + r[4] * b[10]) + r[1] * b[9]);
	r[11] = -((r[8] * b[11] + r[5] * b[10]) + r[2] * b[9]);
	return out;
}

glm::vec3 geometry::BallInPitchSpace(const affine::AffineMatrix& inverse, const glm::vec3& p)
{
	const auto& m = inverse.m;
	return {((m[6] * p.z + m[3] * p.y) + m[0] * p.x) + m[9], ((m[7] * p.z + m[4] * p.y) + m[1] * p.x) + m[10],
	        ((m[8] * p.z + m[5] * p.y) + m[2] * p.x) + m[11]};
}

glm::vec3 geometry::PointInPitchSpaceForGoal(const affine::AffineMatrix& inverse, const glm::vec3& p)
{
	const auto& m = inverse.m;
	return {((m[3] * p.y + m[6] * p.z) + m[0] * p.x) + m[9], ((m[1] * p.x + m[7] * p.z) + m[4] * p.y) + m[10],
	        ((m[5] * p.y + m[2] * p.x) + m[8] * p.z) + m[11]};
}

bool geometry::AlongThePitch(float pitchSpaceX, float halfLength)
{
	return pitchSpaceX <= halfLength && -halfLength <= pitchSpaceX;
}

std::optional<glm::vec3> geometry::BallHeading(const glm::vec3& from, const glm::vec3& to)
{
	const float dx = to.x - from.x;
	const float dy = to.y - from.y;
	const float dz = to.z - from.z;
	const float length = std::sqrt((dz * dz + dx * dx) + dy * dy);
	if (length < k_ShortestHeading)
	{
		return std::nullopt;
	}
	const float inv = 1.0f / length;
	return glm::vec3(inv * dx, inv * dy, dz * inv);
}

bool geometry::CrossesGoalMouth(const glm::vec3& from, const glm::vec3& to, const GoalBox& box)
{
	// the x87 comparisons: "below" also holds when unordered, as does "on"
	const auto below = [](float a, float b) { return !(a >= b); };
	const auto on = [](float a, float b) { return !(a < b) && !(a > b); };
	const auto above = [](float a, float b) { return a > b; };
	const float line = box.centre.x;
	if (below(from.x, line))
	{
		if (below(to.x, line))
		{
			return false;
		}
	}
	else
	{
		if (!below(to.x, line))
		{
			return false;
		}
		// starting on the plane and going back over it: a goal at once
		if (on(from.x, line))
		{
			return true;
		}
	}
	const float dx = to.x - from.x;
	const float t = (line - from.x) / dx;
	const float y = (to.y - from.y) * t + from.y;
	if (above(y, box.halfSize.y))
	{
		return false;
	}
	const float z = (to.z - from.z) * t + from.z;
	if (above(z, box.halfSize.z))
	{
		return false;
	}
	return !below(z, -box.halfSize.z);
}

glm::vec3 geometry::GoalPointInWorld(const affine::AffineMatrix& placement, const GoalBox& box)
{
	const auto& m = placement.m;
	const auto& c = box.centre;
	return {((m[6] * c.z + m[3] * c.y) + m[0] * c.x) + m[9], ((m[7] * c.z + m[1] * c.x) + m[4] * c.y) + m[10],
	        ((m[8] * c.z + m[2] * c.x) + m[5] * c.y) + m[11]};
}
