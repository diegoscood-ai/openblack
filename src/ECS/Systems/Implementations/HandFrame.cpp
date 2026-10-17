/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's matrix as the original builds it and the up of the empty hand (the end of the hand's placement). Wiki:
// docs/wiki/hand/animation-and-placement.md#where-the-hand-is-placed.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <glm/geometric.hpp>

#include "3D/HandOrientation.h"
#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Hand/HandFeel.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

void HandSystem::UpdateHeading() noexcept
{
	// d = (-sin H, 0, cos H), horizontal towards the camera, with the heading H taken before drawing from the camera ->
	// mouse ray only when |ray.x| or |ray.z| > 0.01 (hand_orientation::HeadingAlongRay, d being its opposite)
	_handHeadingBack = -hand_orientation::HeadingAlongRay(_mouseRayDirection, -_handHeadingBack);
	// the heading H itself, the ray's yaw rounded to a float, under the same lean
	if (std::abs(_mouseRayDirection.x) > hand_orientation::k_MinHeadingLean ||
	    std::abs(_mouseRayDirection.z) > hand_orientation::k_MinHeadingLean)
	{
		_handHeading = static_cast<float>(affine::GetYAngle(_mouseRayDirection));
	}
}

glm::mat3 HandSystem::HandMatrixRotation(glm::vec3 up) noexcept
{
	// from the heading H itself and the up as given (the scale and the left hand's mirrored X are the transform's)
	UpdateHeading();
	return hand_orientation::HandRotation(_handHeading, up);
}

glm::vec3 HandSystem::UpdateNormalUp(float seconds) noexcept
{
	// The end of the placement: over plain land the up is the land normal at the hand's last position (read before
	// that position's last write). (pending) over an object, 0.25 toCam - hitNormal + (0, 0.5, 0) (when the hand
	// feels with a mesh intersection, and the hand higher than 1.6 hs above the ground); the rotate tricon.
	// Then the three per-axis Zoomers take the new target (in 0.4 s) only when the mouse's x changed this frame or the
	// roll is not 0 (0 in the normal hand state), and are updated every frame; the up is their value normalised
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (Locator::terrainSystem::has_value() && _mouse.x != _upMouseX)
	{
		_upMouseX = _mouse.x;
		const auto& terrain = Locator::terrainSystem::value();
		const glm::vec2 xz(transform.position.x, transform.position.z);
		// over an object the hand feels, its up target (_feelUp), unless the hand is lower over the land than half its
		// size (3.2 x the hand scale x 0.5): the land normal; normalised
		auto target = terrain.GetNormalAt(xz);
		const float aboveLand = transform.position.y - terrain.GetHeightAt(xz);
		if (!hand_feel::TurnsToLand(_feelUp.has_value(), true, aboveLand, 3.2f * _handScale) && glm::length(*_feelUp) > 0.0f)
		{
			target = glm::normalize(*_feelUp);
		}
		// hovering at the edge of the screen and showing that it turns the camera, the up stands towards the camera's
		// focus from the point the hand aims at, made a unit vector again
		if (_hoverPose == hand_navigation_pose::Pose::Rotate && Locator::camera::has_value())
		{
			target = hand_orientation::NormaliseUp(
			    hand_navigation_pose::UpTowardsFocus(_cursorRayPoint, Locator::camera::value().GetFocus()));
		}
		// the same as the three per-axis Zoomers with speed 0
		_up.SetDestinationWithTime(target, hand_orientation::k_UpEaseSeconds);
	}
	_up.Update(seconds);
	const auto value = _up.GetCurrentValue();
	_normalUp = hand_orientation::NormaliseUp(value);
	return _normalUp;
}
