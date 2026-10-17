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

#include <utility>
#include <vector>

#include "3D/Billboard.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/TownDesireFlagArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
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

/// A town's flags, unseen until it has a store standing built: one for each desire whose info gives it a place
void MakeFlags(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().townDesire;
	TownDesireFlags made;
	made.flags.fill(entt::null);
	for (size_t i = 0; i < info.size() && i < made.flags.size(); ++i)
	{
		const auto point = town_desire_flags::PointOf(info.at(i).worshipSiteSlot);
		if (!point.has_value())
		{
			continue;
		}
		made.flags.at(i) = ecs::archetypes::TownDesireFlagArchetype::Create(town, static_cast<TownDesireInfo>(i), *point,
		                                                                    info.at(i).worshipSiteMesh);
	}
	registry.AssignState<TownDesireFlags>(town, made);
}

/// Each new store of its town takes the flag to its own place for it, from the store's model, over the ground
void PlaceOnStore(TownDesireFlag& flag, entt::entity store)
{
	const auto& registry = Locator::entitiesRegistry::value();
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

void TownDesireSystem::UpdateFlags(float seconds)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = Locator::infoConstants::value().townDesire;

	std::vector<entt::entity> towns;
	registry.Each<const Town, const TownDesire>([&](entt::entity town, const Town&, const TownDesire&) {
		if (!registry.AllOf<TownDesireFlags>(town))
		{
			towns.push_back(town);
		}
	});
	for (const auto town : towns)
	{
		MakeFlags(town);
	}

	// A flag holds still while it is out of view, as the game moves it only as it draws it
	const auto* camera = Locator::camera::has_value() ? &Locator::camera::value() : nullptr;
	const auto viewProjection = camera != nullptr ? camera->GetViewProjectionMatrix() : glm::mat4(1.0f);
	const auto eye = camera != nullptr ? camera->GetOrigin() : glm::vec3(0.0f);

	std::vector<entt::entity> orphans;
	std::vector<std::pair<entt::entity, bool>> drawn;
	registry.Each<TownDesireFlag, Transform>([&](entt::entity entity, TownDesireFlag& flag, Transform& transform) {
		if (!registry.Valid(flag.town) || !registry.AllOf<Town>(flag.town))
		{
			orphans.push_back(entity);
			return;
		}
		const auto store = ecs::town_queries::GetStoragePit(flag.town);
		if (store != entt::null && store != flag.store)
		{
			PlaceOnStore(flag, store);
		}
		// A store works while built and not hurt down to the share at which it stops working
		const bool working = store != entt::null && ecs::abode_queries::IsFunctional(store);
		const float desire = town_desire::GetRawDesire(flag.town, flag.desire);
		const float showsAfter = info.at(static_cast<size_t>(flag.desire)).showsAfterPercent;
		const bool shows = town_desire_flags::Shows(working, flag.height.value, showsAfter);
		if (shows && flag.shown && camera != nullptr &&
		    !town_desire_flags::InView(viewProjection * glm::vec4(transform.position, 1.0f)))
		{
			return;
		}
		town_desire_flags::Step(flag.height, shows, desire, seconds);
		if (shows != flag.shown)
		{
			drawn.emplace_back(entity, shows);
		}
		flag.shown = shows;
		if (!shows)
		{
			return;
		}
		auto position = flag.place;
		const float ground =
		    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt({position.x, position.z}) : 0.0f;
		position.y = ground + flag.overGround + town_desire_flags::Rise(flag.height.value);
		transform.position = position;
		transform.rotation = graphics::billboard::YawToEye(position, eye);
		transform.scale = glm::vec3(town_desire_flags::DrawnScale(flag.height.value, showsAfter));
	});
	for (const auto& [flag, shows] : drawn)
	{
		SetDrawn(flag, shows);
	}
	for (const auto orphan : orphans)
	{
		registry.Destroy(orphan);
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
