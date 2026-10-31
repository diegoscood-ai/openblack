/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFart.h"

#include <cmath>

#include <numbers>

using namespace openblack;

bool creature_fart::Farts(std::optional<uint32_t> current, std::optional<uint32_t> fart)
{
	return current.has_value() && fart.has_value() && *current == *fart;
}

bool creature_fart::MakesFaint(float alignment)
{
	// Written as the game makes the test, so that an alignment that is no number makes them faint too
	return !(alignment >= k_FaintingBelow);
}

float creature_fart::PuffSize(float drawnSize)
{
	return drawnSize * k_PuffSizePerSize;
}

std::array<glm::vec3, creature_fart::k_Puffs> creature_fart::PuffVelocities(float yaw, float drawnSize,
                                                                            const FloatRandom& random)
{
	const float size = drawnSize * k_CloudPerSize;
	// Behind the body is its turn and half a turn more. The sine and cosine keep their full precision until they are
	// scaled, and only the scaled values are rounded
	const float behind = yaw + std::numbers::pi_v<float>;
	const auto behindX = static_cast<float>(std::sin(static_cast<double>(behind)) * static_cast<double>(size));
	const auto behindZ = -static_cast<float>(std::cos(static_cast<double>(behind)) * static_cast<double>(size));
	const float half = size * 0.5f;
	std::array<glm::vec3, k_Puffs> velocities {};
	for (auto& velocity : velocities)
	{
		// One draw at a time, each spread rounded on its own before it is added
		const float spreadZ = random(size) - half;
		const float spreadY = random(size) - half;
		const float spreadX = random(size) - half;
		velocity = glm::vec3(spreadX + behindX, spreadY, behindZ + spreadZ);
	}
	return velocities;
}
