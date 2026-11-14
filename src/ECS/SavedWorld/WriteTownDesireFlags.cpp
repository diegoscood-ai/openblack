/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The towns' desire flags' records of a saved game's world file, in the original's order: the object's parts, the
// flag's town, then its place among the store's points, its desire, the desire it flies for and its mesh
// (docs/wiki/engine/saved-games.md#the-animals-and-the-towns-desire-flags). Every value comes from the entity; the
// place's flags and the object's values openblack does not read come from the record the flag's load kept, and are
// zero for a flag made in play.

#include <cstdint>

#include <optional>
#include <string>
#include <variant>

#include <AnimalRecords.h>
#include <SavedRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/TownDesireFlags.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::LoadedRecord;
using ecs::components::TownDesireFlag;
using gamesave::SavedRecord;
using gamesave::TownDesireFlagsRecord;

namespace
{
/// The number of the row every desire flag is made with, the first prayer site's, among the game's tables (counted
/// from the last row, as the saved games number them): 1856
constexpr uint32_t k_PrayerSiteRow = 1856;

/// The mesh a flag is drawn with: the one its load kept, else the one its desire's row gives as it is made
[[nodiscard]] std::optional<uint32_t> MeshOf(const TownDesireFlag& flag, const TownDesireFlagsRecord* kept)
{
	if (kept != nullptr)
	{
		return kept->mesh;
	}
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& rows = Locator::infoConstants::value().townDesire;
	const auto desire = static_cast<size_t>(flag.desire);
	if (desire >= rows.size())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(rows.at(desire).worshipSiteMesh);
}

/// A desire flag: the place (its height over the ground its own), the object's values (where it was made as its
/// target place, its own scale), its town, its place among the store's points, the desire it flies for, its desire and
/// its mesh
std::optional<SavedRecord> WriteDesireFlag(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& flag = registry.Get<const TownDesireFlag>(entity);
	const auto* loaded = registry.TryGet<const LoadedRecord<TownDesireFlagsRecord>>(entity);
	const auto* kept = loaded != nullptr ? &loaded->record : nullptr;
	const auto mesh = MeshOf(flag, kept);
	if (!mesh.has_value())
	{
		context.Fail("desire flag " + std::to_string(entt::to_integral(entity)) + " has no row for its desire's mesh");
		return std::nullopt;
	}
	TownDesireFlagsRecord record = kept != nullptr ? *kept : TownDesireFlagsRecord {};
	record.position = PositionPartOf(context, entity, kept != nullptr ? &kept->position : nullptr);
	record.position.position.altitude = flag.overGround;
	const auto madeAt = gamesave::MapCoords {.x = map_coords::ToFixed(flag.madeAt.x), .z = map_coords::ToFixed(flag.madeAt.z)};
	record.object = ObjectPartOf(context, entity, kept != nullptr ? &kept->object : nullptr,
	                             kept != nullptr ? kept->object.coords : madeAt, k_PrayerSiteRow);
	record.object.scale = flag.objectScale;
	record.town.number = context.Reference(flag.town);
	record.slot = static_cast<int32_t>(flag.point - ecs::town_desire_flags::k_FirstFlagPoint);
	record.desire = flag.storedDesire;
	record.desireType = static_cast<uint32_t>(flag.desire);
	record.mesh = *mesh;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterTownDesireFlagWriters(ClassWriters& writers)
{
	writers.Register(
	    TownDesireFlagsRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<TownDesireFlag>(entity); }, WriteDesireFlag);
}
