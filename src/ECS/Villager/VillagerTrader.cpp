/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerTrader.h"

#include "Common/GUtilsDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/DiscipleWorkRules.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerTrace.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"

// The trader disciple (VillagerTrader.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Villager* VillagerOf(entt::entity villager)
{
	return Entities().TryGet<Villager>(villager);
}

/// The trade town, when it is still a town
entt::entity TradeTownOf(const Villager& v)
{
	return v.tradeTown != entt::null && Entities().Valid(v.tradeTown) && Entities().AllOf<Town>(v.tradeTown) ? v.tradeTown
	                                                                                                         : entt::null;
}

/// The villager's town swapped for another for a while, as the trader's work does: its own is put back when the scope
/// ends
class TownSwap
{
public:
	TownSwap(Villager& v, entt::entity town)
	    : _v(v)
	    , _own(v.town)
	{
		_v.town = town;
	}
	TownSwap(const TownSwap&) = delete;
	TownSwap& operator=(const TownSwap&) = delete;
	~TownSwap() { _v.town = _own; }
	[[nodiscard]] entt::entity Own() const { return _own; }

private:
	Villager& _v;
	entt::entity _own;
};

} // namespace

uint32_t CheckTrader(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto tradeTown = TradeTownOf(*v);
	if (tradeTown == entt::null)
	{
		return 0;
	}
	// What the trade town wants, raw, boosted and cheated
	const auto& desire = Entities().Get<const Town>(tradeTown).desire;
	const float wood = town_desire::GetRawDesire(desire, static_cast<size_t>(TownDesireInfo::ForWood));
	const float food = town_desire::GetRawDesire(desire, static_cast<size_t>(TownDesireInfo::ForFood));
	const auto resource = disciple_work::TradeResource(food, wood);
	if (!resource.has_value())
	{
		return SetTraderNothingToDo(villager);
	}
	// The resource is kept where the original keeps its target (cleared with it)
	v->targetThing = entt::null;
	v->tradeResource = *resource;
	TraceFormatted(villager, "trader: food {:.3f} wood {:.3f} -> {}", food, wood,
	               *resource == ResourceType::Food ? "food" : "wood");
	SetTopState(villager, CheckTraderPickUpOrDropOff(villager, *resource));
	return 1;
}

VillagerStates CheckTraderPickUpOrDropOff(entt::entity villager, ResourceType type)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return VillagerStates::DecideWhatToDo;
	}
	// Its own town's drop-off point, then the trade town's
	const auto own = GetResourceDropoffPos(villager, type);
	glm::ivec2 trade {};
	{
		const TownSwap swap(*v, v->tradeTown);
		trade = GetResourceDropoffPos(villager, type);
	}
	// The larger load (which also rewrites the type asked for, as the game's does)
	auto held = type;
	const auto amount = GetResourceHeld(villager, held);
	const auto here = town_queries::PosOf(villager);
	const auto leg =
	    disciple_work::ChooseTradeLeg(amount, InfoOf(villager).maxTraderFoodCarried, gutils::GetDistanceInMetres(here, own),
	                                  gutils::GetDistanceInMetres(here, trade));
	return leg == disciple_work::TradeLeg::Deliver ? VillagerStates::ArrivesAtStoragePitForTraderDropOff
	                                               : VillagerStates::ArrivesAtStoragePitForTraderPickUp;
}

uint32_t ArrivesAtStoragePitForTraderPickUp(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	auto type = v->tradeResource;
	if (type != ResourceType::Food && type != ResourceType::Wood)
	{
		return 0;
	}
	const auto& info = InfoOf(villager);
	const uint32_t limit = type == ResourceType::Food ? info.maxTraderFoodCarried : info.maxTraderWoodCarried;
	// The larger load rewrites the type fetched: a trader carrying more of the other fetches that (kept from the game)
	const auto held = GetResourceHeld(villager, type);
	const auto result = ArrivesAtStoragePitForResource(villager, type, disciple_work::TradeFetchAmount(limit, held),
	                                                   VillagerStates::DecideWhatToDo, VillagerStates::DecideWhatToDo);
	if (result == 0)
	{
		return SetTraderNothingToDo(villager);
	}
	return result;
}

uint32_t ArrivesAtStoragePitForTraderDropOff(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const auto tradeTown = v->tradeTown;
	entt::entity own = entt::null;
	int16_t woodDropped = 0;
	int16_t foodDropped = 0;
	uint32_t result = 0;
	{
		// Dropped at the trade town's store
		const TownSwap swap(*v, tradeTown);
		own = swap.Own();
		const auto woodHeld = static_cast<uint16_t>(v->resourceHeld.at(1));
		const auto foodHeld = static_cast<uint16_t>(v->resourceHeld.at(0));
		result = ArrivesAtStoragePitForDropOff(action);
		v = VillagerOf(villager);
		woodDropped = static_cast<int16_t>(woodHeld - v->resourceHeld.at(1));
		foodDropped = static_cast<int16_t>(foodHeld - v->resourceHeld.at(0));
	}
	// What was dropped leaves its own town's carried totals for the trade town's
	if (auto* town = own != entt::null && Entities().Valid(own) ? Entities().TryGet<Town>(own) : nullptr)
	{
		town->stats.woodCarried -= static_cast<float>(woodDropped);
		town->stats.foodCarried -= static_cast<float>(foodDropped);
	}
	if (auto* town = tradeTown != entt::null && Entities().Valid(tradeTown) ? Entities().TryGet<Town>(tradeTown) : nullptr)
	{
		town->stats.woodCarried += static_cast<float>(woodDropped);
		town->stats.foodCarried += static_cast<float>(foodDropped);
	}
	return result;
}

uint32_t SetTraderNothingToDo(entt::entity villager)
{
	auto* v = VillagerOf(villager);
	if (v == nullptr)
	{
		return 0;
	}
	const TownSwap swap(*v, v->tradeTown);
	return SetDiscipleNothingToDo(villager) != 0 ? 1 : 0;
}
} // namespace openblack::ecs::villager
