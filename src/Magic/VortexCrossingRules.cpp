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

namespace openblack::vortex::crossing
{

namespace
{
/// The share of the way a thing has blended, 0 to 1: nought (and a NaN) gives 0, a share not below one gives 1
float Blend(float age, float time)
{
	const auto share = static_cast<float>(static_cast<double>(age) / static_cast<double>(time));
	if (!(share > 0.0f))
	{
		return 0.0f;
	}
	return share < 1.0f ? share : 1.0f;
}

/// The swing round the middle is this many times the setting's speed
constexpr double k_SwingScale = 30.0;
/// Once it has blended this far, a thing that can't be destroyed may go from the middle
constexpr float k_IndestructibleGoesAt = 0.5f;
/// A place across the land as the map takes it from metres: x 65536, then x 0.1 (as a float), truncated
int32_t MapFixed(float metres)
{
	constexpr double k_Fixed = 65536.0;
	constexpr auto k_Tenth = static_cast<double>(0.1f);
	return static_cast<int32_t>(static_cast<double>(metres) * k_Fixed * k_Tenth);
}
} // namespace

float PileShare(float random)
{
	constexpr auto k_Spread = static_cast<double>(0.4f);
	constexpr auto k_Least = static_cast<double>(0.3f);
	return static_cast<float>(static_cast<double>(random) * k_Spread + k_Least);
}

std::optional<AttractStep> Attract(const AttractSettings& settings, const AttractAtom& atom, glm::vec3 origin, float dt,
                                   float invDt, const GroundAt& ground)
{
	const auto& p = atom.position;
	const auto& taken = atom.takenVelocity;
	// its distance and angle from the middle, across the land
	const double dx = static_cast<double>(p.x) - static_cast<double>(origin.x);
	const double dz = static_cast<double>(p.z) - static_cast<double>(origin.z);
	const auto radius = static_cast<float>(std::sqrt(dz * dz + dx * dx));
	const auto angle = static_cast<float>(std::atan2(dz, dx));
	const float blend = Blend(atom.age, settings.blendTime);
	const float velocityBlend = Blend(atom.age, settings.blendTimeVelocity);
	const double swingSpeed = static_cast<double>(blend) * static_cast<double>(settings.thetaSpeed);
	// drawn in by the share of its distance, never nearer than the least
	auto radius1 = static_cast<float>(static_cast<double>(radius) - static_cast<double>(blend) *
	                                                                    static_cast<double>(settings.radialSpeed) *
	                                                                    static_cast<double>(dt) * static_cast<double>(radius));
	if (!(settings.minRadius < radius1))
	{
		radius1 = settings.minRadius;
	}
	const auto swung = static_cast<float>(swingSpeed * static_cast<double>(dt) * k_SwingScale / static_cast<double>(radius1) +
	                                      static_cast<double>(angle));
	// At the middle, once blended in, it is gone; a thing that can't be destroyed goes from half way blended
	if (radius1 < settings.deletionRadius && (!(blend < 1.0f) || (atom.indestructible && !(blend < k_IndestructibleGoesAt))))
	{
		return std::nullopt;
	}
	// the point on its circle it would be at this step
	const double cosine = std::cos(static_cast<double>(swung));
	const auto sine = static_cast<float>(std::sin(static_cast<double>(swung)));
	const auto sineRadius = static_cast<float>(static_cast<double>(sine) * static_cast<double>(radius1));
	const double targetX = cosine * static_cast<double>(radius1) + static_cast<double>(origin.x);
	const auto targetY = static_cast<float>(0.0 + static_cast<double>(origin.y));
	const auto targetZ = static_cast<float>(static_cast<double>(sineRadius) + static_cast<double>(origin.z));
	const auto toY = static_cast<float>(static_cast<double>(targetY) - static_cast<double>(p.y));
	const auto toZ = static_cast<float>(static_cast<double>(targetZ) - static_cast<double>(p.z));
	// the velocity that would put it there this step, less the one it was taken with, then blended in
	const glm::vec3 wanted {
	    static_cast<float>((targetX - static_cast<double>(p.x)) * static_cast<double>(invDt) - static_cast<double>(taken.x)),
	    static_cast<float>(static_cast<double>(toY) * static_cast<double>(invDt) - static_cast<double>(taken.y)),
	    static_cast<float>(static_cast<double>(toZ) * static_cast<double>(invDt) - static_cast<double>(taken.z)),
	};
	const glm::vec3 velocity = taken + wanted * velocityBlend;
	// across the land: x through its rounded step, z through its unrounded one
	const auto stepX = static_cast<float>(static_cast<double>(velocity.x) * static_cast<double>(dt));
	const auto x = static_cast<float>(static_cast<double>(stepX) + static_cast<double>(p.x));
	const auto z = static_cast<float>(static_cast<double>(dt) * static_cast<double>(velocity.z) + static_cast<double>(p.z));
	// its height rides from where it was taken onto the funnel over the land under it
	const auto funnelSquare = static_cast<float>(static_cast<double>(settings.yOffset) * static_cast<double>(settings.yOffset) /
	                                             static_cast<double>(settings.rZeroHeight));
	map_coords::MapCoords under;
	under.x = MapFixed(x);
	under.z = MapFixed(z);
	under.altitude = 0.0f;
	const auto land = static_cast<double>(ground(under));
	const double funnel =
	    std::sqrt(static_cast<double>(radius1) * static_cast<double>(funnelSquare)) + static_cast<double>(settings.yOffset);
	const double start = static_cast<double>(atom.startHeight);
	const auto y = static_cast<float>((funnel - start) * static_cast<double>(blend) + land + start);
	const glm::vec3 position {x, y, z};
	const auto movedY = static_cast<float>(static_cast<double>(y) - static_cast<double>(p.y));
	const auto movedZ = static_cast<float>(static_cast<double>(z) - static_cast<double>(p.z));
	const glm::vec3 moved {
	    static_cast<float>((static_cast<double>(x) - static_cast<double>(p.x)) * static_cast<double>(invDt)),
	    static_cast<float>(static_cast<double>(movedY) * static_cast<double>(invDt)),
	    static_cast<float>(static_cast<double>(movedZ) * static_cast<double>(invDt)),
	};
	return AttractStep {.position = position, .velocity = moved};
}

} // namespace openblack::vortex::crossing
