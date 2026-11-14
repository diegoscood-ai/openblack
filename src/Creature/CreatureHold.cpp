/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureHold.h"

#include <cmath>

#include <utility>

#include "3D/ObjectMatrix.h"

using namespace openblack;

glm::mat3 creature_hold::HeldTurn(const glm::mat3& handRows, bool leftHand)
{
	auto turn = handRows;
	affine::NormaliseRows(turn);
	std::swap(turn[1], turn[2]);
	turn[0] = -turn[0];
	if (leftHand)
	{
		turn[1] = -turn[1];
	}
	else
	{
		turn[2] = -turn[2];
	}
	return turn;
}

glm::mat3 creature_hold::ScaledRows(const glm::mat3& turn, float scale)
{
	return {turn[0] * scale, turn[1] * scale, turn[2] * scale};
}

glm::vec3 creature_hold::HungBelow(const glm::vec3& grasp, const glm::vec3& scaledUp, float lowering)
{
	auto offset = scaledUp;
	if (offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f)
	{
		// the length taken as a float, summed x, y, then z, and the row times the lowering over it
		const float length = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
		offset *= lowering / length;
	}
	return grasp - offset;
}

creature_hold::HeldPlacement creature_hold::Place(const glm::mat3& handRows, const glm::vec3& grasp, bool leftHand, float scale,
                                                  float lowering)
{
	const auto turn = HeldTurn(handRows, leftHand);
	return {.turn = turn, .position = HungBelow(grasp, turn[1] * scale, lowering)};
}

glm::vec3 creature_hold::AnglesAtRelease(const glm::mat3& handRows, bool leftHand, float scale)
{
	float y = 0.0f;
	float x = 0.0f;
	float z = 0.0f;
	affine::DecomposeYXZ(ScaledRows(HeldTurn(handRows, leftHand), scale), y, x, z);
	return {x, y, z};
}
