/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DefaultWorldCameraModel.h"

#include <array>
#include <numeric>
#include <utility>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Services/Confirmation.h"
#include "Camera.h"
#include "CameraFlight.h"
#include "CameraHelp.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "EngineConfig.h"
#include "FightOrbit.h"
#include "Help/HelpProfile.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "ScriptCamera.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace std::chrono_literals;

// TODO(#708): Add to global configurations
// A notch of the wheel zooms by this much, and so does a zoom key for each frame it is held without the wheel
constexpr auto k_WheelZoomPerNotch = 60.0f;
constexpr auto k_InteractionSpeedMultiplier = 400.0f;
// Vanilla black and white uses a pretty bad PI/2 approximation
constexpr auto k_CameraModelHalfPi = 1.53938043f;
constexpr auto k_RotateOnSpeedMultiplier = glm::vec2(1.9f, -1.7f);
constexpr auto k_TwoButtonZoomFactor = 1.9f;
// Both buttons turn the camera by the same amount for the mouse's movement across, once it has moved far enough
constexpr auto k_TwoButtonTurnFactor = 1.9f;
// Zoom and keyboard movement steps are scaled by three times the camera's height above its focus, within these bounds.
// Below the focus the zoom scale stops at k_ZoomScaleMaxBelowFocus.
constexpr auto k_StepScaleHeightFactor = 3.0f;
constexpr auto k_StepScaleMin = 60.0f;
constexpr auto k_ZoomScaleMax = 2000.0f;
constexpr auto k_ZoomScaleMaxBelowFocus = 4.0f * k_StepScaleMin;
// The tilt of a unit of pitch input, in radians
constexpr auto k_PitchPerInput = 0.002f;
constexpr auto k_CameraInteractionStepSize = 3.0f;
constexpr auto k_MinimalCameraAnimationDuration = 1'500'000us;
constexpr auto k_FlyingDistanceThresholds = std::array<float, 4> {100.0f, 60.0f, 30.0f, 15.0f};
constexpr auto k_FlyingThresholdFactor = 1.5f;
constexpr auto k_GroundDistanceMinimum = 10.0f;
// How much a flight's middle point rises per metre flown across the land: the bookmarks' and the other flights to a
// place, and the double click's
constexpr auto k_PlaceFlightRise = 0.3f;
constexpr auto k_DoubleClickFlightRise = 0.1f;
// The watched fight's flight does not rise
constexpr auto k_FightFlightRise = 0.0f;
// The clear view glides in and out over this long, and moves the camera once further in than this
constexpr auto k_ClearViewSeconds = 0.5f;
constexpr auto k_ClearViewShown = 0.01f;
// Its close view is this far from the hand's point: 50 for a camera further than 300 from its focus, 10 for one nearer
// than 50, 15 otherwise
constexpr auto k_ClearViewFarCamera = 300.0f;
constexpr auto k_ClearViewNearCamera = 50.0f;
constexpr auto k_ClearViewFromFar = 50.0f;
constexpr auto k_ClearViewFromNear = 10.0f;
constexpr auto k_ClearViewDistance = 15.0f;
// A drag given up flies the camera this far from the hand's point
constexpr auto k_GivenUpFlightDistance = 1000.0f;
// The time since a flight began starts here for a short double click flight and for the flight after a drag given up
constexpr auto k_ShortFlightStartSeconds = 1.5f * 0.5f;
constexpr auto k_GivenUpFlightStartSeconds = 0.01f;
// For the original 32 x 32 block maps: centred on the map, radius = its side (5120); BWLandEditor maps scale it
constexpr auto k_ConstrainDiscCentre = glm::vec3(2560.0f, 0.0f, 2560.0f);
constexpr auto k_ConstrainDiscRadius = 5120.0f;
constexpr auto k_MaxAltitude = 30'000.0f;
constexpr auto k_FloatingHeight = 2.9999f; // 3 in vanilla, but less due to fp precision with recorded data in tests

glm::vec3 EulerFromPoints(glm::vec3 p0, glm::vec3 p1)
{
	const auto diff = p0 - p1;
	// If the camera is directly above the focus point, set pitch to 90 degrees.
	if (glm::all(glm::lessThan(glm::abs(glm::xz(diff)), glm::vec2(0.1f, 0.1f))))
	{
		return {0.0f, k_CameraModelHalfPi, 0.0f};
	}
	// Otherwise, calculate yaw and pitch based on the direction to the focus point.
	return {glm::pi<float>() - glm::atan(diff.x, -diff.z), glm::atan(diff.y, glm::length(glm::xz(diff))), 0.0f};
}

/// Calculates the projection length of a vector onto another vector.
///
/// Given three points `p1`, `p2`, and `p3`, this function computes the projection length
/// of the vector from `p1` to `p3` onto the direction defined by the vector from `p1` to `p2`.
/// The result represents how much `p3` is "along" the direction from `p1` to `p2`, scaled
/// by the magnitude of the vector from `p1` to `p3`.
///
/// # Arguments
///
/// * `p1` - The origin point.
/// * `p2` - The point defining the direction vector.
/// * `p3` - The point whose projection onto the direction from `p1` to `p2` is calculated.
///
/// # Returns
///
/// The projection length of `p3` onto the direction from `p1` to `p2`.
float PointDistanceAlongLineSegment(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3)
{
	const glm::vec3 v12 = p2 - p1; // Vector from p1 to p2
	const glm::vec3 v13 = p3 - p1; // Vector from p1 to p3

	// Normalize v12 to get its direction
	const glm::vec3 u12 = glm::normalize(v12);

	// Project v13 onto u12 to get the projection length from p1
	return glm::abs(glm::dot(v13, u12)); // Using glm::abs to ensure the distance is non-negative
}

namespace
{
/// Whether the camera features allow one of them
[[nodiscard]] bool Allows(uint32_t features, camera_help::Feature feature)
{
	return (features & static_cast<uint32_t>(camera_help::Bit(feature))) != 0;
}
} // namespace

DefaultWorldCameraModel::DefaultWorldCameraModel()
    : DefaultWorldCameraModel(ecs::object::GetRadius)
{
}

DefaultWorldCameraModel::DefaultWorldCameraModel(FighterRadius fighterRadius)
    : _fighterRadius(std::move(fighterRadius))
{
}

DefaultWorldCameraModel::~DefaultWorldCameraModel() = default;

void DefaultWorldCameraModel::TiltZoom(glm::vec3& eulerAngles, float scalingFactor, float zoomDelta)
{
	// Update the camera's yaw if there's significant horizontal movement.
	if (glm::abs(_rotateAroundDelta.y) > glm::epsilon<float>())
	{
		const auto windowSize = Locator::windowing::value().GetSize();
		eulerAngles.x += _rotateAroundDelta.y * glm::pi<float>() / windowSize.x;
	}

	// Update the camera's pitch if there's significant vertical movement.
	if (glm::abs(_rotateAroundDelta.x) > glm::epsilon<float>())
	{
		const auto pitchStep = _rotateAroundDelta.x * k_PitchPerInput;
		eulerAngles.y -= pitchStep;
		// Clamp the pitch angle to keep the camera within between -30 and 78.75 degrees.
		eulerAngles.y = glm::clamp(eulerAngles.y, -1.0f / 6.0f * glm::pi<float>(), 7.0f / 16.0f * glm::pi<float>());
		if (_mode == Mode::ArcBall)
		{
			const auto distanceFromBound = (2.0f * k_CameraInteractionStepSize) - _distanceFromBoundY;
			if (distanceFromBound > glm::epsilon<float>())
			{
				const auto verticalStep = distanceFromBound * pitchStep * _focusDistance * 0.051f;
				_targetFocus += verticalStep;
				_focusAtClick += verticalStep;
			}
		}
	}

	{
		// Compute a keyboard input offset based on the current pitch and yaw.
		const auto clampedTan = glm::clamp(glm::tan(eulerAngles.y), 0.2f, 2.0f);
		const auto localMovement = _keyBoardMoveDelta * glm::vec2(1.0f / clampedTan, -1.0f) * scalingFactor * 0.001f;
		const auto planarMovement = glm::vec2(glm::cos(-eulerAngles.x), glm::sin(-eulerAngles.x));
		const auto offset = glm::vec3(localMovement.y * planarMovement.x - localMovement.x * planarMovement.y, 0.0,
		                              glm::dot(localMovement, planarMovement));
		_targetOrigin += offset;
		_targetFocus += offset;
		_focusAtClick += offset;
	}

	_averageIslandDistance += zoomDelta;
	_averageIslandDistance = glm::max(_averageIslandDistance, k_CameraInteractionStepSize + 0.1f);
}

float DefaultWorldCameraModel::GetVerticalLineInverseDistanceWeighingRayCast(const Camera& camera) const
{
	std::vector<float> inverseHitDistances;
	inverseHitDistances.reserve(0x10);

	for (int i = 0; i < 0x10; ++i)
	{
		const glm::vec2 coord = glm::vec2(0.5f, i / 16.0f);

		if (const auto hit = camera.RaycastScreenCoordToLand(coord, false, Camera::Interpolation::Target))
		{
			inverseHitDistances.push_back(1.0f / glm::length(hit->position - _targetOrigin));
		}
	}

	// Default distance of 50 if no hits happen, therefore divide by size + 1
	const auto average = std::accumulate(inverseHitDistances.cbegin(), inverseHitDistances.cend(), 1.0f / 50.0f) /
	                     (inverseHitDistances.size() + 1);
	return 1.0f / average;
}

void DefaultWorldCameraModel::ComputeDistanceFromBoundY()
{
	if (_targetOrigin.y < _targetFocus.y)
	{
		_distanceFromBoundY = 0.0f;
	}
	else
	{
		const auto groundAltitude = Locator::terrainSystem::value().GetHeightAt(glm::xz(_targetOrigin));
		const auto boundsMinY = groundAltitude + k_CameraInteractionStepSize;
		const auto boundsMaxY = glm::min(groundAltitude, 0.0f) + k_MaxAltitude;
		_distanceFromBoundY = glm::min(glm::abs(_targetOrigin.y - boundsMinY), glm::abs(_targetOrigin.y - boundsMaxY));
	}
}

bool DefaultWorldCameraModel::ConstrainCamera(std::chrono::microseconds dt, float mouseMovementDistance, glm::vec3 eulerAngles,
                                              const Camera& camera)
{
	const auto originBackup = _targetOrigin;
	bool originHasBeenAdjusted = false;
	originHasBeenAdjusted |= ConstrainAltitude();
	originHasBeenAdjusted |= ConstrainDisc();

	const auto dtSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
	const auto threshold = glm::max(300.0f, 1.6f * _focusDistance * dtSeconds);
	if ((mouseMovementDistance == 0.0f && glm::distance2(_targetOrigin, originBackup) > threshold * threshold) ||
	    originHasBeenAdjusted)
	{
		if (_mode != Mode::DraggingLandscape)
		{
			_originFocusDistanceAtInteractionStart =
			    glm::max(glm::distance(_targetOrigin, _targetFocus), k_CameraInteractionStepSize + 0.1f);
			UpdateFocusPointInteractionParameters(_targetOrigin, _targetFocus, eulerAngles, camera);
		}
	}

	return originHasBeenAdjusted;
}

bool DefaultWorldCameraModel::ConstrainAltitude()
{
	bool hasBeenAdjusted = false;
	const auto minAltitude = k_FloatingHeight + Locator::terrainSystem::value().GetHeightAt(glm::xz(_targetOrigin));
	if (_targetOrigin.y < minAltitude)
	{
		_targetOrigin.y = minAltitude;
		hasBeenAdjusted = true;
	}

	if (_mode != Mode::ArcBall && _rotateAroundDelta.x != 0.0f)
	{
		_targetFocus = _targetOrigin + glm::normalize(_targetFocus - _targetOrigin) * _focusDistance;
	}

	return hasBeenAdjusted;
}

bool DefaultWorldCameraModel::ConstrainDisc()
{
	bool hasBeenAdjusted = false;

	const float scale = static_cast<float>(Locator::terrainSystem::value().GetCellsPerSide()) / 512.0f;
	const auto centre = k_ConstrainDiscCentre * scale;
	const auto radius = k_ConstrainDiscRadius * scale;
	const auto delta = _targetOrigin - centre;
	const auto distance2 = glm::length2(delta);

	if (distance2 > radius * radius)
	{
		_targetOrigin = centre + delta * (radius / glm::sqrt(distance2));
		hasBeenAdjusted = true;
	}

	return hasBeenAdjusted;
}

void DefaultWorldCameraModel::UpdateFocusPointInteractionParameters(glm::vec3 origin, glm::vec3 focus, glm::vec3 eulerAngles,
                                                                    const Camera& camera)
{
	_focusAtClick = _targetFocus;
	_screenSpaceMouseRaycastHitAtClick =
	    camera.RaycastMouseToLand(true, Camera::Interpolation::Target).and_then(ecs::components::GetTransformPosition);
	if (_screenSpaceMouseRaycastHitAtClick.has_value())
	{
		_arcBallRadius = PointDistanceAlongLineSegment(origin, focus, *_screenSpaceMouseRaycastHitAtClick);
	}
	_originAtClick = _targetOrigin;
	_mouseAtClick = Locator::gameActionSystem::value().GetMousePosition();
	_originFocusDistanceAtInteractionStart = glm::distance(origin, focus);
	// Dragging the land, the camera, the cursor and the plane are those of the press
	if (_mode == Mode::DraggingLandscape && _dragging && _landGrip.has_value())
	{
		_originAtClick = _landGrip->origin;
		_focusAtClick = _landGrip->focus;
		_mouseAtClick = _landGrip->cursor;
		_originToHandPlaneNormal = _landGrip->plane.normal;
		_alignmentAtInteractionStart = _landGrip->plane.distance;
		_originFocusDistanceAtInteractionStart = glm::distance(_landGrip->origin, _landGrip->focus);
	}
	_averageIslandDistance = GetVerticalLineInverseDistanceWeighingRayCast(camera);
	{
		const auto diff = _targetOrigin - _targetFocus;
		// If the camera is directly above the focus point, set pitch to 90 degrees.
		if (glm::all(glm::lessThan(glm::abs(glm::xz(diff)), glm::vec2(0.1f, 0.1f))))
		{
			eulerAngles = glm::vec3(0.0f, glm::half_pi<float>(), 0.0f);
		}
		// Otherwise, calculate yaw and pitch based on the direction to the focus point.
		else
		{
			eulerAngles =
			    glm::vec3(glm::pi<float>() - glm::atan(diff.x, -diff.z), glm::atan(diff.y, glm::length(glm::xz(diff))), 0.0f);
		}
	}
	const auto extra =
	    (_focusDistance - _averageIslandDistance) * glm::clamp(eulerAngles.y / (6.0f * glm::pi<float>()), 0.0f, 1.0f);
	_averageIslandDistance += extra;
}

void DefaultWorldCameraModel::UpdateMode(const Camera& camera, glm::vec3 eulerAngles, float zoomDelta, glm::uvec2 mouseCurrent)
{
	switch (_mode)
	{
	case Mode::Cartesian:
		UpdateModeCartesian();
		break;
	case Mode::Polar:
		UpdateModePolar(eulerAngles, zoomDelta == 0.0f);
		break;
	case Mode::ArcBall:
		UpdateModeArcBall(eulerAngles, mouseCurrent, camera.GetHorizontalFieldOfView());
		break;
	case Mode::DraggingLandscape:
		// A drag given up flies to the hand's point at once, as the double click does, when the flight is allowed
		if (UpdateModeDragging(camera, mouseCurrent) && Allows(_features, camera_help::Feature::DoubleClickFly))
		{
			ReportFlightToHand();
			UpdateModeFlying(eulerAngles, true);
		}
		break;
	case Mode::FlyingToPoint:
		UpdateModeFlying(eulerAngles);
		break;
	}

	if (_mode != Mode::ArcBall && _rotateAroundDelta.x != 0.0f)
	{
		const auto diff = _targetFocus - _targetOrigin;
		_targetOrigin = _currentOrigin;
		_targetFocus = _targetOrigin + diff;
		_focusAtClick = _targetFocus;
	}
}

void DefaultWorldCameraModel::UpdateModeCartesian()
{
	// Drag focus on land
	const auto dist =
	    _screenSpaceCenterRaycastHit.has_value() ? glm::distance(*_screenSpaceCenterRaycastHit, _targetOrigin) : _focusDistance;
	_targetFocus = ProjectPointOnForwardVector(glm::max(dist - 1.0f, 0.1f));
	_focusAtClick = _targetFocus;
}

void DefaultWorldCameraModel::UpdateModePolar(glm::vec3 eulerAngles, bool recalculatePoint)
{
	// Pitch should already be clamped in TiltZoom

	// Put average distance point on half-line from camera
	const auto averageIslandPoint = _targetOrigin + _averageIslandDistance * glm::normalize(GetTargetForwardVector());

	// Rotate camera origin based on euler angles as polar coordinates and focus and distance at interaction
	_targetOrigin = script_camera::PointFromDistanceHeadingAndPitch(_focusAtClick, _originFocusDistanceAtInteractionStart,
	                                                                eulerAngles.x, eulerAngles.y);

	if (recalculatePoint)
	{
		const auto diff = averageIslandPoint - ProjectPointOnForwardVector(_averageIslandDistance);
		_targetOrigin += diff;
		_focusAtClick += diff;
	}
	_targetFocus = _focusAtClick;
}

void DefaultWorldCameraModel::UpdateModeArcBall(glm::vec3 eulerAngles, glm::u16vec2 mouseCurrent, float xFov)
{
	if (!_screenSpaceMouseRaycastHitAtClick.has_value())
	{
		return;
	}
	const auto point = script_camera::PointFromDistanceHeadingAndPitch(_focusAtClick, _originFocusDistanceAtInteractionStart,
	                                                                   eulerAngles.x, eulerAngles.y);

	const auto basisZ = glm::normalize(point - _focusAtClick);
	const auto basisX = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), basisZ));
	const auto basisY = glm::normalize(glm::cross(basisZ, basisX));

	const auto windowSize = static_cast<glm::vec2>(Locator::windowing::value().GetSize());
	const auto aspect = windowSize.y / windowSize.x;
	const auto screenCentreOffset = static_cast<glm::vec2>(mouseCurrent) / windowSize - 0.5f;

	const auto halfTanX = glm::tan(xFov * 0.5f);
	const auto halfTanY = halfTanX * aspect;

	const auto cameraOffsetX = screenCentreOffset.x * halfTanX * basisX * _arcBallRadius * 2.0f;
	const auto cameraOffsetY = screenCentreOffset.y * halfTanY * basisY * _arcBallRadius * 2.0f;

	_targetFocus = _screenSpaceMouseRaycastHitAtClick.value() + cameraOffsetX + cameraOffsetY;
	_targetOrigin = _targetFocus + basisZ * _arcBallRadius;
	_focusAtClick = _targetFocus;
}

bool DefaultWorldCameraModel::UpdateModeDragging(const Camera& camera, glm::u16vec2 mouseCurrent)
{
	if (!_landGrip.has_value())
	{
		return false;
	}
	// Land gripped too far ahead isn't dragged: the zoom hint blinks and every control is dropped until all are let go
	if (_landGrip->depth > camera_pan::k_MaxGripDepth)
	{
		_dragGivenUp = true;
		_dragging = false;
		_tricons = camera_drag::tricon::k_GivenUp;
		return true;
	}
	// Panning needs the land grab, and the land under the cursor now and where the land was gripped
	if (!Allows(_features, camera_help::Feature::GrabLand) || !_screenSpaceMouseRaycastHit.has_value() || !_landGrip->land)
	{
		return false;
	}

	// The lines of sight through the cursor now and at the grip, both from the camera as it was at the grip
	const auto screenSize = glm::vec2(Locator::windowing::value().GetSize());
	const auto inverseViewProjection = glm::inverse(camera.GetProjectionMatrix(Camera::Projection::Normal) *
	                                                glm::lookAt(_originAtClick, _focusAtClick, glm::vec3(0.0f, 1.0f, 0.0f)));
	const auto rayThrough = [&inverseViewProjection, screenSize](glm::vec2 cursor) {
		const auto ndc = glm::vec2((cursor.x / screenSize.x - 0.5f) * 2.0f, ((1.0f - cursor.y / screenSize.y) - 0.5f) * 2.0f);
		const auto near = inverseViewProjection * glm::vec4(ndc, 0.0f, 1.0f);
		const auto far = inverseViewProjection * glm::vec4(ndc, 0.5f, 1.0f);
		return glm::normalize(glm::vec3(far) / far.w - glm::vec3(near) / near.w);
	};
	const auto place =
	    camera_pan::Pan({.normal = _originToHandPlaneNormal, .distance = _alignmentAtInteractionStart}, _originAtClick,
	                    _focusAtClick, _originFocusDistanceAtInteractionStart, rayThrough(glm::vec2(mouseCurrent)),
	                    rayThrough(glm::vec2(_mouseAtClick)), glm::ivec2(mouseCurrent), glm::ivec2(_mouseAtClick));
	if (!place.has_value())
	{
		return false;
	}
	// The camera help counts the drag in each frame of the pan with the camera or the mouse moving. (pending) the input
	// mask, which none of the camera's help calls passes yet
	if (_dragCountsForHelp)
	{
		help_profile::CameraHelpCallback(help_profile::CameraReason::Drag, 0);
	}

	// The camera stops short of land in its way, or of the sea, as the drawn camera sees it
	const auto stopped = Locator::terrainSystem::has_value()
	                         ? StopPanShortOfLand(Locator::terrainSystem::value(), *place, _originAtClick, camera.GetOrigin())
	                         : *place;
	_targetOrigin = stopped.origin;
	_targetFocus = stopped.focus;
	return false;
}

camera_pan::CameraPlace DefaultWorldCameraModel::StopPanShortOfLand(const LandIslandInterface& island,
                                                                    const camera_pan::CameraPlace& place,
                                                                    glm::vec3 originAtGrip, glm::vec3 drawnCamera)
{
	// The land's ray cast, from the camera at the grip through its new origin; without land, the sea where the line
	// going down meets it, within 7500 across from the drawn camera
	if (place.origin == originAtGrip)
	{
		return place;
	}
	glm::vec2 hit(0.0f);
	if (!island.RayCast(originAtGrip, place.origin, hit, drawnCamera))
	{
		return place;
	}
	return camera_pan::StopShortOfLand(place, originAtGrip, glm::vec3(hit.x, 0.0f, hit.y));
}

void DefaultWorldCameraModel::UpdateModeFlying(glm::vec3 eulerAngles, bool afterGivenUpDrag)
{
	if (!_screenSpaceCenterRaycastHit.has_value())
	{
		return;
	}
	const auto point = _handPosition.value_or(_screenSpaceCenterRaycastHit.value_or(glm::zero<glm::vec3>()));

	const auto distanceFromHitPoint = glm::length(point - *_screenSpaceCenterRaycastHit);
	const auto distanceFromOrigin = glm::length(point - _targetOrigin);

	const auto thresholdDistance = distanceFromOrigin / k_FlyingThresholdFactor;

	const bool wooshingDistance =
	    thresholdDistance > k_FlyingDistanceThresholds[0] && distanceFromHitPoint > k_GroundDistanceMinimum;
	float distanceFromFocus;
	if (wooshingDistance)
	{
		distanceFromFocus = k_FlyingDistanceThresholds[0];
	}
	else if (thresholdDistance > k_FlyingDistanceThresholds[1])
	{
		distanceFromFocus = k_FlyingDistanceThresholds[1];
	}
	else if (thresholdDistance > k_FlyingDistanceThresholds[2])
	{
		distanceFromFocus = k_FlyingDistanceThresholds[2];
	}
	else
	{
		distanceFromFocus = k_FlyingDistanceThresholds[3];
	}
	// After a drag given up: 1000 away, keeping the camera's heading
	if (afterGivenUpDrag)
	{
		distanceFromFocus = k_GivenUpFlightDistance;
	}
	_flightSeconds = afterGivenUpDrag ? k_GivenUpFlightStartSeconds : (wooshingDistance ? 0.0f : k_ShortFlightStartSeconds);

	// Every flight but the one after a drag given up turns to the best side of the point at its own distance (not
	// shaped), from the camera's heading, and takes the pitch that goes with it
	if (!afterGivenUpDrag)
	{
		float pitch = eulerAngles.y;
		eulerAngles.x =
		    camera_flight::FindBestAngle(Locator::terrainSystem::value(), eulerAngles.x, distanceFromFocus, point, pitch);
		eulerAngles.y = pitch;
	}
	eulerAngles.y = glm::clamp(eulerAngles.y, glm::pi<float>() / 8.0f, glm::pi<float>() * 10.0f / 21.0f);

	_targetFocus = point;
	_targetOrigin =
	    script_camera::PointFromDistanceHeadingAndPitch(_targetFocus, distanceFromFocus, eulerAngles.x, eulerAngles.y);

	// The double click's flight whooshes without SetFlight's own test. It does so with the focus distance 100 when one
	// distance is more than 100 * 1.5 and the other more than 10 (the test below), and also after a drag given up, with
	// the focus distance 1000. That one goes straight there, with no flight over a midpoint
	if (afterGivenUpDrag)
	{
		PlayWoosh();
		return;
	}
	if (wooshingDistance)
	{
		_flightPath = CharterFlight(Locator::terrainSystem::value(), _targetOrigin, _targetFocus, _currentOrigin,
		                            k_DoubleClickFlightRise);
		PlayWoosh();
	}
}

void DefaultWorldCameraModel::UpdateCameraInterpolationValues(const Camera& camera)
{
	// Get current curve interpolated values from camera: the model reads the camera's zoomers, not the shaken camera
	// (the shake only moves the drawn camera), so the shake never feeds back into the model
	_currentOrigin = camera.GetOriginZoomer().GetCurrentValue();
	_currentFocus = camera.GetFocusZoomer().GetCurrentValue();
	_targetOrigin = camera.GetOrigin(Camera::Interpolation::Target);
	_targetFocus = camera.GetFocus(Camera::Interpolation::Target);
}

void DefaultWorldCameraModel::UpdateRaycastHitPoints(const Camera& camera)
{
	// Raycast mouse and screen center
	{
		const auto hit = camera.RaycastMouseToLand(false, Camera::Interpolation::Target);
		_screenSpaceMouseRaycastHit = hit.and_then(ecs::components::GetTransformPosition);
	}
	{
		const auto hit = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, false, Camera::Interpolation::Target);
		_screenSpaceCenterRaycastHit = hit.and_then(ecs::components::GetTransformPosition);
	}
}

float DefaultWorldCameraModel::GetZoomScale() const
{
	// From the height between the camera and its focus, where they are going: 3 x the difference, at least 60, at most
	// 240 below the focus and 2000 above it
	const float origin = _targetOrigin.y;
	const float focus = _targetFocus.y;
	if (origin < focus)
	{
		const float scale = (focus - origin) * k_StepScaleHeightFactor;
		return scale <= k_StepScaleMin ? k_StepScaleMin : (scale < k_ZoomScaleMaxBelowFocus ? scale : k_ZoomScaleMaxBelowFocus);
	}
	const float scale = (origin - focus) * k_StepScaleHeightFactor;
	return scale <= k_StepScaleMin ? k_StepScaleMin : (scale < k_ZoomScaleMax ? scale : k_ZoomScaleMax);
}

void DefaultWorldCameraModel::UpdateFocusDistance()
{
	_focusDistance =
	    _screenSpaceCenterRaycastHit
	        .and_then([this](auto hit) -> std::optional<float> { return glm::max(10.0f, glm::distance(hit, _targetOrigin)); })
	        .value_or(glm::max(10.0f, _averageIslandDistance));
}

std::optional<CameraModel::CameraInterpolationUpdateInfo> DefaultWorldCameraModel::Update(std::chrono::microseconds dt,
                                                                                          const Camera& camera)
{
	_elapsedTime += dt;
	const float frameSeconds = std::chrono::duration<float>(dt).count();
	UpdateFightWatch(dt);

	UpdateCameraInterpolationValues(camera);
	// While a fight is watched and the land wasn't gripped, the camera turns about where it looks
	if (_fight.watching && _fight.arena.has_value() && !_grippedBefore)
	{
		_focusAtClick = _targetFocus;
	}
	const auto originAtFrameStart = _targetOrigin;
	UpdateRaycastHitPoints(camera);
	UpdateFocusDistance();

	// A drag just pressed grips the land under the cursor, or the focus with no land there
	if (_dragging && !_landGrip.has_value())
	{
		const auto gripped = _screenSpaceMouseRaycastHit.value_or(_targetFocus);
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt(glm::xz(_targetOrigin))
		                        : gripped.y;
		_landGrip = LandGrip {
		    .origin = _targetOrigin,
		    .focus = _targetFocus,
		    .cursor = glm::u16vec2(Locator::gameActionSystem::value().GetMousePosition()),
		    .plane = camera_pan::PlaneThrough(gripped, _targetOrigin, ground),
		    .land = _screenSpaceMouseRaycastHit.has_value(),
		    .depth = PointDistanceAlongLineSegment(_targetOrigin, _targetFocus, gripped),
		};
	}

	// Get angles (yaw, pitch, roll). Roll is always 0
	glm::vec3 eulerAngles = EulerFromPoints(_targetOrigin, _focusAtClick);

	// Dragging the land away from a watched fight stops watching it, and the camera is gripped as it is now
	if (DraggedAwayFromFight())
	{
		EndFightNow();
		UpdateFocusPointInteractionParameters(_targetOrigin, _targetFocus, eulerAngles, camera);
	}

	if (_mode != _modePrev && _mode != Mode::FlyingToPoint && _modePrev != Mode::FlyingToPoint)
	{
		UpdateFocusPointInteractionParameters(camera.GetOrigin(Camera::Interpolation::Target),
		                                      camera.GetFocus(Camera::Interpolation::Target), eulerAngles, camera);
	}

	ComputeDistanceFromBoundY();

	// The features SET_INTERFACE_INTERACTION allows. Without Zoom the zoom input is 0, without Rotate the rotation is
	// skipped, without Pitch the pitch. The scripts' turns run between the controls and the update, which reads them
	// again
	_features = CameraFeatures();
	if (!Allows(_features, camera_help::Feature::Zoom))
	{
		_rotateAroundDelta.z = 0.0f;
	}
	if (!Allows(_features, camera_help::Feature::Rotate))
	{
		_rotateAroundDelta.y = 0.0f;
		_dragTurnAndTilt.x = 0.0f;
	}
	if (!Allows(_features, camera_help::Feature::Pitch) && !_autoTilting)
	{
		_rotateAroundDelta.x = 0.0f;
		_dragTurnAndTilt.y = 0.0f;
	}
	// A watched fight's turn and tilt take the player's; a tilt dragged from the screen's edge goes the other way. Outside
	// a fight they are set again when one starts
	if (_fight.watching)
	{
		if (_rotateAroundDelta.y != 0.0f && Locator::windowing::has_value())
		{
			_fight.yaw += _rotateAroundDelta.y * glm::pi<float>() / static_cast<float>(Locator::windowing::value().GetSize().x);
		}
		_fight.pitch -= (_rotateAroundDelta.x - _dragTurnAndTilt.y) * k_PitchPerInput;
		_fight.pitch += _dragTurnAndTilt.y * k_PitchPerInput;
	}

	// Get step size
	const auto scalingFactor = k_StepScaleMin;
	const auto zoomDelta = _rotateAroundDelta.z * 0.0015f * GetZoomScale();

	if (_mode == Mode::Polar || _mode == Mode::ArcBall)
	{
		// The player's zoom, rotate and pitch of this frame go to the camera help (the help events 25..29 of
		// GET_TOTAL_EVENTS, counted once a turn); a drag round the edge or up and down made its own reports, and the
		// self-tilting camera's tilt is not the player's. (pending) the input mask (keyboard, mouse buttons, wheel)
		help_profile::OnPlayerCameraMove(_rotateAroundDelta.y - _dragTurnAndTilt.x,
		                                 _autoTilting ? 0.0f : _rotateAroundDelta.x - _dragTurnAndTilt.y, zoomDelta, 0);
		// The confirmation sound's feeds: the turn and the tilt of this frame over the frame's seconds; 0 on the frames
		// without them
		{
			const float seconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
			const auto width =
			    static_cast<float>(Locator::windowing::has_value() ? Locator::windowing::value().GetSize().x : 800);
			if (seconds > 0.0f)
			{
				audio::confirmation::FeedAngle(_rotateAroundDelta.y * glm::pi<float>() / width, seconds);
				audio::confirmation::FeedPitch(_rotateAroundDelta.x * k_PitchPerInput, seconds);
			}
		}
		// Adjust camera's orientation based on user input. Call will reset deltas.
		TiltZoom(eulerAngles, scalingFactor, zoomDelta);
	}
	else if (const float seconds = std::chrono::duration_cast<std::chrono::duration<float>>(dt).count(); seconds > 0.0f)
	{
		// A frame without a turn or a tilt feeds 0 to both
		audio::confirmation::FeedAngle(0.0f, seconds);
		audio::confirmation::FeedPitch(0.0f, seconds);
	}

	const auto mouseCurrent = Locator::gameActionSystem::value().GetMousePosition();
	_originFocusDistanceAtInteractionStart =
	    glm::max(_originFocusDistanceAtInteractionStart + zoomDelta, k_CameraInteractionStepSize + 0.1f);
	// Zooming while the land is gripped takes the grip further ahead or nearer, as far as it goes
	if (_landGrip.has_value() && Allows(_features, camera_help::Feature::Zoom))
	{
		_landGrip->depth = glm::max(_landGrip->depth + zoomDelta, k_CameraInteractionStepSize + 0.1f);
	}
	const auto mouseMovementDistance =
	    glm::max(glm::distance(static_cast<glm::vec2>(mouseCurrent), static_cast<glm::vec2>(_mouseAtClick)) *
	                 _originFocusDistanceAtInteractionStart * 0.11f,
	             50.0f);

	// A watched fight puts the camera round its fighters instead of the controls' mode, unless only the land is dragged,
	// or nothing is gripped and a keyboard move with the land grab allowed takes the camera this frame
	_fight.easeSeconds = 0.0f;
	const bool keyboardMoveStands =
	    Allows(_features, camera_help::Feature::GrabLand) && !_gripping && _keyBoardMoveDelta != glm::vec2();
	if (!_fight.watching || _gripOnly || keyboardMoveStands || !FollowFight(zoomDelta, frameSeconds))
	{
		UpdateMode(camera, eulerAngles, zoomDelta, mouseCurrent);
	}

	const bool originHasBeenAdjusted = ConstrainCamera(dt, mouseMovementDistance, eulerAngles, camera);

	// The self-tilting camera keeps to its height over the land in the frames it tilts or nothing is done: it stays
	// where it was at the frame's start, that height above the land there, looking the way it now does
	if (Allows(_features, camera_help::Feature::AutoPitch) && (_autoTilting || _idleMouse))
	{
		const auto ground = Locator::terrainSystem::has_value()
		                        ? Locator::terrainSystem::value().GetHeightAt(glm::xz(originAtFrameStart))
		                        : 0.0f;
		const auto origin = glm::vec3(originAtFrameStart.x, ground + camera_help::GetAutoPitchDistance(), originAtFrameStart.z);
		_targetFocus += origin - _targetOrigin;
		_targetOrigin = origin;
		_focusAtClick = _targetFocus;
	}

	// The clear view, over everything else: as far from the view it started from to its close view as it has come
	if (_clearView.value > k_ClearViewShown && _clearViewPlan.toOrigin.has_value())
	{
		const auto share = _clearView.value;
		_targetOrigin = (*_clearViewPlan.toOrigin - _clearViewPlan.fromOrigin) * share + _clearViewPlan.fromOrigin;
		_targetFocus = (_clearViewPlan.toFocus - _clearViewPlan.fromFocus) * share + _clearViewPlan.fromFocus;
	}

	_flightSeconds += frameSeconds;
	return ComputeUpdateReturnInfo(originHasBeenAdjusted, camera.GetInterpolatorTime());
}

std::optional<CameraModel::CameraInterpolationUpdateInfo>
DefaultWorldCameraModel::ComputeUpdateReturnInfo(bool originHasBeenAdjusted, std::chrono::microseconds t)
{
	if (!_flightPath.has_value())
	{
		constexpr auto k_TimeThreshold = 1'500'000us;
		auto duration = 300'000us;
		if (originHasBeenAdjusted)
		{
			duration *= 2;
		}
		if (_elapsedTime <= k_TimeThreshold)
		{
			duration =
			    std::chrono::microseconds {glm::mix(k_MinimalCameraAnimationDuration.count(), duration.count(),
			                                        (static_cast<float>(_elapsedTime.count()) / k_TimeThreshold.count()))};
		}
		// The orbit round a watched fight eases the camera in over its own time
		if (_fight.easeSeconds != 0.0f)
		{
			duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<float>(_fight.easeSeconds));
		}
		return {{GetTargetOrigin(), GetTargetFocus(), duration}};
	}
	if (t > k_MinimalCameraAnimationDuration / 2)
	{
		std::optional<FlightPath> backup = std::nullopt;
		std::swap(backup, _flightPath);
		_elapsedTime = decltype(_elapsedTime)::zero();
		// The flight's last leg starts the time since the flight began again
		_flightSeconds = 0.0f;
		return {{backup->origin, backup->focus, k_MinimalCameraAnimationDuration}};
	}
	if (_flightPath->midpoint.has_value())
	{
		std::optional<glm::vec3> backup = std::nullopt;
		std::swap(backup, _flightPath->midpoint);
		return {{*backup, _flightPath->focus,
		         std::chrono::duration_cast<std::chrono::microseconds>(k_MinimalCameraAnimationDuration * 0.9f)}};
	}
	return std::nullopt;
}

void DefaultWorldCameraModel::HandleActions(std::chrono::microseconds dt)
{
	_rotateAroundDelta = glm::vec3();
	_keyBoardMoveDelta = glm::vec2();
	_dragTurnAndTilt = glm::vec2();

	// Compute delta position (dp) based on the elapsed time and speed.
	const auto dp = k_InteractionSpeedMultiplier * std::chrono::duration_cast<std::chrono::duration<float>>(dt).count();
	const auto& actionSystem = Locator::gameActionSystem::value();
	// Moving over the land can be sped up or slowed down; turning, tilting and zooming keep the game's speed
	const auto moveSpeed =
	    Locator::camera::has_value() ? Locator::camera::value().GetKeyboardMoveSpeed() : k_KeyboardMoveSpeedDefault;
	const auto moveDp = ScaleKeyboardMove(dp, moveSpeed);
	// What the scripts let the player do
	_features = CameraFeatures();

	// TODO(#709): Set tricons based on ZOOM or TILT if any move is detected

	if (actionSystem.GetAny(input::BindableActionMap::ROTATE_LEFT, input::BindableActionMap::ROTATE_RIGHT))
	{
		const float distance = (actionSystem.Get(input::BindableActionMap::ROTATE_LEFT) ? -1.0f : 1.0f) * dp;
		_rotateAroundDelta.y += distance;
	}

	if (actionSystem.GetAny(input::BindableActionMap::TILT_UP, input::BindableActionMap::TILT_DOWN))
	{
		const float distance = (actionSystem.Get(input::BindableActionMap::TILT_DOWN) ? -1.0f : 1.0f) * dp;
		_rotateAroundDelta.x += distance;
	}

	if (actionSystem.GetAny(input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS))
	{
		const float direction = actionSystem.Get(input::BindableActionMap::MOVE_FORWARDS) ? -1.0f : 1.0f;
		const float distance = direction * dp;
		// If ZOOM_ON is active, apply the movement as a zoom action.
		if (actionSystem.Get(input::BindableActionMap::ZOOM_ON))
		{
			_rotateAroundDelta.z += distance;
		}
		// If ROTATE_ON is active, apply the movement as a tilt action.
		// TODO(#710): fight will always be rotating
		else if (actionSystem.Get(input::BindableActionMap::ROTATE_ON))
		{
			_rotateAroundDelta.x += distance;
		}
		// Otherwise, apply the movement normally.
		else
		{
			_keyBoardMoveDelta.x += direction * moveDp;
		}
	}

	if (actionSystem.GetAny(input::BindableActionMap::MOVE_RIGHT, input::BindableActionMap::MOVE_LEFT))
	{
		const float direction = actionSystem.Get(input::BindableActionMap::MOVE_RIGHT) ? -1.0f : 1.0f;
		const float distance = direction * dp;
		// If ZOOM_ON is active, apply the movement as a zoom action.
		// If ROTATE_ON is active, apply the movement as a tilt action.
		// TODO(#710): fight will always be rotating
		if (actionSystem.GetAny(input::BindableActionMap::ZOOM_ON, input::BindableActionMap::ROTATE_ON))
		{
			_rotateAroundDelta.y += distance;
		}
		// Otherwise, apply the movement normally.
		else
		{
			_keyBoardMoveDelta.y -= direction * moveDp;
		}
	}

	const auto handPositions = actionSystem.GetHandPositions();
	_handPosition = handPositions[0].or_else([handPositions] { return handPositions[1]; });

	// Ctrl and Shift held together glide the camera over half a second from where it is to a close view of the hand's
	// point, and let go glide it back
	{
		const auto seconds = std::chrono::duration<float>(dt).count();
		if (actionSystem.Get(input::BindableActionMap::ZOOM_ON) && actionSystem.Get(input::BindableActionMap::ROTATE_ON))
		{
			if (_clearView.value < k_ClearViewShown)
			{
				PlanClearView();
			}
			if (_clearViewPlan.point.has_value())
			{
				auto& point = *_clearViewPlan.point;
				point.y =
				    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(point)) : point.y;
				_clearViewPlan.toFocus = point;
				_clearViewPlan.toOrigin = script_camera::PointFromDistanceHeadingAndPitch(
				    point, _clearViewPlan.distance, _clearViewPlan.heading, _clearViewPlan.pitch);
			}
			_clearView.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_ClearViewSeconds);
		}
		else
		{
			_clearView.SetDestinationWithSpeedAndTime(0.0f, 0.0f, k_ClearViewSeconds);
		}
		_clearView.Update(seconds);
	}

	// The wheel zooms a fixed step a notch, whatever the frame time, while a zoom action is performed: zooming out first,
	// then in, each a notch its way when the wheel wasn't turned. A turn without either is lost, and so is the wheel
	// during the clear view
	if (_clearView.value < k_ClearViewShown)
	{
		float notches = 0.0f;
		if (actionSystem.Get(input::BindableActionMap::ZOOM_OUT))
		{
			notches = actionSystem.GetMouseWheelDelta() != 0.0f ? actionSystem.GetMouseWheelDelta() : -1.0f;
		}
		else if (actionSystem.Get(input::BindableActionMap::ZOOM_IN))
		{
			notches = actionSystem.GetMouseWheelDelta() != 0.0f ? actionSystem.GetMouseWheelDelta() : 1.0f;
		}
		if (notches != 0.0f)
		{
			_rotateAroundDelta.z -= notches * k_WheelZoomPerNotch;
		}
	}

	// After a drag given up, the land grip, the buttons and every move of the camera are dropped until a frame with none
	// of them
	bool rotateAroundMouse = actionSystem.Get(input::BindableActionMap::ROTATE_AROUND_MOUSE_ON);
	bool gripHeld = _handPosition.has_value() && actionSystem.Get(input::BindableActionMap::MOVE);
	bool bothButtons = actionSystem.Get(input::UnbindableActionMap::TWO_BUTTON_CLICK);
	if (_dragGivenUp)
	{
		if (gripHeld || rotateAroundMouse || actionSystem.Get(input::BindableActionMap::ACTION) || bothButtons ||
		    _rotateAroundDelta != glm::vec3() || _keyBoardMoveDelta != glm::vec2())
		{
			gripHeld = false;
			rotateAroundMouse = false;
			bothButtons = false;
			_rotateAroundDelta = glm::vec3();
			_keyBoardMoveDelta = glm::vec2();
		}
		else
		{
			_dragGivenUp = false;
		}
	}

	// The middle button and both buttons turn the camera only with their feature
	rotateAroundMouse = rotateAroundMouse && Allows(_features, camera_help::Feature::JustZoom);
	bothButtons = bothButtons && Allows(_features, camera_help::Feature::JustZoom);
	// Both buttons zoom as the mouse moves up and down, and turn the camera as it moves across once it has moved a
	// fortieth of the screen's width across in a frame or since they were pressed
	if (bothButtons)
	{
		_rotateAroundDelta.z += actionSystem.GetMouseDelta().y * k_TwoButtonZoomFactor;
	}
	{
		const auto width = Locator::windowing::has_value() ? Locator::windowing::value().GetSize().x : 1;
		const auto across = _twoButtonTurn.Update(bothButtons, actionSystem.GetMouseDelta().x,
		                                          static_cast<int>(actionSystem.GetMousePosition().x), width);
		if (across != 0)
		{
			_rotateAroundDelta.y += static_cast<float>(across) * k_TwoButtonTurnFactor;
		}
	}
	if (rotateAroundMouse)
	{
		const auto mouseDelta = static_cast<glm::vec2>(actionSystem.GetMouseDelta());
		_rotateAroundDelta += glm::vec3(glm::yx(mouseDelta * k_RotateOnSpeedMultiplier), 0.0f);
	}

	// A pan counts as a drag for the camera help with any camera input, or with the grip, the middle button or both
	// buttons held and the mouse moved more than 2 pixels across or down. Without the both-buttons feature the middle
	// button and both buttons don't count, and a keyboard move alone doesn't count while gripping
	{
		const bool otherInput = _rotateAroundDelta != glm::vec3() || rotateAroundMouse;
		const bool input = otherInput || (_keyBoardMoveDelta != glm::vec2() && !gripHeld);
		const auto mouseMoved = glm::abs(actionSystem.GetMouseDelta());
		_dragCountsForHelp =
		    input || ((gripHeld || rotateAroundMouse || bothButtons) && (mouseMoved.x > 2 || mouseMoved.y > 2));
		// That camera input, the land gripped or both buttons held drop the flight on its way, whoever started it. The
		// camera carries on from where the flight's leg was taking it, as the controls move it. A flight that starts
		// later in the frame, as the double click's does, is kept
		if (input || gripHeld || bothButtons)
		{
			_flightPath.reset();
		}
	}
	// The land gripped, and whether with no other camera control (a keyboard move alone doesn't count)
	_grippedBefore = _gripping;
	_gripping = gripHeld;
	_gripOnly = gripHeld && _rotateAroundDelta == glm::vec3() && !rotateAroundMouse && !bothButtons;

	// The hints and what a drag of the land turns into, for the hand to show and the camera to follow
	const bool dragHeld = gripHeld && !rotateAroundMouse;
	_controlsTime += dt;
	if (!dragHeld)
	{
		HandleDrag(false);
	}
	// What the player does with the camera this frame, before the self-tilting camera takes the tilt
	const bool turning = Allows(_features, camera_help::Feature::Rotate) && _rotateAroundDelta.y != 0.0f;
	const bool tilting = Allows(_features, camera_help::Feature::Pitch) && _rotateAroundDelta.x != 0.0f;
	const bool zooming = Allows(_features, camera_help::Feature::Zoom) && _rotateAroundDelta.z != 0.0f;
	const bool playerInput = _rotateAroundDelta != glm::vec3() || _keyBoardMoveDelta != glm::vec2() || rotateAroundMouse;
	// The self-tilting camera, while the land isn't gripped: a fifth of the way towards its pitch each frame, at most
	// the frame's seconds, nothing within a hundredth of it. The player's own tilt is dropped meanwhile
	_autoTilting = false;
	if (Allows(_features, camera_help::Feature::AutoPitch) && !gripHeld)
	{
		float heading = 0.0f;
		float pitch = 0.0f;
		script_camera::HeadingAndPitchFromPoints(_targetOrigin, _focusAtClick, heading, pitch);
		const auto input =
		    camera_help::AutoPitchInput(camera_help::GetAutoPitchAngle(), pitch, std::chrono::duration<float>(dt).count());
		_rotateAroundDelta.x = input.value_or(0.0f);
		_autoTilting = input.has_value();
	}
	// Nothing gripped and nothing done with the camera: the self-tilting camera keeps to its height
	_idleMouse = !gripHeld && !playerInput;

	// The hints follow the cursor while nothing is dragged and the camera isn't being turned, moved or zoomed, as far as
	// the features let it, or while the camera tilts itself
	if (((!_dragging && !rotateAroundMouse && !turning && !tilting && !zooming && _keyBoardMoveDelta == glm::vec2()) ||
	     _autoTilting) &&
	    Locator::windowing::has_value())
	{
		const auto screenSize = Locator::windowing::value().GetSize();
		const auto cursor =
		    camera_drag::NormalisedCursor(glm::ivec2(actionSystem.GetMousePosition()), screenSize, ViewHeight(screenSize));
		_tricons = camera_drag::IdleTricons(cursor, _screenSpaceMouseRaycastHit.has_value(), Windowed());
		// No tilt from the screen's edge while a fight is watched
		if (_fight.watching)
		{
			_tricons &= ~camera_drag::tricon::k_Pitch;
		}
	}

	_modePrev = _mode;
	// The double click's flight needs its feature bit, the land grab its own
	const bool canFly = Allows(_features, camera_help::Feature::DoubleClickFly);
	const bool canGrab = Allows(_features, camera_help::Feature::GrabLand);
	// A drag the double click's flight doesn't take
	if (dragHeld && !(canFly && actionSystem.Get(input::UnbindableActionMap::DOUBLE_CLICK)))
	{
		HandleDrag(true);
	}
	if (canFly && _handPosition.has_value() && actionSystem.Get(input::UnbindableActionMap::DOUBLE_CLICK))
	{
		_mode = Mode::FlyingToPoint;
		ReportFlightToHand();
	}
	else if (rotateAroundMouse)
	{
		_mode = Mode::ArcBall;
	}
	else if (canGrab && gripHeld)
	{
		_mode = FollowDrag();
	}
	else if (_keyBoardMoveDelta != glm::vec2() || _rotateAroundDelta != glm::vec3() || playerInput)
	{
		_mode = Mode::Polar;
	}
	else
	{
		_mode = Mode::Cartesian;
	}
}

void DefaultWorldCameraModel::HandleDrag(bool held)
{
	if (!held)
	{
		_dragging = false;
		_landGrip.reset();
		return;
	}
	if (!Locator::windowing::has_value())
	{
		return;
	}
	const auto& actionSystem = Locator::gameActionSystem::value();
	const auto screenSize = Locator::windowing::value().GetSize();
	const auto milliseconds =
	    static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(_controlsTime).count());
	if (!_dragging)
	{
		// The drag takes the hints of the cursor before it was pressed
		_dragging = true;
		const auto cursor = glm::ivec2(actionSystem.GetMousePosition());
		_drag.Start(_tricons, camera_drag::NormalisedCursor(cursor, screenSize, ViewHeight(screenSize)), milliseconds);
		_ringCursor = cursor;
		// The land is gripped as the camera next looks at it
		_landGrip.reset();
	}
	else
	{
		_drag.Move(actionSystem.GetMouseDelta(), screenSize, ViewHeight(screenSize), milliseconds,
		           _screenSpaceMouseRaycastHit.has_value(), _features);
	}
}

DefaultWorldCameraModel::Mode DefaultWorldCameraModel::FollowDrag()
{
	using camera_drag::DragMode;
	if (!_dragging || !Locator::windowing::has_value())
	{
		return Mode::DraggingLandscape;
	}
	const auto mode = _drag.GetMode();
	if (!mode.has_value())
	{
		return Mode::Cartesian;
	}
	auto& actionSystem = Locator::gameActionSystem::value();
	const auto screenSize = Locator::windowing::value().GetSize();
	// A drag with only a keyboard move drops the move
	if (_rotateAroundDelta == glm::vec3())
	{
		_keyBoardMoveDelta = glm::vec2();
	}
	switch (*mode)
	{
	case DragMode::Pan:
		return Mode::DraggingLandscape;
	case DragMode::EdgeRotate:
	{
		// Without turning nothing happens, the cursor included
		if (!Allows(_features, camera_help::Feature::Rotate))
		{
			return Mode::Cartesian;
		}
		// The cursor is put back on the ring, and the camera turns about its focus by the angle swept round the middle
		const auto step = camera_drag::EdgeRotate(glm::ivec2(actionSystem.GetMousePosition()), _ringCursor, screenSize,
		                                          ViewHeight(screenSize));
		_ringCursor = step.cursor;
		actionSystem.WarpCursor(step.cursor);
		_dragTurnAndTilt.x = step.angle * static_cast<float>(screenSize.x) / glm::pi<float>();
		_rotateAroundDelta.y += _dragTurnAndTilt.x;
		// The camera help: Rotate when the frame counts as a drag, then the way it turned. Anything under a hundredth to
		// the right counts as turning to the left, standing still included, as the original does
		if (_dragCountsForHelp)
		{
			help_profile::CameraHelpCallback(help_profile::CameraReason::Rotate, 0);
		}
		if (step.angle > 0.01f)
		{
			help_profile::CameraHelpCallback(help_profile::CameraReason::RotateCW, 0);
		}
		if (step.angle < 0.01f)
		{
			help_profile::CameraHelpCallback(help_profile::CameraReason::RotateCCW, 0);
		}
		return Mode::Polar;
	}
	case DragMode::Pitch:
	case DragMode::PitchFromTop:
	{
		if (!Allows(_features, camera_help::Feature::Pitch))
		{
			return Mode::Cartesian;
		}
		if (_dragCountsForHelp)
		{
			help_profile::CameraHelpCallback(help_profile::CameraReason::Pitch, 0);
		}
		const auto fov = Locator::camera::has_value() ? Locator::camera::value().GetHorizontalFieldOfView() : 0.0f;
		_dragTurnAndTilt.y = camera_drag::PitchStep(actionSystem.GetMouseDelta().y, screenSize.y, fov) / k_PitchPerInput;
		_rotateAroundDelta.x += _dragTurnAndTilt.y;
		return Mode::Polar;
	}
	}
	return Mode::DraggingLandscape;
}

int DefaultWorldCameraModel::ViewHeight(glm::ivec2 screenSize)
{
	const bool bars =
	    Locator::cinematicDirectorSystem::has_value() && Locator::cinematicDirectorSystem::value().IsWideScreenOn();
	return camera_drag::ViewHeight(screenSize, bars);
}

bool DefaultWorldCameraModel::Windowed()
{
	return Locator::config::has_value() && Locator::config::value().displayMode == windowing::DisplayMode::Windowed;
}

void DefaultWorldCameraModel::SetFlight(glm::vec3 origin, glm::vec3 focus)
{
	FlyTo(Locator::terrainSystem::value(), origin, focus, k_PlaceFlightRise);
}

void DefaultWorldCameraModel::FlyTo(const LandIslandInterface& land, glm::vec3 origin, glm::vec3 focus, float rise)
{
	_flightPath = CharterFlight(land, origin, focus, _currentOrigin, rise);
	// The bookmarks', the scripts' and the watched fight's flights: the woosh only when the camera is farther than
	// 100 * 1.5 from the new position
	if (glm::distance(origin, _currentOrigin) > k_FlyingDistanceThresholds[0] * 1.5f)
	{
		PlayWoosh();
	}
}

void DefaultWorldCameraModel::PlayWoosh()
{
	// 46 G_Woosh_01 + (tick count & 3): no owner, mode 3, no loops, 2D, InGame
	audio::PlaySoundEffect(audio::Owner::None(), 46 + static_cast<int>(audio::TickCount() & 3), 3, 0, false, false,
	                       audio::SfxBank::InGame);
}

void DefaultWorldCameraModel::PlanClearView()
{
	// From the view the camera has now to the hand's point, seen from the same side, nearer, and tilted to a quarter of
	// the sum of its pitch and three quarters of pi
	_clearViewPlan.fromOrigin = _targetOrigin;
	_clearViewPlan.fromFocus = _targetFocus;
	_clearViewPlan.point = _handPosition;
	_clearViewPlan.toOrigin.reset();
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(_targetOrigin, _targetFocus, _clearViewPlan.heading, pitch);
	const auto distance = glm::distance(_targetOrigin, _targetFocus);
	if (distance > k_ClearViewFarCamera)
	{
		_clearViewPlan.distance = k_ClearViewFromFar;
	}
	else if (distance < k_ClearViewNearCamera)
	{
		_clearViewPlan.distance = k_ClearViewFromNear;
	}
	else
	{
		_clearViewPlan.distance = k_ClearViewDistance;
	}
	_clearViewPlan.pitch = (pitch + 0.75f * glm::pi<float>()) * 0.25f;
}

void DefaultWorldCameraModel::ReportFlightToHand()
{
	// DoubleClickObject when it was on an object: (approximate) openblack's camera does not tell them apart, both count
	// as the position's
	help_profile::CameraHelpCallback(help_profile::CameraReason::DoubleClickPos, 0);
}

uint32_t DefaultWorldCameraModel::CameraFeatures() const
{
	const auto features = camera_help::GetEnabledFeatures();
	return static_cast<uint32_t>(_fight.watching ? camera_help::DuringFight(features) : features);
}

void DefaultWorldCameraModel::StartFight(entt::entity fighterA, entt::entity fighterB, glm::vec3 arenaCentre, float arenaRadius)
{
	// The fight's side has decided the camera watches: it starts a quarter turn round, tilted down, 5.5 of the second
	// fighter's radii out, after a flight to look at the arena
	if (fighterA == entt::null || fighterB == entt::null)
	{
		return;
	}
	_fight.watching = true;
	_fight.arena = FightArena {.fighterA = fighterA, .fighterB = fighterB, .centre = arenaCentre, .radius = arenaRadius};
	_fight.distance = fight_orbit::k_StartDistance;
	_fight.yaw = fight_orbit::k_StartYaw;
	_fight.pitch = fight_orbit::k_StartPitch;
	_fight.status = FightStatus::On;
	_fight.lingerLeftMs = 0;
	_fight.firstFrame = true;
	_fight.turn = Zoomer {};
	_fight.focus.SetPosition(glm::vec3(0.0f));
	// The flight: from the arena's centre, its x and z as a map position holds them and its height the land's there
	// (the creature's arenas have none of their own), to the view of its rim half a radius up, with no rise. The orbit
	// runs from the next frame on, and its place reaches the camera once the flight's last leg is set
	if (Locator::terrainSystem::has_value())
	{
		const auto& land = Locator::terrainSystem::value();
		const auto centre = map_coords::ToWorld(&land, map_coords::FromMetres(glm::xz(arenaCentre)));
		const auto view = camera_flight::ViewOfPoint(land, centre, camera_flight::ArenaLookPoint(centre, arenaRadius));
		FlyTo(land, view.origin, view.focus, k_FightFlightRise);
	}
	_flightSeconds = 0.0f;
}

void DefaultWorldCameraModel::EndFightNow()
{
	_fight.watching = false;
	_fight.arena.reset();
	_fight.lingerLeftMs = 0;
	_fight.status = FightStatus::Ended;
}

void DefaultWorldCameraModel::EndFight()
{
	// Only once: a fight already over keeps the time it has left
	if (_fight.status != FightStatus::On)
	{
		return;
	}
	if (_fight.lingerLeftMs <= 0)
	{
		_fight.lingerLeftMs = fight_orbit::k_LingerMs;
	}
	_fight.status = FightStatus::Lingering;
}

bool DefaultWorldCameraModel::WantToQuitFight(glm::vec3 arenaCentre, float arenaRadius) const
{
	// Not without the land at the screen's middle
	if (!_screenSpaceCenterRaycastHit.has_value())
	{
		return false;
	}
	return fight_orbit::WantToQuit(arenaCentre, arenaRadius, _currentOrigin, *_screenSpaceCenterRaycastHit, 1.0f);
}

void DefaultWorldCameraModel::UpdateFightWatch(std::chrono::microseconds dt)
{
	if (!_fight.watching)
	{
		return;
	}
	_fight.overArenaMs = 0;
	if (_fight.status == FightStatus::Lingering)
	{
		_fight.lingerLeftMs -= static_cast<int32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count());
		if (_fight.lingerLeftMs < 0)
		{
			_fight.status = FightStatus::Ended;
		}
	}
	if (_fight.status == FightStatus::Ended)
	{
		EndFightNow();
		return;
	}
	// (approximate) the arena is gone once either fighter is
	const auto there = [](entt::entity fighter) {
		return Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(fighter) &&
		       Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(fighter);
	};
	if (!_fight.arena.has_value() || !there(_fight.arena->fighterA) || !there(_fight.arena->fighterB))
	{
		EndFight();
		_fight.arena.reset();
	}
}

bool DefaultWorldCameraModel::DraggedAwayFromFight() const
{
	// Gripping the land in the frame before, with the camera and the land at the screen's middle too far from the arena,
	// its radius counted at three quarters
	return _flightSeconds > fight_orbit::k_DragAwaySeconds && _fight.watching && _fight.arena.has_value() &&
	       _screenSpaceCenterRaycastHit.has_value() &&
	       fight_orbit::WantToQuit(_fight.arena->centre, _fight.arena->radius, _targetOrigin, *_screenSpaceCenterRaycastHit,
	                               fight_orbit::k_DragAwayShare) &&
	       _grippedBefore;
}

bool DefaultWorldCameraModel::FollowFight(float zoomDelta, float seconds)
{
	if (!_fight.arena.has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto placeOf = [&registry](entt::entity fighter) -> std::optional<glm::vec3> {
		const auto* transform = registry.Valid(fighter) ? registry.TryGet<const ecs::components::Transform>(fighter) : nullptr;
		return transform != nullptr ? std::optional(transform->position) : std::nullopt;
	};
	const auto a = placeOf(_fight.arena->fighterA);
	const auto b = placeOf(_fight.arena->fighterB);
	if (!a.has_value() || !b.has_value())
	{
		return false;
	}
	// The camera eases into the orbit's place more slowly while its mode is new
	_fight.easeSeconds = fight_orbit::EaseSeconds(script_camera::Get().modeSeconds);
	const float radiusA = _fighterRadius(_fight.arena->fighterA);
	const float radiusB = _fighterRadius(_fight.arena->fighterB);
	const float spacing = fight_orbit::Spacing(*a, *b);
	// (pending) the middle is also raised by a quarter of the two fighters' heights over something not read yet
	const auto middle = fight_orbit::Middle(*a, *b);

	// The zoom moves the camera nearer or further; too far ends the watch, though the camera is still placed this frame
	if (zoomDelta != 0.0f)
	{
		_fight.distance = fight_orbit::Zoom(_fight.distance, zoomDelta, spacing, radiusA, radiusB);
	}
	if (_fight.distance > fight_orbit::k_MaxDistance)
	{
		EndFightNow();
	}
	_fight.distance = fight_orbit::ClampDistance(_fight.distance);
	_fight.pitch = fight_orbit::ClampPitch(_fight.pitch);
	const float distance = fight_orbit::OrbitDistance(_fight.distance, spacing, radiusA, radiusB);

	// The focus and the turn follow the fighters over five seconds, put there at once on the first frame
	const float heading = fight_orbit::FightersHeading(*a, *b, _fight.turn.value);
	if (_fight.firstFrame)
	{
		_fight.turn.SetPosition(heading);
		_fight.focus.SetPosition(middle);
		_fight.firstFrame = false;
	}
	else
	{
		_fight.turn.SetDestinationWithSpeedAndTime(fight_orbit::TurnTowards(_fight.turn.value, heading), 0.0f,
		                                           fight_orbit::k_FollowSeconds);
		_fight.focus.SetDestinationWithTime(middle, fight_orbit::k_FollowSeconds);
	}
	_fight.turn.Update(seconds);
	_fight.focus.Update(seconds);

	const auto focus = _fight.focus.GetCurrentValue();
	_targetFocus = focus;
	_focusAtClick = focus;
	_targetOrigin =
	    script_camera::PointFromDistanceHeadingAndPitch(focus, distance, _fight.yaw - _fight.turn.value, _fight.pitch);
	return true;
}

glm::vec3 DefaultWorldCameraModel::GetTargetOrigin() const
{
	return _targetOrigin;
}

glm::vec3 DefaultWorldCameraModel::GetTargetFocus() const
{
	return _targetFocus;
}

CameraModel::HandCues DefaultWorldCameraModel::GetHandCues() const
{
	if (_dragging)
	{
		return {.tricons = _drag.GetTricons(), .dragging = true, .dragMode = _drag.GetMode()};
	}
	return {.tricons = _tricons};
}

std::chrono::seconds DefaultWorldCameraModel::GetIdleTime() const
{
	SPDLOG_LOGGER_WARN(spdlog::get("game"), "TODO: Idle Time not implemented");
	return {};
}

glm::vec3 DefaultWorldCameraModel::GetTargetForwardVector() const
{
	return GetTargetFocus() - GetTargetOrigin();
}

glm::vec3 DefaultWorldCameraModel::GetTargetForwardUnitVector() const
{
	return glm::normalize(GetTargetForwardVector());
}

glm::vec3 DefaultWorldCameraModel::ProjectPointOnForwardVector(float distanceFromOrigin) const
{
	return _targetOrigin + GetTargetForwardUnitVector() * distanceFromOrigin;
}
