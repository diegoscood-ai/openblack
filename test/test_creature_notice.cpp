/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureNoticeAgendas.h"
#include "Creature/CreaturePlanActions.h"

using namespace openblack;
using namespace openblack::creature_mind;
using openblack::creature_face::Cue;

namespace
{
constexpr uint32_t k_Thing = 7;
constexpr glm::vec2 k_Camera {30.0f, 40.0f};

/// A random stream that answers the given value and counts the draws, each with its range
struct Draws
{
	uint32_t answer {0};
	std::vector<uint32_t> ranges;
	[[nodiscard]] Random Stream()
	{
		return [this](uint32_t range) {
			ranges.push_back(range);
			return answer;
		};
	}
};

void ExpectPoint(const Step& step, float seconds)
{
	EXPECT_EQ(step.kind, Step::Kind::Object);
	EXPECT_EQ(step.order.kind, ObjectOrder::Kind::PointAt);
	EXPECT_EQ(step.order.object, k_Thing);
	EXPECT_EQ(step.order.seconds, seconds);
	EXPECT_EQ(step.face, Cue::Amazed);
	ASSERT_TRUE(step.gaze.has_value());
	EXPECT_EQ(step.gaze->object, k_Thing);
	EXPECT_TRUE(step.gaze->itself);
	EXPECT_FALSE(step.main);
}

void ExpectTurnToCamera(const Step& step)
{
	EXPECT_EQ(step.kind, Step::Kind::Move);
	EXPECT_EQ(step.movement.kind, Movement::Kind::TurnToFace);
	EXPECT_EQ(step.movement.point, k_Camera);
	ASSERT_TRUE(step.gaze.has_value());
	EXPECT_TRUE(step.gaze->camera);
	EXPECT_EQ(step.face, Cue::None);
	EXPECT_FALSE(step.main);
}

void ExpectLook(const Step& step, Cue face)
{
	EXPECT_EQ(step.kind, Step::Kind::LookAtCamera);
	EXPECT_EQ(step.seconds, 1.0f);
	EXPECT_FALSE(step.turns.has_value());
	EXPECT_EQ(step.face, face);
	ASSERT_TRUE(step.gaze.has_value());
	EXPECT_TRUE(step.gaze->camera);
	EXPECT_TRUE(step.main);
}

void ExpectClip(const Step& step, size_t animation)
{
	EXPECT_EQ(step.kind, Step::Kind::Action);
	EXPECT_EQ(step.animation, animation);
	EXPECT_EQ(step.face, Cue::None);
	EXPECT_FALSE(step.main);
}
} // namespace

TEST(CreatureNotice, AHelpfulDeedIsPointedAtThenTheCameraLookedAtWithCuriosityAndCompassionThenOneOfTwoClips)
{
	for (const uint32_t answer : {0U, 1U})
	{
		Draws draws {.answer = answer};
		const auto agenda = NoticeDeed(NoticeKind::Helpful, k_Thing, false, k_Camera, draws.Stream());
		ASSERT_EQ(agenda.size(), 5U);
		ExpectPoint(agenda[0], 2.0f);
		ExpectTurnToCamera(agenda[1]);
		ExpectLook(agenda[2], Cue::Curiosity);
		ExpectLook(agenda[3], Cue::Compassion);
		ExpectClip(agenda[4], answer == 0 ? 55 : 220);
		EXPECT_EQ(draws.ranges, std::vector<uint32_t> {2});
	}
}

TEST(CreatureNotice, AnAggressiveDeedIsPointedAtBrieflyLookedAtInAngerAndEndsInTheAngryClipAfterTheDazedOneOneTimeInThree)
{
	Draws dazed {.answer = 0};
	auto agenda = NoticeDeed(NoticeKind::Aggressive, k_Thing, false, k_Camera, dazed.Stream());
	ASSERT_EQ(agenda.size(), 5U);
	ExpectPoint(agenda[0], 0.8f);
	ExpectTurnToCamera(agenda[1]);
	ExpectLook(agenda[2], Cue::Anger);
	ExpectClip(agenda[3], 218);
	ExpectClip(agenda[4], 53);
	EXPECT_EQ(dazed.ranges, std::vector<uint32_t> {3});

	Draws notDazed {.answer = 2};
	agenda = NoticeDeed(NoticeKind::Aggressive, k_Thing, false, k_Camera, notDazed.Stream());
	ASSERT_EQ(agenda.size(), 4U);
	ExpectLook(agenda[2], Cue::Anger);
	ExpectClip(agenda[3], 53);
}

TEST(CreatureNotice, TheNeutralStealingAndPlayfulDeedsLookTwiceWithCuriosityAndPlayTheirOwnClipsWithNoDraw)
{
	const std::vector<std::pair<NoticeKind, size_t>> kinds {
	    {NoticeKind::Neutral, 220}, {NoticeKind::Stealing, 60}, {NoticeKind::Playful, 55}};
	for (const auto& [kind, clip] : kinds)
	{
		Draws draws;
		const auto agenda = NoticeDeed(kind, k_Thing, false, k_Camera, draws.Stream());
		ASSERT_EQ(agenda.size(), 5U);
		ExpectPoint(agenda[0], 2.0f);
		ExpectTurnToCamera(agenda[1]);
		ExpectLook(agenda[2], Cue::Curiosity);
		ExpectLook(agenda[3], Cue::Curiosity);
		ExpectClip(agenda[4], clip);
		EXPECT_TRUE(draws.ranges.empty());
	}
}

TEST(CreatureNotice, WithItsHandFullItDoesNotPointAndWithNoCameraItDoesNotTurn)
{
	Draws draws;
	auto agenda = NoticeDeed(NoticeKind::Neutral, k_Thing, true, k_Camera, draws.Stream());
	ASSERT_EQ(agenda.size(), 4U);
	ExpectTurnToCamera(agenda[0]);

	agenda = NoticeDeed(NoticeKind::Neutral, k_Thing, false, std::nullopt, draws.Stream());
	ASSERT_EQ(agenda.size(), 4U);
	ExpectPoint(agenda[0], 2.0f);
	ExpectLook(agenda[1], Cue::Curiosity);
}

TEST(CreatureNotice, TheTablesFiveNoticingActionsAreCarriedOutOnTheThingTheDeedWasDoneTo)
{
	using creature_plan_actions::Build;
	const std::vector<std::pair<const char*, Build>> actions {{"NoticeHelpfulAction", Build::NoticeHelpful},
	                                                          {"NoticeAggressiveAction", Build::NoticeAggressive},
	                                                          {"NoticeAction", Build::NoticeNeutral},
	                                                          {"NoticeStealingAction", Build::NoticeStealing},
	                                                          {"NoticePlayfulAction", Build::NoticePlayful}};
	creature_plan_actions::Situation situation;
	situation.camera = k_Camera;
	situation.hasPlayer = true;
	for (const auto& [name, build] : actions)
	{
		const auto* executor = creature_plan_actions::For(name);
		ASSERT_NE(executor, nullptr) << name;
		EXPECT_EQ(executor->build, build);
		Draws draws {.answer = 1};
		const auto agenda =
		    creature_plan_actions::Agenda(*executor, k_Thing, glm::vec2 {0.0f}, situation, draws.Stream(), std::nullopt);
		ASSERT_TRUE(agenda.has_value()) << name;
		ExpectPoint(agenda->front(), build == Build::NoticeAggressive ? 0.8f : 2.0f);
		ExpectTurnToCamera(agenda->at(1));
		// Without the thing there is nothing to notice
		EXPECT_FALSE(
		    creature_plan_actions::Agenda(*executor, std::nullopt, glm::vec2 {0.0f}, situation, draws.Stream(), std::nullopt)
		        .has_value());
	}
	// Holding something, it does not point; without a player it does not turn to the camera
	situation.handFull = true;
	situation.hasPlayer = false;
	Draws draws;
	const auto agenda = creature_plan_actions::Agenda(*creature_plan_actions::For("NoticeAction"), k_Thing, glm::vec2 {0.0f},
	                                                  situation, draws.Stream(), std::nullopt);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3U);
	ExpectLook(agenda->front(), Cue::Curiosity);
}
