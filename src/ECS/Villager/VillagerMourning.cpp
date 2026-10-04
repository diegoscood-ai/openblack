/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMourning.h"

#include <cmath>

#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerReactions.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"

// VillagerReaction.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerMourning.h; the disassembly is in
// dev\documentacion\aldeanos\v12\mourn.txt; MakeChildOrphaned 0x7580D0 / FindChildrenAndOrphanThem 0x756BE0 read with
// bwdis.py).

namespace openblack::ecs::villager_mourning
{
using namespace components;
namespace tq = openblack::ecs::town_queries;
using Index = LivingAction::Index;

namespace
{
/// Villager +0x94 (the reaction it follows) and +0xBC (the dead villager)
struct MourningState
{
	entt::entity dead {entt::null};
	uint32_t reaction {0};
};
std::unordered_map<entt::entity, MourningState> g_States;
/// Reaction +0x1C: how many Livings took it (Living::AddReaction 0x5F0F92 `inc`), by reaction id
std::unordered_map<uint32_t, uint32_t> g_Takers;

/// 1000 / [0xD01A38] (game_clock::MsPerTurn, 100): the unsigned `div` of 0x7666B7 / 0x766875 (the original faults on
/// 0; 0 here)
uint32_t TurnsPerSecond()
{
	const auto ms = game_clock::MsPerTurn();
	return ms != 0 ? 1000u / ms : 0u;
}

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(const LivingAction& action)
{
	return Entities().ToEntity(action);
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

const ReactionInfo& DeathReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToDeath));
}

/// Villager::AddReaction 0x763440 (vt +0x990): vt +0x98C(r, 1) (not ported), then Living::AddReaction 0x5F0F30:
/// StorePreviousState when it follows none, SetTopState(state), RemoveFromDance (pending: dance), +0x94 = r, into the
/// reaction's list (+0x18) and ++ +0x1C
void AddReaction(entt::entity villager, uint32_t reaction, VillagerStates state)
{
	if (!villager_reactions::IsReacting(villager))
	{
		villager::StorePreviousState(villager);
	}
	villager_reactions::SetTopState(villager, state);
	g_States[villager].reaction = reaction;
	++g_Takers[reaction];
}

/// fn_0074E1D0 / fn_0074E200 (the same code): ftol(fild(ftol(m)) / 10 [0x99A1BC] x 65536 [0x8AC408]): the whole metres
/// (truncated) as a whole distance (each x87 step rounded to float: 24-bit control word, 0x7DEE0D)
int32_t WholeDistanceOfWholeMetres(float metres)
{
	const auto whole = static_cast<int32_t>(metres);
	return static_cast<int32_t>(static_cast<float>(whole) / 10.0f * 65536.0f);
}

void TraceIf(entt::entity villager, const std::string& line)
{
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, line);
	}
}
} // namespace

// ---- the pure layer ----------------------------------------------------------------------------------------------

int32_t PointTurns(float roll20)
{
	// 0x7666C5 fadd 2.0 (0x8AB478); 0x7666D2 fimul (1000 / [0xD01A38]); 0x7666D6 __ftol (truncation): on the x87 stack
	// until the one truncation, each step rounded to float (24-bit control word, 0x7DEE0D)
	return static_cast<int32_t>((roll20 + 2.0f) * static_cast<float>(static_cast<int32_t>(TurnsPerSecond())));
}

int32_t MournTurns(float roll3)
{
	// 0x766883 fadd 4.0 (0x8AB418); 0x766890 fimul (1000 / [0xD01A38]); 0x766894 __ftol: float steps on the x87 stack
	// (24-bit control word, 0x7DEE0D)
	return static_cast<int32_t>((roll3 + 4.0f) * static_cast<float>(static_cast<int32_t>(TurnsPerSecond())));
}

// ---- the reaction ------------------------------------------------------------------------------------------------

uint8_t ReactToDeathPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr)
	{
		return 0;
	}
	const auto priority = static_cast<uint8_t>(DeathReactionInfo().priority & 0xFFu); // byte 0xD4FFBC
	// 0x766447..0x766455: the initiator (+0x14) IsCreature (vt +0x34) -> the priority
	if (fire::traits::IsCreature(found->initiator))
	{
		return priority;
	}
	// 0x766462..0x766488: my town (vt +0x48) with a graveyard (+0x748) that IsFunctional (vt +0xD4) -> 0
	const auto* v = Entities().TryGet<const Villager>(villager);
	if (v != nullptr && v->town != entt::null && villager::HasFunctionalGraveyard(v->town))
	{
		return 0;
	}
	// 0x76648A: I am the initiator -> 0; 0x76648E: reaction +0x1C >= 10 (unsigned jae) -> 0
	if (villager == found->initiator)
	{
		return 0;
	}
	const auto takers = g_Takers.find(reaction);
	if (takers != g_Takers.end() && takers->second >= 10)
	{
		return 0;
	}
	return priority;
}

void SetupReactToDeath(entt::entity villager, entt::entity dead, uint32_t reaction)
{
	if (ActionOf(villager) == nullptr)
	{
		return;
	}
	// 0x7665B8..0x7665CC: a creature initiator -> AddReaction(r, 7 LOOKING_AT_OBJECT_REACTION) (vt +0x990)
	if (dead != entt::null && fire::traits::IsCreature(dead))
	{
		AddReaction(villager, reaction, VillagerStates::LookingAtObjectReaction);
	}
	else
	{
		// 0x7665CE..0x7665F5: GameRand(2) (VillagerReaction.cpp 0x726): 0 -> +0x58 = 0 (ax), AddReaction(r, 0xCD); else
		// AddReaction(r, 0xCE)
		if (villager::GameRand(2) == 0)
		{
			ActionOf(villager)->turnsUntilStateChange = 0;
			AddReaction(villager, reaction, VillagerStates::PointAtDeadPerson);
		}
		else
		{
			AddReaction(villager, reaction, VillagerStates::GoTowardsDeadPerson);
		}
	}
	// 0x766603..0x766608: +0xBC = the dead villager
	g_States[villager].dead = dead;
	TraceIf(villager, fmt::format("mourn: {} of {}", static_cast<uint32_t>(villager::GetState(villager, Index::Top)),
	                              static_cast<uint32_t>(dead)));
}

void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Entities();
	if (!registry.AllOf<Villager, LivingAction>(villager) || !villager::IsAvailableForReaction(villager))
	{
		return;
	}
	// Living +0x94: it already follows a reaction (0x6E40D0). Replacing it by a higher scoring one (0x6E4134,
	// reactions::MaySwitch) is not ported for the villagers (as the fire's, teleport's and shield's)
	if (villager_reactions::IsReacting(villager))
	{
		return;
	}
	const auto* me = registry.TryGet<const Transform>(villager);
	const auto* dead = registry.TryGet<const Transform>(reaction.initiator);
	if (me == nullptr || dead == nullptr)
	{
		return;
	}
	// ApplyReactionToLivingObjectsAtSquare's distance: (|dz| + |dx|) / 2 from the initiator
	const auto& info = DeathReactionInfo();
	const float distance =
	    0.5f * (std::abs(me->position.x - dead->position.x) + std::abs(me->position.z - dead->position.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// fn_006E4620 (reactions::Score) with GLivingInfo.isReacting[REACT_TO_DEATH] and ReactToDeathPriority 0x766440
	const bool reacts = villager::InfoOf(villager).isReacting.isReactingToDeath != 0;
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToDeath), reacts,
	                                             ReactToDeathPriority(villager, reaction.id), distance);
	if (score == 0 || !effects::reactions::Records(villager, static_cast<uint8_t>(openblack::Reaction::ReactToDeath),
	                                                info.numGameTurnsForNormalThingsBeforeReactingAgain,
	                                                effects::reactions::Turn()))
	{
		return;
	}
	effects::reactions::MarkStarted(reaction.id, effects::reactions::Turn()); // 0x6E4109: +0x2C = the turn if still 0
	SetupReactToDeath(villager, reaction.initiator, reaction.id);
}

bool IsReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() && it->second.reaction != 0;
}

entt::entity ReactionObject(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() ? it->second.dead : entt::entity(entt::null);
}

void StopReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	if (it == g_States.end())
	{
		return;
	}
	// Living::StopReacting 0x5F1140: with a reaction (+0x94), unlinked from its list (+0x18) and 0x5F1180..0x5F1187
	// `dec [edi+0x1C]` (one taker fewer: the cap of ReactToDeathPriority counts the current mourners), then
	// fn_005F0FE0 gives its record the turn; +0x94 and +0xBC go
	if (it->second.reaction != 0)
	{
		if (const auto takers = g_Takers.find(it->second.reaction); takers != g_Takers.end() && takers->second > 0)
		{
			--takers->second;
		}
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToDeath),
		                                  effects::reactions::Turn());
	}
	g_States.erase(it);
}

// ---- the states --------------------------------------------------------------------------------------------------

uint32_t PointAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// 0x766686..0x766694: LookAtObject(+0xBC, 1) 0x5EC520 (LookAtPos of its position, step 0x80) facing it ->
	if (dead != entt::null && Entities().Valid(dead) && villager::LookAtPos(villager, tq::PosOf(dead), 1) != 0)
	{
		// 0x766698: ++counter (+0x58); 0x7666AB GameFloatRand(20) (VillagerReaction.cpp 0x744), drawn every facing turn
		++action.turnsUntilStateChange;
		const auto turns = PointTurns(villager::GameFloatRand(20.0f));
		// 0x7666DB..0x7666EC: (i16) counter > t (jle) -> SetTopState(0xCE)
		if (static_cast<int16_t>(action.turnsUntilStateChange) > turns)
		{
			villager_reactions::SetTopState(villager, VillagerStates::GoTowardsDeadPerson);
		}
	}
	return 1;
}

uint32_t GoTowardsDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// 0x766708..0x766716: no +0xBC -> 0
	if (dead == entt::null || !Entities().Valid(dead))
	{
		return 0;
	}
	// 0x76671B..0x76672C: R = GetReaction()->GetInfo() +0x44 (maxDistanceToRunAwayFromObject, 4.0 for row 23)
	const float r = DeathReactionInfo().maxDistanceToRunAwayFromObject;
	// 0x766730..0x766743: d = fn_0074CD50 (GetDistanceInMetres) from me (+0x14) to the dead one (+0x14)
	const auto me = tq::PosOf(villager);
	const auto at = tq::PosOf(dead);
	const float d = gutils::GetDistanceInMetres(me, at);
	// 0x766747..0x76675B: 1.2 R < d (fcompp; test ah, 1) and 0x766771..0x76678D: d - R > 1 (test ah, 0x41)
	if (r * 1.2f < d && d - r > 1.0f)
	{
		// 0x76676C GetAngleFromXZ(me, the dead one); 0x766795..0x7667B2: fn_0074E1D0 / fn_0074E200 (d - R) then
		// fn_0074D3E0 / fn_0074D400 (angle, that): me + the step of the whole metres of d - R towards it
		const uint16_t angle = tq::GetAngleFromXZ(me, at);
		const auto step = gutils::StepFromAngle8(angle, WholeDistanceOfWholeMetres(d - r));
		// 0x7667C7..0x7667DD: SetupMoveToWithHug(that point, 0xCE)
		villager::SetupMoveToWithHug(villager, tq::ToMetres(me + step), VillagerStates::GoTowardsDeadPerson);
		return 1;
	}
	// 0x7667EF..0x7667F8: SetTopState(0xCF)
	villager_reactions::SetTopState(villager, VillagerStates::LookAtDeadPerson);
	return 1;
}

uint32_t LookAtDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// 0x766813..0x766835: LookAtObject(+0xBC, 1) == 1 -> counter (+0x58) = 0, SetTopState(0xD0)
	if (dead != entt::null && Entities().Valid(dead) && villager::LookAtPos(villager, tq::PosOf(dead), 1) == 1)
	{
		action.turnsUntilStateChange = 0;
		villager_reactions::SetTopState(villager, VillagerStates::MournDeadPerson);
	}
	return 1;
}

uint32_t MournDeadPerson(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// 0x76685B ++counter; 0x766869 GameFloatRand(3) (VillagerReaction.cpp 0x77A), drawn every turn
	++action.turnsUntilStateChange;
	const auto turns = MournTurns(villager::GameFloatRand(3.0f));
	// 0x766899..0x7668A5: (i16) counter > t -> StopReactingAndSetState (vt +0x99C)
	if (static_cast<int16_t>(action.turnsUntilStateChange) > turns)
	{
		villager_reactions::StopReactingAndSetState(villager);
	}
	return 1;
}

uint32_t ExitReaction(LivingAction& action, VillagerStates next)
{
	// 0x7527AC..0x7527C9: not a reactive state -> StopReacting (vt +0x998), which knows this reaction
	return villager_reactions::ExitReaction(action, next);
}

bool ReactionValidate(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto dead = ReactionObject(villager);
	// 0x756A03..0x756A15: no object, or IsAvailable (vt +0x2C: Villager::IsAvailable 0x751D50) != 1 -> PopFromPrevious;
	// 0x756A17..0x756A3B: REACT_TO_DEATH's whetherReactionFinishesIfInitiatorInHand (1) and the object in the hand
	bool pop = dead == entt::null || !villager::IsAvailable(dead);
	if (!pop)
	{
		pop = DeathReactionInfo().whetherReactionFinishesIfInitiatorInHand != 0 && fire::traits::InHand(dead);
	}
	if (pop)
	{
		TraceIf(villager, "ReactionValidate: the dead villager went -> PopFromPrevious");
		villager_reactions::PopFromPrevious(villager);
	}
	return !pop;
}

// ---- orphans -----------------------------------------------------------------------------------------------------

void FindChildrenAndOrphanThem(entt::entity mother)
{
	auto& registry = Entities();
	// 0x756BE7: GetTown (vt +0x48); none -> nothing
	const auto* v = registry.TryGet<const Villager>(mother);
	if (v == nullptr || v->town == entt::null || !registry.Valid(v->town))
	{
		return;
	}
	const auto town = v->town;
	// 0x756BF1..0x756C27: the structures (+0x754, next +0x9C), each one's inhabitants (+0xA0, next +0xE4); the next is
	// read after MakeChildOrphaned (which unlinks nothing)
	for (const auto abode : town_stats::AbodesOf(town))
	{
		const auto inhabitants = abode_villagers::VillagersOf(abode);
		for (const auto child : inhabitants)
		{
			MakeChildOrphaned(child, mother);
		}
	}
	// 0x756C29..0x756C4C: the homeless list (+0x768, next +0xE4)
	const auto homeless = town_villagers::Homeless(town);
	for (const auto child : homeless)
	{
		MakeChildOrphaned(child, mother);
	}
}

uint32_t MakeChildOrphaned(entt::entity child, entt::entity mother)
{
	auto& registry = Entities();
	auto* v = registry.Valid(child) ? registry.TryGet<Villager>(child) : nullptr;
	// 0x7580D3..0x7580DD: mother (+0x100) is not her -> 0
	if (v == nullptr || v->mother != mother)
	{
		return 0;
	}
	// 0x7580DF..0x7580F3: IsVillagerAvailable 0x752290 -> SetTopState(0x83 MORN_DEATH) (vt +0x8E8). No age test: grown-up
	// children mourn too (literal)
	if (villager::IsVillagerAvailable(child))
	{
		villager::SetTopState(child, VillagerStates::MornDeath);
		TraceIf(child, fmt::format("orphan: -> 131 (mother {})", static_cast<uint32_t>(mother)));
	}
	// 0x7580F9: mother = 0; 1
	if (auto* again = registry.TryGet<Villager>(child))
	{
		again->mother = entt::null;
	}
	return 1;
}

void Clear()
{
	g_States.clear();
	g_Takers.clear();
}
} // namespace openblack::ecs::villager_mourning
