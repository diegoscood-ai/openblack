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
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreaturePlayedShare.h"

using namespace openblack;
using namespace openblack::creature_layers;
using skeletal_animation::Animation;
using skeletal_animation::Matrix;
using skeletal_animation::Pose;

namespace
{
constexpr float k_Tolerance = 1e-4f;
constexpr float k_Pi = std::numbers::pi_v<float>;

/// Each of a body's three animations as long as the others
BodyDurations DurationsEach(std::optional<uint32_t> duration)
{
	return {duration, duration, duration};
}

BodyLengths LengthsEach(std::optional<int32_t> lengthMs)
{
	return {lengthMs, lengthMs, lengthMs};
}

void ExpectMatrixNear(const Matrix& a, const Matrix& b)
{
	for (size_t r = 0; r < 3; ++r)
	{
		for (size_t c = 0; c < 3; ++c)
		{
			EXPECT_NEAR(a.at(r).at(c), b.at(r).at(c), k_Tolerance) << "row " << r << " column " << c;
		}
	}
}

/// A skeleton of a root with a bone either side of it, all with no rest rotation
skeletal_animation::Skeleton Symmetric(std::array<glm::mat4, 3>& rest)
{
	rest = {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 0.0f)),
	        glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, 2.0f, 0.0f))};
	const std::array<uint32_t, 3> parents {skeletal_animation::k_NoParent, 0, 0};
	return skeletal_animation::Skeleton::FromRestMatrices(parents, rest);
}

/// A still animation of one keyframe per entry, moving the given bones
Animation Frames(std::vector<uint32_t> rotated, std::vector<std::vector<glm::vec3>> eulers,
                 std::vector<uint32_t> translated = {}, std::vector<std::vector<glm::vec3>> translations = {},
                 uint32_t duration = 1000)
{
	Animation animation {.duration = duration,
	                     .looping = true,
	                     .rotatedJoints = std::move(rotated),
	                     .translatedJoints = std::move(translated),
	                     .frames = {}};
	for (size_t i = 0; i < eulers.size(); ++i)
	{
		animation.frames.push_back(
		    {.eulerAngles = eulers[i], .translations = i < translations.size() ? translations[i] : std::vector<glm::vec3> {}});
	}
	return animation;
}
} // namespace

TEST(CreatureLayers, BiggerCreaturesPlayMoreSlowly)
{
	EXPECT_NEAR(PlaybackRate(0.0f), 1.6f, k_Tolerance);
	EXPECT_NEAR(PlaybackRate(1.0f), 1.175f, k_Tolerance);
	EXPECT_NEAR(PlaybackRate(2.0f), 0.75f, k_Tolerance);
}

TEST(CreatureLayers, AnActionPlaysOnceThenTheBodyStands)
{
	const BodyAction stand {};
	EXPECT_FALSE(IsPlaying(stand));
	EXPECT_EQ(CurrentAnimation(stand), animations::k_Stand);

	auto body = PlayOnce(stand, animations::k_Tired, true);
	ASSERT_TRUE(body.has_value());
	EXPECT_TRUE(body->mirrored);
	EXPECT_EQ(CurrentAnimation(*body), animations::k_Tired);
	// Nothing else starts while it plays
	EXPECT_FALSE(PlayOnce(*body, animations::k_Happy, false).has_value());
	EXPECT_FALSE(PlaySequence(*body, 1, 2, 3).has_value());

	auto played = AdvanceBody(*body, 600.0f, DurationsEach(1000));
	EXPECT_TRUE(IsPlaying(played));
	EXPECT_NEAR(played.timeMs, 600.0f, k_Tolerance);
	played = AdvanceBody(played, 400.0f, DurationsEach(1000));
	EXPECT_FALSE(IsPlaying(played));
	EXPECT_FALSE(played.mirrored);
}

TEST(CreatureLayers, TheSharePlayedIsTheWholeMillisecondsOverTheLength)
{
	auto body = *PlayOnce({}, animations::k_Tired, false);
	EXPECT_EQ(ShareAnimation(body), animations::k_Tired);
	body.timeMs = 600.7f;
	EXPECT_FLOAT_EQ(PlayedShare(body, 0, 1000.0f), 0.6f);
	body.timeMs = 999.0f;
	EXPECT_FLOAT_EQ(PlayedShare(body, 0, 1000.0f), 0.999f);
	// Standing, the share is the stand pose's time at the breath's point
	const BodyAction stand {};
	EXPECT_EQ(ShareAnimation(stand), animations::k_Stand);
	EXPECT_FLOAT_EQ(PlayedShare(stand, 1104, 2208.0f), 0.5f);
}

TEST(CreatureLayers, ASequenceSharesItsLoopsLengthInEveryPart)
{
	auto body = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	EXPECT_EQ(ShareAnimation(body), animations::k_Sit);
	body = AdvanceBody(body, 500.0f, {500, 400, 400});
	EXPECT_EQ(ShareAnimation(body), animations::k_Sit);
	body = AdvanceBody(EndLoop(body), 10.0f, {500, 400, 400});
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndSit);
	EXPECT_EQ(ShareAnimation(body), animations::k_Sit);
}

TEST(CreatureLayers, ADanceSharesTheStandsLengthUntilItsMovesAndTheMovesAfter)
{
	auto body = *PlayDance({}, 1000.0f);
	// The start is measured against the stand pose
	EXPECT_EQ(ShareAnimation(body), animations::k_Stand);
	body.timeMs = 300.0f;
	EXPECT_FLOAT_EQ(PlayedShare(body, 0.0f, 2000.0f), 0.15f);
	body = AdvanceBodyByTurn(body, 1000, LengthsEach(500)).layer;
	ASSERT_EQ(body.phase, BodyAction::Phase::Loop);
	body.animations[1] = animations::k_FirstDanceMove + 2;
	EXPECT_EQ(ShareAnimation(body), animations::k_FirstDanceMove + 2);
	// A move's time is as it is drawn: the real time over the move's own, on its length
	body.timeMs = 250.0f;
	EXPECT_FLOAT_EQ(PlayedShare(body, 0.0f, 2000.0f), 0.25f);
	// The finish after a move is measured against the last move
	body = EndDance(body);
	ASSERT_EQ(body.phase, BodyAction::Phase::End);
	EXPECT_EQ(ShareAnimation(body), animations::k_FirstDanceMove + 2);
}

TEST(CreatureLayers, ADanceEndedInItsStartSharesTheStandsLengthInItsFinish)
{
	auto body = EndDance(*PlayDance({}, 1000.0f));
	body = AdvanceBodyByTurn(body, 1000, LengthsEach(500)).layer;
	ASSERT_EQ(body.phase, BodyAction::Phase::End);
	EXPECT_EQ(ShareAnimation(body), animations::k_Stand);
}

TEST(CreatureLayers, WithNoActionTheShareIsTheIdleTimeOverTheStandsLength)
{
	// the legs' time of a walk, in whole milliseconds
	EXPECT_FLOAT_EQ(PlayedShare({}, 1100.9f, 2200.0f), 0.5f);
}

TEST(CreaturePlayedShare, AnActionOnAThingIsNamedByWhatItKeepsOrLetsGoElseTheStandPose)
{
	using creature_object_actions::Kind;
	constexpr size_t k_Played = 97;
	for (const auto kind : {Kind::Keep, Kind::PutDown, Kind::Discard, Kind::Lob, Kind::Eat})
	{
		EXPECT_EQ(creature_played_share::ObjectActionAnimation(kind, k_Played), k_Played);
	}
	for (const auto kind : {Kind::PickUp, Kind::Destroy, Kind::Throw, Kind::Point, Kind::TakeFromHand})
	{
		EXPECT_EQ(creature_played_share::ObjectActionAnimation(kind, k_Played), animations::k_Stand);
	}
	EXPECT_EQ(creature_played_share::CatchAnimation(true), creature_catch::k_CatchStep);
	EXPECT_EQ(creature_played_share::CatchAnimation(false), animations::k_Stand);
	EXPECT_FLOAT_EQ(creature_played_share::Share(750.9f, 1500.0f), 0.5f);
}

TEST(CreatureLayers, AMissingAnimationEndsAtOnce)
{
	const auto body = PlayOnce({}, animations::k_Tired, false);
	ASSERT_TRUE(body.has_value());
	EXPECT_FALSE(IsPlaying(AdvanceBody(*body, 1.0f, DurationsEach(std::nullopt))));
}

TEST(CreatureLayers, ASitStartsLoopsUntilToldThenEnds)
{
	auto body = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	EXPECT_EQ(CurrentAnimation(body), animations::k_StartSit);
	const BodyDurations durations {500, 400, 800};
	// The start played to its end starts the loop, which moves on by the whole step: 500 ms of a loop of 400
	body = AdvanceBody(body, 500.0f, durations);
	EXPECT_TRUE(IsLooping(body));
	EXPECT_EQ(CurrentAnimation(body), animations::k_Sit);
	EXPECT_NEAR(body.timeMs, 100.0f, k_Tolerance);
	// The loop wraps round for as long as it isn't told to end
	body = AdvanceBody(body, 600.0f, durations);
	EXPECT_TRUE(IsLooping(body));
	EXPECT_NEAR(body.timeMs, 300.0f, k_Tolerance);
	// Told to end, the end starts and plays on by the same step
	body = AdvanceBody(EndLoop(body), 10.0f, durations);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndSit);
	EXPECT_NEAR(body.timeMs, 10.0f, k_Tolerance);
	body = AdvanceBody(body, 790.0f, durations);
	EXPECT_FALSE(IsPlaying(body));
}

TEST(CreatureLayers, AHurriedBodyEndsItsActionAtTwiceTheSpeed)
{
	// A body told to hurry ends its sequence at two times the usual speed, through the end of the sequence
	auto body = *PlaySequence({}, animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo);
	const BodyDurations durations {500, 1000, 1000};
	body = AdvanceBody(AdvanceBody(body, 500.0f, durations), 100.0f, durations);
	ASSERT_TRUE(IsLooping(body));
	// the end starts and plays on by the step, at twice its milliseconds
	body = AdvanceBody(EndLoop(Hurry(body)), 16.0f, durations);
	EXPECT_EQ(CurrentAnimation(body), animations::k_EndPoo);
	EXPECT_NEAR(body.timeMs, 32.0f, k_Tolerance);
	body = AdvanceBody(body, 100.0f, durations);
	EXPECT_NEAR(body.timeMs, 232.0f, k_Tolerance);
	body = AdvanceBody(body, 400.0f, durations);
	EXPECT_FALSE(IsPlaying(body));
	// The body stands again and plays the next action at the usual speed
	body = *PlayOnce(body, animations::k_Happy, false);
	body = AdvanceBody(body, 100.0f, DurationsEach(1000));
	EXPECT_NEAR(body.timeMs, 100.0f, k_Tolerance);
}

TEST(CreatureLayers, ABodyThatStandsIsNotHurried)
{
	EXPECT_FALSE(Hurry({}).hurried);
}

TEST(CreatureLayers, ABodyTimedByWhatPlaysItStaysWherePut)
{
	// A fight sets its fighter's move at the move's own time each frame; the frame mustn't move it on as well, or the
	// time shown would run ahead by however long each frame took, and go back when a frame takes less
	const BodyAction move {
	    .kind = BodyAction::Kind::Sequence,
	    .phase = BodyAction::Phase::Loop,
	    .animations = {120, 120, 120},
	    .timeMs = 70.0f,
	    .timedByPlayer = true,
	};
	for (const auto milliseconds : {0.0f, 1.0f, 16.0f, 700.0f})
	{
		const auto played = AdvanceBody(move, milliseconds, DurationsEach(500));
		EXPECT_EQ(played.phase, BodyAction::Phase::Loop);
		EXPECT_FLOAT_EQ(played.timeMs, 70.0f);
	}
}

TEST(CreatureLayers, AFaceRunsBackBeforeTheNextStarts)
{
	auto face = PullFace({}, 16, 5000.0f);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 16u);
	face = AdvanceFace(face, 300.0f, 1000);
	EXPECT_NEAR(face.timeMs, 300.0f, k_Tolerance);
	// Another face is pulled: the smile runs back to its start at full speed, then the growl starts
	face = PullFace(face, 18, 5000.0f);
	face = AdvanceFace(face, 200.0f, 1000);
	EXPECT_EQ(face.current, 16u);
	EXPECT_NEAR(face.timeMs, 100.0f, k_Tolerance);
	face = AdvanceFace(face, 200.0f, 1000);
	EXPECT_EQ(face.current, 18u);
	EXPECT_NEAR(face.timeMs, 0.0f, k_Tolerance);
	// Relaxed: it runs back a quarter as fast
	face = AdvanceFace(face, 400.0f, 1000);
	face = RelaxFace(face);
	face = AdvanceFace(face, 400.0f, 1000);
	EXPECT_EQ(face.current, 18u);
	EXPECT_NEAR(face.timeMs, 300.0f, k_Tolerance);
	face = AdvanceFace(face, 1200.0f, 1000);
	EXPECT_FALSE(face.current.has_value());
}

TEST(CreatureLayers, AFaceHoldsItsExpressionRatherThanLooping)
{
	auto face = PullFace({}, 20, 3000.0f);
	face = AdvanceFace(face, 10.0f, 250);
	// Played through, it holds its last frame without starting over: the mouth doesn't open and close again
	for (int frame = 0; frame < 100; ++frame)
	{
		face = AdvanceFace(face, 16.0f, 250);
		EXPECT_EQ(face.current, 20u);
		if (frame > 16)
		{
			EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
		}
	}
}

TEST(CreatureLayers, AFaceRelaxesWhenItsTimeIsUp)
{
	auto face = PullFace({}, 21, 1000.0f, creature_face::Cue::Sad);
	EXPECT_EQ(face.cue, creature_face::Cue::Sad);
	face = AdvanceFace(face, 1.0f, 250);
	face = AdvanceFace(face, 500.0f, 250);
	EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
	face = AdvanceFace(face, 600.0f, 250);
	// Let go of after a second, it runs back at a quarter speed, 150 ms of its expression in the last 600 ms
	EXPECT_FALSE(face.wanted.has_value());
	EXPECT_EQ(face.cue, creature_face::Cue::None);
	EXPECT_EQ(face.current, 21u);
	EXPECT_NEAR(face.timeMs, 100.0f, k_Tolerance);
	face = AdvanceFace(face, 200.0f, 250);
	EXPECT_NEAR(face.timeMs, 50.0f, k_Tolerance);
	face = AdvanceFace(face, 1000.0f, 250);
	EXPECT_FALSE(face.current.has_value());
}

TEST(CreatureLayers, AFacePulledWithoutATimeIsHeldForAnHour)
{
	auto face = PullFace({}, 18, 0.0f);
	face = AdvanceFace(face, 30.0f * 60.0f * 1000.0f, 250);
	EXPECT_EQ(face.wanted, 18u);
	face = AdvanceFace(face, 31.0f * 60.0f * 1000.0f, 250);
	EXPECT_FALSE(face.wanted.has_value());
}

TEST(CreatureLayers, PullingTheHeldFaceAgainKeepsHoldingIt)
{
	auto face = PullFace({}, 16, 500.0f);
	face = AdvanceFace(face, 1.0f, 250);
	face = AdvanceFace(face, 400.0f, 250);
	face = PullFace(face, 16, 500.0f);
	face = AdvanceFace(face, 400.0f, 250);
	EXPECT_EQ(face.wanted, 16u);
	EXPECT_EQ(face.current, 16u);
	EXPECT_NEAR(face.timeMs, 250.0f, k_Tolerance);
}

TEST(CreatureLayers, AFaceTheSpeciesLacksStaysAtItsStart)
{
	auto face = PullFace({}, 26, 1000.0f);
	face = AdvanceFace(face, 1.0f, std::nullopt);
	face = AdvanceFace(face, 100.0f, std::nullopt);
	EXPECT_EQ(face.current, 26u);
	EXPECT_NEAR(face.timeMs, 0.0f, k_Tolerance);
}

TEST(CreatureLayers, AGesturePlaysOnce)
{
	auto gesture = PlayGesture({}, animations::k_Yawn);
	ASSERT_TRUE(gesture.has_value());
	EXPECT_FALSE(PlayGesture(*gesture, animations::k_Yawn).has_value());
	auto played = AdvanceGesture(*gesture, 900.0f, 1000);
	EXPECT_EQ(played.animation, animations::k_Yawn);
	played = AdvanceGesture(played, 100.0f, 1000);
	EXPECT_FALSE(played.animation.has_value());
}

namespace
{
/// An animation's time moved on by the original's turn, and the stretches it looked for moments over (none past
/// `count`), as the original gives them
struct TimeCase
{
	int32_t timeMs;
	int32_t deltaMs;
	int32_t durationMs;
	int32_t next;
	size_t count;
	PassedStretch first;
	PassedStretch second;
};

void ExpectAdvance(const TimeAdvance& got, const TimeCase& c)
{
	const auto where = ::testing::Message() << "time " << c.timeMs << " step " << c.deltaMs << " length " << c.durationMs;
	EXPECT_EQ(got.timeMs, c.next) << where;
	ASSERT_EQ(got.Passed().size(), c.count) << where;
	if (c.count > 0)
	{
		EXPECT_EQ(got.Passed()[0], c.first) << where;
	}
	if (c.count > 1)
	{
		EXPECT_EQ(got.Passed()[1], c.second) << where;
	}
}

// From the original: near the end, at it and past it, steps of none, one length and more, of no length, backwards
constexpr std::array k_OnceCases {
    TimeCase {0, 117, 1000, 117, 1, {0, 117}, {}},
    TimeCase {900, 117, 1000, 1000, 1, {900, 1000}, {}},
    TimeCase {883, 117, 1000, 1000, 1, {883, 1000}, {}},
    TimeCase {884, 117, 1000, 1000, 1, {884, 1000}, {}},
    TimeCase {999, 1, 1000, 1000, 1, {999, 1000}, {}},
    TimeCase {999, 2, 1000, 1000, 1, {999, 1000}, {}},
    TimeCase {0, 0, 1000, 0, 1, {0, 0}, {}},
    TimeCase {500, 0, 1000, 500, 1, {500, 500}, {}},
    TimeCase {1000, 0, 1000, 1000, 1, {1000, 1000}, {}},
    TimeCase {1200, 0, 1000, 1000, 1, {1200, 1000}, {}},
    TimeCase {0, 1000, 1000, 1000, 1, {0, 1000}, {}},
    TimeCase {0, 2000, 1000, 1000, 1, {0, 1000}, {}},
    TimeCase {0, 2500, 1000, 1000, 1, {0, 1000}, {}},
    TimeCase {500, 1600, 1000, 1000, 1, {500, 1000}, {}},
    TimeCase {950, 117, 100, 100, 1, {950, 100}, {}},
    TimeCase {10, 234, 100, 100, 1, {10, 100}, {}},
    TimeCase {0, 117, 0, 0, 1, {0, 0}, {}},
    TimeCase {0, 0, 0, 0, 1, {0, 0}, {}},
    TimeCase {5, 3, 0, 0, 1, {5, 0}, {}},
    TimeCase {0, 1, 1, 1, 1, {0, 1}, {}},
    TimeCase {0, 2, 1, 1, 1, {0, 1}, {}},
    TimeCase {40, -10, 100, 30, 1, {40, 30}, {}},
    TimeCase {5, -10, 100, -5, 1, {5, -5}, {}},
    TimeCase {1000, 5, 1000, 1000, 1, {1000, 1000}, {}},
    TimeCase {1500, 5, 1000, 1000, 1, {1500, 1000}, {}},
    TimeCase {33, 66, 99, 99, 1, {33, 99}, {}},
};

constexpr std::array k_LoopingCases {
    TimeCase {0, 117, 1000, 117, 1, {0, 117}, {}},
    TimeCase {900, 117, 1000, 17, 2, {900, 1017}, {-100, 17}},
    TimeCase {883, 117, 1000, 0, 1, {883, 1000}, {}},
    TimeCase {884, 117, 1000, 1, 2, {884, 1001}, {-116, 1}},
    TimeCase {999, 1, 1000, 0, 1, {999, 1000}, {}},
    TimeCase {999, 2, 1000, 1, 2, {999, 1001}, {-1, 1}},
    TimeCase {0, 0, 1000, 0, 0, {}, {}},
    TimeCase {500, 0, 1000, 500, 0, {}, {}},
    TimeCase {1000, 0, 1000, 1000, 0, {}, {}},
    TimeCase {1200, 0, 1000, 1200, 0, {}, {}},
    TimeCase {0, 1000, 1000, 0, 1, {0, 1000}, {}},
    TimeCase {0, 2000, 1000, 999, 2, {0, 2000}, {-1000, 1000}},
    TimeCase {0, 2500, 1000, 999, 2, {0, 2500}, {-1000, 1500}},
    TimeCase {500, 1600, 1000, 999, 2, {500, 2100}, {-500, 1100}},
    TimeCase {950, 117, 100, 99, 2, {950, 1067}, {850, 967}},
    TimeCase {10, 234, 100, 99, 2, {10, 244}, {-90, 144}},
    TimeCase {0, 117, 0, -1, 2, {0, 117}, {0, 117}},
    TimeCase {0, 0, 0, 0, 0, {}, {}},
    TimeCase {5, 3, 0, -1, 2, {5, 8}, {5, 8}},
    TimeCase {0, 1, 1, 0, 1, {0, 1}, {}},
    TimeCase {0, 2, 1, 0, 2, {0, 2}, {-1, 1}},
    TimeCase {40, -10, 100, 30, 1, {40, 30}, {}},
    TimeCase {5, -10, 100, -5, 1, {5, -5}, {}},
    TimeCase {1000, 5, 1000, 5, 2, {1000, 1005}, {0, 5}},
    TimeCase {1500, 5, 1000, 505, 2, {1500, 1505}, {500, 505}},
    TimeCase {33, 66, 99, 0, 1, {33, 99}, {}},
};

/// A face before and after the original's turn: 0 is no face
struct FaceCase
{
	std::array<int32_t, 4> before; // current, wanted, held, time
	int32_t ms;
	std::optional<int32_t> durationMs;
	std::array<int32_t, 4> after;
	std::optional<PassedStretch> passed;
};

std::optional<size_t> FaceId(int32_t id)
{
	return id == 0 ? std::nullopt : std::optional<size_t>(static_cast<size_t>(id));
}

TurnFace MakeFace(const std::array<int32_t, 4>& f)
{
	return {.current = FaceId(f[0]), .wanted = FaceId(f[1]), .heldMs = f[2], .timeMs = f[3]};
}

// From the original: held faces playing on to their end, the wanted face let go of, relaxing a quarter as fast
// (rounded down, so 3 ms do nothing and -3 ms move it on by one), running back to the next face, a face held with a
// time below 0, steps of none, and a species without the face's animation
const std::array k_FaceCases {
    FaceCase {{5, 5, 1000, 100}, 100, 1500, {5, 5, 900, 200}, PassedStretch {100, 200}},
    FaceCase {{5, 5, 1000, 1450}, 100, 1500, {5, 5, 900, 1500}, PassedStretch {1450, 1500}},
    FaceCase {{5, 5, 50, 300}, 100, 1500, {5, 0, 0, 275}, std::nullopt},
    FaceCase {{5, 5, 100, 300}, 100, 1500, {5, 0, 0, 275}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, 100, 1500, {5, 0, 0, 275}, std::nullopt},
    FaceCase {{5, 0, 0, 20}, 100, 1500, {0, 0, 0, 0}, std::nullopt},
    FaceCase {{5, 0, 0, 25}, 100, 1500, {0, 0, 0, 0}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, 103, 1500, {5, 0, 0, 275}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, 3, 1500, {5, 0, 0, 300}, std::nullopt},
    FaceCase {{5, 7, 500, 300}, 100, 1500, {5, 7, 400, 200}, std::nullopt},
    FaceCase {{5, 7, 500, 80}, 100, 1500, {7, 7, 400, 0}, std::nullopt},
    FaceCase {{0, 7, 500, 0}, 100, 1500, {7, 7, 400, 0}, std::nullopt},
    FaceCase {{0, 0, 0, 0}, 100, 1500, {0, 0, 0, 0}, std::nullopt},
    FaceCase {{5, 5, -5, 300}, 100, 1500, {5, 5, -5, 400}, PassedStretch {300, 400}},
    FaceCase {{5, 0, -5, 300}, 100, 1500, {5, 0, -5, 275}, std::nullopt},
    FaceCase {{5, 5, 0, 300}, 100, 1500, {5, 0, 0, 275}, std::nullopt},
    FaceCase {{0, 7, 0, 0}, 100, 1500, {0, 0, 0, 0}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, -100, 1500, {5, 0, 0, 325}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, -3, 1500, {5, 0, 0, 301}, std::nullopt},
    FaceCase {{5, 5, 1000, 100}, 0, 1500, {5, 5, 1000, 100}, PassedStretch {100, 100}},
    FaceCase {{5, 0, 0, 300}, 0, 1500, {5, 0, 0, 300}, std::nullopt},
    FaceCase {{5, 5, 1000, 100}, 100, std::nullopt, {5, 5, 900, 100}, std::nullopt},
    FaceCase {{5, 0, 0, 300}, 100, std::nullopt, {5, 0, 0, 275}, std::nullopt},
};
} // namespace

TEST(CreatureLayers, AnAnimationPlayedOnceStopsAtItsEndAsTheOriginals)
{
	for (const auto& c : k_OnceCases)
	{
		ExpectAdvance(AdvanceOnce(c.timeMs, c.deltaMs, c.durationMs), c);
	}
}

TEST(CreatureLayers, ALoopingAnimationComesRoundOnceAsTheOriginals)
{
	for (const auto& c : k_LoopingCases)
	{
		ExpectAdvance(AdvanceLooping(c.timeMs, c.deltaMs, c.durationMs), c);
	}
}

TEST(CreatureLayers, TheTurnsWholeMillisecondsWrapRoundAsThirtyTwoBitNumbers)
{
	constexpr auto k_Max = std::numeric_limits<int32_t>::max();
	constexpr auto k_Min = std::numeric_limits<int32_t>::min();
	EXPECT_EQ(AdvanceOnce(k_Max, 1, 1000).timeMs, k_Min);
	const auto looped = AdvanceLooping(k_Max, 1, 1000);
	EXPECT_EQ(looped.timeMs, k_Min);
	EXPECT_EQ(looped.Passed().size(), 1u);
}

TEST(CreatureLayers, TheFaceTurnsAsTheOriginals)
{
	for (const auto& c : k_FaceCases)
	{
		const auto got = AdvanceFaceByTurn(MakeFace(c.before), c.ms, c.durationMs);
		const auto where = ::testing::Message() << "face " << c.before[0] << " wanted " << c.before[1] << " held "
		                                        << c.before[2] << " at " << c.before[3] << " by " << c.ms;
		EXPECT_EQ(got.face, MakeFace(c.after)) << where;
		EXPECT_EQ(got.passed, c.passed) << where;
	}
}

TEST(CreatureLayers, TheFaceRelaxingShiftsTheTurnDownTwoBits)
{
	// A quarter of 103 rounded down is 25, where a float quarter would take 25.75
	const TurnFace relaxing {.current = 20u, .wanted = std::nullopt, .heldMs = 0, .timeMs = 1000};
	EXPECT_EQ(AdvanceFaceByTurn(relaxing, 103, 1500).face.timeMs, 975);
	// Backwards, a negative step shifted down rounds away from zero
	EXPECT_EQ(AdvanceFaceByTurn(relaxing, -1, 1500).face.timeMs, 1001);
}

namespace
{
/// The one stretch or two a layer passed, as (from, to) pairs
std::vector<PassedStretch> StretchesOf(const std::optional<PassedAnimation>& passed)
{
	if (!passed.has_value())
	{
		return {};
	}
	const auto stretches = passed->advance.Passed();
	return std::vector<PassedStretch>(stretches.begin(), stretches.end());
}

/// An action played once, from a time
BodyAction OnceAt(size_t animation, float timeMs)
{
	return {.kind = BodyAction::Kind::Once, .animations = {animation, animation, animation}, .timeMs = timeMs};
}

/// Sitting down, sitting and getting up, in a part, from a time
BodyAction SittingAt(BodyAction::Phase phase, float timeMs)
{
	return {.kind = BodyAction::Kind::Sequence,
	        .phase = phase,
	        .animations = {animations::k_StartSit, animations::k_Sit, animations::k_EndSit},
	        .timeMs = timeMs};
}
} // namespace

TEST(CreatureLayers, ABodyMovesOnByTheTurnsWholeStep)
{
	// A turn's step of 117 ms at size 1, where the frames' float rate would have played 117.5
	const auto played = AdvanceBodyByTurn(OnceAt(55, 100.0f), 117, LengthsEach(600));
	EXPECT_EQ(played.layer.timeMs, 217.0f);
	ASSERT_TRUE(played.passed.has_value());
	EXPECT_EQ(played.passed->animation, 55u);
	EXPECT_EQ(StretchesOf(played.passed), (std::vector<PassedStretch> {{.fromMs = 100, .toMs = 217}}));
	// A time between whole milliseconds is taken to the one below
	EXPECT_EQ(AdvanceBodyByTurn(OnceAt(55, 100.7f), 117, LengthsEach(600)).layer.timeMs, 217.0f);
}

TEST(CreatureLayers, AnActionPlayedOnceIsOverTheTurnItsStepReachesItsEnd)
{
	// Played to its end, then the body stands
	const auto ending = AdvanceBodyByTurn(OnceAt(55, 500.0f), 117, LengthsEach(600));
	EXPECT_FALSE(IsPlaying(ending.layer));
	EXPECT_EQ(StretchesOf(ending.passed), (std::vector<PassedStretch> {{.fromMs = 500, .toMs = 600}}));
	// Exactly at its end too
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(OnceAt(55, 483.0f), 117, LengthsEach(600)).layer));
	EXPECT_TRUE(IsPlaying(AdvanceBodyByTurn(OnceAt(55, 482.0f), 117, LengthsEach(600)).layer));
	// A species without the animation ends it at once
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(OnceAt(55, 0.0f), 117, LengthsEach(std::nullopt)).layer));
}

TEST(CreatureLayers, ASequenceStartsLoopsAndEndsByTheTurn)
{
	using Phase = BodyAction::Phase;
	// The start played to its end goes on to the loop from its beginning, which moves on by the same step
	const auto started = AdvanceBodyByTurn(SittingAt(Phase::Start, 950.0f), 117, LengthsEach(1000));
	EXPECT_EQ(started.layer.phase, Phase::Loop);
	EXPECT_EQ(started.layer.timeMs, 117.0f);
	EXPECT_EQ(started.passed->animation, animations::k_StartSit);
	EXPECT_EQ(StretchesOf(started.passed), (std::vector<PassedStretch> {{.fromMs = 950, .toMs = 1000}}));
	ASSERT_TRUE(started.then.has_value());
	EXPECT_EQ(started.then->animation, animations::k_Sit);
	EXPECT_EQ(StretchesOf(started.then), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// The loop comes round as the original's does: the stretch to past the end, then the one from before its start
	const auto looped = AdvanceBodyByTurn(SittingAt(Phase::Loop, 950.0f), 117, LengthsEach(1000));
	EXPECT_EQ(looped.layer.phase, Phase::Loop);
	EXPECT_EQ(looped.layer.timeMs, 67.0f);
	EXPECT_EQ(looped.passed->animation, animations::k_Sit);
	EXPECT_EQ(StretchesOf(looped.passed),
	          (std::vector<PassedStretch> {{.fromMs = 950, .toMs = 1067}, {.fromMs = -50, .toMs = 67}}));
	// Told to end, the end starts from its beginning and plays on by the same turn's step; held, the loop keeps its time
	auto ending = SittingAt(Phase::Loop, 300.0f);
	ending.endWanted = true;
	const auto ended = AdvanceBodyByTurn(ending, 117, LengthsEach(1000));
	EXPECT_EQ(ended.layer.phase, Phase::End);
	EXPECT_EQ(ended.layer.timeMs, 117.0f);
	ASSERT_TRUE(ended.passed.has_value());
	EXPECT_EQ(ended.passed->animation, animations::k_EndSit);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	auto held = SittingAt(Phase::Loop, 300.0f);
	held.holdLoop = true;
	EXPECT_EQ(AdvanceBodyByTurn(held, 117, LengthsEach(1000)).layer.timeMs, 300.0f);
	// The end played to its end leaves the body standing
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(SittingAt(Phase::End, 900.0f), 117, LengthsEach(1000)).layer));
}

namespace
{
/// A start, loop and end in its loop, told to end by what plays it
BodyAction LoopToldToEnd(size_t start, size_t loop, size_t end, bool holdLoop)
{
	auto body = *PlaySequence({}, start, loop, end, holdLoop);
	body.phase = BodyAction::Phase::Loop;
	body.timeMs = 250.0f;
	return EndLoop(body);
}
} // namespace

TEST(CreatureLayers, ASitThatEndsGetsUpByTheWholeStepOfTheTurnItEnds)
{
	// The mind ends a sit: in that same turn the getting up starts and plays on by the step, 117 ms at size 1
	const auto ended = AdvanceBodyByTurn(LoopToldToEnd(animations::k_StartSit, animations::k_Sit, animations::k_EndSit, false),
	                                     117, {1000, 1500, 1200});
	EXPECT_EQ(ended.layer.phase, BodyAction::Phase::End);
	EXPECT_EQ(CurrentAnimation(ended.layer), animations::k_EndSit);
	EXPECT_EQ(ended.layer.timeMs, 117.0f);
	ASSERT_TRUE(ended.passed.has_value());
	EXPECT_EQ(ended.passed->animation, animations::k_EndSit);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
}

TEST(CreatureLayers, ASleepThatEndsWakesByTheWholeStepOfTheTurnItEnds)
{
	// The mind ends a sleep on a turn half frozen, a step of 58: the waking plays 58 ms of itself in that turn
	const auto ended = AdvanceBodyByTurn(
	    LoopToldToEnd(animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep, false), 58, {1000, 2000, 1800});
	EXPECT_EQ(CurrentAnimation(ended.layer), animations::k_EndSleep);
	EXPECT_EQ(ended.layer.timeMs, 58.0f);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 58}}));
}

TEST(CreatureLayers, AFaintThatEndsGetsUpByTheWholeStepOfTheTurnItEnds)
{
	// Lying fainted, its last frame held, the fight gets it up: the getting up plays the turn's step in that turn
	auto lying = LoopToldToEnd(animations::k_Faint, animations::k_Faint, animations::k_GetUp, true);
	const auto ended = AdvanceBodyByTurn(lying, 117, {1300, 1300, 2400});
	EXPECT_EQ(CurrentAnimation(ended.layer), animations::k_GetUp);
	EXPECT_EQ(ended.layer.timeMs, 117.0f);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// hurried, by twice the step
	EXPECT_EQ(AdvanceBodyByTurn(Hurry(lying), 117, {1300, 1300, 2400}).layer.timeMs, 234.0f);
}

TEST(CreatureLayers, ALoopEndedToWalkStandsInTheTurnItEndsWhenItsEndIsShorterThanTheStep)
{
	// Its route ready, the walk ends what the body loops; an end of 100 ms plays to its end within the step of 117, so
	// the body stands at the end of that same turn and the walk can start on the next
	const auto looping = LoopToldToEnd(animations::k_StartPoo, animations::k_Poo, animations::k_EndPoo, false);
	const auto ended = AdvanceBodyByTurn(looping, 117, {1000, 1000, 100});
	EXPECT_FALSE(IsPlaying(ended.layer));
	ASSERT_TRUE(ended.passed.has_value());
	EXPECT_EQ(ended.passed->animation, animations::k_EndPoo);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 100}}));
	// A species without the end stands at once
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(looping, 117, {1000, 1000, std::nullopt}).layer));
}

TEST(CreatureLayers, AStartThatEndsMovesItsLoopOnByTheWholeStepOfThatTurn)
{
	// A sleep's start of 1000 ms at 950: the step of 117 plays it to its end, then the sleep's loop of 400 from its
	// beginning by the whole step, not by what was left of it
	auto starting = *PlaySequence({}, animations::k_StartSleep, animations::k_Sleep, animations::k_EndSleep);
	starting.timeMs = 950.0f;
	const auto started = AdvanceBodyByTurn(starting, 117, {1000, 400, 1200});
	EXPECT_TRUE(IsLooping(started.layer));
	EXPECT_EQ(started.layer.timeMs, 117.0f);
	EXPECT_EQ(StretchesOf(started.passed), (std::vector<PassedStretch> {{.fromMs = 950, .toMs = 1000}}));
	ASSERT_TRUE(started.then.has_value());
	EXPECT_EQ(started.then->animation, animations::k_Sleep);
	EXPECT_EQ(StretchesOf(started.then), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// A loop shorter than the step comes round in that same step; hurried, the step is doubled for both
	EXPECT_EQ(AdvanceBodyByTurn(starting, 117, {1000, 100, 1200}).layer.timeMs, 17.0f);
	EXPECT_EQ(AdvanceBodyByTurn(Hurry(starting), 117, {1000, 400, 1200}).layer.timeMs, 234.0f);
	// A held loop (a faint) is not moved on, and a loop the species lacks starts at its beginning; nothing more is passed
	auto fainting = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp, true);
	fainting.timeMs = 950.0f;
	const auto fainted = AdvanceBodyByTurn(fainting, 117, {1000, 1000, 1200});
	EXPECT_TRUE(IsLooping(fainted.layer));
	EXPECT_EQ(fainted.layer.timeMs, 950.0f);
	EXPECT_FALSE(fainted.then.has_value());
	const auto lacking = AdvanceBodyByTurn(starting, 117, {1000, std::nullopt, 1200});
	EXPECT_EQ(lacking.layer.timeMs, 0.0f);
	EXPECT_FALSE(lacking.then.has_value());
}

TEST(CreatureLayers, AStartToldToEndGoesStraightIntoItsEndMovedOnByTheSameStep)
{
	// A sit told to end while it still sits down: the step of 117 plays the start to its end, then the getting up from
	// its beginning by the whole step; the loop never plays
	auto starting = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	starting.timeMs = 950.0f;
	starting = EndLoop(starting);
	const auto ended = AdvanceBodyByTurn(starting, 117, {1000, 1500, 1200});
	EXPECT_EQ(ended.layer.phase, BodyAction::Phase::End);
	EXPECT_EQ(ended.layer.timeMs, 117.0f);
	EXPECT_EQ(StretchesOf(ended.passed), (std::vector<PassedStretch> {{.fromMs = 950, .toMs = 1000}}));
	ASSERT_TRUE(ended.then.has_value());
	EXPECT_EQ(ended.then->animation, animations::k_EndSit);
	EXPECT_EQ(StretchesOf(ended.then), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// An end shorter than the step comes round, as a loop does, and is seen over on the next turn
	const auto shortEnd = AdvanceBodyByTurn(starting, 117, {1000, 1500, 100});
	EXPECT_EQ(shortEnd.layer.phase, BodyAction::Phase::End);
	EXPECT_EQ(shortEnd.layer.timeMs, 17.0f);
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(shortEnd.layer, 117, {1000, 1500, 100}).layer));
	// Without the end, it stands
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(starting, 117, {1000, 1500, std::nullopt}).layer));
	// By the frame, the same
	starting.timeMs = 990.0f;
	const auto framed = AdvanceBody(starting, 18.0f, {1000, 1500, 1200});
	EXPECT_EQ(framed.phase, BodyAction::Phase::End);
	EXPECT_NEAR(framed.timeMs, 18.0f, k_Tolerance);
}

TEST(CreatureLayers, AFaintToldToEndWhileItFallsGetsUpAtOnce)
{
	// Still falling, 300 ms into a faint of 1300, it is got up: the getting up starts at once and plays on by the
	// turn's step of 117; the fall is not moved on or passed
	auto falling = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp, true);
	falling.timeMs = 300.0f;
	falling = EndLoop(falling);
	const auto gotUp = AdvanceBodyByTurn(falling, 117, {1300, 1300, 2400});
	EXPECT_EQ(gotUp.layer.phase, BodyAction::Phase::End);
	EXPECT_EQ(CurrentAnimation(gotUp.layer), animations::k_GetUp);
	EXPECT_EQ(gotUp.layer.timeMs, 117.0f);
	ASSERT_TRUE(gotUp.passed.has_value());
	EXPECT_EQ(gotUp.passed->animation, animations::k_GetUp);
	EXPECT_EQ(StretchesOf(gotUp.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	EXPECT_FALSE(gotUp.then.has_value());
	// hurried, by twice the step; a getting up shorter than the step is over in that turn
	EXPECT_EQ(AdvanceBodyByTurn(Hurry(falling), 117, {1300, 1300, 2400}).layer.timeMs, 234.0f);
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(falling, 117, {1300, 1300, 100}).layer));
	// By the frame, the same
	const auto framed = AdvanceBody(falling, 18.0f, {1300, 1300, 2400});
	EXPECT_EQ(CurrentAnimation(framed), animations::k_GetUp);
	EXPECT_NEAR(framed.timeMs, 18.0f, k_Tolerance);
	// A sit told to end while it still sits down plays its start out first
	auto sitting = *PlaySequence({}, animations::k_StartSit, animations::k_Sit, animations::k_EndSit);
	sitting.timeMs = 300.0f;
	const auto stillSitting = AdvanceBodyByTurn(EndLoop(sitting), 117, {1300, 1300, 2400});
	EXPECT_EQ(stillSitting.layer.phase, BodyAction::Phase::Start);
	EXPECT_EQ(stillSitting.layer.timeMs, 417.0f);
}

TEST(CreatureLayers, ADancesStartThatEndsMovesItsFirstMoveOrItsFinishOnInTheSameTurn)
{
	// A start of 1000 ms at 950: the step of 117 plays it to its end, then the first move, wanted, has played the turn's
	// 100 ms of real time from its beginning
	auto starting = *PlayDance({}, 2000.0f);
	starting.timeMs = 950.0f;
	const auto moving = AdvanceBodyByTurn(starting, 117, {1000, 3000, 1200}, 100);
	EXPECT_EQ(moving.layer.phase, BodyAction::Phase::Loop);
	EXPECT_TRUE(moving.layer.chooseDanceMove);
	EXPECT_TRUE(moving.layer.firstDanceMove);
	EXPECT_EQ(moving.layer.timeMs, 100.0f);
	EXPECT_EQ(StretchesOf(moving.passed), (std::vector<PassedStretch> {{.fromMs = 950, .toMs = 1000}}));
	EXPECT_FALSE(moving.then.has_value());
	// Told to end while it starts, it goes straight into its finish, moved on by the whole step
	const auto finishing = AdvanceBodyByTurn(EndDance(starting), 117, {1000, 3000, 1200}, 100);
	EXPECT_EQ(finishing.layer.phase, BodyAction::Phase::End);
	EXPECT_FALSE(finishing.layer.chooseDanceMove);
	EXPECT_EQ(finishing.layer.timeMs, 117.0f);
	ASSERT_TRUE(finishing.then.has_value());
	EXPECT_EQ(finishing.then->animation, animations::k_DanceFinish);
	EXPECT_EQ(StretchesOf(finishing.then), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// A finish shorter than the step comes round, and is seen over on the next turn
	const auto shortFinish = AdvanceBodyByTurn(EndDance(starting), 117, {1000, 3000, 100}, 100);
	EXPECT_EQ(shortFinish.layer.timeMs, 17.0f);
	EXPECT_FALSE(IsPlaying(AdvanceBodyByTurn(shortFinish.layer, 117, {1000, 3000, 100}, 100).layer));
}

TEST(CreatureLayers, AFaintLiesAtTheTimeItsFallLastStoodAtAndNeverMoves)
{
	// A fall of 1300 ms at 1250: the step of 117 plays it to its end (its sounds passed to the end), and the creature
	// lies at 1250, never at the fall's first frame, nor at its end
	auto falling = *PlaySequence({}, animations::k_Faint, animations::k_Faint, animations::k_GetUp, true);
	falling.timeMs = 1250.0f;
	const auto fell = AdvanceBodyByTurn(falling, 117, {1300, 1300, 2400});
	EXPECT_TRUE(IsLooping(fell.layer));
	EXPECT_EQ(fell.layer.timeMs, 1250.0f);
	EXPECT_EQ(StretchesOf(fell.passed), (std::vector<PassedStretch> {{.fromMs = 1250, .toMs = 1300}}));
	EXPECT_FALSE(fell.then.has_value());
	// lying, turn after turn, it stays there and passes nothing
	auto lying = fell.layer;
	for (int turn = 0; turn < 5; ++turn)
	{
		const auto still = AdvanceBodyByTurn(lying, 117, {1300, 1300, 2400});
		EXPECT_FALSE(still.passed.has_value());
		lying = still.layer;
	}
	EXPECT_EQ(lying.timeMs, 1250.0f);
	// By the frame, the same
	auto framed = falling;
	framed.timeMs = 1290.0f;
	framed = AdvanceBody(framed, 18.0f, {1300, 1300, 2400});
	EXPECT_TRUE(IsLooping(framed));
	EXPECT_FLOAT_EQ(framed.timeMs, 1290.0f);
	EXPECT_FLOAT_EQ(AdvanceBody(framed, 18.0f, {1300, 1300, 2400}).timeMs, 1290.0f);
}

TEST(CreatureLayers, StandingOrTimedByWhatPlaysItTheBodyIsNotMovedOnByTheTurn)
{
	BodyAction standing {};
	standing.timeMs = 40.0f;
	const auto stood = AdvanceBodyByTurn(standing, 117, LengthsEach(1000));
	EXPECT_EQ(stood.layer.timeMs, 0.0f);
	EXPECT_FALSE(stood.passed.has_value());
	auto fighting = SittingAt(BodyAction::Phase::Loop, 250.5f);
	fighting.timedByPlayer = true;
	const auto fought = AdvanceBodyByTurn(fighting, 117, LengthsEach(1000));
	EXPECT_EQ(fought.layer.timeMs, 250.5f);
	EXPECT_FALSE(fought.passed.has_value());
}

TEST(CreatureLayers, AHurriedActionPlayedOnceIsOverInHalfTheTurns)
{
	// Six turns of 100 ms play an action of 600 ms to its end; hurried, three do
	auto usual = OnceAt(55, 0.0f);
	auto hurried = Hurry(usual);
	ASSERT_TRUE(hurried.hurried);
	for (int turn = 0; turn < 3; ++turn)
	{
		usual = AdvanceBodyByTurn(usual, 100, LengthsEach(600)).layer;
		hurried = AdvanceBodyByTurn(hurried, 100, LengthsEach(600)).layer;
	}
	EXPECT_TRUE(IsPlaying(usual));
	EXPECT_EQ(usual.timeMs, 300.0f);
	EXPECT_FALSE(IsPlaying(hurried));
	// The body that stands again is no longer hurried
	EXPECT_FALSE(hurried.hurried);
}

TEST(CreatureLayers, AGestureIsOverTheTurnItsStepReachesItsEnd)
{
	const GestureLayer nodding {.animation = animations::k_FirstGesture, .timeMs = 0.0f};
	const auto nodded = AdvanceGestureByTurn(nodding, 117, 900);
	EXPECT_EQ(nodded.layer.timeMs, 117.0f);
	EXPECT_EQ(nodded.passed->animation, animations::k_FirstGesture);
	EXPECT_EQ(StretchesOf(nodded.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 117}}));
	// the step reaching the end is cut to it: played to its end, then over
	const auto ending = AdvanceGestureByTurn({.animation = animations::k_FirstGesture, .timeMs = 800.0f}, 117, 900);
	EXPECT_FALSE(ending.layer.animation.has_value());
	EXPECT_EQ(StretchesOf(ending.passed), (std::vector<PassedStretch> {{.fromMs = 800, .toMs = 900}}));
	// a gesture the species lacks is over at once; none plays, none moves
	EXPECT_FALSE(AdvanceGestureByTurn(nodding, 117, std::nullopt).layer.animation.has_value());
	const auto none = AdvanceGestureByTurn({}, 117, 900);
	EXPECT_FALSE(none.layer.animation.has_value());
	EXPECT_FALSE(none.passed.has_value());
}

TEST(CreatureLayers, TheFaceMovesOnByTheTurnsMillisecondsAndRelaxesWhenLetGo)
{
	constexpr size_t k_Smile = animations::k_FirstFace;
	auto face = PullFace({}, k_Smile, 150.0f, creature_face::Cue::Idle);
	// the first turn makes the wanted face current, from its start
	const auto first = AdvanceFaceLayerByTurn(face, 100, std::nullopt);
	EXPECT_EQ(first.layer.current, k_Smile);
	EXPECT_EQ(first.layer.remainingMs, 50.0f);
	EXPECT_EQ(first.layer.cue, creature_face::Cue::Idle);
	EXPECT_FALSE(first.passed.has_value());
	// held still, it plays on towards its end by the turn's milliseconds
	face = first.layer;
	face.remainingMs = 1000.0f;
	const auto playing = AdvanceFaceLayerByTurn(face, 100, 800);
	EXPECT_EQ(playing.layer.timeMs, 100.0f);
	EXPECT_EQ(playing.passed->animation, k_Smile);
	EXPECT_EQ(StretchesOf(playing.passed), (std::vector<PassedStretch> {{.fromMs = 0, .toMs = 100}}));
	// its time up, it is let go of: no cue, and it runs back by a quarter of the turn
	auto letGo = playing.layer;
	letGo.remainingMs = 30.0f;
	const auto relaxed = AdvanceFaceLayerByTurn(letGo, 103, 800);
	EXPECT_FALSE(relaxed.layer.wanted.has_value());
	EXPECT_EQ(relaxed.layer.cue, creature_face::Cue::None);
	EXPECT_EQ(relaxed.layer.remainingMs, 0.0f);
	EXPECT_EQ(relaxed.layer.current, k_Smile);
	EXPECT_EQ(relaxed.layer.timeMs, 75.0f);
	EXPECT_FALSE(relaxed.passed.has_value());
}

TEST(CreatureLayers, TheHeadSpeedsUpSteadily)
{
	const auto first = TurnHead({}, 1.0f, 2.0f, 0.1f, k_YawLimit);
	EXPECT_NEAR(first.velocity, 0.2f, k_Tolerance);
	EXPECT_NEAR(first.angle, 0.02f, k_Tolerance);
	const auto second = TurnHead(first, 1.0f, 2.0f, 0.1f, k_YawLimit);
	EXPECT_NEAR(second.velocity, 0.4f, k_Tolerance);
}

TEST(CreatureLayers, TheHeadSettlesWithoutOvershooting)
{
	LookAxis axis {};
	float largest = 0.0f;
	for (int i = 0; i < 400; ++i)
	{
		axis = TurnHead(axis, -0.8f, 4.0f, 0.02f, k_YawLimit);
		largest = std::max(largest, -axis.angle);
	}
	EXPECT_FLOAT_EQ(axis.angle, -0.8f);
	EXPECT_FLOAT_EQ(axis.velocity, 0.0f);
	EXPECT_LE(largest, 0.8f + k_Tolerance);
}

TEST(CreatureLayers, TheHeadNeverTurnsPastItsLimit)
{
	LookAxis axis {.angle = 1.5f, .velocity = 5.0f};
	axis = TurnHead(axis, 3.0f, 100.0f, 0.1f, k_PitchLimit);
	EXPECT_LE(axis.angle, k_PitchLimit);
}

TEST(CreatureLayers, AnglesTowardsATarget)
{
	const glm::vec3 head {0.0f, 15.0f, 0.0f};
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	const auto straight = AnglesTowards(head, ahead, {0.0f, 15.0f, -10.0f});
	EXPECT_NEAR(straight.yaw, 0.0f, k_Tolerance);
	EXPECT_NEAR(straight.pitch, 0.0f, k_Tolerance);
	// Facing -z in the left-handed world, +x is on the creature's left
	const auto left = AnglesTowards(head, ahead, {10.0f, 15.0f, -10.0f});
	EXPECT_NEAR(left.yaw, k_Pi / 4.0f, k_Tolerance);
	const auto right = AnglesTowards(head, ahead, {-10.0f, 15.0f, -10.0f});
	EXPECT_NEAR(right.yaw, -k_Pi / 4.0f, k_Tolerance);
	const auto up = AnglesTowards(head, ahead, {0.0f, 25.0f, -10.0f});
	EXPECT_NEAR(up.pitch, k_Pi / 4.0f, k_Tolerance);
}

TEST(CreatureLayers, ATargetAlongAMapAxisGivesNoSidewaysTurn)
{
	const glm::vec3 head {0.0f, 15.0f, 0.0f};
	const glm::vec3 ahead {0.0f, 0.0f, -1.0f};
	// Straight to the creature's right, along the x axis: no sideways turn, only up and down
	const auto along = AnglesTowards(head, ahead, {-10.0f, 15.0f, 0.0f});
	EXPECT_EQ(along.yaw, 0.0f);
	EXPECT_NEAR(along.pitch, 0.0f, k_Tolerance);
	const auto alongAndUp = AnglesTowards(head, ahead, {10.0f, 25.0f, 0.0f});
	EXPECT_EQ(alongAndUp.yaw, 0.0f);
	EXPECT_NEAR(alongAndUp.pitch, k_Pi / 4.0f, k_Tolerance);
	// The direction must be more than 1e-4 off each axis: 5e-5 is not, 2e-4 is
	EXPECT_EQ(AnglesTowards(head, ahead, {-10.0f, 15.0f, -0.0005f}).yaw, 0.0f);
	EXPECT_NEAR(AnglesTowards(head, ahead, {-10.0f, 15.0f, -0.002f}).yaw, -k_Pi / 2.0f, 1e-3f);
}

TEST(CreatureLayers, TheMiddleOfAHeadTurnLooksAhead)
{
	EXPECT_EQ(LookTime(0.0f, k_YawLimit, 1000), 500u);
	EXPECT_EQ(LookTime(k_YawLimit, k_YawLimit, 1000), 1000u);
	EXPECT_EQ(LookTime(-k_YawLimit, k_YawLimit, 1000), 0u);
	EXPECT_EQ(LookTime(k_PitchLimit / 2.0f, k_PitchLimit, 1000), 750u);
}

TEST(CreatureLayers, AnimationNames)
{
	EXPECT_EQ(animations::Name(animations::k_Tired), "tired");
	EXPECT_EQ(animations::Name(animations::k_FriendlyWave), "friendly_wave");
	EXPECT_EQ(animations::Name(animations::k_FirstFace), "smile");
	EXPECT_EQ(animations::Name(animations::k_Yawn), "yawn");
	EXPECT_EQ(animations::Name(animations::k_Sit), "sit");
}

TEST(SkeletalLayers, WeightedPosesSum)
{
	const std::vector<Pose> a {{.rotation = skeletal_animation::k_Identity, .translation = {2.0f, 0.0f, 0.0f}}};
	const std::vector<Pose> b {
	    {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.4f, 0.0f}), .translation = {4.0f, 2.0f, 0.0f}}};
	const std::array<std::vector<Pose>, 2> poses {a, b};
	const std::array<float, 2> weights {0.75f, 0.25f};
	const auto sum = skeletal_animation::WeightedSum(poses, weights);
	ASSERT_EQ(sum.size(), 1u);
	EXPECT_NEAR(sum[0].translation.x, 2.5f, k_Tolerance);
	EXPECT_NEAR(sum[0].translation.y, 0.5f, k_Tolerance);
	ExpectMatrixNear(sum[0].rotation, skeletal_animation::Lerp(a[0].rotation, b[0].rotation, 0.25f));
}

TEST(SkeletalLayers, ALayerAtItsReferenceChangesNothing)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	const auto layer = Frames({1}, {{glm::vec3(0.3f, 0.1f, 0.0f)}, {glm::vec3(0.3f, 0.6f, 0.0f)}}, {1},
	                          {{glm::vec3(1.0f, 0.0f, 0.0f)}, {glm::vec3(1.0f, 0.5f, 0.0f)}});
	std::vector<Pose> poses(3, Pose {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}), .translation = {}});
	const auto before = poses;
	skeletal_animation::AddLayer(poses, layer, 0, 0, skeleton);
	ExpectMatrixNear(poses[1].rotation, before[1].rotation);
	EXPECT_NEAR(glm::length(poses[1].translation - before[1].translation), 0.0f, k_Tolerance);
}

TEST(SkeletalLayers, ALayerAddsItsTurnAwayFromTheReference)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	// Two keyframes 0.5 apart in y, sampled exactly on the second
	const auto layer = Frames({1}, {{glm::vec3(0.0f, 0.1f, 0.0f)}, {glm::vec3(0.0f, 0.6f, 0.0f)}}, {1},
	                          {{glm::vec3(1.0f, 0.0f, 0.0f)}, {glm::vec3(1.0f, 0.5f, 0.0f)}});
	std::vector<Pose> poses(3, Pose {.rotation = skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}), .translation = {}});
	skeletal_animation::AddLayer(poses, layer, 500, 0, skeleton);
	// Turned on by the layer's half radian: 0.2 + 0.5 about y
	ExpectMatrixNear(poses[1].rotation, skeletal_animation::RotationYXZ({0.0f, 0.7f, 0.0f}));
	EXPECT_NEAR(poses[1].translation.y, 0.5f, k_Tolerance);
	// Bones the layer doesn't move stay as they were
	ExpectMatrixNear(poses[2].rotation, skeletal_animation::RotationYXZ({0.0f, 0.2f, 0.0f}));
}

TEST(SkeletalLayers, MirrorBonesPairAcrossTheBody)
{
	std::array<glm::mat4, 3> rest {};
	[[maybe_unused]] const auto skeleton = Symmetric(rest);
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	ASSERT_EQ(mirror.size(), 3u);
	EXPECT_EQ(mirror[0], 0u);
	EXPECT_EQ(mirror[1], 2u);
	EXPECT_EQ(mirror[2], 1u);
}

TEST(SkeletalLayers, AnUnpairedBoneKeepsItsPlace)
{
	const std::array<glm::mat4, 2> rest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 0.0f))};
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	EXPECT_EQ(mirror[1], 1u);
}

TEST(SkeletalLayers, AMirroredAnimationMovesTheOtherSide)
{
	std::array<glm::mat4, 3> rest {};
	const auto skeleton = Symmetric(rest);
	const auto mirror = skeletal_animation::MirrorJoints(rest);
	const glm::vec3 euler {0.1f, 0.2f, 0.3f};
	const auto stand = Frames({0}, {{glm::vec3(0.0f)}}, {0}, {{glm::vec3(0.0f, 1.0f, 0.0f)}});
	const auto animation = Frames({1}, {{euler}, {euler}}, {0}, {{glm::vec3(2.0f, 1.0f, 3.0f)}, {glm::vec3(2.0f, 1.0f, 3.0f)}});

	const auto plain = skeletal_animation::SampleCycle(animation, stand, 0, skeleton);
	ExpectMatrixNear(plain[1].rotation, skeletal_animation::RotationYXZ(euler));
	ExpectMatrixNear(plain[2].rotation, skeletal_animation::k_Identity);
	EXPECT_NEAR(plain[0].translation.x, 2.0f, k_Tolerance);

	const auto mirrored = skeletal_animation::SampleCycle(animation, stand, 0, skeleton, mirror);
	// The right bone's keyframes move the left, their y and z angles negated
	ExpectMatrixNear(mirrored[2].rotation, skeletal_animation::RotationYXZ({0.1f, -0.2f, -0.3f}));
	ExpectMatrixNear(mirrored[1].rotation, skeletal_animation::k_Identity);
	// The root's sideways movement away from the stand is flipped
	EXPECT_NEAR(mirrored[0].translation.x, -2.0f, k_Tolerance);
	EXPECT_NEAR(mirrored[0].translation.y, 1.0f, k_Tolerance);
	EXPECT_NEAR(mirrored[0].translation.z, 3.0f, k_Tolerance);
}
