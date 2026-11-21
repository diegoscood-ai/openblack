/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The wind sway the trees and the ripe fields share, before its speeds are first drawn (no random numbers)

#include <cstdint>

#include <array>
#include <bit>
#include <chrono>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/VegetationSystem.h"

using namespace openblack;

TEST(VegetationSystem, NoLeanBeforeTheFirstUpdate)
{
	const ecs::systems::VegetationSystem vegetation;
	for (uint8_t slot = 0; slot < ecs::systems::VegetationInterface::k_SwayCount; ++slot)
	{
		EXPECT_EQ(vegetation.GetLean(slot), 0.0f);
	}
}

TEST(VegetationSystem, StillSwaysLeanTheirFullWayUntilTheSpeedsAreDrawn)
{
	ecs::systems::VegetationSystem vegetation;
	// The speeds are 0 for the first two seconds: every sway stays at its start, leaning back by 3% of the height
	vegetation.Update(std::chrono::duration<float, std::milli>(1000.0f));
	EXPECT_FLOAT_EQ(vegetation.GetLean(0), -0.03f);
	// The slots wrap at 16
	EXPECT_EQ(vegetation.GetLean(16), vegetation.GetLean(0));
}

TEST(VegetationSystem, AFieldLeansFurtherThanATree)
{
	ecs::systems::VegetationSystem vegetation;
	vegetation.Update(std::chrono::duration<float, std::milli>(1000.0f));
	glm::mat4 model(2.0f);
	model[1].x = 5.0f;
	model[1].w = 7.0f;
	const auto swaying = vegetation.GetFieldMatrix(model, 4.0f, 3);
	EXPECT_EQ(swaying[1].x, 0.0f);
	EXPECT_EQ(swaying[1].z, 4.0f * 1.75f * vegetation.GetLean(3));
	// The rest of the matrix is kept, the second column's w too
	EXPECT_EQ(swaying[1].y, 2.0f);
	EXPECT_EQ(swaying[1].w, 7.0f);
	EXPECT_EQ(swaying[0], model[0]);
	EXPECT_EQ(swaying[3], model[3]);
}

TEST(VegetationSystem, ASwayStepsAndLeansInTheGamesPrecision)
{
	// The phase moves on by the frame's whole ms times the speed times 7 / 6600, and the lean is the cosine times
	// 0.03 rounded once: the bits of the original's trees' pre-draw run on the exe through the oracle (a float cosine
	// times a float 0.03 is a bit apart in two of these)
	struct Case
	{
		float milliseconds, speed, phase;
		uint32_t nextPhase, lean;
	};
	constexpr std::array<Case, 5> k_Cases = {{
	    {16.0f, 1.37f, 0.0f, 0x3CBE739Cu, 0xBCF5B18Fu},
	    {33.0f, 1.9f, 2.5f, 0x40244189u, 0x3CCE3A3Eu},
	    {17.0f, 1.0001f, 100.25f, 0x42C8893Cu, 0xBCED505Du},
	    {50.0f, 1.5f, 7.77f, 0x40FB2F7Au, 0xB90B8C86u},
	    {1.0f, 1.25f, 1.5707963f, 0x3FC93B4Bu, 0x3826CE32u},
	}};
	using ecs::systems::VegetationSystem;
	for (const auto& c : k_Cases)
	{
		const float phase = VegetationSystem::NextPhase(c.phase, c.milliseconds, c.speed);
		EXPECT_EQ(std::bit_cast<uint32_t>(phase), c.nextPhase) << c.phase;
		EXPECT_EQ(std::bit_cast<uint32_t>(VegetationSystem::LeanAt(phase)), c.lean) << c.phase;
	}
}

TEST(VegetationSystem, FourThousandSwaysStepAndLeanAsTheOriginal)
{
	// 256 frames of 0 to 2000 ms, each stepping 16 sways of speeds 1 to 2 from phases up to a million: the FNV-1a hash
	// of every phase's and lean's bits, as the original's trees' pre-draw gave them on the exe through the oracle
	uint32_t seed = 0x5EEDu;
	const auto next = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return seed;
	};
	uint32_t hash = 0x811C9DC5u;
	const auto mix = [&hash](uint32_t bits) {
		for (int shift = 0; shift < 32; shift += 8)
		{
			hash = (hash ^ ((bits >> shift) & 0xFFu)) * 0x01000193u;
		}
	};
	using ecs::systems::VegetationSystem;
	for (int frame = 0; frame < 256; ++frame)
	{
		const auto milliseconds = static_cast<float>(next() % 2001u);
		for (int sway = 0; sway < 16; ++sway)
		{
			const float speed = std::bit_cast<float>(0x3F800000u | (next() & 0x7FFFFFu));
			// half from anywhere below a million, half from 0.5 to 32768
			const uint32_t phaseBits = (next() & 1u) != 0 ? next() % 0x49742400u : 0x3F000000u + next() % 0x08000000u;
			const float phase = VegetationSystem::NextPhase(std::bit_cast<float>(phaseBits), milliseconds, speed);
			mix(std::bit_cast<uint32_t>(phase));
			mix(std::bit_cast<uint32_t>(VegetationSystem::LeanAt(phase)));
		}
	}
	EXPECT_EQ(hash, 0xC09E3FD6u);
}
