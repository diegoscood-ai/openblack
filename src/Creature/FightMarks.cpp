/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightMarks.h"

#include <algorithm>

#include "Common/GUtilsX87.h"

using namespace openblack;
using namespace openblack::creature_fight_marks;

namespace
{
namespace x87 = gutils::x87;

/// The shares of the queue the colour changes at, and their inverse, as the game's doubles
constexpr double k_Third = 0.33;
constexpr double k_TwoThirds = 0.66;
constexpr double k_PerThird = 3.0303030303030303;
constexpr double k_Full = 255.0;

/// A share of a third of the way through the colours, as a byte: truncated, and allowed past 255 as the game's is
int32_t Ramp(double share)
{
	return static_cast<int32_t>(x87::Mul(x87::Mul(share, k_PerThird), k_Full));
}

/// The turn by a heading into a creature's frame (one way) or out of it (the other): the cosine and sine of the
/// heading as the game's matrix stores them
struct Turn
{
	double cosine;
	double sine;
};
Turn TurnOf(float heading)
{
	return {.cosine = x87::Store(x87::Cos(heading)), .sine = x87::Store(x87::Sin(heading))};
}
} // namespace

void Marks::AddOnSkin(Mark::Kind kind, const creature_skin_ray::Spot& spot)
{
	if (Full())
	{
		return;
	}
	marks.at(count) = {.kind = kind, .spot = spot, .world = glm::vec3(0.0f), .local = glm::vec3(0.0f), .heldMs = 0, .frame = 0};
	pending = true;
	++count;
}

void Marks::AddOnGround(const glm::vec3& world, const glm::vec3& local)
{
	if (Full())
	{
		return;
	}
	marks.at(count) = {.kind = Mark::Kind::Ground, .spot = {}, .world = world, .local = local, .heldMs = 0, .frame = 0};
	++count;
}

void Marks::KeepNewest()
{
	if (count > 1)
	{
		marks.front() = marks.at(count - 1);
		count = 1;
	}
}

void Marks::Shunt()
{
	if (count == 0)
	{
		return;
	}
	std::shift_left(marks.begin(), marks.begin() + static_cast<std::ptrdiff_t>(count), 1);
	--count;
}

void Marks::DropWaiting()
{
	if (count > 0)
	{
		--count;
		pending = false;
	}
}

void Marks::Hold(uint32_t ms)
{
	if (pending && count > 0)
	{
		auto& held = marks.at(count - 1).heldMs;
		held = std::min(held + ms, k_MostHeldMs);
	}
}

glm::vec3 creature_fight_marks::LocalOf(const glm::vec3& point, const glm::vec3& position, float heading)
{
	// the way to the point turned by the creature's heading the other way round, as the press works it out: the cosine
	// and the sine stored as floats, the sine unrounded for the first axis
	const auto [cosine, sine] = TurnOf(heading);
	const auto sineUnrounded = x87::Sin(heading);
	const double across = x87::Sub(point.x, position.x);
	const float up = x87::Store(x87::Sub(point.y, position.y));
	const double along = x87::Store(x87::Sub(point.z, position.z));
	return {x87::Store(x87::Add(x87::Mul(cosine, across), x87::Mul(sineUnrounded, -along))), up,
	        x87::Store(x87::Add(x87::Mul(sine, x87::Store(across)), x87::Mul(cosine, along)))};
}

glm::vec3 creature_fight_marks::WorldOf(const glm::vec3& local, const glm::vec3& position, float heading)
{
	// the creature's matrix by its heading, applied as the game writes it out: z first, then y, then x, then the place
	const auto [cosine, sine] = TurnOf(heading);
	const auto axis = [&local](double m0, double m3, double m6, double m9) {
		return x87::Store(
		    x87::Add(x87::Add(x87::Add(x87::Mul(local.z, m6), x87::Mul(local.y, m3)), x87::Mul(local.x, m0)), m9));
	};
	return {axis(cosine, 0.0, sine, position.x), axis(0.0, 1.0, 0.0, position.y), axis(-sine, 0.0, cosine, position.z)};
}

glm::vec3 creature_fight_marks::DrawnOnGround(const glm::vec3& world, const glm::vec3& now, float turnFraction)
{
	const auto axis = [turnFraction](float from, float to) {
		return x87::Store(x87::Add(from, x87::Mul(x87::Sub(to, from), turnFraction)));
	};
	return {axis(world.x, now.x), axis(world.y, now.y), axis(world.z, now.z)};
}

uint32_t creature_fight_marks::Colour(size_t index, size_t queueSize)
{
	const double share = x87::Div(static_cast<double>(static_cast<int32_t>(queueSize) - static_cast<int32_t>(index)),
	                              static_cast<double>(queueSize));
	int32_t red = 255;
	int32_t green = 255;
	int32_t blue = 0;
	if (share < k_Third)
	{
		red = Ramp(share);
		green = 0;
	}
	else if (share < k_TwoThirds)
	{
		green = Ramp(x87::Sub(share, k_Third));
	}
	else
	{
		blue = Ramp(x87::Sub(share, k_TwoThirds));
	}
	// put together as the game does: the alpha above the red, then shifted up past the green and the blue, each OR-ed
	// in whole
	auto argb = static_cast<uint32_t>(red) | 0xFFFFA000u;
	argb = (argb << 8) | static_cast<uint32_t>(green);
	argb = (argb << 8) | static_cast<uint32_t>(blue);
	return argb;
}

glm::vec3 creature_fight_marks::TowardsCamera(const glm::vec3& at, const glm::vec3& camera)
{
	const auto axis = [](float from, float to) {
		return x87::Store(x87::Add(x87::Store(x87::Mul(x87::Store(x87::Sub(to, from)), 0.5)), from));
	};
	return {axis(at.x, camera.x), axis(at.y, camera.y), axis(at.z, camera.z)};
}
