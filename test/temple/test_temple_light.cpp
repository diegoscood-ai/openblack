/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <initializer_list>
#include <limits>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/TempleLight.h"
#include "3D/TemplePool.h"

using namespace openblack;

TEST(TempleLight, NeutralIsAsItIs)
{
	const auto light = TempleLight::At(0.0f, 12345);
	EXPECT_EQ(light.multiply, glm::vec3(1.0f));
	EXPECT_EQ(light.add, glm::vec3(0.0f));
}

TEST(TempleLight, WhollyEvilIsRed)
{
	const auto light = TempleLight::At(-1.0f, 0);
	EXPECT_FLOAT_EQ(light.multiply.r, 1.0f);
	EXPECT_FLOAT_EQ(light.multiply.g, 175.0f / 255.0f);
	EXPECT_FLOAT_EQ(light.multiply.b, 160.0f / 255.0f);
	EXPECT_EQ(light.add.g, 0.0f);
	EXPECT_EQ(light.add.b, 0.0f);
	// Near the height of the red pulse, just short of 22, it is 21 by 255 of 256
	EXPECT_FLOAT_EQ(TempleLight::At(-1.0f, 300 * 8).add.r, 20.0f / 255.0f);
}

TEST(TempleLight, HalfEvilIsHalfAsRed)
{
	// 127 of 255: green is 255 less 80 by 127 over 256, rounded down
	const auto light = TempleLight::At(-0.5f, 0);
	EXPECT_FLOAT_EQ(light.multiply.g, 215.0f / 255.0f);
	EXPECT_FLOAT_EQ(light.multiply.b, 207.0f / 255.0f);
}

TEST(TempleLight, GoodPulsesThroughTheColours)
{
	// At the start: red 22, green 22 + 11 sin(2 pi / 3) and blue 22 + 11 sin(2 pi), each by 255 of 256
	const auto light = TempleLight::At(1.0f, 0);
	EXPECT_EQ(light.multiply, glm::vec3(1.0f));
	EXPECT_FLOAT_EQ(light.add.r, 21.0f / 255.0f);
	EXPECT_FLOAT_EQ(light.add.g, 30.0f / 255.0f);
	EXPECT_FLOAT_EQ(light.add.b, 21.0f / 255.0f);
	// An eighth of the way round, red is 22 + 11 sin(pi / 4)
	EXPECT_FLOAT_EQ(TempleLight::At(1.0f, 64 * 8).add.r, 28.0f / 255.0f);
}

TEST(TempleLight, ColoursAreMultipliedByTheLight)
{
	const auto light = TempleLight::At(-1.0f, 0);
	const auto colour = light.Colour(glm::vec3(0x80 / 255.0f));
	EXPECT_FLOAT_EQ(colour.r, 127.0f / 255.0f);
	EXPECT_FLOAT_EQ(colour.g, 87.0f / 255.0f);
	EXPECT_FLOAT_EQ(colour.b, 80.0f / 255.0f);
}

TEST(TempleFade, EachRoomFadesFromItsStartToItsEndAsLastSet)
{
	EXPECT_EQ(temple_fade::k_CreatureRoom.from, 40.0f);
	EXPECT_EQ(temple_fade::k_CreatureRoom.to, 150.0f);
	EXPECT_EQ(temple_fade::k_Room.from, 60.0f);
	EXPECT_EQ(temple_fade::k_Room.to, 200.0f);
	EXPECT_EQ(temple_fade::k_NoRoom.from, 30.0f);
	EXPECT_EQ(temple_fade::k_NoRoom.to, 80.0f);
	const auto same = [](const TempleFade& a, const TempleFade& b) { return a.from == b.from && a.to == b.to; };
	// the creature's room's own once it has been drawn; until then, as on its first frame, a room's
	EXPECT_TRUE(same(temple_fade::Of(true, true, true), temple_fade::k_CreatureRoom));
	EXPECT_TRUE(same(temple_fade::Of(true, true, false), temple_fade::k_Room));
	EXPECT_TRUE(same(temple_fade::Of(true, false, true), temple_fade::k_Room));
	// no room at all
	EXPECT_TRUE(same(temple_fade::Of(false, true, true), temple_fade::k_NoRoom));
	EXPECT_TRUE(same(temple_fade::Of(false, false, false), temple_fade::k_NoRoom));
	// the light starts with a room's
	EXPECT_TRUE(same(TempleLight::At(0.0f, 0).fade, temple_fade::k_Room));
}

TEST(TempleFade, TheShareIsTheOriginalsStepByStep)
{
	// The original's shares, from its own table and lookup run natively, at depths in the three fades
	struct Case
	{
		float depth;
		uint32_t share;
	};
	const auto check = [](const TempleFade& fade, std::initializer_list<Case> cases) {
		for (const auto& [depth, share] : cases)
		{
			EXPECT_EQ(temple_fade::Share(depth, fade), share) << "depth " << depth;
		}
	};
	check(temple_fade::k_CreatureRoom, {
	                                       {0.0f, 255},
	                                       {39.0f, 255},
	                                       {40.0f, 255},
	                                       {41.0f, 254},
	                                       {100.0f, 170},
	                                       {150.0f, 100},
	                                       {151.0f, 100},
	                                       {1000.0f, 100},
	                                       // just past a step's start, where the product in floats rounds up to it
	                                       {49.8828125f, 241},
	                                       {61.162109375f, 225},
	                                       {107.03125f, 160},
	                                   });
	check(temple_fade::k_Room, {
	                               {59.0f, 255},
	                               {67.25f, 247},
	                               {130.0f, 177},
	                               {201.0f, 100},
	                           });
	check(temple_fade::k_NoRoom, {
	                                 {29.0f, 255},
	                                 {37.25f, 233},
	                                 {55.0f, 177},
	                                 {81.0f, 100},
	                             });
	// a depth that is not a number takes the first step, as the original's comparisons send it
	EXPECT_EQ(temple_fade::Share(std::numeric_limits<float>::quiet_NaN(), temple_fade::k_CreatureRoom), 255u);
}

TEST(TempleFade, AColourKeepsItsAlphaAndFadesItsRedGreenAndBlue)
{
	EXPECT_EQ(temple_fade::Apply(0xFFFFFFFFu, 0.0f, temple_fade::k_CreatureRoom), 0xFFFEFEFEu);
	EXPECT_EQ(temple_fade::Apply(0x80FF8040u, 1000.0f, temple_fade::k_CreatureRoom), 0x80633219u);
}

TEST(TempleFade, TheDepthIsTheViewsThirdRow)
{
	glm::mat4 view(0.0f);
	view[0][2] = 2.0f;
	view[1][2] = 3.0f;
	view[2][2] = 5.0f;
	view[3][2] = 7.0f;
	view[0][0] = 100.0f; // another row, not read
	EXPECT_EQ(temple_fade::Depth(view, glm::vec3(1.0f, 10.0f, 100.0f)), 2.0f + 30.0f + 500.0f + 7.0f);
}

TEST(TempleFade, AStepIsTheWayFromTheStartInEvenStepsHeldAtTheEnd)
{
	EXPECT_EQ(temple_fade::Step(0.0f, temple_fade::k_Room), 0);
	EXPECT_EQ(temple_fade::Step(60.0f, temple_fade::k_Room), 0);
	EXPECT_EQ(temple_fade::Step(130.0f, temple_fade::k_Room), 512);
	EXPECT_EQ(temple_fade::Step(200.0f, temple_fade::k_Room), 1023);
	EXPECT_EQ(temple_fade::Step(5000.0f, temple_fade::k_Room), 1023);
	EXPECT_EQ(temple_fade::Step(std::numeric_limits<float>::quiet_NaN(), temple_fade::k_Room), 0);
	EXPECT_FLOAT_EQ(temple_fade::StepsPerUnit(temple_fade::k_Room), 1024.0f / 140.0f);
	// the share of a step is the share at its depth
	EXPECT_EQ(temple_fade::ShareOfStep(0), 255u);
	EXPECT_EQ(temple_fade::ShareOfStep(512), 177u);
	EXPECT_EQ(temple_fade::ShareOfStep(1023), 100u);
}

TEST(TempleFade, TheColourAddedIsTwoFifthsOfTheTemplesLight)
{
	const auto neutral = temple_fade::EndColour(TempleLight::At(0.0f, 0).multiply);
	EXPECT_FLOAT_EQ(neutral.r, 102.0f);
	EXPECT_FLOAT_EQ(neutral.g, 102.0f);
	EXPECT_FLOAT_EQ(neutral.b, 102.0f);
	// the evil temple's light is 255, 175, 160
	const auto evil = temple_fade::EndColour(TempleLight::At(-1.0f, 0).multiply);
	EXPECT_FLOAT_EQ(evil.r, 102.0f);
	EXPECT_FLOAT_EQ(evil.g, 70.0f);
	EXPECT_FLOAT_EQ(evil.b, 64.0f);
}

TEST(TempleFade, TheColourAddedAtAStepRoundsHalvesToEven)
{
	const glm::vec3 end {102.0f, 70.0f, 64.0f};
	EXPECT_EQ(temple_fade::AddedAt(0, end), glm::vec3(0.0f));
	EXPECT_EQ(temple_fade::AddedAt(512, end), glm::vec3(51.0f, 35.0f, 32.0f));
	// a quarter of 102 is 25.5, which rounds to 26; of 70, 17.5 rounds to 18
	EXPECT_EQ(temple_fade::AddedAt(256, end), glm::vec3(26.0f, 18.0f, 16.0f));
	EXPECT_EQ(temple_fade::AddedAt(1023, end), end);
}

TEST(TempleFade, AModelIsFoggedOnceAtItsDepth)
{
	const glm::vec3 end {102.0f};
	// even before the fog starts a colour loses a little to the byte of 255
	const auto near = temple_fade::Once(glm::vec3(224.0f), glm::vec3(0.0f), 10.0f, temple_fade::k_Room, end);
	EXPECT_EQ(near.colour, glm::vec3(223.0f));
	EXPECT_EQ(near.added, glm::vec3(0.0f));
	// half way: 224 by 177 of 256, and the fog's 51 added to the colour added already
	const auto half = temple_fade::Once(glm::vec3(224.0f), glm::vec3(0.0f, 0.0f, 20.0f), 130.0f, temple_fade::k_Room, end);
	EXPECT_EQ(half.colour, glm::vec3(154.0f));
	EXPECT_EQ(half.added, glm::vec3(51.0f, 51.0f, 71.0f));
	// the colour added saturates
	const auto far = temple_fade::Once(glm::vec3(255.0f), glm::vec3(250.0f), 1000.0f, temple_fade::k_Room, end);
	EXPECT_EQ(far.colour, glm::vec3(99.0f));
	EXPECT_EQ(far.added, glm::vec3(255.0f));
}

TEST(TempleFade, AWallsVertexMovesTowardsTheFogByAQuarterOfTheStep)
{
	const glm::vec3 end {102.0f};
	const glm::vec3 white {255.0f};
	const glm::vec3 glow {0.0f, 0.0f, 20.0f};
	// up to the start it keeps its own colours
	const auto near = temple_fade::AtVertex(white, glow, 60.0f, temple_fade::k_Room, end);
	EXPECT_EQ(near.colour, white);
	EXPECT_EQ(near.added, glow);
	// half way it is half way to the step's colours: from 255 to 177 by 128 of 256, rounded down, and from 0 to 51
	const auto half = temple_fade::AtVertex(white, glow, 130.0f, temple_fade::k_Room, end);
	EXPECT_EQ(half.colour, glm::vec3(216.0f));
	EXPECT_EQ(half.added, glm::vec3(25.0f, 25.0f, 35.0f));
	// from the end on it takes the last step's colours alone
	const auto far = temple_fade::AtVertex(white, glow, 200.0f, temple_fade::k_Room, end);
	EXPECT_EQ(far.colour, glm::vec3(100.0f));
	EXPECT_EQ(far.added, glm::vec3(102.0f));
	// at no depth that is a number it keeps its own
	const auto none = temple_fade::AtVertex(white, glow, std::numeric_limits<float>::quiet_NaN(), temple_fade::k_Room, end);
	EXPECT_EQ(none.colour, white);
	EXPECT_EQ(none.added, glow);
}

TEST(TemplePool, TheClockMovesByTheFramesWholeMillisecondsTimesAThousandth)
{
	// A thousandth times the milliseconds, not the milliseconds over a thousand: the two differ in the last bit for 5
	// and 30
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_pool::Advance(0.0f, 5)), 0x3BA3D70Bu);
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_pool::Advance(0.0f, 30)), 0x3CF5C290u);
	EXPECT_EQ(std::bit_cast<uint32_t>(temple_pool::Advance(1.0f, 30)), 0x3F83D70Au);
}

TEST(TemplePool, TheAlphaSwellsAndEbbsWithTheSine)
{
	EXPECT_EQ(temple_pool::Alpha(0.0f), 128);
	EXPECT_EQ(temple_pool::Alpha(1.0f), 154);
	EXPECT_EQ(temple_pool::Alpha(2.5f), 147);
	EXPECT_EQ(temple_pool::Alpha(4.0f), 103);
}

TEST(TemplePool, TheWaterIsTheLightAt208Of256RoundedDown)
{
	EXPECT_EQ(temple_pool::WaterColour({255.0f, 175.0f, 160.0f}), glm::vec3(207.0f, 142.0f, 130.0f));
	EXPECT_EQ(temple_pool::WaterColour(glm::vec3(255.0f)), glm::vec3(207.0f));
	EXPECT_EQ(temple_pool::WaterColour(glm::vec3(0.0f)), glm::vec3(0.0f));
}

TEST(TemplePool, TheSecondLayerIsLiftedTurnedAndTakesWhatTheFirstLacks)
{
	const auto layers = temple_pool::Layers(1.0f);
	EXPECT_EQ(layers[0].alpha, 154);
	EXPECT_EQ(layers[0].alpha + layers[1].alpha, 255);
	EXPECT_EQ(layers[0].position, glm::vec3(0.0f));
	EXPECT_EQ(layers[1].position, glm::vec3(0.0f, 0.05f, 0.0f));
	EXPECT_EQ(layers[0].yaw, 0.0f);
	EXPECT_EQ(layers[1].yaw, 0.7853982f);
	EXPECT_EQ(layers[0].uvOffset, glm::vec2(-0.01f, 0.007f));
	EXPECT_EQ(layers[1].uvOffset, glm::vec2(0.01f, 0.005f));
}
