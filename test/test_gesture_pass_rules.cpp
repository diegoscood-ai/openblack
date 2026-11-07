/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rules of the interface's gesture pass (src/Magic/Gestures/PowerUpSystem.h) and of the miracle in the hand
// (src/Magic/Core/SpellSeed.h), against the wiki (docs/wiki/magic/gestures.md#what-is-looked-for-and-when,
// docs/wiki/magic/spell-core.md): the circle that sizes a seed, the circle handed to the cast, the help events the
// gestures raise, the selection's categories and its icons, the leash picker's tables and the seed's delay in the hand.
// Fakes and plain values only; the locator is only used to inject the tables.

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/SpellSeed.h"
#include "Enums.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/SpellSeed.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic::gestures;
using openblack::help_profile::Event;

namespace
{
constexpr Gesture k_ForkRight = 9;
constexpr Gesture k_Heart = 13;
constexpr Gesture k_CyrillicL = 16;
/// Two of the player's miracles, called up with the spiral, and one of the creature's, with the inverse spiral
constexpr int k_Shield = 4;
constexpr int k_Water = 10;
constexpr int k_CreatureSeed = 22;

/// The worship icons: the seeds whose icons can be requested, walked in that order
struct FakeIcons final: IconProvider
{
	std::vector<int> requestable;
	std::set<int> valid;
	int requested {-1};
	std::function<Gesture(int)> selectionOf;

	[[nodiscard]] bool AnyRequestableIconOfCategory(Gesture category) const override
	{
		return std::ranges::any_of(requestable, [&](int seed) { return selectionOf(seed) == category; });
	}
	void ForEachRequestableIcon(Gesture category, const std::function<void(int seedType)>& visit) const override
	{
		for (const int seed : requestable)
		{
			if (selectionOf(seed) == category)
			{
				visit(seed);
			}
		}
	}
	[[nodiscard]] bool IconValidForRequest(int seedType) const override { return valid.contains(seedType); }
	void RequestSpell(int seedType) override { requested = seedType; }
};

/// The seed info's selection gestures and gestures of the three seeds
SelectionTables Tables()
{
	SelectionTables tables;
	tables.selectionGesture = [](int seed) {
		return seed == k_CreatureSeed ? k_InverseSpiral : (seed == k_Shield || seed == k_Water ? k_Spiral : k_None);
	};
	tables.gesture = [](int seed) {
		return seed == k_Shield ? k_ForkRight : (seed == k_Water ? k_Heart : (seed == k_CreatureSeed ? k_CyrillicL : k_None));
	};
	tables.gestureStage2 = [](int /*seed*/) { return k_None; };
	return tables;
}

FakeIcons AllIcons()
{
	FakeIcons icons;
	icons.requestable = {k_Shield, k_Water, k_CreatureSeed};
	icons.valid = {k_Shield, k_Water, k_CreatureSeed};
	icons.selectionOf = Tables().selectionGesture;
	return icons;
}

/// A recognise that answers yes only for the gesture given
std::function<bool(Gesture)> Drawn(Gesture drawn)
{
	return [drawn](Gesture g) { return g == drawn; };
}
} // namespace

TEST(CircleGesture, TheCircleIsLookedForWhileASizedSeedIsHeldWithTheActionPressed)
{
	ecs::components::SpellSeed seed;
	int asked = 0;
	const auto circleSized = [&asked](const ecs::components::SpellSeed& /*held*/) {
		++asked;
		return k_Circle;
	};
	EXPECT_EQ(CircleLookedFor(true, &seed, false, circleSized), k_Circle);
	// not without the action press, a seed in the hand, or with a circle remembered: then the seed is not even asked
	asked = 0;
	EXPECT_EQ(CircleLookedFor(false, &seed, false, circleSized), k_None);
	EXPECT_EQ(CircleLookedFor(true, nullptr, false, circleSized), k_None);
	EXPECT_EQ(CircleLookedFor(true, &seed, true, circleSized), k_None);
	EXPECT_EQ(asked, 0);
	// a seed no gesture sizes waits for none
	EXPECT_EQ(CircleLookedFor(true, &seed, false, [](const ecs::components::SpellSeed& /*held*/) { return k_None; }), k_None);
}

TEST(CircleGesture, TheCircleIsLookedForWhetherOrNotTheSeedIsReady)
{
	const auto circleSized = [](const ecs::components::SpellSeed& /*held*/) { return k_Circle; };
	ecs::components::SpellSeed seed;
	seed.ready = false;
	EXPECT_EQ(CircleLookedFor(true, &seed, false, circleSized), k_Circle);
	seed.ready = true;
	EXPECT_EQ(CircleLookedFor(true, &seed, false, circleSized), k_Circle);
}

TEST(CircleGesture, TheCastTakesTheRememberedCirclesMiddleAndSize)
{
	InterfaceGestures state;
	state.circlePending = true;
	state.circlePosition = {1250.5f, 12.0f, 980.25f};
	state.circleSize = 42.5f;
	state.circleGesture = k_Circle;
	ASSERT_TRUE(TakeCircle(state));
	EXPECT_EQ(state.gesture.position, glm::vec3(1250.5f, 12.0f, 980.25f));
	EXPECT_EQ(state.gesture.size, 42.5f);
	EXPECT_EQ(state.gesture.gesture, k_Circle);

	// with no circle remembered the cast's packet has no gesture, and its point and size stay
	InterfaceGestures none;
	none.gesture = {.gesture = k_Spiral, .position = {1.0f, 2.0f, 3.0f}, .size = 7.0f};
	none.circleSize = 42.5f;
	EXPECT_FALSE(TakeCircle(none));
	EXPECT_EQ(none.gesture.gesture, k_None);
	EXPECT_EQ(none.gesture.position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(none.gesture.size, 7.0f);
}

TEST(PowerUpStage, EachOutcomeRaisesItsHelpEvent)
{
	EXPECT_EQ(PowerUpStageHelp(PowerUpStageOutcome::Kind::PoweredUp).value_or(Event::Dummy), Event::PowerUpGesture);
	EXPECT_EQ(PowerUpStageHelp(PowerUpStageOutcome::Kind::PoweredDown).value_or(Event::Dummy), Event::GestureCancelPowerUp);
	EXPECT_EQ(PowerUpStageHelp(PowerUpStageOutcome::Kind::ShakenOut).value_or(Event::Dummy), Event::GestureCancelHeld);
	EXPECT_FALSE(PowerUpStageHelp(PowerUpStageOutcome::Kind::None).has_value());
	// the help profile's numbers: a power-up asked for 18, called off 20, a seed shaken out 22
	EXPECT_EQ(static_cast<int>(Event::PowerUpGesture), 18);
	EXPECT_EQ(static_cast<int>(Event::GestureCancelPowerUp), 20);
	EXPECT_EQ(static_cast<int>(Event::GestureCancelHeld), 22);
}

TEST(Selection, OpeningAndEachStageRaiseTheirHelpEvents)
{
	// the tutorial counts the selection opened (15), a stage drawn (16) and a miracle called up (17)
	EXPECT_EQ(static_cast<int>(k_SelectionOpenedHelp), 15);
	EXPECT_EQ(SelectionHelp(Selection::Outcome::StageOk), std::vector<Event> {Event::StageGesture});
	EXPECT_EQ(SelectionHelp(Selection::Outcome::Requested), (std::vector<Event> {Event::StageGesture, Event::GetSpellGesture}));
	EXPECT_EQ(static_cast<int>(Event::StageGesture), 16);
	EXPECT_EQ(static_cast<int>(Event::GetSpellGesture), 17);
	// a scribble closing it (21), and nothing for nothing drawn
	EXPECT_EQ(SelectionHelp(Selection::Outcome::Cancelled), std::vector<Event> {Event::GestureCancelSelect});
	EXPECT_EQ(static_cast<int>(Event::GestureCancelSelect), 21);
	EXPECT_TRUE(SelectionHelp(Selection::Outcome::None).empty());
}

TEST(Selection, TheInverseSpiralOpensItForTheCreaturesMiracles)
{
	const auto icons = AllIcons();
	Selection selection;
	ASSERT_TRUE(selection.Open(k_InverseSpiral, icons, Tables()));
	EXPECT_TRUE(selection.open);
	EXPECT_EQ(selection.category, k_InverseSpiral);
	EXPECT_TRUE(selection.candidate[k_CreatureSeed]);
	EXPECT_FALSE(selection.candidate[k_Shield]);
	EXPECT_FALSE(selection.candidate[k_Water]);
	EXPECT_TRUE(selection.activeGesture[k_CyrillicL]);
	EXPECT_FALSE(selection.activeGesture[k_ForkRight]);
	EXPECT_FALSE(selection.activeGesture[k_Heart]);
	// the spiral, the player's own
	ASSERT_TRUE(selection.Open(k_Spiral, icons, Tables()));
	EXPECT_EQ(selection.category, k_Spiral);
	EXPECT_FALSE(selection.candidate[k_CreatureSeed]);
	EXPECT_TRUE(selection.candidate[k_Shield]);
	EXPECT_TRUE(selection.candidate[k_Water]);
	EXPECT_FALSE(selection.activeGesture[k_CyrillicL]);
}

TEST(Selection, ItOffersOnlyTheMiraclesWhoseIconsCanBeRequested)
{
	// the water's icon cannot be requested: it is not offered
	auto icons = AllIcons();
	icons.requestable = {k_Shield, k_CreatureSeed};
	Selection selection;
	ASSERT_TRUE(selection.Open(k_Spiral, icons, Tables()));
	EXPECT_TRUE(selection.candidate[k_Shield]);
	EXPECT_FALSE(selection.candidate[k_Water]);
	EXPECT_FALSE(selection.activeGesture[k_Heart]);
	// with none requestable it does not open
	icons.requestable.clear();
	Selection closed;
	EXPECT_FALSE(closed.Open(k_Spiral, icons, Tables()));
	EXPECT_FALSE(closed.open);

	// an icon that can no longer supply its miracle when the gesture is drawn is not asked, and the selection closes
	icons = AllIcons();
	ASSERT_TRUE(selection.Open(k_Spiral, icons, Tables()));
	icons.valid.erase(k_Shield);
	EXPECT_EQ(selection.Stage(0.1f, 30.0f, Drawn(k_ForkRight), icons), Selection::Outcome::None);
	EXPECT_EQ(icons.requested, -1);
	EXPECT_FALSE(selection.candidate[k_Shield]);
	// one that can is asked for its miracle
	icons.valid.insert(k_Shield);
	ASSERT_TRUE(selection.Open(k_Spiral, icons, Tables()));
	EXPECT_EQ(selection.Stage(0.1f, 30.0f, Drawn(k_ForkRight), icons), Selection::Outcome::Requested);
	EXPECT_EQ(icons.requested, k_Shield);
}

TEST(LeashPicker, TheTablesGiveTheSquareSpiralTheTimeOutAndTheLeashesGestures)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	// info.dat's values (the wiki): the square spiral opens the picker, 25 s, and by leash type {E, vertical scribble,
	// E, heart}: the evil leash the vertical scribble, the rope the E and the good leash the heart
	auto info = std::make_unique<InfoConstants>();
	info->spellSystem.leashSelectionSystemTimeOut = 25.0f;
	info->spellSystem.leashSelectionStart = GestureType::SquareSpirial;
	info->spellSystem.leashSelectionGestures = {GestureType::EShape, GestureType::VerticalScribble, GestureType::EShape,
	                                            GestureType::Heart};
	Locator::infoConstants::reset(info.release());
	const auto tables = LeashSelectionTables::FromInfo();
	EXPECT_EQ(tables.start, Gesture(15));
	EXPECT_EQ(tables.timeOut, 25.0f);
	EXPECT_EQ(tables.pickGestures, (std::array<Gesture, 4> {17, 7, 17, 13}));
	EXPECT_EQ(tables.pickGestures[static_cast<size_t>(LeashType::Evil)], Gesture(7));
	EXPECT_EQ(tables.pickGestures[static_cast<size_t>(LeashType::Rope)], Gesture(17));
	EXPECT_EQ(tables.pickGestures[static_cast<size_t>(LeashType::Good)], Gesture(13));

	// without the tables nothing opens the picker
	Locator::infoConstants::reset();
	EXPECT_EQ(LeashSelectionTables::FromInfo().start, k_None);
}

TEST(SeedInHand, ASeedBecomesReadyAfterOneAndAHalfSecondsInTheHand)
{
	EXPECT_EQ(game_clock::k_MsPerTurn, 100u);
	EXPECT_FALSE(magic::seed::ReadyAfterTurnsInHand(0, 1.5f));
	EXPECT_FALSE(magic::seed::ReadyAfterTurnsInHand(14, 1.5f));
	// 15 turns of 100 ms come to just over 1.5 s in single precision, as in the original
	EXPECT_TRUE(magic::seed::ReadyAfterTurnsInHand(15, 1.5f));
	EXPECT_TRUE(magic::seed::ReadyAfterTurnsInHand(16, 1.5f));
	// a longer delay waits longer
	EXPECT_FALSE(magic::seed::ReadyAfterTurnsInHand(15, 2.0f));
	EXPECT_TRUE(magic::seed::ReadyAfterTurnsInHand(21, 2.0f));
}
