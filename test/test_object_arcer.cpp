/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The electric arcs over what a lightning bolt struck (Particles/ParticleArcRules.h): the queue's gate, the points an
// arc picks on a fake model, pinned to the original's own code, and UR_ObjectArcer on a synthetic file stepped on the
// synced stream, with the game random hook seeing every draw

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <bit>
#include <memory>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Locator.h"
#include "Particles/ParticleArcRules.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/ExplodeObject.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{

/// SF_OnFire's arcer, as the game's file has it: group 2 holds the rule, its atoms carry the ribbon's group 3
constexpr std::string_view k_Arcer = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_ObjectArcer UR_ObjectArcer0
BEGINPROPERTIES
PROPERTY AverageTicksPerUpdate INTEGER 2
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 1 3
PROPERTY JointsPerArc INTEGER 8
PROPERTY ChainCreator PERSIS_PNTR ParticleChainCreator0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RandomFrac FLOAT 0.1
PROPERTY RandomTangents FLOAT 0.2
PROPERTY RemoveOnCloseDown BOOL 0
PROPERTY ScaleTangents FLOAT 3.0531
ENDPROPERTIES
ENDCLASS
BEGINCLASS ParticleChainCreator ParticleChainCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
PROPERTY NumTexturesForWholeChain INTEGER -1
ENDPROPERTIES
ENDCLASS
)";

/// The same rule without a creator
constexpr std::string_view k_ArcerNoCreator = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY InitiallyCreated ARRAY SIZE 25 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
ENDPROPERTIES
BEGINCLASS UR_ObjectArcer UR_ObjectArcer0
BEGINPROPERTIES
PROPERTY Group INTEGER 2
PROPERTY NextGroups ARRAY SIZE 1 3
ENDPROPERTIES
ENDCLASS
)";

std::shared_ptr<const File> Parse(std::string_view text, const char* name)
{
	auto file = File::Parse(text, name);
	return file.has_value() ? std::make_shared<const File>(*file) : nullptr;
}

/// Every synced draw's range, in order; each returns 0
void HookDraws(std::vector<uint32_t>& ranges)
{
	game_random::testing::SetGameRand(
	    [&ranges](uint32_t n) -> uint32_t {
		    ranges.push_back(n);
		    return 0;
	    },
	    nullptr);
}

glm::vec3 Vec(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

/// One vertex of a fake model: its position and normal, as float bits
struct VertexBits
{
	std::array<uint32_t, 3> position;
	std::array<uint32_t, 3> normal;
};
/// A draw the pick made: its range and what it gave
struct DrawBits
{
	int32_t range;
	int32_t value;
};
/// The original's pick, run on its own code with a fake 3D object (its matrix rows, then the translation) and a fake
/// mesh, its draws on the local stream: the ranges and values it drew, then the five points and their normals
using PrimitiveBits = std::vector<VertexBits>;
using SubMeshBits = std::vector<PrimitiveBits>;
struct PickCase
{
	std::array<uint32_t, 9> axes;
	std::array<uint32_t, 3> position;
	std::vector<SubMeshBits> subMeshes;
	std::vector<DrawBits> draws;
	std::array<uint32_t, 15> points;
	std::array<uint32_t, 15> normals;
};

/// Worked out by the original's code (the oracle), its draws on a fake local seed
const std::vector<PickCase> k_PickCases {
    PickCase {
        .axes = {0x3F4CCCCD, 0x00000000, 0xBF19999A, 0x00000000, 0x3F800000, 0x00000000, 0x3F19999A, 0x00000000, 0x3F4CCCCD},
        .position = {0x44E16CCD, 0x41DC0000, 0x45255800},
        .subMeshes =
            {SubMeshBits {
                 PrimitiveBits {VertexBits {{0x3F800000, 0x40000000, 0x40400000}, {0x00000000, 0x3F800000, 0x00000000}},
                                VertexBits {{0xBFC00000, 0x3E800000, 0x40800000}, {0x3F19999A, 0x00000000, 0x3F4CCCCD}},
                                VertexBits {{0x3DCCCCCD, 0x40E00000, 0xC0000000}, {0x00000000, 0x00000000, 0x3F800000}}}},
             SubMeshBits {
                 PrimitiveBits {VertexBits {{0x40400000, 0x40400000, 0x40400000}, {0x3F800000, 0x00000000, 0x00000000}}},
                 PrimitiveBits {VertexBits {{0x3F000000, 0xBF000000, 0x41100000}, {0x00000000, 0xBF800000, 0x00000000}},
                                VertexBits {{0x40000000, 0x3F800000, 0x00000000}, {0x3F350481, 0x3F350481, 0x00000000}}}}},
        .draws = {DrawBits {2, 0}, DrawBits {1, 0}, DrawBits {3, 2}, DrawBits {3, 0}, DrawBits {3, 1}, DrawBits {3, 2},
                  DrawBits {3, 0}},
        .points = {0x44E148F6, 0x420A0000, 0x45253D71, 0x44E1C000, 0x41EC0000, 0x452574CD, 0x44E19333, 0x41DE0000, 0x4525999A,
                   0x44E148F6, 0x420A0000, 0x45253D71, 0x44E1C000, 0x41EC0000, 0x452574CD},
        .normals = {0x3F10315A, 0x3C0CAD07, 0x3F538473, 0x3F10307D, 0x3C11D622, 0x3F5384D2, 0x3F103B56, 0x3C0CAF95, 0x3F537DA6,
                    0x3F10315A, 0x3C0CAD07, 0x3F538473, 0x3F10307D, 0x3C11D622, 0x3F5384D2}},
    PickCase {
        .axes = {0x3FD9999A, 0x3E4CCCCD, 0xBE99999A, 0xBDCCCCCD, 0x40066666, 0x3D4CCCCD, 0x3ECCCCCD, 0xBE4CCCCD, 0x3FF33333},
        .position = {0x00000000, 0x00000000, 0x00000000},
        .subMeshes = {SubMeshBits {
            PrimitiveBits {VertexBits {{0x3F800000, 0x40000000, 0x40400000}, {0x00000000, 0x3F800000, 0x00000000}},
                           VertexBits {{0xBF800000, 0xC0000000, 0xC0400000}, {0x00000000, 0x00000000, 0x00000000}},
                           VertexBits {{0x40B00000, 0x3F000000, 0xBF000000}, {0x3E99999A, 0x3E99999A, 0x3F666666}}}}},
        .draws = {DrawBits {1, 0}, DrawBits {1, 0}, DrawBits {3, 2}, DrawBits {3, 0}, DrawBits {3, 0}, DrawBits {3, 0},
                  DrawBits {3, 2}},
        .points = {0x4111999A, 0x40100000, 0xC024CCCD, 0x402CCCCD, 0x40733333, 0x40AFFFFF, 0x402CCCCD, 0x40733333, 0x40AFFFFF,
                   0x402CCCCD, 0x40733333, 0x40AFFFFF, 0x4111999A, 0x40100000, 0xC024CCCD},
        .normals = {0x3EE174DD, 0x3E88E261, 0x3F5B6ADE, 0xBD42C594, 0x3F7FA351, 0x3CC2C594, 0xBD42C594, 0x3F7FA351, 0x3CC2C594,
                    0xBD42C594, 0x3F7FA351, 0x3CC2C594, 0x3EE174DD, 0x3E88E261, 0x3F5B6ADE}},
    PickCase {
        .axes = {0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000},
        .position = {0x44B22000, 0x427B999A, 0x4511BE66},
        .subMeshes = {SubMeshBits {
            PrimitiveBits {VertexBits {{0x00000000, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0x00000000}},
                           VertexBits {{0x40000000, 0x40800000, 0x40C00000}, {0xBF000000, 0x3F000000, 0xBF350481}}}}},
        .draws = {DrawBits {1, 0}, DrawBits {1, 0}, DrawBits {2, 0}, DrawBits {2, 1}, DrawBits {2, 1}, DrawBits {2, 1},
                  DrawBits {2, 1}},
        .points = {0x44B22000, 0x427B999A, 0x4511BE66, 0x44B26000, 0x4285CCCD, 0x45121E66, 0x44B26000, 0x4285CCCD, 0x45121E66,
                   0x44B26000, 0x4285CCCD, 0x45121E66, 0x44B26000, 0x4285CCCD, 0x45121E66},
        .normals = {0x3F0573D4, 0x3CBC801B, 0x3F5A6267, 0x3F05727E, 0x3CBE0EE0, 0x3F5A62E3, 0x3F05727E, 0x3CBE0EE0, 0x3F5A62E3,
                    0x3F05727E, 0x3CBE0EE0, 0x3F5A62E3, 0x3F05727E, 0x3CBE0EE0, 0x3F5A62E3}},
};

explode_object::SourceMesh MeshOf(const PickCase& c)
{
	explode_object::SourceMesh mesh;
	for (const auto& subMesh : c.subMeshes)
	{
		auto& made = mesh.subMeshes.emplace_back();
		for (const auto& primitive : subMesh)
		{
			auto& prim = made.primitives.emplace_back();
			for (const auto& vertex : primitive)
			{
				prim.positions.push_back(Vec(vertex.position));
				prim.normals.push_back(Vec(vertex.normal));
				prim.uvs.emplace_back(0.0f);
			}
		}
	}
	return mesh;
}

/// The queue starts empty and ends empty in every test
class ObjectArcer: public ::testing::Test
{
protected:
	void SetUp() override { object_arcer::Clear(); }
	void TearDown() override { object_arcer::Clear(); }
};

} // namespace

TEST_F(ObjectArcer, queuesAStillModelWhileFewerThanAHundredArcsLive)
{
	using object_arcer::Queues;
	EXPECT_TRUE(Queues(true, false, 0));
	EXPECT_TRUE(Queues(true, false, 99));
	EXPECT_FALSE(Queues(true, false, 100));
	// Nothing without a model, nor for an animated one
	EXPECT_FALSE(Queues(false, false, 0));
	EXPECT_FALSE(Queues(true, true, 0));
}

TEST_F(ObjectArcer, picksFivePointsOfOnePrimitiveAsTheOriginal)
{
	for (const auto& c : k_PickCases)
	{
		glm::mat3 axes(1.0f);
		for (int column = 0; column < 3; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				axes[column][row] = std::bit_cast<float>(c.axes[static_cast<size_t>(column * 3 + row)]);
			}
		}
		size_t next = 0;
		const auto picked = object_arcer::PickPoints(MeshOf(c), axes, Vec(c.position), [&c, &next](int32_t n) {
			EXPECT_LT(next, c.draws.size());
			if (next >= c.draws.size())
			{
				return 0;
			}
			const auto draw = c.draws[next++];
			EXPECT_EQ(n, draw.range);
			return draw.value;
		});
		EXPECT_EQ(next, c.draws.size());
		ASSERT_EQ(picked.points.size(), object_arcer::k_ArcPoints);
		ASSERT_EQ(picked.normals.size(), object_arcer::k_ArcPoints);
		for (size_t i = 0; i < object_arcer::k_ArcPoints; ++i)
		{
			for (int k = 0; k < 3; ++k)
			{
				const auto index = i * 3 + static_cast<size_t>(k);
				EXPECT_EQ(std::bit_cast<uint32_t>(picked.points[i][k]), c.points[index]) << i << " " << k;
				EXPECT_EQ(std::bit_cast<uint32_t>(picked.normals[i][k]), c.normals[index]) << i << " " << k;
			}
		}
	}
}

TEST_F(ObjectArcer, aModelWithNoVertexGivesNoPointAndNoDraw)
{
	explode_object::SourceMesh mesh;
	mesh.subMeshes.emplace_back().primitives.emplace_back();
	int draws = 0;
	const auto picked = object_arcer::PickPoints(mesh, glm::mat3(1.0f), glm::vec3(0.0f), [&draws](int32_t /*n*/) {
		++draws;
		return 0;
	});
	EXPECT_TRUE(picked.points.empty());
	EXPECT_EQ(draws, 0);
}

// The always-on effect lives on: with nothing queued the rule makes no atom and draws nothing, and stays attached
TEST_F(ObjectArcer, emptyQueueMakesNothingAndTheEffectLivesOn)
{
	const game_random::testing::ScopedState random;
	std::vector<uint32_t> ranges;
	HookDraws(ranges);
	const auto file = Parse(k_Arcer, "SF_OnFireTest");
	ASSERT_NE(file, nullptr);
	Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Synced);
	for (int i = 0; i < 10; ++i)
	{
		effect.Step(0.1f);
	}
	EXPECT_EQ(effect.AtomCount(), 0u);
	EXPECT_TRUE(ranges.empty());
	EXPECT_FALSE(effect.Finished());
	EXPECT_EQ(object_arcer::LiveArcs(), 0u);
}

TEST_F(ObjectArcer, noCreatorDetaches)
{
	const game_random::testing::ScopedState random;
	std::vector<uint32_t> ranges;
	HookDraws(ranges);
	const auto file = Parse(k_ArcerNoCreator, "SF_OnFireNoCreatorTest");
	ASSERT_NE(file, nullptr);
	Effect effect(file, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Synced);
	effect.Step(0.1f);
	EXPECT_EQ(effect.AtomCount(), 0u);
	EXPECT_TRUE(ranges.empty());
	EXPECT_TRUE(effect.Finished());
}

TEST_F(ObjectArcer, anObjectWithoutAModelIsNotQueued)
{
	object_arcer::Queue(entt::null);
	EXPECT_EQ(object_arcer::QueuedCount(), 0u);
}
