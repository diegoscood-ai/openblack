/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The score rules of the TownInteract aim: the mind attends to its towns' desires, expands them and its worshippers

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "ComputerPlayerScoresDetail.h"
#include "ECS/Villager/VillagerDisciple.h"

using openblack::map_coords::MapCoords;

namespace openblack::computer_player
{

namespace
{

/// The resources, by their numbers
constexpr uint32_t k_WoodResource = 1;
/// The town desires, by their numbers
constexpr uint32_t k_FoodDesire = 0;
constexpr uint32_t k_WoodDesire = 1;
constexpr uint32_t k_AbodesDesire = 5;
constexpr uint32_t k_CivicDesire = 6;
/// The kinds of disciple, by their numbers
constexpr uint32_t k_BuilderDisciple = 4;
constexpr uint32_t k_BreederDisciple = 5;
/// The number of kinds of disciple
constexpr uint32_t k_DiscipleKinds = 13;
/// A town has enough breeders at this many
constexpr float k_EnoughBreeders = 2.0f;
/// A little, so that nothing divides by 0
constexpr float k_Little = 0.001f;
/// How much a disciple's share counts against the town's want
constexpr double k_DiscipleShareWeight = 0.25;
/// Above this many places in its abodes, a town wants no more abodes from a scaffold
constexpr int32_t k_MostAdultPlaces = 175;
/// A scaffold's pull by the town's want: so much of the want, and so much more
constexpr float k_WantShare = 0.75f;
constexpr float k_WantBase = 0.25f;
/// A scaffold within this of the hand, beyond the influence power, still pulls
constexpr float k_ScaffoldReach = 50.0f;
/// The tries at a town's edge for a scaffold's building, each a quarter turn of a half circle further
constexpr uint32_t k_PlaceTries = 4;
constexpr uint32_t k_EighthTurnBits = 0x3F490FDBu;
constexpr uint32_t k_FullTurnBits = 0x40C90FDBu;
constexpr uint32_t k_TwoThirdsBits = 0x3F2AAAABu;
/// A cell whose corners differ by more than this is too steep to build on
constexpr int32_t k_SteepCell = 10;
/// The share of the counted asks a miracle's charge is worth
constexpr uint32_t k_ChargeShareBits = 0x3C23D70Au;
/// The worshippers each icon of a site wants
constexpr float k_WorshippersPerIcon = 4.0f;
/// Below and above these, a town's will to send worshippers is none or full
constexpr float k_LeastWillToSend = 0.2f;
constexpr float k_FullWillToSend = 0.7f;
/// A villager this near the site's gate is at the site already
constexpr float k_AtTheGate = 50.0f;

/// The best of the player's towns by a want (none for a town that does not count), strictly better than 0; the
/// node takes it
template <typename Want>
float BestTown(scores::ScoreAt& at, entt::entity& chosen, Want want)
{
	float best = 0.0f;
	chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		if (const auto value = want(town); value.has_value() && *value > best)
		{
			best = *value;
			chosen = town;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

/// One of the town's villagers at home, of each sex, that could breed; the first of a sex without a breeder
float BreederCandidate(scores::ScoreAt& at)
{
	const auto above = scores::AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	std::array<entt::entity, 2> candidates {entt::null, entt::null};
	std::array<bool, 2> hasBreeder {false, false};
	for (const auto abode : at.world.TownAbodes(above))
	{
		for (const auto villager : at.world.AbodeInhabitants(abode))
		{
			const auto facts = at.world.VillagerFactsOf(villager);
			// only those at home: a reachable villager is out
			if (!facts.available || facts.atWorshipSite || facts.reachable || facts.child || !facts.sexuallyActive ||
			    at.world.ScriptControlled(villager))
			{
				continue;
			}
			const auto sex = static_cast<size_t>(facts.sex);
			if (facts.disciple)
			{
				if (facts.discipleType == k_BreederDisciple)
				{
					hasBreeder.at(sex) = true;
				}
			}
			else if (candidates.at(sex) == entt::null)
			{
				candidates.at(sex) = villager;
			}
		}
	}
	for (size_t sex = 0; sex < candidates.size(); ++sex)
	{
		if (!hasBreeder.at(sex) && candidates.at(sex) != entt::null)
		{
			at.state().thing = candidates.at(sex);
			return 1.0f;
		}
	}
	return 0.0f;
}

/// The town a thing belongs to, or that the worship site a building site builds serves first
entt::entity TownToWorkFor(const ScoreWorld& world, entt::entity thing)
{
	if (const auto town = world.TownOf(thing); town != entt::null)
	{
		return town;
	}
	if (!world.IsBuildingSite(thing))
	{
		return entt::null;
	}
	const auto building = world.BuildingSiteFactsOf(thing).building;
	if (building == entt::null || !world.IsWorshipSite(building))
	{
		return entt::null;
	}
	const auto towns = world.SiteTowns(building);
	return towns.empty() ? entt::entity {entt::null} : towns.front();
}

/// The town's want of what a scaffold of the value would build
float ScaffoldWant(const ScoreWorld& world, entt::entity town, int32_t value, float fallOff)
{
	switch (value)
	{
	case 1:
	case 2:
	{
		float want = world.RawDesire(town, k_AbodesDesire) * k_WantShare;
		want = want + k_WantBase;
		want = want * fallOff;
		return world.TownAdultPlaces(town) > k_MostAdultPlaces ? 0.0f : want;
	}
	case 3:
	case 4:
	case 5:
	{
		float want = world.RawDesire(town, k_CivicDesire) * k_WantShare;
		want = want + k_WantBase;
		return want * fallOff;
	}
	case 7:
	{
		const float twice = fallOff + fallOff;
		return scores::Below(twice, 1.0f) ? twice : 1.0f;
	}
	default:
		return fallOff;
	}
}

/// The best scaffold of the player's workshops to use, but `skip`: by the distance from the hand, the town's want of
/// what it would build and the influence at it
float BestScaffold(scores::ScoreAt& at, entt::entity& scaffold, entt::entity skip)
{
	const auto hand = at.world.HandPlace();
	const float reach = at.world.InfluencePower(at.world.Me()) + k_ScaffoldReach;
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto abode : at.world.TownAbodes(town))
		{
			if (!at.world.IsWorkshop(abode))
			{
				continue;
			}
			for (const auto candidate : at.world.WorkshopScaffolds(abode))
			{
				if (candidate == entt::null || candidate == skip)
				{
					continue;
				}
				const auto facts = at.world.ScaffoldFactsOf(candidate);
				if (facts.inSite || facts.busy)
				{
					continue;
				}
				const auto place = at.world.Place(candidate);
				const float fallOff = gutils::GetDistanceModifier(gutils::GetDistanceInMetres(hand, place), reach);
				const float want = ScaffoldWant(at.world, town, facts.value, fallOff);
				const float value = InfluenceFactor(at.world, place) * want;
				if (value > best)
				{
					best = value;
					chosen = candidate;
				}
			}
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	scaffold = chosen;
	return best;
}

} // namespace

map_coords::MapCoords ClampCellsOntoMap(const map_coords::MapCoords& place, const ScoreWorld::Cells& cells)
{
	const auto clamp = [](int32_t value, int32_t count) {
		auto cell = static_cast<int16_t>(static_cast<uint32_t>(value) >> 16u);
		if (cell < 0)
		{
			cell = 0;
		}
		else if (cell > count - 1)
		{
			cell = static_cast<int16_t>(count - 1);
		}
		// only the cell moves; the fraction stays
		const auto high = static_cast<uint32_t>(static_cast<uint16_t>(cell)) << 16u;
		return static_cast<int32_t>(high | (static_cast<uint32_t>(value) & 0xFFFFu));
	};
	auto clamped = place;
	clamped.x = clamp(place.x, cells.x);
	clamped.z = clamp(place.z, cells.z);
	return clamped;
}

bool CellIsSteep(const std::optional<std::array<uint8_t, 4>>& corners)
{
	if (!corners.has_value())
	{
		return false;
	}
	const auto [lowest, highest] = std::minmax_element(corners->begin(), corners->end());
	return static_cast<int32_t>(*highest) - static_cast<int32_t>(*lowest) > k_SteepCell;
}

float DesireForWorshippers(const ScoreWorld& world, entt::entity site)
{
	const auto icons = world.SiteIconCount(site);
	if (icons == 0 || !world.SiteBuilt(site))
	{
		return 0.0f;
	}
	const auto share = [&]() {
		const float wanted = static_cast<float>(icons) * k_WorshippersPerIcon;
		return static_cast<float>(world.SiteWorshippers(site)) / wanted;
	};
	const float first = share();
	const float full = scores::Below(first, 1.0f) ? share() : 1.0f;
	float value = full * full;
	value = value * full;
	value = value * full;
	return 1.0f - value;
}

float DesireToGiveWorshippers(const ScoreWorld& world, entt::entity town)
{
	const auto people = world.TownAdults(town) + world.TownChildren(town);
	const auto facts = world.TownWorshipFacts(town);
	if (people == 0 || facts.emergency || !facts.siteFunctional)
	{
		return 0.0f;
	}
	const auto busy = facts.worshipping + facts.onTheWay;
	// the count whole over the people as an integer, rounded once
	float share = static_cast<float>(static_cast<double>(busy) / static_cast<double>(static_cast<int32_t>(people)));
	share = scores::Below(share, 1.0f) ? share : 1.0f;
	const float left = 1.0f - share;
	float value = left * left;
	value = value * left;
	if (!scores::Below(value, k_FullWillToSend))
	{
		return 1.0f;
	}
	return scores::Below(value, k_LeastWillToSend) ? 0.0f : value;
}

float DesireForVillagerAvailableFromTown(const ScoreWorld& world, entt::entity site)
{
	const float want = DesireForWorshippers(world, site);
	if (scores::Same(want, 0.0f))
	{
		return 0.0f;
	}
	float best = 0.0f;
	for (const auto town : world.SiteTowns(site))
	{
		const float will = DesireToGiveWorshippers(world, town);
		if (will > best)
		{
			best = will;
		}
	}
	return best * want;
}

namespace scores
{

/// Wood: over what the mind has seen, the newest first, a forest (its grown trees by its centre tree's wood and the
/// influence there, by the fall-off of its distance from the player's own hand, and by the influence again) or a loose
/// tree (its kind's wood by that fall-off), neither held by a script; the strictly best, its tree; the fall-off of its
/// distance from the mind's hand
float ScoreGetWoodFromForest(ScoreAt& at)
{
	const float power = at.world.InfluencePower(at.world.Me());
	const auto hand = at.world.LeaderHandPlace();
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto& seen : at.mind.seen)
	{
		float value = 0.0f;
		entt::entity candidate = entt::null;
		if (seen.forest.has_value())
		{
			const auto grown = static_cast<float>(at.world.ForestGrownTrees(*seen.forest));
			if (Same(grown, 0.0f))
			{
				continue;
			}
			const auto centre = at.world.ForestCentreTree(*seen.forest);
			if (centre == entt::null || at.world.ScriptControlled(centre))
			{
				continue;
			}
			const auto place = at.world.Place(centre);
			const float factor = InfluenceFactor(at.world, place);
			value = at.world.TreeWoodValue(centre) * factor;
			value = value * grown;
			const float distance = gutils::GetDistanceInMetres(place, hand) - at.world.ThingRadius(centre);
			value = gutils::DistanceChangeToBelief(distance, power) * value;
			value = InfluenceFactor(at.world, place) * value;
			candidate = centre;
		}
		else if (at.world.IsTree(seen.thing))
		{
			if (at.world.ScriptControlled(seen.thing))
			{
				continue;
			}
			const auto wood = static_cast<float>(at.world.TreeKindWood(seen.thing));
			const float distance =
			    gutils::GetDistanceInMetres(at.world.Place(seen.thing), hand) - at.world.ThingRadius(seen.thing);
			value = gutils::DistanceChangeToBelief(distance, power) * wood;
			candidate = seen.thing;
		}
		else
		{
			continue;
		}
		if (value > best)
		{
			best = value;
			chosen = candidate;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return gutils::DistanceChangeToBelief(gutils::GetDistanceInMetres(at.world.HandPlace(), at.world.Place(chosen)), power);
}

float ScoreTownWithHighestFoodDesire(ScoreAt& at)
{
	entt::entity town = entt::null;
	const float best = BestTown(
	    at, town, [&](entt::entity candidate) { return std::optional<float>(at.world.TownDesire(candidate, k_FoodDesire)); });
	if (town == entt::null)
	{
		return 0.0f;
	}
	// the food it is short of, the store's whole count taken off once
	const float desired = at.world.DesiredFood(town);
	const auto available = static_cast<double>(at.world.FoodAvailable(town));
	const auto shortBy = static_cast<float>(static_cast<double>(desired) - available);
	at.state().intValue = static_cast<int32_t>(shortBy);
	return best;
}

float ScoreCreateDiscipleToFulfillTownDesire(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsTown(above))
	{
		return 0.0f;
	}
	const auto town = above;
	if (static_cast<float>(at.world.TownDisciples(town, k_BreederDisciple)) < k_EnoughBreeders)
	{
		const float breeder = BreederCandidate(at);
		if (breeder > 0.0f)
		{
			at.state().intValue = static_cast<int32_t>(k_BreederDisciple);
			return breeder;
		}
	}
	const float worship = at.world.TotemWorship(town);
	float total = k_Little;
	for (uint32_t kind = 0; kind < k_DiscipleKinds; ++kind)
	{
		total = total + static_cast<float>(at.world.TownDisciples(town, kind));
	}
	float best = 0.0f;
	uint32_t chosen = 0;
	for (uint32_t kind = 1; kind < k_DiscipleKinds; ++kind)
	{
		const auto desire = ecs::villager::GetDiscipleInfo(static_cast<uint8_t>(kind)).townDesire;
		if (desire == -1)
		{
			continue;
		}
		float villagers = 1.0f - worship;
		villagers = static_cast<float>(at.world.TownAdults(town)) * villagers;
		villagers = villagers * 0.5f;
		if (Same(villagers, 0.0f))
		{
			continue;
		}
		float share = static_cast<float>(at.world.TownDisciples(town, kind)) + k_Little;
		share = share / total;
		share = 1.0f - share;
		share = static_cast<float>(static_cast<double>(share) * k_DiscipleShareWeight);
		const float ratio = total / villagers;
		const float full = Below(ratio, 1.0f) ? ratio : 1.0f;
		const float left = 1.0f - full;
		float value = at.world.TownDesire(town, static_cast<uint32_t>(desire)) * left;
		value = value * share;
		if (value > best)
		{
			best = value;
			chosen = kind;
		}
	}
	if (chosen == 0)
	{
		return 0.0f;
	}
	const float value = ScoreFindFreeVillagerBuilderInTown(at) * best;
	at.state().intValue = static_cast<int32_t>(chosen);
	return value;
}

float ScoreTownWithHighestWoodDesire(ScoreAt& at)
{
	entt::entity town = entt::null;
	return BestTown(at, town, [&](entt::entity candidate) {
		return at.world.StoragePit(candidate) != entt::null ? std::optional<float>(at.world.TownDesire(candidate, k_WoodDesire))
		                                                    : std::nullopt;
	});
}

float ScoreShallCastWoodSpell(ScoreAt& at)
{
	const auto above = AboveParent(at);
	std::optional<MapCoords> place;
	if (above != entt::null && at.world.IsTown(above))
	{
		if (const auto drop = at.world.ResourceDropObject(above, k_WoodResource); drop != entt::null)
		{
			place = at.world.Place(drop);
		}
	}
	else if (above != entt::null && at.world.HasPlace(above))
	{
		place = at.world.Place(above);
	}
	else if (above != entt::null && at.world.IsBuildingSite(above))
	{
		if (const auto building = at.world.BuildingSiteFactsOf(above).building; building != entt::null)
		{
			place = at.world.Place(building);
		}
	}
	return SpellForNode(at, MagicType::Wood, place);
}

float ScoreGiveBuildingSiteWood(ScoreAt& at)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto site : at.world.TownBuildingSites(town))
		{
			const auto facts = at.world.BuildingSiteFactsOf(site);
			if (!facts.available || facts.building == entt::null || AtMost(facts.woodNeeded, 0.0f) ||
			    (facts.rootBuilt && AtMost(facts.desireToBeRepaired, 0.0f)))
			{
				continue;
			}
			const float share = static_cast<float>(facts.wood) / facts.woodNeeded;
			const float full = Below(share, 1.0f) ? share : 1.0f;
			float filled = full * full;
			filled = filled * full;
			const float left = 1.0f - filled;
			const float value = InfluenceFactor(at.world, at.world.Place(facts.building)) * left;
			if (value > best)
			{
				best = value;
				chosen = site;
			}
		}
		// the first town with a site to give wood to is the one
		if (chosen != entt::null)
		{
			at.state().thing = chosen;
			return best;
		}
	}
	return 0.0f;
}

float ScoreGiveBuildingSiteWorkers(ScoreAt& at)
{
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto site : at.world.TownBuildingSites(town))
		{
			const auto facts = at.world.BuildingSiteFactsOf(site);
			if (!facts.available || facts.building == entt::null || facts.maxBuilders <= 0 || facts.buildersNeeded <= 0 ||
			    (facts.rootBuilt && AtMost(facts.desireToBeRepaired, 0.0f)))
			{
				continue;
			}
			const float builders = static_cast<float>(at.world.TownDisciples(town, k_BuilderDisciple));
			const float share = builders / static_cast<float>(facts.maxBuilders);
			const float full = Below(share, 1.0f) ? share : 1.0f;
			float want = 1.0f - full;
			want = want * facts.desireForVillagers;
			const float value = InfluenceFactor(at.world, at.world.Place(facts.building)) * want;
			if (value > best)
			{
				best = value;
				chosen = site;
			}
		}
		if (chosen != entt::null)
		{
			at.state().thing = chosen;
			return best;
		}
	}
	return 0.0f;
}

float ScoreFindFreeVillagerBuilderInTown(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null)
	{
		return 0.0f;
	}
	const auto town = TownToWorkFor(at.world, above);
	if (town == entt::null)
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto abode : at.world.TownAbodes(town))
	{
		for (const auto villager : at.world.AbodeInhabitants(abode))
		{
			if (FreeVillager(at, villager, best))
			{
				chosen = villager;
			}
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

float ScoreCombineScaffolds(ScoreAt& at)
{
	float abodes = 0.0f;
	float civic = 0.0f;
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		abodes = at.world.RawDesire(town, k_AbodesDesire) + abodes;
		civic = at.world.RawDesire(town, k_CivicDesire) + civic;
	}
	if (AtMost(civic, abodes))
	{
		return 0.0f;
	}
	entt::entity scaffold = entt::null;
	static_cast<void>(BestScaffold(at, scaffold, entt::null));
	if (scaffold == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = scaffold;
	return 1.0f;
}

float ScoreFindScaffoldToCombineWith(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsScaffold(above))
	{
		return 0.0f;
	}
	entt::entity scaffold = above;
	static_cast<void>(BestScaffold(at, scaffold, above));
	if (scaffold == above)
	{
		return 0.0f;
	}
	at.state().thing = scaffold;
	return 1.0f;
}

float ScoreFindPlaceToUseScaffoldForBuilding(ScoreAt& at)
{
	entt::entity scaffold = entt::null;
	static_cast<void>(BestScaffold(at, scaffold, entt::null));
	if (scaffold == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = scaffold;
	return 1.0f;
}

float ScoreFindLocationToUseScaffold(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsScaffold(above))
	{
		return 0.0f;
	}
	const auto value = at.world.ScaffoldFactsOf(above).value;
	// the original makes a trial planned abode here and frees it at the end: its town is none, so it is on no list,
	// and the town's planning only writes into it; openblack's planning writes into a plan of its own
	float best = 0.0f;
	entt::entity chosen = entt::null;
	MapCoords chosenPlace {};
	const auto cells = at.world.MapCells();
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		float want = 1.0f;
		if (static_cast<uint32_t>(value) <= 2)
		{
			want = at.world.RawDesire(town, k_AbodesDesire);
			if (at.world.TownAdultPlaces(town) > k_MostAdultPlaces)
			{
				want = 0.0f;
			}
		}
		else if (static_cast<uint32_t>(value) < 5)
		{
			want = at.world.RawDesire(town, k_CivicDesire);
		}
		const float radius = TownRadius(at.world.TownArea(town));
		const float spread = at.world.GameFloatRand(radius * FromBits(k_TwoThirdsBits));
		const float half = radius * 0.5f;
		const float distance = spread + half;
		float angle = at.world.GameFloatRand(FromBits(k_FullTurnBits));
		for (uint32_t attempt = 0; attempt < k_PlaceTries; ++attempt)
		{
			const auto place =
			    ClampCellsOntoMap(at.world.TownAreaCentre(town) + gutils::GetPosFromAngle(angle, distance), cells);
			if (at.world.TryToBuildAt(town, place, at.world.ScaffoldFactsOf(above).value))
			{
				const auto cellX = static_cast<int32_t>(static_cast<uint32_t>(place.x) >> 16u);
				const auto cellZ = static_cast<int32_t>(static_cast<uint32_t>(place.z) >> 16u);
				if (!CellIsSteep(at.world.CellCorners(cellX, cellZ)) && want > best)
				{
					best = want;
					chosen = town;
					chosenPlace = place;
				}
			}
			angle = angle + FromBits(k_EighthTurnBits);
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	at.state().point = at.world.WorldPoint(chosenPlace);
	return best;
}

float ScoreFillWorkshop(ScoreAt& at)
{
	for (const auto town : at.world.TownsOf(at.world.Me()))
	{
		for (const auto abode : at.world.TownAbodes(town))
		{
			if (!at.world.IsWorkshop(abode) || !at.world.AbodeFunctional(abode) || at.world.WorkshopSpace(abode) == 0 ||
			    at.world.AbodeResource(abode, k_WoodResource) >= at.world.ScaffoldWoodValue())
			{
				continue;
			}
			// the first workshop short of wood
			at.state().thing = abode;
			return static_cast<float>(at.world.WorkshopSpace(abode)) / at.world.WorkshopMostObjects(abode);
		}
	}
	return 0.0f;
}

float ScoreGiveWorshippersFood(ScoreAt& at)
{
	if (!at.world.MyCitadel().has_value())
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : at.world.CitadelSites())
	{
		if (site == entt::null)
		{
			continue;
		}
		if (const float want = at.world.SiteFoodDesire(site); want > best)
		{
			best = want;
			chosen = site;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	at.state().intValue = at.world.SiteFoodNeededFromTown(chosen);
	return best;
}

float ScoreWorshipSiteThatNeedsVillagers(ScoreAt& at)
{
	if (!at.world.MyCitadel().has_value())
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : at.world.CitadelSites())
	{
		if (site == entt::null)
		{
			continue;
		}
		if (const float want = DesireForVillagerAvailableFromTown(at.world, site); want > best)
		{
			best = want;
			chosen = site;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

float ScoreTakeVillagerFromTownToWorshipSite(ScoreAt& at)
{
	const auto above = AboveParent(at);
	if (above == entt::null || !at.world.IsWorshipSite(above))
	{
		return 0.0f;
	}
	const auto gate = at.world.SiteGatePlace(above);
	const auto towns = at.world.SiteTowns(above);
	if (towns.empty())
	{
		return 0.0f;
	}
	// one free to go, out of the gate's reach (and, from an abode, reachable and where the mind can act)
	const auto away = [&](entt::entity villager) {
		return gutils::GetDistanceInMetres(gate, at.world.Place(villager)) > k_AtTheGate;
	};
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto town : towns)
	{
		const float will = DesireToGiveWorshippers(at.world, town);
		if (!(will > best))
		{
			continue;
		}
		entt::entity found = entt::null;
		for (const auto abode : at.world.TownAbodes(town))
		{
			for (const auto villager : at.world.AbodeInhabitants(abode))
			{
				const auto facts = at.world.VillagerFactsOf(villager);
				if (facts.availableForWorship && facts.reachable && away(villager) &&
				    InfluenceFactor(at.world, at.world.Place(villager)) > 0.0f)
				{
					found = villager;
					break;
				}
			}
			if (found != entt::null)
			{
				break;
			}
		}
		if (found == entt::null)
		{
			for (const auto villager : at.world.TownHomeless(town))
			{
				if (at.world.VillagerFactsOf(villager).availableForWorship && away(villager))
				{
					found = villager;
					break;
				}
			}
		}
		if (found != entt::null)
		{
			best = will;
			chosen = found;
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

float ScoreChargeUpSpell(ScoreAt& at)
{
	if (!at.world.MyCitadel().has_value())
	{
		return 0.0f;
	}
	float best = 0.0f;
	entt::entity chosen = entt::null;
	for (const auto site : at.world.CitadelSites())
	{
		if (site == entt::null)
		{
			continue;
		}
		if (at.world.SiteBuilt(site) && at.world.SiteWorshippers(site) != 0)
		{
			for (const auto icon : at.world.SiteIcons(site))
			{
				if (at.world.IconCharging(icon) || !Same(at.world.IconChantStore(icon), 0.0f))
				{
					continue;
				}
				const auto asked = at.mind.iconsFound.at(static_cast<size_t>(at.world.IconMagicType(icon)));
				if (asked == 0)
				{
					continue;
				}
				float value = static_cast<float>(asked) * FromBits(k_ChargeShareBits);
				value = Below(value, 1.0f) ? value : 1.0f;
				if (value > best)
				{
					best = value;
					chosen = icon;
				}
			}
		}
		else if (!at.world.SiteBuilt(site) && at.world.SiteRepaired(site))
		{
			// a repaired site the mind's player has not finished is built at once, as the original does here
			at.world.BuildSite(site, 1.0f);
		}
	}
	if (chosen == entt::null)
	{
		return 0.0f;
	}
	at.state().thing = chosen;
	return best;
}

float ScoreMakeDiscipleBreader(ScoreAt& at)
{
	const auto towns = at.world.TownsOf(at.world.Me());
	if (towns.empty())
	{
		return 0.0f;
	}
	// a town drawn at random, then the next ones round, for one with too few breeders
	const auto start = at.world.GameRand(static_cast<uint32_t>(towns.size()));
	for (size_t step = 0; step < towns.size(); ++step)
	{
		const auto town = towns.at((start + step) % towns.size());
		if (static_cast<float>(at.world.TownDisciples(town, k_BreederDisciple)) < k_EnoughBreeders)
		{
			at.state().thing = town;
			return 1.0f;
		}
	}
	return 0.0f;
}

} // namespace scores

} // namespace openblack::computer_player
