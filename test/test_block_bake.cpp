/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The prints step of a block's texture (3D/BlockBake.h) with fake objects: its order (the objects' shadows and
// footprints, the land's footprints, the snow, the shadows' darkening last) and which objects and land footprints take
// part (docs/wiki/world/terrain.md#the-prints-the-16th-step).

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/BlockBake.h"

using namespace openblack::block_texture;

namespace
{
constexpr int32_t k_Size = 256;

std::array<float, 12> At(float x, float z, float y = 0.0f)
{
	return {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, x, y, z};
}

// a picture of one opaque white texel over a square of `half` around the footprint's centre
struct Square
{
	explicit Square(float half)
	    : triangles {{.corners = {glm::vec2 {-half, -half}, glm::vec2 {half, -half}, glm::vec2 {half, half}},
	                  .texels = {glm::vec2 {0.0f}, glm::vec2 {0.0f}, glm::vec2 {0.0f}}},
	                 {.corners = {glm::vec2 {-half, -half}, glm::vec2 {half, half}, glm::vec2 {-half, half}},
	                  .texels = {glm::vec2 {0.0f}, glm::vec2 {0.0f}, glm::vec2 {0.0f}}}}
	{
	}
	std::array<uint16_t, 1> white {0xFFFF};
	std::vector<PrintTriangle> triangles;
	[[nodiscard]] PrintPicture Picture() const { return {.width = 1, .height = 1, .texels = white, .triangles = triangles}; }
};

size_t TexelOf(float x, float z)
{
	return static_cast<size_t>(x * 1.6f) * k_Size + static_cast<size_t>(z * 1.6f);
}
} // namespace

TEST(BlockBake, AnObjectThatNeitherCastsNorPrintsIsLeftOut)
{
	const Square square(20.0f);
	std::vector<uint16_t> texels(k_Size * k_Size, 0x5000);
	BakeScratch scratch(k_Size);
	const std::array<BakeObject, 1> objects {{{.placement = At(80.0f, 80.0f), .footprint = square.Picture()}}};
	BakePrints(texels, {0, 0}, {0.0f, 0.0f}, {.objects = objects}, scratch);
	EXPECT_EQ(texels[TexelOf(80.0f, 80.0f)], 0x5000);
}

TEST(BlockBake, AShadowCasterPrintsItsMeshsFootprintToo)
{
	const Square square(20.0f);
	std::vector<uint16_t> texels(k_Size * k_Size, 0x5000);
	BakeScratch scratch(k_Size);
	const std::array<BakeObject, 1> objects {
	    {{.placement = At(80.0f, 80.0f), .castsShadow = true, .footprint = square.Picture()}}};
	BakePrints(texels, {0, 0}, {0.0f, 0.0f}, {.objects = objects}, scratch);
	EXPECT_EQ(texels[TexelOf(80.0f, 80.0f)], 0x5EEE);
}

TEST(BlockBake, TheShadowsDarkenWhatThePrintsLeft)
{
	// a flat square on the ground under the footprint: its shadow is itself, and darkens the printed white by half
	const Square square(20.0f);
	const std::array<glm::vec3, 4> corners {glm::vec3 {-30.0f, 0.0f, -30.0f}, glm::vec3 {30.0f, 0.0f, -30.0f},
	                                        glm::vec3 {30.0f, 0.0f, 30.0f}, glm::vec3 {-30.0f, 0.0f, 30.0f}};
	const std::array<std::array<uint16_t, 3>, 2> faces {{{0, 1, 2}, {0, 2, 3}}};
	const std::array<ShadowPart, 1> parts {{{.vertices = corners, .triangles = faces, .doubleSided = true}}};
	std::vector<uint16_t> texels(k_Size * k_Size, 0x5000);
	BakeScratch scratch(k_Size);
	const std::array<BakeObject, 1> objects {
	    {{.placement = At(80.0f, 80.0f), .castsShadow = true, .shadow = parts, .footprint = square.Picture()}}};
	BakePrints(texels, {0, 0}, {0.0f, 0.0f}, {.objects = objects}, scratch);
	// 0xE x 128 / 256 = 7 a nibble, the alpha kept
	EXPECT_EQ(texels[TexelOf(80.0f, 80.0f)], 0x5777);
	// outside the shadow and the print: as painted
	EXPECT_EQ(texels[TexelOf(10.0f, 150.0f)], 0x5000);
}

TEST(BlockBake, OnlyTheLandsFootprintsOverTheBlockPrint)
{
	const Square square(20.0f);
	std::vector<uint16_t> texels(k_Size * k_Size, 0x5000);
	BakeScratch scratch(k_Size);
	// the block at (1, 2): its square from (160, 320) to (320, 480)
	const std::array<LandPrint, 2> prints {{
	    {.picture = square.Picture(), .placement = At(240.0f, 400.0f), .box = {220.0f, 380.0f, 260.0f, 420.0f}},
	    {.picture = square.Picture(), .placement = At(240.0f, 400.0f), .box = {330.0f, 380.0f, 360.0f, 420.0f}},
	}};
	BakePrints(texels, {1, 2}, {160.0f, 320.0f}, {.landPrints = std::span(prints).first(1)}, scratch);
	EXPECT_EQ(texels[TexelOf(80.0f, 80.0f)], 0x5EEE);
	std::ranges::fill(texels, uint16_t {0x5000});
	BakePrints(texels, {1, 2}, {160.0f, 320.0f}, {.landPrints = std::span(prints).last(1)}, scratch);
	EXPECT_EQ(texels[TexelOf(80.0f, 80.0f)], 0x5000);
}
