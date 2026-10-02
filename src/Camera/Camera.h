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
	[[nodiscard]] virtual glm::mat4 GetViewMatrix(Interpolation interpolation) const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix() const;
	[[nodiscard]] const glm::mat4& GetProjectionMatrix(Projection projection) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Interpolation interpolation = Camera::Interpolation::Current) const;
	[[nodiscard]] glm::mat4 GetViewProjectionMatrix(Projection projection,
	                                                Interpolation interpolation = Camera::Interpolation::Current) const;

	[[nodiscard]] std::optional<ecs::components::Transform>
	RaycastMouseToLand(bool includeWater = true, Interpolation interpolation = Camera::Interpolation::Current) const;
	[[nodiscard]] std::optional<ecs::components::Transform>
	RaycastScreenCoordToLand(glm::vec2 screenCoord, bool includeWater,
	                         Interpolation interpolation = Camera::Interpolation::Current) const;

	[[nodiscard]] glm::vec3 GetOrigin(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetOriginVelocity(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocus(Interpolation interpolation = Interpolation::Current) const;
	[[nodiscard]] glm::vec3 GetFocusVelocity(Interpolation interpolation = Interpolation::Current) const;

	/// Get rotation as euler angles in radians
	[[nodiscard]] glm::vec3 GetRotation() const;

	Camera& SetOrigin(const glm::vec3& position);
	Camera& SetFocus(const glm::vec3& position);

	/// GCamera's Zoomer3d of the position (+0x118) and of the focus (+0x88)
	[[nodiscard]] Zoomer3d& GetOriginZoomer() { return _origin; }
	[[nodiscard]] Zoomer3d& GetFocusZoomer() { return _focus; }
	[[nodiscard]] const Zoomer3d& GetOriginZoomer() const { return _origin; }
	[[nodiscard]] const Zoomer3d& GetFocusZoomer() const { return _focus; }
	/// The camera shake, fn_008210C0 in LH3DTech::UpdateCamera 0x819920 (from GCamera::Update 0x442622): a world-space
	/// translation of the DRAWN position and focus only, before they are copied to g_camera 0xEA1DB8 / 0xEA1DC4; the
	/// Zoomer3d are never touched, so it does not build up. GetOrigin / GetFocus (Current) and GetViewMatrix add it
	/// (GET_CAMERA_POSITION sees the shaking camera). Set every frame (camera_shake::Adjust), zero when there is none
	void SetDrawOffset(const glm::vec3& origin, const glm::vec3& focus)
	{
		_originDrawOffset = origin;
		_focusDrawOffset = focus;
	}
	/// The time of the zoomers since their last destination (the position's x Zoomer, CurrentTime +0x14)
	[[nodiscard]] std::chrono::microseconds GetInterpolatorTime() const;

	Camera& SetProjectionMatrixPerspective(float xFov, float aspect, float nearClip, float farClip);
	Camera& SetProjectionMatrix(const glm::mat4& projection);

	[[nodiscard]] glm::vec3 GetForward() const;
	[[nodiscard]] glm::vec3 GetRight() const;
	[[nodiscard]] glm::vec3 GetUp() const;

	[[nodiscard]] std::unique_ptr<Camera> Reflect() const;

	void DeprojectScreenToWorld(glm::vec2 screenCoord, glm::vec3& outWorldOrigin, glm::vec3& outWorldDirection,
	                            Interpolation interpolation = Camera::Interpolation::Current) const;
	bool ProjectWorldToScreen(glm::vec3 worldPosition, glm::vec4 viewport, glm::vec3& outScreenPosition,
	                          Interpolation interpolation = Camera::Interpolation::Current) const;

	void Update(std::chrono::microseconds dt);
	/// The zoomers' part of GCamera::Update 0x441F80, after the mode's Update (vt+8, 0x441FD9): the mode's new
	/// destinations (CameraModeNew3::Update sets them every frame with Zoomer3d::SetDestinationWithTime 0x44E760,
	/// 0x4604A4..0x4604D2), then each Zoomer::Update(min(dt, 0.1 [0x8AB22C])) (0x441FB0..0x442029)
	void UpdateZoomers(const std::optional<CameraModel::CameraInterpolationUpdateInfo>& updateInfo, float seconds);
	void HandleActions(std::chrono::microseconds dt);

	[[nodiscard]] glm::mat4 GetRotationMatrix() const;
	[[nodiscard]] Projection GetCameraProjection() const;

	CameraModel& GetModel() { return *_model; }
	[[nodiscard]] const CameraModel& GetModel() const { return *_model; }

protected:
	Zoomer3d _origin; ///< GCamera +0x118
	Zoomer3d _focus;  ///< GCamera +0x88
	glm::vec3 _originDrawOffset {0.0f}; ///< the shake added to the drawn position (fn_008210C0)
	glm::vec3 _focusDrawOffset {0.0f};  ///< the shake added to the drawn focus (fn_008210C0)
	float _xFov = 0.0f; // TODO(#707): This should be a zoomer for animations
	glm::mat4 _projectionMatrix = glm::mat4 {1.0f};
	glm::mat4 _projectionMatrixReversedZ = glm::mat4 {1.0f};
	std::unique_ptr<CameraModel> _model;
	Projection _cameraProjection = Projection::ReversedZ;
};

} // namespace openblack
