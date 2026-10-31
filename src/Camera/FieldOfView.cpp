/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FieldOfView.h"

#include <cmath>

#include <glm/mat4x4.hpp>

#include "3D/AffineMatrix.h"
#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Camera/Camera.h"
#include "Common/GUtilsAngle.h"
#include "Creature/CreaturePlayerAttention.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreaturePose.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPlacement.h"
#include "ECS/PlacedTurn.h"
#include "ECS/Registry.h"
#include "ECS/ScriptTypes.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::field_of_view
{
using graphics::region_on_screen::PointOnScreen;
using graphics::region_on_screen::SphereOnScreen;

bool CurrentView(View& out)
{
	if (!Locator::camera::has_value() || !Locator::windowing::has_value())
	{
		return false;
	}
	const auto& camera = Locator::camera::value();
	const auto frame = graphics::billboard::CameraFrame::From(camera);
	// The world-to-clipping matrix (affine::FrameMatrices) and the near plane
	out.worldToClipping = frame.clipMatrices.worldToClipping;
	out.nearW = frame.nearZ;
	out.screen = Locator::windowing::value().GetSize();
	out.eye = camera.GetOrigin(); // the drawn camera
	out.tanHalfFov = std::tan(camera.GetHorizontalFieldOfView() * 0.5f);
	return out.screen.x > 0 && out.screen.y > 0;
}

bool PosInView(const View& view, bool insideCitadel, const glm::vec3& point)
{
	return !insideCitadel && PointOnScreen(view, point);
}

bool PosInView(const glm::vec3& point)
{
	View view;
	if (!CurrentView(view))
	{
		return false;
	}
	return PosInView(view, game_clock::IsInsideCitadel(), point);
}

glm::vec3 MapPointOf(const glm::vec3& position)
{
	return map_coords::ToWorld(map_coords::FromWorld(position));
}

std::optional<glm::vec3> PointOfThing(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(thing); transform != nullptr)
	{
		return MapPointOf(transform->position);
	}
	// The things openblack keeps no Transform for: a script's timer stands at the map's corner, a dance at its place
	if (registry.AllOf<ecs::components::ScriptTimer>(thing))
	{
		return map_coords::ToWorld(map_coords::MapCoords {});
	}
	if (const auto* dance = registry.TryGet<const ecs::components::Dance>(thing); dance != nullptr)
	{
		return map_coords::ToWorld(dance->place);
	}
	return std::nullopt;
}

bool IsObjectForTheScreenTest(entt::entity thing)
{
	// a script's marker and a dance are no objects either
	return ecs::script_type::IsObject(thing) &&
	       !Locator::entitiesRegistry::value().AnyOf<ecs::components::ScriptMarker, ecs::components::Dance>(thing);
}

bool ThingInView(const View& view, bool insideCitadel, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Inside the citadel or no thing -> false
	if (thing == entt::null || !registry.Valid(thing) || insideCitadel)
	{
		return false;
	}
	// An object: its mesh's sphere (none -> false)
	if (IsObjectForTheScreenTest(thing))
	{
		return ObjectOnScreen(view, thing);
	}
	const auto point = PointOfThing(thing);
	return point.has_value() && PointOnScreen(view, *point);
}

bool ThingInView(entt::entity thing)
{
	View view;
	if (!CurrentView(view))
	{
		return false;
	}
	return ThingInView(view, game_clock::IsInsideCitadel(), thing);
}

bool ObjectOnScreen(entt::entity thing)
{
	View view;
	if (!CurrentView(view))
	{
		return false;
	}
	return ObjectOnScreen(view, thing);
}

bool ObjectOnScreen(const View& view, entt::entity thing)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
	const auto* mesh = registry.TryGet<const ecs::components::Mesh>(thing);
	if (transform == nullptr || mesh == nullptr)
	{
		return false;
	}
	// The 3D object's mesh, none -> false
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return false;
	}
	const auto box = meshes.Handle(mesh->id)->GetBoundingBox();
	// the model's own placement (its class's constructor, ECS/ObjectPlacement.h): the box centre through its cells in
	// the original's order, the radius from its one scale, and the eye's test from its own position
	const auto placed = ecs::object_placement::Of(registry, thing);
	const glm::vec3 centre = affine::BoxCentreThroughObject(affine::FromModel(placed.cells), box.Center());
	const float radius = ObjectSphereRadius(box, placed.scale);
	return SphereOnScreen(view, centre, radius, glm::vec3(placed.cells[3]));
}

float ObjectSphereRadius(const AxisAlignedBoundingBox& meshBox, float scale)
{
	return scale * ecs::object::HalfDiagonal(ecs::object::HalfExtents(meshBox));
}

namespace
{
/// The float of pi / 180 the game turns degrees into radians with
constexpr float k_RadiansPerDegree = 0.0174532924f;

/// One edge's side: dz cos e - dx sin e, the cosine wide, the sine a float
float EdgeSide(float dx, float dz, float edge)
{
	const double cosine = std::cos(static_cast<double>(edge));
	const auto sine = static_cast<float>(std::sin(static_cast<double>(edge)));
	return static_cast<float>(static_cast<double>(dz) * cosine) - dx * sine;
}
} // namespace

float HalfAngleOfDegrees(float degrees)
{
	return (degrees * 0.5f) * k_RadiansPerDegree;
}

bool CameraWithinAngle(glm::vec2 thing, float yAngle, float halfAngle, glm::vec2 camera)
{
	const float dx = camera.x - thing.x;
	const float dz = camera.y - thing.y;
	if (!(EdgeSide(dx, dz, yAngle - halfAngle) >= 0.0f))
	{
		return false;
	}
	return !(EdgeSide(dx, dz, yAngle + halfAngle) > 0.0f);
}

std::optional<uint32_t> CreatureViewParts(float degrees)
{
	const auto parts = static_cast<uint32_t>(map_coords::FtoL(360.0f / degrees));
	if (parts == 0)
	{
		return std::nullopt;
	}
	return parts;
}

bool ThingCanViewCamera(entt::entity thing, float degrees)
{
	if (!Locator::camera::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(thing);
	if (transform == nullptr)
	{
		return false;
	}
	// The drawn camera's position
	const glm::vec3 eye = Locator::camera::value().GetOrigin();
	if (registry.AllOf<ecs::components::Creature>(thing))
	{
		const auto parts = CreatureViewParts(degrees);
		if (!parts.has_value())
		{
			return false; // openblack guard: the game divides by 0 here
		}
		const auto* animation = registry.TryGet<const ecs::components::CreatureAnimation>(thing);
		const float look =
		    creature_perceived_desires::LookYaw(animation != nullptr ? animation->lookAt : std::nullopt, transform->position,
		                                        ecs::creature_pose::BodyYaw(registry, thing, transform->rotation));
		const auto heading = static_cast<uint16_t>(gutils::ConvertScawenAngleToGameAngle(look));
		return creature_player_attention::IsDirectionClose(
		    map_coords::FromMetres({transform->position.x, transform->position.z}), map_coords::FromMetres({eye.x, eye.z}),
		    heading, *parts);
	}
	// Any other object: from its map position, by the angle it keeps about the upright
	const float yAngle = registry.AnyOf<ecs::components::Villager, ecs::components::Animal>(thing)
	                         ? ecs::villager_physics::ObjectYAngle(thing)
	                         : ecs::placed_turn::AnglesOf(registry, thing).y;
	return CameraWithinAngle({map_coords::Quantise(transform->position.x), map_coords::Quantise(transform->position.z)}, yAngle,
	                         HalfAngleOfDegrees(degrees), {eye.x, eye.z});
}

} // namespace openblack::field_of_view
