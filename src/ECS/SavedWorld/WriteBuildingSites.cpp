/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The building sites' records of a saved game's world file, in the original's order: the building, its builders and
// scaffolds, a list nothing fills, a word only the constructor sets, the ring of building positions, the builders'
// count, whether it repairs, the forced desire and the life it measures the repair by; a standard site then its wood
// pile, a citadel's its six piles (docs/wiki/engine/saved-games.md#writing-the-world-file, the building sites).

#include <cstdint>
#include <cstring>

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <SpellIconRecords.h>
#include <VillageRecords.h>

#include "ECS/Components/BuildingSite.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// The number a record names an object by, none (0) for none
[[nodiscard]] gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

[[nodiscard]] gamesave::RefList ListOf(WriteContext& context, const std::vector<entt::entity>& entities)
{
	gamesave::RefList list;
	for (const auto entity : entities)
	{
		if (entity != entt::null && context.GetRegistry().Valid(entity))
		{
			list.numbers.push_back(context.Reference(entity));
		}
	}
	return list;
}

/// The values every building site writes, into a record of either class
template <typename Record>
void FillSite(WriteContext& context, const components::BuildingSite& site, Record& record)
{
	record.rootBuilding.number = ReferenceOrNone(context, site.root);
	record.workers = ListOf(context, site.builders);
	record.scaffolds = ListOf(context, site.scaffolds);
	// the ring as the original holds it: each position's x, y and z in metres
	static_assert(sizeof(site.ring) == sizeof(record.positions));
	std::memcpy(record.positions.data(), site.ring.data(), sizeof(site.ring));
	record.secondWord = static_cast<uint32_t>(site.builderCount);
	record.thirdWord = site.isRepairSite ? 1 : 0;
	record.forcedDesire = site.desireBoost;
	record.life = site.repairBase;
}

std::optional<gamesave::SavedRecord> WriteStandardSite(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& site = registry.Get<const components::BuildingSite>(entity);
	gamesave::StandardBuildingSiteRecord record;
	FillSite(context, site, record);
	record.woodPile.number = ReferenceOrNone(context, site.woodPile);
	return record;
}

std::optional<gamesave::SavedRecord> WriteWorkshopSite(WriteContext& context, entt::entity entity)
{
	const auto& site = context.GetRegistry().Get<const components::BuildingSite>(entity);
	gamesave::WorkshopBuildingSiteRecord record;
	FillSite(context, site, record);
	return record;
}

/// A citadel's site: the building site's values, then its six wood piles by their slots
std::optional<gamesave::SavedRecord> WriteCitadelSite(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& site = registry.Get<const components::BuildingSite>(entity);
	gamesave::CitadelBuildingSiteRecord record;
	FillSite(context, site, record);
	for (const auto pile : registry.Get<const components::CitadelBuildingSite>(entity).piles)
	{
		record.piles.numbers.push_back(ReferenceOrNone(context, pile));
	}
	return record;
}

/// A site of that kind; a citadel's is a class of its own
[[nodiscard]] bool IsKind(const ecs::Registry& registry, entt::entity entity, components::BuildingSite::Kind kind)
{
	const auto* site = registry.TryGet<const components::BuildingSite>(entity);
	return site != nullptr && site->kind == kind && !registry.AllOf<components::CitadelBuildingSite>(entity);
}
} // namespace

void openblack::ecs::saved_world_write::RegisterBuildingSiteWriters(ClassWriters& writers)
{
	writers.Register(
	    gamesave::StandardBuildingSiteRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return IsKind(registry, entity, components::BuildingSite::Kind::Standard);
	    },
	    WriteStandardSite);
	writers.Register(
	    gamesave::CitadelBuildingSiteRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::BuildingSite, components::CitadelBuildingSite>(entity);
	    },
	    WriteCitadelSite);
	writers.Register(
	    gamesave::WorkshopBuildingSiteRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return IsKind(registry, entity, components::BuildingSite::Kind::Workshop);
	    },
	    WriteWorkshopSite);
}
