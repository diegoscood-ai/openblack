/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <ios>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/ModelInverseSquareRoot.h"
#include "Particles/CreatureSpellMaths.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
constexpr float k_Epsilon = 1e-3f;
} // namespace

TEST(CreatureSpellMaths, WispsWindRoundTheBoxOfTheBones)
{
	const std::vector<glm::vec3> bones {{-2.0f, 0.0f, -1.0f}, {2.0f, 10.0f, 2.0f}};
	const auto box = maths::WispBoxOf(bones);
	EXPECT_FLOAT_EQ(box.centreX, 0.0f);
	EXPECT_FLOAT_EQ(box.centreZ, 0.5f);
	EXPECT_FLOAT_EQ(box.bottom, 0.0f);
	EXPECT_FLOAT_EQ(box.height, 10.0f);
	EXPECT_FLOAT_EQ(box.radius, 2.5f);
	// At the top, narrowed by the taper; with no wobble yet
	const auto top = maths::WispOrbit(box, 0.0f, 0.0f, 0.5f, 0.0f);
	EXPECT_NEAR(top.y, 12.0f, k_Epsilon);
	EXPECT_NEAR(top.x, (1.0f - 0.5f * std::pow(0.3f * 3.3333333f, 2.0f)) * 2.5f * 1.2f, k_Epsilon);
	// At the bottom, full width
	const auto bottom = maths::WispOrbit(box, 0.0f, std::numbers::pi_v<float>, 0.5f, 0.0f);
	EXPECT_NEAR(bottom.y, 0.0f, k_Epsilon);
	EXPECT_NEAR(bottom.x, 3.0f, k_Epsilon);
}

TEST(CreatureSpellMaths, WispsFlyFromTheHandOverTwoSecondsAndFadeIn)
{
	const glm::vec3 hand(0.0f);
	const glm::vec3 orbit(10.0f, 0.0f, 0.0f);
	EXPECT_FLOAT_EQ(maths::WispPosition(hand, orbit, 0.0f).x, 0.0f);
	EXPECT_FLOAT_EQ(maths::WispPosition(hand, orbit, 1.0f).x, 5.0f);
	EXPECT_FLOAT_EQ(maths::WispPosition(hand, orbit, 3.0f).x, 10.0f);
	EXPECT_EQ(maths::WispAlpha(0.0f), 20);
	EXPECT_EQ(maths::WispAlpha(0.25f), 35);
	EXPECT_EQ(maths::WispAlpha(2.0f), 50);
	// Three over two seconds, no more
	EXPECT_FLOAT_EQ(maths::WispsDue(0.0f, 3, 0.1f, 2.0f), 0.15f);
	EXPECT_FLOAT_EQ(maths::WispsDue(2.95f, 3, 0.1f, 2.0f), 3.0f);
}

TEST(CreatureSpellMaths, TheFliesCircleTheHeadHalfAsWideAgainAsTheEyes)
{
	const glm::vec3 right(-1.0f, 10.0f, 0.0f);
	const glm::vec3 left(1.0f, 10.0f, 0.0f);
	const auto start = maths::ItchOrbit(right, left, 0.0f, 1.0f);
	EXPECT_NEAR(start.x, -1.5f, k_Epsilon);
	const auto quarter = maths::ItchOrbit(right, left, std::numbers::pi_v<float> / 2.0f, 1.0f);
	EXPECT_NEAR(glm::length(quarter - glm::vec3(0.0f, 10.0f, 0.0f)), 1.5f, k_Epsilon);
	EXPECT_NEAR(quarter.y, 10.0f, k_Epsilon);
}

TEST(CreatureSpellMaths, ACreaturesCastGivesUpOnceItSwingsTooFar)
{
	// The game takes 40 as radians: a cosine of about -0.667, so only a swing past about 132 degrees ends it
	EXPECT_FALSE(maths::SwungTooFar({1.0f, 0.0f}, {0.0f, 1.0f}, 40.0f));
	EXPECT_TRUE(maths::SwungTooFar({1.0f, 0.0f}, {-1.0f, 0.1f}, 40.0f));
	EXPECT_FALSE(maths::SwungTooFar({0.0f, 0.0f}, {-1.0f, 0.0f}, 40.0f));
}

namespace
{
/// The original's inverse square root, run in the original game (its table built first): each input's bits and the
/// bits it returns. The edges first (0, 1, 2, 0.5, the smallest and largest, just under and over 1, -0, infinity),
/// then a spread from 0 to 4 and from 0.0001 to a million
struct InverseSquareRootCase
{
	uint32_t in;
	uint32_t out;
};
constexpr std::array<InverseSquareRootCase, 32> k_InverseSquareRootCases {{
    {0x00000000u, 0x5F87C000u}, {0x3F800000u, 0x3F7FFFA0u}, {0x40000000u, 0x3F3504F3u}, {0x3F000000u, 0x3FB504F3u},
    {0x40800000u, 0x3EFFFFA0u}, {0x41200000u, 0x3EA1E896u}, {0x42C80000u, 0x3DCCCCA2u}, {0x3E800000u, 0x3FFFFFA0u},
    {0x00000001u, 0x5F87BFFFu}, {0x007FFFFFu, 0x5EFFFE81u}, {0x00800000u, 0x5EFFFFA0u}, {0x7F7FFFFFu, 0x1F7FFE81u},
    {0x3F7FFFFFu, 0x3F7FFE81u}, {0x3F800001u, 0x3F7FFFA0u}, {0x80000000u, 0x5F87C000u}, {0x7F800000u, 0xFF800000u},
    {0x40771A1Cu, 0x3F02489Fu}, {0x3D8A494Cu, 0x40764B6Bu}, {0x3F7577E6u, 0x3F82B753u}, {0x3F6A18ABu, 0x3F85DAAAu},
    {0x406CC234u, 0x3F05197Eu}, {0x3EA1B568u, 0x3FE3C016u}, {0x40254428u, 0x3F1F4DB9u}, {0x4071ED25u, 0x3F03AB53u},
    {0x4448CFDCu, 0x3D1085BCu}, {0x3A82B27Du, 0x41FD5795u}, {0x420DE4EDu, 0x3E2BEB19u}, {0x3EB0B825u, 0x3FD9DF66u},
    {0x47DA2B8Au, 0x3B441617u}, {0x460EFFE2u, 0x3C2B431Cu}, {0x46C440DDu, 0x3BCEBEC4u}, {0x47A9E00Fu, 0x3B5E350Eu},
}};

glm::vec2 Bits(uint32_t x, uint32_t y)
{
	return {std::bit_cast<float>(x), std::bit_cast<float>(y)};
}
} // namespace

TEST(CreatureSpellMathsOracle, TheWaysAreMadeUnitLengthByTheOriginalsInverseSquareRoot)
{
	for (const auto& c : k_InverseSquareRootCases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(gutils::ModelInverseSquareRoot(std::bit_cast<float>(c.in))), c.out)
		    << std::hex << c.in;
	}
}

TEST(CreatureSpellMathsOracle, NearTheLimitTheTableRootDecidesAsTheOriginal)
{
	// Turned by a hair less than the limit's 2.30 radians, as the original's inverse square root makes the two ways
	// (dot -0.66693026): not too far, where their exact lengths (dot -0.66693836) put it just past the cosine of 40,
	// -0.66693806
	EXPECT_FALSE(maths::SwungTooFar(Bits(0x409B3C6Bu, 0x40CAC048u), Bits(0x3FF19BC0u, 0xC11F62EFu), 40.0f));
	// A way of no length meets the other at a dot of 0: too far for a limit whose cosine is above 0, not for 40
	EXPECT_TRUE(maths::SwungTooFar({0.0f, 0.0f}, {-1.0f, 0.0f}, 1.0f));
	EXPECT_TRUE(maths::SwungTooFar({3.0f, 4.0f}, {0.0f, 0.0f}, 1.0f));
	EXPECT_FALSE(maths::SwungTooFar({0.0f, 0.0f}, {-1.0f, 0.0f}, 40.0f));
	// The table root makes each way a little short of unit length, so straight on meets at 0.99999368: within a limit
	// of a tenth, but past a limit of nothing
	EXPECT_FALSE(maths::SwungTooFar({3.0f, 4.0f}, {6.0f, 8.0f}, 0.1f));
	EXPECT_TRUE(maths::SwungTooFar({3.0f, 4.0f}, {6.0f, 8.0f}, 0.0f));
}

namespace
{
/// The wisps' orbit and the flies' circle, run in the original game's rules: a wisp's box (centre x, bottom, centre
/// z, radius, height), theta, phi, taper and age, then the point the rule makes; most of them cases where a sine or
/// cosine rounded to a float first gives another point
struct WispOrbitCase
{
	uint32_t centreX;
	uint32_t bottom;
	uint32_t centreZ;
	uint32_t radius;
	uint32_t height;
	uint32_t theta;
	uint32_t phi;
	uint32_t taper;
	uint32_t age;
	uint32_t point[3];
};
constexpr WispOrbitCase k_WispOrbitCases[] = {
    {0xC4CB3CF4u,
     0xC1A8A697u,
     0x44D28231u,
     0x4099F4AAu,
     0x410582B4u,
     0x3FF01329u,
     0x40C59B03u,
     0x3F800000u,
     0x436F80DCu,
     {0xC4CB3361u, 0xC11BFA36u, 0x44D28682u}},
    {0x440DC82Eu,
     0x423ADBA6u,
     0xC3983BCEu,
     0x40DA904Fu,
     0x4000770Fu,
     0xC06EBA17u,
     0xC0A9F74Cu,
     0x3F800000u,
     0x42C7BEBCu,
     {0x440BB404u, 0x42439534u, 0xC394CE86u}},
    {0xC4F7E375u,
     0x40C11890u,
     0xC4EA6C16u,
     0x40EAD2D6u,
     0x41065836u,
     0xC0580EC6u,
     0x3FFE4545u,
     0x3F800000u,
     0x4036B07Eu,
     {0xC4F90191u, 0x41261382u, 0xC4E9E272u}},
    {0xC4F4AC72u,
     0x405E2401u,
     0x44D5ADB5u,
     0x40DFE61Bu,
     0x3F22BA46u,
     0xBEB8B589u,
     0x3FC21B1Cu,
     0x3F800000u,
     0x407868F4u,
     {0xC4F3F990u, 0x40723824u, 0x44D516E4u}},
    {0x4427085Au,
     0x40AFA918u,
     0xC2C30CD1u,
     0x40E3FB70u,
     0x4132962Du,
     0xC09B3E9Eu,
     0x3F4968F7u,
     0x3F800000u,
     0x40B726BEu,
     {0x44276384u, 0x418AA4B7u, 0xC2B8B4F6u}},
    {0x43981F81u,
     0xC1E53F6Au,
     0x438E1949u,
     0x40AE2206u,
     0x402774ABu,
     0x40B44CB5u,
     0x408CED28u,
     0x3F800000u,
     0x42E69315u,
     {0x439ABBDCu, 0xC1EDC294u, 0x438BC82Cu}},
    {0xC4F269B1u,
     0xC1F77B37u,
     0x4433E08Cu,
     0x40E5EFC2u,
     0x41142192u,
     0x3ED90CC0u,
     0x409276D9u,
     0x3F800000u,
     0x433ECCF5u,
     {0xC4F14568u, 0xC1C00C21u, 0x4434DF09u}},
    {0x44F3C795u,
     0xC1514F8Du,
     0x434EEA6Cu,
     0x40B7E459u,
     0x4130E1FCu,
     0x4070ACC8u,
     0x40C86769u,
     0x3EA751BCu,
     0x42841F32u,
     {0x44F32B50u, 0xBF8D6C2Fu, 0x434BD38Eu}},
    {0x4359737Cu,
     0xBF540033u,
     0x44C15048u,
     0x4049E490u,
     0x411A5ED7u,
     0xC0C32EB7u,
     0x401ECB7Fu,
     0x3FD33B63u,
     0x43C71AD8u,
     {0x435E03D2u, 0xBEBCD25Eu, 0x44C178A7u}},
    {0xC3D3B359u,
     0xC11B82FDu,
     0xC4838796u,
     0x401ECF94u,
     0x4133541Au,
     0xC085ACDFu,
     0x40BCCFBDu,
     0x3F3573F9u,
     0x438459F3u,
     {0xC3D41C5Bu, 0x40580890u, 0xC4837314u}},
    {0x44C97FABu,
     0xC16D3685u,
     0xC4B5152Bu,
     0x40E3951Fu,
     0x401E4FCAu,
     0xC013DDA9u,
     0xC0AB1317u,
     0x3E57688Fu,
     0x438C2B08u,
     {0x44C8ADA0u, 0xC14E3AD7u, 0xC4B59171u}},
    {0xC4AECF80u,
     0xC10A923Bu,
     0x4499995Au,
     0x40FF9625u,
     0x41342ED3u,
     0x4066E4C5u,
     0xBF7259CAu,
     0x3F1D1DF3u,
     0x40B28F06u,
     {0xC4AFC879u, 0x4017DAE6u, 0x449900ADu}},
    {0xC3F74B17u,
     0xC1552BE3u,
     0xC30BCEBBu,
     0x40B81A22u,
     0x410B164Cu,
     0xC0A42D8Bu,
     0x401B8C53u,
     0x3F448BFBu,
     0x44067132u,
     {0xC3F59F64u, 0xC144DE8Fu, 0xC30660ADu}},
    {0x448CE313u,
     0xC0F48A73u,
     0x44313273u,
     0x3F3D5784u,
     0x40C9C123u,
     0xC0351835u,
     0xC014B63Du,
     0x3FB143DCu,
     0x43FBCA1Au,
     {0x448CD03Cu, 0xC0CF82D8u, 0x44312803u}},
    {0x44176024u,
     0xC13694A4u,
     0x423355D5u,
     0x40E4C74Fu,
     0x4118BC41u,
     0xC0995371u,
     0x3CFC12E0u,
     0x3F800000u,
     0x41B52B40u,
     {0x4416D45Eu, 0xBDBBD1D8u, 0x4233B49Au}},
    {0xC43B75AEu,
     0x41A740B0u,
     0x4244F627u,
     0x4024953Cu,
     0x413BB36Bu,
     0x3FF527F8u,
     0xBFB89339u,
     0x3EA833EDu,
     0x42D9FE09u,
     {0xC43BA7E1u, 0x41E830A1u, 0x42512125u}},
    {0xC4545C20u,
     0xC010BCB9u,
     0x4420C3D4u,
     0x40376403u,
     0x41148F3Au,
     0xC01D70D7u,
     0xC00FC0EFu,
     0x3DBBC047u,
     0x40916104u,
     {0xC45538B0u, 0xBEA00960u, 0x442013CAu}},
    {0x44723B44u,
     0xC05D9A6Bu,
     0x436F40FDu,
     0x3FFC3C9Eu,
     0x41053E83u,
     0xBFF789C8u,
     0x4030CD8Fu,
     0x3FAC7A1Eu,
     0x42C6FFC4u,
     {0x4471E2D0u, 0xC046B774u, 0x436D9CEEu}},
    {0xC4B326D5u,
     0x4078BADFu,
     0xC4DA90B3u,
     0x40E27CA1u,
     0x40231911u,
     0x4000D89Fu,
     0xBF76E2D7u,
     0x3F800000u,
     0x40F316DBu,
     {0xC4B379CAu, 0x40C7E308u, 0xC4D96AFDu}},
    {0x44EC5233u,
     0xC13816BFu,
     0x43ECA70Du,
     0x40836687u,
     0x412D6C62u,
     0xBF06DA73u,
     0x404F17B7u,
     0x3F894D5Bu,
     0x410F15E5u,
     {0x44ECD174u, 0xC14D0800u, 0x43EB8516u}},
    {0x44897391u,
     0xC222A339u,
     0xC4A6198Eu,
     0x3FE41280u,
     0x40C32C9Au,
     0xBFC5B353u,
     0xC0858E47u,
     0x3F800000u,
     0x3EB95ADDu,
     {0x44897E93u, 0xC21AABD5u, 0xC4A650CDu}},
    {0xC4C41DA3u,
     0xC02988E8u,
     0xC4D5BEECu,
     0x40AFB4FDu,
     0x408720B0u,
     0xBFA73805u,
     0xC0850886u,
     0x3F800000u,
     0x440E4681u,
     {0xC4C419B2u, 0xBF075C4Cu, 0xC4D673FEu}},
    {0x4435AD8Fu,
     0xC1938EA7u,
     0xC47A1DFFu,
     0x404CA020u,
     0x40028AAFu,
     0xBF897D39u,
     0x40662728u,
     0x3F800000u,
     0x40BEBC15u,
     {0x44363E48u, 0xC1906ADCu, 0xC47B2A31u}},
    {0xC4C6FC32u,
     0x405D7B32u,
     0xC41E8CE4u,
     0x4097BB1Au,
     0x4096C502u,
     0xC06B5EDEu,
     0x4023D9FCu,
     0x3F800000u,
     0x40B173E3u,
     {0xC4C794E5u, 0x40833243u, 0xC41DE9C3u}},
    {0xC49BB517u,
     0xC207925Du,
     0x44AB861Au,
     0x4019E8FBu,
     0x3FF79514u,
     0x3D7F4F79u,
     0xBEEB877Eu,
     0x3F9F230Cu,
     0x4311F5ACu,
     {0xC49B895Bu, 0xC1F621FEu, 0x44AB8CA8u}},
    {0xC20074EDu,
     0x4149FC0Eu,
     0xC4ACA9D3u,
     0x40B5F62Fu,
     0x3FF70006u,
     0xC0033C84u,
     0x409B12A5u,
     0x3F800000u,
     0x43E00DE3u,
     {0xC20D5BADu, 0x4143DF97u, 0xC4AD4CFAu}},
    {0xC3DE9111u,
     0xC1C3ECB1u,
     0x44C70AD6u,
     0x3F29D247u,
     0x4127AE80u,
     0x40812113u,
     0xC007B7E7u,
     0x3F800000u,
     0x40D82A93u,
     {0xC3DEBF01u, 0xC1AB51FFu, 0x44C6F42Eu}},
    {0x44B92018u,
     0x4297EE7Cu,
     0x4414FB14u,
     0x40CC68F8u,
     0x3FF5BDD5u,
     0x4082EBD1u,
     0x40A5C2A4u,
     0x3F800000u,
     0x4103C34Du,
     {0x44B89124u, 0x42992CD4u, 0x44139863u}},
    {0x445B937Du,
     0x42BAC9FDu,
     0xC4A4C068u,
     0x40BC2DF0u,
     0x3F48D376u,
     0x408F2E94u,
     0x40C458F0u,
     0x3F847BBEu,
     0x43FEA81Bu,
     {0x445BE332u, 0x42BABF6Eu, 0xC4A4C77Fu}},
    {0x43902BC8u,
     0xC1FB5D27u,
     0x43F83501u,
     0x40576069u,
     0x40FD5BDCu,
     0xC0259F65u,
     0xBF9178F0u,
     0x3F800000u,
     0x435AC89Eu,
     {0x438E7C7Eu, 0xC1BF736Eu, 0x43F71B03u}},
    {0x44C93EA4u,
     0x41CE7E36u,
     0x44E84D53u,
     0x40443F9Fu,
     0x3FDAD809u,
     0xC0AD0DA2u,
     0x40A68822u,
     0x3F800000u,
     0x437F70B8u,
     {0x44C968FEu, 0x41D7555Du, 0x44E8927Cu}},
    {0xC3BA6581u,
     0x42AE7CF1u,
     0xC24D385Au,
     0x3F45BA66u,
     0x4087F846u,
     0x40406E6Bu,
     0x40A0FA38u,
     0x3F800000u,
     0x4342A51Du,
     {0xC3BACA1Fu, 0x42B5582Fu, 0xC24C86B3u}},
};

/// The right eye, the left eye, the seconds and the orbiting speed, then the point the flies' rule makes
struct ItchOrbitCase
{
	uint32_t right[3];
	uint32_t left[3];
	uint32_t seconds;
	uint32_t speed;
	uint32_t point[3];
};
constexpr ItchOrbitCase k_ItchOrbitCases[] = {
    {{0x4040DD6Eu, 0x40E656ABu, 0x409EA7D8u},
     {0xC1162375u, 0xC0ABC359u, 0xBFB9B17Fu},
     0x44088260u,
     0x40B8008Au,
     {0xC13EAFDFu, 0xBFCFB834u, 0x41482274u}},
    {{0x409F174Du, 0xC201F1C0u, 0x41F8C786u},
     {0x409DE623u, 0xC20420BEu, 0x41F6D483u},
     0x4393297Au,
     0x3F5F4CDAu,
     {0x409418B9u, 0xC201FEA7u, 0x41F92750u}},
    {{0xBE150C9Bu, 0xBF870485u, 0x3FCF6175u},
     {0x3D8BC2E7u, 0xBF1C341Du, 0x3F9F5396u},
     0x432B7BB8u,
     0x3F800000u,
     {0x3EC966CFu, 0xBF3EA354u, 0x3FCA46E0u}},
    {{0x40CACA4Eu, 0xBE51C461u, 0xC092AE17u},
     {0xBF2DC758u, 0x40E1DDD0u, 0xC089D37Au},
     0x42CB0E3Du,
     0x3F800000u,
     {0x40ADEFB9u, 0x3EF02628u, 0xC12EA6C9u}},
    {{0x40092E6Fu, 0x400B240Du, 0xBF9CF788u},
     {0x3DF717A4u, 0x40301C9Eu, 0xBF65F221u},
     0x40BB0C41u,
     0x3EDF6782u,
     {0xBE8A4BBCu, 0x4034A65Eu, 0xBFDE409Fu}},
    {{0x3F217283u, 0x3FB68513u, 0x40B68765u},
     {0xC113ABC4u, 0x3FEEFEF6u, 0xC1062104u},
     0x40F45F3Cu,
     0x3F800000u,
     {0x40F3B1E9u, 0x3FC99FA5u, 0xC0C91B1Cu}},
    {{0xC093E124u, 0x40E9D543u, 0x407493D0u},
     {0xC038C158u, 0xC1125A6Fu, 0xC0B26FDDu},
     0x40DD1874u,
     0x3F800000u,
     {0x405A9140u, 0x41114970u, 0x40CB42D8u}},
    {{0x3E43EFC5u, 0x40053575u, 0xC03644F3u},
     {0x3D42B429u, 0x3EC5C070u, 0xBF820CD0u},
     0x43C3FBAFu,
     0x3F9ED20Au,
     {0xBF8367FDu, 0x3E3DB538u, 0xBF61BE48u}},
    {{0xC0EAC23Au, 0x4037D6BAu, 0xC0DDD9BFu},
     {0xC0AC9B6Du, 0xC03BD329u, 0x3FE5107Fu},
     0x43AEDD06u,
     0x3F800000u,
     {0x3F821844u, 0xC016426Cu, 0xBF102AF2u}},
    {{0x3F5018DBu, 0xBE368DE5u, 0xBFBB12DAu},
     {0xBE8DDBDEu, 0xBEAE6DDAu, 0xBEFEECD4u},
     0x410F079Cu,
     0x409AFE90u,
     {0x3FAE34D9u, 0xBE290624u, 0xBF829767u}},
    {{0xC09AB76Du, 0xC06627C8u, 0x409CFDB0u},
     {0x3F8CA716u, 0x3C469CE2u, 0x412ADF24u},
     0x4327A5DAu,
     0x3F800000u,
     {0x4088CE54u, 0xBF2B9EFAu, 0x40A56BA7u}},
    {{0xC10F0DA6u, 0x408F4B6Cu, 0x3FCE5910u},
     {0xC0B96FAAu, 0xC0982BE3u, 0xC0EC2B71u},
     0x42C0747Fu,
     0x40AFC853u,
     {0xC00C624Cu, 0x40946194u, 0x4084AF14u}},
    {{0xBF97E048u, 0x4101B371u, 0xBF1C384Bu},
     {0x4036DC06u, 0x40AE79FAu, 0xC0933FEBu},
     0x40B481B7u,
     0x3F800000u,
     {0xC064A0ECu, 0x4105F81Cu, 0xC00DB7DCu}},
    {{0xBFA9E9C2u, 0xC085F6C6u, 0xBEE59316u},
     {0xC116DD40u, 0x3FE8C7DBu, 0xC11776BCu},
     0x43B61AF2u,
     0x3F800000u,
     {0xBF85DB0Eu, 0xC0B2B56Eu, 0x4045F696u}},
    {{0x4124FD05u, 0x3E071D83u, 0xBF848E48u},
     {0x3F162795u, 0xBEBCF753u, 0xC0C4160Du},
     0x41EF4434u,
     0x3F800000u,
     {0x40060744u, 0xBDC23C00u, 0x407C91EAu}},
    {{0xC0A9B54Eu, 0x402A116Cu, 0xBF8D5A2Du},
     {0xC113FFB5u, 0x412F6D9Eu, 0xC0D9154Au},
     0x4398B0D0u,
     0x4089335Au,
     {0xC0A4780Du, 0x41382CA0u, 0xC1231B44u}},
    {{0x3EB4D451u, 0xC03A3E5Cu, 0xBF4E9EB5u},
     {0x3E47ED41u, 0xBF7C0EDAu, 0x3F70529Fu},
     0x40AFA64Au,
     0x40BBD4A3u,
     {0xBF8583E6u, 0xC03CCF08u, 0xBF78560Cu}},
    {{0x401C90BDu, 0x40B8A7ECu, 0x411A09D1u},
     {0xC108309Cu, 0xC103C164u, 0xC09044AEu},
     0x4211C698u,
     0x3F800000u,
     {0xC154825Du, 0x4001D13Du, 0x417C5F40u}},
    {{0xC034E912u, 0xC13C9019u, 0x401EF0D6u},
     {0xC0A12F72u, 0xC078D743u, 0xBF7DAF41u},
     0x43856AE1u,
     0x404E8800u,
     {0xBF36B478u, 0xC1581803u, 0x400CF7AEu}},
    {{0xC0D2C6DEu, 0xC0FA8C24u, 0x3EA8270Au},
     {0x3FA5767Cu, 0xC10A2D95u, 0x40AE6987u},
     0x4360A384u,
     0x3F800000u,
     {0x3F8F6798u, 0xC1039508u, 0xC0464AACu}},
    {{0x4213AA13u, 0x41F87D87u, 0xC198A110u},
     {0x42198410u, 0x41E2F7DBu, 0xC186C1CEu},
     0x42C9E797u,
     0x411CB458u,
     {0x421221FBu, 0x41E35763u, 0xC17B0F9Du}},
    {{0x40FBB902u, 0xC11134D9u, 0x3F8D068Au},
     {0x410CC6B0u, 0xC0EBBB93u, 0xBF92A928u},
     0x421562DEu,
     0x40BD1CB5u,
     {0x41151688u, 0xC111DC24u, 0x3FE226A1u}},
    {{0xBFC1CCA9u, 0xBF4A9DB7u, 0x3F82CCF1u},
     {0xBFD693CEu, 0xBEE17FC4u, 0x3F8B2FEDu},
     0x40F02CD7u,
     0x3F800000u,
     {0xBFD41D47u, 0xBF34B319u, 0x3F480CBEu}},
    {{0x40A44E27u, 0xBE143E92u, 0xC0A980CBu},
     {0x411F5A97u, 0xC0DF4580u, 0xC08C9843u},
     0x43CABCD6u,
     0x3F800000u,
     {0x41354107u, 0xC1092AE1u, 0xC0AF2E08u}},
    {{0x3F9458FDu, 0xC0205704u, 0x3E0201D8u},
     {0xBF37C4C6u, 0xBFDE34CDu, 0xBFC99DCDu},
     0x43AE4CA1u,
     0x408B01A3u,
     {0x3FD84908u, 0xC02C9B94u, 0x3EF67E76u}},
    {{0x4076CFAFu, 0xC057F91Eu, 0xC108C201u},
     {0x411B0CDEu, 0x40D40583u, 0xC09C0EC5u},
     0x411E6A73u,
     0x3F800000u,
     {0x414E2D3Du, 0x4104A8FAu, 0xC0F99903u}},
    {{0x43F2B33Au, 0x445D08FBu, 0xC3D20EA2u},
     {0x43F94206u, 0x445FAD09u, 0xC3D574C8u},
     0x40A267FFu,
     0x3EFA0E4Du,
     {0x43FBC8CAu, 0x445FEAA8u, 0xC3D20B82u}},
    {{0xBF699CDFu, 0xBF2B973Du, 0xBEE12F6Du},
     {0xBF83A2C3u, 0xBF42D776u, 0xBE16284Fu},
     0x43B26388u,
     0x3F800000u,
     {0xBF3A8B10u, 0xBF33A2ADu, 0xBE7FE042u}},
    {{0xC440F883u, 0xC465510Cu, 0x445D6589u},
     {0xC4418B5Au, 0xC4652DB8u, 0x445D6EA0u},
     0x410BA586u,
     0x3F800000u,
     {0xC4419AE1u, 0xC4652B10u, 0x445D269Du}},
    {{0xC386B637u, 0xC42E9F8Cu, 0xC4EBFA90u},
     {0xC384C812u, 0xC42E6EB5u, 0xC4EB5033u},
     0x43E835A4u,
     0x3F64D12Fu,
     {0xC387E1DAu, 0xC42EA8A5u, 0xC4EBF4A7u}},
    {{0xBFDC0771u, 0xC161EA1Bu, 0xC0DF9447u},
     {0xBF1EBDB9u, 0xC1BC7AB8u, 0x3EDAEDEEu},
     0x40E91231u,
     0x3F800000u,
     {0xC1124234u, 0xC17041B2u, 0xC0A55AACu}},
    {{0x421185D9u, 0x41006C20u, 0x419D690Bu},
     {0x421A3298u, 0x410842C7u, 0x41996A30u},
     0x42E179E7u,
     0x40FA8CCBu,
     {0x421C78C4u, 0x410A332Fu, 0x41998A1Eu}},
};

glm::vec3 Point(const uint32_t (&bits)[3])
{
	return {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
}

void ExpectBits(glm::vec3 got, const uint32_t (&bits)[3])
{
	EXPECT_EQ(std::bit_cast<uint32_t>(got.x), bits[0]);
	EXPECT_EQ(std::bit_cast<uint32_t>(got.y), bits[1]);
	EXPECT_EQ(std::bit_cast<uint32_t>(got.z), bits[2]);
}
} // namespace

TEST(CreatureSpellMathsOracle, TheWispsWindRoundAsTheOriginalsRuleReckonsIt)
{
	for (const auto& c : k_WispOrbitCases)
	{
		const maths::WispBox box {.centreX = std::bit_cast<float>(c.centreX),
		                          .centreZ = std::bit_cast<float>(c.centreZ),
		                          .bottom = std::bit_cast<float>(c.bottom),
		                          .height = std::bit_cast<float>(c.height),
		                          .radius = std::bit_cast<float>(c.radius)};
		SCOPED_TRACE(testing::Message() << std::hex << c.theta << " " << c.phi << " " << c.age);
		ExpectBits(maths::WispOrbit(box, std::bit_cast<float>(c.theta), std::bit_cast<float>(c.phi),
		                            std::bit_cast<float>(c.taper), std::bit_cast<float>(c.age)),
		           c.point);
	}
}

TEST(CreatureSpellMathsOracle, TheFliesCircleAsTheOriginalsRuleReckonsIt)
{
	for (const auto& c : k_ItchOrbitCases)
	{
		SCOPED_TRACE(testing::Message() << std::hex << c.right[0] << " " << c.seconds);
		ExpectBits(
		    maths::ItchOrbit(Point(c.right), Point(c.left), std::bit_cast<float>(c.seconds), std::bit_cast<float>(c.speed)),
		    c.point);
	}
}
