/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeartShell.h"

#include <cstddef>

#include <algorithm>
#include <iterator>
#include <numeric>

using namespace openblack;

float heart_shell::Along(const std::array<float, 4>& values, float clock)
{
	// Up to the first moment (or no number at all), the first value
	if (!(clock > k_Moments[0]))
	{
		return values[0];
	}
	if (clock < k_Moments[3])
	{
		for (size_t i = 0; i + 1 < k_Moments.size(); ++i)
		{
			if (clock < k_Moments.at(i + 1))
			{
				const float share = (clock - k_Moments.at(i)) / (k_Moments.at(i + 1) - k_Moments.at(i));
				return share * (values.at(i + 1) - values.at(i)) + values.at(i);
			}
		}
	}
	return values[3];
}

float heart_shell::Size(float clock)
{
	return Along(k_Sizes, clock) * k_SizeScale;
}

float heart_shell::Reach(float clock, float pulse)
{
	return Along(k_Reaches, clock) * pulse;
}

glm::mat4 heart_shell::Model(glm::vec3 heart, float size)
{
	glm::mat4 model(0.0f);
	model[0][0] = size;
	model[1][1] = size;
	model[2][2] = size;
	model[3] = glm::vec4(heart, 1.0f);
	return model;
}

std::vector<size_t> heart_shell::DrawOrder(std::span<const uint32_t> stamps)
{
	std::vector<size_t> order(stamps.size());
	std::iota(order.begin(), order.end(), size_t {0});
	std::ranges::stable_sort(order, [stamps](size_t a, size_t b) { return stamps[a] > stamps[b]; });
	return order;
}

std::vector<heart_shell::PassDraw> heart_shell::Pass(std::span<const PassHeart> hearts)
{
	std::vector<uint32_t> stamps;
	stamps.reserve(hearts.size());
	std::ranges::transform(hearts, std::back_inserter(stamps), &PassHeart::creationStamp);
	std::vector<PassDraw> draws;
	draws.reserve(hearts.size() * 2);
	for (const auto heart : DrawOrder(stamps))
	{
		if (hearts[heart].shell)
		{
			draws.push_back({.heart = heart, .step = PassStep::Shell});
		}
		if (hearts[heart].model)
		{
			draws.push_back({.heart = heart, .step = PassStep::Model});
		}
	}
	return draws;
}
