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
#include <memory>
#include <optional>

#include <glm/vec3.hpp>

#include "CameraDrag.h"
#include "FightWatch.h"
#include "Hand/HandTricons.h"

namespace openblack
{

class Camera;
class LandIslandInterface;

class CameraModel
{
public:
	enum class Model : uint8_t
	{
		DefaultWorld,
		Old,
	};

	struct CameraInterpolationUpdateInfo
	{
		glm::vec3 origin;
		glm::vec3 focus;
		std::chrono::microseconds duration;
	};

	struct FlightPath
	{
		glm::vec3 origin;
		glm::vec3 focus;
		std::optional<glm::vec3> midpoint;
	};

	/// A projection a model looks through instead of the configured one
	struct Lens
	{
		/// In degrees
		float horizontalFieldOfView;
		float nearClip;
	};

	/// What the camera does with the mouse, for the hand to show
	struct HandCues
	{
		/// The camera hints, as camera_drag::tricon
		uint32_t tricons {0};
		/// The land is being dragged, and what the drag has turned into once decided
		bool dragging {false};
		std::optional<camera_drag::DragMode> dragMode;
		/// Ctrl and Shift held for a clear view: dragging, the hand grips
		bool clearViewGrip {false};
		/// The player's camera holds the land gripped this frame, as its controls read it: the middle button, both
		/// buttons and the hold after a drag given up or at the fence let go of it, and it is never held while the
		/// controls are not read (a script's camera, a camera path, a hand demo) or by another camera. The hand is in its
		/// camera state while it holds
		bool landGripped {false};
		/// What the player's camera's helper icons by the hand are picked from this frame, none for other cameras (the
		/// hand's influence and the clock are left for the caller), and how far the clear view has come in
		std::optional<hand_tricons::WorldCameraFrame> icons;
		float clearView {0.0f};
		/// Where the clear view put the hand this frame, on the place on the land of the object it looks at, while it
		/// follows one; the camera's hand is drawn there while it grips
		std::optional<glm::vec3> clearViewHandPlace;
	};

	/// What the player's own camera finds at the middle of the screen: whether that pixel meets the land, and the
	/// distance from the eye it heads for to that point (at least 10; without the land, its average distance to it)
	struct ScreenCentre
	{
		bool onLand {false};
		float headingDistance {0.0f};
	};

	static std::unique_ptr<CameraModel> CreateModel(Model model);

	/// A flight from the current origin to a new place, through a middle point: halfway between the two origins, raised
	/// by their distance across the land times `rise`, and at least 10 above the land there
	[[nodiscard]] static FlightPath CharterFlight(const LandIslandInterface& land, glm::vec3 origin, glm::vec3 focus,
	                                              glm::vec3 currentOrigin, float rise);

	virtual ~CameraModel();

	virtual std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) = 0;
	virtual void HandleActions(std::chrono::microseconds dt) = 0;
	/// The controls are not read this frame (a script's camera, a hand demo or a camera path has the camera as it
	/// begins), or the model has just been given the camera: the update that follows has none of this frame's controls.
	/// Nothing by default
	virtual void SkipActions() {}
	/// A script's control of the camera has ended and the player's camera is made again: a model that keeps its own
	/// state from frame to frame starts again from the camera as it is now. Nothing by default
	virtual void Restart() {}
	/// The player has left the citadel: a model that keeps its own state starts again from the camera as it is now, a
	/// watched fight kept when `keepFight` and both its fighters are still there. Nothing by default
	virtual void StartAgain(bool /*keepFight*/) {}
	/// A camera mode put over the player's (a script's dual camera) has gone, and the player's mode is current again,
	/// with the camera's origin at `cameraOrigin`: a model that keeps its own state goes back to the view it had. Nothing
	/// by default
	virtual void Resume(glm::vec3 /*cameraOrigin*/) {}
	/// A new player's mode goes on top of this one, which stays under it until that one is popped: a copy of the model as
	/// it is now, for the camera to have again then. The player's own camera keeps its own view and its watched fight;
	/// none for the other models
	[[nodiscard]] virtual std::unique_ptr<CameraModel> KeptUnder() const { return nullptr; }
	virtual void SetFlight(glm::vec3 origin, glm::vec3 focus) = 0;
	/// The camera has been put here from outside: whatever it was doing (a flight, a drag of the land, easing towards
	/// somewhere else) is forgotten, so that it stays here until the player or a script moves it
	virtual void PlaceAt(glm::vec3 /*origin*/, glm::vec3 /*focus*/) {}
	[[nodiscard]] virtual glm::vec3 GetTargetOrigin() const = 0;
	[[nodiscard]] virtual glm::vec3 GetTargetFocus() const = 0;
	[[nodiscard]] virtual std::optional<Lens> GetLens() const { return std::nullopt; }
	/// What the camera does with the mouse, which the hand shows: the hints the cursor's place offers, and what a
	/// drag of the land has turned into
	[[nodiscard]] virtual HandCues GetHandCues() const { return {}; }
	/// The middle of the screen as the player's own camera reads it; none for the other cameras
	[[nodiscard]] virtual std::optional<ScreenCentre> GetScreenCentre() const { return std::nullopt; }
	/// The camera's watching of creature fights: only the player's own camera watches them, the other models have none
	[[nodiscard]] virtual camera::FightWatch* GetFightWatch() { return nullptr; }
	[[nodiscard]] virtual const camera::FightWatch* GetFightWatch() const { return nullptr; }
	/// Whether the player can go on drawing a gesture while this camera moves; a moving camera wipes the gesture under
	/// every other camera
	[[nodiscard]] virtual bool LetsPlayerGestureWhileMoving() const { return false; }
};

} // namespace openblack
