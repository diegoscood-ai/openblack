/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexCrossingRules.h"

#include <cmath>

#include "Common/X87Precision.h"

namespace openblack::vortex::crossing
{

namespace
{
// Every step below is worked at the game thread's 24-bit precision, which rounds each add, subtract, multiply, divide
// and square root to a float: plain float arithmetic. Only the arctangent, the sine and the cosine keep the FPU's
// 64 bits until their first use (game_math::x87)

/// The share of the way a thing has blended, 0 to 1: nought (and a NaN) gives 0, a share not below one gives 1
float Blend(float age, float time)
{
	const float share = age / time;
	if (!(share > 0.0f))
	{
		return 0.0f;
	}
	return share < 1.0f ? share : 1.0f;
}

/// The swing round the middle is this many times the setting's speed
constexpr float k_SwingScale = 30.0f;
/// Once it has blended this far, a thing that can't be destroyed may go from the middle
constexpr float k_IndestructibleGoesAt = 0.5f;
/// A place across the land as the map takes it from metres: x 65536, then x 0.1, each rounded to a float, truncated
int32_t MapFixed(float metres)
{
	constexpr float k_Fixed = 65536.0f;
	constexpr float k_Tenth = 0.1f;
	return static_cast<int32_t>(metres * k_Fixed * k_Tenth);
}
} // namespace

float PileShare(float random)
{
	constexpr float k_Spread = 0.4f;
	constexpr float k_Least = 0.3f;
	return random * k_Spread + k_Least;
}

AtomStart AtomFromMatrix(const glm::mat3& rows, glm::vec3 position)
{
	// the length of the up row, its squares summed z, y, then x
	const auto& up = rows[1];
	const float scale = std::sqrt(up.z * up.z + up.y * up.y + up.x * up.x);
	const float inverse = 1.0f / scale;
	AtomStart start {.position = position, .scale = scale, .rows = rows};
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t cell = 0; cell < 3; ++cell)
		{
			start.rows[row][cell] = inverse * rows[row][cell];
		}
	}
	return start;
}

std::optional<AttractStep> Attract(const AttractSettings& settings, const AttractAtom& atom, glm::vec3 origin, float dt,
                                   float invDt, const GroundAt& ground)
{
	namespace x87 = game_math::x87;
	const auto& p = atom.position;
	const auto& taken = atom.takenVelocity;
	// its distance and angle from the middle, across the land; the angle as the arctangent leaves it, kept as a float
	const float dx = p.x - origin.x;
	const float dz = p.z - origin.z;
	const float radius = std::sqrt(dz * dz + dx * dx);
	const float angle = x87::Store(x87::Atan2(static_cast<double>(dz), static_cast<double>(dx)));
	const float blend = Blend(atom.age, settings.blendTime);
	const float velocityBlend = Blend(atom.age, settings.blendTimeVelocity);
	// drawn in by the share of its distance, never nearer than the least
	float radius1 = radius - blend * settings.radialSpeed * dt * radius;
	if (!(settings.minRadius < radius1))
	{
		radius1 = settings.minRadius;
	}
	const float swung = blend * settings.thetaSpeed * dt * k_SwingScale / radius1 + angle;
	// At the middle, once blended in, it is gone; a thing that can't be destroyed goes from half way blended
	if (radius1 < settings.deletionRadius && (!(blend < 1.0f) || (atom.indestructible && !(blend < k_IndestructibleGoesAt))))
	{
		return std::nullopt;
	}
	// the point on its circle it would be at this step: the sine kept as a float, the cosine's product taken from its
	// 64 bits
	const auto turn = x87::SinCos(static_cast<double>(swung));
	const float sine = x87::Store(turn.sin);
	const float sineRadius = sine * radius1;
	const float targetX = static_cast<float>(x87::Mul(turn.cos, static_cast<double>(radius1))) + origin.x;
	const float targetY = 0.0f + origin.y;
	const float targetZ = sineRadius + origin.z;
	// the velocity that would put it there this step, less the one it was taken with, then blended in
	const glm::vec3 wanted {
	    (targetX - p.x) * invDt - taken.x,
	    (targetY - p.y) * invDt - taken.y,
	    (targetZ - p.z) * invDt - taken.z,
	};
	const glm::vec3 velocity = taken + wanted * velocityBlend;
	const float x = velocity.x * dt + p.x;
	const float z = dt * velocity.z + p.z;
	// its height rides from where it was taken onto the funnel over the land under it
	const float funnelSquare = settings.yOffset * settings.yOffset / settings.rZeroHeight;
	map_coords::MapCoords under;
	under.x = MapFixed(x);
	under.z = MapFixed(z);
	under.altitude = 0.0f;
	const float land = ground(under);
	const float start = atom.startHeight;
	const float y = (std::sqrt(radius1 * funnelSquare) + settings.yOffset - start) * blend + land + start;
	const glm::vec3 position {x, y, z};
	const glm::vec3 moved {(x - p.x) * invDt, (y - p.y) * invDt, (z - p.z) * invDt};
	return AttractStep {.position = position, .velocity = moved};
}

} // namespace openblack::vortex::crossing
