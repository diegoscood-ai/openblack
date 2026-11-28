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
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LandMovedUnder.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
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

namespace
{
/// A spell icon drawn as built: a town centre's always, a worship site's once its site is built (not under construction,
/// all of it built); one going up is drawn as a building where it was placed
bool IconDrawnBuilt(const Registry& registry, entt::entity icon)
{
	const auto* worship = registry.TryGet<const WorshipSpellIcon>(icon);
	if (worship == nullptr || !registry.Valid(worship->site))
	{
		return true;
	}
	const auto* site = registry.TryGet<const CitadelPartBuild>(worship->site);
	return site == nullptr || ((site->buildFlags & CitadelPartBuild::k_UnderConstruction) == 0 && !(site->percentBuilt < 1.0f));
}
} // namespace

bool land_reseat::FollowsTheLand(const Registry& registry, entt::entity thing)
{
	return registry.AnyOf<Villager, Animal, VillageTotem, Fragment>(thing) || registry.AllOf<Pot, PileSink>(thing) ||
	       (registry.AllOf<SpellIcon>(thing) && IconDrawnBuilt(registry, thing));
}

bool land_reseat::StaysUntilPlaced(const Registry& registry, entt::entity thing)
{
	if (FollowsTheLand(registry, thing) || registry.AllOf<Creature>(thing))
	{
		return false;
	}
	return registry.AnyOf<Tree, Abode, Feature, Field, AnimatedStatic, WorshipSite, MobileStatic, MobileObject, Pot,
	                      SpellDispenser, TeleportStone, Scaffold>(thing) ||
	       registry.AllOf<SpellIcon>(thing);
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
	    entt::exclude<Villager, Animal, Creature>);
	registry.Each<const VillageTotem, const Transform>(
	    [&](entt::entity thing, const VillageTotem&, const Transform& transform) { record(thing, transform); });
	registry.Each<const Fragment, const Transform>(
	    [&](entt::entity thing, const Fragment&, const Transform& transform) { record(thing, transform); });
	registry.Each<const SpellIcon, const Transform>([&](entt::entity thing, const SpellIcon&, const Transform& transform) {
		if (IconDrawnBuilt(registry, thing))
		{
			record(thing, transform);
		}
	});
	registry.Each<const Transform>([&](entt::entity thing, const Transform& transform) {
		if (StaysUntilPlaced(registry, thing))
		{
			record(thing, transform);
		}
	});
	return grounds;
}

size_t land_reseat::Reseat(Registry& registry, std::span<const events::GroundBefore> grounds,
                           const std::function<float(glm::vec2)>& groundAt)
{
	size_t moved = 0;
	for (const auto& [thing, groundBefore] : grounds)
	{
		if (!registry.Valid(thing))
		{
			continue;
		}
		auto* transform = registry.TryGet<Transform>(thing);
		if (transform == nullptr)
		{
			continue;
		}
		const float groundAfter = groundAt(Xz(*transform));
		if (StaysUntilPlaced(registry, thing))
		{
			// drawn where it was placed; its height over the ground it stood on is kept for its next placing
			const auto* marked = registry.TryGet<const LandMovedUnder>(thing);
			if (ReseatedHeight(transform->position.y, groundBefore, groundAfter).has_value() &&
			    (marked == nullptr || marked->standing != transform->position))
			{
				registry.AssignOrReplaceState<LandMovedUnder>(
				    thing, LandMovedUnder {.standing = transform->position, .groundBefore = groundBefore});
			}
			continue;
		}
		if (!FollowsTheLand(registry, thing))
		{
			continue;
		}
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
			const float change = *y - transform->position.y;
			transform->position.y = *y;
			// a town's totem is drawn on the ground at its own point, its icon and the second totem with it, and rises
			// from its rest height there
			if (auto* totem = registry.TryGet<VillageTotem>(thing))
			{
				totem->restY += change;
				for (const auto part : {totem->icon, totem->ghost, totem->ghostIcon})
				{
					if (auto* placed = registry.Valid(part) ? registry.TryGet<Transform>(part) : nullptr)
					{
						placed->position.y += change;
					}
				}
			}
			// the places its drawing glides between move with it, so it is drawn on the new ground at once
			if (auto* drawn = registry.TryGet<DrawPosition>(thing))
			{
				drawn->turnStart.y += change;
			}
			++moved;
		}
	}
	if (moved != 0)
	{
		registry.SetDirty();
	}
	return moved;
}

std::optional<float> land_reseat::KeptAltitude(const Registry& registry, entt::entity thing, glm::vec3 position)
{
	const auto* marked = registry.TryGet<const LandMovedUnder>(thing);
	if (marked == nullptr || marked->standing != position)
	{
		return std::nullopt;
	}
	return position.y - marked->groundBefore;
}

bool land_reseat::PlaceOnTheLandNow(Registry& registry, entt::entity thing, const std::function<float(glm::vec2)>& groundAt)
{
	const auto* marked = registry.TryGet<const LandMovedUnder>(thing);
	if (marked == nullptr)
	{
		return false;
	}
	const auto standing = marked->standing;
	const float groundBefore = marked->groundBefore;
	registry.RemoveState<LandMovedUnder>(thing);
	auto* transform = registry.TryGet<Transform>(thing);
	if (transform == nullptr || transform->position != standing)
	{
		return false;
	}
	const auto y = ReseatedHeight(transform->position.y, groundBefore, groundAt(Xz(*transform)));
	if (!y.has_value() || *y == transform->position.y)
	{
		return false;
	}
	transform->position.y = *y;
	registry.SetDirty();
	return true;
}

bool land_reseat::PlaceOnTheLandNow(Registry& registry, entt::entity thing)
{
	if (!registry.AllOf<LandMovedUnder>(thing) || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto& island = Locator::terrainSystem::value();
	return PlaceOnTheLandNow(registry, thing, [&island](glm::vec2 xz) { return island.GetHeightAt(xz); });
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
	const bool hasPhysics = Locator::dynamicsSystem::has_value();
	return RecordGrounds(
	    Locator::entitiesRegistry::value(), minCorner, maxCorner, [&island](glm::vec2 xz) { return island.GetHeightAt(xz); },
	    [held, hasPhysics](entt::entity thing) {
		    return held == thing || (hasPhysics && Locator::dynamicsSystem::value().IsFlying(thing));
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
