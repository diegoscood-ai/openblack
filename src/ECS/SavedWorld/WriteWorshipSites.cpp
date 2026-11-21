/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The worship sites' records of a saved game's world file, with their altars, what the altars show of the sites' needs
// and the three signs that show it, in the original's order (docs/wiki/engine/saved-games.md#writing-the-world-file,
// the worship sites). A site, an altar or a sign a load made is written as its load kept it; one made in play from the
// entity, each value openblack does not hold as the original's new one starts it.

#include <cstdint>

#include <bit>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <TownRecords.h>
#include <WorshipRecords.h>

#include "ECS/Components/ShowNeeds.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;
using gamesave::CitadelRecord;
using gamesave::SavedRecord;
using gamesave::ShowNeedsRecord;
using gamesave::ShowNeedsVisualsRecord;
using gamesave::WorshipSiteRecord;
using gamesave::WorshipTotemRecord;

namespace
{
/// The worship sites' rows run down from this one, a row for each of the tribes' sites (counted from the last row, as
/// the saved games number them)
constexpr uint32_t k_FirstWorshipSiteRow = 2405;
/// The tribes' rows run down from this one
constexpr uint32_t k_FirstTribeRow = 742;
/// The rows of what the signs show run down from this one: hunger, rest, people hiding, a workshop's wood
constexpr uint32_t k_FirstShowNeedsRow = 1855;
/// The resources a site counts on their way: food, then wood
constexpr size_t k_Resources = 2;

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

/// The row of the site's own info, its tribe's
[[nodiscard]] uint32_t SiteRowOf(const components::WorshipSite& site)
{
	return k_FirstWorshipSiteRow - site.infoIndex;
}

/// The influence a citadel part starts with: its row's
[[nodiscard]] float InfluenceOf(const components::WorshipSite& site)
{
	return Locator::infoConstants::has_value() ? Locator::infoConstants::value().worshipSite.at(site.infoIndex).influence
	                                           : 0.0f;
}

/// What every piece of a citadel writes, for one made in play: the building's parts of the row given at its place,
/// which keeps no height, its row's influence, its citadel and an object never set
[[nodiscard]] gamesave::CitadelPartFields NewPartOf(WriteContext& context, entt::entity entity,
                                                    const components::WorshipSite& site, uint32_t row)
{
	gamesave::CitadelPartFields part;
	part.base = MultiMapFixedFieldsOf(context, entity, nullptr, row);
	part.base.position.position.altitude = 0.0f;
	part.influence = InfluenceOf(site);
	if (site.citadel != entt::null && context.GetRegistry().Valid(site.citadel))
	{
		part.citadel.number = context.Reference(site.citadel, CitadelRecord::k_Type);
	}
	return part;
}

/// A worship site: one a load made as kept, with the spell icons it holds now; one made in play, its citadel part of
/// its tribe's row, its tribe, what its altar shows of its needs, its dance, its towns, its food pot, the villagers at
/// the site and their list, its spell icons, the food on its way (it never counts wood on its way), its six chants, the
/// two cheats, its slot, whether an icon took chants this turn, those asking to go home, its altar, its strain and its
/// pulse, and its mana path. Two words and a byte only the save and the load touch, a word only its making clears, the
/// turn its dance was last checked (never checked) and the cheat's next spell seed are 0, as a new site starts them
std::optional<SavedRecord> WriteSite(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& site = registry.Get<const components::WorshipSite>(entity);
	if (auto kept = context.KeptRecordOf(entity, WorshipSiteRecord::k_Type); kept.has_value())
	{
		// the site's list of its spell icons as it is now, head first: a deleted icon leaves the list as it goes, so
		// one deleted since the load is not named
		auto& record = std::get<WorshipSiteRecord>(*kept);
		record.icons = ListOf(context, site.icons);
		// its worshippers without those gone since, its count down by one for each, as a villager comes off its site's
		// worship list as it is deleted; its dance and its totem none once gone, as the site's links go with them
		const auto worshipping = record.worshipping.numbers.size();
		context.DropGone(record.worshipping);
		for (auto left = worshipping - record.worshipping.numbers.size(); left > 0 && record.worshippers != 0; --left)
		{
			--record.worshippers;
		}
		record.dance.number = context.UnlessGone(record.dance.number);
		record.totem.number = context.UnlessGone(record.totem.number);
		return std::move(*kept);
	}
	WorshipSiteRecord record;
	record.part = NewPartOf(context, entity, site, SiteRowOf(site));
	record.tribeInfo = k_FirstTribeRow - static_cast<uint32_t>(site.tribe);
	const bool hasTotem = site.totem != entt::null && registry.Valid(site.totem);
	record.showNeeds.number = hasTotem ? context.Reference(site.totem, ShowNeedsRecord::k_Type) : 0;
	record.dance.number = ReferenceOrNone(context, site.dance);
	record.towns = ListOf(context, site.towns);
	record.foodPot.number = ReferenceOrNone(context, site.foodPot);
	record.worshippers = static_cast<uint32_t>(site.villagersAtSite);
	record.worshipping = ListOf(context, site.villagers);
	record.icons = ListOf(context, site.icons);
	record.resourcesOnTheWay.values.assign(k_Resources, 0);
	record.resourcesOnTheWay.values.front() = static_cast<uint32_t>(site.foodOnTheWay);
	record.chants = {site.battery, site.sacrificed, site.available, site.used, site.requested, site.chantDamage};
	record.infiniteChants = site.infiniteChants ? 1 : 0;
	record.freeMaintenance = site.freeMaintenance ? 1 : 0;
	record.slot = site.slot;
	record.isSupplyingChants = site.iconTookChants ? 1 : 0;
	record.villagersGoingHome = ListOf(context, site.goHomeRequests);
	record.totem.number = hasTotem ? context.Reference(site.totem, WorshipTotemRecord::k_Type) : 0;
	record.overloaded = site.strain;
	record.manaPath.number =
	    site.manaPathEffect != 0
	        ? context.Reference(SourceKey {.type = gamesave::ParticleInterfaceRecord::k_Type, .id = site.manaPathEffect})
	        : 0;
	record.overloadPulsePhase = site.strainPhase;
	record.overloadPulse = site.strainPulse;
	return record;
}

/// A worship site's altar: one a load made as kept; one made in play, a citadel part of its site's row at its place,
/// with no specular colour (the light a citadel's heart sheds on its parts as it is destroyed is not held), and its site
std::optional<SavedRecord> WriteTotem(WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, WorshipTotemRecord::k_Type); kept.has_value())
	{
		return std::move(*kept);
	}
	const auto& registry = context.GetRegistry();
	const auto& totem = registry.Get<const components::WorshipTotem>(entity);
	if (totem.site == entt::null || !registry.Valid(totem.site))
	{
		context.Fail("worship site's altar " + std::to_string(entt::to_integral(entity)) + " has no site");
		return std::nullopt;
	}
	const auto& site = registry.Get<const components::WorshipSite>(totem.site);
	WorshipTotemRecord record;
	record.part = NewPartOf(context, entity, site, SiteRowOf(site));
	record.site.number = context.Reference(totem.site, WorshipSiteRecord::k_Type);
	return record;
}

/// What an altar shows of its site's needs, which the altar stands for: the altar and its three signs
std::optional<SavedRecord> WriteNeeds(WriteContext& context, entt::entity totemEntity)
{
	const auto& registry = context.GetRegistry();
	const auto& totem = registry.Get<const components::WorshipTotem>(totemEntity);
	ShowNeedsRecord record;
	record.object.number = context.Reference(totemEntity, WorshipTotemRecord::k_Type);
	if (totem.site != entt::null && registry.Valid(totem.site))
	{
		for (const auto sign : registry.Get<const components::WorshipSite>(totem.site).needsSigns)
		{
			record.visuals.numbers.push_back(ReferenceOrNone(context, sign));
		}
	}
	return record;
}

/// A sign of a need: one a load made as kept; one made in play an object of its row at its owner's place with its
/// owner's height (none for an altar), whole, at scale 1 and turned 0; a word its making clears, its own scale (1 as made),
/// what it belongs to (an altar's sign to what the altar shows, a workshop's to the workshop) and the need it shows
std::optional<SavedRecord> WriteSign(WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, ShowNeedsVisualsRecord::k_Type); kept.has_value())
	{
		return std::move(*kept);
	}
	const auto& registry = context.GetRegistry();
	const auto& sign = registry.Get<const components::ShowNeedsVisuals>(entity);
	ShowNeedsVisualsRecord record;
	record.position = PositionPartOf(context, entity, nullptr);
	// made at its owner's place, the height its owner's place keeps too: none for an altar, a workshop's own
	const bool ofAnAltar =
	    sign.owner != entt::null && registry.Valid(sign.owner) && registry.AllOf<components::WorshipTotem>(sign.owner);
	record.position.position.altitude = ofAnAltar || sign.owner == entt::null || !registry.Valid(sign.owner)
	                                        ? 0.0f
	                                        : PositionPartOf(context, sign.owner, nullptr).position.altitude;
	// an object of its own kind's table, which counts as fixed, though no map cell lists it
	record.position.flags |= position_flags::k_CountsAsFixed;
	record.object = ObjectPartOf(context, entity, nullptr, record.position.position, k_FirstShowNeedsRow - sign.infoIndex);
	record.words[1] = std::bit_cast<uint32_t>(1.0f);
	if (sign.owner != entt::null && registry.Valid(sign.owner))
	{
		record.other.number = registry.AllOf<components::WorshipTotem>(sign.owner)
		                          ? context.Reference(sign.owner, ShowNeedsRecord::k_Type)
		                          : context.Reference(sign.owner);
	}
	record.lastWord = std::bit_cast<uint32_t>(sign.desire);
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterWorshipSiteWriters(ClassWriters& writers)
{
	writers.Register(
	    WorshipSiteRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) {
		    return registry.AllOf<components::WorshipSite, components::CitadelPartBuild>(entity);
	    },
	    WriteSite);
	writers.Register(
	    WorshipTotemRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::WorshipTotem>(entity); },
	    WriteTotem);
	// what an altar shows is named only through its altar, by its class: no entity is claimed for it
	writers.Register(ShowNeedsRecord::k_Type, {}, WriteNeeds);
	writers.Register(
	    ShowNeedsVisualsRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::ShowNeedsVisuals>(entity); },
	    WriteSign);
}
