/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Worship/LeashPosts.h"

using namespace openblack;
using namespace openblack::worship;

namespace
{
/// A temple turned a quarter about Y at (100, 5, 200): its local x points along -z and its local z along +x
glm::mat4 QuarterTurnedTemple()
{
	glm::mat4 temple(0.0f);
	temple[0] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
	temple[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	temple[2] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
	temple[3] = glm::vec4(100.0f, 5.0f, 200.0f, 1.0f);
	return temple;
}

/// A special point at `at`, with no turn of its own
glm::mat4 SpecialPoint(const glm::vec3& at)
{
	glm::mat4 point(1.0f);
	point[3] = glm::vec4(at, 1.0f);
	return point;
}

std::vector<glm::mat4> ThreePoints()
{
	return {SpecialPoint({2.0f, 3.0f, 4.0f}), SpecialPoint({-1.0f, 0.0f, 0.0f}), SpecialPoint({0.0f, 0.0f, 0.0f})};
}
} // namespace

TEST(LeashPosts, PostsHangTheLeashesInOrder)
{
	EXPECT_EQ(leash_posts::TypeOf(0), LeashType::Evil);
	EXPECT_EQ(leash_posts::TypeOf(1), LeashType::Rope);
	EXPECT_EQ(leash_posts::TypeOf(2), LeashType::Good);
}

TEST(LeashPosts, APostStandsAtItsSpecialPointOnTheTemple)
{
	const auto points = ThreePoints();
	const auto temple = QuarterTurnedTemple();
	EXPECT_EQ(leash_posts::Point(temple, points, 0), glm::vec3(104.0f, 8.0f, 198.0f));
	EXPECT_EQ(leash_posts::Point(temple, points, 1), glm::vec3(100.0f, 5.0f, 201.0f));
	EXPECT_EQ(leash_posts::Point(temple, points, 2), glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST(LeashPosts, ThePointIsSummedInTheOriginalsOrder)
{
	// Two terms of 2^-24 next to a 1: added to each other first they make 1 + 2^-23; added to the 1 one at a time each
	// is lost to the rounding. x adds the y and z terms first, y adds the x term and the y term first
	constexpr float k_Tiny = 0x1p-24f;
	glm::mat4 temple(0.0f);
	temple[0] = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
	temple[1] = glm::vec4(k_Tiny, k_Tiny, 0.0f, 0.0f);
	temple[2] = glm::vec4(k_Tiny, k_Tiny, 1.0f, 0.0f);
	temple[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const std::vector<glm::mat4> points = {SpecialPoint({1.0f, 1.0f, 1.0f})};
	const auto at = leash_posts::Point(temple, points, 0);
	EXPECT_EQ(at.x, 1.0f + 0x1p-23f);
	EXPECT_EQ(at.y, 1.0f);
	EXPECT_EQ(at.z, 1.0f);
}

TEST(LeashPosts, WithoutItsPointAPostStandsAtTheTemple)
{
	const auto temple = QuarterTurnedTemple();
	const auto points = ThreePoints();
	const std::vector<glm::mat4> two(points.begin(), points.begin() + 2);
	EXPECT_EQ(leash_posts::Point(temple, two, 2), glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_EQ(leash_posts::Point(temple, {}, 0), glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST(LeashPosts, OnTheLandAPostRisesWithTheGround)
{
	// The fake ground rises half a unit for every unit along x: 50 under the temple, 52 under the first post
	const land_morph::Ground ground = [](glm::vec2 xz) { return xz.x * 0.5f; };
	const auto temple = QuarterTurnedTemple();
	const auto points = ThreePoints();
	EXPECT_EQ(leash_posts::PointOnLand(temple, points, 0, ground), glm::vec3(104.0f, 10.0f, 198.0f));
	// The second post is over the temple's own ground: not lifted
	EXPECT_EQ(leash_posts::PointOnLand(temple, points, 1, ground), glm::vec3(100.0f, 5.0f, 201.0f));
	// With no point the temple's position is not lifted either
	EXPECT_EQ(leash_posts::PointOnLand(temple, {}, 0, ground), glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST(LeashPosts, APostTakesFourDrawsInOrder)
{
	ASSERT_EQ(leash_posts::k_Draws.size(), 4u);
	EXPECT_EQ(std::bit_cast<uint32_t>(leash_posts::k_TwoPi), 0x40C90FDBu);
	const std::array<float, 4> highs = {1.0f, leash_posts::k_TwoPi, leash_posts::k_TwoPi, 15.0f};
	for (size_t i = 0; i < highs.size(); ++i)
	{
		EXPECT_EQ(leash_posts::k_Draws.at(i).low, 0.0f);
		EXPECT_EQ(leash_posts::k_Draws.at(i).high, highs.at(i));
	}
	const auto spin = leash_posts::SeedSpin(0.5f, 1.0f, 2.0f, 7.0f);
	EXPECT_EQ(spin.scroll, 0.5f);
	EXPECT_EQ(spin.xAngle, 1.0f);
	EXPECT_EQ(spin.zAngle, 2.0f);
	EXPECT_EQ(spin.frame, 7.0f);
}

TEST(LeashPosts, FrameSecondsAreTheMillisecondsByAThousandth)
{
	EXPECT_EQ(leash_posts::FrameSeconds(0), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(leash_posts::FrameSeconds(16)), 0x3C83126Fu);
}

TEST(LeashPosts, ASecondMovesEveryPartOn)
{
	auto spin = leash_posts::SeedSpin(0.75f, 6.2f, 6.2f, 14.5f);
	leash_posts::Step(spin, 1.0f);
	// 0.75 + 0.5 = 1.25, less its whole part
	EXPECT_EQ(spin.scroll, 0.25f);
	// 6.2 + 0.1 and 6.2 + 1.0, each less one turn of 2 pi
	EXPECT_EQ(std::bit_cast<uint32_t>(spin.xAngle), 0x3C89BE00u);
	EXPECT_EQ(std::bit_cast<uint32_t>(spin.zAngle), 0x3F6AB458u);
	// 14.5 + 10 = 24.5, less 15
	EXPECT_EQ(spin.frame, 9.5f);
	EXPECT_EQ(leash_posts::SpriteCell(spin), 9);
}

TEST(LeashPosts, SmallStepsDoNotWrap)
{
	auto spin = leash_posts::SeedSpin(0.25f, 1.0f, 1.0f, 3.0f);
	leash_posts::Step(spin, 0.05f);
	EXPECT_EQ(spin.scroll, (0.5f * 0.05f) + 0.25f);
	EXPECT_EQ(spin.xAngle, (0.1f * 0.05f) + 1.0f);
	EXPECT_EQ(spin.zAngle, (1.0f * 0.05f) + 1.0f);
	EXPECT_EQ(spin.frame, 3.5f);
	EXPECT_EQ(leash_posts::SpriteCell(spin), 3);
}

TEST(LeashPosts, WholeTurnsWrapToZero)
{
	auto spin = leash_posts::SeedSpin(0.0f, leash_posts::k_TwoPi, leash_posts::k_TwoPi, 15.0f);
	leash_posts::Step(spin, 0.0f);
	EXPECT_EQ(spin.xAngle, 0.0f);
	EXPECT_EQ(spin.zAngle, 0.0f);
	EXPECT_EQ(spin.frame, 0.0f);
	EXPECT_EQ(leash_posts::SpriteCell(spin), 0);
}

TEST(LeashPosts, AHairUnderAWholeTurnWrapsAHairUnderZero)
{
	// Just under 15 times a fifteenth rounds to 1, so a whole turn is taken off: the clock goes a hair below 0, and
	// the frame, truncated towards 0, is 0
	auto spin = leash_posts::SeedSpin(0.0f, 0.0f, 0.0f, std::bit_cast<float>(0x416FFFFFu));
	leash_posts::Step(spin, 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(spin.frame), 0xB5800000u);
	EXPECT_EQ(leash_posts::SpriteCell(spin), 0);
	// Just under 2 pi times its reciprocal stays under 1: no turn is taken off
	const float underTwoPi = std::bit_cast<float>(0x40C90FDAu);
	auto turning = leash_posts::SeedSpin(0.0f, underTwoPi, 0.0f, 0.0f);
	leash_posts::Step(turning, 0.0f);
	EXPECT_EQ(turning.xAngle, underTwoPi);
}

TEST(LeashPosts, EachCollarShowsItsLeashsBand)
{
	EXPECT_EQ(leash_posts::CollarBand(0), 0.375f);
	EXPECT_EQ(leash_posts::CollarBand(1), 0.125f);
	EXPECT_EQ(leash_posts::CollarBand(2), 0.25f);
}

TEST(LeashPosts, TheSmokeIsASixthOfTheLandsLight)
{
	// The land's light at noon: 0xF3 + 0xFF + 0xFB = 749, a sixth 124.8; the alpha byte is not counted
	EXPECT_EQ(leash_posts::SmokeBrightness(0xFFF3FFFBu), 124u);
	EXPECT_EQ(leash_posts::SmokeBrightness(0x00F3FFFBu), 124u);
	EXPECT_EQ(leash_posts::SmokeBrightness(0xFFFFFFFFu), 127u);
	EXPECT_EQ(leash_posts::SmokeBrightness(0x00000006u), 1u);
	EXPECT_EQ(leash_posts::SmokeBrightness(0x00000005u), 0u);
	EXPECT_EQ(leash_posts::SmokeColour(124, false), 0x7CFFFFFFu);
	EXPECT_EQ(leash_posts::SmokeColour(124, true), 0x7CC18119u);
}

TEST(LeashPosts, PickingALeashPicksItsPost)
{
	EXPECT_EQ(leash_posts::PickAfterSet(leash_posts::k_NoPick, LeashType::Evil), 0);
	EXPECT_EQ(leash_posts::PickAfterSet(leash_posts::k_NoPick, LeashType::Rope), 1);
	EXPECT_EQ(leash_posts::PickAfterSet(leash_posts::k_NoPick, LeashType::Good), 2);
	EXPECT_EQ(leash_posts::PickAfterSet(0, LeashType::Good), 2);
	// No leash, or a number that is no leash, leaves the pick
	EXPECT_EQ(leash_posts::PickAfterSet(1, LeashType::None), 1);
	EXPECT_EQ(leash_posts::PickAfterSet(2, static_cast<LeashType>(0)), 2);
	EXPECT_EQ(leash_posts::PickAfterSet(leash_posts::k_NoPick, static_cast<LeashType>(4)), leash_posts::k_NoPick);
}

TEST(LeashPosts, ThePickGivesBackItsLeash)
{
	EXPECT_EQ(leash_posts::PickedType(leash_posts::k_NoPick), LeashType::None);
	EXPECT_EQ(leash_posts::PickedType(0), LeashType::Evil);
	EXPECT_EQ(leash_posts::PickedType(1), LeashType::Rope);
	EXPECT_EQ(leash_posts::PickedType(2), LeashType::Good);
	// Any other pick reads as compassion
	EXPECT_EQ(leash_posts::PickedType(5), LeashType::Good);
	for (size_t post = 0; post < leash_posts::k_Count; ++post)
	{
		const auto type = leash_posts::TypeOf(post);
		EXPECT_EQ(leash_posts::PickedType(leash_posts::PickAfterSet(leash_posts::k_NoPick, type)), type);
	}
}

TEST(LeashPosts, TheScriptsReadTheTemplesPick)
{
	// No temple heart: 0, which is no leash; a heart with nothing picked: none
	EXPECT_EQ(leash_posts::ScriptLeashType(std::nullopt), 0);
	EXPECT_EQ(leash_posts::ScriptLeashType(leash_posts::k_NoPick), -1);
	EXPECT_EQ(leash_posts::ScriptLeashType(0), 1);
	EXPECT_EQ(leash_posts::ScriptLeashType(1), 2);
	EXPECT_EQ(leash_posts::ScriptLeashType(2), 3);
}

TEST(LeashPosts, EachPostHasItsToolTip)
{
	EXPECT_EQ(leash_posts::ToolTipOf(0), 0xEC8u);
	EXPECT_EQ(leash_posts::ToolTipOf(1), 0xECAu);
	EXPECT_EQ(leash_posts::ToolTipOf(2), 0xEC9u);
}

TEST(LeashPosts, OnlyItsOwnPlayerTapsAPost)
{
	EXPECT_TRUE(leash_posts::ValidToTap(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(leash_posts::ValidToTap(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_TRUE(leash_posts::ValidToTap(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO));
	// the hand feels a post one metre round its point
	EXPECT_EQ(leash_posts::k_HandCollisionRadius, 1.0f);
}

TEST(LeashPosts, ATapPicksThePostOrUnpicksThePickedOne)
{
	// nothing picked: the post is picked, the click sounds for the local interface and its leash is sent
	auto tap = leash_posts::Tap(leash_posts::k_NoPick, 1, true);
	EXPECT_EQ(tap.pick, 1);
	EXPECT_TRUE(tap.click);
	EXPECT_EQ(tap.sent, LeashType::Rope);
	// another post picked: the same, the leash of the post tapped; no click for another interface
	tap = leash_posts::Tap(1, 0, false);
	EXPECT_EQ(tap.pick, 0);
	EXPECT_FALSE(tap.click);
	EXPECT_EQ(tap.sent, LeashType::Evil);
	EXPECT_EQ(leash_posts::Tap(0, 2, true).sent, LeashType::Good);
	// the picked post: unpicked, silently, nothing sent
	tap = leash_posts::Tap(2, 2, true);
	EXPECT_EQ(tap.pick, leash_posts::k_NoPick);
	EXPECT_FALSE(tap.click);
	EXPECT_FALSE(tap.sent.has_value());
	EXPECT_EQ(leash_posts::k_TapSample, 42);
}

TEST(LeashPosts, ALeashPickedAtTheTempleIsRefusedOnlyUnderTheMoodSpells)
{
	EXPECT_FALSE(leash_posts::LeashRefused(true, false, false));
	EXPECT_TRUE(leash_posts::LeashRefused(true, true, false));
	EXPECT_TRUE(leash_posts::LeashRefused(true, false, true));
	// without a creature nothing is tested
	EXPECT_FALSE(leash_posts::LeashRefused(false, true, true));
}

TEST(LeashPosts, APostShowsOnABuiltTempleWhenTheCreatureKnowsItsLeash)
{
	EXPECT_TRUE(leash_posts::Shown(1.0f, true, true));
	// a temple a hair short of built, no creature, a leash not known: not shown
	EXPECT_FALSE(leash_posts::Shown(0.99999994f, true, true));
	EXPECT_FALSE(leash_posts::Shown(1.0f, false, true));
	EXPECT_FALSE(leash_posts::Shown(1.0f, true, false));
}

TEST(LeashPosts, TheCollarTurnsInItsOwnFrameAtThePost)
{
	// unturned at its post: the turn alone, cell for cell, and the post's position
	const glm::mat4 post = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f));
	const leash_posts::Spin spin {.scroll = 0.25f, .xAngle = 0.5f, .zAngle = 2.0f, .frame = 3.0f};
	const auto collar = leash_posts::CollarMatrix(post, spin);
	const glm::mat3 turn = affine::RotationYXZ(0.0f, spin.xAngle, spin.zAngle);
	for (int c = 0; c < 3; ++c)
	{
		EXPECT_EQ(glm::vec3(collar[c]), turn[c]) << c;
	}
	EXPECT_EQ(glm::vec3(collar[3]), glm::vec3(100.0f, 5.0f, 200.0f));
	// a turned post: the spin comes first, in the collar's own frame
	const auto turnedPost = QuarterTurnedTemple();
	const auto turned = leash_posts::CollarMatrix(turnedPost, spin);
	const glm::mat3 expected = glm::mat3(turnedPost) * turn;
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(turned[c][r], expected[c][r], 1e-6f) << c << r;
		}
	}
	EXPECT_EQ(glm::vec3(turned[3]), glm::vec3(turnedPost[3]));
	// no spin: the post's own matrix
	EXPECT_EQ(leash_posts::CollarMatrix(turnedPost, {}), turnedPost);
}

TEST(LeashPosts, ThePickedCollarHangsOnTheHand)
{
	// the hand's root bone at (10, 20, 30), turned 0.7 about y and scaled 2: the collar takes the turn, not the scale
	const float angle = 0.7f;
	const glm::mat4 root = glm::scale(
	    glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 20.0f, 30.0f)), angle, glm::vec3(0.0f, 1.0f, 0.0f)),
	    glm::vec3(2.0f));
	const float handScale = 0.01f;
	const float s = handScale * leash_posts::k_HandCollarScale;
	const auto collar = leash_posts::CollarOnHand(root, handScale);
	const glm::mat3 turn = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 1.0f, 0.0f)));
	for (int c = 0; c < 3; ++c)
	{
		for (int r = 0; r < 3; ++r)
		{
			EXPECT_NEAR(collar[c][r], turn[c][r] * 0.5f * s, 1e-5f) << c << r;
		}
	}
	// half its size, 0.05 s along the bone's x and 0.25 s along its y, from the bone's point
	const glm::vec3 at = glm::vec3(10.0f, 20.0f, 30.0f) + turn * glm::vec3(0.05f * s, 0.25f * s, 0.0f);
	for (int r = 0; r < 3; ++r)
	{
		EXPECT_NEAR(collar[3][r], at[r], 1e-5f) << r;
	}
	EXPECT_EQ(leash_posts::k_HandCollarScale, 173.52948f);
}
