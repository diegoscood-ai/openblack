/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land's texture detail line (ECS/DrawList/TextureDetail.h), pinned to the original's functions run natively by
// the exe oracle: the line's rebuild in the plane at the land's height (18 cases), its place at the land's creation,
// a block's texture level (14 cases) and the tile's alpha at a vertex (ties rounded to even).

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/DrawList/TextureDetail.h"

using namespace openblack::ecs::draw_list;

namespace
{
float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t B(float value)
{
	return std::bit_cast<uint32_t>(value);
}

struct RebuildCase
{
	const char* name;
	std::array<uint32_t, 3> eye;
	std::array<uint32_t, 3> view;
	std::array<uint32_t, 3> along;
	std::array<uint32_t, 3> point;
};
} // namespace

TEST(TextureDetail, TheLineIsRebuiltInThePlaneAtTheLandsHeight)
{
	// the line 800 ahead with its band of 170 (85 each side); its point (11, 22, 33) before each rebuild, kept when the
	// view is too steep
	constexpr std::array<uint32_t, 3> k_Kept {0x41300000, 0x41B00000, 0x42040000};
	const std::array<RebuildCase, 18> cases {{
	    {"y50 +z",
	     {0x447A0000, 0x42480000, 0x44FA0000},
	     {0x00000000, 0xBF19999A, 0x3F4CCCCD},
	     {0x3F800000, 0x00000000, 0x80000000},
	     {0x00000000, 0x42480000, 0x453B8000}},
	    {"y300 +z",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x00000000, 0xBF19999A, 0x3F4CCCCD},
	     {0x3F800000, 0x00000000, 0x80000000},
	     {0x00000000, 0x42DD199A, 0x45329E9A}},
	    {"y = 110.55",
	     {0x447A0000, 0x42DD199A, 0x44FA0000},
	     {0x3EA89910, 0xBF0C7F8D, 0x3F44B293},
	     {0x3F6B4D17, 0x00000000, 0xBEC9AFCA},
	     {0x00000000, 0x42DD199B, 0x4558DC39}},
	    {"y50 -z",
	     {0x447A0000, 0x42480000, 0x44FA0000},
	     {0x00000000, 0xBF19999A, 0xBF4CCCCD},
	     {0xBF800000, 0x00000000, 0x80000000},
	     {0x80000000, 0x42480000, 0x447A0000}},
	    {"y300 -z",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x00000000, 0xBF19999A, 0xBF4CCCCD},
	     {0xBF800000, 0x00000000, 0x80000000},
	     {0x80000000, 0x42DD199A, 0x448EC2CD}},
	    {"y50 +x",
	     {0x447A0000, 0x42480000, 0x44FA0000},
	     {0x3F4CCCCD, 0xBF19999A, 0x00000000},
	     {0x00000000, 0x00000000, 0xBF800000},
	     {0x44FA0000, 0x42480000, 0x80000000}},
	    {"y300 +x",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x3F4CCCCD, 0xBF19999A, 0x00000000},
	     {0x00000000, 0x00000000, 0xBF800000},
	     {0x44E83D33, 0x42DD199A, 0x80000000}},
	    {"y300 -x",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0xBF4CCCCD, 0xBF19999A, 0x00000000},
	     {0x00000000, 0x80000000, 0x3F800000},
	     {0x430E1664, 0x42DD199A, 0x00000000}},
	    {"y300 45 degrees",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x3F000000, 0xBF3504F3, 0x3F000000},
	     {0x3F3504F3, 0x00000000, 0xBF3504F3},
	     {0x00000000, 0x42DD199A, 0x4587609E}},
	    {"y50 arbitrary",
	     {0x449A5000, 0x42480000, 0x445B1000},
	     {0x3E9A67AE, 0xBECDDF93, 0x3F5D5058},
	     {0x3F71B71A, 0x00000000, 0xBEA8A377},
	     {0x00000000, 0x42480001, 0x450B845B}},
	    {"y300 arbitrary",
	     {0x45129C00, 0x43960000, 0x45580800},
	     {0xBF60ED54, 0xBED1164E, 0x3E7D705F},
	     {0x3E8AD29F, 0x80000000, 0x3F76690D},
	     {0x440967FF, 0x42DD199A, 0x00000000}},
	    {"y300 arbitrary 2",
	     {0x43FA0000, 0x43960000, 0x457A0000},
	     {0x3F670C3F, 0xBE4D6038, 0xBEC31B69},
	     {0xBEC72758, 0x00000000, 0xBF6BD703},
	     {0xC3AC4BDC, 0x42DD199A, 0x80000000}},
	    {"y80 arbitrary",
	     {0x453B8000, 0x42A00000, 0x453B8000},
	     {0xBE198DCF, 0xBF6654B7, 0xBED1DB68},
	     {0xBF706A5A, 0x00000000, 0x3EAFE9EA},
	     {0x80000000, 0x42A00000, 0x45061C19}},
	    {"y-20 arbitrary",
	     {0x44BB8000, 0xC1A00000, 0x44BB8000},
	     {0x3E4D5302, 0xBE99FE41, 0x3F6EB07F},
	     {0x3F7A4728, 0x00000000, 0xBE574AF9},
	     {0x00000000, 0xC19FFFFF, 0x45278997}},
	    {"near vertical",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x38D1B717, 0xBF800000, 0x00000000},
	     {0x00000000, 0x00000000, 0xB8D1B717},
	     k_Kept},
	    {"both tiny",
	     {0x447A0000, 0x43960000, 0x44FA0000},
	     {0x3B449B65, 0xBF7FFFAC, 0x3A831244},
	     {0x3A831244, 0x00000000, 0xBB449B65},
	     k_Kept},
	    {"NaN y +z",
	     {0x447A0000, 0x7FC00000, 0x44FA0000},
	     {0x00000000, 0xBF19999A, 0x3F4CCCCD},
	     {0x3F800000, 0x00000000, 0x80000000},
	     {0x00000000, 0x42DD199A, 0xFFC00000}},
	    {"NaN y +x",
	     {0x447A0000, 0x7FC00000, 0x44FA0000},
	     {0x3F4CCCCD, 0xBF19999A, 0x00000000},
	     {0x00000000, 0x00000000, 0xBF800000},
	     {0xFFC00000, 0x42DD199A, 0x80000000}},
	}};
	for (const auto& c : cases)
	{
		const LodLine before {.along = {1.0f, 0.0f, 0.0f},
		                      .point = {F(k_Kept[0]), F(k_Kept[1]), F(k_Kept[2])},
		                      .distance = 800.0f,
		                      .width = 170.0f,
		                      .halfWidth = 85.0f};
		const auto line = RebuildTextureLine(before, glm::vec3(F(c.eye[0]), F(c.eye[1]), F(c.eye[2])),
		                                     glm::vec3(F(c.view[0]), F(c.view[1]), F(c.view[2])));
		for (int i = 0; i < 3; ++i)
		{
			const bool nanExpected = std::isnan(F(c.point[i]));
			if (nanExpected)
			{
				EXPECT_TRUE(std::isnan(line.point[i])) << c.name << " point " << i;
			}
			else
			{
				EXPECT_EQ(B(line.point[i]), c.point[i]) << c.name << " point " << i;
			}
			EXPECT_EQ(B(line.along[i]), c.along[i]) << c.name << " along " << i;
		}
		EXPECT_EQ(line.distance, 800.0f) << c.name;
		EXPECT_EQ(line.width, 170.0f) << c.name;
		EXPECT_EQ(line.halfWidth, 85.0f) << c.name;
	}
}

TEST(TextureDetail, TheLandsCreationPlacesTheLineByTheTexturePool)
{
	const LodLine current = k_StartTextureLine;
	const auto at = [&current](int32_t index, int32_t count) { return TextureLineAtLandCreation(current, index, count); };
	EXPECT_EQ(at(4, k_TexturePoolCount).distance, 800.0f);
	EXPECT_EQ(at(4, k_TexturePoolCount).width, 170.0f);
	EXPECT_EQ(at(4, k_TexturePoolCount).halfWidth, 85.0f);
	EXPECT_EQ(at(4, 601).distance, 800.0f);
	EXPECT_EQ(at(4, 600).distance, 700.0f);
	EXPECT_EQ(at(4, 501).distance, 700.0f);
	EXPECT_EQ(at(4, 500).distance, 600.0f);
	EXPECT_EQ(at(4, 401).distance, 600.0f);
	EXPECT_EQ(at(4, 401).halfWidth, 85.0f);
	EXPECT_EQ(at(4, 400).distance, 400.0f);
	EXPECT_EQ(at(4, 400).width, 70.0f);
	EXPECT_EQ(at(4, 400).halfWidth, 35.0f);
	EXPECT_EQ(at(4, -1).distance, 400.0f); // signed
	// at any other index the line stays: the start's 300, 40 and 20
	EXPECT_EQ(at(3, k_TexturePoolCount).distance, 300.0f);
	EXPECT_EQ(at(3, k_TexturePoolCount).width, 40.0f);
	EXPECT_EQ(at(3, k_TexturePoolCount).halfWidth, 20.0f);
	// set afresh at index 4: along (1, 0, 0), point 0
	LodLine moved = current;
	moved.along = {0.0f, 0.0f, 1.0f};
	moved.point = {5.0f, 6.0f, 7.0f};
	const auto fresh = TextureLineAtLandCreation(moved, 4, k_TexturePoolCount);
	EXPECT_EQ(fresh.along, glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(fresh.point, glm::vec3(0.0f));
	EXPECT_EQ(TextureLineAtLandCreation(moved, 2, k_TexturePoolCount).point, moved.point);
}

TEST(TextureDetail, ABlocksLevelComesFromItsFourCornersAgainstTheLine)
{
	struct Case
	{
		uint32_t x, z;
		bool pending;
		uint32_t alongX, alongZ;
		uint32_t pointX, pointZ;
		uint32_t halfWidth;
		TextureLevel level;
	};
	constexpr uint32_t k_One = 0x3F800000;
	constexpr uint32_t k_Nan = 0x7FC00000;
	constexpr std::array<Case, 13> cases {{
	    {0x00000000, 0xC47A0000, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::Own},
	    {0x00000000, 0x447A0000, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::TileOnly},
	    {0x00000000, 0x00000000, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::Blended},
	    {0x00000000, 0xC3750000, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::Blended},
	    {0x00000000, 0xC3750007, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::Own},
	    {0x00000000, 0x42AA0000, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::Blended},
	    {0x00000000, 0x42AA000D, false, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::TileOnly},
	    {0x428D6BDE, 0x00000000, false, 0x3F3504F3, 0x3F3504F3, 0, 0, 0x41A00000, TextureLevel::Blended},
	    {0x00000000, 0x00000000, false, k_One, 0, k_Nan, k_Nan, 0x42AA0000, TextureLevel::TileOnly},
	    {0x00000000, 0xC47A0000, false, k_One, 0, 0, 0, k_Nan, TextureLevel::TileOnly},
	    {0x00000000, 0xC47A0000, true, k_One, 0, 0, 0, 0x42AA0000, TextureLevel::TileOnly},
	    {0xC47A0000, 0x00000000, false, 0, k_One, 0, 0, 0x42AA0000, TextureLevel::TileOnly},
	    {0x447A0000, 0x00000000, false, 0, k_One, 0, 0, 0x42AA0000, TextureLevel::Own},
	}};
	for (size_t i = 0; i < cases.size(); ++i)
	{
		const auto& c = cases[i];
		const LodLine line {.along = {F(c.alongX), 0.0f, F(c.alongZ)},
		                    .point = {F(c.pointX), 0.0f, F(c.pointZ)},
		                    .distance = 800.0f,
		                    .width = 170.0f,
		                    .halfWidth = F(c.halfWidth)};
		EXPECT_EQ(BlockTextureLevel(glm::vec2(F(c.x), F(c.z)), line, c.pending), c.level) << i;
	}
}

TEST(TextureDetail, TheTilesAlphaRampsAcrossTheBandTiesToEven)
{
	// along z: v is the vertex's x; the band 170 wide, so (85 - v) x 1.5
	const LodLine line {.along = {0.0f, 0.0f, 1.0f}, .point = {}, .distance = 800.0f, .width = 170.0f, .halfWidth = 85.0f};
	const float scale = TileAlphaScale(line);
	EXPECT_EQ(scale, 1.5f);
	EXPECT_EQ(TileAlpha(line, scale, 86.0f, 0.0f), 0);  // near: the block's own texture shows
	EXPECT_EQ(TileAlpha(line, scale, 85.0f, 0.0f), 0);  // at the near side: (85 - 85) x 1.5
	EXPECT_EQ(TileAlpha(line, scale, 82.0f, 0.0f), 4);  // 4.5 to even
	EXPECT_EQ(TileAlpha(line, scale, 72.0f, 0.0f), 20); // 19.5 to even
	EXPECT_EQ(TileAlpha(line, scale, 62.0f, 0.0f), 34); // 34.5 to even
	EXPECT_EQ(TileAlpha(line, scale, 0.0f, 0.0f), 128); // 127.5 to even
	EXPECT_EQ(TileAlpha(line, scale, -84.0f, 0.0f), 254);
	EXPECT_EQ(TileAlpha(line, scale, -85.0f, 0.0f), 255); // at the far side: kept
	EXPECT_EQ(TileAlpha(line, scale, -200.0f, 0.0f), 255);
	EXPECT_EQ(TileAlpha(line, scale, std::numeric_limits<float>::quiet_NaN(), 0.0f), 255);
}
