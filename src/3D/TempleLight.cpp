/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleLight.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/common.hpp>

using namespace openblack;

namespace
{
/// A turn of the pulse is 512 steps of eight milliseconds
constexpr float k_PulseStep = 0.012271846f;
/// The pulse's colours are a third of a turn apart
constexpr float k_Third = 2.0943952f;
constexpr float k_TwoThirds = 4.1887903f;
constexpr int32_t k_PulseMiddle = 22;
constexpr int32_t k_PulseReach = 11;
/// Evil's light, red, green and blue
constexpr std::array<int32_t, 3> k_Evil {0xFF, 0xAF, 0xA0};

int32_t Pulse(float angle)
{
	return static_cast<int32_t>(std::sin(angle) * k_PulseReach + k_PulseMiddle);
}
} // namespace

TempleLight TempleLight::At(float alignment, uint32_t milliseconds)
{
	const float angle = static_cast<float>((milliseconds >> 3) & 0x1FF) * k_PulseStep;
	std::array<int32_t, 3> multiply {0xFF, 0xFF, 0xFF};
	std::array<int32_t, 3> add {};
	if (alignment < 0.0f)
	{
		multiply = k_Evil;
		add[0] = static_cast<int32_t>((std::sin(angle + k_TwoThirds) + 1.0) * k_PulseReach);
	}
	else
	{
		add = {Pulse(angle), Pulse(angle + k_Third), Pulse(angle + k_Third + k_TwoThirds)};
	}

	// Each comes in the more the further the alignment leans: the multiply from white, in whole bytes as the game's
	// arithmetic has it, and the add from nothing
	const auto strength = static_cast<int32_t>(std::abs(alignment) * 255.0);
	TempleLight light;
	for (size_t i = 0; i < 3; ++i)
	{
		const auto darkened = static_cast<uint32_t>((multiply.at(i) - 0xFF) * strength);
		light.multiply[static_cast<glm::length_t>(i)] = static_cast<float>(((darkened >> 8) - 1) & 0xFF) / 255.0f;
		light.add[static_cast<glm::length_t>(i)] = static_cast<float>(((add.at(i) * strength) >> 8) & 0xFF) / 255.0f;
	}
	return light;
}

TempleFade temple_fade::Of(bool inARoom, bool inCreatureRoom, bool creatureRoomDrawnBefore)
{
	if (!inARoom)
	{
		return k_NoRoom;
	}
	return inCreatureRoom && creatureRoomDrawnBefore ? k_CreatureRoom : k_Room;
}

float temple_fade::Depth(const glm::mat4& view, const glm::vec3& point)
{
	// the view's z row (glm's columns hold the rows' entries), each product and sum a float
	const float zy = (view[2][2] * point.z) + (view[1][2] * point.y);
	const float zyx = zy + (view[0][2] * point.x);
	return zyx + view[3][2];
}

uint32_t temple_fade::Share(float depth, const TempleFade& fade)
{
	// Nearer than the start, or no number, is the first step, and past the end the last. In between the step is the way
	// from the start times the steps per unit, each a float, truncated
	int32_t step = 0;
	if (!(depth >= fade.from))
	{
		step = 0;
	}
	else if (!(depth <= fade.to))
	{
		step = k_Steps - 1;
	}
	else
	{
		const float perUnit = static_cast<float>(k_Steps) / (fade.to - fade.from);
		const float along = (depth - fade.from) * perUnit;
		step = std::clamp(static_cast<int32_t>(along), 0, k_Steps - 1);
	}
	// step k leaves 256 less (256 - 100) k / 1024 of 256, at 255/256 of the byte
	const auto left = 256 - (((256 - k_Darkest) * step) >> 10);
	return static_cast<uint32_t>((255 * left) >> 8);
}

uint32_t temple_fade::Apply(uint32_t argb, float depth, const TempleFade& fade)
{
	const auto share = Share(depth, fade);
	uint32_t faded = argb & 0xFF000000u;
	for (const uint32_t shift : {0u, 8u, 16u})
	{
		faded |= ((((argb >> shift) & 0xFFu) * share) >> 8) << shift;
	}
	return faded;
}

glm::vec3 TempleLight::Colour(glm::vec3 colour) const
{
	// Each byte of the colour by the light's, over 256
	const auto bytes = glm::floor(colour * 255.0f + 0.5f);
	const auto light = glm::floor(multiply * 255.0f + 0.5f);
	return glm::floor(bytes * light / 256.0f) / 255.0f;
}
