/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerWorship.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerTeleport.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
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

namespace
{
/// ArrivesAtWorshipSiteForWorship 0x76BE00: the arrive point is reached within 10 m (0x8AB414)
constexpr float k_ArriveDistance = 10.0f;
/// HidingAtWorshipSite 0x76C5E0: vt 0x85C AreWeThere(hide point, 1.0) (the 1.0 is the immediate at 0x76C621)
constexpr float k_HideDistance = 1.0f;

/// The state's row of info.dat villagerStateTable
const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

/// Villager::IsStateExitFunctionSameAs (vt 0x96C) 0x752530 for an exit shared by `a` and `b`: the next state leaves by
/// the same exit function, or it is not a final state (table +0x0C: a move on the way)
bool IsStateExitFunctionSameAs(VillagerStates next, VillagerStates a, VillagerStates b)
{
	const auto* info = TableOf(next);
	return next == a || next == b || info == nullptr || info->isFinalState == 0;
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
	if (component == nullptr || component->town == entt::null || !Entities().Valid(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// Villager::GetWorshipSite 0x76C340: its town's
entt::entity SiteOf(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return entt::null;
	}
	const auto* magic = Entities().TryGet<const TownMagic>(town);
	if (magic == nullptr || magic->worshipSite == entt::null || !Entities().Valid(magic->worshipSite))
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

VillagerStates StateNow(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return VillagerStates::InvalidState;
	}
	return Locator::livingActionSystem::value().VillagerGetState(*action, LivingAction::Index::Top);
}

void SetState(entt::entity villager, VillagerStates state)
{
	auto* action = Entities().TryGet<LivingAction>(villager);
	if (action != nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, false);
	}
}

/// The walk (the DECIDE_WHAT_TO_DO pattern: the WallHug goal and a fresh linear move)
void WalkTo(entt::entity villager, const glm::vec3& goal)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (wallHug == nullptr)
	{
		return;
	}
	wallHug->goal = glm::xz(goal);
	wallHug->step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	StateOf(villager).walking = true;
}

/// true while the pathfinding still moves the villager (VillagerMoveToPos's test)
bool Walking(entt::entity villager)
{
	auto& registry = Entities();
	if (!StateOf(villager).walking)
	{
		return false;
	}
	const bool moving =
	    registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(villager);
	if (!moving)
	{
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		StateOf(villager).walking = false;
	}
	return moving;
}

float FlatDistance(const glm::vec3& a, const glm::vec3& b)
{
	return glm::distance(glm::xz(a), glm::xz(b));
}

glm::vec3 PositionOf(entt::entity villager)
{
	return Entities().Get<const Transform>(villager).position;
}

/// Villager::GetLifeDesireFromLife 0x75BBC0: 1 - ((life - min(life, threshold)) / (1 - threshold))^2. The info row is
/// always 0 here and below: damageThresholdToGoHome (0.3) and chantLifeRate (5e-6) are the same in every villager row
/// (sources.md §2.5)
float DesireForLife(entt::entity villager)
{
	const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
	const float life = life::LifeOf(villager);
	const float over = (life - std::min(threshold, life)) / (1.0f - threshold);
	return 1.0f - over * over;
}

/// Villager::AddVillagerToWorshipSite 0x76C3F0
void AddVillagerToWorshipSite(entt::entity villager, entt::entity siteEntity)
{
	auto& state = StateOf(villager);
	if (!state.atSite)
	{
		worship::percentage::AddWorshipper(TownOf(villager)); // fn_0073E3E0
		state.atSite = true;
		state.requestedGoHome = false; // +0x118 = 0
	}
	// fn_0077D040: into the site's list of villagers (+0xD4, at the head), counted in +0xC8 only when new
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	if (std::ranges::find(site.villagers, villager) == site.villagers.end())
	{
		site.villagers.insert(site.villagers.begin(), villager);
		++site.villagersAtSite;
	}
}

/// WorshipSite::RemoveVillagerFromWorshipCount 0x77D0A0: with a count (+0xC8), off the list (if it is there) and the
/// count one down
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

/// Villager::RemoveVillagerFromWorshipSite 0x76C440 (public for Town::RemoveVillager 0x73E2B2)
void villager_worship::RemoveVillagerFromWorshipSite(entt::entity villager)
{
	auto& state = StateOf(villager);
	const auto town = TownOf(villager);
	if (town != entt::null && state.atSite)
	{
		worship::percentage::RemoveWorshipper(town); // fn_0073E3F0
	}
	if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
	{
		// fn_0077D110 (the villager is in the site's list) -> RemoveVillagerFromWorshipCount 0x77D0A0
		auto& site = Entities().Get<WorshipSite>(siteEntity);
		if (std::ranges::find(site.villagers, villager) != site.villagers.end())
		{
			RemoveVillagerFromWorshipCount(site, villager);
		}
		// vt 0x978 / 0xB08(1): out of the dance group
		worship::site::RemoveDancer(siteEntity, villager);
	}
	state.atSite = false;
	state.dancing = false;
	state.requestedGoHome = false;
}

namespace
{

/// WorshipSite::RemoveVillagerRequestingToGoHome 0x77E1D0
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

/// fn_0077E0C0: into the go-home queue (sorted by the desire for life, the highest first), and another villager sent
/// (AdjustWorshipersWorshipping(1, 0, 1))
void RequestGoHome(entt::entity siteEntity, entt::entity villager)
{
	auto& state = StateOf(villager);
	auto& queue = Entities().Get<WorshipSite>(siteEntity).goHomeRequests;
	if (state.requestedGoHome || std::ranges::find(queue, villager) != queue.end())
	{
		return;
	}
	const float desire = DesireForLife(villager);
	const auto at = std::ranges::find_if(queue, [&](entt::entity other) { return DesireForLife(other) < desire; });
	queue.insert(at, villager);
	state.requestedGoHome = true;
	if (const auto town = TownOf(villager); town != entt::null)
	{
		worship::percentage::AdjustWorshipersWorshipping(town, 1, false, true);
	}
}

/// Villager::CheckRequestGoHome 0x76C8D0: below damageThresholdToGoHome (0.3) it asks to go home; above, it stops asking
void CheckRequestGoHome(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return;
	}
	const float threshold = Locator::infoConstants::value().villager.at(0).damageThresholdToGoHome;
	if (life::LifeOf(villager) < threshold)
	{
		if (!StateOf(villager).requestedGoHome)
		{
			RequestGoHome(siteEntity, villager);
		}
	}
	else if (StateOf(villager).requestedGoHome)
	{
		RemoveGoHomeRequest(siteEntity, villager);
	}
}

/// The town's centre is functional and built (vt 0x2C, vt 0x890). (inferido) both tests are skipped: a town centre
/// that exists counts as ready
bool TownCentreReady(entt::entity town)
{
	return worship::town::TownCentreOf(town) != entt::null;
}

/// Villager::CanIGetToTheWorshipSite 0x76BC20: 1 when the site is within the town info's
/// maxDistanceThatVillagersWillGoToWorship (+0x148; GetDistanceInMetres 0x74CD70 on both MapCoords, flat, <= at
/// 0x76BC5E..0x76BC69). Farther, the villager's player (GetPlayer vt 0x1C, 0x76BC6F) looks for a teleport stone that
/// shortens the trip (GPlayer fn_0064D6B0 = teleport::FindRouteStone, the call at 0x76BC84): 1 with that stone in
/// `routeStone`, 0 when it finds none (0x76BCA3). Without a player it is 1 with no stone (0x76BC74).
bool CanIGetToTheWorshipSite(entt::entity villager, entt::entity siteEntity, entt::entity* routeStone = nullptr)
{
	const float maximum = Locator::infoConstants::value().town.maxDistanceThatVillagersWillGoToWorship;
	const auto at = PositionOf(villager);
	const auto site = Entities().Get<const Transform>(siteEntity).position;
	if (FlatDistance(at, site) <= maximum)
	{
		return true;
	}
	const auto player = villager_teleport::PlayerOf(villager);
	if (!player.has_value())
	{
		return true;
	}
	// fn_0064D6B0 walks the player's stone list (GPlayer +0xA58, newest first)
	const auto& stones = magic::teleport::StonesOf(*player);
	std::vector<glm::vec3> positions;
	positions.reserve(stones.size());
	for (const auto stone : stones)
	{
		positions.push_back(magic::teleport::MapPositionOf(stone));
	}
	const int index = magic::teleport::FindRouteStone(positions, magic::ToMap(at), magic::ToMap(site), maximum);
	if (index < 0)
	{
		return false;
	}
	if (routeStone != nullptr)
	{
		*routeStone = stones[static_cast<size_t>(index)];
	}
	return true;
}

/// Living::SetReactionDoneWhen 0x6E44A0 (Living +0x98, components::ReactionRecords): the record of that type gets the
/// game turn (GGame +0x205A40, 0x6E44F1..0x6E44FD); without one a new {type, turn} record is appended at the tail, the
/// head (the oldest) dropped first when the list already holds 3 (0x6E4507..0x6E4540). Unlike fn_006E4340
/// (reactions::Records) it forgets nothing older than 1800 turns, and unlike StopReacting's RefreshRecord it adds
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

/// Villager::GotoWorshipSiteForWorship 0x76BCC0: Dance +0x114 and the flag 0x10, the walk to the arrive point (state
/// 59), the town's count of villagers on the way
bool GotoWorshipSiteForWorship(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	const auto town = TownOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	auto& site = Entities().Get<WorshipSite>(siteEntity);
	++site.dancersOnWay;
	auto& state = StateOf(villager);
	state.onWay = true;
	// site vt 0x6C4 > 0 (the footpath) -> SetupMoveToOnFootpath(arrive point, 59); else SetState(59)
	const auto arrive = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	SetState(villager, VillagerStates::ArrivesAtWorshipSiteForWorship);
	if (arrive)
	{
		WalkTo(villager, *arrive);
	}
	if (town != entt::null)
	{
		worship::percentage::AddVillagerOnWay(town, villager);
		StateOf(villager).onWayInTown = true;
	}
	if (worship::trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("ai"), "Worship trace: villager {} goes to worship site {}",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity));
	}
	return true;
}

/// Villager::StartWorshippingAtWorshipSite 0x76C4C0: into the dance group, to its dance place (state 60)
bool StartWorshippingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	worship::site::AddDancer(siteEntity, villager); // GroupBehaviour::FindDanceGroup
	StateOf(villager).dancing = true;
	SetState(villager, VillagerStates::WorshippingAtWorshipSite);
	WalkTo(villager, worship::site::DancePosition(siteEntity, villager)); // Villager::SetupMoveToPos(pos, 60)
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// Villager::StartHidingAtWorshipSite 0x76C550: to the site's hide point (vt 0x864, state 213)
bool StartHidingAtWorshipSite(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return false;
	}
	SetState(villager, VillagerStates::HidingAtWorshipSite);
	if (const auto hide = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Hide); hide)
	{
		WalkTo(villager, *hide);
	}
	AddVillagerToWorshipSite(villager, siteEntity);
	return true;
}

/// Villager::CheckVillagerGoBackToTownFromWorship 0x76BEC0: the site gone or not the town's player's -> 163; fewer
/// needed and first in the go-home queue (fn_0077E0A0: an empty queue counts as first, read) -> 248. 1 when it left.
bool CheckVillagerGoBackToTownFromWorship(entt::entity villager)
{
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return false;
	}
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null || !TownCentreReady(town) ||
	    Entities().Get<const WorshipSite>(siteEntity).player != worship::town::OwnerOf(town))
	{
		SetState(villager, VillagerStates::DecideWhatToDo);
		return true;
	}
	if (worship::percentage::GetWorshipersNeeded(town, false, false, nullptr) >= 0)
	{
		return false;
	}
	const auto& queue = Entities().Get<const WorshipSite>(siteEntity).goHomeRequests;
	if (!queue.empty() && queue.front() != villager)
	{
		return false;
	}
	// 0x76BF59..0x76BF6D: SetTopState(0xF8) (vt +0x8E8) == 1 -> 1. Its code, not the TOP: a pause first (239 with
	// FINAL 248, Villager::SetTopState 0x7520A2) also returns 1 and the villager has left
	return ecs::villager::SetTopState(villager, VillagerStates::GoHomeFromWorship) == ecs::villager::k_Done;
}

/// Villager::ReduceVillagerLifeByChant 0x76C800: life - chantDamage x chantLifeRate (vt 0x5B8); at 0 VillagerDead
/// (reason 4, worship). 0x21 when it died.
uint32_t ReduceVillagerLifeByChant(entt::entity villager)
{
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		return 1;
	}
	const auto& site = Entities().Get<const WorshipSite>(siteEntity);
	const float rate = Locator::infoConstants::value().villager.at(0).chantLifeRate;
	life::ReduceLife(villager, site.chantDamage * rate);
	if (life::LifeOf(villager) > 0.0f)
	{
		return 1;
	}
	// 0x76C873: VillagerDead(4 CHANT, town ? town's player : none, chantDamage x chantLifeRate, 1) (ECS/Villager/
	// VillagerDeath.h); 0x21. The worship-death count is the town's TownDeaths::byReason[4] (Town::GetDeathsFromWorshipping
	// 0x740D60), and the site lets the villager go through the worship state's exit (SetDying's SetTopState(14))
	ecs::villager::VillagerDead(villager, DeathReason::Chant, ecs::villager::GetPlayerOf(villager), site.chantDamage * rate, 1);
	return 0x21;
}

/// Villager::CheckAllowedToRestAtWorshipSite 0x76C9A0 (1): a hungry villager eats from the site's food pot
/// (GetFoodAtWorshipSite 241). openblack's villagers have no belly yet (Villager +0xE8): never.
bool CheckAllowedToRestAtWorshipSite(entt::entity /*villager*/)
{
	return false;
}

/// Villager::ProcessInWorship 0x76C890: 0x23 when it stops (left, died, or went to eat), else 1
uint32_t ProcessInWorship(entt::entity villager)
{
	if (CheckVillagerGoBackToTownFromWorship(villager))
	{
		return 0x23;
	}
	CheckRequestGoHome(villager);
	if (ReduceVillagerLifeByChant(villager) == 0x21)
	{
		return 0x23;
	}
	return CheckAllowedToRestAtWorshipSite(villager) ? 0x23 : 1;
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
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
	// CheckNeededForWorshipSiteBuilding 0x76C930 is skipped. (inferido) openblack's sites are always built
	if (!GotoWorshipSiteForWorship(villager))
	{
		return false;
	}
	// 0x76BB99..0x76BC07: when a teleport stone is what makes the site reachable, the walk goes through it. The
	// REACT_TO_TELEPORT record gets the turn (SetReactionDoneWhen 0x6E44A0: the record's turn, added when there is none,
	// so the villager will not take another teleport reaction for a while), the villager leaves the reactor list of the reaction it was following
	// (0x76BBAF..0x76BBF2; openblack keeps no reactor list in a Reaction, and SetupReactToTeleport replaces its +0x94 /
	// +0xBC anyway), and StartReacting(REACT_TO_TELEPORT, stone, the stone's reaction) (vt 0x994 = Living::StartReacting
	// 0x6E4590, which dispatches to Villager::SetupReactToTeleport 0x766250) pushes GO_TOWARDS_TELEPORT_REACTION over the
	// walk that was just started; TELEPORT_REACTION jumps it and PopFromPrevious puts it back on the way to the site
	if (routeStone != entt::null)
	{
		SetReactionDoneWhen(villager, static_cast<uint8_t>(openblack::Reaction::ReactToTeleport)); // push 0x14, 0x76BBA6
		villager_teleport::SetupReactToTeleport(villager, routeStone, magic::teleport::ReactionOf(routeStone));
		if (worship::trace::Enabled() || magic::teleport::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("ai"),
			                   "Worship trace: villager {} goes to worship site {} through teleport stone {}",
			                   static_cast<uint32_t>(villager), static_cast<uint32_t>(siteEntity),
			                   static_cast<uint32_t>(routeStone));
		}
	}
	return true;
}

bool villager_worship::IsAvailableForWorshipSite(entt::entity villager, bool /*secondPass*/)
{
	// IsVillagerAvailable 0x752290: +0x25 bit 4 clear, IsAvailableForStateChange, and the state's availability bit
	// (GVillagerStateTableInfo +0xB8 = file field0xa8, GetVillagerAvailableState 0x751F40). The +0xE0 bit 0x200 (the
	// first pass skips it) is not known: no openblack villager has it.
	const auto state = static_cast<size_t>(StateNow(villager));
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	if (state >= table.size() || (table.at(state).field0xa8 & 1) == 0)
	{
		return false;
	}
	return !IsAtOrOnTheWayToWorshipSite(villager);
}

bool villager_worship::IsAtOrOnTheWayToWorshipSite(entt::entity villager)
{
	if (const auto* state = Entities().TryGet<const WorshipVillager>(villager); state != nullptr && state->atSite)
	{
		return true;
	}
	const auto now = StateNow(villager);
	return now == VillagerStates::ArrivesAtWorshipSiteForWorship || now == VillagerStates::ArrivesAtWorshipSiteWithSupplies;
}

void villager_worship::SendBackToTown(entt::entity villager)
{
	SetState(villager, VillagerStates::DecideWhatToDo);
}

uint32_t villager_worship::GotoWorshipSiteForWorshipState(LivingAction& action)
{
	// 0x76BCC0 is also the state function of row 58 (VillagerOriginalFns.h): a villager that comes back to the walk
	// (Villager::PopFromPrevious after a reaction state, which resumes 59 as 58) runs GotoWorshipSiteForWorship again, so
	// it sets 59 and walks to the arrive point once more. That is how the villager that used the teleport stones on the
	// way to the site (CheckWorshipActivity 0x76BB99) goes on from the stone it came out of
	return GotoWorshipSiteForWorship(EntityOf(action)) ? 1 : 0;
}

uint32_t villager_worship::ArrivesAtWorshipSiteForWorship(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (Walking(villager))
	{
		return 0; // the footpath walk (the state is the move's final state)
	}
	const auto siteEntity = SiteOf(villager);
	if (siteEntity == entt::null)
	{
		SetState(villager, VillagerStates::DecideWhatToDo); // vt 0x8C8
		return 0;
	}
	const auto arrive = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Arrive);
	if (!arrive || FlatDistance(PositionOf(villager), *arrive) < k_ArriveDistance)
	{
		const auto& site = Entities().Get<const WorshipSite>(siteEntity);
		if (static_cast<uint32_t>(worship::site::DancerCount(site)) < worship::site::InfoOf(site).maxDancersVisible)
		{
			StartWorshippingAtWorshipSite(villager);
		}
		else
		{
			StartHidingAtWorshipSite(villager);
		}
		return 0;
	}
	WalkTo(villager, *arrive);
	return 0;
}

uint32_t villager_worship::WorshippingAtWorshipSite(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& state = StateOf(villager);
	if (state.onWay)
	{
		// the flag 0x10 clears and the dance's +0x114 goes down on the first turn
		state.onWay = false;
		if (const auto siteEntity = SiteOf(villager); siteEntity != entt::null)
		{
			auto& site = Entities().Get<WorshipSite>(siteEntity);
			site.dancersOnWay = std::max(0, site.dancersOnWay - 1);
		}
	}
	if (Walking(villager))
	{
		return 0; // Villager::SetupMoveToPos(dance place, 60)
	}
	if (ProcessInWorship(villager) == 1)
	{
		// Living::PerformDance(the dance, 60): the dance clip (VillagerAnimations maps 60 to it)
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
		return 0;
	}
	if (Walking(villager))
	{
		return 0;
	}
	const auto hide = worship::site::GetSpecialPos(siteEntity, worship::site::Point::Hide);
	// (inferido) the + 1 m stands in for AreWeThere: the WallHug stops within its arrive step of the point
	if (!hide || FlatDistance(PositionOf(villager), *hide) <= k_HideDistance + 1.0f)
	{
		ProcessInWorship(villager);
		return 0;
	}
	WalkTo(villager, *hide); // Living::SetupMoveToWithHug(hide point, 213)
	return 0;
}

bool villager_worship::ExitMoveToWorshipSite(LivingAction& action, VillagerStates next)
{
	// Villager::ExitMoveToWorshipSite 0x76C170: leaving for a state with another exit (vt 0x96C) that is not a teleport
	// reaction (0x76C197: exit ExitReactToTeleport 0x766390, states 201, 202, 251) -> off the town's list of villagers
	// on the way, the flag 0x10 cleared
	if (IsStateExitFunctionSameAs(next, VillagerStates::GotoWorshipSiteForWorship,
	                              VillagerStates::ArrivesAtWorshipSiteForWorship) ||
	    next == VillagerStates::GoTowardsTeleportReaction || next == VillagerStates::TeleportReaction ||
	    next == VillagerStates::GoTowardsTeleportReactionQuickly)
	{
		return false;
	}
	const auto villager = EntityOf(action);
	auto& state = StateOf(villager);
	// (aproximado) the original reaches 60 / 213 through SetupMoveToPos / SetupMoveToOnFootpath's move state (not a
	// final one, so this exit does nothing then) and WorshippingAtWorshipSite 0x76C680 later clears the flag with Dance
	// +0x114; openblack enters 60 / 213 at once, so the flag is kept for them
	if (next != VillagerStates::WorshippingAtWorshipSite && next != VillagerStates::HidingAtWorshipSite)
	{
		state.onWay = false;
	}
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
	// Villager::ExitAtWorshipSite 0x76C1F0: leaving for a state with another exit (vt 0x96C): off the go-home queue;
	// for a state that is not a reactive one (table +0xB8, IsReactiveState 0x7525B0) and not 241 (eating at the site)
	// -> RemoveVillagerFromWorshipSite; else only off the site's count (0x77D0A0) and out of the dance (vt 0x978
	// IsDancing / vt 0xB08 RemoveFromDance(1)): it stays counted in the town's worshippers (+0xE0 0x2)
	if (IsStateExitFunctionSameAs(next, VillagerStates::WorshippingAtWorshipSite, VillagerStates::HidingAtWorshipSite))
	{
		return false;
	}
	const auto villager = EntityOf(action);
	const auto siteEntity = SiteOf(villager);
	RemoveGoHomeRequest(siteEntity, villager);
	const auto* info = TableOf(next);
	if ((info != nullptr && info->field0xb8 != 0) || next == VillagerStates::GetFoodAtWorshipSite)
	{
		if (siteEntity != entt::null)
		{
			RemoveVillagerFromWorshipCount(Entities().Get<WorshipSite>(siteEntity), villager);
			worship::site::RemoveDancer(siteEntity, villager);
		}
		StateOf(villager).dancing = false;
		return false;
	}
	RemoveVillagerFromWorshipSite(villager);
	return false;
}
