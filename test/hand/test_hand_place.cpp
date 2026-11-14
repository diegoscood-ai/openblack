/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where the hand is placed each frame and how it is posed there (hand_place)

#include <cmath>
#include <cstdint>

#include <bit>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandMorph.h"
#include "3D/MapCoords.h"
#include "Enums.h"
#include "Hand/HandPlaceRules.h"

#include "../oracle/OracleData.h"

using namespace openblack;
namespace place = openblack::hand_place;

TEST(HandPlace, TheCursorsCopyTrailsItByAtMostEightyPixels)
{
	// far behind, the copy is pulled to 80 pixels of the cursor on each axis
	const auto behind = place::StepSmoothMouse({.position = {0.0f, 0.0f}}, {100.0f, -200.0f}, 0.1f);
	EXPECT_FLOAT_EQ(behind.position.x, 20.0f);
	EXPECT_FLOAT_EQ(behind.position.y, -120.0f);
	// then its velocity is pulled towards the cursor with a stiffness of 20 and keeps 0.03 of itself over a second
	const auto still = place::StepSmoothMouse({.position = {0.0f, 0.0f}, .velocity = {10.0f, 0.0f}}, {0.0f, 0.0f}, 1.0f);
	EXPECT_FLOAT_EQ(still.position.x, 10.0f);
	EXPECT_FLOAT_EQ(still.velocity.x, (10.0f - 10.0f * 20.0f) * 0.03f);
	EXPECT_FLOAT_EQ(still.velocity.y, 0.0f);
	// on the cursor and at rest it stays
	const auto rest = place::StepSmoothMouse({.position = {50.0f, 60.0f}}, {50.0f, 60.0f}, 0.02f);
	EXPECT_EQ(rest.position, glm::vec2(50.0f, 60.0f));
	EXPECT_EQ(rest.velocity, glm::vec2(0.0f));
}

TEST(HandPlace, TheTrailingGapLeansTheHandUnlessBelowATenThousandth)
{
	// sideways by the copy's lag behind the cursor, in pixels
	EXPECT_EQ(place::LeanMotion({100.0f, 50.0f}, {60.0f, 50.0f}, false).sideways, std::optional(-40.0f));
	// the other way round while gripping the land
	EXPECT_EQ(place::LeanMotion({100.0f, 50.0f}, {60.0f, 50.0f}, true).sideways, std::optional(40.0f));
	// never more than the whole lean, 80 pixels
	EXPECT_EQ(place::LeanMotion({300.0f, 50.0f}, {0.0f, 50.0f}, false).sideways, std::optional(-80.0f));
	EXPECT_EQ(place::LeanMotion({-300.0f, 50.0f}, {0.0f, 50.0f}, false).sideways, std::optional(80.0f));
	// a gap smaller than a ten-thousandth leans nothing
	EXPECT_EQ(place::LeanMotion({0.0f, 50.0f}, {0.00005f, 50.0f}, false).sideways, std::nullopt);
	// back and forth by the cursor's lag behind the copy up the screen
	EXPECT_EQ(place::LeanMotion({0.0f, 50.0f}, {0.0f, 10.0f}, false).forwards, std::optional(40.0f));
	// that lean is skipped while the copy's own height is below a ten-thousandth, whatever the gap
	EXPECT_EQ(place::LeanMotion({0.0f, 40.0f}, {0.0f, 0.0f}, false).forwards, std::nullopt);
}

TEST(HandPlace, TheUpTakesANewTargetOnlyWhenTheCursorsXChanged)
{
	EXPECT_FALSE(place::TakesNewUpTarget(320.0f, 320.0f));
	EXPECT_TRUE(place::TakesNewUpTarget(321.0f, 320.0f));
	// entering the normal state forgets the last x, so the first frame takes one
	EXPECT_TRUE(place::TakesNewUpTarget(0.0f, -1e30f));
}

TEST(HandPlace, TheNormalStateHoldsNothingAndWorksNoTotem)
{
	EXPECT_TRUE(place::IsNormalState(false, HoldType::None, false));
	EXPECT_TRUE(place::IsNormalState(false, HoldType::Side, false));
	EXPECT_FALSE(place::IsNormalState(true, HoldType::Side, false));
	// a thing held no way leaves the hand in its normal state
	EXPECT_TRUE(place::IsNormalState(true, HoldType::None, false));
	EXPECT_FALSE(place::IsNormalState(false, HoldType::None, true));
}

TEST(HandPlace, OverAWorshipIconTheHandAimsThreePointTwoShortOfItWhateverItsSize)
{
	EXPECT_FLOAT_EQ(place::k_IconDepthOffset, 3.2f);
	EXPECT_FLOAT_EQ(place::HandDistanceTarget(false, 0.0f, 20.0f, place::k_IconDepthOffset), 16.8f);
	// never nearer than 1
	EXPECT_FLOAT_EQ(place::HandDistanceTarget(false, 0.0f, 3.0f, place::k_IconDepthOffset), 1.0f);
	// a screen object's depth wins
	EXPECT_FLOAT_EQ(place::HandDistanceTarget(true, 7.0f, 20.0f, 3.2f), 7.0f);
	EXPECT_FLOAT_EQ(place::HandDistanceTarget(true, 0.2f, 20.0f, 3.2f), 1.0f);
}

TEST(HandPlace, HoldingTheHandsSizeIsSubtractedOnlyForAnUnreadySeedOrWhileACreatureIsOffered)
{
	// the empty hand always stops its own size short of the land
	EXPECT_TRUE(place::SubtractsHandSize(false, false, HoldType::None, false));
	// holding, only a miracle's seed not yet ready (the magic hold) or an offer to a creature subtracts it
	EXPECT_FALSE(place::SubtractsHandSize(false, true, HoldType::Side, false));
	EXPECT_FALSE(place::SubtractsHandSize(false, true, HoldType::Above, false));
	EXPECT_TRUE(place::SubtractsHandSize(false, true, HoldType::Magic, false));
	EXPECT_TRUE(place::SubtractsHandSize(false, true, HoldType::Side, true));
	EXPECT_TRUE(place::SubtractsHandSize(false, true, HoldType::Villager, true));
	// pulling at something, never
	EXPECT_FALSE(place::SubtractsHandSize(true, true, HoldType::Tree, true));
	EXPECT_FALSE(place::SubtractsHandSize(true, false, HoldType::None, false));
}

TEST(HandPlace, WhileACreatureIsOfferedWithMoreThan300MsLeftOrReachesTheScaleIsTakenFromItsTakePoint)
{
	const glm::vec3 eye {1.0f, 10.0f, -30.0f};
	const glm::vec3 take {4.0f, 6.0f, -18.0f};
	// 3, -4, 12: 13 away
	EXPECT_EQ(place::TakePointViewDistance(eye, take, 301, false), 13.0f);
	EXPECT_EQ(place::TakePointViewDistance(eye, take, 1084, false), 13.0f);
	// 300 ms or less left: the land's distance stands
	EXPECT_FALSE(place::TakePointViewDistance(eye, take, 300, false).has_value());
	EXPECT_FALSE(place::TakePointViewDistance(eye, take, 0, false).has_value());
	// reaching for what it was given, whatever is left of the offer
	EXPECT_EQ(place::TakePointViewDistance(eye, take, 0, true), 13.0f);
	// no take point (no creature's body): the land's
	EXPECT_FALSE(place::TakePointViewDistance(eye, std::nullopt, 1100, true).has_value());
	// summed x, y, then z, each step a float
	const glm::vec3 far {16777216.0f, 1.0f, 1.0f};
	const auto summed = place::TakePointViewDistance(glm::vec3(0.0f), far, 1100, false);
	ASSERT_TRUE(summed.has_value());
	EXPECT_EQ(*summed, std::sqrt((16777216.0f * 16777216.0f + 1.0f) + 1.0f));
}

TEST(HandPlace, ALockedPileFieldOrFishFarmKeepsTheHandAtItsOwnPlaceAboveTheObject)
{
	// the hand's own x, z, not the mouse's; the ground plus the object's height for the hand, then the hold's height
	const auto grip = place::LockedInteractionGrip({12.5f, -40.25f}, 3.0f, 1.5f, 0.25f);
	EXPECT_EQ(grip.x, 12.5f);
	EXPECT_EQ(grip.z, -40.25f);
	EXPECT_EQ(grip.y, 4.75f);
	// summed in that order, as float steps: the ground and the object first
	const float ground = 16777216.0f;
	EXPECT_EQ(place::LockedInteractionGrip({0.0f, 0.0f}, ground, 1.0f, 1.0f).y, (ground + 1.0f) + 1.0f);
	EXPECT_NE(place::LockedInteractionGrip({0.0f, 0.0f}, ground, 1.0f, 1.0f).y, ground + (1.0f + 1.0f));
}

TEST(HandPlace, TheSpringStartsAtTheSecondPressWithNoSpeedAndAMiraclesSeedSkipsIt)
{
	// the holding state: a held thing, not pulled, held some way, not a seed
	EXPECT_TRUE(place::SpringHoldingState(true, false, HoldType::Side, false));
	EXPECT_FALSE(place::SpringHoldingState(true, false, HoldType::Magic, true));
	EXPECT_FALSE(place::SpringHoldingState(true, true, HoldType::Tree, false));
	EXPECT_FALSE(place::SpringHoldingState(true, false, HoldType::None, false));
	EXPECT_FALSE(place::SpringHoldingState(false, false, HoldType::Side, false));
	// off, the hand follows until it is ready to throw, and then the spring takes hold
	EXPECT_EQ(place::SpringFrameOf(true, false, true, false), place::SpringFrame::Follow);
	EXPECT_EQ(place::SpringFrameOf(true, false, true, true), place::SpringFrame::Start);
	// on, it steps from the next frame
	EXPECT_EQ(place::SpringFrameOf(true, true, true, true), place::SpringFrame::Step);
	EXPECT_EQ(place::SpringFrameOf(true, true, true, false), place::SpringFrame::Step);
	// out of the holding state (a seed, nothing held) there is no spring
	EXPECT_EQ(place::SpringFrameOf(false, true, true, true), place::SpringFrame::Follow);
	EXPECT_EQ(place::SpringFrameOf(false, false, true, true), place::SpringFrame::Follow);
	// with no place yet, the hand is placed first
	EXPECT_EQ(place::SpringFrameOf(true, true, false, true), place::SpringFrame::Start);
}

TEST(HandPlace, AStandingTreeHangsAFurtherTenthOfItsHeightLower)
{
	EXPECT_FLOAT_EQ(place::HoldHeight(1.9f, false, 12.0f), 1.9f);
	EXPECT_FLOAT_EQ(place::HoldHeight(1.9f, true, 12.0f), 1.9f + 0.1f * 12.0f);
	EXPECT_FLOAT_EQ(place::HoldHeight(0.2f, true, 0.0f), 0.2f);
}

TEST(HandPlace, EachHoldPlaysItsOwnStillPose)
{
	const place::HoldClips all {.wiggle = true, .above = true, .side = true};
	EXPECT_EQ(place::HoldClipOf(true, HoldType::Magic, all), place::HoldClip::Wiggle);
	EXPECT_EQ(place::HoldClipOf(true, HoldType::Above, all), place::HoldClip::Above);
	for (const auto side : {HoldType::Side, HoldType::Tree, HoldType::Villager})
	{
		EXPECT_EQ(place::HoldClipOf(true, side, all), place::HoldClip::Side);
	}
	// the magic grip without its wiggle takes the side pose
	EXPECT_EQ(place::HoldClipOf(true, HoldType::Magic, {.above = true, .side = true}), place::HoldClip::Side);
	// a pose the hand doesn't have is not played, nor any while nothing is held
	EXPECT_EQ(place::HoldClipOf(true, HoldType::Above, {.wiggle = true, .side = true}), place::HoldClip::None);
	EXPECT_EQ(place::HoldClipOf(false, HoldType::Above, all), place::HoldClip::None);
}

TEST(HandPlace, EachTurnTheHeldThingIsKeptAtTheHandsMapPositionItsHeightAboveTheLandKept)
{
	const glm::vec3 hand {123.456f, 30.0f, 654.321f};
	std::vector<glm::vec2> asked;
	const land_morph::Ground slope = [&asked](glm::vec2 xz) {
		asked.push_back(xz);
		return 0.01f * xz.x;
	};
	const auto kept = place::HeldTurnPlace(hand, &slope);
	EXPECT_EQ(kept.x, map_coords::Quantise(hand.x));
	EXPECT_EQ(kept.z, map_coords::Quantise(hand.z));
	// the land is looked up at the hand's own position first, then at the kept one
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[1], glm::vec2(kept.x, kept.z));
	const float altitude = hand.y - 0.01f * asked[0].x;
	EXPECT_FLOAT_EQ(kept.y, 0.01f * kept.x + altitude);
	// without a land the height is the hand's
	const auto flat = place::HeldTurnPlace(hand, nullptr);
	EXPECT_EQ(flat, glm::vec3(map_coords::Quantise(hand.x), 30.0f, map_coords::Quantise(hand.z)));
}

// Each turn's place of a held thing, bit for bit with the original's (the oracle's held_place.txt): its map position
// from the hand's point, the land asked at that same map position, and its height above the land kept, a zero height
// above the land being a positive zero. The land gives a chosen height; the place's height is that height plus the
// height above it, as the map position reads it back
TEST(HandPlace, EachTurnsPlaceOfAHeldThingIsTheOriginalsBitForBit)
{
	const auto table = oracle::Table::Load("held_place.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto column = [&table](const char* name) { return table->Column(name).value_or(0); };
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto at = [&table, &column, row](const char* name) { return table->Float(row, column(name)); };
		const auto bits = [&table, &column, row](const char* name) { return table->Bits(row, column(name)); };
		const float height = at("ground");
		std::vector<glm::vec2> asked;
		const land_morph::Ground land = [&asked, height](glm::vec2 xz) {
			asked.push_back(xz);
			return height;
		};
		const auto kept = place::HeldTurnPlace({at("x"), at("y"), at("z")}, &land);
		const auto fixed = [&bits](const char* name) { return static_cast<int32_t>(bits(name)); };
		EXPECT_EQ(std::bit_cast<uint32_t>(kept.x), std::bit_cast<uint32_t>(map_coords::ToMetres(fixed("place_x"))))
		    << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(kept.z), std::bit_cast<uint32_t>(map_coords::ToMetres(fixed("place_z"))))
		    << "row " << row;
		ASSERT_FALSE(asked.empty()) << "row " << row;
		EXPECT_EQ(asked.front(), glm::vec2(map_coords::ToMetres(fixed("ground_x")), map_coords::ToMetres(fixed("ground_z"))))
		    << "row " << row;
		EXPECT_EQ(std::bit_cast<uint32_t>(kept.y), std::bit_cast<uint32_t>(height + at("altitude"))) << "row " << row;
	}
}

// The action collide's point is the collided object's position: over a worship icon the icon's own (Land 2's worship
// site: the seed graphic hit at (3321.090, 83.052, 3247.213), its icon at (3319.950, 81.027, 3247.775)), else the land
TEST(HandPlace, TheCollidePointIsTheCollidedObjectsOwnElseTheLand)
{
	const glm::vec3 icon {3319.950f, 81.027f, 3247.775f};
	const glm::vec3 land {3301.2f, 77.0f, 3238.6f};
	EXPECT_EQ(hand_place::CollidePointOf(icon, land), std::optional(icon));
	EXPECT_EQ(hand_place::CollidePointOf(std::nullopt, land), std::optional(land));
	EXPECT_EQ(hand_place::CollidePointOf(std::nullopt, std::nullopt), std::nullopt);
}
