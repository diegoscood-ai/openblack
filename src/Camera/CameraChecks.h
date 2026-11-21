/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Camera/WorldCamera.h"

namespace openblack
{
class Camera;
} // namespace openblack

namespace openblack::camera_stack
{
class ModeStack;
} // namespace openblack::camera_stack

namespace openblack::ecs::systems
{
class CreatureModeSystemInterface;
}

/// The turn's check of the camera's stacked modes, in one pass: from the bottom of the stack every mode that is no longer
/// valid is deleted, only the current one being cleaned up first; then a stack left empty starts a new player's mode;
/// then, if the current mode was among those deleted, the new current one is restarted once. The mode's seconds run on.
/// The modes are kept by three owners (the script camera's stack, the placed paths, Creature Mode's follow), in the stack's
/// one push order: the mode pushed last, whatever its owner, is the current one
namespace openblack::camera_checks
{

/// The kind of the camera's current mode
enum class Top : uint8_t
{
	/// The player's mode: the bottom one, or a new one on top of the others
	Player,
	/// Creature Mode's follow
	Follow,
	/// A placed path
	Path,
	/// The creature interaction's camera
	Interaction,
	/// A dual camera
	Dual,
	/// A script's camera mode
	Script,
	/// The dance camera, which holds the view on a dance
	Dance,
};

/// Which owners' current modes the pass deleted
struct Went
{
	/// The script camera's current mode (a dual camera)
	bool script {false};
	/// The placed path on top, the one that has the camera
	bool path {false};
	/// Creature Mode's follow while it has the camera
	bool follow {false};
	/// The dance camera on top
	bool dance {false};
};

/// What restarts after the pass
enum class Restart : uint8_t
{
	/// Nothing: the current mode stayed, or the new one's restart does nothing (a follow, a dual camera, a script's mode,
	/// the creature interaction's camera)
	None,
	/// A placed path: it sets its close near plane again
	Path,
	/// The player's mode: it starts again and flies back to the view it had
	Player,
	/// The stack was left empty with the player's own mode gone: a new player's mode starts from the camera as it is
	NewPlayersMode,
};

/// Whether the pass deleted the current mode, from its kind before the pass and the owners' deletions
[[nodiscard]] constexpr bool TopWent(Top before, const Went& went)
{
	switch (before)
	{
	case Top::Dual:
	case Top::Script:
		return went.script;
	case Top::Path:
		return went.path;
	case Top::Follow:
		return went.follow;
	case Top::Dance:
		return went.dance;
	case Top::Player:
	case Top::Interaction:
		return false;
	}
	return false;
}

/// The one restart after the pass: `before` and `after` are the current mode's kinds before and after it, `went` what
/// it deleted, `newPlayersMode` whether it left the stack empty with the player's own mode gone
[[nodiscard]] constexpr Restart RestartAfter(Top before, Top after, const Went& went, bool newPlayersMode)
{
	if (newPlayersMode)
	{
		return Restart::NewPlayersMode;
	}
	if (!TopWent(before, went))
	{
		return Restart::None;
	}
	switch (after)
	{
	case Top::Path:
		return Restart::Path;
	case Top::Player:
		return Restart::Player;
	case Top::Follow:
	case Top::Interaction:
	case Top::Dual:
	case Top::Script:
	case Top::Dance:
		return Restart::None;
	}
	return Restart::None;
}

/// Which update moves the camera in the frame's camera update
enum class FrameUpdate : uint8_t
{
	/// The script camera's mode: its own update, which runs first every frame, moved the camera
	Script,
	/// A placed path that has the camera
	Path,
	/// The dance camera: it holds the view, the camera's zoomers going on to where they were heading, and its frame step
	/// may end it (UpdateDanceCamera)
	Dance,
	/// The camera's own model: the player's mode, or Creature Mode's follow while it has the view
	Player,
};

/// The frame's camera update: the script camera's mode when its update moved the camera (one of its modes drives it, not
/// inside the citadel); else the dance camera when it is the current mode (not inside the citadel); else a placed path
/// that has the camera, read after Creature Mode's update (which can take the camera or give it back); else the camera's
/// own model
[[nodiscard]] constexpr FrameUpdate FrameUpdateFor(bool scriptDrove, bool pathHoldsCamera, bool danceCurrent = false)
{
	if (scriptDrove)
	{
		return FrameUpdate::Script;
	}
	if (danceCurrent)
	{
		return FrameUpdate::Dance;
	}
	return pathHoldsCamera ? FrameUpdate::Path : FrameUpdate::Player;
}

/// What the dance camera's frame step does
struct DanceFrame
{
	/// It ends: no longer valid, it stays on the stack until the turn's check deletes it
	bool ends {false};
	/// The camera's whole stack goes (ClearStack), a new player's mode starting from the camera as it is
	bool clearsStack {false};
};

/// The dance camera's frame step, pure: its dance gone ends it; its dance no longer asking for the camera ends it and
/// clears the camera's stack; the hand gripping the land (`landGripped`, from the interface's hand state as Creature
/// Mode's follow reads it) ends it
[[nodiscard]] constexpr DanceFrame DanceFrameOf(bool danceThere, bool useCamera, bool landGripped)
{
	DanceFrame frame;
	if (!danceThere)
	{
		frame.ends = true;
		return frame;
	}
	if (!useCamera)
	{
		frame.ends = true;
		frame.clearsStack = true;
	}
	if (landGripped)
	{
		frame.ends = true;
	}
	return frame;
}

/// The frame's camera update as the camera's services have it now, `scriptDrove` being what the script camera's update
/// gave this frame: whether a placed path has the camera is read only when it did not drive
[[nodiscard]] FrameUpdate CurrentFrameUpdate(bool scriptDrove);

/// The current mode's kind, as the camera's services have it now
[[nodiscard]] Top CurrentTop();

/// A new player's mode on top of the current mode, as the game makes one over any current mode that is not a player's
/// (docs/wiki/camera/game-camera.md#openblack): the mode stays under it, the new mode starts from the camera as it is,
/// with no flight and no fight watched, and has every placed path not yet under one under it. A dual camera that drove
/// the camera hands its zoomers back first; the follow whose model the camera has (the current mode, or under a dual
/// camera or a path pushed over it) keeps it under the new mode, the camera having a new player's camera. Refused, with
/// nothing done, while the current mode cannot be left (a script's camera mode still going, the creature interaction's
/// camera). True when a new mode was made; false with a player's mode current already
bool PutNewPlayersModeOver();
/// As PutNewPlayersModeOver, with Creature Mode's follows those of `follows` (none: no follow)
bool PutNewPlayersModeOver(ecs::systems::CreatureModeSystemInterface* follows);

/// The turn's check (once a turn, at the turn's camera checks)
void CheckModes();

/// The dance camera of `dance`, called in by its key frame (docs/wiki/beings/dances.md#the-dance-camera-actions-18-19):
/// on top of the whole stack, the current mode, holding the view where it is. A dual camera that drove the camera hands
/// its zoomers back first. Refused, with nothing done, while the current mode cannot be left (a script's camera mode
/// still going, the creature interaction's camera). True when it was made
bool MakeDanceCamera(entt::entity dance);

/// The dance camera's frame step while it is the current mode (DanceFrameOf, with the interface's hand state), then the
/// camera's zoomers go on with no new destination
void UpdateDanceCamera(Camera& camera, std::chrono::microseconds dt, int32_t interfaceHandState);

/// The map's clear: every mode of the stack goes (the follow, the placed paths and the script camera's modes, the new
/// game's clear putting the lens back too), and a new player's mode starts from the camera as it is
void ClearStack(bool newGame);

/// Leaving the citadel, the camera's zoomers the island's again and its current mode given back its model: the way in
/// left the camera's modes as they were (docs/wiki/interface/temple-interior.md). With a place picked on the map the
/// camera is cut there. A player's mode then starts again, keeping a watched fight unless a place was picked, its help
/// icons by the hand with it; another mode (Creature Mode's follow, a placed path, a dual camera) goes on as it is, but
/// with a place picked a new player's mode goes over it from there (PutNewPlayersModeOver), the mode staying under it,
/// unless it cannot be left (world_camera::OnLeave)
void LeaveCitadel(std::optional<world_camera::Place> picked);

/// Whether the camera's own model is the current mode of `stack` (none: no stack, the player's own mode): the player's
/// mode, Creature Mode's follow or the creature interaction's camera. A placed path, the dance camera and the script
/// camera's own modes move the camera by their own updates instead
[[nodiscard]] bool CameraModelIsCurrent(const camera_stack::ModeStack* stack);

/// One frame of the island's camera behind the temple (`world`, the temple's GetWorldCamera), in the frame's camera update:
/// the camera goes on under the rooms' own camera as it does outside. None of the player's controls reach it; the camera's
/// model then moves it when it is the current mode (CameraModelIsCurrent), and its zoomers go on in every mode. The view
/// it leaves is the one its next update casts its rays from
void UpdateBehindTemple(Camera& world, std::chrono::microseconds dt);

} // namespace openblack::camera_checks
