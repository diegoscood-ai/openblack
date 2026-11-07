/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <bit>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/TempleExteriorMorph.h"

using namespace openblack;
using namespace openblack::TempleExteriorMorph;

namespace
{
void ExpectPlace(const Place& place, const Place& expected)
{
	EXPECT_EQ(place.sizeLow, expected.sizeLow);
	EXPECT_EQ(place.sizeHigh, expected.sizeHigh);
	EXPECT_FLOAT_EQ(place.sizeWeight, expected.sizeWeight);
	EXPECT_EQ(place.stageLow, expected.stageLow);
	EXPECT_EQ(place.stageHigh, expected.stageHigh);
	EXPECT_FLOAT_EQ(place.stageWeight, expected.stageWeight);
}

/// A vertex from its eight values' bits: position, texture coordinates, normal
l3d::L3DVertex VertexOfBits(const std::array<uint32_t, 8>& bits)
{
	const auto f = [&bits](size_t i) { return std::bit_cast<float>(bits.at(i)); };
	return {.position = {f(0), f(1), f(2)}, .texCoord = {f(3), f(4)}, .normal = {f(5), f(6), f(7)}};
}

std::array<uint32_t, 8> BitsOf(const l3d::L3DVertex& v)
{
	return {std::bit_cast<uint32_t>(v.position.x), std::bit_cast<uint32_t>(v.position.y), std::bit_cast<uint32_t>(v.position.z),
	        std::bit_cast<uint32_t>(v.texCoord.x), std::bit_cast<uint32_t>(v.texCoord.y), std::bit_cast<uint32_t>(v.normal.x),
	        std::bit_cast<uint32_t>(v.normal.y),   std::bit_cast<uint32_t>(v.normal.z)};
}

/// Fifteen fake temple meshes of two vertices each: every value of a vertex tells its mesh and its place
std::array<std::vector<l3d::L3DVertex>, k_Sizes * k_Stages> FakeTemples()
{
	std::array<std::vector<l3d::L3DVertex>, k_Sizes * k_Stages> meshes;
	for (uint32_t size = 0; size < k_Sizes; ++size)
	{
		for (uint32_t stage = 0; stage < k_Stages; ++stage)
		{
			auto& mesh = meshes.at((size * k_Stages) + stage);
			for (uint32_t i = 0; i < 2; ++i)
			{
				const auto s = static_cast<float>(size);
				const auto t = static_cast<float>(stage);
				const auto k = static_cast<float>(i);
				mesh.push_back({
				    .position = {s, t, k},
				    .texCoord = {t, s},
				    .normal = {k, s, t},
				});
			}
		}
	}
	return meshes;
}
} // namespace

TEST(TempleExteriorMorph, AlignmentTargetIsThePlayersFromZeroToJustShortOfOne)
{
	EXPECT_FLOAT_EQ(AlignmentTarget(0.0f), 0.5f);
	EXPECT_FLOAT_EQ(AlignmentTarget(-1.0f), 0.0f);
	EXPECT_FLOAT_EQ(AlignmentTarget(0.5f), 0.75f);
	// From 1 up it stops just short of 1, below -1 at 0
	EXPECT_EQ(AlignmentTarget(1.0f), 0.9999f);
	EXPECT_EQ(AlignmentTarget(1.5f), 0.9999f);
	EXPECT_EQ(AlignmentTarget(-1.5f), 0.0f);
}

TEST(TempleExteriorMorph, SizeTargetIsTwiceTheShareOfInfluence)
{
	// The first land always gives the small share, whatever the powers
	EXPECT_FLOAT_EQ(SizeTarget(true, 0.0f, 0.0f), 0.02f);
	EXPECT_FLOAT_EQ(SizeTarget(true, 300.0f, 400.0f), 0.02f);
	// Elsewhere the player's power over everyone's, and the small share when nobody has any
	EXPECT_FLOAT_EQ(SizeTarget(false, 30.0f, 100.0f), 0.6f);
	EXPECT_FLOAT_EQ(SizeTarget(false, 0.0f, 100.0f), 0.0f);
	EXPECT_FLOAT_EQ(SizeTarget(false, 0.0f, 0.0f), 0.02f);
	// More than half of the influence is the largest temple
	EXPECT_EQ(SizeTarget(false, 80.0f, 100.0f), 1.0f);
}

TEST(TempleExteriorMorph, ANewTempleIsNeutralSmallAndBlendedOnceAtOnce)
{
	State state;
	EXPECT_EQ(state.alignment, 0.5f);
	EXPECT_EQ(state.alignmentTarget, 0.5f);
	EXPECT_EQ(state.size, 0.0f);
	EXPECT_EQ(state.sizeTarget, 0.0f);
	// Its first blend is due though it has not moved; after it, none until it does
	EXPECT_TRUE(NeedsBlend(state));
	Blended(state);
	EXPECT_FALSE(NeedsBlend(state));
	EXPECT_FALSE(Turn(state, 0.5f, 0.0f));
	EXPECT_FALSE(NeedsBlend(state));
}

TEST(TempleExteriorMorph, ATurnStepsTowardTheTargetsAndBlendsPastThreeHundredths)
{
	State state;
	Blended(state);
	// 0.5 -> 0.516: not yet far enough from the 0.5 blended
	EXPECT_FALSE(Turn(state, 0.9999f, 0.02f));
	EXPECT_FLOAT_EQ(state.alignment, 0.516f);
	EXPECT_FLOAT_EQ(state.size, 0.016f);
	EXPECT_EQ(state.alignmentTarget, 0.9999f);
	EXPECT_FLOAT_EQ(state.sizeTarget, 0.02f);
	EXPECT_FALSE(NeedsBlend(state));
	// 0.532 is: the turn says so, and the blend keeps that look
	EXPECT_TRUE(Turn(state, 0.9999f, 0.02f));
	EXPECT_FLOAT_EQ(state.alignment, 0.532f);
	EXPECT_EQ(state.size, 0.02f); // the rest of the way, less than a step
	EXPECT_TRUE(NeedsBlend(state));
	Blended(state);
	EXPECT_EQ(state.blendedAlignment, state.alignment);
	EXPECT_EQ(state.blendedSize, state.size);
	EXPECT_FALSE(NeedsBlend(state));
	// The size alone, 0.02 on the first land, never moves far enough to blend
	State small;
	Blended(small);
	for (int turn = 0; turn < 10; ++turn)
	{
		EXPECT_FALSE(Turn(small, 0.5f, 0.02f));
	}
	EXPECT_EQ(small.size, 0.02f);
}

TEST(TempleExteriorMorph, ATurnBlendsWhenTheSizeMovesFarEnough)
{
	State state;
	Blended(state);
	EXPECT_FALSE(Turn(state, 0.5f, 1.0f)); // 0.016
	EXPECT_TRUE(Turn(state, 0.5f, 1.0f));  // 0.032
	Blended(state);
	// Back down: the same steps the other way
	EXPECT_FALSE(Turn(state, 0.5f, 0.0f));
	EXPECT_TRUE(Turn(state, 0.5f, 0.0f));
	EXPECT_EQ(state.size, 0.0f);
}

TEST(TempleExteriorMorph, VerticesAreTheCornersMeshesByTheirWeights)
{
	const auto meshes = FakeTemples();
	const VerticesOf verticesOf = [&meshes](uint32_t size, uint32_t stage) { return &meshes.at((size * k_Stages) + stage); };
	std::vector<l3d::L3DVertex> blended(2);
	// Half of sizes 0 and 1 and half of stages 2 and 3: every value the mean of the four
	ASSERT_TRUE(BlendVertices(PlaceOf(0.25f, 0.625f), verticesOf, blended));
	EXPECT_FLOAT_EQ(blended[1].position.x, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].position.y, 2.5f);
	EXPECT_FLOAT_EQ(blended[1].position.z, 1.0f);
	EXPECT_FLOAT_EQ(blended[1].texCoord.x, 2.5f);
	EXPECT_FLOAT_EQ(blended[1].texCoord.y, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].normal.x, 1.0f);
	EXPECT_FLOAT_EQ(blended[1].normal.y, 0.5f);
	EXPECT_FLOAT_EQ(blended[1].normal.z, 2.5f);
	// The start, small and neutral, is the middle stage's mesh alone
	ASSERT_TRUE(BlendVertices(PlaceOf(0.0f, 0.5f), verticesOf, blended));
	EXPECT_FLOAT_EQ(blended[0].position.x, 0.0f);
	EXPECT_FLOAT_EQ(blended[0].position.y, 2.0f);
	EXPECT_FLOAT_EQ(blended[0].position.z, 0.0f);
}

TEST(TempleExteriorMorph, AMissingOrMisshapenMeshOfAnyWeightFailsTheBlend)
{
	const auto meshes = FakeTemples();
	std::vector<l3d::L3DVertex> blended(2);
	// Every mesh of the place is read, even one with no share: the small neutral temple reads sizes 0 and 1 of stages
	// 2 and 3
	const VerticesOf onlyTheMiddle = [&meshes](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
		return size == 0 && stage == 2 ? &meshes.at(2) : nullptr;
	};
	EXPECT_FALSE(BlendVertices(PlaceOf(0.0f, 0.5f), onlyTheMiddle, blended));
	EXPECT_FALSE(BlendVertices(PlaceOf(0.25f, 0.625f), onlyTheMiddle, blended));
	const VerticesOf allButTheUpperStage = [&meshes](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
		return stage == 3 ? nullptr : &meshes.at((size * k_Stages) + stage);
	};
	EXPECT_FALSE(BlendVertices(PlaceOf(0.0f, 0.5f), allButTheUpperStage, blended));
	// The last stage has no stage above it to read
	EXPECT_TRUE(BlendVertices(PlaceOf(0.0f, 1.0f), allButTheUpperStage, blended));
	const std::vector<l3d::L3DVertex> three(3);
	const VerticesOf misshapen = [&three](uint32_t, uint32_t) { return &three; };
	EXPECT_FALSE(BlendVertices(PlaceOf(0.0f, 0.5f), misshapen, blended));
}

TEST(TempleExteriorMorph, BlendedVerticesAreLaidOnTheLand)
{
	// A land rising 0.5 a metre along x, and a temple standing at a height of 4, turned a quarter about y
	const land_morph::Ground ground = [](glm::vec2 xz) { return 0.5f * xz.x; };
	const auto object = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 4.0f, 200.0f)), glm::radians(90.0f),
	                                glm::vec3(0.0f, 1.0f, 0.0f));
	std::vector<l3d::L3DVertex> vertices {
	    {.position = {0.0f, 1.0f, 0.0f}, .texCoord = {0.25f, 0.75f}, .normal = {0.0f, 1.0f, 0.0f}},
	    {.position = {0.0f, 2.0f, 10.0f}, .texCoord = {0.5f, 0.5f}, .normal = {1.0f, 0.0f, 0.0f}},
	};
	const auto original = vertices;
	BakeToLand(ground, object, vertices);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		// y - (the temple's height - the land's under the vertex), no scale; nothing else moves
		const auto& p = original[i].position;
		const auto world = glm::vec3(object * glm::vec4(p.x, p.y, p.z, 1.0f));
		EXPECT_FLOAT_EQ(vertices[i].position.y, p.y - (4.0f - ground(glm::vec2(world.x, world.z))));
		EXPECT_EQ(vertices[i].position.x, p.x);
		EXPECT_EQ(vertices[i].position.z, p.z);
		EXPECT_EQ(vertices[i].texCoord.x, original[i].texCoord.x);
		EXPECT_EQ(vertices[i].normal.x, original[i].normal.x);
	}
	// The vertex at the temple's foot, on land at 50, goes 46 up
	EXPECT_FLOAT_EQ(vertices[0].position.y, 1.0f + 46.0f);
	// The one 10 m off along its own z is turned onto x, where the land is 5 higher or lower
	EXPECT_NEAR(std::abs(vertices[1].position.y - original[1].position.y - 46.0f), 5.0f, 1e-3f);
}

TEST(TempleExteriorMorph, StepsTowardWhereItIsToBe)
{
	EXPECT_FLOAT_EQ(Step(0.5f, 1.0f), 0.516f);
	EXPECT_FLOAT_EQ(Step(0.5f, 0.0f), 0.484f);
	EXPECT_FLOAT_EQ(Step(0.99f, 1.0f), 1.0f);
	// Near enough, it is there
	EXPECT_FLOAT_EQ(Step(0.9995f, 1.0f), 1.0f);
}

TEST(TempleExteriorMorph, ASmallNeutralTempleIsTheMiddleStage)
{
	// All of size 0 and all of stage 2
	ExpectPlace(PlaceOf(0.0f, 0.5f), {0, 1, 1.0f, 2, 3, 1.0f});
}

TEST(TempleExteriorMorph, BetweenStagesAndSizesItBlendsTheFour)
{
	// Half of size 0 and 1, and half of stage 2 and 3
	ExpectPlace(PlaceOf(0.25f, 0.625f), {0, 1, 0.5f, 2, 3, 0.5f});
}

TEST(TempleExteriorMorph, TheEndsAreTheirOwnMeshes)
{
	ExpectPlace(PlaceOf(0.0f, 0.0f), {0, 1, 1.0f, 0, 1, 1.0f});
	ExpectPlace(PlaceOf(1.0f, 1.0f), {1, 2, 0.0f, 4, 4, 0.0f});
	// The largest and best is that mesh exactly
	const auto meshes = FakeTemples();
	const VerticesOf verticesOf = [&meshes](uint32_t size, uint32_t stage) { return &meshes.at((size * k_Stages) + stage); };
	std::vector<l3d::L3DVertex> blended(2);
	ASSERT_TRUE(BlendVertices(PlaceOf(1.0f, 1.0f), verticesOf, blended));
	EXPECT_EQ(BitsOf(blended[1]), BitsOf(meshes.at((2 * k_Stages) + 4)[1]));
}

TEST(TempleExteriorMorph, TheNestedBlendMatchesTheOriginalsBits)
{
	// Each case: the size and the alignment, the place's two weights' bits, the four meshes' one vertex (the lower
	// stage's small and large, the upper stage's small and large) and the blend, from the original's vertex blend run
	// three times as the temple runs it
	struct Case
	{
		float size;
		float alignment;
		uint32_t sizeWeight;
		uint32_t stageWeight;
		std::array<std::array<uint32_t, 8>, 4> meshes;
		std::array<uint32_t, 8> blended;
	};
	const std::array<Case, 3> cases {{
	    {0.3f,
	     0.37f,
	     0x3ECCCCCC,
	     0x3F051EB8,
	     {{{0x4217E6FB, 0xC038D1E5, 0x4207C98F, 0xC05FF2A5, 0x4218533B, 0x42372707, 0xC0E7F328, 0x4206E918},
	       {0x42171365, 0xC1C425F3, 0xC2348A09, 0xC216E6C9, 0xC0F587C8, 0xC236A7D8, 0x4064DB7F, 0x421E1F46},
	       {0xBFE34F6F, 0x42188327, 0x419622CE, 0x418766BC, 0xC194770C, 0x41C28D77, 0x4214C580, 0xC2248F00},
	       {0x4243D5E3, 0xC1DD715E, 0xC225AC76, 0x42236E14, 0x41D352A7, 0x420CE8F2, 0xC0CCC8DF, 0xC24E074E}}},
	     {0x4205C4A0, 0xC10E6DC4, 0xC1758AB0, 0x40212E30, 0x411924A6, 0x4121770C, 0x409CFDC0, 0xC0589350}},
	    {0.517f,
	     0.8123f,
	     0x3F774BC6,
	     0x3F40346C,
	     {{{0xC1492223, 0xC2591649, 0xC1CD4FD4, 0xC25DC339, 0x3F4E47AA, 0xC245EB62, 0x424FC19E, 0x41BF696C},
	       {0xC1AFB184, 0x42559FE0, 0xC25035F4, 0xC1E8DE47, 0xC24CF5CE, 0xC10D7EEE, 0xC20F16ED, 0xC146703F},
	       {0x41C4AA5F, 0x4239EDFB, 0x3D7E81A9, 0x4218E528, 0xC1853217, 0x422C2B5B, 0x3FE6ECC9, 0x41C251A1},
	       {0xC21C261A, 0x4122433F, 0xC1BE4B45, 0x42162B2E, 0x4083DE54, 0xBD4AD926, 0x42033233, 0x40BC471D}}},
	     {0xC082E7F2, 0xC1D5C8B8, 0xC1A107A1, 0xC1FB76E4, 0xC0964B1C, 0xC1CE0240, 0x4215E047, 0x41B77839}},
	    {0.75f,
	     0.1111f,
	     0x3F000000,
	     0x3F0E3BCD,
	     {{{0xC19F97A1, 0xC23126F8, 0x416F0D2F, 0x424C2A8B, 0x42245299, 0xC24E8525, 0xC1A7B9E8, 0xC26F2E52},
	       {0x41A7B595, 0x4184BFB9, 0x41F797A9, 0xC228D419, 0xC207D2E6, 0xC10113B8, 0x41E34180, 0xC20C9538},
	       {0x421AF634, 0xC164180B, 0x4233A83D, 0x425CCFD5, 0x408FF3EE, 0x424A1CC2, 0xC115B071, 0x41B1DBFA},
	       {0x42268429, 0x42229BC9, 0xC2436C47, 0xC1E618CC, 0xC12C4DFA, 0x422BC175, 0xC1D653C2, 0xC23967C3}}},
	     {0x41911EA5, 0xBFEA049C, 0x413DF54E, 0x4105450E, 0x3F15FFE4, 0x4085E704, 0xC0BEDDD6, 0xC1FDDEDE}},
	}};
	for (const auto& c : cases)
	{
		const auto place = PlaceOf(c.size, c.alignment);
		EXPECT_EQ(std::bit_cast<uint32_t>(place.sizeWeight), c.sizeWeight) << c.size;
		EXPECT_EQ(std::bit_cast<uint32_t>(place.stageWeight), c.stageWeight) << c.alignment;
		std::array<std::vector<l3d::L3DVertex>, 4> meshes;
		for (size_t m = 0; m < meshes.size(); ++m)
		{
			meshes.at(m) = {VertexOfBits(c.meshes.at(m))};
		}
		const VerticesOf verticesOf = [&place, &meshes](uint32_t size, uint32_t stage) -> const std::vector<l3d::L3DVertex>* {
			const size_t upper = stage == place.stageHigh && stage != place.stageLow ? 2 : 0;
			const size_t large = size == place.sizeHigh ? 1 : 0;
			return &meshes.at(upper + large);
		};
		std::vector<l3d::L3DVertex> blended(1);
		ASSERT_TRUE(BlendVertices(place, verticesOf, blended));
		EXPECT_EQ(BitsOf(blended[0]), c.blended) << c.size << ", " << c.alignment;
	}
}

TEST(TempleExteriorMorph, MeshNames)
{
	EXPECT_EQ(MeshName(1, 4), "b_temple14_l3d");
}

TEST(TempleExteriorMorph, TexturesGoEvilToNeutralToGood)
{
	const auto evil = TextureOf(0.0f);
	EXPECT_EQ(evil.from, Look::Evil);
	EXPECT_EQ(evil.to, Look::Neutral);
	EXPECT_EQ(evil.weight, 0);
	EXPECT_EQ(TextureOf(0.25f).weight, 127);
	EXPECT_EQ(TextureOf(0.5f).to, Look::Neutral);
	EXPECT_EQ(TextureOf(0.5f).weight, 255);
	const auto good = TextureOf(1.0f);
	EXPECT_EQ(good.from, Look::Neutral);
	EXPECT_EQ(good.to, Look::Good);
	EXPECT_EQ(good.weight, 255);
	EXPECT_EQ(ImageName(Look::Good, 3), "good3");
}

TEST(TempleExteriorMorph, TexelsBlendByChannelKeepingTheFirstsAlpha)
{
	const std::vector<uint16_t> from {0xF000, 0x1FFF};
	const std::vector<uint16_t> to {0x0FFF, 0x0000};
	std::vector<uint16_t> blended(2);
	BlendTexels(from, to, 255, blended);
	EXPECT_EQ(blended[0], 0xFFFF);
	EXPECT_EQ(blended[1], 0x1000);
	BlendTexels(from, to, 0, blended);
	EXPECT_EQ(blended, from);
	// Halfway, 15 by 128 over 255 is 7 in each
	BlendTexels(from, to, 128, blended);
	EXPECT_EQ(blended[0], 0xF777);
}
