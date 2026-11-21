/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The turn's one check of the camera's stacked modes: which restart follows it, from the current mode's kind before and
// after and what the owners deleted. Only the current mode going restarts anything, and only once, the new current one
// by its kind: a placed path its near plane, the player's mode its flight back, the rest nothing

#include <gtest/gtest.h>

#include "Camera/CameraChecks.h"

using namespace openblack::camera_checks;

TEST(CameraChecks, NothingRestartsWhileTheCurrentModeStays)
{
	EXPECT_EQ(RestartAfter(Top::Player, Top::Player, {}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Path, Top::Path, {}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Dual, {}, false), Restart::None);
	// A dual camera under the script mode deleted: the script mode stays current
	EXPECT_EQ(RestartAfter(Top::Script, Top::Script, {.script = false}, false), Restart::None);
	// A path under a new player's mode, or the follow under a path, goes quietly
	EXPECT_EQ(RestartAfter(Top::Player, Top::Player, {.path = true}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Path, Top::Path, {.follow = true}, false), Restart::None);
	// The player's mode and the creature interaction's camera never go at the check
	EXPECT_EQ(RestartAfter(Top::Interaction, Top::Interaction, {.script = true, .path = true, .follow = true}, false),
	          Restart::None);
}

TEST(CameraChecks, ADualCameraGoneOverAPathRestartsOnlyThePath)
{
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Path, {.script = true}, false), Restart::Path);
}

TEST(CameraChecks, APathGoneOverTheFollowRestartsNothing)
{
	EXPECT_EQ(RestartAfter(Top::Path, Top::Follow, {.path = true}, false), Restart::None);
}

TEST(CameraChecks, ADualCameraGoneOverAFollowAnotherDualCameraOrTheInteractionsCameraRestartsNothing)
{
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Follow, {.script = true}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Dual, {.script = true}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Script, {.script = true}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Interaction, {.script = true}, false), Restart::None);
}

TEST(CameraChecks, TheCurrentModeGoneOverThePlayersModeRestartsItOnce)
{
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Player, {.script = true}, false), Restart::Player);
	EXPECT_EQ(RestartAfter(Top::Path, Top::Player, {.path = true}, false), Restart::Player);
	EXPECT_EQ(RestartAfter(Top::Follow, Top::Player, {.follow = true}, false), Restart::Player);
}

TEST(CameraChecks, SeveralModesGoneInOneTurnRestartOnlyTheFinalCurrentOne)
{
	// A path and the follow under it, or a dual camera and the path under it: one restart of the player's mode
	EXPECT_EQ(RestartAfter(Top::Path, Top::Player, {.path = true, .follow = true}, false), Restart::Player);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Player, {.script = true, .path = true}, false), Restart::Player);
	// A dual camera and the upper of two paths: the lower path has the camera again
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Path, {.script = true, .path = true}, false), Restart::Path);
	// A dual camera and a path over the follow: the follow's restart does nothing
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Follow, {.script = true, .path = true}, false), Restart::None);
}

TEST(CameraChecks, AnEmptyStackWithThePlayersModeGoneStartsANewOne)
{
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Player, {.script = true}, true), Restart::NewPlayersMode);
}

TEST(CameraChecks, OnlyTheCurrentOwnersDeletionIsTheCurrentModesGoing)
{
	EXPECT_TRUE(TopWent(Top::Dual, {.script = true}));
	EXPECT_TRUE(TopWent(Top::Script, {.script = true}));
	EXPECT_FALSE(TopWent(Top::Dual, {.path = true, .follow = true}));
	EXPECT_TRUE(TopWent(Top::Path, {.path = true}));
	EXPECT_FALSE(TopWent(Top::Path, {.script = true, .follow = true}));
	EXPECT_TRUE(TopWent(Top::Follow, {.follow = true}));
	EXPECT_FALSE(TopWent(Top::Follow, {.script = true, .path = true}));
	EXPECT_FALSE(TopWent(Top::Player, {.script = true, .path = true, .follow = true}));
	EXPECT_FALSE(TopWent(Top::Interaction, {.script = true, .path = true, .follow = true}));
}

// The frame's camera update: the script camera's mode when its update moved the camera, whatever else has it; else a
// placed path that has the camera; else the camera's own model (the player's mode, or the follow while it has the view)
TEST(CameraChecks, TheFramesUpdateIsTheScriptCamerasWhenItDroveThenAPathsThenThePlayers)
{
	EXPECT_EQ(FrameUpdateFor(true, false), FrameUpdate::Script);
	EXPECT_EQ(FrameUpdateFor(true, true), FrameUpdate::Script);
	EXPECT_EQ(FrameUpdateFor(false, true), FrameUpdate::Path);
	EXPECT_EQ(FrameUpdateFor(false, false), FrameUpdate::Player);
}

// The dance camera on top: its going is the dance owner's, and nothing restarts when it is the new current mode; the
// frame's update is its own while it is the current mode and the script camera did not drive
TEST(CameraChecks, TheDanceCamerasGoingIsItsOwnAndItsFrameUpdateHoldsTheView)
{
	EXPECT_TRUE(TopWent(Top::Dance, {.dance = true}));
	EXPECT_FALSE(TopWent(Top::Dance, {.script = true, .path = true, .follow = true}));
	EXPECT_FALSE(TopWent(Top::Follow, {.dance = true}));
	EXPECT_EQ(RestartAfter(Top::Dance, Top::Player, {.dance = true}, false), Restart::Player);
	EXPECT_EQ(RestartAfter(Top::Dance, Top::Follow, {.dance = true}, false), Restart::None);
	EXPECT_EQ(RestartAfter(Top::Dance, Top::Path, {.dance = true}, false), Restart::Path);
	EXPECT_EQ(RestartAfter(Top::Dual, Top::Dance, {.script = true}, false), Restart::None);
	EXPECT_EQ(FrameUpdateFor(false, false, true), FrameUpdate::Dance);
	EXPECT_EQ(FrameUpdateFor(false, true, true), FrameUpdate::Dance);
	EXPECT_EQ(FrameUpdateFor(true, false, true), FrameUpdate::Script);
}

// The dance camera's frame step: its dance gone ends it; its dance no longer asking for the camera ends it and clears
// the camera's stack; the hand gripping the land ends it; else it holds the view
TEST(CameraChecks, TheDanceCameraEndsOnItsDanceGoneTheCameraNoLongerAskedForOrTheLandGripped)
{
	EXPECT_FALSE(DanceFrameOf(true, true, false).ends);
	EXPECT_FALSE(DanceFrameOf(true, true, false).clearsStack);
	EXPECT_TRUE(DanceFrameOf(false, true, false).ends);
	EXPECT_FALSE(DanceFrameOf(false, true, false).clearsStack);
	EXPECT_TRUE(DanceFrameOf(true, false, false).ends);
	EXPECT_TRUE(DanceFrameOf(true, false, false).clearsStack);
	EXPECT_TRUE(DanceFrameOf(true, true, true).ends);
	EXPECT_FALSE(DanceFrameOf(true, true, true).clearsStack);
}
