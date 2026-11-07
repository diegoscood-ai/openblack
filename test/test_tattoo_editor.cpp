/******************************************************************************
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
#include <limits>
#include <map>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "3D/TempleLight.h"
#include "Creature/CreatureLayers.h"
#include "Creature/TattooEditor.h"
#include "Graphics/ArgbColour.h"

using namespace openblack;
using namespace openblack::creature_tattoo_editor;
using creature_tattoo::k_NoSite;
using creature_tattoo::Site;
using creature_tattoo::Slot;
using creature_tattoo::Slots;

namespace
{
constexpr glm::u8vec3 k_Red {255, 0, 0};
constexpr glm::u8vec3 k_Blue {0, 0, 255};

Slots Full()
{
	Slots slots {};
	for (uint8_t i = 0; i < slots.size(); ++i)
	{
		slots.at(i) = {.design = i, .site = i, .colour = k_Red};
	}
	return slots;
}

/// A place at a texel of skin 1
Site At(uint8_t u, uint8_t v)
{
	return {.enabled = true, .u = u, .v = v, .skin = 1, .size = 0.25f, .mirror = false, .rotation = 0};
}

using Triangles = std::vector<std::array<uint16_t, 3>>;

/// A view down the z axis from the origin, the depth being z: x and y are kept as they are and w is z
glm::mat4 AlongZ()
{
	glm::mat4 transform(0.0f);
	transform[0][0] = 1.0f;
	transform[1][1] = 1.0f;
	transform[2][2] = 1.0f;
	transform[2][3] = 1.0f;
	return transform;
}

constexpr glm::ivec2 k_Screen {640, 480};

SiteView ViewAlongZ()
{
	return {.eye = glm::vec3(0.0f), .worldToClip = AlongZ(), .nearPlane = 1.0f, .screenSize = k_Screen};
}
} // namespace

TEST(TattooEditor, OpeningKeepsTheTattoosToGoBackTo)
{
	auto slots = Full();
	const auto session = Open(slots);
	EXPECT_EQ(session.slots, slots);
	EXPECT_EQ(session.opened, slots);
	EXPECT_FALSE(session.changed);
}

TEST(TattooEditor, ASymbolDroppedOnAPlaceGoesInTheFirstEmptySlot)
{
	auto session = Open({});
	EXPECT_TRUE(Drop(session, 5, k_Red, 3));
	EXPECT_EQ(session.slots.at(0), (Slot {.design = 5, .site = 3, .colour = k_Red}));
	EXPECT_TRUE(session.changed);
	EXPECT_TRUE(Drop(session, 6, k_Blue, 4));
	EXPECT_EQ(session.slots.at(1), (Slot {.design = 6, .site = 4, .colour = k_Blue}));
}

TEST(TattooEditor, TheSameSymbolOnTheSamePlaceOnlyTakesTheNewColour)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	EXPECT_FALSE(Drop(session, 5, k_Blue, 3));
	EXPECT_EQ(session.slots.at(0), (Slot {.design = 5, .site = 3, .colour = k_Blue}));
	EXPECT_TRUE(session.slots.at(1).Empty());
}

TEST(TattooEditor, ANewSymbolOnAPlaceStaysWithAnotherWhileASlotIsEmpty)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	ASSERT_TRUE(Drop(session, 6, k_Red, 3));
	EXPECT_EQ(session.slots.at(0).site, 3);
	EXPECT_EQ(session.slots.at(1).site, 3);
}

TEST(TattooEditor, WithEverySlotFullASymbolReplacesWhatIsOnThePlace)
{
	auto session = Open(Full());
	EXPECT_TRUE(Drop(session, 12, k_Blue, 2));
	EXPECT_EQ(session.slots.at(2), (Slot {.design = 12, .site = 2, .colour = k_Blue}));
}

TEST(TattooEditor, DroppingAwayFromThePlacesChangesNothing)
{
	auto session = Open({});
	EXPECT_FALSE(Drop(session, 5, k_Red, std::nullopt));
	EXPECT_EQ(session.slots, Slots {});
	EXPECT_FALSE(session.changed);
}

TEST(TattooEditor, LiftingTakesTheLastTattooOnThePlaceKeepingItsSymbol)
{
	auto session = Open({});
	ASSERT_TRUE(Drop(session, 5, k_Red, 3));
	ASSERT_TRUE(Drop(session, 6, k_Blue, 3));
	session.changed = false;
	const auto lifted = Lift(session, 3);
	ASSERT_TRUE(lifted.has_value());
	EXPECT_EQ(*lifted, (Slot {.design = 6, .site = 3, .colour = k_Blue}));
	EXPECT_EQ(session.slots.at(1).site, k_NoSite);
	EXPECT_EQ(session.slots.at(1).design, 6);
	EXPECT_EQ(session.slots.at(0).site, 3);
	EXPECT_TRUE(session.changed);
	EXPECT_FALSE(Lift(session, 7).has_value());
}

TEST(TattooEditor, OkWarnsOnlyInANetworkGameAfterAChange)
{
	auto session = Open({});
	EXPECT_EQ(Ok(session, true), Accept::Close);
	ASSERT_TRUE(Drop(session, 1, k_Red, 0));
	EXPECT_EQ(Ok(session, false), Accept::Close);
	EXPECT_EQ(Ok(session, true), Accept::Warn);
}

TEST(TattooEditor, ThePaletteCellSpansTheWholeControl)
{
	EXPECT_EQ(PaletteCell({5, 35}), glm::uvec2(0, 0));
	// 52 wide across 32 columns, 519 high across 128 rows
	EXPECT_EQ(PaletteCell({5 + 26, 35 + 260}), glm::uvec2(16, 64));
	EXPECT_EQ(PaletteCell({56, 553}), glm::uvec2(31, 127));
	EXPECT_EQ(PaletteCell({-100, -100}), glm::uvec2(0, 0));
	EXPECT_EQ(PaletteCell({900, 900}), glm::uvec2(31, 127));
}

TEST(TattooEditor, SlidersFollowThePointerWithinTheirHeight)
{
	EXPECT_FLOAT_EQ(SliderPosition(35, k_BrightnessRect), 0.0f);
	EXPECT_FLOAT_EQ(SliderPosition(554, k_BrightnessRect), 1.0f);
	EXPECT_FLOAT_EQ(SliderPosition(0, k_BrightnessRect), 0.0f);
	EXPECT_FLOAT_EQ(SliderPosition(1000, k_BrightnessRect), 1.0f);
	EXPECT_NEAR(SliderPosition(294, k_BrightnessRect), 259.0f / 519.0f, 1e-6f);
}

TEST(TattooEditor, ThePlaceUnderThePointerIsTheNearestFacingOne)
{
	const std::array sites {
	    SiteOnScreen {.site = 0, .screen = {100, 100}, .facingCamera = 0.9f},
	    SiteOnScreen {.site = 1, .screen = {110, 100}, .facingCamera = -0.5f},
	    SiteOnScreen {.site = 2, .screen = {130, 100}, .facingCamera = -0.1f},
	};
	EXPECT_EQ(SiteUnderPointer(sites, {112, 100}), 0);
	EXPECT_EQ(SiteUnderPointer(sites, {125, 100}), 2);
	// No further than 64 pixels
	EXPECT_EQ(SiteUnderPointer(sites, {100, 164}), 0);
	EXPECT_EQ(SiteUnderPointer(sites, {100, 165}), std::nullopt);
	EXPECT_EQ(SiteUnderPointer({}, {0, 0}), std::nullopt);
}

TEST(TattooEditor, SteeringSpeedsTheViewUpByHowFarThePointerIs)
{
	Orbit orbit;
	Steer(orbit, {100, 100}, {90, 120}, 10.0f);
	EXPECT_FLOAT_EQ(orbit.turnSpeed, 10.0f * 10.0f * 0.001f * 0.2f);
	EXPECT_FLOAT_EQ(orbit.tipSpeed, 20.0f * 10.0f * 0.001f * 0.1f);
}

TEST(TattooEditor, TheViewTurnsRoundItsFocusAndSlowsDown)
{
	Orbit orbit {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 1.0f, .tipSpeed = 0.0f};
	Turn(orbit, 100.0f);
	const auto angle = 1.0f * 100.0f * 0.0003f;
	EXPECT_NEAR(orbit.eye.x, 10.0f * std::cos(angle), 1e-5f);
	EXPECT_NEAR(orbit.eye.z, 10.0f * std::sin(angle), 1e-5f);
	EXPECT_NEAR(orbit.eye.y, 5.0f, 1e-5f);
	EXPECT_NEAR(orbit.turnSpeed, std::exp(100.0f * 0.0003f * -8.0f), 1e-6f);
}

TEST(TattooEditor, TheViewStaysBetweenAnEighthAndAsHighAsItIsFar)
{
	Orbit low {.eye = {10.0f, 0.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 0.0f};
	Turn(low, 10.0f);
	EXPECT_FLOAT_EQ(low.eye.y, 1.25f);
	Orbit high {.eye = {10.0f, 50.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 0.0f};
	Turn(high, 10.0f);
	EXPECT_FLOAT_EQ(high.eye.y, 10.0f);
	Orbit tipping {.eye = {10.0f, 5.0f, 0.0f}, .focus = {0.0f, 0.0f, 0.0f}, .turnSpeed = 0.0f, .tipSpeed = 1.0f};
	Turn(tipping, 100.0f);
	EXPECT_NEAR(tipping.eye.y, 5.0f + (10.0f * 100.0f * 0.0003f), 1e-5f);
}

TEST(TattooEditor, TheCaveLooksAtItsCreatureFromBesideAndAbove)
{
	const auto view = CaveView({10.0f, 0.0f, 20.0f}, 0.5f);
	EXPECT_EQ(view.focus, glm::vec3(10.0f, 3.75f, 20.0f));
	EXPECT_EQ(view.eye, glm::vec3(10.0f - 11.25f, 5.625f, 20.0f));
	EXPECT_EQ(CaveView({}, 0.01f).focus, glm::vec3(0.0f, 0.5f, 0.0f));
	EXPECT_EQ(CaveView({}, 5.0f).focus, glm::vec3(0.0f, 10.0f, 0.0f));
}

TEST(TattooEditor, TheCreatureStandsAlongTheCavesPlacesByItsSize)
{
	const glm::vec3 first {0.0f, 0.0f, 0.0f};
	const glm::vec3 third {8.0f, 4.0f, -2.0f};
	// Size 1 stands half way, size 2 or more at the first place, none at the third
	EXPECT_EQ(CavePlace(first, third, 1.0f, false), glm::vec3(4.0f, 2.0f, -1.0f));
	EXPECT_EQ(CavePlace(first, third, 2.5f, false), first);
	EXPECT_EQ(CavePlace(first, third, 0.0f, false), third);
	// The coin takes a small creature twice as far from the middle towards the first place
	EXPECT_EQ(CavePlace(first, third, 0.5f, true), glm::vec3(4.0f, 2.0f, -1.0f));
	EXPECT_EQ(CavePlace(first, third, 1.0f, true), first);
	// and doesn't count from a size of 1.25
	EXPECT_EQ(CavePlace(first, third, 1.25f, true), CavePlace(first, third, 1.25f, false));
}

TEST(TattooEditor, TheCopysRestTossesTheCoinThenTheRestsFlag)
{
	// A neutral creature makes no sparkles: both tosses are made, in that order, each of 2, whatever the size
	for (const float size : {0.5f, 1.25f, 4.0f})
	{
		std::vector<int32_t> asked;
		std::vector<uint32_t> answers {1, 0};
		const auto rest = DrawCopyRest(size, 0.2f, 40, [&](int32_t n) {
			asked.push_back(n);
			const auto answer = answers.front();
			answers.erase(answers.begin());
			return answer;
		});
		EXPECT_EQ(asked, (std::vector<int32_t> {2, 2}));
		EXPECT_FALSE(rest.restFlag);
		// the coin is kept only below 1.25
		EXPECT_EQ(rest.sleeping, size < 1.25f);
	}
	// the rest's flag is the second toss
	std::vector<uint32_t> answers {0, 1};
	const auto rest = DrawCopyRest(1.0f, -0.2f, 40, [&](int32_t) {
		const auto answer = answers.front();
		answers.erase(answers.begin());
		return answer;
	});
	EXPECT_FALSE(rest.sleeping);
	EXPECT_TRUE(rest.restFlag);
}

TEST(TattooEditor, TheCopysMomentsDrawTheirSparklesAroundTheRestsFlag)
{
	const auto drawn = [](float alignment) {
		std::vector<int32_t> asked;
		const auto rest = DrawCopyRest(1.0f, alignment, 40, [&](int32_t n) {
			asked.push_back(n);
			// the coin falls one way, every other draw the other
			return asked.size() == 1 ? 1u : 0u;
		});
		EXPECT_TRUE(rest.sleeping);
		return asked;
	};
	// good: in each of the two moments, three sparkles, each a place (three draws of 101), a bone, a colour of 6 and a
	// kind of 2; the rest's flag between them
	std::vector<int32_t> good {2};
	for (int moment = 0; moment < 2; ++moment)
	{
		for (int i = 0; i < 3; ++i)
		{
			good.insert(good.end(), {101, 101, 101, 40, 6, 2});
		}
		if (moment == 0)
		{
			good.push_back(2);
		}
	}
	EXPECT_EQ(drawn(0.21f), good);
	// held to 1 first, so any good creature over 0.2 makes them
	EXPECT_EQ(drawn(5.0f), good);
	// evil: the same without the kind
	std::vector<int32_t> evil {2};
	for (int moment = 0; moment < 2; ++moment)
	{
		for (int i = 0; i < 3; ++i)
		{
			evil.insert(evil.end(), {101, 101, 101, 40, 6});
		}
		if (moment == 0)
		{
			evil.push_back(2);
		}
	}
	EXPECT_EQ(drawn(-0.21f), evil);
	EXPECT_EQ(drawn(-5.0f), evil);
	// an alignment that is not a number is held to -1: evil
	EXPECT_EQ(drawn(std::numeric_limits<float>::quiet_NaN()), evil);
	// 0.2 and -0.2 themselves make none
	EXPECT_EQ(drawn(0.2f), (std::vector<int32_t> {2, 2}));
	EXPECT_EQ(drawn(-0.2f), (std::vector<int32_t> {2, 2}));
}

TEST(TattooEditor, TheCopyIsMadeOnlyAsTheRoomStartsAndOnlyWhenTheCreatureIsShown)
{
	// The first frame of a visit makes it when the room is to show the creature; any later frame never does
	EXPECT_TRUE(creature_tattoo_editor::MakesCopy(false, true));
	EXPECT_FALSE(creature_tattoo_editor::MakesCopy(false, false));
	EXPECT_FALSE(creature_tattoo_editor::MakesCopy(true, true));
	EXPECT_FALSE(creature_tattoo_editor::MakesCopy(true, false));
}

TEST(TattooEditor, TheCopyFacesTheFireAsTheOriginal)
{
	// The original's numbers for the way from the place to the fire, x, y, z, and the bits of the heading
	struct Case
	{
		glm::vec3 way;
		uint32_t heading;
	};
	const std::array cases {
	    Case {{1.0f, 0.0f, 0.0f}, 0x3FC90FDBu},
	    Case {{0.0f, 0.0f, 1.0f}, 0x40490FDBu},
	    Case {{-1.0f, 0.0f, 0.0f}, 0xBFC90FDBu},
	    Case {{0.0f, 0.0f, -1.0f}, 0x00000000u},
	    Case {{3.0f, 0.0f, 4.0f}, 0x401FE0BCu},
	    Case {{-3.0f, 0.0f, 4.0f}, 0xC01FE0BCu},
	    Case {{-3.0f, 0.0f, -4.0f}, 0xBF24BC7Du},
	    Case {{3.0f, 0.0f, -4.0f}, 0x3F24BC7Du},
	    Case {{0.0f, 0.0f, 0.0f}, 0x00000000u},
	    Case {{2.5f, 1.0f, std::bit_cast<float>(0xBF333333u)}, 0x3FA61DE8u},
	    Case {{std::bit_cast<float>(0xB8D1B717u), 0.0f, 1.0f}, 0xC0490E38u},
	    // too short a way to have a direction: none
	    Case {{std::bit_cast<float>(0x3A03126Fu), 0.0f, std::bit_cast<float>(0x3A378034u)}, 0x00000000u},
	    Case {{-12.75f, 0.0f, std::bit_cast<float>(0x3A83126Fu)}, 0xBFC9126Du},
	};
	for (const auto& c : cases)
	{
		SCOPED_TRACE(testing::Message() << "way " << c.way.x << " " << c.way.y << " " << c.way.z);
		EXPECT_EQ(std::bit_cast<uint32_t>(CopyHeading(glm::vec3(0.0f), c.way)), c.heading);
	}
	// the way is the fire less the place, each a float subtraction
	EXPECT_EQ(std::bit_cast<uint32_t>(CopyHeading({10.0f, 2.0f, -6.0f}, {13.0f, 2.0f, -2.0f})), 0x401FE0BCu);
}

TEST(TattooEditor, APlaceIsFoundWhereItsTriangleHoldsItOnTheSkin)
{
	// The second vertex is half the skin to the right of the first, the third half the skin below it
	const std::array<TexturedVertex, 3> vertices {TexturedVertex {.uv = {0.5f, 0.5f}, .bone = 3},
	                                              TexturedVertex {.uv = {1.0f, 0.5f}, .bone = 4},
	                                              TexturedVertex {.uv = {0.5f, 1.0f}, .bone = 5}};
	const Triangles triangles {{0, 1, 2}};
	const std::array primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
	const auto found = FindSiteTriangle(primitives, At(192, 160));
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->primitive, 0u);
	EXPECT_EQ(found->vertices, (std::array<uint16_t, 3> {0, 1, 2}));
	EXPECT_EQ(found->bones, (std::array<uint32_t, 3> {3, 4, 5}));
	EXPECT_FLOAT_EQ(found->u, 0.5f);
	EXPECT_FLOAT_EQ(found->v, 0.25f);

	// A slanted triangle: the centre of (0, 0), (0.5, 0.25) and (0.25, 0.5) is a third of the way to each
	const std::array<TexturedVertex, 3> slanted {TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 0},
	                                             TexturedVertex {.uv = {0.5f, 0.25f}, .bone = 0},
	                                             TexturedVertex {.uv = {0.25f, 0.5f}, .bone = 0}};
	const std::array slantedPrimitives {TexturedPrimitive {.skin = 1, .vertices = slanted, .triangles = triangles}};
	const auto centre = FindSiteTriangle(slantedPrimitives, At(64, 64));
	ASSERT_TRUE(centre.has_value());
	EXPECT_FLOAT_EQ(centre->u, 1.0f / 3.0f);
	EXPECT_FLOAT_EQ(centre->v, 1.0f / 3.0f);
}

TEST(TattooEditor, APlaceOnATrianglesEdgeOrCornerIsOnIt)
{
	const std::array<TexturedVertex, 3> vertices {TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {0.5f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {0.0f, 0.5f}, .bone = 0}};
	const Triangles triangles {{0, 1, 2}};
	const std::array primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
	// On the edge from the second vertex to the third
	const auto edge = FindSiteTriangle(primitives, At(64, 64));
	ASSERT_TRUE(edge.has_value());
	EXPECT_FLOAT_EQ(edge->u, 0.5f);
	EXPECT_FLOAT_EQ(edge->v, 0.5f);
	// On the second vertex, and on the first
	const auto corner = FindSiteTriangle(primitives, At(128, 0));
	ASSERT_TRUE(corner.has_value());
	EXPECT_FLOAT_EQ(corner->u, 1.0f);
	EXPECT_FLOAT_EQ(corner->v, 0.0f);
	EXPECT_TRUE(FindSiteTriangle(primitives, At(0, 0)).has_value());
	// Just past the edge
	EXPECT_FALSE(FindSiteTriangle(primitives, At(64, 65)).has_value());
}

TEST(TattooEditor, ATriangleFlatOnTheSkinHoldsNoPlace)
{
	// The first triangle's coordinates lie on a line through the place, the second's make a triangle a 128th of the
	// skin across, too small, and the third's one a 64th across
	const std::array<TexturedVertex, 7> vertices {
	    TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 0},          TexturedVertex {.uv = {0.5f, 0.0f}, .bone = 0},
	    TexturedVertex {.uv = {1.0f, 0.0f}, .bone = 0},          TexturedVertex {.uv = {1.0f / 128.0f, 0.0f}, .bone = 0},
	    TexturedVertex {.uv = {0.0f, 1.0f / 128.0f}, .bone = 0}, TexturedVertex {.uv = {1.0f / 64.0f, 0.0f}, .bone = 0},
	    TexturedVertex {.uv = {0.0f, 1.0f / 64.0f}, .bone = 0},
	};
	const Triangles flat {{0, 1, 2}};
	const Triangles small {{0, 3, 4}, {0, 5, 6}};
	const std::array onALine {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = flat}};
	EXPECT_FALSE(FindSiteTriangle(onALine, At(0, 0)).has_value());
	const std::array primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = flat},
	                             TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = small}};
	const auto found = FindSiteTriangle(primitives, At(0, 0));
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->primitive, 1u);
	EXPECT_EQ(found->vertices, (std::array<uint16_t, 3> {0, 5, 6}));
}

TEST(TattooEditor, APlaceIsFoundOnlyOnItsOwnSkin)
{
	const std::array<TexturedVertex, 3> vertices {TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {1.0f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {0.0f, 1.0f}, .bone = 0}};
	const Triangles triangles {{0, 1, 2}};
	const std::array others {TexturedPrimitive {.skin = 0, .vertices = vertices, .triangles = triangles},
	                         TexturedPrimitive {.skin = std::nullopt, .vertices = vertices, .triangles = triangles},
	                         TexturedPrimitive {.skin = 2, .vertices = vertices, .triangles = triangles}};
	EXPECT_FALSE(FindSiteTriangle(others, At(10, 10)).has_value());
	const std::array withItsOwn {others[0], others[1], others[2],
	                             TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
	const auto found = FindSiteTriangle(withItsOwn, At(10, 10));
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->primitive, 3u);
}

TEST(TattooEditor, TheFirstTriangleHoldingAPlaceIsTaken)
{
	// The two halves of the skin, the same in a primitive and in the next
	const std::array<TexturedVertex, 4> vertices {
	    TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 1}, TexturedVertex {.uv = {1.0f, 0.0f}, .bone = 2},
	    TexturedVertex {.uv = {0.0f, 1.0f}, .bone = 3}, TexturedVertex {.uv = {1.0f, 1.0f}, .bone = 4}};
	const Triangles triangles {{3, 2, 1}, {0, 1, 2}};
	const std::array primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles},
	                             TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
	// (0.25, 0.25) is only in the second triangle, of the first primitive
	const auto inSecond = FindSiteTriangle(primitives, At(64, 64));
	ASSERT_TRUE(inSecond.has_value());
	EXPECT_EQ(inSecond->primitive, 0u);
	EXPECT_EQ(inSecond->vertices, (std::array<uint16_t, 3> {0, 1, 2}));
	EXPECT_EQ(inSecond->bones, (std::array<uint32_t, 3> {1, 2, 3}));
	// (0.75, 0.25) is on the edge both triangles share, so the first takes it
	const auto shared = FindSiteTriangle(primitives, At(192, 64));
	ASSERT_TRUE(shared.has_value());
	EXPECT_EQ(shared->primitive, 0u);
	EXPECT_EQ(shared->vertices, (std::array<uint16_t, 3> {3, 2, 1}));
	EXPECT_FLOAT_EQ(shared->u, 0.25f);
	EXPECT_FLOAT_EQ(shared->v, 0.75f);
}

TEST(TattooEditor, APlaceNoTriangleHoldsIsNotFound)
{
	const std::array<TexturedVertex, 3> vertices {TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {0.25f, 0.0f}, .bone = 0},
	                                              TexturedVertex {.uv = {0.0f, 0.25f}, .bone = 0}};
	const Triangles triangles {{0, 1, 2}};
	const std::array primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
	EXPECT_FALSE(FindSiteTriangle(primitives, At(200, 200)).has_value());
	EXPECT_FALSE(FindSiteTriangle({}, At(0, 0)).has_value());
}

TEST(TattooEditor, APointIsProjectedToPixelsFromTheTopLeft)
{
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.0f, 0.0f, 2.0f}), glm::ivec2(320, 240));
	// Up is up the screen
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {1.0f, 0.5f, 2.0f}), glm::ivec2(480, 180));
	// Cut towards zero on both sides of it
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.003f, -0.003f, 1.0f}), glm::ivec2(320, 240));
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {-2.003f, 0.0f, 1.0f}), glm::ivec2(-320, 240));
}

TEST(TattooEditor, APointIsProjectedOffTheScreenButNotFromNearerThanTheNearPlane)
{
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {4.0f, 0.0f, 2.0f}), glm::ivec2(960, 240));
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.0f, 0.0f, 1.0f}), glm::ivec2(320, 240));
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.0f, 0.0f, 0.99f}), std::nullopt);
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.0f, 0.0f, -2.0f}), std::nullopt);
	EXPECT_EQ(ProjectToPixels(AlongZ(), 1.0f, k_Screen, {0.0f, 0.0f, std::nanf("")}), std::nullopt);
}

TEST(TattooEditor, APlaceOnScreenFacesTheCameraByItsInwardNormal)
{
	const std::array points {
	    // Its inward normal points away from the eye, of any length: it faces the camera
	    SitePoint {.site = 0, .position = {0.0f, 0.0f, 4.0f}, .inwardNormal = {0.0f, 0.0f, 5.0f}},
	    // Turned away
	    SitePoint {.site = 1, .position = {0.0f, 0.0f, 4.0f}, .inwardNormal = {0.0f, 0.0f, -0.1f}},
	    // Side on, the way from the eye slanted
	    SitePoint {.site = 2, .position = {2.0f, 0.0f, 2.0f}, .inwardNormal = {0.0f, 3.0f, 0.0f}},
	    SitePoint {.site = 3, .position = {2.0f, 0.0f, 2.0f}, .inwardNormal = {1.0f, 0.0f, 0.0f}},
	    // No normal
	    SitePoint {.site = 4, .position = {0.0f, 0.0f, 4.0f}, .inwardNormal = {0.0f, 0.0f, 0.0f}},
	};
	const auto sites = SitesOnScreen(points, ViewAlongZ());
	ASSERT_EQ(sites.size(), points.size());
	for (size_t i = 0; i < sites.size(); ++i)
	{
		EXPECT_EQ(sites[i].site, points[i].site);
	}
	EXPECT_FLOAT_EQ(sites[0].facingCamera, 1.0f);
	EXPECT_EQ(sites[0].screen, glm::ivec2(320, 240));
	EXPECT_FLOAT_EQ(sites[1].facingCamera, -1.0f);
	EXPECT_FLOAT_EQ(sites[2].facingCamera, 0.0f);
	EXPECT_NEAR(sites[3].facingCamera, std::sqrt(0.5f), 1e-6f);
	EXPECT_EQ(sites[3].screen, glm::ivec2(640, 240));
	EXPECT_FLOAT_EQ(sites[4].facingCamera, 0.0f);
}

TEST(TattooEditor, OnlyAPlaceNearerThanTheNearPlaneIsLeftOut)
{
	const std::array points {
	    SitePoint {.site = 5, .position = {0.0f, 0.0f, 0.5f}, .inwardNormal = {0.0f, 0.0f, 1.0f}},
	    SitePoint {.site = 6, .position = {30.0f, 0.0f, 2.0f}, .inwardNormal = {0.0f, 0.0f, 1.0f}},
	};
	const auto sites = SitesOnScreen(points, ViewAlongZ());
	ASSERT_EQ(sites.size(), 1u);
	EXPECT_EQ(sites[0].site, 6);
	EXPECT_EQ(sites[0].screen, glm::ivec2(5120, 240));
}

TEST(TattooEditor, APlaceTurnedALittlePastSideOnStillCounts)
{
	// The way from the eye is down z; the normals are turned past side on by a cosine of -0.19 and -0.21
	const auto turned = [](float cosine) { return glm::vec3(std::sqrt(1.0f - (cosine * cosine)), 0.0f, cosine); };
	const std::array points {
	    SitePoint {.site = 0, .position = {0.0f, 0.0f, 4.0f}, .inwardNormal = turned(-0.21f)},
	    SitePoint {.site = 1, .position = {0.0f, 0.0f, 4.0f}, .inwardNormal = turned(-0.19f)},
	};
	const auto sites = SitesOnScreen(points, ViewAlongZ());
	ASSERT_EQ(sites.size(), 2u);
	EXPECT_NEAR(sites[0].facingCamera, -0.21f, 1e-6f);
	EXPECT_NEAR(sites[1].facingCamera, -0.19f, 1e-6f);
	EXPECT_EQ(SiteUnderPointer(sites, {320, 240}), 1);
	EXPECT_EQ(SiteUnderPointer(std::span(sites).first(1), {320, 240}), std::nullopt);
}

TEST(TattooEditor, TheCopyMovesOnByTheFramesRealTimeHeldTo200)
{
	EXPECT_EQ(CopyFrameMs(1), 1u);
	EXPECT_EQ(CopyFrameMs(33), 33u);
	EXPECT_EQ(CopyFrameMs(200), 200u);
	EXPECT_EQ(CopyFrameMs(201), 200u);
	EXPECT_EQ(CopyFrameMs(5000), 200u);
}

TEST(TattooEditor, TheCopysRestIsTheSleepOrTheSitAroundItsLoop)
{
	namespace animations = creature_layers::animations;
	// static action 0x1D plays 28, 29 and 30, and 0x26 plays 37, 38 and 39
	EXPECT_EQ(CopyRestAnimations(true), (std::array<size_t, 3> {28, 29, 30}));
	EXPECT_EQ(CopyRestAnimations(true),
	          (std::array<size_t, 3> {animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep}));
	EXPECT_EQ(CopyRestAnimations(false), (std::array<size_t, 3> {37, 38, 39}));
	EXPECT_EQ(CopyRestAnimations(false),
	          (std::array<size_t, 3> {animations::k_StartSit, animations::k_Sit, animations::k_EndSit}));
	// the yawn it may start while sitting is concurrent action 0xCA
	EXPECT_EQ(creature_layers::animations::k_Yawn, 0xCAu);
}

TEST(TattooEditor, TheCopyYawnsOneDrawnFrameInAThousandOnlyWhileSittingAndNotInTheReflection)
{
	std::vector<int32_t> asked;
	uint32_t answer = 0;
	const auto localRand = [&](int32_t n) {
		asked.push_back(n);
		return answer;
	};
	// sitting, in the room's own pass: one draw of 1000, a yawn only on 23
	answer = 23;
	EXPECT_TRUE(CopyStartsYawn(false, false, localRand));
	answer = 22;
	EXPECT_FALSE(CopyStartsYawn(false, false, localRand));
	answer = 24;
	EXPECT_FALSE(CopyStartsYawn(false, false, localRand));
	EXPECT_EQ(asked, (std::vector<int32_t> {1000, 1000, 1000}));
	// asleep, or drawn in the main room's reflection: no draw at all
	asked.clear();
	answer = 23;
	EXPECT_FALSE(CopyStartsYawn(true, false, localRand));
	EXPECT_FALSE(CopyStartsYawn(false, true, localRand));
	EXPECT_FALSE(CopyStartsYawn(true, true, localRand));
	EXPECT_TRUE(asked.empty());
}

TEST(TattooEditor, EachMomentOfTheCopyDrawsItsSparklesAsTheRoomsStartDoes)
{
	const auto drawn = [](float alignment) {
		std::vector<int32_t> asked;
		DrawCopyMoment(alignment, 12, [&](int32_t n) {
			asked.push_back(n);
			return 0u;
		});
		return asked;
	};
	std::vector<int32_t> good;
	std::vector<int32_t> evil;
	for (int i = 0; i < 3; ++i)
	{
		good.insert(good.end(), {101, 101, 101, 12, 6, 2});
		evil.insert(evil.end(), {101, 101, 101, 12, 6});
	}
	EXPECT_EQ(drawn(0.5f), good);
	EXPECT_EQ(drawn(-0.5f), evil);
	EXPECT_TRUE(drawn(0.0f).empty());
}

TEST(TattooEditor, TheCopysColourIsItsGreyInTheTemplesLightThenFaded)
{
	// a neutral temple's light is white: 0xE0 x 0xFF >> 8 = 0xDF, the alpha 0xFE; then 255/256 of that before the fade
	EXPECT_EQ(CopyColour(0xFFFFFFFFu, 0.0f, temple_fade::k_CreatureRoom), 0xFEDEDEDEu);
	// fully faded, 100/256: 0xDF x 100 >> 8 = 0x57
	EXPECT_EQ(CopyColour(0xFFFFFFFFu, 500.0f, temple_fade::k_CreatureRoom), 0xFE575757u);
	// the temple's light is the same product as TempleLight::Colour, channel by channel
	for (const float alignment : {-1.0f, -0.5f, 0.0f, 0.3f, 1.0f})
	{
		for (const uint32_t milliseconds : {0u, 1000u, 2500u})
		{
			const auto light = TempleLight::At(alignment, milliseconds);
			const auto byte = [](float channel) { return static_cast<uint32_t>(std::lround(channel * 255.0f)); };
			const auto citadel =
			    argb_colour::Argb(byte(light.multiply.r), byte(light.multiply.g), byte(light.multiply.b), 0xFF);
			const auto lit = light.Colour(glm::vec3(0xE0 / 255.0f));
			const auto product = argb_colour::MultiplyArgbShift8(k_CopyColour, citadel);
			EXPECT_EQ(byte(lit.r), argb_colour::Red(product));
			EXPECT_EQ(byte(lit.g), argb_colour::Green(product));
			EXPECT_EQ(byte(lit.b), argb_colour::Blue(product));
			// and the copy's colour, unfaded, is that product at 255/256
			const auto colour = CopyColour(citadel, 0.0f, temple_fade::k_CreatureRoom);
			EXPECT_EQ(argb_colour::Red(colour), (argb_colour::Red(product) * 255u) >> 8);
			EXPECT_EQ(argb_colour::Alpha(colour), 0xFEu);
		}
	}
}

namespace
{
/// The spins' lengths of a species that has them all
std::optional<int32_t> SpinLengths(size_t animation)
{
	const std::map<size_t, int32_t> lengths {{3, 1000}, {4, 1200}, {5, 1600}, {6, 1100}, {7, 1300}, {8, 1700}};
	const auto found = lengths.find(animation);
	return found != lengths.end() ? std::optional(found->second) : std::nullopt;
}

/// The copy sitting by the fire, in its loop, and the room's frame it goes by with the editor closed, facing the fire
struct CopyAtRest
{
	creature_layers::BodyAction body {*creature_layers::PlaySequence({}, 37, 38, 39)};
	creature_layers::GestureLayer gesture;
	std::optional<glm::vec3> lookAt;
	/// The editor's next idle action far off
	CopyLife life {.idleMs = 1'000'000};
	CopyFrame frame {.place = {0.0f, 0.0f, 0.0f}, .fire = {0.0f, 0.0f, -5.0f}};
	std::vector<int32_t> draws;
	/// What the local stream gives, in turn, and 1 once they are used up
	std::vector<uint32_t> given;

	CopyAtRest()
	{
		body.phase = creature_layers::BodyAction::Phase::Loop;
		frame.heading = static_cast<float>(affine::GetYAngle(frame.fire - frame.place));
	}
	CopyStepped Step()
	{
		return CopyStep(
		    life, body, gesture, lookAt, frame, [](size_t) { return true; },
		    [this](int32_t n) {
			    draws.push_back(n);
			    if (given.empty())
			    {
				    return 1u;
			    }
			    const auto value = given.front();
			    given.erase(given.begin());
			    return value;
		    });
	}
	/// The body's action played to its end
	void Ends() { body = {}; }
};
} // namespace

TEST(TattooEditor, TheCopysTurnPlaysTheSpinsOfItsSideBlendedByTheAngle)
{
	// the heading's way round, under a quarter turn: the first two of 6, 7 and 8, by the angle over a quarter turn, for a
	// length between theirs rounded to the nearest (1100 + 200 x 0.3183 = 1163.66)
	auto turn = StartCopyTurn(0.0f, 0.5f, SpinLengths);
	ASSERT_TRUE(turn.spins.has_value());
	EXPECT_EQ(turn.target, 0.5f);
	EXPECT_EQ(turn.spins->from, 6u);
	EXPECT_EQ(turn.spins->to, 7u);
	EXPECT_EQ(turn.spins->weight, 0.5f * 0.636619747f);
	EXPECT_EQ(turn.spins->durationMs, 1164);
	EXPECT_EQ(turn.timeMs, 0);
	// the other way round, beyond a quarter turn: the last two of 3, 4 and 5 (1200 + 400 x 0.2732 = 1309.30)
	turn = StartCopyTurn(0.0f, -2.0f, SpinLengths);
	ASSERT_TRUE(turn.spins.has_value());
	EXPECT_EQ(turn.spins->from, 4u);
	EXPECT_EQ(turn.spins->to, 5u);
	EXPECT_EQ(turn.spins->weight, (2.0f - 1.57079637f) * 0.636619747f);
	EXPECT_EQ(turn.spins->durationMs, 1309);
	// the angle is taken within half a turn: from 3 to -3 is 0.28 the heading's way round
	turn = StartCopyTurn(3.0f, -3.0f, SpinLengths);
	ASSERT_TRUE(turn.spins.has_value());
	EXPECT_EQ(turn.spins->from, 6u);
	EXPECT_EQ(turn.spins->durationMs, 1136);
	// no turn at all goes the other way, by none
	turn = StartCopyTurn(1.0f, 1.0f, SpinLengths);
	ASSERT_TRUE(turn.spins.has_value());
	EXPECT_EQ(turn.spins->from, 3u);
	EXPECT_EQ(turn.spins->weight, 0.0f);
	EXPECT_EQ(turn.spins->durationMs, 1000);
	// a species without one of the side's three spins has none
	const auto withoutLast = [](size_t animation) { return animation == 8 ? std::nullopt : SpinLengths(animation); };
	EXPECT_FALSE(StartCopyTurn(0.0f, 0.5f, withoutLast).spins.has_value());
	EXPECT_TRUE(StartCopyTurn(0.0f, -0.5f, withoutLast).spins.has_value());
}

TEST(TattooEditor, TheCopyKeepsItsHeadingUntilItsSpinsEndThenFacesTheTurnsWay)
{
	std::optional<CopyTurn> turn = StartCopyTurn(0.0f, 0.5f, SpinLengths);
	float heading = 0.0f;
	for (int32_t played = 100; played < 1164; played += 100)
	{
		StepCopyTurn(turn, heading, 100, 2000);
		ASSERT_TRUE(turn.has_value());
		EXPECT_EQ(turn->timeMs, played);
		EXPECT_EQ(heading, 0.0f);
	}
	// the step that reaches the blend's length ends the spins and gives the heading
	StepCopyTurn(turn, heading, 100, 2000);
	ASSERT_TRUE(turn.has_value());
	EXPECT_FALSE(turn->spins.has_value());
	EXPECT_EQ(turn->timeMs, 0);
	EXPECT_EQ(heading, 0.5f);
	// and the next moment finds it facing the way: the turn is over
	StepCopyTurn(turn, heading, 100, 2000);
	EXPECT_FALSE(turn.has_value());
	EXPECT_EQ(heading, 0.5f);
}

TEST(TattooEditor, ATurnWithinATenThousandthIsOverAtItsFirstMoment)
{
	std::optional<CopyTurn> turn = StartCopyTurn(1.0f, 1.00005f, SpinLengths);
	float heading = 1.0f;
	StepCopyTurn(turn, heading, 16, 2000);
	EXPECT_FALSE(turn.has_value());
	EXPECT_EQ(heading, 1.00005f);
}

TEST(TattooEditor, WithoutSpinsTheCopysHeadingComesRoundOverItsStandsLength)
{
	std::optional<CopyTurn> turn = StartCopyTurn(0.0f, 1.0f, [](size_t) { return std::optional<int32_t> {}; });
	ASSERT_TRUE(turn.has_value());
	ASSERT_FALSE(turn->spins.has_value());
	float heading = 0.0f;
	// each moment the share of the stand's 1000 ms gone before it, of what is left of the turn
	const std::array expected {0.0f, 0.1f, 0.28f, 0.49600002f, 0.6976f, 0.8488f, 0.93952f, 0.981856f, 0.9963712f, 1.0f};
	for (const auto value : expected)
	{
		StepCopyTurn(turn, heading, 100, 1000);
		ASSERT_TRUE(turn.has_value());
		EXPECT_EQ(heading, value);
	}
	StepCopyTurn(turn, heading, 100, 1000);
	EXPECT_FALSE(turn.has_value());
}

TEST(TattooEditor, TheCopysAnimationsMoveOnMoreSlowlyTheBiggerItIs)
{
	EXPECT_EQ(CopyAnimationStep(1.0f, 16), 18);
	EXPECT_EQ(CopyAnimationStep(1.25f, 200), 213);
	EXPECT_EQ(CopyAnimationStep(0.05f, 33), 52);
	// the biggest goes back, cut towards zero
	EXPECT_EQ(CopyAnimationStep(4.0f, 100), -10);
}

TEST(TattooEditor, TheEditorsFirstFrameGetsTheCopyUpToTurnToTheEditorsEye)
{
	CopyAtRest copy;
	copy.frame.editorOpen = true;
	copy.frame.eye = {-3.0f, 1.0f, 0.0f};
	copy.frame.focus = {0.0f, 0.5f, 0.0f};
	auto stepped = copy.Step();
	// it gets up first: the turn waits. The editor's idle wait is drawn
	EXPECT_TRUE(copy.body.endWanted);
	EXPECT_FALSE(stepped.turn.has_value());
	EXPECT_TRUE(copy.life.turnWanted);
	EXPECT_TRUE(copy.life.editorHadCopy);
	EXPECT_EQ(copy.draws, std::vector<int32_t>({20000}));
	// once it stands, the turn to the eye starts, once
	copy.Ends();
	stepped = copy.Step();
	ASSERT_TRUE(stepped.turn.has_value());
	EXPECT_EQ(*stepped.turn, static_cast<float>(affine::GetYAngle(glm::vec3(-3.0f, 0.5f, 0.0f))));
	EXPECT_FALSE(copy.Step().turn.has_value());
	// the editor's later frames don't get it up again
	copy.body = *creature_layers::PlaySequence({}, 37, 38, 39);
	static_cast<void>(copy.Step());
	EXPECT_FALSE(copy.body.endWanted);
	EXPECT_EQ(copy.draws.size(), 1u);
}

TEST(TattooEditor, TheCopyGetsUpByTheWholeStepOfTheNextFrame)
{
	// The editor's first frame gets the sitting copy up after the frame's moment; the next frame's moment starts its
	// getting up and plays it on by that moment's step, at the rate of its size
	CopyAtRest copy;
	copy.frame.editorOpen = true;
	copy.frame.eye = {-3.0f, 1.0f, 0.0f};
	copy.frame.focus = {0.0f, 0.5f, 0.0f};
	static_cast<void>(copy.Step());
	ASSERT_TRUE(copy.body.endWanted);
	const auto step = 16.0f * creature_layers::PlaybackRate(1.0f);
	copy.body = creature_layers::AdvanceBody(copy.body, step, {1000, 1000, 1200});
	EXPECT_EQ(copy.body.phase, creature_layers::BodyAction::Phase::End);
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), 39u);
	EXPECT_FLOAT_EQ(copy.body.timeMs, step);
}

TEST(TattooEditor, TheCopysRestMovesItsLoopOnByTheWholeStepOfTheFrameItsStartEnds)
{
	// The copy's sit down of 1000 ms, at 990: the frame's moment of 18 ms ends it, and the sit moves on by all 18
	auto body = *creature_layers::PlaySequence({}, 37, 38, 39);
	body.timeMs = 990.0f;
	body = creature_layers::AdvanceBody(body, 18.0f, {1000, 2000, 1200});
	EXPECT_TRUE(creature_layers::IsLooping(body));
	EXPECT_FLOAT_EQ(body.timeMs, 18.0f);
}

TEST(TattooEditor, WithTheEditorOpenTheCopyLooksAtThePointerForTheFirstSecondAndAHalfOfAPress)
{
	CopyAtRest copy;
	copy.frame.editorOpen = true;
	copy.life.editorHadCopy = true;
	copy.frame.pointer = {1.0f, 2.0f, 3.0f};
	copy.lookAt = copy.frame.fire;
	// no button: it looks nowhere
	copy.frame.ticks = 500;
	static_cast<void>(copy.Step());
	EXPECT_FALSE(copy.lookAt.has_value());
	copy.frame.buttonHeld = true;
	copy.frame.ticks = 1000;
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.lookAt, copy.frame.pointer);
	copy.frame.ticks = 2499;
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.lookAt, copy.frame.pointer);
	copy.frame.ticks = 2500;
	static_cast<void>(copy.Step());
	EXPECT_FALSE(copy.lookAt.has_value());
	// let go and pressed again, it looks again from the new press
	copy.frame.buttonHeld = false;
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.life.heldSince, 0u);
	copy.frame.buttonHeld = true;
	copy.frame.ticks = 4000;
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.lookAt, copy.frame.pointer);
	EXPECT_EQ(copy.life.heldSince, 4000u);
	EXPECT_TRUE(copy.draws.empty());
}

TEST(TattooEditor, ClosedAndFacingTheFireTheCopyRestsAgainDrawingItsFlagEveryFrame)
{
	CopyAtRest copy;
	copy.Ends();
	auto stepped = copy.Step();
	// a draw of 2 for the rest's flag; it sits again, looking into the fire
	EXPECT_EQ(copy.draws, std::vector<int32_t>({2}));
	EXPECT_EQ(stepped.restFlag, true);
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), 37u);
	EXPECT_EQ(copy.lookAt, copy.frame.fire);
	EXPECT_FALSE(stepped.turn.has_value());
	// while it plays its rest, a draw each frame and no new rest
	copy.body.timeMs = 120.0f;
	stepped = copy.Step();
	EXPECT_EQ(copy.draws.size(), 2u);
	EXPECT_FALSE(stepped.restFlag.has_value());
	EXPECT_EQ(copy.body.timeMs, 120.0f);
	// nor while it turns, or when its species lacks one of the three animations
	copy.Ends();
	copy.frame.turning = true;
	EXPECT_FALSE(copy.Step().restFlag.has_value());
	copy.frame.turning = false;
	const auto lacking = CopyStep(
	    copy.life, copy.body, copy.gesture, copy.lookAt, copy.frame, [](size_t index) { return index != 39; },
	    [&copy](int32_t n) {
		    copy.draws.push_back(n);
		    return 0u;
	    });
	EXPECT_FALSE(lacking.restFlag.has_value());
	EXPECT_FALSE(creature_layers::IsPlaying(copy.body));
	EXPECT_EQ(copy.draws.size(), 4u);
}

TEST(TattooEditor, AsleepTheCopyRestsAgainWithoutLookingIntoTheFire)
{
	CopyAtRest copy;
	copy.Ends();
	copy.frame.sleeping = true;
	static_cast<void>(copy.Step());
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), 28u);
	EXPECT_FALSE(copy.lookAt.has_value());
	EXPECT_EQ(copy.draws.size(), 1u);
}

TEST(TattooEditor, ClosedAndNotFacingTheFireTheCopyGetsUpAndTurnsToIt)
{
	CopyAtRest copy;
	const auto facing = copy.frame.heading;
	// 0.01 off is not facing it
	copy.frame.heading = facing + 0.01f;
	auto stepped = copy.Step();
	EXPECT_TRUE(copy.draws.empty());
	EXPECT_TRUE(copy.body.endWanted);
	EXPECT_FALSE(stepped.turn.has_value());
	copy.Ends();
	stepped = copy.Step();
	ASSERT_TRUE(stepped.turn.has_value());
	EXPECT_EQ(*stepped.turn, facing);
	// while it turns no other turn starts; just under 0.01 off it rests
	copy.frame.turning = true;
	EXPECT_FALSE(copy.Step().turn.has_value());
	copy.frame.turning = false;
	copy.life.turnWanted = false;
	copy.frame.heading = facing + 0.0099f;
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws.size(), 1u);
	EXPECT_TRUE(creature_layers::IsPlaying(copy.body));
}

TEST(TattooEditor, ATurnToTheEyeNotStartedWhenTheEditorClosesGoesToTheFire)
{
	CopyAtRest copy;
	copy.frame.editorOpen = true;
	copy.frame.eye = {-3.0f, 1.0f, 0.0f};
	static_cast<void>(copy.Step());
	ASSERT_TRUE(copy.life.turnWanted);
	// closed while it is still getting up: the turn wanted is now to the fire, which it already faces, so it sits down
	// again and the turn waits on
	copy.frame.editorOpen = false;
	copy.Ends();
	const auto stepped = copy.Step();
	EXPECT_FALSE(stepped.turn.has_value());
	EXPECT_TRUE(copy.life.turnWanted);
	EXPECT_EQ(copy.life.turnHeading, copy.frame.heading);
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), 37u);
	EXPECT_FALSE(copy.life.editorHadCopy);
}

TEST(TattooEditor, EachOpeningOfTheEditorStartsTheIdleWaitAgain)
{
	// what an earlier opening left: a wait run out and five actions
	CopyAtRest copy;
	copy.life = {.idleMs = -40, .idles = 5};
	copy.frame.editorOpen = true;
	copy.frame.frameMs = 16;
	copy.given = {7000};
	static_cast<void>(copy.Step());
	// shown, the editor draws the wait, 20000 + 7000, and the frame's time comes off it: no action yet
	EXPECT_EQ(copy.draws, std::vector<int32_t>({20000}));
	EXPECT_EQ(copy.life.idles, 0);
	EXPECT_EQ(copy.life.idleMs, 27000 - 16);
	EXPECT_TRUE(copy.body.endWanted);
	EXPECT_TRUE(creature_layers::IsLooping(copy.body));
	// the next frames take their time off it, with no draw
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.life.idleMs, 27000 - 32);
	EXPECT_EQ(copy.draws.size(), 1u);
}

TEST(TattooEditor, WhenTheWaitRunsOutTheCopyShowsOffScratchesOrYawns)
{
	CopyAtRest copy;
	copy.Ends();
	copy.frame.editorOpen = true;
	copy.life.editorHadCopy = true;
	copy.life.idleMs = 40;
	copy.frame.frameMs = 20;
	// 40 - 20 is not yet run out
	static_cast<void>(copy.Step());
	EXPECT_TRUE(copy.draws.empty());
	EXPECT_EQ(copy.life.idleMs, 20);
	// run out at none: the show-off, played once, with its flag's draw
	copy.given = {0, 1, 500};
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws, std::vector<int32_t>({3, 2, 20000}));
	EXPECT_EQ(copy.body.kind, creature_layers::BodyAction::Kind::Once);
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), k_CopyIdleShowOff);
	EXPECT_EQ(copy.life.idleMs, 20500);
	// the scratch, refused while the show-off plays, its flag drawn all the same
	copy.draws.clear();
	copy.life.idleMs = 0;
	copy.given = {1, 0, 0};
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws, std::vector<int32_t>({3, 2, 20000}));
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), k_CopyIdleShowOff);
	// the yawn, on top of whatever plays, with no flag
	copy.draws.clear();
	copy.life.idleMs = 0;
	copy.given = {2, 0};
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws, std::vector<int32_t>({3, 20000}));
	EXPECT_EQ(copy.gesture.animation, creature_layers::animations::k_Yawn);
	EXPECT_EQ(copy.life.idles, 3);
}

TEST(TattooEditor, FromTheTenthIdleActionTheCopySitsDownWhenItIsFree)
{
	CopyAtRest copy;
	copy.Ends();
	copy.frame.editorOpen = true;
	copy.life.editorHadCopy = true;
	copy.life.idles = 9;
	copy.life.idleMs = 0;
	copy.given = {1, 0};
	auto stepped = copy.Step();
	// no pick of three: the sit, with its flag
	EXPECT_EQ(copy.draws, std::vector<int32_t>({2, 20000}));
	EXPECT_EQ(stepped.restFlag, true);
	EXPECT_EQ(creature_layers::CurrentAnimation(copy.body), 37u);
	EXPECT_FALSE(copy.lookAt.has_value());
	// busy, the eleventh draws only the next wait
	copy.draws.clear();
	copy.life.idleMs = 0;
	stepped = copy.Step();
	EXPECT_EQ(copy.draws, std::vector<int32_t>({20000}));
	EXPECT_FALSE(stepped.restFlag.has_value());
	EXPECT_EQ(copy.life.idles, 11);
}

TEST(TattooEditor, TheLeftButtonHeldStartsTheIdleWaitAgainAndEndsTheCopysRest)
{
	CopyAtRest copy;
	copy.frame.editorOpen = true;
	copy.life.editorHadCopy = true;
	copy.life.idles = 4;
	copy.frame.leftHeld = true;
	copy.given = {300};
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws, std::vector<int32_t>({20000}));
	EXPECT_EQ(copy.life.idleMs, 20300);
	EXPECT_EQ(copy.life.idles, 0);
	EXPECT_TRUE(copy.body.endWanted);
	// every frame it is held
	static_cast<void>(copy.Step());
	EXPECT_EQ(copy.draws.size(), 2u);
}
