/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSpellMaths.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/GUtilsX87.h"
#include "Common/ModelInverseSquareRoot.h"

using namespace openblack::particles;
namespace x87 = openblack::gutils::x87;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// How sharply a wisp narrows above its full width
constexpr float k_TaperSteepness = 3.3333333f;
/// A wisp's opacity comes up from the least by the rise over this many seconds
constexpr float k_WispFadeInSeconds = 0.5f;
constexpr float k_WispLeastAlpha = 20.0f;
constexpr float k_WispAlphaRise = 30.0f;
} // namespace

maths::WispBox maths::WispBoxOf(std::span<const glm::vec3> bones)
{
	if (bones.empty())
	{
		return {};
	}
	glm::vec3 low(std::numeric_limits<float>::max());
	glm::vec3 high(std::numeric_limits<float>::lowest());
	for (const auto& bone : bones)
	{
		low = glm::min(low, bone);
		high = glm::max(high, bone);
	}
	const float dx = high.x - low.x;
	const float dz = high.z - low.z;
	return {.centreX = (low.x + high.x) * 0.5f,
	        .centreZ = (low.z + high.z) * 0.5f,
	        .bottom = low.y,
	        .height = high.y - low.y,
	        .radius = 0.5f * std::sqrt(dx * dx + dz * dz)};
}

glm::vec3 maths::WispOrbit(const WispBox& box, float theta, float phi, float taper, float age)
{
	// In the FPU's order and rounding: each sine and cosine unrounded into the operation after it
	const double up = x87::Mul(x87::Add(x87::Cos(phi), 1.0), 0.5);
	double width = 1.0;
	if (!(up < k_WispFullWidthBelow))
	{
		const double over = x87::Mul(x87::Sub(up, k_WispFullWidthBelow), k_TaperSteepness);
		width = x87::Sub(1.0, x87::Mul(x87::Mul(over, over), taper));
	}
	const auto [sinTheta, cosTheta] = x87::SinCos(theta);
	const float roundX = x87::Store(x87::Mul(x87::Mul(x87::Mul(cosTheta, width), box.radius), k_WispReach));
	const float x = x87::Store(x87::Add(roundX, box.centreX));
	const float y = x87::Store(x87::Add(box.bottom, x87::Store(x87::Mul(x87::Mul(up, box.height), k_WispReach))));
	const double z = x87::Add(x87::Mul(x87::Mul(x87::Mul(sinTheta, width), box.radius), k_WispReach), box.centreZ);
	// It wobbles on its way round, each axis by two sines summed before they are rounded
	const double t = age;
	const float wobbleX = x87::Store(x87::Add(x87::Sin(x87::Mul(t, 1.3f)), x87::Sin(t)));
	const float wobbleY = x87::Store(x87::Add(x87::Sin(x87::Mul(t, 1.13f)), x87::Sin(x87::Mul(t, 0.53f))));
	const double wobbleZ = x87::Add(x87::Sin(x87::Mul(t, 0.9f)), x87::Sin(x87::Mul(t, 2.72f)));
	const float byX = x87::Store(x87::Mul(x87::Store(x87::Mul(wobbleX, k_WispWobble)), box.radius));
	const float byY = x87::Store(x87::Mul(x87::Store(x87::Mul(wobbleY, k_WispWobble)), box.radius));
	const double byZ = x87::Mul(x87::Mul(wobbleZ, k_WispWobble), box.radius);
	return {x87::Store(x87::Add(byX, x)), x87::Store(x87::Add(y, byY)), x87::Store(x87::Add(z, byZ))};
}

glm::vec3 maths::WispPosition(glm::vec3 hand, glm::vec3 orbit, float age)
{
	const float flown = std::clamp(age / k_WispFlightSeconds, 0.0f, 1.0f);
	return hand + (orbit - hand) * flown;
}

uint8_t maths::WispAlpha(float age)
{
	const float risen = std::clamp(age / k_WispFadeInSeconds, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int32_t>(risen * k_WispAlphaRise + k_WispLeastAlpha));
}

float maths::WispsDue(float due, int most, float dt, float emitSeconds)
{
	const auto mostDue = static_cast<float>(most);
	if (!(due < mostDue))
	{
		return due;
	}
	return std::min(due + mostDue * dt / emitSeconds, mostDue);
}

bool maths::HasAtomRate(int most, float lifetime)
{
	return x87::Div(static_cast<double>(most), lifetime) > 0.0;
}

float maths::AtomsDue(float due, int most, float lifetime, float dt)
{
	return x87::Store(x87::MulAdd(x87::Div(static_cast<double>(most), lifetime), dt, due));
}

glm::vec3 maths::FlungVelocity(glm::vec3 random, float speed, float size)
{
	const double scale = x87::Mul(speed, size);
	return {x87::Store(x87::Mul(random.x, scale)), x87::Store(x87::Mul(random.y, scale)),
	        x87::Store(x87::Mul(random.z, scale))};
}

glm::vec3 maths::SteamStart(glm::vec3 foot, glm::vec3 random, float size)
{
	const float spread = x87::Store(x87::Mul(size, k_SteamSpread));
	// The first part's product goes straight into its sum; the other two are stored first
	const float x = x87::Store(x87::Add(x87::Mul(random.x, spread), foot.x));
	const float y = x87::Store(x87::Add(foot.y, x87::Store(x87::Mul(random.y, spread))));
	const float z = x87::Store(x87::Add(foot.z, x87::Store(x87::Mul(random.z, spread))));
	return {x, y, z};
}

std::optional<float> maths::SteamOffLand(float y, float ground, float minHeight, float size)
{
	const double lift = x87::Mul(minHeight, size);
	if (x87::Sub(y, ground) >= lift)
	{
		return std::nullopt;
	}
	return x87::Store(x87::Add(lift, ground));
}

glm::vec3 maths::ItchOrbit(glm::vec3 rightEye, glm::vec3 leftEye, float seconds, float orbitSpeed)
{
	const auto centre = (rightEye + leftEye) * 0.5f;
	const auto out = (rightEye - centre) * k_ItchReach;
	glm::vec3 across(out.z - 0.0f, 0.0f, 0.0f - out.x);
	// Both lengths summed in the original's order: y² + z², then x²
	const double outLength =
	    x87::Sqrt(x87::Add(x87::Add(x87::Mul(out.y, out.y), x87::Mul(out.z, out.z)), x87::Mul(out.x, out.x)));
	// A part that is 0, or not a number, counts as none
	const auto none = [](float v) { return !(v < 0.0f || v > 0.0f); };
	if (!none(across.x) || !none(across.y) || !none(across.z))
	{
		const double acrossLength = x87::Sqrt(
		    x87::Add(x87::Add(x87::Mul(across.z, across.z), x87::Mul(across.y, across.y)), x87::Mul(across.x, across.x)));
		const double scale = x87::Div(outLength, acrossLength);
		across = {x87::Store(x87::Mul(scale, across.x)), x87::Store(x87::Mul(scale, across.y)),
		          x87::Store(x87::Mul(scale, across.z))};
	}
	const float angle = std::fmod(seconds * orbitSpeed, k_TwoPi);
	const auto [sinAngle, cosAngle] = x87::SinCos(angle);
	// The way across takes the sine unrounded; the cosine is stored to a float first
	const glm::vec3 turned(x87::Store(x87::Mul(across.x, sinAngle)), x87::Store(x87::Mul(across.y, sinAngle)),
	                       x87::Store(x87::Mul(across.z, sinAngle)));
	return turned + (centre + out * x87::Store(cosAngle));
}

bool maths::SwungTooFar(glm::vec2 began, glm::vec2 now, float limit)
{
	// Each way made unit length as the game makes it, by the 3D engine's inverse square root of x² + z²
	const float a = gutils::ModelInverseSquareRoot(began.x * began.x + began.y * began.y);
	const float b = gutils::ModelInverseSquareRoot(now.x * now.x + now.y * now.y);
	const glm::vec2 from(began.x * a, began.y * a);
	const glm::vec2 to(now.x * b, now.y * b);
	const float dot = from.x * to.x + from.y * to.y;
	// The game's cosine keeps more than a float's precision for the comparison
	return std::cos(static_cast<double>(limit)) > static_cast<double>(dot);
}
