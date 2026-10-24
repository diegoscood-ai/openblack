/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

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
	/// view, the field of view, both projections and the one used; not the model. The copy of the camera the draw
	/// reads (Game::Run, at the end of the frame's logic)
	void CopyViewTo(Camera& out) const;

	/// How much faster than the game's own speed the movement keys move the camera, kept within the allowed range
	void SetKeyboardMoveSpeed(float speed);
	[[nodiscard]] float GetKeyboardMoveSpeed() const { return _keyboardMoveSpeed; }

protected:
	Zoomer3 _origin;
	Zoomer3 _focus;
	glm::vec3 _originDrawOffset {0.0f};  ///< the shake added to the drawn position
	glm::vec3 _focusDrawOffset {0.0f};   ///< the shake added to the drawn focus
	std::optional<glm::mat4> _drawnView; ///< the falling spell's view, while it (mode 2) draws
	float _xFov = 0.0f;                  // TODO(#707): This should be a zoomer for animations
	float _aspect = 1.0f;                ///< SetProjectionMatrixPerspective's aspect (W / H)
	float _nearClip = 1.0f;              ///< SetProjectionMatrixPerspective's near clip
	float _pixelNearClip = 3.5f;         ///< the last SetPixelNearClip, the game's starting one before it
	glm::mat4 _projectionMatrix = glm::mat4 {1.0f};
	glm::mat4 _projectionMatrixReversedZ = glm::mat4 {1.0f};
	std::unique_ptr<CameraModel> _model;
	Projection _cameraProjection = Projection::ReversedZ;
	float _keyboardMoveSpeed = k_KeyboardMoveSpeedDefault;
};

} // namespace openblack
