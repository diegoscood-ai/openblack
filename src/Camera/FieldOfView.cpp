/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldOfView.h"

#include <algorithm>
#include <cmath>

#include <glm/gtx/transform.hpp>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "Camera/Camera.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::field_of_view
{
using graphics::region_on_screen::PointOnScreen;
using graphics::region_on_screen::SphereOnScreen;

namespace
{
/// (pending, Edificios) g_game +0x205A28 == 1: openblack has no temple interior state yet
bool InsideTemple()
{
	return false;
}
} // namespace

bool CurrentView(View& out)
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	out.worldToClip = camera.GetViewProjectionMatrix(); // g_world_to_clipping [0xEA9E40] (RendererShadows.cpp)
	out.nearW = graphics::billboard::CameraFrame::From(camera).nearZ; // [0xE839E0]
	out.screen = Locator::windowing::value().GetSize();               // [0xE839E4] / [0xE839E8]
	out.eye = camera.GetOrigin();                                     // g_camera [0xEA1DB8]
	out.tanHalfFov = std::tan(camera.GetHorizontalFieldOfView() * 0.5f);
	return out.screen.x > 0 && out.screen.y > 0;
}

bool PosInView(const glm::vec3& point)
{
	View view;
	if (InsideTemple() || !CurrentView(view)) // 0x6F80ED..0x6F8109
	{
		return false;
	}
	return PointOnScreen(view, point); // 0x6F810F
}

bool ThingInView(entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	View view;
	// 0x6F819F..0x6F81B4: inside the temple or no thing -> false
	if (thing == entt::null || !registry.Valid(thing) || InsideTemple() || !CurrentView(view))
	{
		return false;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
	if (transform == nullptr)
	{
		return false; // (approximate) a thing without a place (a timer): the original would test (0, altitude, 0)
	}
	if (const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing); mesh != nullptr)
	{
		// fn_0081F1A0: the 3D object's mesh (vt +0xF8), none -> false
		auto& meshes = Locator::resources::value().GetMeshes();
		if (!meshes.Contains(mesh->id))
		{
			return false;
		}
		const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
		const glm::mat4 model = glm::translate(transform->position) * glm::mat4(transform->rotation) *
		                        glm::scale(transform->scale);
		const glm::vec3 centre = glm::vec3(model * glm::vec4(box.Center(), 1.0f));
		const float scale = std::max({transform->scale.x, transform->scale.y, transform->scale.z}); // +0x44
		const float radius = glm::length(box.Size()) * 0.5f * scale; // (approximate) LH3DBoundingBox +0x1C
		return SphereOnScreen(view, centre, radius, transform->position);
	}
	// 0x6F81F4..0x6F822B: (x, GetAltitude(MapCoords) + relY (+0x1C), z) of a GameThingWithPos that is not an Object
	return PointOnScreen(view, transform->position);
}

} // namespace openblack::field_of_view
