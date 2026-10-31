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

#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "Camera/ScreenPoint.h"
#include "Help/SpeedIndicator.h"

using namespace openblack;
using namespace openblack::help;

namespace
{
constexpr float k_Pi = glm::pi<float>();

/// An indicator showing `state` at full fade: the frame that switches to it, then a second of real time
SpeedIndicator FullyShowing(SpeedIndicatorState state)
{
	SpeedIndicator indicator;
	EXPECT_FALSE(indicator.Update(state, 1, {}).has_value());
	EXPECT_TRUE(indicator.Update(state, 1000, {}).has_value());
	EXPECT_EQ(indicator.Shown(), state);
	EXPECT_EQ(indicator.Fade(), 1.0f);
	return indicator;
}
} // namespace

TEST(SpeedIndicator, PausedOverridesTheSpeed)
{
	EXPECT_EQ(StateOf(false, 0.5f), SpeedIndicatorState::Slow);
	EXPECT_EQ(StateOf(false, 1.0f), SpeedIndicatorState::None);
	EXPECT_EQ(StateOf(false, 2.0f), SpeedIndicatorState::Fast);
	EXPECT_EQ(StateOf(true, 2.0f), SpeedIndicatorState::Paused);
	EXPECT_EQ(StateOf(true, 1.0f), SpeedIndicatorState::Paused);
	// The texts follow each other from the paused one
	EXPECT_EQ(SpeedText(SpeedIndicatorState::Paused), 4731u);
	EXPECT_EQ(SpeedText(SpeedIndicatorState::Slow), 4732u);
	EXPECT_EQ(SpeedText(SpeedIndicatorState::Fast), 4733u);
}

TEST(SpeedIndicator, TheFadeIsEasedInAndOut)
{
	EXPECT_EQ(EasedFade(-0.1f), 0.0f);
	EXPECT_EQ(EasedFade(std::nanf("")), 0.0f);
	EXPECT_EQ(EasedFade(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(EasedFade(0.5f), 0.5f);
	EXPECT_EQ(EasedFade(1.0f), 1.0f);
	EXPECT_EQ(EasedFade(1.5f), 1.0f);
	EXPECT_NEAR(EasedFade(0.3f), (1.0f - std::cos(0.3f * k_Pi)) * 0.5f, 1e-6f);
}

TEST(SpeedIndicator, NothingIsShownAtNormalSpeed)
{
	SpeedIndicator indicator;
	for (int i = 0; i < 100; ++i)
	{
		EXPECT_FALSE(indicator.Update(SpeedIndicatorState::None, 16, {}).has_value());
	}
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::None);
	EXPECT_EQ(indicator.Fade(), 0.0f);
	EXPECT_EQ(indicator.Phase(), 0.0f);
}

TEST(SpeedIndicator, FadesInAThousandthOfThreeAMillisecond)
{
	SpeedIndicator indicator;
	// The first frame only takes the new words, at no fade: nothing drawn
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Slow, 16, {}).has_value());
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Slow);
	EXPECT_EQ(indicator.Fade(), 0.0f);
	const auto frame = indicator.Update(SpeedIndicatorState::Slow, 100, {});
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(indicator.Fade(), 100.0f * k_SpeedFadeInPerMs);
	EXPECT_EQ(frame->shown, SpeedIndicatorState::Slow);
	EXPECT_EQ(frame->t, EasedFade(indicator.Fade()));
	EXPECT_EQ(frame->depth, (frame->t + 1.0f) * 15.0f);
	// The tumble goes on only in frames that draw
	EXPECT_EQ(frame->phase, 100.0f * k_SpeedPhasePerMs);

	// A millisecond at a time it reaches 1 at the 334th
	SpeedIndicator steps;
	ASSERT_FALSE(steps.Update(SpeedIndicatorState::Fast, 1, {}).has_value());
	for (int i = 0; i < 333; ++i)
	{
		ASSERT_TRUE(steps.Update(SpeedIndicatorState::Fast, 1, {}).has_value());
	}
	EXPECT_LT(steps.Fade(), 1.0f);
	const auto full = steps.Update(SpeedIndicatorState::Fast, 1, {});
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(steps.Fade(), 1.0f);
	EXPECT_EQ(full->t, 1.0f);
	EXPECT_EQ(full->depth, 30.0f);
}

TEST(SpeedIndicator, TheOldWordsFadeOutBeforeTheNewOnesFadeIn)
{
	auto indicator = FullyShowing(SpeedIndicatorState::Slow);
	// Sped up: the slow words fade out at a thousandth of two a millisecond, still drawn
	const auto fading = indicator.Update(SpeedIndicatorState::Fast, 100, {});
	ASSERT_TRUE(fading.has_value());
	EXPECT_EQ(fading->shown, SpeedIndicatorState::Slow);
	EXPECT_EQ(indicator.Fade(), 1.0f - 100.0f * k_SpeedFadeOutPerMs);
	// Down to 0 the fast words take their place, at no fade
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Fast, 400, {}).has_value());
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Fast);
	EXPECT_EQ(indicator.Fade(), 0.0f);
	const auto fast = indicator.Update(SpeedIndicatorState::Fast, 100, {});
	ASSERT_TRUE(fast.has_value());
	EXPECT_EQ(fast->shown, SpeedIndicatorState::Fast);

	// Back to normal speed they fade out and nothing is shown
	auto back = FullyShowing(SpeedIndicatorState::Fast);
	EXPECT_TRUE(back.Update(SpeedIndicatorState::None, 400, {}).has_value());
	EXPECT_FALSE(back.Update(SpeedIndicatorState::None, 100, {}).has_value());
	EXPECT_EQ(back.Shown(), SpeedIndicatorState::None);
	EXPECT_FALSE(back.Update(SpeedIndicatorState::None, 100, {}).has_value());
}

TEST(SpeedIndicator, ASequenceResetsTheWords)
{
	auto indicator = FullyShowing(SpeedIndicatorState::Slow);
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Fast, 16, {.sequenceMode = true}).has_value());
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Fast);
	EXPECT_EQ(indicator.Fade(), 0.0f);
	// Paused too
	auto paused = FullyShowing(SpeedIndicatorState::Paused);
	EXPECT_FALSE(paused.Update(SpeedIndicatorState::Paused, 16, {.sequenceMode = true}).has_value());
	EXPECT_EQ(paused.Fade(), 0.0f);
}

TEST(SpeedIndicator, AScriptOwningTheSpeedResetsAllButPaused)
{
	for (const auto state : {SpeedIndicatorState::Slow, SpeedIndicatorState::Fast})
	{
		auto indicator = FullyShowing(state);
		EXPECT_FALSE(indicator.Update(state, 16, {.scriptOwnsSpeed = true}).has_value());
		EXPECT_EQ(indicator.Shown(), state);
		EXPECT_EQ(indicator.Fade(), 0.0f);
	}
	// Normal speed under a script: the fading words are put out at once
	auto normal = FullyShowing(SpeedIndicatorState::Slow);
	EXPECT_FALSE(normal.Update(SpeedIndicatorState::None, 16, {.scriptOwnsSpeed = true}).has_value());
	EXPECT_EQ(normal.Shown(), SpeedIndicatorState::None);
	EXPECT_EQ(normal.Fade(), 0.0f);
	// Paused still shows
	auto paused = FullyShowing(SpeedIndicatorState::Paused);
	const auto frame = paused.Update(SpeedIndicatorState::Paused, 16, {.scriptOwnsSpeed = true});
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(frame->shown, SpeedIndicatorState::Paused);
	EXPECT_EQ(paused.Fade(), 1.0f);
}

TEST(SpeedIndicator, TheEditorAFilmAndTheSetupBoxResetTheWords)
{
	const std::array<SpeedIndicatorGates, 3> gates {{
	    {.editorMode = true},
	    {.videoPlaying = true},
	    {.setupBoxFlag = true},
	}};
	for (const auto& gate : gates)
	{
		auto indicator = FullyShowing(SpeedIndicatorState::Fast);
		EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Slow, 16, gate).has_value());
		EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Slow);
		EXPECT_EQ(indicator.Fade(), 0.0f);
		// Paused too
		auto paused = FullyShowing(SpeedIndicatorState::Paused);
		EXPECT_FALSE(paused.Update(SpeedIndicatorState::Paused, 16, gate).has_value());
		EXPECT_EQ(paused.Shown(), SpeedIndicatorState::Paused);
		EXPECT_EQ(paused.Fade(), 0.0f);
	}
}

TEST(SpeedIndicator, WordsUnderABoxComeBackFromNothingOnceItCloses)
{
	// paused words fully shown, then a box opens over them: nothing is drawn and the fade stays at 0
	auto indicator = FullyShowing(SpeedIndicatorState::Paused);
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Paused, 16, {.setupBoxFlag = true}).has_value());
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Paused, 1000, {.setupBoxFlag = true}).has_value());
	EXPECT_EQ(indicator.Fade(), 0.0f);
	// the box closed: the words fade in again from nothing
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Paused, 0, {}).has_value());
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Paused);
	const auto frame = indicator.Update(SpeedIndicatorState::Paused, 1000, {});
	ASSERT_TRUE(frame.has_value());
	EXPECT_EQ(indicator.Fade(), 1.0f);
}

TEST(SpeedIndicator, WithoutAGameNothingIsShownNorReset)
{
	SpeedIndicator indicator;
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Slow, 1, {.gameRunning = false}).has_value());
	EXPECT_FALSE(indicator.Update(SpeedIndicatorState::Slow, 100, {.gameRunning = false}).has_value());
	EXPECT_EQ(indicator.Shown(), SpeedIndicatorState::Slow);
	EXPECT_EQ(indicator.Fade(), 100.0f * k_SpeedFadeInPerMs);
	EXPECT_EQ(indicator.Phase(), 0.0f);
}

TEST(SpeedIndicator, TheTumbleWrapsOncePastFourPi)
{
	auto indicator = FullyShowing(SpeedIndicatorState::Paused);
	// The second of FullyShowing's frames drew: 1.5 radians
	EXPECT_EQ(indicator.Phase(), 1000.0f * k_SpeedPhasePerMs);
	float expected = indicator.Phase();
	for (int i = 0; i < 8; ++i)
	{
		ASSERT_TRUE(indicator.Update(SpeedIndicatorState::Paused, 1000, {}).has_value());
		expected += 1000.0f * k_SpeedPhasePerMs;
	}
	// 13.5 is past 4 pi: once taken off
	EXPECT_FLOAT_EQ(expected, 13.5f);
	EXPECT_EQ(indicator.Phase(), expected - 4.0f * k_Pi);
	EXPECT_NEAR(indicator.Phase(), 0.933629f, 1e-6f);
}

TEST(SpeedIndicator, TheLettersSpanHalfTheScreenAtTheirFurthest)
{
	// 17 letters at depth 30 on a screen 1024 wide: a letter every 30.1 pixels, centred on 512
	EXPECT_EQ(LetterColumn(1024, 17, 30.0f, 0), 271);
	EXPECT_EQ(LetterColumn(1024, 17, 30.0f, 8), 512);
	EXPECT_EQ(LetterColumn(1024, 17, 30.0f, 16), 752);
	// Nearer, they spread out: at depth 15 twice as far apart
	EXPECT_LT(LetterColumn(1024, 17, 15.0f, 0), LetterColumn(1024, 17, 30.0f, 0) - 200);
	// One letter is in the middle; an odd width is halved down
	EXPECT_EQ(LetterColumn(1024, 1, 30.0f, 0), 512);
	EXPECT_EQ(LetterColumn(1025, 1, 30.0f, 0), 512);
}

TEST(SpeedIndicator, EachLetterTurnsAboutOneAxisThenTheOther)
{
	const auto at = [](float phase) { return TurnOf(phase, 0); };
	EXPECT_FALSE(at(0.0f).aboutVertical);
	EXPECT_EQ(at(0.0f).angle, 0.0f);
	// Folded into [-pi / 2, pi / 2)
	EXPECT_FALSE(at(k_Pi / 2.0f).aboutVertical);
	EXPECT_FLOAT_EQ(at(k_Pi / 2.0f).angle, -k_Pi / 2.0f);
	EXPECT_EQ(at(k_Pi).angle, 0.0f);
	EXPECT_FALSE(at(3.0f * k_Pi / 2.0f).aboutVertical);
	EXPECT_LT(at(3.0f * k_Pi / 2.0f).angle, k_Pi / 2.0f);
	EXPECT_NEAR(at(3.0f * k_Pi / 2.0f).angle, k_Pi / 2.0f, 1e-6f);
	// The second half of the cycle is about the vertical axis
	EXPECT_TRUE(at(2.0f * k_Pi).aboutVertical);
	EXPECT_EQ(at(2.0f * k_Pi).angle, 0.0f);
	EXPECT_TRUE(at(7.0f * k_Pi / 2.0f).aboutVertical);
	EXPECT_NEAR(at(7.0f * k_Pi / 2.0f).angle, -k_Pi / 2.0f, 1e-6f);
	// Each letter is a third of a radian behind the one before: the second at the start of the cycle is near its end
	const auto second = TurnOf(0.0f, 1);
	EXPECT_TRUE(second.aboutVertical);
	EXPECT_NEAR(second.angle, -1.0f / 3.0f, 1e-6f);
	for (float phase = 0.0f; phase < 4.0f * k_Pi; phase += 0.1f)
	{
		const auto turn = TurnOf(phase, 5);
		EXPECT_GE(turn.angle, -k_Pi / 2.0f) << phase;
		EXPECT_LT(turn.angle, k_Pi / 2.0f) << phase;
	}
}

TEST(SpeedIndicator, AnUnturnedLetterFacesTheEyeTiltedByThePitch)
{
	const float pitch = 0.3f;
	const auto axes = LetterAxes({}, pitch);
	// Along the text: to the right
	EXPECT_NEAR(axes[0].x, 1.0f, 1e-6f);
	EXPECT_NEAR(axes[0].y, 0.0f, 1e-6f);
	EXPECT_NEAR(axes[0].z, 0.0f, 1e-6f);
	// Down the letters: down the screen, leaning with the pitch
	EXPECT_NEAR(axes[1].x, 0.0f, 1e-6f);
	EXPECT_NEAR(axes[1].y, -std::cos(pitch), 1e-6f);
	EXPECT_NEAR(axes[1].z, std::sin(pitch), 1e-6f);
	// Off the page: back towards the eye, along the ray to the letters' row
	EXPECT_NEAR(axes[2].x, 0.0f, 1e-6f);
	EXPECT_NEAR(axes[2].y, -std::sin(pitch), 1e-6f);
	EXPECT_NEAR(axes[2].z, -std::cos(pitch), 1e-6f);
	// A turn about the vertical axis turns the text's direction away from the screen
	const auto turned = LetterAxes({.aboutVertical = true, .angle = 0.5f}, 0.0f);
	EXPECT_NEAR(turned[0].x, std::cos(0.5f), 1e-6f);
	EXPECT_NEAR(turned[1].y, -1.0f, 1e-6f);
}

TEST(SpeedIndicator, ThePitchIsTheAngleBetweenTheRays)
{
	EXPECT_NEAR(Pitch({0.0f, 0.0f, 10.0f}, {0.0f, 10.0f, 10.0f}), k_Pi / 4.0f, 1e-6f);
	EXPECT_NEAR(Pitch({0.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 5.0f}), 0.0f, 1e-3f);
	// A ray of nothing stays nothing: a right angle
	EXPECT_NEAR(Pitch({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), k_Pi / 2.0f, 1e-6f);
}

TEST(SpeedIndicator, TheLettersStandInARowNearTheTopOfTheScreen)
{
	// A camera at the origin looking along z, on a screen of 1024 by 768
	const graphics::billboard::CameraFrame camera {};
	const auto lens = screen_point::LensOf(1024, 768, glm::radians(60.0f));
	const auto frames = PlaceLetters(camera, lens, 1024, 768, 17, 30.0f, 0.0f);
	ASSERT_EQ(frames.size(), 17u);
	const auto row = screen_point::CameraPointFromScreen(lens, 0, 768 / 20, 30.0f);
	const auto centre = screen_point::CameraPointFromScreen(lens, 512, 384, 10.0f);
	const auto top = screen_point::CameraPointFromScreen(lens, 512, 768 / 20, 10.0f);
	for (size_t i = 0; i < frames.size(); ++i)
	{
		const auto& frame = frames[i];
		const auto expected = screen_point::CameraPointFromScreen(lens, LetterColumn(1024, 17, 30.0f, i), 768 / 20, 30.0f);
		EXPECT_FLOAT_EQ(frame.origin.x, expected.x) << i;
		EXPECT_FLOAT_EQ(frame.origin.y, row.y) << i;
		EXPECT_FLOAT_EQ(frame.origin.z, 30.0f) << i;
		// With the camera the world's axes, the letters' axes are those of their turn
		const auto axes = LetterAxes(TurnOf(0.0f, i), Pitch(centre, top));
		for (size_t axis = 0; axis < 3; ++axis)
		{
			EXPECT_NEAR(frame.axes.at(axis).x, axes.at(axis).x, 1e-6f) << i << " " << axis;
			EXPECT_NEAR(frame.axes.at(axis).y, axes.at(axis).y, 1e-6f) << i << " " << axis;
			EXPECT_NEAR(frame.axes.at(axis).z, axes.at(axis).z, 1e-6f) << i << " " << axis;
		}
	}
	// Left to right across the screen
	EXPECT_LT(frames.front().origin.x, 0.0f);
	EXPECT_GT(frames.back().origin.x, 0.0f);
	EXPECT_GT(row.y, 0.0f);
}
