/******************************************************************************
 * Copyright (c) 2018-2024 openblack developers
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

#include <entt/fwd.hpp>
#include <entt/resource/resource.hpp>
#include <glm/mat4x4.hpp>

namespace openblack
{
class CameraPath;
}

namespace openblack::ecs::systems
{

class CameraPathSystemInterface
{
public:
	virtual void Start(entt::id_type id) = 0;
	virtual void Stop() = 0;
	virtual void Play() = 0;
	virtual void Pause() = 0;
	/// Once a frame. While a placed path has the camera: the path's update (its glide, or its aim while it plays), then
	/// the camera's zoomers moved on by the frame, as the camera's own update moves them
	virtual void Update(const std::chrono::microseconds& dt) = 0;
	virtual bool IsPathing() = 0;
	virtual bool IsPaused() = 0;
	/// Who placed a path: any key its creator keeps for it, the same in every call about that path
	using PathOwner = uint64_t;
	/// A path placed in the world by a matrix takes the camera, as a miracle's effect takes its caster's camera
	/// along. Refused, returning false, while the camera's current mode cannot be left (a script holds the camera),
	/// and without a path or a camera. It goes on top of whatever had the camera, another placed path included, from
	/// where the camera's zoomers are. At its first update it glides onto the path over the pause and a little more.
	/// Meanwhile the player's other camera controls do nothing, while the hand stays free, drawn and following the
	/// mouse: the path never hides it, and the near plane is the close one (near_clipping::k_Close). The path is one
	/// the resource cache holds
	[[nodiscard]] virtual bool Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement,
	                                 float pauseSeconds) = 0;
	/// Every frame the path's animation is drawn: the path's time at the drawn frame, in whole milliseconds
	/// (camera_path::PathTimeFromFrame), and whether the animation plays. While it plays, each update of the camera
	/// sends it to the path's point at that time, to arrive a moment later, so that it follows a little behind
	virtual void FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing) = 0;
	/// The owner lets go of its path, wherever it is among the placed paths: when the drawn frame reaches the last one
	/// (camera_path::ReachedLastFrame), or the owner goes. The camera stays where it is, and a placed path under it has
	/// the camera again
	virtual void Release(PathOwner owner) = 0;
	/// Whether a placed path has the camera, so that the player's camera controls are left alone and the near plane is
	/// the close one
	[[nodiscard]] virtual bool HoldsCamera() const = 0;
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
	};
	/// Once a frame, before the camera handles the player's controls: a movement key that would move the camera this
	/// frame, or the hand gripping the land, gives the camera back to the player at once, from where it is. The placed
	/// paths under the player's camera never have it again
	virtual void HandlePlayerControl(const PlayerControl& control) = 0;
};
} // namespace openblack::ecs::systems
