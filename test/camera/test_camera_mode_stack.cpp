/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Camera/CameraModeStack.h"

using namespace openblack::camera_stack;

namespace
{
constexpr auto k_A = static_cast<entt::entity>(11);
constexpr auto k_B = static_cast<entt::entity>(12);
constexpr auto k_C = static_cast<entt::entity>(13);
} // namespace

TEST(ScriptCameraModeStack, EachNewModeGoesOnTopAndOnlyTheTopIsCurrent)
{
	ModeStack stack;
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.Current(), nullptr);
	EXPECT_TRUE(stack.CanLeaveCurrent()); // the player's mode can always be left
	EXPECT_FALSE(stack.PopDual());
	EXPECT_EQ(stack.PopScript(), ModeStack::Popped::None);
	EXPECT_EQ(stack.EndScript(), 0u);
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}});
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_TRUE(stack.CanLeaveCurrent()); // a dual camera can be left
	EXPECT_FALSE(stack.ScriptOnStack());
	stack.Push({.kind = StackedMode::Kind::Script});
	EXPECT_TRUE(stack.ScriptCurrent());
	EXPECT_FALSE(stack.CanLeaveCurrent()); // a living script mode cannot
	EXPECT_EQ(stack.CurrentDual(), nullptr);
	EXPECT_FALSE(stack.PopDual()); // only a current dual camera is released
	EXPECT_EQ(stack.Size(), 2u);
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_B, .b = k_C}});
	ASSERT_NE(stack.CurrentDual(), nullptr);
	EXPECT_EQ(stack.CurrentDual()->b, k_C);
	EXPECT_TRUE(stack.ScriptOnStack());
	EXPECT_EQ(stack.EndScript(), 0u); // the script mode is not the current one
	EXPECT_TRUE(stack.PopDual());     // back to the script mode under it
	EXPECT_TRUE(stack.ScriptCurrent());
	EXPECT_NE(stack.EndScript(), 0u); // and a new player's mode goes over the dual camera under the script mode
	// the script mode, no longer valid, keeps its place until the turn's check
	ASSERT_EQ(stack.Size(), 3u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_TRUE(stack.Modes()[1].dead);
	EXPECT_EQ(stack.Modes().front().kind, StackedMode::Kind::Dual);
}

TEST(ScriptCameraModeStack, TheTurnsCheckDeletesInvalidDualCamerasWhereverTheyAre)
{
	ModeStack stack;
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}});
	stack.Push({.kind = StackedMode::Kind::Script});
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_B, .b = k_C}});
	const auto withoutA = [](const DualMode& dual) { return dual.a != k_A && dual.b != k_A; };
	EXPECT_FALSE(stack.RemoveInvalid(withoutA)); // the bottom one goes; the current one stays
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_EQ(stack.Modes().front().kind, StackedMode::Kind::Script);
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_TRUE(stack.RemoveInvalid([](const DualMode&) { return false; })); // the current one goes
	EXPECT_TRUE(stack.ScriptCurrent());                                      // the script mode is never removed by the check
	EXPECT_EQ(stack.Size(), 1u);
}

namespace
{
StackedMode NumberedDual(int number)
{
	return {.kind = StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B, .point = {static_cast<float>(number), 0.0f, 0.0f}}};
}
} // namespace

TEST(ScriptCameraModeStack, TwelveModesFitWithThePlayersModeCounted)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	EXPECT_EQ(stack.Places(), 1u); // the player's own mode
	for (int i = 0; i < 11; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i)), Dropped::None);
	}
	EXPECT_EQ(stack.Places(), ModeStack::k_Places);
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Size(), 11u);
}

TEST(ScriptCameraModeStack, TheThirteenthModeDeletesTheBottomOne)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	// The player's own mode goes first, then the lowest of the stacked modes, the rest moving down a place
	EXPECT_EQ(stack.Push(NumberedDual(11)), Dropped::PlayersMode);
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Size(), 12u);
	EXPECT_EQ(stack.Places(), 12u);
	EXPECT_EQ(stack.Push(NumberedDual(12)), Dropped::Own);
	ASSERT_EQ(stack.Size(), 12u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.CurrentDual()->point.x, 12.0f);
	// Pops then go down to the bottom mode, and an emptied stack takes a new player's mode, once
	for (int i = 12; i > 0; --i)
	{
		ASSERT_NE(stack.CurrentDual(), nullptr);
		EXPECT_EQ(stack.CurrentDual()->point.x, static_cast<float>(i));
		EXPECT_FALSE(stack.NewPlayersModeIfGone());
		EXPECT_TRUE(stack.PopDual());
	}
	EXPECT_TRUE(stack.Empty());
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_TRUE(stack.NewPlayersModeIfGone());
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_FALSE(stack.NewPlayersModeIfGone());
	EXPECT_EQ(stack.Places(), 1u);
}

TEST(ScriptCameraModeStack, TheOtherSystemsModesTakePlacesUnderTheseOnes)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	// Creature Mode's follow and a placed path under them: nine dual cameras fill the stack
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), 2), Dropped::None);
	}
	EXPECT_EQ(stack.Push(NumberedDual(9), 2), Dropped::PlayersMode);
	// The lowest is then one of theirs, which this module does not delete
	EXPECT_EQ(stack.Push(NumberedDual(10), 2), Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.Size(), 11u);
	// which their own systems are to delete
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
	// Nor does a new player's mode come while they are on the stack
	stack.Clear();
	EXPECT_FALSE(stack.PlayersModeGone());
}

// A script mode over a follow: the follow keeps its own place under it, its entry counted among the others
TEST(ScriptCameraModeStack, AScriptModeOverAFollowLeavesTheFollowItsOwnPlaceUnderIt)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	stack.PushFollow(7);
	EXPECT_EQ(stack.Push({.kind = StackedMode::Kind::Script}, stack.OtherCount()), Dropped::None);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), 3u);
	EXPECT_TRUE(stack.FollowOnStack());
	EXPECT_FALSE(stack.FollowCurrent());
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), stack.OtherCount()), Dropped::None);
	}
	EXPECT_EQ(stack.Push(NumberedDual(9), stack.OtherCount()), Dropped::PlayersMode);
	// The follow under the script mode is the bottom then: its own system deletes it, and the script mode stays
	EXPECT_EQ(stack.Push(NumberedDual(10), stack.OtherCount()), Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
	EXPECT_FALSE(stack.RemoveFollow(7));
	EXPECT_TRUE(stack.ScriptOnStack());
	EXPECT_EQ(stack.Push(NumberedDual(11), stack.OtherCount()), Dropped::Own);
	EXPECT_FALSE(stack.ScriptOnStack());
}

// A script's end leaves its mode no longer valid in its places under the new player's mode, counted for the stack's 12,
// until the turn's check deletes it
TEST(ScriptCameraModeStack, AnEndedScriptModeKeepsItsPlaceUntilTheTurnsCheck)
{
	ModeStack stack;
	stack.Push({.kind = StackedMode::Kind::Script});
	EXPECT_FALSE(stack.CanLeaveCurrent());
	ASSERT_NE(stack.EndScript(true), 0u);
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_TRUE(stack.Modes().front().dead);
	EXPECT_EQ(stack.Places(), 3u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_FALSE(stack.ScriptOnStack());
	// the check deletes it, the current new player's mode staying
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return true; }));
	ASSERT_EQ(stack.Size(), 1u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_EQ(stack.Places(), 2u);
}

TEST(ScriptCameraModeStack, AScriptsEndPutsANewPlayersModeOverTheModesUnderIt)
{
	ModeStack stack;
	stack.Push(NumberedDual(0));
	stack.Push({.kind = StackedMode::Kind::Script});
	const auto id = stack.EndScript(true);
	ASSERT_NE(id, 0u);
	// The script mode, no longer valid, keeps its place until the turn's check deletes it
	ASSERT_EQ(stack.Size(), 3u);
	EXPECT_EQ(stack.Places(), 4u);
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return true; }));
	// The dual camera under it stays, and the player's mode it went over is kept for it
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_TRUE(stack.PlayersModeCurrent());
	EXPECT_FALSE(stack.ScriptOnStack());
	EXPECT_TRUE(stack.KeepsModel(id));
	EXPECT_EQ(stack.Places(), 3u); // the bottom player's mode, the dual camera and the new one
	// Popped, it gives the kept model back, and the dual camera is current again
	const auto popped = stack.PopPlayersMode();
	ASSERT_TRUE(popped.has_value());
	EXPECT_EQ(popped->id, id);
	EXPECT_TRUE(popped->keptUnder);
	EXPECT_FALSE(stack.KeepsModel(id));
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_FALSE(stack.PopPlayersMode().has_value()); // a dual camera is not the player's mode
}

TEST(ScriptCameraModeStack, EachScriptsEndPutsAnotherPlayersModeOnTop)
{
	ModeStack stack;
	stack.Push({.kind = StackedMode::Kind::Script});
	const auto first = stack.EndScript(true);
	stack.Push({.kind = StackedMode::Kind::Script});
	const auto second = stack.EndScript(true);
	ASSERT_NE(first, second);
	// both script modes keep their places until the turn's check
	EXPECT_EQ(stack.Size(), 4u);
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return true; }));
	EXPECT_EQ(stack.Size(), 2u);
	EXPECT_EQ(stack.Places(), 3u);
	// Popped one by one down to the bottom one, which leaves nothing under it
	EXPECT_EQ(stack.PopPlayersMode()->id, second);
	EXPECT_EQ(stack.PopPlayersMode()->id, first);
	const auto bottom = stack.PopPlayersMode();
	ASSERT_TRUE(bottom.has_value());
	EXPECT_EQ(bottom->id, 0u);
	EXPECT_FALSE(bottom->keptUnder);
	EXPECT_TRUE(stack.Empty());
}

TEST(ScriptCameraModeStack, AScriptsEndOverAGoneBottomPlayersModeKeepsNothing)
{
	ModeStack stack;
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	stack.Push({.kind = StackedMode::Kind::Script});
	ASSERT_TRUE(stack.PlayersModeGone());
	// The new player's mode goes on top while the deleted script mode still has its place: the lowest dual camera goes
	const auto id = stack.EndScript(true);
	EXPECT_FALSE(stack.KeepsModel(id));
	EXPECT_TRUE(stack.PlayersModeGone());
	ASSERT_EQ(stack.Size(), 12u); // ten dual cameras, the script mode no longer valid and the new player's mode
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_TRUE(stack.PlayerCurrent());
}

TEST(ScriptCameraModeStack, AFullStackDeletingTheBottomPlayersModeLosesItsKeptModel)
{
	ModeStack stack;
	const auto id = stack.PushPlayer(true);
	ASSERT_TRUE(stack.KeepsModel(id));
	for (int i = 0; i < 10; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	EXPECT_EQ(stack.Places(), ModeStack::k_Places);
	EXPECT_TRUE(stack.KeepsModel(id));
	EXPECT_EQ(stack.Push(NumberedDual(10)), ModeStack::Dropped::PlayersMode);
	EXPECT_FALSE(stack.KeepsModel(id)); // the bottom one it kept is gone
	// The new player's mode is the lowest then, and goes next: the one put over the rest keeps nothing of it either
	const auto over = stack.PushPlayer(true);
	EXPECT_FALSE(stack.KeepsModel(over));
	EXPECT_EQ(stack.Modes().front().kind, StackedMode::Kind::Dual);
}

TEST(ScriptCameraModeStack, AScriptModeUnderANewPlayersModeIsNoLongerTheScriptsCamera)
{
	ModeStack stack;
	stack.Push({.kind = StackedMode::Kind::Script});
	stack.Push(NumberedDual(0));
	stack.Push({.kind = StackedMode::Kind::Script});
	EXPECT_NE(stack.EndScript(), 0u);
	// The first script mode stays under the new player's mode, which a script's camera can be started over; the ended
	// one keeps its place until the turn's check
	ASSERT_EQ(stack.Size(), 4u);
	EXPECT_EQ(stack.Modes().front().kind, StackedMode::Kind::Script);
	EXPECT_FALSE(stack.ScriptOnStack());
	EXPECT_TRUE(stack.CanLeaveCurrent());
	stack.Push({.kind = StackedMode::Kind::Script});
	EXPECT_TRUE(stack.ScriptOnStack());
}

TEST(ScriptCameraModeStack, TheLastModePoppedOffWithThePlayersModeGoneMakesANewOne)
{
	using Popped = ModeStack::Popped;
	// The creature interaction's camera at the bottom, then dual cameras over it until the player's own mode is deleted
	ModeStack stack;
	stack.Push({.kind = StackedMode::Kind::Interaction});
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	// Its pop empties the stack: a new player's mode is made, the bottom from then on
	EXPECT_EQ(stack.PopInteraction(), Popped::NewPlayersMode);
	EXPECT_TRUE(stack.Empty());
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Places(), 1u);
	EXPECT_EQ(stack.PopInteraction(), Popped::None);
	// The script mode the same
	stack.Push({.kind = StackedMode::Kind::Script});
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	EXPECT_EQ(stack.PopScript(), Popped::NewPlayersMode);
	EXPECT_FALSE(stack.PlayersModeGone());
}

TEST(ScriptCameraModeStack, APopOverAModeLeftUnderItMakesNoNewPlayersMode)
{
	using Popped = ModeStack::Popped;
	ModeStack stack;
	// With the player's own mode there, the pop leaves it current
	stack.Push({.kind = StackedMode::Kind::Interaction});
	EXPECT_EQ(stack.PopInteraction(), Popped::ModeUnder);
	EXPECT_FALSE(stack.PlayersModeGone());
	// With it gone, a mode of this module's or another system's (here a placed path) left under it is current again
	stack.Push(NumberedDual(0), 1);
	stack.Push({.kind = StackedMode::Kind::Script}, 1);
	for (int i = 1; i < 10; ++i)
	{
		stack.Push(NumberedDual(i), 1);
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 1; i < 10; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	EXPECT_EQ(stack.PopScript(1), Popped::ModeUnder);
	EXPECT_TRUE(stack.PlayersModeGone());
	ASSERT_TRUE(stack.PopDual());
	stack.Push({.kind = StackedMode::Kind::Interaction}, 1);
	EXPECT_EQ(stack.PopInteraction(1), Popped::ModeUnder);
	EXPECT_TRUE(stack.PlayersModeGone());
}

// The other systems' entries, the placed paths and Creature Mode's follows, kept in their push order, the first the
// lowest: the one a full stack deletes first. A new follow is a new entry on top, the one before it staying where it is
TEST(CameraModeStackEntries, TheOtherSystemsEntriesKeepTheirPushOrderTheFirstTheLowest)
{
	ModeStack stack;
	EXPECT_EQ(stack.OtherCount(), 0u);
	EXPECT_FALSE(stack.LowestOther().has_value());
	stack.PushPath(1);
	stack.PushFollow(1);
	stack.PushPath(3);
	EXPECT_EQ(stack.OtherCount(), 3u);
	EXPECT_TRUE(stack.FollowOnStack());
	ASSERT_TRUE(stack.LowestOther().has_value());
	EXPECT_EQ(stack.LowestOther()->kind, StackedMode::Kind::Path);
	EXPECT_EQ(stack.LowestOther()->path, 1u);
	stack.RemovePath(1);
	EXPECT_EQ(stack.LowestOther()->kind, StackedMode::Kind::Follow);
	// another follow: a new entry on top, the first one still the lowest
	stack.PushFollow(2);
	EXPECT_EQ(stack.OtherCount(), 3u);
	EXPECT_EQ(stack.LowestOther()->follow, 1u);
	EXPECT_EQ(stack.OtherEntries().back().follow, 2u);
	EXPECT_EQ(stack.CurrentFollow(), 2u);
	stack.RemovePath(42);                // no entry: nothing
	EXPECT_TRUE(stack.RemoveFollow(2));  // the current mode
	EXPECT_FALSE(stack.RemoveFollow(1)); // under the path
	EXPECT_FALSE(stack.FollowOnStack());
	EXPECT_EQ(stack.OtherCount(), 1u);
	EXPECT_FALSE(stack.RemoveFollow(1)); // no entry: nothing
	EXPECT_EQ(stack.OtherCount(), 1u);
}

// A new player's mode goes over every placed path not under one already, and its pop gives back only those it went over
TEST(CameraModeStackEntries, ANewPlayersModeGoesOverThePathsNotUnderOneAlready)
{
	ModeStack stack;
	EXPECT_FALSE(stack.CoverPaths(5)); // none placed
	stack.PushPath(1);
	EXPECT_TRUE(stack.CoverPaths(5));
	stack.PushPath(2);
	EXPECT_EQ(stack.PathUnder(1), 5u);
	EXPECT_EQ(stack.PathUnder(2), 0u);
	EXPECT_TRUE(stack.CoverPaths(6));
	EXPECT_EQ(stack.PathUnder(1), 5u);
	EXPECT_EQ(stack.PathUnder(2), 6u);
	EXPECT_FALSE(stack.CoverPaths(7)); // every one is under a mode already
	stack.UncoverPaths(0);
	EXPECT_EQ(stack.PathUnder(1), 5u);
	stack.UncoverPaths(5);
	EXPECT_EQ(stack.PathUnder(1), 0u);
	EXPECT_EQ(stack.PathUnder(2), 6u);
	EXPECT_EQ(stack.PathUnder(42), 0u); // no entry
	// the follow's entry is under none of them
	stack.PushFollow(1);
	EXPECT_TRUE(stack.CoverPaths(8));
	EXPECT_EQ(stack.OtherEntries().back().under, 0u);
}

// The other entries are not among the script camera's modes, and the script camera's clear leaves them to their own
// systems
TEST(CameraModeStackEntries, TheScriptCamerasClearLeavesTheOtherEntriesToTheirSystems)
{
	ModeStack stack;
	stack.PushPath(1);
	stack.PushFollow(1);
	EXPECT_TRUE(stack.Empty());
	EXPECT_TRUE(stack.PlayersModeCurrent());
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}}, stack.OtherCount());
	EXPECT_EQ(stack.Size(), 1u);
	stack.Clear();
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.OtherCount(), 2u);
}
// The stack's one push order, whatever the kind (docs/wiki/camera/game-camera.md#openblack): a mode pushed later is over
// every mode pushed before it, and only the top is current

// A placed path started over a current dual camera is the current mode: the dual camera's release finds no current dual
// camera and pops nothing, and a full stack deletes the bottom mode, the dual camera pushed before the path
TEST(CameraModeStackPushOrder, APathStartedOverADualCameraIsTheCurrentModeAndTheDualCameraTheBottom)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	stack.Push(NumberedDual(0));
	stack.PushPath(1);
	EXPECT_TRUE(stack.OtherCurrent());
	EXPECT_FALSE(stack.DualCurrent());
	EXPECT_EQ(stack.CurrentDual(), nullptr);
	EXPECT_EQ(stack.Current(), nullptr);
	EXPECT_TRUE(stack.CanLeaveCurrent());
	// the camera's own zoomers are the path's: a mode pushed over it goes on from them
	EXPECT_TRUE(stack.OverAnotherCamera());
	EXPECT_FALSE(stack.PopDual());
	ASSERT_EQ(stack.Size(), 1u);
	// the turn's check deleting the dual camera under it does not delete the current mode
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return false; }));
	EXPECT_TRUE(stack.Empty());
	EXPECT_TRUE(stack.RemovePath(1)); // the current mode
	// a dual camera, then a path over it
	stack.Push(NumberedDual(0), stack.OtherCount());
	stack.PushPath(2);
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return true; }));
	// dual cameras over them fill the stack: the player's own mode goes, then the dual camera pushed before the path
	for (int i = 1; i < 10; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), stack.OtherCount()), Dropped::None);
	}
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_EQ(stack.Push(NumberedDual(10), stack.OtherCount()), Dropped::PlayersMode);
	EXPECT_EQ(stack.Push(NumberedDual(11), stack.OtherCount()), Dropped::Own);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
	ASSERT_EQ(stack.Size(), 11u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.OtherCount(), 1u);
	// and the next push deletes the path, now the bottom
	EXPECT_EQ(stack.Push(NumberedDual(12), stack.OtherCount()), Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
}

// The turn's validity check over a dual camera pushed over a path, both no longer valid: the original's pass from the
// bottom of the stack deletes the path first, not current, so not cleaned up, and the dual camera last, the current
// mode. Within a validity pass only the mode current as it began answers "current", whatever went before it; outside
// one, the stack's live answer: the path, the top once the dual camera has gone
TEST(CameraModeStackPushOrder, TheValidityPassCleansUpOnlyTheModeCurrentAsItBegan)
{
	ModeStack stack;
	stack.PushPath(1);
	stack.Push(NumberedDual(0), stack.OtherCount());
	ASSERT_TRUE(stack.DualCurrent());
	stack.BeginValidityPass();
	EXPECT_TRUE(stack.RemoveInvalid([](const DualMode&) { return false; }));
	EXPECT_FALSE(stack.RemovePath(1));
	stack.EndValidityPass();
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.OtherCount(), 0u);

	// with no pass, the path is the current mode once the dual camera over it has gone
	stack.PushPath(2);
	stack.Push(NumberedDual(1), stack.OtherCount());
	EXPECT_TRUE(stack.RemoveInvalid([](const DualMode&) { return false; }));
	EXPECT_TRUE(stack.RemovePath(2));

	// a pass over a path over a dual camera: the path, pushed last, is the one current
	stack.Push(NumberedDual(2), stack.OtherCount());
	stack.PushPath(3);
	stack.BeginValidityPass();
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return false; }));
	EXPECT_TRUE(stack.RemovePath(3));
	stack.EndValidityPass();
}

// Creature Mode's follow started over a current dual camera is pushed over it, the current mode; the dual camera's
// release pops nothing; once the follow comes off the stack, the dual camera is current again
TEST(CameraModeStackPushOrder, AFollowStartedOverADualCameraIsTheCurrentMode)
{
	ModeStack stack;
	stack.Push(NumberedDual(0));
	ASSERT_TRUE(stack.DualCurrent());
	stack.PushFollow();
	EXPECT_TRUE(stack.FollowCurrent());
	EXPECT_FALSE(stack.DualCurrent());
	EXPECT_TRUE(stack.PlayersModeCurrent());
	EXPECT_FALSE(stack.PopDual());
	// a dual camera started over the follow is over it, as before
	stack.Push(NumberedDual(1), stack.OtherCount());
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_TRUE(stack.PopDual());
	EXPECT_TRUE(stack.FollowCurrent());
	// the follow taken off, current: the dual camera under it is current again
	EXPECT_TRUE(stack.RemoveFollow());
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_FALSE(stack.RemoveFollow());
}

// Creature Mode's follow started while a placed path has the camera is pushed over the path, the current mode; the path
// stays under it, and is current again once the follow comes off. A path started over the follow is over it
TEST(CameraModeStackPushOrder, AFollowStartedOverAPathIsTheCurrentModeWithThePathUnderIt)
{
	ModeStack stack;
	stack.PushPath(1);
	EXPECT_FALSE(stack.FollowOverPath(1));
	stack.PushFollow();
	EXPECT_TRUE(stack.FollowOverPath(1));
	EXPECT_TRUE(stack.FollowCurrent());
	EXPECT_FALSE(stack.FollowOverPath(42)); // no entry
	stack.PushPath(2);
	EXPECT_FALSE(stack.FollowOverPath(2));
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_TRUE(stack.RemovePath(2));  // the current mode
	EXPECT_FALSE(stack.RemovePath(1)); // under the follow
	EXPECT_TRUE(stack.FollowCurrent());
}

// A script mode over Creature Mode's follow (docs/wiki/camera/script-camera.md#pending): the follow is a mode of its own
// under it, current again once the script mode is popped
TEST(CameraModeStackFollows, AScriptModeOverAFollowLeavesItUnderAndItsPopMakesTheFollowCurrentAgain)
{
	ModeStack stack;
	stack.PushFollow(1);
	ASSERT_TRUE(stack.FollowCurrent());
	EXPECT_EQ(stack.Push({.kind = StackedMode::Kind::Script}, stack.OtherCount()), ModeStack::Dropped::None);
	EXPECT_TRUE(stack.ScriptCurrent());
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_TRUE(stack.FollowOnStack());
	EXPECT_EQ(stack.Places() + stack.OtherCount(), 3u); // the player's own, the follow and the script mode
	EXPECT_EQ(stack.PopScript(stack.OtherCount()), ModeStack::Popped::ModeUnder);
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.CurrentFollow(), 1u);
	EXPECT_TRUE(stack.PlayersModeCurrent()); // the follow moves the camera's own zoomers
}

// A script's end over a follow: the new player's mode goes over the ended script mode and the follow, the turn's check
// deletes only the ended script mode, and the pop of the new player's mode makes the follow current again
TEST(CameraModeStackFollows, AScriptsEndOverAFollowLeavesTheFollowUnderTheNewPlayersMode)
{
	ModeStack stack;
	stack.PushFollow(1);
	stack.Push({.kind = StackedMode::Kind::Script}, stack.OtherCount());
	// the camera's model is the follow's, so the new player's mode keeps none of its own
	const auto id = stack.EndScript(false, stack.OtherCount());
	ASSERT_NE(id, 0u);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), 4u); // the player's own, the follow, the ended script mode, the new one
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_FALSE(stack.RemoveInvalid([](const DualMode&) { return true; }));
	ASSERT_EQ(stack.Size(), 1u);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), 3u);
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_FALSE(stack.KeepsModel(id));
	const auto popped = stack.PopPlayersMode();
	ASSERT_TRUE(popped.has_value());
	EXPECT_EQ(popped->id, id);
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.CurrentFollow(), 1u);
}

// Every follow is a mode of its own (docs/wiki/camera/game-camera.md#openblack): a second follow goes over the first,
// which stays under it, and is current again once the modes over it come off; one taken off from under another leaves
// the current one as it is, and each counts for the stack's places
TEST(CameraModeStackFollows, EachFollowIsAModeOfItsOwnTheNewestOverTheOthers)
{
	ModeStack stack;
	stack.PushFollow(1);
	stack.Push(NumberedDual(0), stack.OtherCount());
	stack.PushFollow(2);
	EXPECT_EQ(stack.OtherCount(), 2u);
	EXPECT_EQ(stack.CurrentFollow(), 2u);
	EXPECT_FALSE(stack.DualCurrent());
	// the newest taken off: the dual camera between them is current again, the first follow under it
	EXPECT_TRUE(stack.RemoveFollow(2));
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_TRUE(stack.PopDual());
	EXPECT_EQ(stack.CurrentFollow(), 1u);
	// a follow of the same thing again is a new mode too, and the one under it, taken off, leaves it current
	stack.PushFollow(3);
	EXPECT_FALSE(stack.RemoveFollow(1));
	EXPECT_EQ(stack.CurrentFollow(), 3u);
	EXPECT_EQ(stack.OtherCount(), 1u);
	stack.PushFollow(4);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), 3u);
}

// A placed path or a follow pushed onto a full stack goes through the stack's one push
// (docs/wiki/camera/game-camera.md#openblack): the bottom mode is deleted first whatever its kind, the player's own mode,
// then the lowest of the other systems' entries (deleted by its own system), then the script camera's lowest mode; the
// stack never holds more than its 12 places
TEST(CameraModeStackFullPush, APathOrAFollowPushedOntoAFullStackDeletesTheBottomWhateverItIs)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	EXPECT_EQ(stack.PushFollow(1), Dropped::None);
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), stack.OtherCount()), Dropped::None);
	}
	ASSERT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
	EXPECT_EQ(stack.PushPath(5), Dropped::PlayersMode);
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
	// the follow, pushed before the dual cameras, is the bottom: its own system deletes it
	EXPECT_EQ(stack.PushPath(6), Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
	ASSERT_TRUE(stack.LowestOther().has_value());
	EXPECT_EQ(stack.LowestOther()->follow, 1u);
	EXPECT_FALSE(stack.RemoveFollow(1)); // not the current mode
	EXPECT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
	// the paths were pushed after the dual cameras: the lowest dual camera goes
	EXPECT_EQ(stack.PushFollow(2), Dropped::Own);
	ASSERT_EQ(stack.Size(), 9u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
	EXPECT_EQ(stack.CurrentFollow(), 2u);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
}

// The player's own mode deleted by a follow's push onto a full stack is no longer kept for the new player's mode over it,
// as with the script camera's own pushes
TEST(CameraModeStackFullPush, AFollowsPushDeletingThePlayersModeLeavesTheNewPlayersModeOverItNothingToKeep)
{
	using Dropped = ModeStack::Dropped;
	ModeStack stack;
	const auto id = stack.PushPlayer(true);
	ASSERT_TRUE(stack.KeepsModel(id));
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), stack.OtherCount()), Dropped::None);
	}
	EXPECT_EQ(stack.PushFollow(1), Dropped::PlayersMode);
	EXPECT_FALSE(stack.KeepsModel(id));
	EXPECT_TRUE(stack.HasPlayer(id));
	// the next push deletes that new player's mode, now the bottom
	EXPECT_EQ(stack.PushPath(1), Dropped::Own);
	EXPECT_FALSE(stack.HasPlayer(id));
	EXPECT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
}

// The dance camera: an entry of its own on top of the whole stack, the current mode, kept by its dance. Ended, it stays
// on the stack until the turn's check deletes it, as one whose dance went; the map's clear takes every one off
TEST(CameraModeStackDances, TheDanceCameraGoesOnTopAndTheTurnsCheckDeletesItOnceEndedOrItsDanceGone)
{
	ModeStack stack;
	EXPECT_FALSE(stack.CurrentDance().has_value());
	stack.Push({.kind = StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}});
	stack.PushFollow(1);
	EXPECT_EQ(stack.PushDance(k_C), ModeStack::Dropped::None);
	EXPECT_EQ(stack.CurrentDance(), k_C);
	EXPECT_FALSE(stack.FollowCurrent());
	EXPECT_FALSE(stack.DualCurrent());
	EXPECT_TRUE(stack.CanLeaveCurrent()); // a dance camera can be left
	// a follow pushed after it is the current mode over it
	stack.PushFollow(2);
	EXPECT_FALSE(stack.CurrentDance().has_value());
	EXPECT_TRUE(stack.RemoveFollow(2));
	EXPECT_EQ(stack.CurrentDance(), k_C);
	// ended: still the current mode until the turn's check, which deletes it
	stack.EndDance(k_C);
	EXPECT_EQ(stack.CurrentDance(), k_C);
	EXPECT_TRUE(stack.RemoveInvalidDances([](entt::entity /*dance*/) { return true; }));
	EXPECT_FALSE(stack.CurrentDance().has_value());
	EXPECT_EQ(stack.CurrentFollow(), 1u);
	// its dance gone, under a follow: deleted, the follow staying the current mode
	stack.PushDance(k_A);
	stack.PushFollow(3);
	EXPECT_FALSE(stack.RemoveInvalidDances([](entt::entity dance) { return dance != k_A; }));
	EXPECT_EQ(stack.OtherCount(), 2u);
	EXPECT_EQ(stack.CurrentFollow(), 3u);
	// one still valid stays
	stack.PushDance(k_B);
	EXPECT_FALSE(stack.RemoveInvalidDances([](entt::entity /*dance*/) { return true; }));
	EXPECT_EQ(stack.CurrentDance(), k_B);
	// the map's clear
	stack.ClearDances();
	EXPECT_FALSE(stack.CurrentDance().has_value());
	EXPECT_EQ(stack.OtherCount(), 2u);
	EXPECT_FALSE(stack.RemoveDance(k_B)); // no entry: nothing
}

// A dance camera pushed after a placed path is the current mode over it, as a follow is
TEST(CameraModeStackDances, ADanceCameraOverAPathTakesTheCameraFromIt)
{
	ModeStack stack;
	stack.PushPath(7);
	EXPECT_FALSE(stack.OverPath(7));
	stack.PushDance(k_A);
	EXPECT_TRUE(stack.OverPath(7));
	EXPECT_FALSE(stack.FollowOverPath(7));
	EXPECT_TRUE(stack.RemoveDance(k_A));
	EXPECT_FALSE(stack.OverPath(7));
	stack.PushFollow(1);
	EXPECT_TRUE(stack.OverPath(7));
	EXPECT_FALSE(stack.OverPath(8)); // no entry: nothing
}

// On a full stack the dance camera's push deletes the bottom mode first, whatever it is; a dance camera the lowest of
// the other entries is the next one a push deletes, as theirs are
TEST(CameraModeStackDances, ADanceCameraOnAFullStackDeletesTheBottomModeFirst)
{
	ModeStack stack;
	stack.PushDance(k_A);
	for (uint64_t i = 1; i + 1 < ModeStack::k_Places; ++i)
	{
		stack.PushFollow(i);
	}
	EXPECT_EQ(stack.Places() + stack.OtherCount(), ModeStack::k_Places);
	EXPECT_EQ(stack.PushDance(k_B), ModeStack::Dropped::PlayersMode); // the player's own mode, the bottom
	EXPECT_EQ(stack.PushFollow(20), ModeStack::Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
	ASSERT_TRUE(stack.LowestOther().has_value());
	EXPECT_EQ(stack.LowestOther()->kind, StackedMode::Kind::Dance);
	EXPECT_EQ(stack.LowestOther()->dance, k_A);
}
