/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandReseat.h"

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/EventManager.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
glm::vec2 Xz(const Transform& transform)
{
	return {transform.position.x, transform.position.z};
}
} // namespace

bool land_reseat::GroundReadsCorners(glm::vec2 xz, glm::ivec2 minCorner, glm::ivec2 maxCorner)
{
	// the ground in a cell is read from its corners (cell) to (cell + 1)
	const glm::ivec2 cell = map_coords::CellOf(xz);
	return cell.x + 1 >= minCorner.x && cell.x <= maxCorner.x && cell.y + 1 >= minCorner.y && cell.y <= maxCorner.y;
}

bool land_reseat::FollowsTheLand(const Registry& registry, entt::entity thing)
{
	return registry.AnyOf<Villager, Animal>(thing) || registry.AllOf<Pot, PileSink>(thing);
}

std::vector<events::GroundBefore> land_reseat::RecordGrounds(Registry& registry, glm::ivec2 minCorner, glm::ivec2 maxCorner,
                                                             const std::function<float(glm::vec2)>& groundAt,
                                                             const std::function<bool(entt::entity)>& offTheGround)
{
	std::vector<events::GroundBefore> grounds;
	const auto record = [&](entt::entity thing, const Transform& transform) {
		if (registry.AnyOf<Unavailable, CarriedByTornado>(thing) ||
		    !GroundReadsCorners(Xz(transform), minCorner, maxCorner) || offTheGround(thing))
		{
			return;
		}
		grounds.push_back({.thing = thing, .ground = groundAt(Xz(transform))});
	};
	registry.Each<const Villager, const Transform>(
	    [&](entt::entity thing, const Villager&, const Transform& transform) { record(thing, transform); });
	registry.Each<const Animal, const Transform>(
	    [&](entt::entity thing, const Animal&, const Transform& transform) { record(thing, transform); },
	    entt::exclude<Villager>);
	registry.Each<const Pot, const PileSink, const Transform>(
	    [&](entt::entity thing, const Pot&, const PileSink&, const Transform& transform) { record(thing, transform); },
	    entt::exclude<Villager, Animal>);
	return grounds;
}

size_t land_reseat::Reseat(Registry& registry, std::span<const events::GroundBefore> grounds,
                           const std::function<float(glm::vec2)>& groundAt)
{
	size_t moved = 0;
	for (const auto& [thing, groundBefore] : grounds)
	{
		if (!registry.Valid(thing) || !FollowsTheLand(registry, thing))
		{
			continue;
		}
		auto* transform = registry.TryGet<Transform>(thing);
		if (transform == nullptr)
		{
			continue;
		}
		const float groundAfter = groundAt(Xz(*transform));
		if (auto* sink = registry.TryGet<PileSink>(thing); sink != nullptr && registry.AllOf<Pot>(thing))
		{
			// a pile is drawn at the ground + its sink offset: its base moves with the ground
			if (const auto base = ReseatedHeight(sink->baseY, groundBefore, groundAfter); base.has_value())
			{
				sink->baseY = *base;
				transform->position.y = sink->baseY + sink->offset.value;
				++moved;
			}
			continue;
		}
		if (const auto y = ReseatedHeight(transform->position.y, groundBefore, groundAfter); y.has_value())
		{
			transform->position.y = *y;
			++moved;
		}
	}
	if (moved != 0)
	{
		registry.SetDirty();
	}
	return moved;
}

std::vector<events::GroundBefore> land_reseat::RecordGroundsUnder(glm::ivec2 minCorner, glm::ivec2 maxCorner)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::terrainSystem::has_value())
	{
		return {};
	}
	const auto& island = Locator::terrainSystem::value();
	std::optional<entt::entity> held;
	if (Locator::handSystem::has_value())
	{
		held = Locator::handSystem::value().GetHeldObject();
	}
	const bool hasPhysics = Locator::physicsObjectsSystem::has_value();
	return RecordGrounds(
	    Locator::entitiesRegistry::value(), minCorner, maxCorner, [&island](glm::vec2 xz) { return island.GetHeightAt(xz); },
	    [held, hasPhysics](entt::entity thing) {
		    return held == thing || (hasPhysics && physics::PhysicsObjects::IsFlying(thing));
	    });
}

void land_reseat::OnLandAltitudesChanged(const events::LandAltitudesChanged& event)
{
	if (event.groundsBefore.empty() || !Locator::entitiesRegistry::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& island = Locator::terrainSystem::value();
	Reseat(Locator::entitiesRegistry::value(), event.groundsBefore, [&island](glm::vec2 xz) { return island.GetHeightAt(xz); });
}

void land_reseat::AddLandReseatEventHandlers(EventManager& manager)
{
	manager.AddHandler<events::LandAltitudesChanged>(OnLandAltitudesChanged);
}
