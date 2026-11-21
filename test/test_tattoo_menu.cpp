/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <string>
#include <vector>

#include <PhysiqueFile.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/TattooEditor.h"
#include "Creature/TattooMenu.h"

using namespace openblack;
using namespace openblack::tattoo_menu;

namespace
{
float HalfStrength(CreatureType /*species*/)
{
	return 0.5f;
}

bool NoAnimation(size_t /*animation*/)
{
	return false;
}

const creature_tattoo_editor::Orbit k_View {.eye = creature_tattoo_editor::k_MenuEye,
                                            .focus = creature_tattoo_editor::k_MenuFocus,
                                            .turnSpeed = 0.0f,
                                            .tipSpeed = 0.0f};

/// The body's frame state, and the draws a frame made on the local stream: each range asked for, answered by `answer`
struct Frames
{
	creature_tattoo_editor::CopyLife life;
	creature_layers::BodyAction body;
	creature_layers::GestureLayer gesture;
	std::optional<glm::vec3> lookAt;
	std::vector<int32_t> draws;
	uint32_t answer {0};

	void Step(const Input& input)
	{
		tattoo_menu::Step(life, body, gesture, lookAt, input, k_View, NoAnimation, [this](int32_t range) {
			draws.push_back(range);
			return range == 3 ? answer : 5000u;
		});
	}
};
} // namespace

// The body the editor shows without a physique file: the first species of the game's tables at size 1, half fat, half
// strong and neither evil nor good (docs/wiki/creature/tattoos.md#the-tattoo-editor-from-the-players-page)
TEST(TattooMenu, StandInIsAHalfFatHalfStrongApe)
{
	const auto shown = StandIn();
	EXPECT_EQ(shown.species, CreatureType::GiantApe);
	EXPECT_FLOAT_EQ(shown.morph.evilGood, 0.0f);
	EXPECT_FLOAT_EQ(shown.morph.thinFat, 0.5f);
	EXPECT_FLOAT_EQ(shown.morph.weakStrong, 0.5f);
	EXPECT_TRUE(shown.firstMarks.empty());
	EXPECT_TRUE(shown.secondMarks.empty());
	EXPECT_EQ(k_Size, 1.0f);
}

TEST(TattooMenu, NoFileShowsTheStandIn)
{
	EXPECT_EQ(FromPhysique(std::nullopt, HalfStrength), StandIn());
}

TEST(TattooMenu, UnknownSpeciesShowsTheStandIn)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = 1000;
	EXPECT_EQ(FromPhysique(physique, HalfStrength), StandIn());
}

// The file's body: its species, the shape its alignment, fatness and strength give, each held to -1..1, the strength
// counting with the species' own, and the file's two lists of skin marks as they are
TEST(TattooMenu, FileGivesSpeciesShapeAndMarks)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = static_cast<uint32_t>(CreatureType::Cow);
	physique.size = 3.0f;
	physique.alignment = -2.0f;
	physique.fatness = 1.0f;
	physique.strength = 1.0f;
	physique.listA = {0x01020304u};
	physique.listB = {0x05060708u, 0x0A0B0C0Du};
	const auto shown = FromPhysique(physique, HalfStrength);
	EXPECT_EQ(shown.species, CreatureType::Cow);
	const auto expected = creature_morph::FromAttributes(-2.0f, 1.0f, 1.0f, 0.5f);
	EXPECT_FLOAT_EQ(shown.morph.evilGood, -1.0f);
	EXPECT_FLOAT_EQ(shown.morph.thinFat, 1.0f);
	EXPECT_FLOAT_EQ(shown.morph.weakStrong, expected.weakStrong);
	EXPECT_EQ(shown.firstMarks, physique.listA);
	EXPECT_EQ(shown.secondMarks, physique.listB);
}

TEST(TattooMenu, RowZeroIsTheApe)
{
	creaturemind::PhysiqueFileData physique;
	physique.speciesRow = 0;
	EXPECT_EQ(FromPhysique(physique, HalfStrength).species, CreatureType::GiantApe);
}

// The editor's first frame draws the wait for the body's first idle action, 20 to 40 seconds, and its count starts
// again; the body never turns to the editor's eye, which only the Creature Cave's room does
// (docs/wiki/creature/tattoos.md#the-tattoo-editor-from-the-players-page)
TEST(TattooMenu, TheFirstFrameDrawsTheWaitForTheFirstIdleAction)
{
	Frames frames;
	frames.Step({.frameMs = 16});
	EXPECT_EQ(frames.draws, (std::vector<int32_t> {creature_tattoo_editor::k_CopyIdleWaitMs}));
	EXPECT_EQ(frames.life.idleMs, creature_tattoo_editor::k_CopyIdleWaitMs + 5000 - 16);
	EXPECT_EQ(frames.life.idles, 0);
	EXPECT_FALSE(frames.life.turnWanted);
	EXPECT_FALSE(frames.lookAt.has_value());
}

// Once the wait has run out by the frames' milliseconds, a draw of 3 picks the action (a scratch here, which draws 2
// for its flag) and the next wait is drawn
TEST(TattooMenu, WhenTheWaitRunsOutAnIdleActionIsPickedAndTheNextWaitDrawn)
{
	Frames frames;
	frames.answer = 1;
	frames.Step({.frameMs = 16});
	frames.draws.clear();
	frames.Step({.frameMs = frames.life.idleMs});
	EXPECT_EQ(frames.draws, (std::vector<int32_t> {3, 2, creature_tattoo_editor::k_CopyIdleWaitMs}));
	EXPECT_EQ(frames.life.idles, 1);
	EXPECT_EQ(frames.life.idleMs, creature_tattoo_editor::k_CopyIdleWaitMs + 5000);
	EXPECT_FALSE(frames.life.turnWanted);
}

// While a button is held the body looks at the point under the pointer for the first second and a half of the
// system's time, then nowhere; let go, it looks nowhere
TEST(TattooMenu, TheBodyLooksAtThePointerForTheFirstSecondAndAHalfOfAPress)
{
	Frames frames;
	const glm::vec3 pointer {1.0f, 2.0f, 3.0f};
	frames.Step({.buttonHeld = true, .pointer = pointer, .ticks = 1000, .frameMs = 16});
	EXPECT_EQ(frames.lookAt, pointer);
	frames.Step({.buttonHeld = true, .pointer = pointer, .ticks = 2499, .frameMs = 16});
	EXPECT_EQ(frames.lookAt, pointer);
	frames.Step({.buttonHeld = true, .pointer = pointer, .ticks = 2500, .frameMs = 16});
	EXPECT_FALSE(frames.lookAt.has_value());
	frames.Step({.buttonHeld = false, .pointer = pointer, .ticks = 2600, .frameMs = 16});
	EXPECT_FALSE(frames.lookAt.has_value());
	EXPECT_EQ(frames.life.heldSince, 0u);
	EXPECT_FLOAT_EQ(k_LookDepth, 10.0f);
}

// The left button held draws the wait again and starts the count again
TEST(TattooMenu, TheLeftButtonDrawsTheWaitAgain)
{
	Frames frames;
	frames.Step({.frameMs = 16});
	frames.draws.clear();
	frames.Step({.buttonHeld = true, .leftHeld = true, .ticks = 1000, .frameMs = 16});
	EXPECT_EQ(frames.draws, (std::vector<int32_t> {creature_tattoo_editor::k_CopyIdleWaitMs}));
	EXPECT_EQ(frames.life.idleMs, creature_tattoo_editor::k_CopyIdleWaitMs + 5000);
	EXPECT_EQ(frames.life.idles, 0);
}

TEST(TattooMenu, TheReadoutNamesTheBodyAndItsWait)
{
	creature_tattoo_editor::CopyLife life;
	life.idleMs = 1234;
	life.idles = 2;
	const auto expected = "species " + std::to_string(static_cast<int>(CreatureType::GiantApe)) +
	                      ", evil-good 0.00, thin-fat 0.50, weak-strong 0.50, marks 0 and 0, next idle in 1234 ms, 2 idles";
	EXPECT_EQ(Readout(StandIn(), life), expected);
}

// The body is drawn first, its hair at the end of the body's own draw and its eyes after, as any creature's; the hair
// only where the body has hair and the hair is shown (raffclar's d9468815c, the eyes put after the hair as the original
// draws them)
TEST(TattooMenuHair, TheHairIsDrawnAfterTheBodyAndBeforeTheEyes)
{
	EXPECT_EQ(DrawOrder(true, true, true), (std::vector<Layer> {Layer::Body, Layer::Hair, Layer::Eyes}));
	EXPECT_EQ(DrawOrder(true, false, true), (std::vector<Layer> {Layer::Body, Layer::Eyes}));
	EXPECT_EQ(DrawOrder(false, true, true), (std::vector<Layer> {Layer::Body, Layer::Eyes}));
	EXPECT_EQ(DrawOrder(true, true, false), (std::vector<Layer> {Layer::Body, Layer::Hair}));
}

TEST(TattooMenu, ButtonNeedsFileShownCreatureAndNoRestart)
{
	EXPECT_TRUE(ButtonShown(true, true, false));
	EXPECT_FALSE(ButtonShown(false, true, false));
	EXPECT_FALSE(ButtonShown(true, false, false));
	EXPECT_FALSE(ButtonShown(true, true, true));
}
