/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's matrix as the original builds it (CHand fn_0046E160) and the up of the empty hand (the tail of
// ObtainRequiredHandPosition 0x5B6DE0..0x5B7145). Research: dev\documentacion\hand\placement\README.md. Wiki:
// docs/bw1-notes/hand-and-interface.md, "Where the hand is placed".

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

glm::mat3 HandSystem::HandMatrixRotation(glm::vec3 up) noexcept
{
	// fn_0046E160: d = (-sin H, 0, cos H), horizontal towards the camera, with H (+0x84, written at 0x46C725) taken in
	// PrepareForDrawing from the camera -> mouse ray (fn_0074CBD0) only when |ray.x| or |ray.z| > 0.01 (the double
	// [0x8C7A10], 0x46C6FB / 0x46C70E); side = norm(d x up), skipped when zero; the model's X =
	// side, Y = side x up, Z = -up (the scale and the left hand's mirrored X are the transform's)
	if (std::abs(_mouseRayDirection.x) > 0.01f || std::abs(_mouseRayDirection.z) > 0.01f)
	{
		_handHeadingBack = -glm::normalize(glm::vec3(_mouseRayDirection.x, 0.0f, _mouseRayDirection.z));
	}
	// (openblack guard) the 1e-6 fallbacks for a zero up or side
	up = glm::length(up) > 1e-6f ? glm::normalize(up) : glm::vec3(0.0f, 1.0f, 0.0f);
	auto side = glm::cross(_handHeadingBack, up);
	side = glm::length(side) > 1e-6f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
	return glm::mat3(side, glm::cross(side, up), -up);
}

glm::vec3 HandSystem::UpdateNormalUp(float seconds) noexcept
{
	// ORHP's tail: over plain land the up is LH3DIsland::GetNormal 0x803630 at the hand's last position (arg5 is read
	// before its last write). (pending) over an object, 0.25 toCam - hitNormal + (0, 0.5, 0) (with
	// HandShouldFeelWithMeshIntersect vt +0x59C, and the hand higher than 1.6 hs above the ground); the rotate tricon.
	// Then the three Zoomers 0xD13FB0 / FE0 / 4010 take the new target (Zoomer::SetDestinationWithSpeedAndTime 0x407D60
	// per axis, 0x5B7075 / 0x5B708E / 0x5B70A7, in 0.4 s: push 0x3ECCCCCD at 0x5B7065) only when the mouse's x changed this
	// frame (CHand +0x4858, 0x46C6EA) or the roll is not 0 (0x5B7043..0x5B705F; 0 in HandStateNormal), and are updated
	// every frame; g_D13F60 is their value normalised (fn_00460710 at 0x5B712A)
	constexpr float k_UpSeconds = 0.4f;
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (Locator::terrainSystem::has_value() && _mouse.x != _upMouseX)
	{
		_upMouseX = _mouse.x;
		const auto normal =
		    Locator::terrainSystem::value().GetNormalAt(glm::vec2(transform.position.x, transform.position.z));
		_up.SetDestinationWithTime(normal, k_UpSeconds); // the same as the three 0x407D60 with speed 0
	}
	_up.Update(seconds);
	const auto value = _up.GetCurrentValue();
	_normalUp = glm::length(value) > 1e-6f ? glm::normalize(value) : glm::vec3(0.0f, 1.0f, 0.0f);
	return _normalUp;
}
