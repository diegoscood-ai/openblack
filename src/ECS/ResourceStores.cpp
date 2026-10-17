/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ResourceStores.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>

#include "ECS/Abodes.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/TakeResource.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/Workshops.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

entt::entity resource_stores::StructureOfPile(entt::entity pile)
{
	if (const auto pit = StoragePitStore::OwnerOf(pile); pit != entt::null)
	{
		return pit;
	}
	if (const auto workshop = workshops::WorkshopOfPile(pile); workshop != entt::null)
	{
		return workshop;
	}
	// a building site's wood pile (a citadel site's six too) is part of the building the site is for, the one it was
	// made for (kept after it is made)
	if (const auto site = building_sites::SiteOfPile(pile); site != entt::null)
	{
		return building_sites::GetRootBuilding(site);
	}
	// a worship site's food pot is part of the worship site
	return WorshipSiteOfFoodPot(pile);
}

entt::entity resource_stores::WorshipSiteOfFoodPot(entt::entity pot)
{
	entt::entity found = entt::null;
	if (pot == entt::null)
	{
		return found;
	}
	Locator::entitiesRegistry::value().Each<const WorshipSite>([pot, &found](entt::entity entity, const WorshipSite& site) {
		if (site.foodPot == pot)
		{
			found = entity;
		}
	});
	return found;
}

namespace
{
/// The resource type of a pot's info, or none
ResourceType PotResourceType(const Pot& pot)
{
	if (pot.type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return ResourceType::None;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
}

/// The building site of a building, or the one a scaffold belongs to
entt::entity BuildingSiteOf(entt::entity object)
{
	if (const auto* scaffold = Locator::entitiesRegistry::value().TryGet<const Scaffold>(object); scaffold != nullptr)
	{
		return scaffold->site;
	}
	return abodes::GetBuildingSite(object);
}
} // namespace

bool resource_stores::IsStoreForType(StoreKind kind, ResourceType type, bool hasBuildingSite)
{
	const bool siteWood = type == ResourceType::Wood && hasBuildingSite;
	switch (kind)
	{
	case StoreKind::StoragePit:
		return true;
	case StoreKind::Abode:
	case StoreKind::Scaffold:
		return siteWood;
	case StoreKind::Workshop:
		return siteWood || type == ResourceType::Wood || type == ResourceType::Any;
	case StoreKind::WorshipSite:
		return siteWood || type == ResourceType::Food || type == ResourceType::Any;
	case StoreKind::StructurePile: // answered with its structure (PileIsStoreForType)
	case StoreKind::LoosePot:
	case StoreKind::None:
		return false;
	}
	return false;
}

bool resource_stores::PileIsStoreForType(bool structureTakesType, ResourceType type, ResourceType pileType)
{
	return structureTakesType && (type == pileType || type == ResourceType::Any);
}

resource_stores::StoreKind resource_stores::KindOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (object == entt::null || !registry.Valid(object))
	{
		return StoreKind::None;
	}
	// the storage pit first: it is a building too
	if (registry.AllOf<StoragePit>(object))
	{
		return StoreKind::StoragePit;
	}
	if (registry.AllOf<Pot>(object))
	{
		return StructureOfPile(object) != entt::null ? StoreKind::StructurePile : StoreKind::LoosePot;
	}
	if (workshops::IsWorkshop(object))
	{
		return StoreKind::Workshop;
	}
	if (registry.AllOf<WorshipSite>(object))
	{
		return StoreKind::WorshipSite;
	}
	if (registry.AllOf<Abode>(object))
	{
		return StoreKind::Abode;
	}
	if (registry.AllOf<Scaffold>(object))
	{
		return StoreKind::Scaffold;
	}
	return StoreKind::None;
}

bool resource_stores::IsResourceStore(entt::entity object, ResourceType type)
{
	const auto kind = KindOf(object);
	if (kind == StoreKind::StructurePile)
	{
		const auto& pot = Locator::entitiesRegistry::value().Get<const Pot>(object);
		return PileIsStoreForType(IsResourceStore(StructureOfPile(object), type), type, PotResourceType(pot));
	}
	return IsStoreForType(kind, type, BuildingSiteOf(object) != entt::null);
}

bool resource_stores::DeleteObjectAndTakeResource(entt::entity store, entt::entity object, const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	switch (KindOf(store))
	{
	case StoreKind::StoragePit:
		return take_resource::StoragePit(store, object, dropper);
	case StoreKind::WorshipSite:
		return worship::site::DeleteObjectAndTakeResource(store, object, dropper);
	case StoreKind::StructurePile:
	case StoreKind::LoosePot:
	case StoreKind::Workshop:
		object_delivery::DoDeleteObjectAndTakeResource(store, object, dropper);
		return true;
	case StoreKind::Abode:
	case StoreKind::Scaffold:
		// a building or a scaffold takes a given object only on its building site
		if (BuildingSiteOf(store) == entt::null)
		{
			return false;
		}
		object_delivery::DoDeleteObjectAndTakeResource(store, object, dropper);
		return true;
	case StoreKind::None:
		return false;
	}
	return false;
}

ecs::systems::ResourceStoreSystemInterface& ecs::resource_stores::Service()
{
	if (!Locator::resourceStoreSystem::has_value())
	{
		std::fputs("resource stores: no resource store service in the locator (Locator::resourceStoreSystem)\n", stderr);
		std::abort();
	}
	return Locator::resourceStoreSystem::value();
}
