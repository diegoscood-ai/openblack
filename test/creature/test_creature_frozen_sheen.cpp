/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ice over a frozen body: its alpha, the wiggle that turns the camera's axes, and the one texture coordinate of each
// face. The numbers are the original's, from its own functions run on the same inputs.

#include <cstdint>

#include <array>
#include <bit>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureFrozenSheen.h"

using namespace openblack::creature_frozen_sheen;

namespace
{
glm::vec3 Vec(const std::array<uint32_t, 3>& bits)
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

struct WiggleCase
{
	std::array<uint32_t, 3> angles;
	int32_t frameGameMs;
	std::array<uint32_t, 3> stepped;
	std::array<uint32_t, 9> rows;
};

// The angles before a frame, the frame's game milliseconds, the angles after it and the turn's rows
constexpr std::array k_WiggleCases {
    WiggleCase {
        {0x00000000u, 0x00000000u, 0x00000000u},
        33,
        {0x3D872B02u, 0x3D94AF4Fu, 0x3DAFB7E9u},
        {0x3F7FFFD8u, 0xBAE06480u, 0x3AACE3A4u, 0x3AE0A4B2u, 0x3F7FFFD5u, 0xBABE263Au, 0xBAAC9031u, 0x3ABE71F7u, 0x3F7FFFDFu}},
    WiggleCase {
        {0x3D872B02u, 0x3D94AF4Fu, 0x3DAFB7E9u},
        33,
        {0x3E072B02u, 0x3E14AF4Fu, 0x3E2FB7E9u},
        {0x3F7FFF64u, 0xBB5F511Fu, 0x3B2C831Au, 0x3B5FD0E2u, 0x3F7FFF58u, 0xBB3DA5F3u, 0xBB2BDD39u, 0x3B3E3C52u, 0x3F7FFF80u}},
    WiggleCase {
        {0x40C80000u, 0x40C66666u, 0x40C33333u},
        50,
        {0x3D88D600u, 0x3CDBAA00u, 0x40C75C29u},
        {0x3F7FFFE8u, 0x3A8B7334u, 0x3AAF0507u, 0xBA8B5B34u, 0x3F7FFFF5u, 0xBA0C9172u, 0xBAAF1824u, 0x3A0C321Fu, 0x3F7FFFEFu}},
    WiggleCase {
        {0x3FC00000u, 0x40400000u, 0x40900000u},
        16,
        {0x3FC41893u, 0x404240B8u, 0x409154CAu},
        {0x3F7FE622u, 0x3CA1C2DFu, 0x3CA3B498u, 0xBCA1720Bu, 0x3F7FF320u, 0xBB0B3012u, 0xBCA40450u, 0x3AE2A542u, 0x3F7FF2C4u}},
    WiggleCase {
        {0x40490FD0u, 0x40000000u, 0x3F000000u},
        100,
        {0x4055DC9Du, 0x400E147Bu, 0x3F428F5Cu},
        {0x3F7FF934u, 0xBC62C619u, 0xBB822E75u, 0x3C61B5C2u, 0x3F7FF176u, 0xBC8280F6u, 0x3B8960BEu, 0x3C820AB7u, 0x3F7FF72Bu}},
    WiggleCase {
        {0x00000000u, 0x00000000u, 0x00000000u},
        0,
        {0x00000000u, 0x00000000u, 0x00000000u},
        {0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3F800000u}},
    WiggleCase {
        {0x40C8F5C3u, 0x40C8F5C3u, 0x40C8F5C3u},
        7,
        {0x3C313000u, 0x3C482000u, 0x3C760000u},
        {0x3F7FFFFFu, 0xB99D6809u, 0x3962CBAAu, 0x399D6F1Fu, 0x3F7FFFFEu, 0xB98013AAu, 0xB962B7F9u, 0x39801C61u, 0x3F7FFFFFu}},
};

struct FaceCase
{
	std::array<uint32_t, 3> a;
	std::array<uint32_t, 3> b;
	std::array<uint32_t, 3> c;
	std::array<uint32_t, 3> axisX;
	std::array<uint32_t, 3> axisY;
	uint32_t u;
	uint32_t v;
};

// A triangle's corners in the clipping space, in their drawn order, the two axes and the face's texture coordinate
constexpr std::array k_FaceCases {
    FaceCase {{0xBF874BE3u, 0xC00612ECu, 0x41D5E253u},
              {0xC0242F39u, 0x3E5C7583u, 0x417E56C0u},
              {0xC029BA79u, 0x3D36BD97u, 0x405B307Fu},
              {0x3F23CF73u, 0xBB575DB2u, 0x3C5634B0u},
              {0xBC2062D8u, 0x3F89C6DAu, 0x3BA7077Eu},
              0x3E650684u,
              0x3E6DEE29u},
    FaceCase {{0x4036E1C8u, 0xC02E1CBCu, 0x420A7CBAu},
              {0xBFA1947Fu, 0xC0089B26u, 0x40CF3C41u},
              {0xBF931601u, 0x3FF2C8F9u, 0x410DE1B3u},
              {0x3F0E637Au, 0x3A84B6BCu, 0xBC8F43EBu},
              {0xBC215B00u, 0x3F062130u, 0x3BEC7430u},
              0x3F45E46Du,
              0x3F01F520u},
    FaceCase {{0xBE8FD14Eu, 0xBF99C769u, 0x4200BEE5u},
              {0x3F98D3E6u, 0xBFC488ACu, 0x41BE9FF3u},
              {0x3E1ACEACu, 0x40100D84u, 0x41EDC05Au},
              {0x3F437E96u, 0xBAAE0E2Cu, 0x3C288517u},
              {0xBC30DF3Bu, 0x3ECF81CCu, 0xBC96FE19u},
              0x3F5F9CC1u,
              0x3EFB915Eu},
    FaceCase {{0x40102EF9u, 0xBF8F0ABBu, 0x41E35EABu},
              {0x3F10F3BFu, 0x3EF57025u, 0x419AAFB9u},
              {0x40028C31u, 0x402AC1EEu, 0x41A0203Bu},
              {0x3F05C6DCu, 0x3BD58704u, 0x3CA193E3u},
              {0x3BDABC3Cu, 0x3F8C81C5u, 0xBB95B208u},
              0x3F3652B4u,
              0x3E6F8D3Au},
    FaceCase {{0xBFFEF058u, 0xC0130904u, 0x4087B046u},
              {0x3FCE00C0u, 0xC00E5557u, 0x41368CC1u},
              {0xBF278052u, 0x400EA044u, 0x40A1FCA3u},
              {0x3F263BC9u, 0x3BF155C4u, 0x3C6E8A76u},
              {0xBBB9BDADu, 0x3F132199u, 0xBBB91C89u},
              0x3E5E9CA8u,
              0x3F0D0334u},
    FaceCase {{0xBFF8AA2Fu, 0xBFCDDB6Du, 0x412DDE4Bu},
              {0xBDB8C729u, 0x3F08E4CAu, 0x413FBFFCu},
              {0xC03E6D95u, 0xBEF8FF11u, 0x418040CAu},
              {0x3F1254ECu, 0x39863B93u, 0x3B9A2194u},
              {0x3B830399u, 0x3ECFC5DDu, 0x3C82EB42u},
              0x3F2ED740u,
              0x3EB79BAAu},
    FaceCase {{0xBF254E56u, 0xBF1B2B24u, 0x40BDE6AFu},
              {0x3F4E44CEu, 0xC02818CAu, 0x4091E50Bu},
              {0xBFDFAB7Du, 0xC001ACF3u, 0x416EC0ACu},
              {0x3F8DE9C2u, 0xBCA328CAu, 0xBBB2C4EAu},
              {0xBCAED2B3u, 0x3FA250B5u, 0x3B958333u},
              0x3D04792Au,
              0x3E4A8FCFu},
    FaceCase {{0xBF50A51Cu, 0xC010D41Eu, 0x420909D5u},
              {0x403D59F8u, 0xBE50F5F5u, 0x41A315F3u},
              {0xC01F0532u, 0xC018C28Cu, 0x41705295u},
              {0x3F4A0333u, 0xBC0D7BB0u, 0x3C93C769u},
              {0x3A6D0616u, 0x3EE47F40u, 0x3AE2590Du},
              0x3EAD6852u,
              0x3F33AECBu},
};
} // namespace

TEST(CreatureFrozenSheen, TheAlphaIsTheFrozenByteTimes255Over256)
{
	EXPECT_EQ(SheenAlpha(0.0f), 0);
	// 1 / 255 is the byte 1, and 1 x 255 >> 8 is still 0
	EXPECT_EQ(SheenAlpha(1.0f / 255.0f), 0);
	EXPECT_EQ(SheenAlpha(2.0f / 255.0f), 1);
	EXPECT_EQ(SheenAlpha(0.5f), 127);
	EXPECT_EQ(SheenAlpha(200.0f / 255.0f), 199);
	EXPECT_EQ(SheenAlpha(1.0f), 254);
}

TEST(CreatureFrozenSheen, ABodysIceIsWhiteAPiecesIsItsColourAtTheSheensAlpha)
{
	// The original's own products: white at the alpha 0x7F over a colour of 0xFF8040C0 gives 0x7F7F3FBF, and the
	// frozen level 200 gives the alpha 0xC7
	EXPECT_EQ(SheenColour(0.5f, 0x008040C0u), 0x7F7F3FBFu);
	EXPECT_EQ(SheenColour(200.0f / 255.0f, 0xFF8040C0u), 0xC77F3FBFu);
	// a body's is white whatever lies under it
	EXPECT_EQ(SheenColour(0.5f, std::nullopt), 0x7FFFFFFFu);
	EXPECT_EQ(SheenColour(1.0f, std::nullopt), 0xFEFFFFFFu);
	// thawed, nothing of it shows
	EXPECT_EQ(SheenColour(0.0f, std::nullopt) >> 24u, 0u);
}

TEST(CreatureFrozenSheen, TheFrozenByteRoundsHalfToEven)
{
	EXPECT_EQ(FrozenLevel(0.5f), 128);
	EXPECT_EQ(FrozenLevel(0.5f / 255.0f), 0);
	EXPECT_EQ(FrozenLevel(1.0f), 255);
}

TEST(CreatureFrozenSheen, TheWiggleStepsAndTurnsAsTheOriginal)
{
	for (const auto& c : k_WiggleCases)
	{
		const auto stepped = StepWiggle(Vec(c.angles), c.frameGameMs);
		EXPECT_EQ(Bits(stepped.x), c.stepped[0]);
		EXPECT_EQ(Bits(stepped.y), c.stepped[1]);
		EXPECT_EQ(Bits(stepped.z), c.stepped[2]);
		const auto rows = WiggleTurn(stepped);
		for (size_t i = 0; i < c.rows.size(); ++i)
		{
			EXPECT_EQ(Bits(rows.at(i / 3)[static_cast<glm::length_t>(i % 3)]), c.rows.at(i)) << "cell " << i;
		}
	}
}

TEST(CreatureFrozenSheen, EachFaceTakesOneCoordinateFromItsNormal)
{
	for (const auto& c : k_FaceCases)
	{
		const auto normal = FaceNormal(Vec(c.a), Vec(c.b), Vec(c.c));
		const auto uv = FaceUv(normal, {.x = Vec(c.axisX), .y = Vec(c.axisY)});
		EXPECT_EQ(Bits(uv.x), c.u);
		EXPECT_EQ(Bits(uv.y), c.v);
	}
}

TEST(CreatureFrozenSheen, ADrawnFaceNormalPointsAwayFromTheEye)
{
	// Counter-clockwise on the screen with y up, in front of the eye: the winding a face is drawn with
	const glm::vec3 a {-1.0f, -1.0f, 10.0f};
	const glm::vec3 b {1.0f, -1.0f, 10.0f};
	const glm::vec3 c {0.0f, 1.0f, 12.0f};
	const auto normal = FaceNormal(a, b, c);
	EXPECT_GT(normal.x * a.x + normal.y * a.y + normal.z * a.z, 0.0f);
	EXPECT_EQ(AwayFromEye(normal, a), normal);
	// The other winding gives the opposite normal, which is turned back
	const auto reversed = FaceNormal(a, c, b);
	EXPECT_EQ(AwayFromEye(reversed, a), -reversed);
	EXPECT_EQ(Bits(FaceUv(AwayFromEye(reversed, a), {.x = {1, 0, 0}, .y = {0, 1, 0}}).x),
	          Bits(FaceUv(AwayFromEye(normal, a), {.x = {1, 0, 0}, .y = {0, 1, 0}}).x));
}

TEST(CreatureFrozenSheen, TheAxesAreTheWigglesRowsBackInTheCamerasScale)
{
	const Rows still {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
	const auto axes = FaceAxes(still, 2.0f, 4.0f);
	EXPECT_EQ(axes.x, glm::vec3(0.5f, 0.0f, 0.0f));
	EXPECT_EQ(axes.y, glm::vec3(0.0f, 0.25f, 0.0f));
	// Facing straight away from the eye, a face takes the middle of the map
	const auto uv = FaceUv({0.0f, 0.0f, 1.0f}, axes);
	EXPECT_EQ(uv, glm::vec2(0.498046875f, 0.498046875f));
}
