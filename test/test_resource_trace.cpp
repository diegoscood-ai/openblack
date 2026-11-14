/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <string>

#include <Debug/ResourceTrace.h>
#include <gtest/gtest.h>

using namespace openblack::debug::resource_trace;

TEST(ResourceTrace, OffWithoutTheVariable)
{
	EXPECT_FALSE(IntervalFrom(nullptr, nullptr).has_value());
	EXPECT_FALSE(IntervalFrom("", "30").has_value());
}

TEST(ResourceTrace, AMinuteByDefault)
{
	EXPECT_EQ(IntervalFrom("1", nullptr), std::chrono::seconds {60});
	EXPECT_EQ(IntervalFrom("1", "abc"), std::chrono::seconds {60});
	EXPECT_EQ(IntervalFrom("1", "0"), std::chrono::seconds {60});
	EXPECT_EQ(IntervalFrom("1", "-5"), std::chrono::seconds {60});
}

TEST(ResourceTrace, SecondsGiven)
{
	EXPECT_EQ(IntervalFrom("1", "15"), std::chrono::seconds {15});
}

TEST(ResourceTrace, FormatsEveryNumber)
{
	const Sample sample {
	    .turn = 812,
	    .workingSet = 900ull * 1024 * 1024,
	    .privateBytes = 768ull * 1024 * 1024 + 512 * 1024,
	    .handles = 512,
	    .gdiObjects = 40,
	    .userObjects = 30,
	    .entitiesAlive = 5000,
	    .entitiesSlots = 6000,
	    .entitiesMade = 7000,
	    .effects = 12,
	    .atoms = 345,
	    .containers = 3,
	    .nextEffectId = 80001,
	    .lightSheets = 2,
	    .strikeAtoms = 1,
	    .arcAtoms = 0,
	    .textures = 410,
	    .shaders = 60,
	    .programs = 30,
	    .uniforms = 90,
	    .vertexBuffers = 200,
	    .indexBuffers = 190,
	    .dynamicVertexBuffers = 7,
	    .dynamicIndexBuffers = 6,
	    .frameBuffers = 9,
	    .vertexLayouts = 11,
	    .occlusionQueries = 4,
	    .textureMemory = 256ll * 1024 * 1024,
	    .renderTargetMemory = 64ll * 1024 * 1024,
	    .gpuMemoryUsed = 0,
	};
	EXPECT_EQ(Format(sample), "Resource trace: turn 812, RAM 900.0 MB private 768.5 MB, handles 512 GDI 40 USER 30, "
	                          "entities 5000 of 6000 (made 7000), effects 12 atoms 345 containers 3 next id 80001 sheets 2 "
	                          "strikes 1 arcs 0, bgfx textures 410 shaders 60 programs 30 uniforms 90 vb 200 ib 190 dvb 7 "
	                          "dib 6 fb 9 layouts 11 queries 4, VRAM textures 256.0 MB targets 64.0 MB");
}

TEST(ResourceTrace, GpuMemoryOnlyWhenTheBackendSaysIt)
{
	const Sample sample {.gpuMemoryUsed = 3ll * 1024 * 1024};
	EXPECT_TRUE(Format(sample).ends_with(" GPU 3.0 MB"));
}
