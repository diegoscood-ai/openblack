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
	/// The game's camera: its mode can be left unless a script holds it
	CameraPathSystem();
	explicit CameraPathSystem(CanLeaveCameraMode canLeaveCameraMode);

	void Start(entt::id_type id) override;
	void Stop() override;
	void Play() override { _state = CameraPathState::PLAYING; }
	void Pause() override { _state = CameraPathState::PAUSED; }
	void Update(const std::chrono::microseconds& dt) override;
	bool IsPathing() override
	{
		return !_placed.empty() || _state == CameraPathState::PLAYING || _state == CameraPathState::PAUSED;
	}
	bool IsPaused() override { return _state == CameraPathState::PAUSED; }
	[[nodiscard]] bool Begin(PathOwner owner, entt::resource<CameraPath> path, const glm::mat4& placement,
	                         float pauseSeconds) override;
	void FollowAt(PathOwner owner, int32_t pathMilliseconds, bool playing) override;
	void Release(PathOwner owner) override;
	[[nodiscard]] bool HoldsCamera() const override { return !_placed.empty(); }
	[[nodiscard]] std::optional<PlacedReadout> CurrentPlaced() const override;
	void HandlePlayerControl(const PlayerControl& control) override;

private:
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
	};
	/// The current placed path's update: the camera's zoomers sent on along the path
	void UpdatePlaced(Placed& placed);
	CanLeaveCameraMode _canLeaveCameraMode;
	/// The placed paths, the last the one that has the camera
	std::vector<Placed> _placed;

	entt::resource<CameraPath> _path;
	/// How far along the path the camera is
	std::chrono::microseconds _elapsed {0};
	CameraPathState _state {CameraPathState::STOPPED};
};
} // namespace openblack::ecs::systems
