/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's sounds as the game times them (Creature/CreatureSoundQueue), and the keys, gate and side effects of one
// sound (Creature/CreatureAudio, Creature/CreatureDust). Every expected value was read from the original game's own
// functions run on the same inputs, except the turn's order of layers, worked out from the rules they pin.

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureAudio.h"
#include "Creature/CreatureDust.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureSoundQueue.h"

using namespace openblack;
using namespace openblack::creature_audio;
using audio::SoundAction;

namespace
{
QueuedSound Sound(float fraction, int32_t action, int32_t mode, bool generic)
{
	return {.fraction = fraction, .action = static_cast<SoundAction>(action), .mode = mode, .generic = generic};
}

std::vector<int32_t> Actions(std::span<const QueuedSound> sounds)
{
	std::vector<int32_t> actions;
	for (const auto& sound : sounds)
	{
		actions.push_back(static_cast<int32_t>(sound.action));
	}
	return actions;
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

SoundEvent Moment(int32_t kind, int32_t timeMs, int32_t action, int32_t mode)
{
	return {.kind = static_cast<EventKind>(kind), .timeMs = timeMs, .action = static_cast<SoundAction>(action), .mode = mode};
}

/// An animation's moments in the order its file lists them: a voice and a shared sound at 100, a hair group hidden at
/// 150, a shared sound at its start, a voice let run out at 300, a shared sound stopped at 299 and a moment of an
/// unknown kind at 120
const std::array<SoundEvent, 7> k_Moments {
    Moment(3, 120, 9, 0), Moment(2, 299, 8, 1), Moment(0, 300, 7, 2), Moment(2, 0, 5, 0),
    Moment(1, 150, 0, 1), Moment(2, 100, 3, 0), Moment(0, 100, 4, 0),
};

struct Checked
{
	std::vector<QueuedSound> sounds;
	std::vector<std::pair<int32_t, bool>> hair;
};

Checked Check(int32_t oldMs, int32_t newMs)
{
	TurnQueue queue;
	Checked checked;
	CheckMoments(k_Moments, oldMs, newMs, queue,
	             [&checked](int32_t group, bool shown) { checked.hair.emplace_back(group, shown); });
	checked.sounds = queue.TakeAll();
	return checked;
}
} // namespace

TEST(CreatureSoundQueue, ASoundGoesBeforeThoseAlreadyQueuedAtItsFraction)
{
	TurnQueue queue;
	queue.Add(Sound(0.5f, 1, 0, false));
	queue.Add(Sound(0.25f, 2, 0, true));
	queue.Add(Sound(0.5f, 3, 0, false));
	queue.Add(Sound(0.0f, 4, 0, true));
	queue.Add(Sound(0.5f, 5, 1, false));
	queue.Add(Sound(1.0f, 6, 2, true));
	const std::vector<QueuedSound> expected {Sound(0.0f, 4, 0, true),  Sound(0.25f, 2, 0, true), Sound(0.5f, 5, 1, false),
	                                         Sound(0.5f, 3, 0, false), Sound(0.5f, 1, 0, false), Sound(1.0f, 6, 2, true)};
	EXPECT_EQ(std::vector<QueuedSound>(queue.Pending().begin(), queue.Pending().end()), expected);
}

TEST(CreatureSoundQueue, AFullQueueDropsTheNewSound)
{
	TurnQueue queue;
	for (const auto& sound : {Sound(0.5f, 1, 0, false), Sound(0.25f, 2, 0, true), Sound(0.5f, 3, 0, false),
	                          Sound(0.0f, 4, 0, true), Sound(0.5f, 5, 1, false), Sound(1.0f, 6, 2, true)})
	{
		queue.Add(sound);
	}
	for (int32_t i = 0; i < 12; ++i)
	{
		queue.Add(Sound(0.75f, 100 + i, 0, false));
	}
	EXPECT_EQ(Actions(queue.Pending()),
	          (std::vector<int32_t> {4, 2, 5, 3, 1, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 6}));
}

TEST(CreatureSoundQueue, AFrameTakesTheSoundsItsDrawingHasPassed)
{
	const std::array<QueuedSound, 7> queued {Sound(0.0f, 1, 0, false), Sound(0.25f, 2, 0, true),  Sound(0.5f, 3, 1, false),
	                                         Sound(0.5f, 4, 0, true),  Sound(0.99f, 5, 2, false), Sound(1.0f, 6, 0, true),
	                                         Sound(1.5f, 7, 0, false)};
	const std::array<std::pair<float, std::vector<int32_t>>, 9> cases {{
	    {0.0f, {}},
	    {0.25f, {1}},
	    {0.5f, {1, 2}},
	    {0.75f, {1, 2, 3, 4}},
	    {0.99f, {1, 2, 3, 4}},
	    {1.0f, {1, 2, 3, 4, 5}},
	    {1.25f, {1, 2, 3, 4, 5, 6}},
	    {2.0f, {1, 2, 3, 4, 5, 6, 7}},
	    {5.0f, {1, 2, 3, 4, 5, 6, 7}},
	}};
	for (const auto& [fraction, played] : cases)
	{
		// added from the last, so that the two at one fraction stay in this order
		TurnQueue queue;
		for (auto sound = queued.rbegin(); sound != queued.rend(); ++sound)
		{
			queue.Add(*sound);
		}
		const auto taken = queue.TakeDrawn(fraction);
		EXPECT_EQ(Actions(taken), played) << fraction;
		EXPECT_EQ(queue.Pending().size(), queued.size() - played.size()) << fraction;
		if (!queue.Pending().empty())
		{
			EXPECT_EQ(static_cast<int32_t>(queue.Pending().front().action), static_cast<int32_t>(played.size()) + 1);
		}
	}
}

TEST(CreatureSoundQueue, TheNextTurnTakesWhateverIsLeft)
{
	TurnQueue queue;
	queue.Add(Sound(0.75f, 1, 0, false));
	queue.Add(Sound(0.25f, 2, 0, true));
	EXPECT_EQ(Actions(queue.TakeAll()), (std::vector<int32_t> {2, 1}));
	EXPECT_TRUE(queue.Pending().empty());
}

TEST(CreatureSoundMoments, MomentsAtOneTimeAreQueuedInTheOrderListed)
{
	auto checked = Check(0, 101);
	ASSERT_EQ(checked.sounds.size(), 3U);
	EXPECT_EQ(checked.sounds[0], Sound(0.0f, 5, 0, true));
	EXPECT_EQ(checked.sounds[1], Sound(std::bit_cast<float>(0x3F7D7721U), 3, 0, true));
	EXPECT_EQ(checked.sounds[2], Sound(std::bit_cast<float>(0x3F7D7721U), 4, 0, false));
	EXPECT_TRUE(checked.hair.empty());
}

TEST(CreatureSoundMoments, TheStretchIsHalfOpenAndUnknownKindsAreIgnored)
{
	EXPECT_EQ(Actions(Check(0, 100).sounds), (std::vector<int32_t> {5}));
	EXPECT_EQ(Actions(Check(0, 1).sounds), (std::vector<int32_t> {5}));
	EXPECT_TRUE(Check(300, 300).sounds.empty());
	EXPECT_TRUE(Check(300, 0).sounds.empty());
	EXPECT_TRUE(Check(101, 299).sounds.empty());

	const auto passed = Check(7, 333);
	ASSERT_EQ(passed.sounds.size(), 4U);
	EXPECT_EQ(Bits(passed.sounds[0].fraction), 0x3E920FB5U);
	EXPECT_EQ(Bits(passed.sounds[1].fraction), 0x3E920FB5U);
	EXPECT_EQ(passed.sounds[2], Sound(std::bit_cast<float>(0x3F654CF5U), 8, 1, true));
	EXPECT_EQ(passed.sounds[3], Sound(std::bit_cast<float>(0x3F6615FDU), 7, 2, false));
}

TEST(CreatureSoundMoments, AHairMomentShowsItsGroupOnlyWithModeZero)
{
	const auto checked = Check(100, 300);
	EXPECT_EQ(checked.hair, (std::vector<std::pair<int32_t, bool>> {{0, false}}));
	ASSERT_EQ(checked.sounds.size(), 3U);
	EXPECT_EQ(checked.sounds[0], Sound(0.0f, 3, 0, true));
	EXPECT_EQ(checked.sounds[1], Sound(0.0f, 4, 0, false));
	EXPECT_EQ(Bits(checked.sounds[2].fraction), 0x3F7EB852U);

	const auto later = Check(99, 300);
	EXPECT_EQ(Bits(later.sounds[0].fraction), 0x3BA3065EU);
	EXPECT_EQ(Bits(later.sounds[2].fraction), 0x3F7EB9F3U);
	// a stretch measured from before the start
	const auto before = Check(-50, 1);
	ASSERT_EQ(before.sounds.size(), 1U);
	EXPECT_EQ(Bits(before.sounds[0].fraction), 0x3F7AFAFBU);
}

namespace
{
struct Advanced
{
	int32_t timeMs;
	std::vector<int32_t> actions;
	std::vector<uint32_t> fractions;
};

/// The 320 ms animation of k_Moments played on, looping or once
Advanced Advance(bool looping, int32_t timeMs, int32_t deltaMs)
{
	TurnQueue queue;
	const AnimationMoments animation {.events = k_Moments, .durationMs = 320};
	Advanced advanced;
	advanced.timeMs =
	    looping ? AdvanceLooping(animation, timeMs, deltaMs, queue, {}) : AdvanceOnce(animation, timeMs, deltaMs, queue, {});
	for (const auto& sound : queue.Pending())
	{
		advanced.actions.push_back(static_cast<int32_t>(sound.action));
		advanced.fractions.push_back(Bits(sound.fraction));
	}
	return advanced;
}
} // namespace

TEST(CreatureSoundMoments, ALoopingAnimationWrapsRoundItsEnd)
{
	struct Case
	{
		int32_t timeMs;
		int32_t deltaMs;
		int32_t next;
		std::vector<int32_t> actions;
		std::vector<uint32_t> fractions;
	};
	const std::vector<Case> cases {
	    {0, 100, 100, {5}, {0}},
	    {0, 0, 0, {}, {}},
	    {250, 100, 30, {8, 7, 5}, {0x3EFAE148U, 0x3F000000U, 0x3F333333U}},
	    {300, 30, 10, {7, 5}, {0, 0x3F2AAAABU}},
	    {300, 20, 0, {7}, {0}},
	    // reaching the end exactly wraps to the start, whose moment waits for the next stretch
	    {310, 10, 0, {}, {}},
	    {310, 9, 319, {}, {}},
	    {319, 1, 0, {}, {}},
	    {100, 220, 0, {3, 4, 8, 7}, {0, 0, 0x3F67904AU, 0x3F68BA2FU}},
	    {0, 320, 0, {5, 3, 4, 8, 7}, {0, 0x3EA00000U, 0x3EA00000U, 0x3F6F3333U, 0x3F700000U}},
	    {-30, 40, 10, {5}, {0x3F400000U}},
	    // more than one length on: twice through, then held at the last millisecond
	    {0,
	     700,
	     319,
	     {5, 3, 4, 8, 7, 5, 3, 4, 8, 7},
	     {0, 0x3E124925U, 0x3E124925U, 0x3EDAB278U, 0x3EDB6DB7U, 0x3EEA0EA1U, 0x3F19999AU, 0x3F19999AU, 0x3F62608CU,
	      0x3F62BE2CU}},
	    {290,
	     400,
	     319,
	     {8, 7, 5, 3, 4, 8, 7},
	     {0x3CB851ECU, 0x3CCCCCCDU, 0x3D99999AU, 0x3EA66666U, 0x3EA66666U, 0x3F528F5CU, 0x3F533333U}},
	};
	for (const auto& c : cases)
	{
		const auto advanced = Advance(true, c.timeMs, c.deltaMs);
		EXPECT_EQ(advanced.timeMs, c.next) << c.timeMs << " + " << c.deltaMs;
		EXPECT_EQ(advanced.actions, c.actions) << c.timeMs << " + " << c.deltaMs;
		EXPECT_EQ(advanced.fractions, c.fractions) << c.timeMs << " + " << c.deltaMs;
	}
}

TEST(CreatureSoundMoments, AnAnimationPlayedOnceStopsAtItsEnd)
{
	struct Case
	{
		int32_t timeMs;
		int32_t deltaMs;
		int32_t next;
		std::vector<int32_t> actions;
		std::vector<uint32_t> fractions;
	};
	const std::vector<Case> cases {
	    {0, 100, 100, {5}, {0}},
	    {0, 0, 0, {}, {}},
	    // the stretch ends at the end, so the fractions are measured over what is left
	    {250, 100, 320, {8, 7}, {0x3F333333U, 0x3F36DB6EU}},
	    {300, 30, 320, {7}, {0}},
	    {310, 10, 320, {}, {}},
	    {310, 9, 319, {}, {}},
	    {0, 700, 320, {5, 3, 4, 8, 7}, {0, 0x3EA00000U, 0x3EA00000U, 0x3F6F3333U, 0x3F700000U}},
	    {290, 400, 320, {8, 7}, {0x3E99999AU, 0x3EAAAAABU}},
	    {100, 220, 320, {3, 4, 8, 7}, {0, 0, 0x3F67904AU, 0x3F68BA2FU}},
	    {-30, 40, 10, {5}, {0x3F400000U}},
	};
	for (const auto& c : cases)
	{
		const auto advanced = Advance(false, c.timeMs, c.deltaMs);
		EXPECT_EQ(advanced.timeMs, c.next) << c.timeMs << " + " << c.deltaMs;
		EXPECT_EQ(advanced.actions, c.actions) << c.timeMs << " + " << c.deltaMs;
		EXPECT_EQ(advanced.fractions, c.fractions) << c.timeMs << " + " << c.deltaMs;
	}
}

// The turn's moments, from what the body's layers passed (values worked out by hand from the rules above)
namespace
{
/// Animations 1 to 3 have one moment each half way through a 100 ms stretch: the face's voice 11, the gesture's shared
/// sound 12 and the action's voice 13. Animation 4 loops over 1000 ms with a shared sound 14 and a hair group shown at
/// 100 ms
const std::array k_Face {Moment(0, 50, 11, 0)};
const std::array k_Gesture {Moment(2, 50, 12, 0)};
const std::array k_Action {Moment(0, 50, 13, 0)};
const std::array k_Walk {Moment(2, 100, 14, 0), Moment(1, 100, 6, 0)};

std::optional<AnimationInfo> InfoOfAnimation(size_t animation)
{
	switch (animation)
	{
	case 1:
		return AnimationInfo {.events = k_Face, .durationMs = 1000, .looping = false};
	case 2:
		return AnimationInfo {.events = k_Gesture, .durationMs = 1000, .looping = false};
	case 3:
		return AnimationInfo {.events = k_Action, .durationMs = 1000, .looping = false};
	case 4:
		return AnimationInfo {.events = k_Walk, .durationMs = 1000, .looping = true};
	default:
		return std::nullopt;
	}
}

creature_layers::PassedAnimation FirstHundred(size_t animation)
{
	return {.animation = animation,
	        .advance = {.timeMs = 100, .stretches = {{{.fromMs = 0, .toMs = 100}, {}}}, .passedCount = 1}};
}

/// The face, the gesture and the action each passed their first 100 ms
const creature_layers::TurnPassed k_Passed {.overlays = {FirstHundred(1), FirstHundred(2)}, .action = FirstHundred(3)};

struct Queued
{
	std::vector<int32_t> actions;
	std::vector<float> fractions;
	std::vector<std::pair<int32_t, bool>> hair;
};

Queued QueueOf(const creature_layers::TurnPassed& passed, std::span<const Played> lastSlots, std::span<const Played> slots,
               std::optional<size_t> soundingSlot)
{
	TurnQueue queue;
	Queued queued;
	QueueTurn(passed, lastSlots, slots, soundingSlot, InfoOfAnimation, queue,
	          [&queued](int32_t group, bool shown) { queued.hair.emplace_back(group, shown); });
	for (const auto& sound : queue.Pending())
	{
		queued.actions.push_back(static_cast<int32_t>(sound.action));
		queued.fractions.push_back(sound.fraction);
	}
	return queued;
}
} // namespace

TEST(CreatureSoundTurn, TheFaceAndGestureAreCheckedBeforeTheActionSoTheActionsSoundsGoFirst)
{
	const auto queued = QueueOf(k_Passed, {}, {}, std::nullopt);
	// all three half way: each sound goes before those already queued at its fraction
	EXPECT_EQ(queued.actions, (std::vector<int32_t> {13, 12, 11}));
	EXPECT_EQ(queued.fractions, (std::vector<float> {0.5f, 0.5f, 0.5f}));
}

TEST(CreatureSoundTurn, WhatTookOverFromTheActionInTheTurnIsCheckedAfterIt)
{
	// a start played to its end, then its loop's first step in the same turn: each sound half way, the loop's checked
	// last, so it goes first
	const creature_layers::TurnPassed passed {.overlays = {}, .action = FirstHundred(1), .nextAction = FirstHundred(3)};
	const auto queued = QueueOf(passed, {}, {}, std::nullopt);
	EXPECT_EQ(queued.actions, (std::vector<int32_t> {13, 11}));
	EXPECT_EQ(queued.fractions, (std::vector<float> {0.5f, 0.5f}));
}

TEST(CreatureSoundTurn, AnimationsPlayedByWeightStandInForTheAction)
{
	// the walk, the heaviest, went from 50 to 150 ms since the turn before: its step half way, and its hair shown
	const std::array last {Played {4, 50.0f}};
	const std::array now {Played {3, 10.0f}, Played {4, 150.0f}};
	const auto queued = QueueOf(k_Passed, last, now, 1);
	EXPECT_EQ(queued.actions, (std::vector<int32_t> {14, 12, 11}));
	EXPECT_EQ(queued.hair, (std::vector<std::pair<int32_t, bool>> {{6, true}}));
	// looped round its end, one stretch over the end and on from the start
	const std::array wrapped {Played {4, 50.0f}};
	const std::array before {Played {4, 950.0f}};
	EXPECT_TRUE(QueueOf({}, before, wrapped, 0).actions.empty());
	const std::array afterStep {Played {4, 101.0f}};
	const auto round = QueueOf({}, before, afterStep, 0);
	EXPECT_EQ(round.actions, (std::vector<int32_t> {14}));
	EXPECT_EQ(round.fractions, (std::vector<float> {150.0f / 151.0f}));
}

TEST(CreatureSoundTurn, AnAnimationJustBroughtInOrUnknownPassesNothing)
{
	const std::array now {Played {4, 150.0f}};
	// not played at the end of the turn before: nothing until the next turn
	EXPECT_TRUE(QueueOf({}, {}, now, 0).actions.empty());
	// nothing weighs anything: the face and gesture sound, and the action played by weight stays silent
	EXPECT_EQ(QueueOf(k_Passed, {}, now, std::nullopt).actions, (std::vector<int32_t> {12, 11}));
	// an animation the species lacks
	const creature_layers::TurnPassed unknown {.overlays = {FirstHundred(9)}, .action = std::nullopt};
	EXPECT_TRUE(QueueOf(unknown, {}, {}, std::nullopt).actions.empty());
}

TEST(CreatureSoundMoments, TheCameraDistanceSumsDepthFirst)
{
	struct Case
	{
		glm::vec3 creature;
		glm::vec3 camera;
		uint32_t distance;
	};
	const std::array<Case, 4> cases {{
	    {{10.0f, 2.0f, 30.0f}, {13.0f, 6.0f, 42.0f}, 0x41500000U},
	    {{1234.567f, 89.1f, 2345.678f}, {1200.25f, 140.75f, 2390.5f}, 0x4299072DU},
	    // summed width first, this one would be a bit further
	    {{0.1f, 0.2f, 0.3f}, {0.7f, -0.4f, 1.9f}, 0x3FE7D15EU},
	    {{-5.3f, 0.0f, 7.77f}, {3.3f, 9.9f, -1.1f}, 0x417D4F2FU},
	}};
	for (const auto& c : cases)
	{
		EXPECT_EQ(Bits(CameraDistance(c.camera, c.creature)), c.distance);
	}
}

TEST(CreatureSoundKeys, TheSizeAndAlignmentKeysMatchTheOriginal)
{
	const std::array<std::pair<float, audio::SoundSize>, 12> sizes {{
	    {-1.0f, audio::SoundSize::Small},
	    {0.0f, audio::SoundSize::Small},
	    {0.5f, audio::SoundSize::Small},
	    {0.6666666f, audio::SoundSize::Small},
	    {0.6666667f, audio::SoundSize::Small},
	    {0.67f, audio::SoundSize::Medium},
	    {1.0f, audio::SoundSize::Medium},
	    {1.3333333f, audio::SoundSize::Medium},
	    {1.3333334f, audio::SoundSize::Medium},
	    {1.34f, audio::SoundSize::Large},
	    {2.0f, audio::SoundSize::Large},
	    {3.0f, audio::SoundSize::Large},
	}};
	for (const auto& [size, key] : sizes)
	{
		EXPECT_EQ(SizeKey(size), key) << size;
	}
	const std::array<std::pair<float, int32_t>, 9> alignments {{
	    {-1.0f, 3},
	    {-0.34f, 3},
	    {-0.3333333f, 3},
	    {-0.3333334f, 3},
	    {0.0f, 2},
	    {0.3333333f, 2},
	    {0.3333334f, 2},
	    {0.34f, 1},
	    {1.0f, 1},
	}};
	for (const auto& [alignment, key] : alignments)
	{
		EXPECT_EQ(AlignmentKey(alignment), key) << alignment;
	}
	// size, alignment, the rig's sound object, the ground and the action, in the banks' order
	EXPECT_EQ(Keys(1.0f, 0.0f, 3, audio::SoundSurface::Gravel, SoundAction::FootstepNormal).ToArray(),
	          (std::array<int32_t, 5> {2, 2, 3, 2, 4}));
}

TEST(CreatureSoundKeys, OnlyAVoiceNeedsTheLocalCreatureOrTheScript)
{
	for (const bool script : {false, true})
	{
		for (const bool local : {false, true})
		{
			const Gate gate {.muted = false, .localPlayersCreature = local, .otherVoicesEnabled = script};
			EXPECT_TRUE(IsHeard(EventKind::Generic, gate));
			EXPECT_EQ(IsHeard(EventKind::Voice, gate), script || local);
		}
	}
}

TEST(CreatureSoundKeys, StepsThrowDustAndCreedGlowsMoveTheHandsWhetherHeardOrNot)
{
	const std::array<std::pair<int32_t, int32_t>, 3> steps {{{3, 3}, {4, 6}, {5, 12}}};
	for (const auto& [action, puffs] : steps)
	{
		EXPECT_EQ(creature_dust::PuffsFor(static_cast<SoundAction>(action)), puffs);
		EXPECT_TRUE(IsFootstep(static_cast<SoundAction>(action)));
		EXPECT_FALSE(MovesCreedGlow(static_cast<SoundAction>(action)));
	}
	for (const int32_t action : {2, 6, 94, 110})
	{
		EXPECT_FALSE(creature_dust::PuffsFor(static_cast<SoundAction>(action)).has_value()) << action;
		EXPECT_FALSE(IsFootstep(static_cast<SoundAction>(action))) << action;
		EXPECT_FALSE(MovesCreedGlow(static_cast<SoundAction>(action))) << action;
	}
	for (const int32_t action : {95, 100, 109})
	{
		EXPECT_TRUE(MovesCreedGlow(static_cast<SoundAction>(action))) << action;
		EXPECT_FALSE(creature_dust::PuffsFor(static_cast<SoundAction>(action)).has_value()) << action;
	}
}
