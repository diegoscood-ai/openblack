/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <bit>
#include <limits>
#include <vector>

#include <gtest/gtest.h>

#include "Temple/HeartFade.h"
#include "Temple/HeartShell.h"
#include "Temple/TempleDestruction.h"

using namespace openblack::temple_destruction;

namespace
{
/// Every turn's events at a tenth of a second a turn, from the first turn on, until the end
std::vector<Events> RunToEnd()
{
	std::vector<Events> turns;
	float clock = 0.0f;
	for (int turn = 0; turn < 400; ++turn)
	{
		const float before = clock;
		clock += 100.0f * 0.001f;
		turns.push_back(Between(before, clock));
		if (turns.back().end)
		{
			break;
		}
	}
	return turns;
}
} // namespace

TEST(TempleDestruction, EachMomentHappensOnceOnTheTurnTheClockPassesIt)
{
	const auto turns = RunToEnd();
	const auto count = [&turns](auto field) {
		int n = 0;
		for (const auto& events : turns)
		{
			n += (events.*field) ? 1 : 0;
		}
		return n;
	};
	EXPECT_EQ(count(&Events::loopStarts), 1);
	EXPECT_EQ(count(&Events::shellShown), 1);
	EXPECT_EQ(count(&Events::shellGoes), 1);
	EXPECT_EQ(count(&Events::glow), 1);
	EXPECT_EQ(count(&Events::explosion), 1);
	EXPECT_EQ(count(&Events::smoke), 1);
	EXPECT_EQ(count(&Events::end), 1);
	// The loop on the first turn, the end with the smoke on the same last turn, about 22 seconds on
	const bool loopsFirst = turns.front().loopStarts;
	const bool smokesLast = turns.back().smoke;
	EXPECT_TRUE(loopsFirst);
	EXPECT_TRUE(smokesLast);
	// The shell goes with the smoke, at the end
	EXPECT_TRUE(turns.back().shellGoes);
	EXPECT_GE(turns.size(), 220u);
	EXPECT_LE(turns.size(), 221u);
}

TEST(TempleDestruction, TheLocalPlayersGlowIsAsStrongAsTheirPulseFromAQuarterToWhole)
{
	EXPECT_EQ(GlowStrength(0.0f), 0.25f);
	EXPECT_EQ(GlowStrength(1.0f), 1.0f);
	EXPECT_EQ(GlowStrength(0.5f), 0.625f);
	EXPECT_EQ(GlowStrength(0.3f), 0.75f * 0.3f + 0.25f);
}

TEST(TempleDestruction, AMomentIsPassedFromAtItToBeforeItAfter)
{
	EXPECT_TRUE(Passes(0.0f, 0.0f, 0.1f));
	EXPECT_FALSE(Passes(0.0f, 0.1f, 0.1f));
	EXPECT_FALSE(Passes(0.1f, 0.0f, 0.2f));
}

TEST(TempleDestruction, TheHeartFadesFromFourteenSecondsOverFour)
{
	EXPECT_EQ(HeartAlpha(0.0f), 255);
	EXPECT_EQ(HeartAlpha(14.0f), 255);
	EXPECT_EQ(HeartAlpha(16.0f), 127);
	EXPECT_EQ(HeartAlpha(18.0f), 0);
	EXPECT_EQ(HeartAlpha(21.0f), 0);
}

TEST(TempleDestruction, BeamsComeFasterBrighterAndShorterLivedOverFourteenSeconds)
{
	EXPECT_FALSE(Beaming(-0.1f));
	EXPECT_TRUE(Beaming(0.0f));
	EXPECT_TRUE(Beaming(13.9f));
	EXPECT_FALSE(Beaming(14.0f));
	EXPECT_EQ(BeamShare(-1.0f), 0.0f);
	EXPECT_EQ(BeamShare(7.0f), 0.5f);
	EXPECT_EQ(BeamShare(20.0f), 1.0f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 0.0f), 1.4f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 0.5f), 1.3f);
	EXPECT_FLOAT_EQ(NextBeam(1.0f, 1.0f), 1.2f);
	const auto first = BeamLookAt(0.0f);
	EXPECT_EQ(first.life, 3.0f);
	EXPECT_EQ(first.speed, 1.0f);
	EXPECT_EQ(first.alpha, 50);
	const auto last = BeamLookAt(1.0f);
	EXPECT_FLOAT_EQ(last.life, 0.7f);
	EXPECT_EQ(last.speed, 1.5f);
	EXPECT_EQ(last.alpha, 200);
	// The alpha is cut down to a whole number
	EXPECT_EQ(BeamLookAt(0.999f).alpha, 199);
}

// Each step is rounded to a float, as the game's single precision rounds it: worked out at a higher precision and
// rounded once, these two would each be one unit apart
TEST(TempleDestruction, TheBeamsStepsAreSingleFloats)
{
	const auto share = std::bit_cast<float>(0x3F55F4B4u);
	EXPECT_EQ(std::bit_cast<uint32_t>(NextBeam(std::bit_cast<float>(0x40C1E131u), share)), 0x40C954ACu);
	EXPECT_EQ(std::bit_cast<uint32_t>(BeamLookAt(std::bit_cast<float>(0x3F4324CAu)).life), 0x3F9F95B2u);
}

TEST(TempleDestruction, TheLocalHeartbeatQuickensAlongItsThreePoints)
{
	// Before the first point and from the last on, their own pace
	EXPECT_EQ(HeartbeatPace(-1.0f), k_HeartbeatPace[0]);
	EXPECT_EQ(HeartbeatPace(0.0f), k_HeartbeatPace[0]);
	EXPECT_EQ(HeartbeatPace(25.0f), k_HeartbeatPace[2]);
	EXPECT_EQ(HeartbeatPace(30.0f), k_HeartbeatPace[2]);
	// At a point, its pace; between two, drawn straight along, a float a step
	EXPECT_EQ(HeartbeatPace(19.0f), k_HeartbeatPace[1]);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(0.1f)), 0x3F23D519u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(9.5f)), 0x3F747644u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(21.5f)), 0x4019A338u);
	EXPECT_EQ(std::bit_cast<uint32_t>(HeartbeatPace(22.0f)), 0x40281150u);
}

TEST(TempleDestruction, TheGlowBeatsFromAQuarterToWhole)
{
	EXPECT_EQ(GlowPower(0.0f), 0.25f);
	EXPECT_EQ(GlowPower(0.5f), 0.625f);
	EXPECT_EQ(GlowPower(1.0f), 1.0f);
}

TEST(TempleDestruction, SpotVisualsLastTheirSecondsInTurns)
{
	EXPECT_EQ(TurnsFor(100, k_GlowSeconds), 60);
	EXPECT_EQ(TurnsFor(100, k_ExplosionSeconds), 150);
	EXPECT_EQ(SmokeTurns(100, 0.0f), 60);
	EXPECT_EQ(SmokeTurns(100, 0.4f), 90);
	EXPECT_EQ(SmokeTurns(100, 0.123f), 69);
}

TEST(TempleDestruction, OnlyTheLocalPlayersLossOutsideSkirmishesAndOnlineGamesEndsTheGameOnce)
{
	EXPECT_TRUE(GameOverStarts({.localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({}));
	EXPECT_FALSE(GameOverStarts({.over = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.skirmish = true, .localTempleDestroying = true}));
	EXPECT_FALSE(GameOverStarts({.multiplayer = true, .localTempleDestroying = true}));
}

// The shell over the heart as it is destroyed

TEST(HeartShell, TheModelSwellsAlongTheClockFromNothingToTwiceItsSize)
{
	using namespace openblack;
	EXPECT_EQ(heart_shell::Size(0.0f), 0.0f);
	EXPECT_EQ(heart_shell::Size(0.5f), 0.5f * 0.3f);
	EXPECT_EQ(heart_shell::Size(14.0f), 0.3f);
	EXPECT_EQ(heart_shell::Size(21.0f), 1.5f * 0.3f);
	EXPECT_EQ(heart_shell::Size(30.0f), 2.0f * 0.3f);
	// a clock that is not a number takes the first moment's
	EXPECT_EQ(heart_shell::Size(std::numeric_limits<float>::quiet_NaN()), 0.0f);
}

TEST(HeartShell, TheShellReachesOutAlongTheClockByTheHeartbeat)
{
	using namespace openblack;
	EXPECT_EQ(heart_shell::Reach(0.5f, 1.0f), 2.5f);
	EXPECT_EQ(heart_shell::Reach(14.0f, 0.25f), 1.25f);
	EXPECT_EQ(heart_shell::Reach(21.0f, 0.5f), 3.75f);
	// another player's heart has no beat: no reach
	EXPECT_EQ(heart_shell::Reach(21.0f, 0.0f), 0.0f);
}

TEST(HeartShell, TheModelIsUnturnedAtTheHeart)
{
	using namespace openblack;
	const auto model = heart_shell::Model({1.0f, 2.0f, 3.0f}, 0.45f);
	EXPECT_EQ(model[0], glm::vec4(0.45f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[1], glm::vec4(0.0f, 0.45f, 0.0f, 0.0f));
	EXPECT_EQ(model[2], glm::vec4(0.0f, 0.0f, 0.45f, 0.0f));
	EXPECT_EQ(model[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
}

TEST(HeartShell, OnceTheHeartIsNoLongerDrawnItsShellIsDrawnAlone)
{
	using namespace openblack;
	using heart_shell::Placement;
	EXPECT_EQ(heart_shell::PlacementOf(true, false), Placement::AtHeart);
	// from a little after the explosion the heart has no row, and its shell is drawn alone
	EXPECT_EQ(heart_shell::PlacementOf(false, !HeartDrawn(true, std::nextafter(k_HiddenAfter, 30.0f))), Placement::Alone);
	// a heart left out for another reason takes its shell with it
	EXPECT_EQ(heart_shell::PlacementOf(false, !HeartDrawn(true, k_HiddenAfter)), Placement::None);
	// a row still drawn keeps the shell at its row
	EXPECT_EQ(heart_shell::PlacementOf(true, true), Placement::AtHeart);
}

TEST(HeartShell, TheHeartsAndTheirShellsAreDrawnNewestFirst)
{
	using namespace openblack;
	// the hearts' list takes each new heart at its head
	const std::vector<uint32_t> stamps {2, 5, 1, 3};
	EXPECT_EQ(heart_shell::DrawOrder(stamps), (std::vector<size_t> {1, 3, 0, 2}));
	EXPECT_TRUE(heart_shell::DrawOrder({}).empty());
}

TEST(HeartShell, EachHeartIsDrawnRightAfterItsOwnShellNewestFirst)
{
	using namespace openblack;
	using heart_shell::PassDraw;
	using heart_shell::PassStep;
	// two hearts destroyed at once, a whole heart, and one past a little after its explosion (its shell alone)
	const std::vector<heart_shell::PassHeart> hearts {
	    {.creationStamp = 1, .shell = true, .model = true},
	    {.creationStamp = 4, .shell = true, .model = true},
	    {.creationStamp = 2, .shell = false, .model = true},
	    {.creationStamp = 3, .shell = true, .model = false},
	};
	EXPECT_EQ(heart_shell::Pass(hearts), (std::vector<PassDraw> {
	                                         {.heart = 1, .step = PassStep::Shell},
	                                         {.heart = 1, .step = PassStep::Model},
	                                         {.heart = 3, .step = PassStep::Shell},
	                                         {.heart = 2, .step = PassStep::Model},
	                                         {.heart = 0, .step = PassStep::Shell},
	                                         {.heart = 0, .step = PassStep::Model},
	                                     }));
	// a single heart: its shell, then its model
	const std::vector<heart_shell::PassHeart> one {{.creationStamp = 7, .shell = true, .model = true}};
	EXPECT_EQ(heart_shell::Pass(one),
	          (std::vector<PassDraw> {{.heart = 0, .step = PassStep::Shell}, {.heart = 0, .step = PassStep::Model}}));
	EXPECT_TRUE(heart_shell::Pass({}).empty());
}

// The colours a citadel heart is drawn with

TEST(HeartFade, AWholeHeartsLandLightIsTimesWhite)
{
	using namespace openblack;
	EXPECT_EQ(heart_fade::Tint(heart_fade::k_Whole), 0xFFFFFFFFu);
	// the heart's alpha in the tint's, the colour white
	EXPECT_EQ(heart_fade::Tint(0x80), 0x80FFFFFFu);
	EXPECT_EQ(heart_fade::Tint(0), 0x00FFFFFFu);
}

TEST(HeartFade, ItIsDrawnSeeThroughOnceItsDrawnAlphaIsLowEnoughWhileItIsDestroyed)
{
	using namespace openblack;
	// the land light's alpha, whole, times the tint's, over 256
	EXPECT_EQ(heart_fade::DrawnAlpha(0xFF), 0xFE);
	EXPECT_EQ(heart_fade::DrawnAlpha(0xF2), 0xF1);
	EXPECT_EQ(heart_fade::DrawnAlpha(0xF1), 0xF0);
	EXPECT_EQ(heart_fade::DrawnAlpha(0), 0);
	EXPECT_FALSE(heart_fade::TurnsSeeThrough(0xF2, true));
	EXPECT_TRUE(heart_fade::TurnsSeeThrough(0xF1, true));
	EXPECT_TRUE(heart_fade::TurnsSeeThrough(0, true));
	// a heart that is not being destroyed never turns see-through
	EXPECT_FALSE(heart_fade::TurnsSeeThrough(0, false));
}

TEST(HeartFade, ASeeThroughModelsMaterialsChangeOnceAndForAll)
{
	using namespace openblack;
	// the original changes them at every see-through draw: once is the same
	for (uint32_t i = 0; i < graphics::render_modes::k_ModeCount; ++i)
	{
		const auto once = graphics::render_modes::ModeFromProperties(static_cast<graphics::render_modes::Mode>(i),
		                                                             heart_fade::k_SeeThroughMaterial);
		EXPECT_EQ(graphics::render_modes::ModeFromProperties(once, heart_fade::k_SeeThroughMaterial), once) << i;
	}
	EXPECT_EQ(graphics::render_modes::ModeFromProperties(graphics::render_modes::Mode::AlphaTextured,
	                                                     heart_fade::k_SeeThroughMaterial),
	          graphics::render_modes::Mode::AlphaTexturedAlphaNoZWrite);
}

TEST(HeartFade, TheSnowOnASeeThroughHeartThinsWithItsAlpha)
{
	using namespace openblack;
	// the level times the alpha's share above 128, out of 128, rounded down
	EXPECT_EQ(heart_fade::SnowLevel(255, 0xF0), 223);
	EXPECT_EQ(heart_fade::SnowLevel(100, 0xF0), 87);
	EXPECT_EQ(heart_fade::SnowLevel(255, 0x81), 1);
	// none from 128 down, nor for a product below 128
	EXPECT_EQ(heart_fade::SnowLevel(255, 0x80), 0);
	EXPECT_EQ(heart_fade::SnowLevel(255, 0x10), 0);
	EXPECT_EQ(heart_fade::SnowLevel(1, 0x81), 0);
	EXPECT_EQ(heart_fade::SnowLevel(0, 0xF0), 0);
	// at most 255
	EXPECT_EQ(heart_fade::SnowLevel(300, 0xFF), 255);
	EXPECT_EQ(heart_fade::SnowLevel(257, 0xFF), 254);
}

TEST(HeartFade, FromThreeAndAHalfSecondsItTakesItsGodsColourByTheHeartbeat)
{
	using namespace openblack;
	// none up to three and a half seconds, or for a clock that is not a number
	EXPECT_FALSE(heart_fade::Specular(3.5f, 1.0f, 0xFFFFFFFFu).has_value());
	EXPECT_FALSE(heart_fade::Specular(std::numeric_limits<float>::quiet_NaN(), 1.0f, 0xFFFFFFFFu).has_value());
	// half way up: the level 35, each channel times 35 over 256, the alpha the god's
	EXPECT_EQ(heart_fade::Specular(5.0f, 1.0f, 0xFF8040C0u), 0xFF11081Au);
	// a sixth of the way: 18.33 truncated to 18
	EXPECT_EQ(heart_fade::Specular(4.0f, 1.0f, 0xFFFFFFFFu), 0xFF111111u);
	// all the way up and held there: the level 60
	EXPECT_EQ(heart_fade::Specular(10.0f, 1.0f, 0xFFFFFFFFu), 0xFF3B3B3Bu);
	EXPECT_EQ(heart_fade::Specular(30.0f, 1.0f, 0x80FFFFFFu), 0x803B3B3Bu);
	// another player's heart has no beat: the level 10 alone
	EXPECT_EQ(heart_fade::Specular(10.0f, 0.0f, 0xFFFFFFFFu), 0xFF090909u);
	// the level keeps its low byte: 310 gives 54
	EXPECT_EQ(heart_fade::Specular(10.0f, 6.0f, 0xFFFFFFFFu), 0xFF353535u);
	// a pulse that is not a number gives the lowest integer, whose low byte is 0
	EXPECT_EQ(heart_fade::Specular(10.0f, std::numeric_limits<float>::quiet_NaN(), 0xFFFFFFFFu), 0xFF000000u);
}

TEST(TempleDestruction, TheHeartIsNotDrawnFromALittleAfterTheExplosion)
{
	EXPECT_EQ(k_HiddenAfter, 21.8f);
	EXPECT_TRUE(HeartDrawn(false, 30.0f));
	EXPECT_TRUE(HeartDrawn(true, 0.0f));
	EXPECT_TRUE(HeartDrawn(true, k_HiddenAfter));
	EXPECT_FALSE(HeartDrawn(true, std::nextafter(k_HiddenAfter, 30.0f)));
}
