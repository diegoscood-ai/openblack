/******************************************************************************
 * Copyright (c) 2018-2024 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>

#include <entt/fwd.hpp>
#include <entt/resource/resource.hpp>
#include <glm/mat4x4.hpp>

namespace openblack
{
class Camera;
class CameraPath;
} // namespace openblack

namespace openblack::ecs::systems
{

class CameraPathSystemInterface
{
public:
	virtual void Start(entt::id_type id) = 0;
	virtual void Stop() = 0;
	/// The debug path player's path (Start) stops; the placed paths stay on the camera's stack of modes. Nothing by default
	virtual void StopPlayback() {}
	virtual void Play() = 0;
	virtual void Pause() = 0;
	/// Once a frame. While a placed path has the camera: the path's update (its glide, or its aim while it plays), then
	/// the camera's zoomers moved on by the frame, as the camera's own update moves them
	virtual void Update(const std::chrono::microseconds& dt) = 0;
	/// Inside the citadel, where none has the camera, the placed path that is the current mode of the camera's stack
	/// still runs its update on the island's camera behind the temple (`world`): its destinations, then that camera's
	/// zoomers moved on by the frame. False, with nothing done, when no placed path is the current mode
	virtual bool UpdateBehindTemple(Camera& /*world*/, const std::chrono::microseconds& /*dt*/) { return false; }
	virtual bool IsPathing() = 0;
	virtual bool IsPaused() = 0;
	/// Who placed a path: any key its creator keeps for it, the same in every call about that path
	using PathOwner = uint64_t;
	/// A path placed in the world by a matrix takes the camera, as a miracle's effect takes its caster's camera
	/// along. Refused, returning false, while the camera's current mode cannot be left (a script holds the camera),
	/// and without a path or a camera. It goes on top of whatever had the camera, another placed path or a dual camera
	/// included, from where the camera's zoomers are (a dual camera's handed back). At its first update it glides onto the path
	/// over the pause and a little more. Meanwhile the player's other camera controls do nothing, while the hand stays free,
	/// drawn and following the mouse: the path never hides it. The near plane stays the camera's height's, as the original's
	/// frame sets it again over the path's close one before any draw. The path is one the resource cache holds
	[[nodiscard]] virtual bool Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement,
	                                 float pauseSeconds) = 0;
	/// Every frame the path's animation is drawn: the path's time at the drawn frame, in whole milliseconds
	/// (camera_path::PathTimeFromFrame), and whether the animation plays. While it plays, each update of the camera
	/// sends it to the path's point at that time, to arrive a moment later, so that it follows a little behind
	virtual void FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing) = 0;
	/// The owner lets go of its path, wherever it is among the placed paths: when the drawn frame reaches the last one
	/// (camera_path::ReachedLastFrame), or the owner goes. The path is only marked as no longer valid: it keeps the
	/// camera and goes on following until the turn's camera checks take it off (RemoveInvalid)
	virtual void Release(PathOwner owner) = 0;
	/// The paths' part of the turn's camera checks (camera_checks::CheckModes): every path let go of is taken off, with
	/// nothing restarted here. True when the one that had the camera went (one under a new player's mode, or covered by
	/// the creature interaction's camera, has not); a placed path under it then has the camera again
	virtual bool RemoveInvalid() = 0;
	/// The map's clear (camera_checks::ClearStack): every placed path goes, covered or not, with nothing restarted here
	virtual void ClearPlaced() {}
	/// Whether a placed path has the camera, so that the player's camera controls are left alone and the near plane is
	/// the close one. Each placed path is an entry of the camera's stack of modes (Kind::Path, by its owner's key), which
	/// this system pushes and takes off: the stack says where it sits, a new player's mode over it included
	/// (camera_stack::ModeStack::CoverPaths, UncoverPaths) and Creature Mode's follow pushed after it
	/// (camera_stack::ModeStack::FollowOverPath), and the system keeps the path's playback. Inside the citadel none has
	/// it: the paths stay on the stack as they were, while the temple's rooms have openblack's one camera
	[[nodiscard]] virtual bool HoldsCamera() const = 0;
	/// A new player's mode over a placed path that is the current mode (camera_checks::PutNewPlayersModeOver: C letting go
	/// of a follow over it, leaving the citadel for a point of the map): it goes on top from where the camera is, the
	/// paths staying under it, as a key taking the camera back puts one. False when no placed path has the camera.
	/// Nothing by default
	virtual bool PutPlayersModeOver() { return false; }
	/// The placed paths on the camera's stack of modes, covered or not
	[[nodiscard]] virtual std::size_t PlacedCount() const = 0;
	/// A camera that cannot be left, the creature interaction's, takes the view over every placed path (`covered`), or
	/// gives it back. Meanwhile no placed path has the camera or is updated (the near plane is not the close one, as the
	/// path's own cleanup puts it back), the player's controls take none of them back, and no path is placed (Begin
	/// refuses, the interaction's camera cannot be left). Given back, the placed path on top, if one is left, has the
	/// camera again from where it is, with the close near plane, as the path's restart
	virtual void SetCovered(bool covered) = 0;
	[[nodiscard]] virtual bool IsCovered() const = 0;
	/// The placed path that has the camera taken off by another (the end of a creature hold it kept the view through):
	/// gone, with the near plane no longer the close one, as its cleanup; the camera stays where it is and the placed
	/// path under it, if any, has the camera. Its owner's release later finds nothing. False when none has the camera
	virtual bool TakeOffCurrent() = 0;
	/// The lowest placed path deleted from the bottom of a full camera stack (the stack's lowest path entry, which goes
	/// with it): gone, with nothing restarted and the near plane left as it is, as the original deletes the bottom mode
	/// with no cleanup. Its owner's release later finds nothing
	virtual void DeleteLowest() {}
	/// The placed path that has the camera, as the debug tools show it
	struct PlacedReadout
	{
		PathOwner owner {0};
		/// The path's time at its animation's last drawn frame (FollowAt)
		int32_t pathMilliseconds {0};
		/// Its animation plays: the camera follows the path
		bool playing {false};
		/// The glide onto the path was set, at its first update
		bool glided {false};
		float pauseSeconds {0.0f};
	};
	/// None while no placed path has the camera
	[[nodiscard]] virtual std::optional<PlacedReadout> CurrentPlaced() const = 0;
	/// What the player is doing that may take the camera back from a placed path: a key moving the camera left, right,
	/// forwards or backwards held through a frame of so many whole milliseconds, or the hand gripping the land to drag
	/// it. Rotating, tilting and zooming don't (see Camera/CameraPathControl.h)
	struct PlayerControl
	{
		bool movementKey {false};
		uint32_t frameMilliseconds {0};
		bool grippingLand {false};
		/// A script's camera is on top of the placed paths, so the one that had the camera is not the current mode
		bool scriptCameraOver {false};
	};
	/// Once a frame, before the camera handles the player's controls: a movement key that would move the camera this
	/// frame, or the hand gripping the land, puts a new player's mode on top at once, from where the camera is (one for
	/// each, camera_path::PlayersModesOnTakeBack). The placed paths stay under it, and have the camera again only if it
	/// is popped. Only the current camera mode reads the keys, so under a script's camera the paths take nothing back
	virtual void HandlePlayerControl(const PlayerControl& control) = 0;
};
} // namespace openblack::ecs::systems
