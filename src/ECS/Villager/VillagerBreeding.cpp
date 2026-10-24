/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBreeding.h"

#include <limits>

#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/TownDesire.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerInteract.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerTrace.h"
#include "ECS/VillagerBreedingRules.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using namespace components;
namespace breeding = villager_breeding;
namespace tq = town_queries;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// A living villager: one with its state machine
bool IsVillagerEntity(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<Villager, LivingAction>(thing);
}

/// The villager's info says it is a woman (children too)
bool IsFemaleVillager(entt::entity villager)
{
	return InfoOf(villager).sex == SexType::Female;
}

float FloatDraw(float limit)
{
	return GameFloatRand(limit);
}

uint32_t IntDraw(uint32_t limit)
{
	return GameRand(limit);
}

/// Sleep is the first of its town's desires (the order the desires are acted on)
bool TownWantsSleepMost(entt::entity town)
{
	return town_desire::GetSortedDesires(town).at(0).index == static_cast<uint32_t>(TownDesireInfo::ForSleep);
}
} // namespace

bool GoAndHaveSexWith(entt::entity villager, entt::entity mate)
{
	const bool touching = MakeVillagesMeet(villager, mate, VillagerStates::StartHavingSex, breeding::k_MeetMateDistance);
	SetTargetThing(villager, mate);
	SetTargetThing(mate, villager);
	return touching;
}

uint32_t StartHavingSexWith(entt::entity villager, entt::entity mate, VillagerStates state)
{
	// Without a mate it would go on alone for its own time and one turn
	auto turns = static_cast<uint16_t>(InfoOf(villager).havingSexTime + 1);
	if (mate != entt::null)
	{
		if (!IsAvailable(mate))
		{
			return 0;
		}
		turns = breeding::LoveTurns(InfoOf(mate).havingSexTime, InfoOf(villager).havingSexTime);
		SetTopState(mate, state);
		SetTargetThing(mate, villager);
		Entities().Get<LivingAction>(mate).turnsUntilStateChange = turns;
	}
	SetTopState(villager, state);
	SetTargetThing(villager, mate);
	Entities().Get<LivingAction>(villager).turnsUntilStateChange = turns;
	return 1;
}

uint32_t StartHavingSex(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto mate = GetTargetThing(villager);
	if (mate != entt::null && IsAvailable(mate))
	{
		// the mate measures the gap to me
		if (object::IsTouching(mate, villager, breeding::k_KissingGap))
		{
			MakeVillagerFaceObject(villager, mate);
			MakeVillagerFaceObject(mate, villager);
			if (StartHavingSexWith(villager, mate, VillagerStates::HavingSex) == 1)
			{
				TraceFormatted(villager, "love 122: kiss #{}", static_cast<uint32_t>(mate));
				return 1;
			}
		}
		else
		{
			TraceFormatted(villager, "love 122: meet #{} again", static_cast<uint32_t>(mate));
			GoAndHaveSexWith(villager, mate);
			return 1;
		}
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t HavingSex(LivingAction& action)
{
	if (!Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		return 1;
	}
	const auto villager = Entities().ToEntity(action);
	// The woman draws her chance as the kiss ends, pregnant already or not
	if (IsFemaleVillager(villager) && IsSexuallyActive(villager))
	{
		const float draw = GameFloatRand(1.0f);
		if (draw < InfoOf(villager).pregnancyChance)
		{
			TraceFormatted(villager, "love 123: pregnant (draw {:.4f})", draw);
			HousewifeGetsPregnant(villager);
		}
	}
	SetTopState(villager, VillagerStates::StopHavingSex);
	return 1;
}

uint32_t StopHavingSex(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (IsAtHome(villager))
	{
		return GoHome(villager);
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t SexAtHome([[maybe_unused]] LivingAction& action)
{
	return 1;
}

uint32_t ExitSex(LivingAction& action, VillagerStates next)
{
	const auto villager = Entities().ToEntity(action);
	// on to another of the love states: nothing
	if (IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	const auto partner = GetTargetThing(villager);
	SetTargetThing(villager, entt::null);
	// the partner, still in the same state as me, lets go and stops too
	if (IsVillagerEntity(partner) && GetState(partner, Index::Top) == GetState(villager, Index::Top))
	{
		SetTargetThing(partner, entt::null);
		SetTopState(partner, VillagerStates::StopHavingSex);
	}
	return 1;
}

bool SexValidate(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (const auto partner = GetTargetThing(villager); partner != entt::null && !IsAvailable(partner))
	{
		SetTargetThing(villager, entt::null);
	}
	return true;
}

void LetPartnerGo(entt::entity villager)
{
	// the final state as it is stored, not the TOP
	if (GetState(villager, Index::Final) != VillagerStates::HavingSex)
	{
		return;
	}
	const auto partner = GetTargetThing(villager);
	if (partner == entt::null)
	{
		return;
	}
	// (openblack, guard) the original's partner is always a villager here
	if (IsVillagerEntity(partner))
	{
		SetTargetThing(partner, entt::null);
		DecideWhatToDo(Entities().Get<LivingAction>(partner));
	}
	SetTargetThing(villager, entt::null);
}

// ---- the breeder disciple ----------------------------------------------------------------------------------------

uint32_t SetupBreederDisciple(entt::entity villager)
{
	// Pregnant: home
	if (IsPregnant(villager))
	{
		SetTopState(villager, VillagerStates::GoHome);
		return 1;
	}
	const auto town = GetTown(villager);
	if (town == entt::null)
	{
		return 0;
	}
	// Its town not too far over full: the places as they stand now (the original's counts move as abodes come and go,
	// not at the town's turn)
	const auto& stats = Entities().Get<const Town>(town).stats;
	const auto room = town_stats::RoomOf(town);
	const breeding::TownHeadcount count {
	    .children = stats.children,
	    .childPlaces = room.childPlaces,
	    .adults = stats.adults,
	    .adultPlaces = room.adultPlaces,
	};
	const float allowance = Locator::infoConstants::value().town.amountOverCapacityThatDiscipleBreedersBreedUntil;
	if (!breeding::BreederMayBreed(count, allowance))
	{
		return 0;
	}
	// The nearest home of its town strictly within reach, in the town's list order
	const auto me = tq::PosOf(villager);
	float best = breeding::k_BreederHomeReach;
	entt::entity home = entt::null;
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const float distance = tq::GetDistanceInMetres(me, tq::PosOf(abode));
		if (distance < best)
		{
			best = distance;
			home = abode;
		}
	}
	glm::ivec2 spot;
	if (home == entt::null)
	{
		// None: by the town's food drop-off
		spot = breeding::BreederSpotByStore(GetResourceDropoffPos(villager, ResourceType::Food), FloatDraw);
		SetupMoveToPos(villager, tq::ToMetres(spot), VillagerStates::BreederDisciple);
	}
	else
	{
		// Out from its door, along the town's footpaths
		spot = breeding::BreederSpotByHome(tq::PosOf(home), abode_queries::GetArrivePos(home), tq::Get2DRadius(home), IntDraw,
		                                   FloatDraw);
		SetupMoveToOnFootpath(villager, home, spot, VillagerStates::BreederDisciple);
	}
	TraceFormatted(villager, "breeder: spot {} {} by {}", spot.x, spot.y, static_cast<uint32_t>(home));
	SetTargetThing(villager, entt::null);
	Entities().Get<Villager>(villager).wanderArea = map_coords::MapCoords {spot.x, spot.y, 0.0f};
	return 1;
}

uint32_t SetupWander(entt::entity villager, const map_coords::MapCoords& area, VillagerStates state)
{
	const auto goal = breeding::WanderGoal(area, FloatDraw);
	SetupMoveToWithHug(villager, map_coords::ToMetres(goal), state);
	Entities().Get<LivingAction>(villager).turnsUntilStateChange = breeding::WanderPause(IntDraw);
	return 1;
}

uint32_t BreederDisciple(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto town = GetTown(villager);
	// (openblack, guard) the original reads its town without a test: a breeder with no town goes home too
	if (IsPregnant(villager) || town == entt::null || TownWantsSleepMost(town))
	{
		SetTopState(villager, VillagerStates::GoHome);
		return 1;
	}
	// The counter goes down every turn; while it had turns left it waits (signed)
	const auto left = static_cast<int16_t>(action.turnsUntilStateChange);
	action.turnsUntilStateChange = static_cast<uint16_t>(left - 1);
	if (left > 0)
	{
		return 1;
	}
	const auto area = Entities().Get<const Villager>(villager).wanderArea;
	SetupWander(villager, area, VillagerStates::BreederDisciple);
	return 1;
}

uint32_t EnterBreeder(LivingAction& action, [[maybe_unused]] VillagerStates final, [[maybe_unused]] VillagerStates next)
{
	// The original skips this while the map is being cleared; openblack's map change enters no states, so it never
	// arises
	const auto villager = Entities().ToEntity(action);
	effects::reactions::CreateReaction(villager, openblack::Reaction::ReactToBreeder, GetPlayerOf(villager), true);
	return 1;
}

uint32_t ExitBreeder(LivingAction& action, [[maybe_unused]] VillagerStates next)
{
	effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(Entities().ToEntity(action), openblack::Reaction::ReactToBreeder);
	return 1;
}

uint32_t BreederJustLanded(LivingAction& action)
{
	auto& registry = Entities();
	const auto breeder = registry.ToEntity(action);
	const auto here = tq::PosOf(breeder);
	// the sex it seeks: the other one, by the villagers' info
	const auto wanted = IsFemaleVillager(breeder) ? SexType::Male : SexType::Female;
	float best = std::numeric_limits<float>::max();
	entt::entity nearest = entt::null;
	// The first villager of each of the nine spiral cells from its own
	map_coords::MapCoords at {here.x, here.y, 0.0f};
	map_coords::Spiral spiral;
	for (int cell = 0; cell < 9; ++cell)
	{
		const auto thing = map_cells::FindType(map_coords::Cell(at), ObjectType::Villager);
		if (IsVillagerEntity(thing) && IsSexuallyActive(thing) && InfoOf(thing).sex == wanted)
		{
			const float distance = tq::GetDistanceInMetres(tq::PosOf(thing), here);
			if (distance < best)
			{
				best = distance;
				nearest = thing;
			}
		}
		map_coords::AddCells(at, spiral.Next());
	}
	if (nearest == entt::null)
	{
		SetTopState(breeder, VillagerStates::DecideWhatToDo);
		return 1;
	}
	TraceFormatted(breeder, "breeder 254: meets {}", static_cast<uint32_t>(nearest));
	GoAndHaveSexWith(breeder, nearest);
	return 1;
}

uint32_t ReactToBreeder(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto breeder = villager_reactions::SlotObject(villager);
	if (!IsVillagerEntity(breeder))
	{
		return 0;
	}
	GoAndHaveSexWith(villager, breeder);
	return 1;
}

uint32_t FollowersOf(uint32_t reaction)
{
	uint32_t count = 0;
	Entities().Each<const LivingReaction>([&count, reaction](entt::entity, const LivingReaction& slot) {
		if (slot.reaction == reaction)
		{
			++count;
		}
	});
	return count;
}

uint8_t ReactToBreederPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr)
	{
		return 0;
	}
	const auto breeder = found->initiator;
	if (!IsVillagerEntity(breeder) || breeder == villager || !IsSexuallyActive(villager))
	{
		return 0;
	}
	// of the other sex, by their info
	if (InfoOf(villager).sex == InfoOf(breeder).sex)
	{
		return 0;
	}
	// only one villager at a time answers the call
	if (FollowersOf(reaction) != 0 || IsScriptControlled(villager))
	{
		return 0;
	}
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToBreeder)).priority;
}

void SetupReactToBreeder(entt::entity villager, entt::entity breeder, uint32_t reaction)
{
	villager_reactions::AddReaction(villager, reaction, VillagerStates::ReactToBreeder);
	villager_reactions::SetSlotObject(villager, breeder);
}

} // namespace openblack::ecs::villager
