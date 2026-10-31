/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptTargetRelative.h"

#include "Common/GUtilsX87.h"

namespace openblack::script::target_relative
{
namespace
{
namespace x87 = gutils::x87;

/// The degrees to radians factor as a float
constexpr double k_DegreesToRadians = static_cast<double>(0.0174532924f);

/// The original's test for a part of the direction: a comparison with nought, which a not-a-number also passes, so
/// only an ordered value other than nought counts
[[nodiscard]] bool IsSomething(float value)
{
	return value < 0.0f || value > 0.0f;
}
} // namespace

glm::vec3 Position(const glm::vec3& from, const glm::vec3& to, float distance, float angle)
{
	float x = x87::Store(x87::Sub(to.x, from.x));
	float y = x87::Store(x87::Sub(to.y, from.y));
	float z = x87::Store(x87::Sub(to.z, from.z));
	if (IsSomething(x) || IsSomething(y) || IsSomething(z))
	{
		const double squares = x87::Add(x87::Add(x87::Mul(z, z), x87::Mul(y, y)), x87::Mul(x, x));
		const float inverse = x87::Store(x87::Div(1.0, x87::Sqrt(squares)));
		x = x87::Store(x87::Mul(inverse, x));
		y = x87::Store(x87::Mul(y, inverse));
		z = x87::Store(x87::Mul(z, inverse));
	}

	// The sine stays unrounded on the FPU's stack; the cosine is stored to a float first
	const auto [sine, cosine] = x87::SinCos(x87::Mul(angle, k_DegreesToRadians));
	const double storedCosine = x87::Store(cosine);
	const x87::Extended minusSine {-sine.high, -sine.low};
	const float turnedX = x87::Store(x87::Add(x87::Mul(minusSine, z), x87::Mul(storedCosine, x)));
	const double turnedZ = x87::Add(x87::Mul(sine, x), x87::Mul(storedCosine, z));

	return {
	    x87::Store(x87::Add(x87::Store(x87::Mul(turnedX, distance)), to.x)),
	    x87::Store(x87::Add(x87::Store(x87::Mul(y, distance)), to.y)),
	    x87::Store(x87::Add(x87::Mul(turnedZ, distance), to.z)),
	};
}

} // namespace openblack::script::target_relative
