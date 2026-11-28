/******************************************************************************
 * Copyright (c) 2018-2024 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "3D/CameraPath.h"
#include "ECS/Systems/CameraPathSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

enum class CameraPathState : uint8_t
{
	PLAYING,
	PAUSED,
	STOPPED
};

class CameraPathSystem final: public CameraPathSystemInterface
{
public:
	/// Whether the camera's current mode can be left for a placed path
	using CanLeaveCameraMode = std::function<bool()>;
	/// A new player's mode put on top of the camera's stack, its number returned (0: no stack to put it on)
	using PushPlayersMode = std::function<uint32_t()>;
	/// The game's camera: its mode can be left unless a script holds it
	CameraPathSystem();
	explicit CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode);
	CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode, PushPlayersMode pushPlayersMode);
	/// The paths still placed take their entries off the camera's stack of modes
	~CameraPathSystem();
	CameraPathSystem(const CameraPathSystem&) = delete;
	CameraPathSystem& operator=(const CameraPathSystem&) = delete;
	CameraPathSystem(CameraPathSystem&&) = delete;
	CameraPathSystem& operator=(CameraPathSystem&&) = delete;

	void Start(entt::id_type id) override;
	void Stop() override;
	void StopPlayback() override;
	void Play() override { _state = CameraPathState::PLAYING; }
	void Pause() override { _state = CameraPathState::PAUSED; }
	void Update(const std::chrono::microseconds& dt) override;
	bool UpdateBehindTemple(Camera& world, const std::chrono::microseconds& dt) override;
	bool IsPathing() override
	{
		return !_placed.empty() || _state == CameraPathState::PLAYING || _state == CameraPathState::PAUSED;
	}
	bool IsPaused() override { return _state == CameraPathState::PAUSED; }
	[[nodiscard]] bool Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement,
	                         float pauseSeconds) override;
	void FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing) override;
	void Release(PathOwner owner) override;
	bool RemoveInvalid() override;
	void ClearPlaced() override;
	[[nodiscard]] bool HoldsCamera() const override;
	[[nodiscard]] std::size_t PlacedCount() const override { return _placed.size(); }
	void SetCovered(bool covered) override { _covered = covered; }
	[[nodiscard]] bool IsCovered() const override { return _covered; }
	bool TakeOffCurrent() override;
	void DeleteLowest() override;
	[[nodiscard]] bool TakeLeftCurrent() override;
	[[nodiscard]] std::optional<PlacedReadout> CurrentPlaced() const override;
	void HandlePlayerControl(const PlayerControl& control) override;
	bool PutPlayersModeOver() override;

private:
	/// One new player's mode on top, the placed paths under it; with no stack of modes to keep them under, every placed
	/// path goes. False then
	bool PushPlayersModeOverPaths();
	/// A path placed in the world that the camera follows
	struct Placed
	{
		PathOwner owner;
		entt::resource<CameraPath> path;
		glm::mat4 placement;
		float pauseSeconds;
		/// The path's time at the animation's last drawn frame, 0 until it is drawn
		int32_t pathMilliseconds {0};
		/// The animation plays
		bool playing {false};
		/// The glide onto the path is set
		bool glided {false};
		/// Its owner has not let go of it yet; one let go of goes at the turn's camera checks
		bool valid {true};
	};
	/// The current placed path's update: the zoomers of `camera` sent on along the path
	void UpdatePlaced(Placed& placed, Camera& camera);
	/// The newest placed path is the current mode of the camera's stack: not covered, under no new player's mode and no
	/// follow or dance camera pushed after it. Inside the citadel too, where none has the camera
	[[nodiscard]] bool PathCurrent() const;
	/// The number of the new player's mode put over the placed path, as its entry on the camera's stack of modes says;
	/// 0 while none is
	[[nodiscard]] static uint32_t Under(const Placed& placed);
	/// Every placed path goes, with its entry on the camera's stack of modes
	void ClearAllPlaced();
	/// A placed path gone (its owner's key) takes its entry off the camera's stack of modes
	static void TakeOffStack(PathOwner owner);
	CanLeaveCameraMode _canLeaveCameraMode;
	PushPlayersMode _pushPlayersMode;
	/// The placed paths' playback, by their owners' keys, in the order of their entries on the camera's stack of modes:
	/// the last the one that has the camera
	std::vector<Placed> _placed;
	/// The creature interaction's camera has the view over them (SetCovered)
	bool _covered {false};
	/// The owner of the placed path that was the current mode at the last TakeLeftCurrent
	std::optional<PathOwner> _lastCurrent;
	/// The map's clear took a path off the bottom of the stack, with its cleanup, since the last TakeLeftCurrent
	bool _bottomCleanedUp {false};

	entt::resource<CameraPath> _path;
	/// How far along the path the camera is
	std::chrono::microseconds _elapsed {0};
	CameraPathState _state {CameraPathState::STOPPED};
};
} // namespace openblack::ecs::systems
