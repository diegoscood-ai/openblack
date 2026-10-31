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

int32_t temple_fade::Step(float depth, const TempleFade& fade)
{
	// Nearer than the start, or no number, is the first step, and past the end the last. In between the step is the way
	// from the start times the steps per unit, each a float, truncated
	if (!(depth >= fade.from))
	{
		return 0;
	}
	if (!(depth <= fade.to))
	{
		return k_Steps - 1;
	}
	const float along = (depth - fade.from) * StepsPerUnit(fade);
	return std::clamp(static_cast<int32_t>(along), 0, k_Steps - 1);
}

float temple_fade::StepsPerUnit(const TempleFade& fade)
{
	return static_cast<float>(k_Steps) / (fade.to - fade.from);
}

uint32_t temple_fade::ShareOfStep(int32_t step)
{
	// step k leaves 256 less (256 - 100) k / 1024 of 256, at 255/256 of the byte
	const auto left = 256 - (((256 - k_Darkest) * step) >> 10);
	return static_cast<uint32_t>((255 * left) >> 8);
}

uint32_t temple_fade::Share(float depth, const TempleFade& fade)
{
	return ShareOfStep(Step(depth, fade));
}

glm::vec3 temple_fade::EndColour(glm::vec3 lightMultiply)
{
	// The light's colour in whole bytes, each times 0.4 as a float
	constexpr float k_EndShare = 0.4f;
	return glm::floor(lightMultiply * 255.0f + 0.5f) * k_EndShare;
}

glm::vec3 temple_fade::AddedAt(int32_t step, glm::vec3 endColour)
{
	// The step's share of the way, exact as a float, times each channel as a float, then rounded as the FPU rounds
	const float along = static_cast<float>(step) * (1.0f / static_cast<float>(k_Steps));
	const glm::vec3 added = endColour * along;
	return {std::nearbyint(added.r), std::nearbyint(added.g), std::nearbyint(added.b)};
}

temple_fade::Fogged temple_fade::Once(glm::vec3 colour, glm::vec3 added, float depth, const TempleFade& fade,
                                      glm::vec3 endColour)
{
	const auto step = Step(depth, fade);
	const auto share = static_cast<float>(ShareOfStep(step));
	return {
	    .colour = glm::floor(colour * share / 256.0f),
	    .added = glm::min(added + AddedAt(step, endColour), glm::vec3(255.0f)),
	};
}

temple_fade::Fogged temple_fade::AtVertex(glm::vec3 colour, glm::vec3 added, float depth, const TempleFade& fade,
                                          glm::vec3 endColour)
{
	// Up to the start (or at no number) the vertex keeps its own colours, and from the end on it takes the last step's
	if (!(depth > fade.from))
	{
		return {.colour = colour, .added = added};
	}
	if (!(depth < fade.to))
	{
		return {.colour = glm::vec3(static_cast<float>(ShareOfStep(k_Steps - 1))), .added = AddedAt(k_Steps - 1, endColour)};
	}
	const auto step = Step(depth, fade);
	// A quarter of the step is the share, of 256, each byte moves by towards the step's
	const auto towards = static_cast<float>(step / 4);
	const glm::vec3 grey(static_cast<float>(ShareOfStep(step)));
	return {
	    .colour = colour + glm::floor((grey - colour) * towards / 256.0f),
	    .added = added + glm::floor((AddedAt(step, endColour) - added) * towards / 256.0f),
	};
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
