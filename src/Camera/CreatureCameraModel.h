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

#include <functional>
#include <optional>

#include "CameraModel.h"
#include "CreatureFollow.h"

namespace openblack
{

/// Creature Mode's camera: locked onto a creature, it follows it round the land, easing after it. The player turns it
/// with the keys (see creature_follow), and the cursor keys alone give the camera back. Creature Mode takes the camera
/// over from the player's own camera with it and hands it back afterwards; it tells it each frame where the creature is.
///
/// It has no lens of its own: it looks at the open world through the configured projection, and the camera keeps it on
/// the world disc as it keeps the player's own model.
class CreatureCameraModel final: public CameraModel
{
public:
	/// The seconds this frame moves the mode's time on by
	using FrameSeconds = std::function<float()>;

	/// Starts on a creature, by its middle and height, from where the camera is heading for. The mode's time moves on by
	/// the frame seconds given, or when none are given by the camera's frame time, as the script camera's follow does
	CreatureCameraModel(glm::vec3 cameraOrigin, glm::vec3 cameraFocus, glm::vec3 creature, float height,
	                    FrameSeconds frameSeconds = {});

	/// Where the creature now is, and how tall it is
	void SetTarget(glm::vec3 position, float height);
	/// The point it looks at, for a thing followed that is not a creature: the follow's own point of the thing
	/// (script_camera::FollowPoint)
	void SetFocusPoint(glm::vec3 focus) { _focus = focus; }
	/// The way the thing followed faces (its game angle), when it faces somewhere: a heading kept behind it turns with it
	void SetFacing(std::optional<uint16_t> wallHugAngle) { _wallHugAngle = wallHugAngle; }
	/// How it eases and turns (Creature Mode's own unless a load gives another, as a follow saved in a cut scene)
	void SetStyle(const creature_follow::Style& style) { _style = style; }
	[[nodiscard]] const creature_follow::Style& GetStyle() const { return _style; }
	/// Swings the camera round and tilts it for a clear view of the creature, as Ctrl and Shift do together
	void ClearView(glm::vec3 landNormal, const creature_follow::GroundHeight& ground);
	/// Turns it by keys, as if held this frame, on a screen so many pixels wide; returns whether they give the camera
	/// back
	creature_follow::KeyOutcome Turn(const creature_follow::Keys& keys, float screenWidth);

	[[nodiscard]] const creature_follow::View& GetView() const { return _view; }
	/// The view a saved game kept, as a load reads it into the follow: the heading, the pitch and the distance as saved,
	/// kept in bounds by the next update
	void SetView(const creature_follow::View& view) { _view = view; }
	[[nodiscard]] float GetSecondsInMode() const { return _seconds; }
	/// Given the camera back by a camera that had it meanwhile (the creature interaction's): its time in the mode starts
	/// again, as at every change of the camera's mode, so it eases after the creature at its starting pace; its view and
	/// its creature are kept
	void Restart()
	{
		_seconds = 0.0f;
		_leftTheZone = false;
	}
	/// Whether the player has asked for their camera back with the cursor keys
	[[nodiscard]] bool WantsToLeave() const { return _leaveRequested; }
	/// Whether its place left the camera fence at its last update: a new player's camera goes on top of it, from where it
	/// stopped on the fence, and the follow stays under it
	[[nodiscard]] bool LeftTheZone() const { return _leftTheZone; }

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// Reads the keys: the cursor keys with Shift or Ctrl, the wheel, and Ctrl and Shift together for a clear view
	void HandleActions(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return creature_follow::Origin(_focus, PlacedView()); }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _focus; }
	/// Following a creature, the camera moves all the time: the player can gesture while it does
	[[nodiscard]] bool LetsPlayerGestureWhileMoving() const override { return true; }

private:
	/// The view it is placed from: its own, with the heading turned with the thing when it is kept behind it
	[[nodiscard]] creature_follow::View PlacedView() const;

	creature_follow::View _view;
	creature_follow::Style _style;
	std::optional<uint16_t> _wallHugAngle;
	glm::vec3 _focus;
	FrameSeconds _frameSeconds;
	float _seconds {0.0f};
	bool _leaveRequested {false};
	bool _leftTheZone {false};
};

} // namespace openblack
