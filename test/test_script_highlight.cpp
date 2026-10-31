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

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/HandTap.h"
#include "Graphics/LightColumn.h"
#include "Help/HelpProfile.h"
#include "Locator.h"
#include "support/RestoreService.h"

// ScriptHighlight's pure rules, against the values of the original game.

using namespace openblack;
namespace sh = openblack::ecs::script_highlight;

TEST(ScriptHighlight, DidYouKnowIsRowOne)
{
	// the did-you-know is info row 1
	EXPECT_FALSE(sh::IsDidYouKnowInfo(0));
	EXPECT_TRUE(sh::IsDidYouKnowInfo(1));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(2));
	EXPECT_FALSE(sh::IsDidYouKnowInfo(3));
}

TEST(ScriptHighlight, ActivationPlaysTheClickOnScrollButNotForADidYouKnow)
{
	// the in-game bank's 134 (click on scroll) for every row but the did-you-know
	EXPECT_EQ(sh::ActivatedSample(0), 134);
	EXPECT_FALSE(sh::ActivatedSample(1).has_value());
	EXPECT_EQ(sh::ActivatedSample(2), 134);
	EXPECT_EQ(sh::ActivatedSample(3), 134);
}

TEST(ScriptHighlight, SpriteAlphaByRow)
{
	// the sprite's alpha by row
	EXPECT_EQ(sh::SpriteAlpha(0), 0x00);
	EXPECT_EQ(sh::SpriteAlpha(1), 0x32);
	EXPECT_EQ(sh::SpriteAlpha(2), 0x96);
	EXPECT_EQ(sh::SpriteAlpha(3), 0x64);
	EXPECT_EQ(sh::SpriteAlpha(4), 0x00);
}

TEST(ScriptHighlight, ScrollsGrowWithTheCameraDistance)
{
	// the whole metres bound to [10, 30], x 1 / 30
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 3.0f, false), 10.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.0f, 20.9f, false), 20.0f * 0.0333333f);
	EXPECT_FLOAT_EQ(sh::DistanceScale(2.0f, 500.0f, false), 30.0f * 2.0f * 0.0333333f);
	// a did-you-know keeps its scale
	EXPECT_FLOAT_EQ(sh::DistanceScale(1.5f, 500.0f, true), 1.5f);
}

TEST(ScriptHighlight, SpinWrapsByTruncation)
{
	// pi / 1000 a game ms; minus ftol(a / 2 pi) turns
	EXPECT_NEAR(sh::SpinAngle(0.0f, 100), 0.314159f, 1e-5f);
	EXPECT_NEAR(sh::SpinAngle(6.2f, 100), 6.2f + 0.314159f - 6.28319f, 1e-4f);
	// a negative angle is not brought into [0, 2 pi) (ftol truncates towards 0)
	EXPECT_NEAR(sh::SpinAngle(-1.0f, 0), -1.0f, 1e-6f);
}

TEST(ScriptHighlight, AHighlightSpinsHalfATurnASecond)
{
	EXPECT_NEAR(sh::SpinAngle(0.0f, 1000), glm::pi<float>(), 1e-5f);
	// past a whole turn it comes round
	EXPECT_NEAR(sh::SpinAngle(6.0f, 1000), 6.0f + glm::pi<float>() - glm::two_pi<float>(), 1e-5f);
	// a negative angle more than a whole turn round is brought back by whole turns towards none
	EXPECT_NEAR(sh::SpinAngle(-7.0f, 0), -7.0f + glm::two_pi<float>(), 1e-5f);
}

TEST(ScriptHighlight, ALitSignFacesTheCamera)
{
	// a quarter turn past the angle to the camera: due east, a quarter turn; due north, half a turn
	const glm::vec3 point(0.0f);
	EXPECT_NEAR(sh::PoseOfDraw(0.0f, true, point, {10.0f, 0.0f, 0.0f}, 1.0f, true, 0).angle, glm::half_pi<float>(), 1e-5f);
	EXPECT_NEAR(sh::PoseOfDraw(0.0f, true, point, {0.0f, 0.0f, 10.0f}, 1.0f, true, 0).angle, glm::pi<float>(), 1e-5f);
}

TEST(ScriptHighlight, ThePoseTurnsOnlyOnDrawnFramesWithNoCatchUp)
{
	// four frames of 33 game ms, the list draws the first and the last: two increments, not four
	const glm::vec3 point(0.0f);
	const glm::vec3 eye(0.0f, 0.0f, 20.5f);
	float angle = 0.0f;
	for (const bool drawn : {true, false, false, true})
	{
		if (drawn)
		{
			angle = sh::PoseOfDraw(angle, false, point, eye, 1.0f, false, 33).angle;
		}
	}
	EXPECT_FLOAT_EQ(angle, sh::SpinAngle(sh::SpinAngle(0.0f, 33), 33));
	EXPECT_NEAR(angle, 2.0f * 33.0f * 0.00314159f, 1e-6f);
}

TEST(ScriptHighlight, ThePoseWraps)
{
	const glm::vec3 point(0.0f);
	const glm::vec3 eye(3.0f, 0.0f, 4.0f);
	// a turning one wraps as SpinAngle does
	EXPECT_NEAR(sh::PoseOfDraw(6.2f, false, point, eye, 1.0f, false, 100).angle, 6.2f + 0.314159f - 6.28319f, 1e-4f);
	// a lit did-you-know faces the eye, wrapped by truncation, whatever its last angle and the frame's ms
	const float yaw = graphics::billboard::YawToEyeAngle(point, eye);
	const float wrapped = yaw - static_cast<float>(static_cast<int32_t>(yaw * 0.159155f)) * 6.28319f;
	EXPECT_FLOAT_EQ(sh::PoseOfDraw(5.0f, true, point, eye, 1.0f, true, 100).angle, wrapped);
	EXPECT_FLOAT_EQ(sh::PoseOfDraw(0.0f, true, point, eye, 1.0f, true, 0).angle, wrapped);
}

TEST(ScriptHighlight, ThePoseScaleIsReadFreshAndDoesNotBuildUp)
{
	// 20.5 units away: a scroll of scale 1 is drawn at 20 / 30, on every drawn frame alike
	const glm::vec3 point(0.0f);
	const glm::vec3 eye(0.0f, 0.0f, 20.5f);
	auto pose = sh::PoseOfDraw(0.0f, false, point, eye, 1.0f, false, 16);
	EXPECT_FLOAT_EQ(pose.scale, 20.0f * 0.0333333f);
	pose = sh::PoseOfDraw(pose.angle, false, point, eye, 1.0f, false, 16);
	EXPECT_FLOAT_EQ(pose.scale, 20.0f * 0.0333333f);
	// a did-you-know keeps its own scale
	EXPECT_FLOAT_EQ(sh::PoseOfDraw(0.0f, false, point, eye, 1.5f, true, 16).scale, 1.5f);
}

TEST(ScriptHighlight, ADrawThatExitsEarlyKeepsTheListsFlagAndStepsNothing)
{
	// hidden, scrolls hidden by the script, or no mesh: the flag the list set before the Draw, and no glint step
	for (const bool onScreen : {false, true})
	{
		const auto step = sh::HighlightDrawStep(true, onScreen, false, true, true);
		EXPECT_EQ(step.glintSteps, 0u);
		EXPECT_TRUE(step.onScreen);
	}
}

TEST(ScriptHighlight, TheGlintsFirstStepIsADoubleOne)
{
	EXPECT_EQ(sh::HighlightDrawStep(false, true, false, true, true).glintSteps, 2u);
	EXPECT_EQ(sh::HighlightDrawStep(false, true, false, true, false).glintSteps, 1u);
	// off the screen they step all the same
	EXPECT_EQ(sh::HighlightDrawStep(false, false, false, true, false).glintSteps, 1u);
	// no glints, nothing to step
	EXPECT_EQ(sh::HighlightDrawStep(false, true, false, false, true).glintSteps, 0u);
}

TEST(ScriptHighlight, TheDrawsFlagIsTheMeshOnScreenOrWhatItDrawsBesides)
{
	EXPECT_FALSE(sh::HighlightDrawStep(false, false, false, true, false).onScreen);
	EXPECT_TRUE(sh::HighlightDrawStep(false, true, false, true, false).onScreen);
	EXPECT_TRUE(sh::HighlightDrawStep(false, false, true, true, false).onScreen);
	EXPECT_TRUE(sh::HighlightDrawStep(false, false, true, false, false).onScreen);
}

TEST(ScriptHighlight, WhatADrawDrawsBesidesTheMesh)
{
	using sh::Info;
	// the glow of an active scroll
	EXPECT_FALSE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Bronze), false));
	EXPECT_TRUE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Bronze), true));
	EXPECT_FALSE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Silver), false));
	EXPECT_TRUE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Silver), true));
	// not for a did-you-know, lit or not
	EXPECT_FALSE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::DidYouKnow), false));
	EXPECT_FALSE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::DidYouKnow), true));
	// the gold scroll's sparkles, lit or not
	EXPECT_TRUE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Gold), false));
	EXPECT_TRUE(sh::DrawsBesidesTheMesh(static_cast<uint32_t>(Info::Gold), true));
}

TEST(ScriptHighlight, TheActiveEffectsFirstStepIsADoubleOne)
{
	EXPECT_EQ(sh::OwnedEffectSteps(true), 2u);
	EXPECT_EQ(sh::OwnedEffectSteps(false), 1u);
}

TEST(ScriptHighlight, SparklesAreSpreadOverTheirCycleAtCreation)
{
	// twenty angles drawn in [0, 2 pi) in order, then the cell offset in [0, 32)
	std::vector<std::string> draws;
	float next = 0.0f;
	const auto floatRand = [&](float x) {
		draws.push_back("float " + std::to_string(x));
		next += 0.25f;
		return next;
	};
	const auto fakeRand = [&](int32_t n) {
		draws.push_back("int " + std::to_string(n));
		return 0x125u; // kept as a byte
	};
	const auto sparkles = sh::MakeSparkles(floatRand, fakeRand);
	ASSERT_EQ(draws.size(), 21u);
	for (size_t i = 0; i < 20; ++i)
	{
		EXPECT_EQ(draws[i], "float " + std::to_string(6.2831855f));
		EXPECT_EQ(sparkles.sparkles[i].ms, -400 * static_cast<int32_t>(i));
		EXPECT_FLOAT_EQ(sparkles.sparkles[i].angle, 0.25f * static_cast<float>(i + 1));
	}
	EXPECT_EQ(draws[20], "int 32");
	EXPECT_EQ(sparkles.cellOffset, 0x25);
	EXPECT_EQ(sparkles.maxAlpha, 0xFF);
}

TEST(ScriptHighlight, SparklesWrapTheirCycleWithANewAngle)
{
	ecs::components::HighlightSparkles sparkles;
	sparkles.sparkles[0] = {.ms = 7990, .angle = 1.0f};
	sparkles.sparkles[1] = {.ms = 7985, .angle = 2.0f};
	sparkles.sparkles[2] = {.ms = -10, .angle = 3.0f};
	int calls = 0;
	const auto floatRand = [&](float) {
		++calls;
		return 5.0f;
	};
	// 8000 is kept; only past it the clock goes back by 8000
	sh::StepSparkles(sparkles, 10, floatRand);
	EXPECT_EQ(sparkles.sparkles[0].ms, 8000);
	EXPECT_FLOAT_EQ(sparkles.sparkles[0].angle, 1.0f);
	EXPECT_EQ(sparkles.sparkles[2].ms, 0);
	EXPECT_EQ(calls, 0);
	sh::StepSparkles(sparkles, 10, floatRand);
	EXPECT_EQ(sparkles.sparkles[0].ms, 10);
	EXPECT_FLOAT_EQ(sparkles.sparkles[0].angle, 5.0f);
	EXPECT_EQ(sparkles.sparkles[1].ms, 5);
	EXPECT_EQ(calls, 2);
	// a frame of no game time draws nothing
	sh::StepSparkles(sparkles, 0, floatRand);
	EXPECT_EQ(calls, 2);
}

TEST(ScriptHighlight, ASparkleRisesShrinksAndFades)
{
	ecs::components::HighlightSparkles sparkles;
	sparkles.position = {10.0f, 4.0f, -2.0f};
	sparkles.cellOffset = 30;
	sparkles.sparkles[0] = {.ms = -1, .angle = 0.0f};
	sparkles.sparkles[1] = {.ms = 0, .angle = 0.5f};
	sparkles.sparkles[2] = {.ms = 4000, .angle = 0.0f};
	sparkles.sparkles[3] = {.ms = 5000, .angle = 0.0f};
	sparkles.sparkles[4] = {.ms = 6500, .angle = 0.0f};
	sparkles.sparkles[5] = {.ms = 8000, .angle = 0.0f};
	// not drawn before its clock reaches 0, nor past the twenty
	EXPECT_FALSE(sh::SparkleSpriteOf(sparkles, 0).has_value());
	EXPECT_FALSE(sh::SparkleSpriteOf(sparkles, 20).has_value());
	const auto start = sh::SparkleSpriteOf(sparkles, 1);
	ASSERT_TRUE(start.has_value());
	EXPECT_EQ(start->position, sparkles.position);
	EXPECT_FLOAT_EQ(start->size, 2.0f);
	EXPECT_EQ(start->alpha, 0);
	EXPECT_FLOAT_EQ(start->angle, 0.5f);
	EXPECT_EQ(start->cell, 31u); // (0 + 1 + 30) mod 32
	// 4000 ms: s = 0.25, 5 units up, half width 1.8; 255 x 0.79999995 truncated
	const auto middle = sh::SparkleSpriteOf(sparkles, 2);
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->position.y, 9.0f);
	EXPECT_FLOAT_EQ(middle->position.x, 10.0f);
	EXPECT_FLOAT_EQ(middle->position.z, -2.0f);
	EXPECT_FLOAT_EQ(middle->size, (1.2f - 2.0f) * 0.25f + 2.0f);
	EXPECT_EQ(middle->alpha, 203);
	EXPECT_EQ(middle->cell, 16u); // (80 + 2 + 30) mod 32
	EXPECT_EQ(sh::SparkleSpriteOf(sparkles, 3)->alpha, 255);
	EXPECT_EQ(sh::SparkleSpriteOf(sparkles, 4)->alpha, 127);
	// the end of the cycle: 20 units up, half width 1.2, faded out
	const auto end = sh::SparkleSpriteOf(sparkles, 5);
	ASSERT_TRUE(end.has_value());
	EXPECT_FLOAT_EQ(end->position.y, 24.0f);
	EXPECT_FLOAT_EQ(end->size, (1.2f - 2.0f) + 2.0f);
	EXPECT_EQ(end->alpha, 0);
	EXPECT_EQ(end->cell, 3u); // (160 + 5 + 30) mod 32
}

TEST(ScriptHighlight, TheDrawsMeshBoundsAreItsBoxThroughThePlacedMatrix)
{
	// scale 2, no turn, at (1, 2, 3): the centre (2 cx + 1, 2 cy + 2, 2 cz + 3) and twice |h|
	affine::AffineMatrix placed;
	placed.m = {2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 2.0f, 1.0f, 2.0f, 3.0f};
	const auto bounds = sh::DrawnMeshBounds(placed, {0.5f, 1.0f, -0.25f}, {1.0f, 2.0f, 2.0f});
	EXPECT_FLOAT_EQ(bounds.centre.x, 2.0f);
	EXPECT_FLOAT_EQ(bounds.centre.y, 4.0f);
	EXPECT_FLOAT_EQ(bounds.centre.z, 2.5f);
	EXPECT_FLOAT_EQ(bounds.radius, 6.0f);
	// a quarter turn: the rows (0, 0, 1), (0, 1, 0), (-1, 0, 0); x' = -cz, z' = cx, the radius the same
	placed.m = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	const auto turned = sh::DrawnMeshBounds(placed, {0.5f, 1.0f, -0.25f}, {1.0f, 2.0f, 2.0f});
	EXPECT_FLOAT_EQ(turned.centre.x, 0.25f);
	EXPECT_FLOAT_EQ(turned.centre.y, 1.0f);
	EXPECT_FLOAT_EQ(turned.centre.z, 0.5f);
	EXPECT_FLOAT_EQ(turned.radius, 3.0f);
}

TEST(ScriptHighlight, SparklesStandOverTheMeshByItsRadius)
{
	const auto point = sh::SparklesPoint({1.0f, 2.0f, 3.0f}, 0.75f);
	EXPECT_FLOAT_EQ(point.x, 1.0f);
	EXPECT_FLOAT_EQ(point.y, 2.75f);
	EXPECT_FLOAT_EQ(point.z, 3.0f);
}

TEST(ScriptHighlight, AScrollIsClickedThroughASphereHalfAMetreWiderThanItsMesh)
{
	// r + 0.5 for every scroll; a did-you-know sign has none, it is clicked on its mesh
	EXPECT_EQ(sh::ClickRadius(static_cast<uint32_t>(sh::Info::Bronze), 2.0f), 2.5f);
	EXPECT_EQ(sh::ClickRadius(static_cast<uint32_t>(sh::Info::Silver), 0.75f), 1.25f);
	EXPECT_EQ(sh::ClickRadius(static_cast<uint32_t>(sh::Info::Gold), 0.0f), 0.5f);
	EXPECT_FALSE(sh::ClickRadius(static_cast<uint32_t>(sh::Info::DidYouKnow), 2.0f).has_value());
}

TEST(ScriptHighlight, TheSpriteStandsTowardsTheEyeAtItsRowsAlpha)
{
	// 1.4 radii towards the eye, 2 radii in size, white at the row's alpha (bronze 0, did-you-know 0x32, silver 0x96, gold
	// 0x64), cell 0
	const glm::vec3 centre {10.0f, 5.0f, 10.0f};
	const glm::vec3 eye {10.0f, 5.0f, 30.0f};
	const auto silver = sh::SpriteOf(static_cast<uint32_t>(sh::Info::Silver), false, centre, 2.0f, eye);
	ASSERT_TRUE(silver.has_value());
	EXPECT_FLOAT_EQ(silver->position.z, 10.0f + 2.8f);
	EXPECT_FLOAT_EQ(silver->size, 4.0f);
	EXPECT_EQ(silver->argb, 0x96FFFFFFu);
	EXPECT_EQ(silver->cell, 0u);
	EXPECT_EQ(sh::SpriteOf(static_cast<uint32_t>(sh::Info::Bronze), true, centre, 2.0f, eye)->argb, 0x00FFFFFFu);
	EXPECT_EQ(sh::SpriteOf(static_cast<uint32_t>(sh::Info::Gold), false, centre, 2.0f, eye)->argb, 0x64FFFFFFu);
	// at least 0.0001 in size
	EXPECT_FLOAT_EQ(sh::SpriteOf(static_cast<uint32_t>(sh::Info::Gold), false, centre, 0.0f, eye)->size, 0.0001f);
	// a did-you-know shows it until it is lit
	EXPECT_EQ(sh::SpriteOf(static_cast<uint32_t>(sh::Info::DidYouKnow), false, centre, 2.0f, eye)->argb, 0x32FFFFFFu);
	EXPECT_FALSE(sh::SpriteOf(static_cast<uint32_t>(sh::Info::DidYouKnow), true, centre, 2.0f, eye).has_value());
}

TEST(ScriptHighlight, ChallengeIdsThatSave)
{
	// the challenges whose click saves the game
	for (uint32_t id = 0; id < 0x50; ++id)
	{
		const bool expected = id == 0x38 || id == 0x3B || id == 0x3C || id == 0x3D;
		EXPECT_EQ(sh::SavesGameWhenClicked(id), expected) << id;
	}
}

TEST(ScriptHighlight, ValidToTap)
{
	EXPECT_TRUE(sh::ValidToTap(true, false, 4143));
	EXPECT_FALSE(sh::ValidToTap(true, true, 0));
	EXPECT_FALSE(sh::ValidToTap(false, false, 53));
	EXPECT_TRUE(sh::ValidToTap(false, true, 53));
	EXPECT_FALSE(sh::ValidToTap(false, true, 0));
}

TEST(ScriptHighlight, TappedOutsideTheInfluence)
{
	namespace tap = openblack::ecs::hand_tap;
	// a highlight needs no influence: a valid one is tapped wherever the hand is
	EXPECT_TRUE(tap::SendsTap(false, false, true, false));
	EXPECT_TRUE(tap::SendsTap(true, false, true, false));
	// every other class needs the hand in the influence
	EXPECT_FALSE(tap::SendsTap(false, true, true, false));
	EXPECT_TRUE(tap::SendsTap(true, true, true, false));
	// never when it is not valid to tap or it is flagged cannot be picked up
	EXPECT_FALSE(tap::SendsTap(true, false, false, false));
	EXPECT_FALSE(tap::SendsTap(false, false, true, true));
	EXPECT_FALSE(tap::SendsTap(true, true, true, true));
}

TEST(ScriptHighlight, ATapOnAStartedScrollCountsTheReminderAndRunsTheChallengesReminder)
{
	using Plan = sh::TapPlan;
	// a scroll with its challenge, tapped by the local player: the help's Reminder event (0x22), lit, no tap sound, then
	// the challenge's reminder
	auto plan = sh::PlanTap(true, false, true);
	EXPECT_EQ(plan.event, Plan::Event::Reminder);
	EXPECT_TRUE(plan.activates);
	EXPECT_FALSE(plan.tapSound);
	EXPECT_EQ(plan.then, Plan::Then::Reminder);
	EXPECT_EQ(static_cast<int32_t>(help_profile::Event::Reminder), 0x22);
	// tapped by another player: no event counted, but the reminder all the same
	plan = sh::PlanTap(true, false, false);
	EXPECT_EQ(plan.event, Plan::Event::None);
	EXPECT_TRUE(plan.activates);
	EXPECT_EQ(plan.then, Plan::Then::Reminder);
	// with no challenge: the script activation event (0x23) and nothing else
	plan = sh::PlanTap(false, false, true);
	EXPECT_EQ(plan.event, Plan::Event::ScriptActivate);
	EXPECT_FALSE(plan.activates);
	EXPECT_EQ(plan.then, Plan::Then::Nothing);
	EXPECT_EQ(static_cast<int32_t>(help_profile::Event::ScriptActivate), 0x23);
	EXPECT_EQ(sh::PlanTap(false, true, false).event, Plan::Event::None);
}

TEST(ScriptHighlight, ATapOnADidYouKnowSoundsForTheLocalPlayerAndOpensItsBubble)
{
	using Plan = sh::TapPlan;
	auto plan = sh::PlanTap(true, true, true);
	EXPECT_EQ(plan.event, Plan::Event::Reminder);
	EXPECT_TRUE(plan.activates);
	EXPECT_TRUE(plan.tapSound);
	EXPECT_EQ(plan.then, Plan::Then::Bubble);
	plan = sh::PlanTap(true, true, false);
	EXPECT_FALSE(plan.tapSound);
	EXPECT_EQ(plan.then, Plan::Then::Bubble);
}

TEST(ScriptHighlight, TapToolTip)
{
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::ScriptHighlight), 0u);
	EXPECT_EQ(sh::OverwriteTapToolTip(ObjectType::Creature), 0xEF2u);
}

TEST(ScriptHighlight, PulseOfProcessHighlights)
{
	// the highlights' pulse with the 100 ms turn: 0.5 rad a turn
	sh::Pulse pulse;
	sh::StepPulse(pulse, 100);
	EXPECT_NEAR(pulse.phase, 0.5f, 1e-6f);
	EXPECT_FLOAT_EQ(pulse.previous, 0.0f);
	EXPECT_NEAR(pulse.value, (1.0f - std::cos(0.5f)) * 0.5f, 1e-6f);
	const float first = pulse.value;
	sh::StepPulse(pulse, 100);
	EXPECT_FLOAT_EQ(pulse.previous, first);
	// the phase loses 2 pi once it is above
	sh::Pulse wrap {6.2f, 0.0f, 0.0f};
	sh::StepPulse(wrap, 100);
	EXPECT_NEAR(wrap.phase, 6.7f - 6.28319f, 1e-5f);
}

TEST(ScriptHighlight, ThePulseGoesRoundOnceInThirteenTurns)
{
	// twelve steps of half a radian stay inside the turn; the thirteenth passes it and starts again
	sh::Pulse pulse;
	for (int i = 0; i < 12; ++i)
	{
		sh::StepPulse(pulse, 100);
	}
	EXPECT_NEAR(pulse.phase, 6.0f, 1e-5f);
	sh::StepPulse(pulse, 100);
	EXPECT_NEAR(pulse.phase, 6.5f - glm::two_pi<float>(), 1e-5f);
}

TEST(ScriptHighlight, ActivePulseAndGlow)
{
	// lerp, bound to [0, 1], x 0.6 + 0.4
	const sh::Pulse pulse {0.0f, 1.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.5f), 0.7f, 1e-6f);
	EXPECT_NEAR(sh::ActivePulse(pulse, 0.0f), 0.4f, 1e-6f);
	const sh::Pulse over {0.0f, 3.0f, 0.0f};
	EXPECT_NEAR(sh::ActivePulse(over, 0.9f), 1.0f, 1e-6f);
	// silver's fixed colour, the others 0x50 alpha only at a pulse of 1
	EXPECT_EQ(sh::ActiveGlowArgb(2, 0.7f), 0x14B4DCFFu);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 0.7f), 0x00FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(3, 1.0f), 0x50FFFF00u);
	EXPECT_EQ(sh::ActiveGlowArgb(0, 1.0f), 0x50FFFF00u);
}

// Only a started scroll glows: a column standing at its mesh's centre, facing the eye and three times as tall, in its
// colour; a scroll not started and a did-you-know sign, lit or not, have none
TEST(ScriptHighlight, AStartedScrollGlowsAsAColumnAtItsCentre)
{
	const glm::vec3 centre {10.0f, 5.0f, 10.0f};
	const glm::vec3 eye {40.0f, 25.0f, 10.0f};
	const auto silver = sh::GlowOf(static_cast<uint32_t>(sh::Info::Silver), true, centre, eye, 0.7f);
	ASSERT_TRUE(silver.has_value());
	EXPECT_EQ(silver->model, graphics::light_column::Model(centre, eye));
	EXPECT_EQ(silver->argb, 0x14B4DCFFu);
	EXPECT_EQ(sh::GlowOf(static_cast<uint32_t>(sh::Info::Gold), true, centre, eye, 1.0f)->argb, 0x50FFFF00u);
	EXPECT_EQ(sh::GlowOf(static_cast<uint32_t>(sh::Info::Bronze), true, centre, eye, 0.4f)->argb, 0x00FFFF00u);
	EXPECT_FALSE(sh::GlowOf(static_cast<uint32_t>(sh::Info::Silver), false, centre, eye, 0.7f).has_value());
	EXPECT_FALSE(sh::GlowOf(static_cast<uint32_t>(sh::Info::DidYouKnow), true, centre, eye, 1.0f).has_value());
}

TEST(ScriptHighlight, DidYouKnowReadLists)
{
	sh::did_you_know_read::Clear();
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, DykCategory::Misc));
	sh::did_you_know_read::MarkRead(4143, DykCategory::Misc);
	EXPECT_TRUE(sh::did_you_know_read::IsRead(4143, DykCategory::Misc));
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, DykCategory::Creature)); // one list a category
	EXPECT_EQ(sh::did_you_know_read::Total(), 1u);
	// at most 48 a category
	for (uint32_t text = 0; text < 60; ++text)
	{
		sh::did_you_know_read::MarkRead(10000 + text, DykCategory::Navigation);
	}
	EXPECT_EQ(sh::did_you_know_read::Total(), 1u + 48u);
	EXPECT_FALSE(sh::did_you_know_read::IsRead(10055, DykCategory::Navigation));
	// past DYK_CATEGORY 4: never read
	EXPECT_FALSE(sh::did_you_know_read::IsRead(4143, static_cast<DykCategory>(7)));
	sh::did_you_know_read::Clear();
	EXPECT_EQ(sh::did_you_know_read::Total(), 0u);
}

TEST(ScriptHighlight, ASavedGamesDidYouKnowListIsWrittenOverTheListAndHoldsItsSavedCount)
{
	namespace dyk = sh::did_you_know_read;
	dyk::Clear();
	dyk::MarkRead(1, DykCategory::Creature);
	dyk::MarkRead(2, DykCategory::Creature);
	dyk::MarkRead(3, DykCategory::Creature);
	dyk::MarkRead(9, DykCategory::Miracles);
	dyk::MarkRead(10, DykCategory::Miracles);
	// the saved texts over the list's start, then the saved count
	const std::array<uint32_t, 2> saved {4143, 4150};
	dyk::Load(DykCategory::Creature, saved, 2);
	EXPECT_TRUE(std::ranges::equal(dyk::In(DykCategory::Creature), saved));
	EXPECT_FALSE(dyk::IsRead(3, DykCategory::Creature));
	// a count past the texts keeps what the list held there, and nothing past its end
	dyk::Load(DykCategory::Miracles, std::span<const uint32_t>(saved).first(1), 3);
	EXPECT_TRUE(std::ranges::equal(dyk::In(DykCategory::Miracles), std::array<uint32_t, 3> {4143, 10, 0}));
	dyk::MarkRead(1, DykCategory::Creature);
	EXPECT_TRUE(std::ranges::equal(dyk::In(DykCategory::Creature), std::array<uint32_t, 3> {4143, 4150, 1}));
	// the other categories and one past the last are left alone
	dyk::Load(static_cast<DykCategory>(7), saved, 2);
	EXPECT_TRUE(dyk::In(DykCategory::Navigation).empty());
	EXPECT_EQ(dyk::Total(), 6u);
	dyk::Clear();
}

TEST(ScriptHighlight, TheLibraryListsTheTipsOfACategoryInTheOrderTheyWereRead)
{
	sh::did_you_know_read::Clear();
	sh::did_you_know_read::MarkRead(4130, DykCategory::Creature);
	sh::did_you_know_read::MarkRead(4120, DykCategory::Creature);
	sh::did_you_know_read::MarkRead(4130, DykCategory::Creature);
	sh::did_you_know_read::MarkRead(4125, DykCategory::VillageLife);
	const auto creature = sh::did_you_know_read::In(DykCategory::Creature);
	ASSERT_EQ(creature.size(), 2u);
	EXPECT_EQ(creature[0], 4130u);
	EXPECT_EQ(creature[1], 4120u);
	EXPECT_EQ(sh::did_you_know_read::In(DykCategory::VillageLife).size(), 1u);
	EXPECT_TRUE(sh::did_you_know_read::In(DykCategory::Navigation).empty());
	// No category after the fifth
	EXPECT_TRUE(sh::did_you_know_read::In(static_cast<DykCategory>(5)).empty());
	sh::did_you_know_read::Clear();
}

// A script's YPOS on a highlight: its draw height and its altitude are set, and its model moves to the ground there plus
// that height (here no land: the ground is 0); a thing that is no highlight has none
TEST(ScriptHighlight, AScriptsHeightIsTheHighlightsAltitudeAndMovesItsModel)
{
	const openblack::test::RestoreService<Locator::entitiesRegistry> keepRegistry;
	const openblack::test::RestoreService<Locator::terrainSystem> keepTerrain;
	Locator::terrainSystem::reset();
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto scroll = registry.Create();
	registry.Assign<ecs::components::ScriptHighlight>(scroll, ecs::components::ScriptHighlight {.x = 100.0f, .z = 250.0f});
	registry.Assign<ecs::components::Transform>(scroll, glm::vec3(100.0f, 0.0f, 250.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(sh::GetYPos(scroll), 0.0f);
	sh::SetYPos(scroll, 3.5f);
	EXPECT_EQ(sh::GetYPos(scroll), 3.5f);
	const auto& highlight = registry.Get<const ecs::components::ScriptHighlight>(scroll);
	ASSERT_TRUE(highlight.drawHeight.has_value());
	EXPECT_EQ(*highlight.drawHeight, 3.5f);
	EXPECT_EQ(registry.Get<const ecs::components::Transform>(scroll).position, glm::vec3(100.0f, 3.5f, 250.0f));
	// not a highlight
	const auto rock = registry.Create();
	sh::SetYPos(rock, 9.0f);
	EXPECT_EQ(sh::GetYPos(rock), 0.0f);
	EXPECT_FALSE(registry.AllOf<ecs::components::ScriptHighlight>(rock));
}

// SET_DRAW_HIGHLIGHT 0 takes the scrolls off the screen but leaves the did-you-know signs; a hidden highlight is never
// drawn
TEST(ScriptHighlight, AScrollIsDrawnOnlyWhileTheScriptsDrawHighlightsButASignAlways)
{
	EXPECT_TRUE(sh::IsShown(0, 1, false));
	EXPECT_FALSE(sh::IsShown(0, 0, false));
	EXPECT_TRUE(sh::IsShown(0, 0, true));
	EXPECT_TRUE(sh::IsShown(0, 1, true));
	// the scripts' value as they gave it: anything but 0 draws
	EXPECT_TRUE(sh::IsShown(0, 2, false));
	EXPECT_TRUE(sh::IsShown(0, -1, false));
	// hidden
	EXPECT_FALSE(sh::IsShown(1, 1, false));
	EXPECT_FALSE(sh::IsShown(1, 1, true));
	EXPECT_FALSE(sh::IsShown(-1, 0, true));
}
