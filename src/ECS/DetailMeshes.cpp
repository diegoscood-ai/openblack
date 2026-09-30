/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DetailMeshes.h"

#include <spdlog/spdlog.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::detail_meshes
{
namespace
{
bool s_appliedHigh = false;

void SetMesh(components::Mesh& mesh, std::initializer_list<MeshId> detailMeshes, MeshId wanted, int& changed)
{
	for (const auto candidate : detailMeshes)
	{
		if (mesh.id == resources::HashIdentifier(candidate))
		{
			const auto id = resources::HashIdentifier(wanted);
			if (mesh.id != id)
			{
				mesh.id = id;
				++changed;
			}
			return;
		}
	}
}
} // namespace

MeshId Villager(const GVillagerInfo& info, bool child)
{
	if (Locator::config::value().hdPeopleHighDetail)
	{
		return child ? info.childMeshHigh : info.highDetail;
	}
	return child ? info.childMeshMedium : info.stdDetail;
}

MeshId Animal(const GAnimalInfo& info)
{
	return Locator::config::value().hdPeopleHighDetail ? info.high : info.std;
}

void Update()
{
	const bool high = Locator::config::value().hdPeopleHighDetail;
	if (high == s_appliedHigh || !Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		s_appliedHigh = high;
		return;
	}
	s_appliedHigh = high;
	auto& registry = Locator::entitiesRegistry::value();
	const auto& infoConstants = Locator::infoConstants::value();
	int changed = 0;
	registry.Each<const components::Villager, components::Mesh>(
	    [&](entt::entity /*unused*/, const components::Villager& villager, components::Mesh& mesh) {
		    for (const auto& info : infoConstants.villager)
		    {
			    if (info.tribeType == villager.tribe && info.villagerNumber == villager.number)
			    {
				    const bool child = villager.lifeStage == components::Villager::LifeStage::Child;
				    SetMesh(mesh,
				            {info.highDetail, info.stdDetail, info.lowDetail, info.childMeshHigh, info.childMeshMedium,
				             info.childMeshLow},
				            Villager(info, child), changed);
				    return;
			    }
		    }
	    });
	registry.Each<const components::Animal, components::Mesh>(
	    [&](entt::entity /*unused*/, const components::Animal& animal, components::Mesh& mesh) {
		    const auto& info = infoConstants.animal.at(static_cast<size_t>(animal.type));
		    SetMesh(mesh, {info.high, info.std, info.low}, Animal(info), changed);
	    });
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Detail meshes: {} ({} villagers and animals changed)",
	                   high ? "high" : "std, like the original", changed);
}

} // namespace openblack::ecs::detail_meshes
