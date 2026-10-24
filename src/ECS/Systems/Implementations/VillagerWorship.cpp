/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerWorship.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Debug/TraceLog.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WalkArrival.h"
#include "ECS/WorshipperRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"
#include "Worship/WorshipSite.h"
#include "Worship/WorshipTrace.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace rules = openblack::ecs::worshipper_rules;

namespace
{

/// The state's row of info.dat villagerStateTable
const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

/// The teleport stones (Locator::teleportSystem)
const ecs::systems::TeleportSystemInterface& TheTeleportSystem()
{
	if (!Locator::teleportSystem::has_value())
	{
		std::fputs("ecs::villager_worship: no teleport service in the locator (Locator::teleportSystem)\n", stderr);
		std::abort();
	}
	return Locator::teleportSystem::value();
}

/// (not named Registry: with `using namespace openblack::ecs` in scope, outside this anonymous namespace `Registry()`
/// would be a functional cast that builds an empty ecs::Registry instead of calling the helper)
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity TownOf(entt::entity villager)
{
	const auto* component = Entities().TryGet<const Villager>(villager);
	// (guard) the original reads this link with no availability test
	if (component == nullptr || component->town == entt::null || !ecs::IsAvailable(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// The villager's worship site: its town's. A villager with no town has none: the original would then look for its
/// citadel's site of its tribe, but a villager's citadel is its town's player's, so that is never reached
entt::entity SiteOf(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return entt::null;
	}
	const auto* magic = Entities().TryGet<const TownMagic>(town);
	// (guard) the original reads this link with no availability test
	if (magic == nullptr || magic->worshipSite == entt::null || !ecs::IsAvailable(magic->worshipSite))
	{
		return entt::null;
	}
	return magic->worshipSite;
}

WorshipVillager& StateOf(entt::entity villager)
{
	auto& registry = Entities();
	if (auto* state = registry.TryGet<WorshipVillager>(villager); state != nullptr)
	{
		return *state;
	}
	return registry.Assign<WorshipVillager>(villager);
}

void SetState(entt::entity villager, VillagerStates state)
{
	auto* action = Entities().TryGet<LivingAction>(villager);
	if (action != nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, false);
	}
}

float FlatDistance(const glm::vec3& a, const glm::vec3& b)
{
	return glm::distance(glm::xz(a), glm::xz(b));
}

glm::vec3 PositionOf(entt::entity villager)
{
	return Entities().Get<const Transform>(villager).position;
}

/// Into the site's count and list of villagers, and the town's worshippers
void AddVillagerToWorshipSite(entt::entity villager, entt::entity siteEntity)
{
	auto& state = StateOf(villager);
	if (!state.atSite)
	{
		worship::percentage::AddWorshipper(TownOf(villager));
		state.atSite = true;
		state.requestedGoHome = false;
	}
	// into the site's list of villagers (at the head), counted only when new
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	if (std::ranges::find(site.villagers, villager) == site.villagers.end())
	{
		site.villagers.insert(site.villagers.begin(), villager);
		++site.villagersAtSite;
	}
}

/// With a count, off the list (if it is there) and the count one down
void RemoveVillagerFromWorshipCount(WorshipSite& site, entt::entity villager)
{
	if (site.villagersAtSite == 0)
	{
		return;
	}
	std::erase(site.villagers, villager);
	--site.villagersAtSite;
}

} // namespace

bool villager_worship::IsAtWorshipSite(entt::entity villager)
{
	const auto* state = Entities().TryGet<const WorshipVillager>(villager);
	return state != nullptr && state->atSite;
}

/// Public for the town's villager removal
void villager_worship::RemoveVillagerFromWorshipSite(entt::entity villager)
{
	auto& state = StateOf(villager);
	const auto town = TownOf(villager);
	// with no town, only the flags below
	if (town != entt::null)
	{
		if (state.atSite)
		{
			worship::percentage::RemoveWorshipper(town);
		}
		if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
		{
			// only when the villager is in the site's list
			auto& site = Entities().Get<WorshipSite>(siteEntity);
			if (std::ranges::find(site.villagers, villager) != site.villagers.end())
			{
				RemoveVillagerFromWorshipCount(site, villager);
			}
		}
		// out of the dance group, with its site or without
		worship::site::LeaveDance(villager);
	}
	state.atSite = false;
	state.dancing = false;
	state.requestedGoHome = false;
}

namespace
{

/// Off the site's go-home queue
void RemoveGoHomeRequest(entt::entity siteEntity, entt::entity villager)
{
	auto& state = StateOf(villager);
	if (!state.requestedGoHome || siteEntity == entt::null)
	{
		return;
	}
	auto& queue = Entities().Get<WorshipSite>(siteEntity).goHomeRequests;
	const auto it = std::ranges::find(queue, villager);
	if (it == queue.end())
	{
		return;
	}
	state.requestedGoHome = false;
	queue.erase(it);
}

/// The town's centre is functional and built. (inferred) both tests are skipped: a town centre that exists counts as
/// ready
bool TownCentreReady(entt::entity town)
{
	return worship::town::TownCentreOf(town) != entt::null;
}

/// 1 when the site is within the town info's maxDistanceThatVillagersWillGoToWorship (flat distance, <=). Farther,
/// the villager's player looks for a teleport stone that shortens the trip (TeleportSystem RouteStoneFor): 1 with that
/// stone in `routeStone`, 0 when it finds none. Without a player it is 1 with no stone.
bool CanIGetToTheWorshipSite(entt::entity villager, entt::entity siteEntity, entt::entity* routeStone = nullptr)
{
	const float maximum = Locator::infoConstants::value().town.maxDistanceThatVillagersWillGoToWorship;
	const auto at = PositionOf(villager);
	const auto site = Entities().Get<const Transform>(siteEntity).position;
	const float distance = FlatDistance(at, site);
	// farther: the player and its stones are asked (Locator::teleportSystem)
	bool hasPlayer = false;
	std::optional<entt::entity> stone;
	if (!(distance <= maximum))
	{
		if (const auto player = villager_teleport::PlayerOf(villager); player.has_value())
		{
			hasPlayer = true;
			stone = TheTeleportSystem().RouteStoneFor(*player, at, site, maximum);
		}
	}
	const bool reachable = rules::CanReachSite(distance, maximum, hasPlayer, stone.has_value());
	if (reachable && stone.has_value() && routeStone != nullptr)
	{
		*routeStone = *stone;
	}
	return reachable;
}

/// The reaction record of that type (components::ReactionRecords) gets the game turn; without one a new {type, turn}
/// record is appended at the tail, the head (the oldest) dropped first when the list already holds 3. Unlike
/// reactions::Records it forgets nothing older than 1800 turns, and unlike StopReacting's RefreshRecord it adds
void SetReactionDoneWhen(entt::entity living, uint8_t type)
{
	auto& registry = Entities();
	auto& memory = registry.AllOf<ReactionRecords>(living) ? registry.Get<ReactionRecords>(living)
	                                                       : registry.Assign<ReactionRecords>(living);
	const auto now = effects::reactions::Turn();
	for (uint8_t i = 0; i < memory.count; ++i)
	{
		if (memory.records[i].type == type)
		{
			memory.records[i].turn = now;
			return;
		}
	}
	if (memory.count >= memory.records.size())
	{
		std::copy(memory.records.begin() + 1, memory.records.end(), memory.records.begin());
		--memory.count;
	}
	memory.records[memory.count++] = {type, now};
}

/// The site's gate (its arrive point) as the map's coordinates
glm::ivec2 GateCoords(const glm::vec3& gate)
{
	return town_queries::ToMapCoords(glm::xz(gate));
}

/// The walk to the site has started
bool SetOffForWorship(entt::entity villager, entt::entity siteEntity, entt::entity town)
{
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	++site.dancersOnWay;
	StateOf(villager).onWay = true;
	// from outside the site: the walk to its gate, on the site's footpaths, then 59; from on the site, 59 at once.
	// (guard) without the mesh's gate point, 59 at once too
	const auto gate = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	const bool walks = gate.has_value() && rules::WalksToTheGate(ecs::object::GetDistanceFromObject(siteEntity, villager));
	if (walks)
	{
		static_cast<void>(ecs::villager::SetupMoveToOnFootpath(villager, siteEntity, GateCoords(*gate),
		                                                       VillagerStates::ArrivesAtWorshipSiteForWorship));
	}
	else
	{
		static_cast<void>(ecs::villager::SetTopState(villager, VillagerStates::ArrivesAtWorshipSiteForWorship));
	}
	if (town != entt::null)
	{
		worship::percentage::AddVillagerOnWay(town, villager);
		StateOf(villager).onWayInTown = true;
	}
	if (worship::trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("ai"), "Worship trace: villager {} goes to worship site {} ({})",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity),
		                   walks ? "walking to its gate" : "at its gate");
	}
	return true;
}

/// A worship disciple stops being one as it sets off, found a site or not; once it has set off, its town's share is
/// raised to keep it at worship
void HandOverWorshipDisciple(entt::entity villager, entt::entity town, bool setOff)
{
	const auto* component = Entities().TryGet<const Villager>(villager);
	if (component == nullptr || component->discipleType != static_cast<uint8_t>(VillagerDisciple::Worship))
	{
		return;
	}
	ecs::villager::SetVillagerDisciple(villager, entt::null, VillagerDisciple::None, 0);
	if (town != entt::null && setOff)
	{
		worship::percentage::IncrementWorshipPercentage(town);
	}
}

/// One more dancer on the way and the on-way flag, the walk to the gate with state 59 to follow (or 59 at once on the
/// site), the town's count of villagers on the way, only to a site that is built. Then the worship disciple's
/// hand-over
bool GotoWorshipSiteForWorship(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	const auto town = TownOf(villager);
	const bool setOff =
	    siteEntity != entt::null && abode_queries::IsBuilt(siteEntity) && SetOffForWorship(villager, siteEntity, town);
	HandOverWorshipDisciple(villager, town, setOff);
	return setOff;
}

/// Into the dance group, then the walk to its dance place with state 60 to follow; counted at the site only when the
/// walk is set up
bool StartWorshippingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	worship::site::AddDancer(siteEntity, villager); // finds the dance group
	StateOf(villager).dancing = true;
	const auto place = worship::site::DancePosition(siteEntity, villager);
	if (ecs::villager::SetupMoveToPos(villager, glm::xz(place), VillagerStates::WorshippingAtWorshipSite) == 0)
	{
		return false;
	}
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// The walk to the site's door, on the site's footpaths, with state 213 to follow, where it worships hidden; counted at
/// the site only when the walk is set up
bool StartHidingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	// (guard) only the arrival at the gate calls it, once it has found the site
	if (siteEntity == entt::null)
	{
		return false;
	}
	if (!ecs::villager::SetupMoveToOnFootpath(villager, siteEntity, abode_queries::GetDoorPos(siteEntity),
	                                          VillagerStates::HidingAtWorshipSite))
	{
		return false;
	}
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// The site gone or not the town's player's -> 163; fewer needed and first in the go-home queue (an empty queue counts
/// as first, read) -> 248. 1 when it left.
bool CheckVillagerGoBackToTownFromWorship(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	const auto siteEntity = SiteOf(villager);
	const bool canWorship = siteEntity != entt::null && TownCentreReady(town) &&
	                        Entities().Get<const WorshipSite>(siteEntity).player == worship::town::OwnerOf(town);
	int needed = 0;
	bool mayGoHome = false;
	if (canWorship)
	{
		needed = worship::percentage::GetWorshipersNeeded(town, false, false, nullptr);
		// the first of the go-home queue may go (an empty queue lets anyone go; nothing joins it, as in the original)
		const auto& queue = Entities().Get<const WorshipSite>(siteEntity).goHomeRequests;
		mayGoHome = queue.empty() || queue.front() == villager;
	}
	switch (rules::CheckGoingBack(canWorship, needed, mayGoHome))
	{
	case rules::GoingBack::Decide:
		SetState(villager, VillagerStates::DecideWhatToDo);
		return true;
	case rules::GoingBack::GoHome:
		// SetTopState(248) == 1 -> 1. Its code, not the TOP: a pause first (239 with FINAL 248) also returns 1 and the
		// villager has left
		return ecs::villager::SetTopState(villager, VillagerStates::GoHomeFromWorship) == ecs::villager::k_Done;
	case rules::GoingBack::Stay:
		break;
	}
	return false;
}

/// life - chantDamage x the villager's own chantLifeRate; at 0 VillagerDead (reason 4, worship). 0x21 when it died.
uint32_t ReduceVillagerLifeByChant(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return 1;
	}
	const auto& site = Entities().Get<const WorshipSite>(siteEntity);
	const float rate = ecs::villager::InfoOf(villager).chantLifeRate;
	const float damage = rules::ChantDamage(site.chantDamage, rate);
	life::ReduceLife(villager, damage);
	if (life::LifeOf(villager) > 0.0f)
	{
		return 1;
	}
	// VillagerDead(4 CHANT, town ? town's player : none, chantDamage x chantLifeRate, 1) (ECS/Villager/
	// VillagerDeath.h); 0x21. The worship-death count is the town's TownDeaths::byReason[4], and the site lets the
	// villager go through the worship state's exit (dying sets the top state 14)
	ecs::villager::VillagerDead(villager, DeathReason::Chant, ecs::villager::GetPlayerOf(villager), damage, 1);
	return 0x21;
}

/// 0x23 when it stops (left, died, or went to eat), else 1. A tired worshipper does not ask to go home: the original
/// has that check but never calls it, so the site's go-home queue stays empty and any worshipper may leave when fewer
/// are needed
uint32_t ProcessInWorship(entt::entity villager)
{
	if (CheckVillagerGoBackToTownFromWorship(villager))
	{
		return 0x23;
	}
	if (ReduceVillagerLifeByChant(villager) == 0x21)
	{
		return 0x23;
	}
	return villager_worship::CheckAllowedToRestAtWorshipSite(villager, true) ? 0x23 : 1;
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

/// The food the town's site wants from it now (its site's CalculateFoodNeededFromTown), but none when the town cannot
/// spare it all: its storage pit's food beyond its people's dinner
uint32_t FoodForWorshipSiteIfEnough(entt::entity town)
{
	const auto* magic = Entities().TryGet<const TownMagic>(town);
	const auto siteEntity = magic != nullptr && magic->worshipSite != entt::null && ecs::IsAvailable(magic->worshipSite)
	                            ? magic->worshipSite
	                            : entt::null;
	const int32_t wanted = siteEntity != entt::null ? worship::site::CalculateFoodNeededFromTown(siteEntity) : 0;
	std::optional<uint32_t> pitFood;
	if (const auto pit = town_queries::GetStoragePit(town); pit != entt::null)
	{
		pitFood = object_resources::GetResource(pit, ResourceType::Food);
	}
	const auto* component = Entities().TryGet<const Town>(town);
	const float dinner = component != nullptr ? component->stats.foodForDinner : 0.0f;
	return static_cast<uint32_t>(rules::FoodForSiteIfEnough(wanted, rules::FoodSpareForWorship(pitFood, dinner)));
}

/// The food it carries, as the original reads it (a signed 16-bit count)
int32_t FoodHeld(entt::entity villager)
{
	const auto* component = Entities().TryGet<const Villager>(villager);
	return component != nullptr ? component->resourceHeld.at(0) : 0;
}
} // namespace

bool villager_worship::CheckNeededForWorship(entt::entity villager)
{
	if (StateOf(villager).atSite)
	{
		if (!StartWorshippingAtWorshipSite(villager))
		{
			RemoveVillagerFromWorshipSite(villager);
			return false;
		}
		return true;
	}
	const auto town = TownOf(villager);
	if (town == entt::null || worship::percentage::GetWorshipPercentage(town) == 0.0f)
	{
		return false;
	}
	bool reachable = true;
	if (worship::percentage::GetWorshipersNeeded(town, true, true, &reachable) <= 0)
	{
		return false;
	}
	return CheckWorshipActivity(villager, reachable);
}

bool villager_worship::CheckWorshipActivity(entt::entity villager, bool requireReachable)
{
	const auto town = TownOf(villager);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null || town == entt::null || !TownCentreReady(town))
	{
		return false;
	}
	if (Entities().Get<const WorshipSite>(siteEntity).player != worship::town::OwnerOf(town))
	{
		return false;
	}
	entt::entity routeStone = entt::null;
	if (!CanIGetToTheWorshipSite(villager, siteEntity, &routeStone) && requireReachable)
	{
		return false;
	}
	// the worship site still to be built (its building site in the town's list) -> no GotoWorshipSiteForWorship. Its
	// site is the villager's (not null here: the citadel's site creation is not reached); without a building site in the
	// list 0; with one, SetTopState(163), then SetupBuildingObject == 1 -> 1
	bool building = false;
	if (const auto buildingSite = building_sites::GetBuildingSiteInList(town, siteEntity); buildingSite != entt::null)
	{
		ecs::villager::SetTopState(villager, VillagerStates::DecideWhatToDo);
		building = ecs::villager::SetupBuildingObject(villager, buildingSite) == 1;
	}
	if (!building && !GotoWorshipSiteForWorship(villager))
	{
		return false;
	}
	// when a teleport stone is what makes the site reachable, the walk goes through it. The REACT_TO_TELEPORT record
	// gets the turn (SetReactionDoneWhen: the record's turn, added when there is none, so the villager will not take
	// another teleport reaction for a while), the villager leaves the reactor list of the reaction it was following
	// (openblack keeps no reactor list in a Reaction, and SetupReactToTeleport replaces its slot anyway), and starting
	// the teleport reaction (which goes to SetupReactToTeleport) pushes GO_TOWARDS_TELEPORT_REACTION over the walk
	// that was just started; TELEPORT_REACTION jumps it and popping back puts it back on the way to the site
	if (routeStone != entt::null)
	{
		SetReactionDoneWhen(villager, static_cast<uint8_t>(openblack::Reaction::ReactToTeleport));
		villager_teleport::SetupReactToTeleport(villager, routeStone, magic::teleport::ReactionOf(routeStone));
		if (debug_env::TraceLogs(worship::trace::Enabled() || magic::teleport::TraceEnabled(), "ai"))
		{
			SPDLOG_LOGGER_INFO(
			    spdlog::get("ai"), "Worship trace: villager {} goes to worship site {} through teleport stone {}",
			    static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity), static_cast<uint32_t>(routeStone));
		}
	}
	return true;
}

bool villager_worship::IsAvailableForWorshipSite(entt::entity villager, bool secondPass)
{
	// IsVillagerAvailable: not controlled by a script, not in the hand, and the availability bit of its final state's
	// row, so that a villager on a walk is judged by where the walk takes it
	if (!ecs::villager::IsVillagerAvailable(villager))
	{
		return false;
	}
	// a disciple goes only on the second pass, once nobody else could be sent
	if (const auto* component = Entities().TryGet<const Villager>(villager);
	    component != nullptr && (component->flags & Villager::k_FlagDisciple) != 0 && !secondPass)
	{
		return false;
	}
	return !IsAtOrOnTheWayToWorshipSite(villager);
}

bool villager_worship::IsAtOrOnTheWayToWorshipSite(entt::entity villager)
{
	// the final state, so that a villager still walking there counts; during a teleport reaction the state it goes
	// back to
	const auto* state = Entities().TryGet<const WorshipVillager>(villager);
	const auto way = rules::WayState(ecs::villager::GetFinalState(villager),
	                                 ecs::villager::GetState(villager, LivingAction::Index::Previous));
	return rules::IsAtOrOnTheWay(state != nullptr && state->atSite, way);
}

glm::vec3 villager_worship::HidingPlace(entt::entity site)
{
	const auto door = town_queries::ToMetres(abode_queries::GetDoorPos(site));
	return {door.x, 0.0f, door.y};
}

bool villager_worship::IsAtHidingPlace(entt::entity villager, entt::entity site)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	if (wallHug == nullptr)
	{
		return false;
	}
	const auto door = abode_queries::GetDoorPos(site);
	return walk_arrival::AreWeThere(map_coords::FromMetres(glm::xz(PositionOf(villager))), {.x = door.x, .z = door.y},
	                                WholeSpeed(*wallHug), rules::k_HidingReach);
}

void villager_worship::SendBackToTown(entt::entity villager)
{
	SetState(villager, VillagerStates::DecideWhatToDo);
}

bool villager_worship::CheckAllowedToRestAtWorshipSite(entt::entity villager, bool moveToFood)
{
	const auto* component = Entities().TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return false;
	}
	const auto siteEntity = SiteOf(villager);
	// hungry, and a meal's worth in the site's pot (none without a site)
	const uint32_t food = siteEntity != entt::null ? worship::site::GetFoodResource(siteEntity) : 0;
	if (!rules::EatsAtSite(component->food, ecs::villager::InfoOf(villager).hungryForFood, food,
	                       ecs::villager::GetAmountOfFoodRequiredForMeal(villager)))
	{
		return false;
	}
	// to the pot, then 241. (guard) the original walks to a missing site's pot unchecked, which only a villager that
	// needs no food could reach
	if (moveToFood && siteEntity != entt::null)
	{
		const auto edge = ecs::villager::GetResourceNearestEdge(siteEntity, ResourceType::Food, villager);
		ecs::villager::SetupMoveToWithHug(villager, town_queries::ToMetres(edge), VillagerStates::GetFoodAtWorshipSite);
	}
	return true;
}

uint32_t villager_worship::GotoWorshipSiteForWorshipState(LivingAction& action)
{
	// this is also the state function of row 58 (VillagerStateFunctions.h): a villager that comes back to the walk (a
	// reaction stores the walk's final state, 59, and popping back resumes 59 as 58) runs GotoWorshipSiteForWorship
	// again, so it sets off for the gate once more. That is how the villager that used the teleport stones on the way to
	// the site (CheckWorshipActivity) goes on from the stone it came out of
	return GotoWorshipSiteForWorship(EntityOf(action)) ? 1 : 0;
}

uint32_t villager_worship::ArrivesAtWorshipSiteForWorship(LivingAction& action)
{
	// the walk to the gate's final state, or entered at once on the site
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return ecs::villager::DecideWhatToDo(action);
	}
	const auto gate = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	// (guard) without the gate point the villager counts as there
	const float distance = gate ? FlatDistance(PositionOf(villager), *gate) : 0.0f;
	const auto& site = Entities().Get<const WorshipSite>(siteEntity);
	const auto dancers = static_cast<uint32_t>(worship::site::DancerCount(site));
	const auto maxDancers = worship::site::InfoOf(site).maxDancersVisible;
	switch (rules::OnReachingGate(distance, dancers, maxDancers))
	{
	case rules::Arrival::Dance:
		static_cast<void>(StartWorshippingAtWorshipSite(villager));
		break;
	case rules::Arrival::Hide:
		static_cast<void>(StartHidingAtWorshipSite(villager));
		break;
	case rules::Arrival::WalkOn:
		// not there yet: the walk to the gate again
		static_cast<void>(ecs::villager::SetupMoveToOnFootpath(villager, siteEntity, GateCoords(*gate),
		                                                       VillagerStates::ArrivesAtWorshipSiteForWorship));
		break;
	}
	return 1;
}

uint32_t villager_worship::WorshippingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& state = StateOf(villager);
	if (state.onWay)
	{
		// the on-way flag clears and the dancers on the way go down on the first turn
		state.onWay = false;
		if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
		{
			auto& site = Entities().Get<WorshipSite>(siteEntity);
			site.dancersOnWay = std::max(0, site.dancersOnWay - 1);
		}
	}
	if (ProcessInWorship(villager) == 1)
	{
		// the dance (60): the dance clip (VillagerAnimations maps 60 to it)
	}
	return 0;
}

uint32_t villager_worship::HidingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// at the door when within its speed and one more map unit of it, on the map's own whole units
	if (villager_worship::IsAtHidingPlace(villager, siteEntity))
	{
		return ProcessInWorship(villager);
	}
	// not there yet: the walk to the door with a hug, then 213
	static_cast<void>(ecs::villager::SetupMoveToWithHug(villager, glm::xz(villager_worship::HidingPlace(siteEntity)),
	                                                    VillagerStates::HidingAtWorshipSite));
	return 1;
}

uint32_t villager_worship::GetFoodAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto edge = town_queries::ToMetres(ecs::villager::GetResourceNearestEdge(siteEntity, ResourceType::Food, villager));
	if (!ecs::villager::AreWeThere(villager, edge, 0.0f))
	{
		ecs::villager::SetupMoveToWithHug(villager, edge, VillagerStates::GetFoodAtWorshipSite);
		return 1;
	}
	// at the pot: a meal from it, eaten, then back to the dance (65) once the clip is played
	if (const auto meal = ecs::villager::GetAmountOfFoodRequiredForMeal(villager); meal != 0)
	{
		static_cast<void>(ecs::villager::GetResourceFrom(villager, siteEntity, ResourceType::Food, static_cast<int16_t>(meal)));
	}
	static_cast<void>(ecs::villager::EatFoodHeld(villager));
	ecs::villager::PlayAnimThenSetState(villager, VillagerStates::RestartWorshippingAtWorshipSite);
	return 1;
}

uint32_t villager_worship::RestartWorshippingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (!StartWorshippingAtWorshipSite(villager))
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

bool villager_worship::ExitGetFoodAtWorship(LivingAction& action, VillagerStates next)
{
	// leaving for a state with another exit that does not keep a villager counted at worship (the state table's
	// worship mark: 58-60, 213 and 241) -> RemoveVillagerFromWorshipSite
	const auto villager = EntityOf(action);
	if (ecs::villager::IsStateExitFunctionSameAs(villager, next))
	{
		return false;
	}
	const auto* info = TableOf(next);
	if (info == nullptr || !ecs::villager::state_info::NoGoHomeWhenHurt(*info))
	{
		RemoveVillagerFromWorshipSite(villager);
	}
	return false;
}

uint32_t villager_worship::GotoAltarForRest(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// along the site's footpath to its rest point. (guard) without the mesh's point, the site's own position
	const auto rest = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Rest)
	                      .value_or(Entities().Get<const Transform>(siteEntity).position);
	ecs::villager::SetupMoveToOnFootpath(villager, siteEntity, town_queries::ToMapCoords(glm::xz(rest)),
	                                     VillagerStates::ArrivesAtAltarForRest);
	return 1;
}

uint32_t villager_worship::ArrivesAtAltarForRest(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (!CheckAllowedToRestAtWorshipSite(villager, false))
	{
		return AtAltarFinishedRest(action);
	}
	ecs::villager::PlayAnimThenSetState(villager, VillagerStates::AtAltarRest);
	return 1;
}

uint32_t villager_worship::AtAltarRest(LivingAction& action)
{
	// the same as arriving: it rests on while allowed
	return ArrivesAtAltarForRest(action);
}

uint32_t villager_worship::AtAltarFinishedRest(LivingAction& action)
{
	if (StartWorshippingAtWorshipSite(EntityOf(action)))
	{
		return 1;
	}
	return ecs::villager::DecideWhatToDo(action);
}

uint32_t villager_worship::GotoStoragePitForWorshipSupplies(entt::entity villager)
{
	const auto pit = ecs::villager::GetStoragePit(villager);
	const auto town = TownOf(villager);
	// (guard) the original reads the town unchecked
	if (pit == entt::null || !abode_queries::IsFunctional(pit) || SiteOf(villager) == entt::null || town == entt::null)
	{
		return 0;
	}
	const auto needed = FoodForWorshipSiteIfEnough(town);
	if (needed == 0)
	{
		return 0;
	}
	// carrying more than is wanted, or all it can: straight to the site
	const int32_t held = FoodHeld(villager);
	if (static_cast<uint32_t>(held) > needed || held == static_cast<int32_t>(ecs::villager::InfoOf(villager).maxFoodCarried))
	{
		return GotoWorshipSiteWithSupplies(villager);
	}
	if (object_resources::GetResource(pit, ResourceType::Food) == 0)
	{
		return 0;
	}
	ecs::villager::SetupMoveToOnFootpath(villager, pit,
	                                     ecs::villager::GetResourceNearestEdge(pit, ResourceType::Food, villager),
	                                     VillagerStates::ArrivesAtStoragePitForWorshipSupplies);
	return 1;
}

uint32_t villager_worship::GotoStoragePitForWorshipSuppliesState(LivingAction& action)
{
	return GotoStoragePitForWorshipSupplies(EntityOf(action));
}

uint32_t villager_worship::ArrivesAtStoragePitForWorshipSupplies(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto town = TownOf(villager);
	// (guard) the original reads the town unchecked
	if (SiteOf(villager) != entt::null && town != entt::null)
	{
		auto needed = static_cast<int32_t>(FoodForWorshipSiteIfEnough(town));
		if (needed != 0)
		{
			if (FoodHeld(villager) >= needed)
			{
				return GotoWorshipSiteWithSupplies(villager);
			}
			// at most what it can still carry
			const int32_t room = ecs::villager::GetFoodCapacity(villager);
			if (room < needed)
			{
				needed = room;
			}
			if (needed > 0)
			{
				return ecs::villager::ArrivesAtStoragePitForResource(
				    villager, ResourceType::Food, static_cast<uint32_t>(needed), VillagerStates::InvalidState,
				    VillagerStates::DecideWhatToDo);
			}
		}
	}
	return ecs::villager::DecideWhatToDo(action);
}

uint32_t villager_worship::GotoWorshipSiteWithSupplies(entt::entity /*villager*/)
{
	// the game never sets off for the site with the food: whoever would gives up here
	return 0;
}

uint32_t villager_worship::GotoWorshipSiteWithSuppliesState(LivingAction& action)
{
	return GotoWorshipSiteWithSupplies(EntityOf(action));
}

uint32_t villager_worship::MoveToWorshipSiteWithSupplies(LivingAction& /*action*/)
{
	return 1;
}

uint32_t villager_worship::ArrivesAtWorshipSiteWithSupplies(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
	{
		// all it carries into the site's pot; what was put in comes off the food on its way
		auto amount = static_cast<uint32_t>(FoodHeld(villager));
		if (amount != 0)
		{
			const auto result = ecs::villager::AtStructureAddResource(villager, siteEntity, ResourceType::Food, amount);
			worship::site::RemoveFromFoodOnTheWay(siteEntity, amount);
			// still on its way to the pot
			if (result == 0x24)
			{
				return 1;
			}
		}
	}
	SetState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_worship::ExitMoveToWorshipSiteWithSupplies(LivingAction& action, VillagerStates next)
{
	// leaving the errand for a state with another exit: what it carries is no longer on its way to the site
	const auto villager = EntityOf(action);
	if (!ecs::villager::IsStateExitFunctionSameAs(villager, next))
	{
		if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
		{
			worship::site::RemoveFromFoodOnTheWay(siteEntity, static_cast<uint32_t>(FoodHeld(villager)));
		}
	}
	return false;
}

bool villager_worship::ExitMoveToWorshipSite(LivingAction& action, VillagerStates next)
{
	// leaving for a state with another exit (the final state's exit against the next state's; a state that is not final
	// counts as the same) that is not a teleport reaction (states 201, 202, 251) -> off the town's list of villagers on
	// the way, the on-way flag cleared. The walks to the dance place and to the door are told their final states (60,
	// 213) as they start, so the flag is cleared there too, before the dance would count it off its dancers on the way
	const auto villager = EntityOf(action);
	if (ecs::villager::IsStateExitFunctionSameAs(villager, next) || rules::IsTeleportReaction(next))
	{
		return false;
	}
	auto& state = StateOf(villager);
	state.onWay = false;
	if (state.onWayInTown)
	{
		if (const auto town = TownOf(villager); town != entt::null)
		{
			worship::percentage::RemoveVillagerOnWay(town, villager);
		}
		state.onWayInTown = false;
	}
	return false;
}

bool villager_worship::ExitAtWorshipSite(LivingAction& action, VillagerStates next)
{
	// leaving for a state with another exit (the final state's exit against the next state's; a state that is not final
	// counts as the same): off the go-home queue; for a state that is not a reactive one and not 241 (eating at the site)
	// -> RemoveVillagerFromWorshipSite; else only off the site's count and out of the dance: it stays counted in the
	// town's worshippers
	const auto villager = EntityOf(action);
	if (ecs::villager::IsStateExitFunctionSameAs(villager, next))
	{
		return false;
	}
	const auto siteEntity = SiteOf(villager);
	RemoveGoHomeRequest(siteEntity, villager);
	const auto* info = TableOf(next);
	if ((info != nullptr && info->isReactionState != 0) || next == VillagerStates::GetFoodAtWorshipSite)
	{
		if (siteEntity != entt::null)
		{
			RemoveVillagerFromWorshipCount(Entities().Get<WorshipSite>(siteEntity), villager);
		}
		// out of the dance group even when its site is gone
		worship::site::LeaveDance(villager);
		StateOf(villager).dancing = false;
		return false;
	}
	RemoveVillagerFromWorshipSite(villager);
	return false;
}
