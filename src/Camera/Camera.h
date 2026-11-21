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

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "CameraModel.h"
#include "Common/Zoomer.h"
#include "ECS/Components/Transform.h"
#include "KeyboardMoveSpeed.h"
#include "ScreenPoint.h"

namespace openblack
{

class Camera
{
public:
	enum class Interpolation : uint8_t
	{
		Current,
		Start,
		Target,
	};
	enum class Projection : uint8_t
	{
		Normal,
		ReversedZ,
	};
	explicit Camera(glm::vec3 focus = glm::vec3(1000.0f, 0.0f, 1000.0f));
	virtual ~Camera();

	[[nodiscard]] float GetHorizontalFieldOfView() const;
	/// The W / H SetProjectionMatrixPerspective was given (the viewport's aspect)
	[[nodiscard]] float GetAspect() const { return _aspect; }
	[[nodiscard]] virtual glm::mat4 GetViewMatrix(Interpolation interpolation) const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix() const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix(Projection projection) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Interpolation interpolation = Camera::Interpolation::Current) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Projection projection,
	                                                Interpolation interpolation = Camera::Interpolation::Current) const;

	/// The island's land under the mouse, as the game's camera finds it (RaycastScreenCoordToLand at the mouse's pixel)
	[[nodiscard]] std::optional<glm::vec3> RaycastMouseToLand() const;
	/// The island's land under a pixel, as the game's camera finds it: PickingSystemInterface::LandFrom2DPoint with the
	/// line moved on by the near clip, the land or else the sea within 7500 of the eye, at the quantised point's height
	[[nodiscard]] std::optional<glm::vec3> RaycastScreenCoordToLand(glm::ivec2 pixel) const;
	/// The same two from the kept view (GetKeptView), as the player's camera casts them
	[[nodiscard]] std::optional<glm::vec3> RaycastMouseToLandFromKeptView() const;
	[[nodiscard]] std::optional<glm::vec3> RaycastScreenCoordToLandFromKeptView(glm::ivec2 pixel) const;

	[[nodiscard]] glm::vec3 GetOrigin(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetOriginVelocity(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocus(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocusVelocity(Interpolation interpolation = Interpolation::Current) const;

	/// Get rotation as euler angles in radians
	[[nodiscard]] glm::vec3 GetRotation() const;

	Camera& SetOrigin(const glm::vec3& position);
	Camera& SetFocus(const glm::vec3& position);

	/// The camera's Zoomer3 of the position and of the focus
	[[nodiscard]] Zoomer3& GetOriginZoomer() { return _origin; }
	[[nodiscard]] Zoomer3& GetFocusZoomer() { return _focus; }
	[[nodiscard]] const Zoomer3& GetOriginZoomer() const { return _origin; }
	[[nodiscard]] const Zoomer3& GetFocusZoomer() const { return _focus; }
	/// The camera shake: a world-space translation of the DRAWN position and focus only, applied when the engine
	/// camera is updated; the Zoomer3 are never touched, so it does not build up. GetOrigin / GetFocus (Current) and
	/// GetViewMatrix add it (GET_CAMERA_POSITION sees the shaking camera). Set every frame (camera_shake::Adjust), zero
	/// when there is none
	void SetDrawOffset(const glm::vec3& origin, const glm::vec3& focus)
	{
		_originDrawOffset = origin;
		_focusDrawOffset = focus;
	}
	/// A drawn view of its own (only the falling spell sets it: magic::falling_spell::WorldToCamera). GetViewMatrix
	/// gives it while it is set; the zoomers are not touched (the game camera is not pushed to the engine meanwhile),
	/// so clearing it draws the game camera again, as the first frame in mode 0 does
	void SetDrawnView(const std::optional<glm::mat4>& view) { _drawnView = view; }
	/// Whether such a drawn view is set
	[[nodiscard]] bool HasDrawnView() const { return _drawnView.has_value(); }
	/// The position and the focus a drawn view of its own was drawn from
	struct Place
	{
		glm::vec3 origin;
		glm::vec3 focus;
	};
	/// The place of the drawn view of its own drawn last (the falling spell's path camera, each frame it is drawn). It
	/// stays the drawn camera's place after the view is cleared, until the game camera is drawn again
	/// (ForgetDrawnPlace), as nothing puts the drawn camera back when the fall ends
	void SetDrawnPlace(const Place& place) { _drawnPlace = place; }
	/// The game camera is drawn again: its own position and focus are the drawn camera's
	void ForgetDrawnPlace() { _drawnPlace.reset(); }
	/// Where the camera is drawn from: the place of the drawn view of its own while one is kept, else GetOrigin /
	/// GetFocus (Current, the shake included). What reads the drawn camera's place reads these; what reads the game
	/// camera's own position reads GetOrigin / GetFocus or the zoomers
	[[nodiscard]] glm::vec3 GetDrawnOrigin() const { return _drawnPlace.has_value() ? _drawnPlace->origin : GetOrigin(); }
	[[nodiscard]] glm::vec3 GetDrawnFocus() const { return _drawnPlace.has_value() ? _drawnPlace->focus : GetFocus(); }
	/// Where the camera's update reads the weather at the camera (script_camera::ApplyShake sets it): the game camera
	/// held over the land, without the debug camera or the shake; while the falling spell runs, the drawn camera of the
	/// frame before held over the land; none inside the citadel, when it is GetOrigin
	void SetWeatherPoint(const std::optional<glm::vec3>& point) { _weatherPoint = point; }
	[[nodiscard]] glm::vec3 GetWeatherPoint() const { return _weatherPoint.value_or(GetOrigin()); }
	/// The time of the zoomers since their last destination (the position's x Zoomer's current time)
	[[nodiscard]] std::chrono::microseconds GetInterpolatorTime() const;

	Camera& SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip, float farClip);
	/// The near clip the world is drawn with: the model's lens's when it has one (the temple's), else the configured one
	[[nodiscard]] float GetDrawnNearClip(float configuredNearClip) const;
	/// The near clip the screen's points are found with: the one the last camera update took (the frame's object draw
	/// list near clip), kept until the next one. The game starts with 3.5
	void SetPixelNearClip(float nearClip) noexcept { _pixelNearClip = nearClip; }
	[[nodiscard]] float GetPixelNearClip() const noexcept { return _pixelNearClip; }
	Camera& SetProjectionMatrix(const glm::mat4& projection);

	[[nodiscard]] glm::vec3 GetForward() const;
	[[nodiscard]] glm::vec3 GetRight() const;
	[[nodiscard]] glm::vec3 GetUp() const;

	[[nodiscard]] std::unique_ptr<Camera> Reflect() const;

	/// A ray from the camera's eye through a point of the screen
	struct EyeRay
	{
		glm::vec3 origin;    ///< the eye
		glm::vec3 direction; ///< of length 1
	};
	/// The ray from the eye through a screen point (0..1, from the top left corner), as the original casts the mouse:
	/// built from the camera's axes, its horizontal field of view and its aspect, so neither clip plane moves it. The
	/// game's rays use it; DeprojectScreenToWorld stays for the drawing and the debug tools
	[[nodiscard]] EyeRay RayFromEye(glm::vec2 screenCoord, Interpolation interpolation = Interpolation::Current) const;
	/// The eye the game's rays start at: the camera's position, or the drawn view's own while one is set
	[[nodiscard]] glm::vec3 GetEye(Interpolation interpolation = Interpolation::Current) const;
	/// The world point of an integer pixel on the near plane, as the game finds it: from the view's axes (the
	/// camera's own, or those of a drawn view while one is set), the horizontal field of view, the screen's size and the
	/// near clip, with the eye added last (screen_point::PixelPoint at depth 0)
	[[nodiscard]] glm::vec3 PixelNearPoint(glm::vec3 eye, glm::ivec2 screenSize, glm::ivec2 pixel, float nearClip) const;
	/// The view the game's rays to the land start from now: GetEye, the axes PixelNearPoint takes and the horizontal
	/// field of view
	[[nodiscard]] screen_point::View GetRayView() const;
	/// Keeps the view as the camera's update leaves it, the view the game draws. The player's camera casts its rays to
	/// the land on its next update from this view: a command that puts the camera between two updates does not move them
	void KeepDrawnView() { _keptView = GetRayView(); }
	/// Forgets the kept view: until the next KeepDrawnView the player's camera casts its rays from the view now
	void ForgetKeptView() { _keptView.reset(); }
	/// The kept view, or the view now when none is kept
	[[nodiscard]] screen_point::View GetKeptView() const { return _keptView.value_or(GetRayView()); }
	/// A ray starting on a plane about twice the near clip in front of the eye, its direction through the inverse
	/// view-projection
	void DeprojectScreenToWorld(glm::vec2 screenCoord, glm::vec3& outWorldOrigin, glm::vec3& outWorldDirection,
	                            Interpolation interpolation = Camera::Interpolation::Current) const;
	/// The eye, and the point of the near plane a point of the screen (0 to 1 from the top left) shows
	struct NearPlanePoint
	{
		glm::vec3 eye;
		glm::vec3 point;
	};
	[[nodiscard]] NearPlanePoint OnNearPlane(glm::vec2 screenCoord,
	                                         Interpolation interpolation = Camera::Interpolation::Current) const;
	bool ProjectWorldToScreen(glm::vec3 worldPosition, glm::vec4 viewport, glm::vec3& outScreenPosition,
	                          Interpolation interpolation = Camera::Interpolation::Current) const;

	void Update(std::chrono::microseconds dt);
	/// The zoomers' part of the camera update, after the mode's Update: the mode's new destinations (the default mode
	/// sets them every frame with Zoomer3::SetDestinationWithTime), then each Zoomer::Update(min(dt, 0.1))
	void UpdateZoomers(const std::optional<CameraModel::CameraInterpolationUpdateInfo>& updateInfo, float seconds);
	void HandleActions(std::chrono::microseconds dt);
	/// The frame's controls are not read: the model is told (CameraModel::SkipActions)
	void SkipActions();

	[[nodiscard]] glm::mat4 GetRotationMatrix() const;
	[[nodiscard]] Projection GetCameraProjection() const;

	CameraModel& GetModel() { return *_model; }
	[[nodiscard]] const CameraModel& GetModel() const { return *_model; }
	/// Hands the camera's control to another model, as the temple does inside, giving back the one it had. The model given
	/// the camera is told it has none of this frame's controls (CameraModel::SkipActions)
	std::unique_ptr<CameraModel> SetModel(std::unique_ptr<CameraModel> model);
	/// (openblack engine) Every member the const getters read, into `out`: the zoomers, the shake, the drawn
	/// view and its place, the weather's point, the field of view, both projections and the one used; not the model. The copy
	/// of the camera the draw reads (Game::Run, at the end of the frame's logic)
	void CopyViewTo(Camera& out) const;

	/// How much faster than the game's own speed the movement keys move the camera, kept within the allowed range
	void SetKeyboardMoveSpeed(float speed);
	[[nodiscard]] float GetKeyboardMoveSpeed() const { return _keyboardMoveSpeed; }

	/// The helper icons by the hand the player's camera shows, as hand_tricons::icon with their offered bits: set each
	/// frame they are faded, kept while they are not (another camera has the view), and none again when the player's
	/// camera starts afresh. A script asks whether any is shown
	void SetShownHandIcons(uint32_t icons) { _shownHandIcons = icons; }
	[[nodiscard]] uint32_t GetShownHandIcons() const { return _shownHandIcons; }

protected:
	Zoomer3 _origin;
	Zoomer3 _focus;
	glm::vec3 _originDrawOffset {0.0f};  ///< the shake added to the drawn position
	glm::vec3 _focusDrawOffset {0.0f};   ///< the shake added to the drawn focus
	std::optional<glm::mat4> _drawnView; ///< the falling spell's view, while it (mode 2) draws
	std::optional<Place> _drawnPlace;    ///< that view's place, until the game camera is drawn again
	std::optional<glm::vec3> _weatherPoint;      ///< where the weather is read while the fall runs (SetWeatherPoint)
	std::optional<screen_point::View> _keptView; ///< the view the last update left (KeepDrawnView)
	float _xFov = 0.0f;                  // TODO(#707): This should be a zoomer for animations
	float _aspect = 1.0f;                ///< SetProjectionMatrixPerspective's aspect (W / H)
	float _nearClip = 1.0f;              ///< SetProjectionMatrixPerspective's near clip
	float _pixelNearClip = 3.5f;         ///< the last SetPixelNearClip, the game's starting one before it
	glm::mat4 _projectionMatrix = glm::mat4 {1.0f};
	glm::mat4 _projectionMatrixReversedZ = glm::mat4 {1.0f};
	std::unique_ptr<CameraModel> _model;
	Projection _cameraProjection = Projection::ReversedZ;
	float _keyboardMoveSpeed = k_KeyboardMoveSpeedDefault;
	uint32_t _shownHandIcons {0};
};

} // namespace openblack
