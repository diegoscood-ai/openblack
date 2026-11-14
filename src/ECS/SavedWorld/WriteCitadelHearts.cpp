/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The citadels' hearts' records of a saved game's world file, the temples, in the original's order: the parts of a
// building of several cells, what every piece of a citadel adds, then the heart's own values; the citadel its first
// heart stands for, and the heart's entrance (docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town).
// Each value openblack holds comes from the entity; every value openblack does not hold, or whose home is not proved,
// comes from the record the heart's load kept, and for a heart made in play is the one a new heart starts with.

#include <cstdint>

#include <algorithm>
#include <bit>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <PlannedRecords.h>
#include <SavedRecords.h>
#include <TownRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Influence/Influence.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using ecs::components::CitadelEntrance;
using ecs::components::CitadelHeart;
using ecs::components::CitadelWorship;
using ecs::components::LoadedRecord;
using ecs::components::Temple;
using gamesave::CitadelEntranceRecord;
using gamesave::CitadelHeartRecord;
using gamesave::CitadelRecord;
using gamesave::SavedRecord;

namespace
{
/// The number of the citadel heart's row among the game's tables (counted from the last row, as the saved games number
/// them): 2411
constexpr uint32_t k_CitadelHeartRow = 2411;
/// The heart's draw colour keeps its fade in its top byte, white below it
constexpr uint32_t k_DrawColourWhite = 0x00FFFFFF;
/// The citadel's row: 2412
constexpr uint32_t k_CitadelRow = 2412;
/// The entrance is made with no row of its own
constexpr uint32_t k_NoRow = 0xFFFFFFFF;

/// The number a record names one of the heart's objects by, none (0) for none
[[nodiscard]] gamesave::ObjectNumber ReferenceOrNone(WriteContext& context, entt::entity entity)
{
	return entity != entt::null && context.GetRegistry().Valid(entity) ? context.Reference(entity) : 0;
}

/// The heart's own values a heart made in play starts with: its part's influence (its row's), its citadel, its town
/// (the town of the plan it was made from), its alignment flock and its entrance; the word after the citadel and the
/// specular colour are 0 (0 in every save the original made)
[[nodiscard]] CitadelHeartRecord NewHeart(WriteContext& context, entt::entity entity, const CitadelHeart& heart)
{
	CitadelHeartRecord record;
	record.base = MultiMapFixedFieldsOf(context, entity, nullptr, k_CitadelHeartRow);
	// a heart's place keeps no height (0 in every save the original made)
	record.base.position.position.altitude = 0.0f;
	if (Locator::infoConstants::has_value())
	{
		record.influence = Locator::infoConstants::value().citadelHeart.influence;
	}
	if (heart.citadel != entt::null && context.GetRegistry().Valid(heart.citadel))
	{
		record.citadel.number = context.Reference(heart.citadel, CitadelRecord::k_Type);
	}
	record.town.number = ReferenceOrNone(context, heart.town);
	record.alignmentFlock.number = ReferenceOrNone(context, heart.flock);
	record.entrance.number = ReferenceOrNone(context, heart.entrance);
	return record;
}

/// A citadel's heart: the building's parts (its place, its row, its build), its influence, citadel, the object after
/// it, its town and its alignment flock as kept (for one made in play, as it starts); the turn of the last blow that
/// harmed it, its destruction (whether it started, its clock, its plasma beam and its glow, the next plasma's time), its
/// draw colour from its fade, and its beam (where from, at what, at which turn); its specular colour and its entrance
/// as kept
std::optional<SavedRecord> WriteHeart(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto* loaded = registry.TryGet<const LoadedRecord<CitadelHeartRecord>>(entity);
	const auto& heart = registry.Get<const CitadelHeart>(entity);
	const auto& temple = registry.Get<const Temple>(entity);
	CitadelHeartRecord record = loaded != nullptr ? loaded->record : NewHeart(context, entity, heart);
	if (loaded != nullptr)
	{
		record.base = MultiMapFixedFieldsOf(context, entity, &loaded->record.base, k_CitadelHeartRow);
	}
	record.lastImpactTurn = heart.breakingBlowTurn;
	record.drawColour = static_cast<int32_t>((static_cast<uint32_t>(temple.fadeAlpha) << 24U) | k_DrawColourWhite);
	record.destructionStarted = temple.destroying ? 1 : 0;
	record.destructionTime = temple.destructionClock;
	record.destructionPlasma.number = ReferenceOrNone(context, temple.destructionBeamSource);
	record.destructionMagic.number = ReferenceOrNone(context, temple.destructionGlow);
	record.nextPlasmaTime = temple.destructionBeamClock;
	record.transferContainer.number = ReferenceOrNone(context, temple.beamSource);
	record.transferObject.number = ReferenceOrNone(context, temple.beamTarget);
	record.lastTransferTurn = temple.beamTurn;
	return record;
}

/// The pieces of a citadel, newest first, as each joins the head of its list as it is made: the hearts of the citadel,
/// its worship sites and their altars
[[nodiscard]] gamesave::RefList PartsOf(WriteContext& context, entt::entity citadel)
{
	const auto& registry = context.GetRegistry();
	std::vector<entt::entity> parts;
	registry.Each<const CitadelHeart>([&parts, citadel](entt::entity entity, const CitadelHeart& heart) {
		if (heart.citadel == citadel)
		{
			parts.push_back(entity);
		}
	});
	registry.Each<const ecs::components::WorshipSite>(
	    [&registry, &parts, citadel](entt::entity entity, const ecs::components::WorshipSite& site) {
		    if (site.citadel == citadel)
		    {
			    parts.push_back(entity);
			    if (site.totem != entt::null && registry.Valid(site.totem))
			    {
				    parts.push_back(site.totem);
			    }
		    }
	    });
	std::ranges::stable_sort(
	    parts, [](entt::entity a, entt::entity b) { return ecs::object_index::Of(a) > ecs::object_index::Of(b); });
	gamesave::RefList list;
	for (const auto part : parts)
	{
		list.numbers.push_back(registry.AllOf<CitadelHeart>(part) ? context.Reference(part, CitadelHeartRecord::k_Type)
		                                                          : context.Reference(part));
	}
	return list;
}

/// A citadel, which its first heart stands for: a loaded one as its load kept it; one made in play at its heart's
/// place, of the citadel's row and its player, its heart, its worship sites by slot, its pieces, its influence (as its
/// first heart fixed it), the worship strain's sound fraction and whether it may make worship sites. The original's
/// other three lists (one nothing fills, its planned pieces and one more) are empty in every save it made
std::optional<SavedRecord> WriteCitadel(WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, CitadelRecord::k_Type); kept.has_value())
	{
		return std::move(*kept);
	}
	const auto& registry = context.GetRegistry();
	const auto* worship = registry.TryGet<const CitadelWorship>(entity);
	if (worship == nullptr)
	{
		context.Fail("citadel " + std::to_string(entt::to_integral(entity)) +
		             " was made in play and has no worship sites' slots");
		return std::nullopt;
	}
	const auto& temple = registry.Get<const Temple>(entity);
	CitadelRecord record;
	record.container.position.position = PositionPartOf(context, entity, nullptr).position;
	record.container.position.position.altitude = 0.0f;
	record.container.info = k_CitadelRow;
	record.container.owner.number = PlayerReference(context, temple.owner);
	record.heart.number = context.Reference(entity, CitadelHeartRecord::k_Type);
	for (const auto site : worship->sites)
	{
		record.parts.numbers.push_back(ReferenceOrNone(context, site));
	}
	record.firstList = PartsOf(context, entity);
	// its influence as its first heart fixes it, stored or not yet
	record.firstWord = std::bit_cast<uint32_t>(influence::CitadelReach(entity));
	record.secondWord = std::bit_cast<uint32_t>(worship->strainSoundFraction);
	record.thirdWord = worship->cannotCreateSites ? 1 : 0;
	return record;
}

/// A citadel's entrance, made with its heart: at the heart's place, with no row, the next creation number after the
/// heart's, whole, at scale 1 and turned 0, counting as fixed
std::optional<SavedRecord> WriteEntrance(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& entrance = registry.Get<const CitadelEntrance>(entity);
	if (entrance.heart == entt::null || !registry.Valid(entrance.heart))
	{
		context.Fail("citadel entrance " + std::to_string(entt::to_integral(entity)) + " has no heart");
		return std::nullopt;
	}
	CitadelEntranceRecord record;
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entrance.heart); transform != nullptr)
	{
		const auto coords = map_coords::FromWorld(transform->position);
		record.position.position = {.x = coords.x, .z = coords.z, .altitude = 0.0f};
	}
	record.position.flags = position_flags::k_CountsAsFixed;
	record.object.info = k_NoRow;
	record.object.coords = record.position.position;
	const auto heartIndex = ecs::object_index::Of(entrance.heart);
	record.object.creationIndex = heartIndex >= 0 ? static_cast<uint32_t>(heartIndex) + 1 : 0;
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterCitadelHeartWriters(ClassWriters& writers)
{
	writers.Register(
	    CitadelHeartRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<CitadelHeart, Temple>(entity); }, WriteHeart);
	// a citadel is named only through its first heart, by its class: no entity is claimed as a citadel
	writers.Register(CitadelRecord::k_Type, {}, WriteCitadel);
	writers.Register(
	    CitadelEntranceRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<CitadelEntrance>(entity); }, WriteEntrance);
}
