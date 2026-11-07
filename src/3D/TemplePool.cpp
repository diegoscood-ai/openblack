/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TemplePool.h"

#include <cmath>

#include <glm/common.hpp>

namespace openblack::temple_pool
{

float Advance(float seconds, int32_t frameMilliseconds)
{
	constexpr float k_SecondsPerMillisecond = 0.001f;
	const float step = static_cast<float>(frameMilliseconds) * k_SecondsPerMillisecond;
	return seconds + step;
}

int32_t Alpha(float seconds)
{
	// the sine taken whole, then the product and the sum each a float, as the game's single precision has them
	const auto swing = static_cast<float>(32.0 * std::sin(static_cast<double>(seconds)));
	return static_cast<int32_t>(swing + 128.0f);
}

glm::vec3 WaterColour(glm::vec3 lightBytes)
{
	return glm::floor(lightBytes * static_cast<float>(k_WaterShare) / 256.0f);
}

std::array<Layer, 2> Layers(float seconds)
{
	const auto alpha = Alpha(seconds);
	return {
	    Layer {.position = glm::vec3(0.0f), .yaw = 0.0f, .alpha = alpha, .uvOffset = {seconds * -0.01f, seconds * 0.007f}},
	    Layer {.position = {0.0f, k_SecondLayerLift, 0.0f},
	           .yaw = k_SecondLayerTurn,
	           .alpha = 255 - alpha,
	           .uvOffset = {seconds * 0.01f, seconds * 0.005f}},
	};
}

} // namespace openblack::temple_pool
