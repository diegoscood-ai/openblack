/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TownDesireSystem.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include <utility>
#include <vector>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/TownDesireFlags.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace town_desire = openblack::ecs::town_desire;
namespace town_desire_flags = openblack::ecs::town_desire_flags;

uint32_t TownDesireSystem::OfferVillager(entt::entity town, bool child, float trigger,
                                         const std::function<uint32_t(TownDesireInfo)>& satisfy)
{
	return town_desire::OfferVillager(town, child, trigger,
	                                  [&satisfy](size_t d) { return satisfy(static_cast<TownDesireInfo>(d)); });
}

float TownDesireSystem::GetDesire(entt::entity town, TownDesireInfo desire) const
{
	return town_desire::GetDesire(town, desire);
}

float TownDesireSystem::GetRawDesire(entt::entity town, TownDesireInfo desire) const
{
	return town_desire::GetRawDesire(town, desire);
}

TownDesireInfo TownDesireSystem::GetMostWanted(entt::entity town) const
{
	return town_desire::GetMostWanted(town);
}

void TownDesireSystem::SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort)
{
	town_desire::SetBoost(town, desire, boost, resort);
}

namespace
{

/// A new store of its town takes the flag to its own place for it, from the store's model, over the ground. The flag
/// leaves the map, takes that position and goes in again, at the tail of the new cell's fixed list even when the cell
/// is the same
void PlaceOnStore(entt::entity entity, TownDesireFlag& flag, entt::entity store)
{
	auto& registry = Locator::entitiesRegistry::value();
	flag.store = store;
	const auto& transform = registry.Get<const Transform>(store);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto meshId = registry.Get<const Mesh>(store).id;
	glm::vec3 local {0.0f};
	if (meshes.Contains(meshId))
	{
		if (const auto& points = meshes.Handle(meshId)->GetExtraMetrics(); flag.point < points.size())
		{
			local = glm::vec3(points.at(flag.point)[3]);
		}
	}
	flag.place = transform.position + (transform.rotation * (local * transform.scale));
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({flag.place.x, flag.place.z}) : 0.0f;
	flag.overGround = flag.place.y - ground;
	ecs::map_cells::RemoveMapObject(entity);
	registry.Get<Transform>(entity).position = flag.place;
	ecs::map_cells::InsertMapObject(entity);
}

/// A flag is drawn only while it shows
void SetDrawn(entt::entity flag, bool drawn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const bool hidden = registry.AllOf<NotDrawn>(flag);
	if (drawn && hidden)
	{
		registry.Remove<NotDrawn>(flag);
	}
	else if (!drawn && !hidden)
	{
		registry.Assign<NotDrawn>(flag);
	}
}

} // namespace

void TownDesireSystem::UpdateFlags()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> orphans;
	registry.Each<TownDesireFlag>([&](entt::entity entity, TownDesireFlag& flag) {
		if (!registry.Valid(flag.town) || !registry.AllOf<Town>(flag.town))
		{
			orphans.push_back(entity);
		}
	});
	for (const auto orphan : orphans)
	{
		// out of the map as it goes
		ecs::map_cells::RemoveMapObject(orphan);
		registry.Destroy(orphan);
	}
}

void TownDesireSystem::OnStoragePitMade(entt::entity town, entt::entity pit)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* flags = registry.TryGet<const TownDesireFlags>(town);
	if (flags == nullptr)
	{
		return;
	}
	for (const auto entity : flags->flags)
	{
		if (auto* flag = entity != entt::null && registry.Valid(entity) ? registry.TryGet<TownDesireFlag>(entity) : nullptr;
		    flag != nullptr)
		{
			PlaceOnStore(entity, *flag, pit);
		}
	}
}

bool TownDesireSystem::DrawFlag(entt::entity entity, float seconds, const glm::vec3& eye)
{
	if (!Locator::infoConstants::has_value())
	{
		std::fputs("town desire flags: no game info in the locator (Locator::infoConstants)\n", stderr);
		std::abort();
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* flag = registry.TryGet<TownDesireFlag>(entity);
	auto* transform = registry.TryGet<Transform>(entity);
	if (flag == nullptr || transform == nullptr || !registry.Valid(flag->town) || !registry.AllOf<Town>(flag->town))
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value().townDesire;
	const auto store = ecs::town_queries::GetStoragePit(flag->town);
	// A store works while built and not hurt down to the share at which it stops working
	const bool working = store != entt::null && ecs::abode_queries::IsFunctional(store);
	const float showsAfter = info.at(static_cast<size_t>(flag->desire)).showsAfterPercent;
	const bool shows = town_desire_flags::Shows(working, flag->height.value, showsAfter);
	// the desire its town's turn left it, held to wholly wanted while it shows
	flag->storedDesire = town_desire_flags::DesireAfterFrame(shows, flag->storedDesire);
	town_desire_flags::Step(flag->height, shows, flag->storedDesire, seconds);
	if (shows != flag->shown)
	{
		flag->shown = shows;
		SetDrawn(entity, shows);
	}
	if (!shows)
	{
		return false;
	}
	auto position = flag->place;
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({position.x, position.z}) : 0.0f;
	position.y = ground + flag->overGround + town_desire_flags::Rise(flag->height.value);
	transform->position = position;
	transform->rotation = graphics::billboard::YawToEye(position, eye);
	// the grow factor alone: the game's flag is drawn at that times 1, whatever its desire's info gives as its scale
	transform->scale = glm::vec3(town_desire_flags::DrawnScale(flag->height.value, showsAfter));
	return true;
}

void TownDesireSystem::ProcessFlags(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* flags = registry.TryGet<TownDesireFlags>(town);
	if (flags == nullptr)
	{
		return;
	}
	// each flag still there takes its town's raw desire with the boosts; one that has gone, or is going (no longer
	// available), leaves its slot empty
	for (auto& entity : flags->flags)
	{
		if (entity == entt::null)
		{
			continue;
		}
		auto* flag = ecs::IsAvailable(entity) ? registry.TryGet<TownDesireFlag>(entity) : nullptr;
		if (flag == nullptr)
		{
			entity = entt::null;
			continue;
		}
		flag->storedDesire = town_desire::GetRawDesire(town, flag->desire);
	}
}

std::optional<TownDesireSystemInterface::FlagToolTip> TownDesireSystem::GetFlagToolTip(entt::entity flag) const
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.Valid(flag) ? registry.TryGet<const TownDesireFlag>(flag) : nullptr;
	if (component == nullptr || !component->shown || !registry.Valid(component->town))
	{
		return std::nullopt;
	}
	return FlagToolTip {.desire = component->desire,
	                    .rawDesire = town_desire::GetRawDesire(component->town, component->desire)};
}
