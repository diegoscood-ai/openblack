/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFire.h"
#include "VillagerMove.h"
#include "VillagerReactions.h"
#include "VillagerWorship.h"

#include <cmath>

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Worship/WorshipPercentage.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// What the port keeps of a villager's fire fields
struct FireState
{
	uint32_t fire {0};                ///< +0x114 (FireEffect id)
	entt::entity object {entt::null}; ///< +0xBC the object of the reaction it follows
	uint32_t reaction {0};            ///< +0x94 its current reaction
	glm::vec2 savedDestination {0.0f}; ///< +0x10C (JustWholeMapXZ)
};
std::unordered_map<entt::entity, FireState> g_States;

FireState& StateOf(entt::entity villager)
{
	return g_States[villager];
}

LivingAction* ActionOf(entt::entity villager)
{
	return Locator::entitiesRegistry::value().TryGet<LivingAction>(villager);
}

VillagerStates Get(const LivingAction& action, LivingAction::Index index)
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

const GVillagerStateTableInfo* TableOf(VillagerStates state)
{
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	const auto i = static_cast<size_t>(state);
	return i < table.size() ? &table[i] : nullptr;
}

/// Villager::GetFinalState 0x751DD0: the top state if it is a final one (table +0x0C), else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// GUtils::GetDistanceInMetres 0x74CD70 (and its twin fn_0074CD50): x, z only, through the table hypotenuse 0x74F680
float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// GRand::GameFloatRand 0x6DE530 (0 for 0, signed like max)
using game_random::GameFloatRand;

/// Villager::SetTopState 0x752010 (vt +0x8E8): the villager core's (ECS/Villager/VillagerCore.h), which runs the exit
/// functions of TOP and of the final state (CallExitStateFunction 0x752320) and the entry of the new state
/// (CallEntryStateFunction 0x7523D0) from the rows of k_VillagerStateTable, once each, with the pause roll (only into a
/// state whose row may pause: none of 163, 215..220) and the codes 1 / 0x2E / 0x2F; the walk's end as
/// villager_reactions::SetTopState
uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	return villager_reactions::SetTopState(villager, state);
}

/// Villager::SetState (vt +0x938, 0x753690) for the stored state (index 2, PREVIOUS): the core's, which skips a state
/// with table +0x10 set and adjusts the town modifiers
void SetStoredState(entt::entity villager, VillagerStates state)
{
	villager::SetState(villager, LivingAction::Index::Previous, state);
}

/// Villager::StorePreviousState 0x763470: the final state is kept in index 2, unless it is a passing one (table +0x10
/// or +0xB8), which keeps what was stored. Raw LivingAction::SetState 0x5ECC90 (0x7634B5), no town modifiers
void StorePreviousState(LivingAction& action)
{
	const auto final = FinalState(action);
	const auto* info = TableOf(final);
	auto stored = final;
	if (info != nullptr && (info->field0x10 != 0 || info->field0xb8 != 0))
	{
		stored = Get(action, LivingAction::Index::Previous);
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(stored);
}

/// Villager::PopFromPrevious 0x751E50 (VillagerReactions.h): the stored state's resume state (file +0x20; nothing
/// stored is row 0, resume 0: INVALID_STATE, as the original), 0x2E -> raw TOP 163, then raw PREVIOUS 0
void PopFromPrevious(entt::entity villager)
{
	villager_reactions::PopFromPrevious(villager);
}

/// Living::SetupMoveToWithHug 0x5F2890: the villager core's (SetCurrentAndDestinationState(MOVE_TO_POS, final) with its
/// exits and entries, then the walk)
void SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	villager::SetupMoveToWithHug(villager, goal, final);
}

/// Living::GetFleeingPositionFromStationaryObject 0x5F2010: `distance` from the object, on its side away from the
/// villager
glm::vec2 FleeingPosition(entt::entity villager, entt::entity object, float distance)
{
	const auto from = PositionOf(object);
	const auto at = PositionOf(villager);
	glm::vec2 d(at.x - from.x, at.z - from.z);
	if (d.x != 0.0f || d.y != 0.0f)
	{
		d *= distance / glm::length(d);
	}
	return glm::vec2(from.x, from.z) + d;
}

/// from + GetPosFromAngle 0x74D580(angle, radius) through MapCoords::operator+ 0x605520, on the (x, z) in metres (each a
/// MapCoords, as the original holds them)
glm::vec2 PosFromAngle(const glm::vec3& from, float angle, float radius)
{
	const auto at = map_coords::FromMetres(glm::vec2(from.x, from.z)) + gutils::GetPosFromAngle(angle, radius);
	return map_coords::ToMetres(at);
}

float Radius2D(entt::entity object)
{
	return fire::traits::Radius(object);
}

/// MobileWallHug::AreWeThere(pos, extra 0) 0x60AD60: d^2 < (the wall hug's step +0x5A + extra)^2, strictly (0x60ADAB
/// `test ah, 0x41`); the step is RebuildMoveByStep 0x609D10's, openblack's WallHug::speed (as PathfindingSystem's)
bool AreWeThere(entt::entity villager, const glm::vec2& at, const glm::vec2& goal)
{
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager);
	const float step = wallHug != nullptr ? wallHug->speed : 0.0f;
	return glm::distance2(at, goal) < step * step;
}

/// The firemen list of a fire's group root
bool IsFireman(const fire::FireEffect& fire, entt::entity villager)
{
	const auto& list = fire.root->firemen;
	return std::find(list.begin(), list.end(), villager) != list.end();
}

/// FireEffect::AddFireman 0x7309A0 / RemoveFireman 0x7309E0 (the group root's list, newest first)
void AddFireman(fire::FireEffect& fire, entt::entity villager)
{
	auto& list = fire.root->firemen;
	list.insert(list.begin(), villager);
}
void RemoveFireman(fire::FireEffect& fire, entt::entity villager)
{
	auto& list = fire.root->firemen;
	std::erase(list, villager);
}

/// Villager::GetFireFightingPos 0x75AA90: on the line from the fire to the villager, just outside the fire
bool FireFightingPosition(entt::entity villager, const fire::FireEffect& fire, glm::vec2& out)
{
	if (fire.object == entt::null)
	{
		return false;
	}
	const auto centre = fire::traits::FireCentre(fire.object);
	const auto at = PositionOf(villager);
	// 0x75AAD4..0x75AAE2: Get3DAngleFromXZ(the fire's centre fn_0072FEF0, the villager +0x14)
	const float angle = gutils::Get3DAngleFromXZ(glm::vec2(centre.x, centre.z), glm::vec2(at.x, at.z));
	// 0x75AAF2..0x75AB16: `fcomp safe, objectRadius; test ah, 1`: safe < the object's radius (vt 0x64) -> the radius,
	// else safe, so the larger of the two; 0x75AB23: + the villager's radius (vt 0x64); 0x75AB3D: + GameFloatRand(1)
	const float radius = Radius2D(fire.object);
	const float safe = fire.SafeFireRadius();
	const float keep = safe < radius ? radius : safe;
	const float distance = keep + Radius2D(villager) + GameFloatRand(1.0f);
	// 0x75AB59..0x75AB6A: the object (+0x14) + GetPosFromAngle(angle, distance)
	out = PosFromAngle(PositionOf(fire.object), angle, distance);
	return true;
}

/// Villager::SetupMoveAroundFire 0x75A770: MOVE_AROUND_FIRE towards `destination`, then `after`; 1 if SetTopState(220)
/// gave 1
bool SetupMoveAroundFire(entt::entity villager, const glm::vec2& destination, VillagerStates after)
{
	// 0x75A775: SetTopState(220) (vt +0x8E8); anything but 1 -> 0, nothing more (0x75A7D7)
	if (SetTopState(villager, VillagerStates::MoveAroundFire) != villager::k_Done)
	{
		return false;
	}
	StateOf(villager).savedDestination = destination; // 0x75A791 +0x10C
	SetStoredState(villager, after);                  // 0x75A7A1 SetState(2, after) (vt +0x938)
	// 0x75A7A7: going on to ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP (59), it is on its town's way list again
	// (Town::AddVillagerOnWayToWorshipSite 0x73E300) with the flag +0xE0 0x10
	auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	if (after == VillagerStates::ArrivesAtWorshipSiteForWorship && v != nullptr && registry.Valid(v->town))
	{
		worship::percentage::AddVillagerOnWay(v->town, villager);
		auto* worshipper = registry.TryGet<WorshipVillager>(villager);
		if (worshipper == nullptr)
		{
			worshipper = &registry.Assign<WorshipVillager>(villager);
		}
		worshipper->onWayInTown = true;
		worshipper->onWay = true;
	}
	return true;
}

/// fn_0075ABA0: the villager stands in the band just outside the fire (its radius plus the fire's) and 2 m further. The
/// fire's part is max(safe radius, the object's radius) (read at 0x75ABE0..0x75ABFB), as in GetFireFightingPos (0x75AAF2)
bool IsBesideFire(entt::entity villager, const fire::FireEffect& fire, float band)
{
	if (fire.object == entt::null)
	{
		return false;
	}
	const float distance = Distance2D(PositionOf(villager), PositionOf(fire.object)); // 0x74CD70 at 0x75ABC7
	const float radius = Radius2D(fire.object);
	const float safe = fire.SafeFireRadius();
	const float keep = safe < radius ? radius : safe;
	const float reach = Radius2D(villager) + keep;
	return distance > reach && reach + band > distance;
}

/// Villager::DecideHowToPutOutFire 0x75A3D0: to the nearest burning member of the group, to beat it
bool DecideHowToPutOutFire(entt::entity villager, fire::FireEffect& fire)
{
	auto* target = fire.NearestFireToFight(PositionOf(villager));
	StateOf(villager).fire = target != nullptr ? target->id : 0;
	if (target == nullptr)
	{
		return false;
	}
	glm::vec2 position;
	if (!FireFightingPosition(villager, *target, position))
	{
		return false;
	}
	SetupMoveAroundFire(villager, position, VillagerStates::PutOutFireByBeating);
	return true;
}

/// Villager::FinishBeingOnFire 0x75B3D0: the saved destination back, and the stored state
void FinishBeingOnFire(entt::entity villager)
{
	if (auto* wallHug = Locator::entitiesRegistry::value().TryGet<WallHug>(villager))
	{
		wallHug->goal = StateOf(villager).savedDestination;
	}
	PopFromPrevious(villager);
}

const ReactionInfo& FireReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToFire));
}

/// fn_006E4340 on the villager's records (Living +0x98, common to the Living: ECS/Effects/Reactions): may it react to
/// this type again (more than `again` turns since the last time)? Remembers it.
bool MayReactAgain(entt::entity villager, openblack::Reaction type, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(type), again, effects::reactions::Turn());
}

/// Villager::IsAvailableForReaction 0x763390: its final state takes reactions (table +0xEC), and it is not held or
/// thrown (the +0xE0 flags, the life threshold and Living::IsAvailableForReaction are not ported)
bool IsAvailableForReaction(entt::entity villager)
{
	const auto* action = ActionOf(villager);
	if (action == nullptr || fire::traits::InHand(villager))
	{
		return false;
	}
	const auto top = Get(*action, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return false;
	}
	const auto* info = TableOf(FinalState(*action));
	return info != nullptr && info->field0xec != 0;
}

/// The REACT_TO_FIRE part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell: with no
/// reaction of its own, fn_006E4620's score (the priority x (1 + 0.5 howImportantIsDistance (R - d) / R), 0 beyond
/// maxReactionDistance) above 0 and not reacted to a fire lately, it starts reacting (StartReacting -> SetupReactToFire).
/// Replacing a current reaction by a higher one is not ported.
void ApplyFireReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.AllOf<Villager>(villager) || villager == reaction.initiator || !IsAvailableForReaction(villager))
	{
		return;
	}
	const auto at = PositionOf(villager);
	const auto from = PositionOf(reaction.initiator);
	// the shield test fn_0072B990 (0x6E4031) is done by reactions::SpreadReaction for every Living class
	const float distance = 0.5f * (std::abs(at.x - from.x) + std::abs(at.z - from.z));
	auto& state = StateOf(villager);
	if (effects::reactions::Find(state.reaction) != nullptr)
	{
		return;
	}
	const auto& info = FireReactionInfo();
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// fn_006E4620 (ECS/Effects/Reactions: Score)
	const auto score = effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToFire), true,
	                                             villager_fire::ReactToFirePriority(villager, reaction.id, 0), distance);
	if (score == 0 ||
	    !MayReactAgain(villager, openblack::Reaction::ReactToFire, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	villager_fire::SetupReactToFire(villager, reaction.initiator, reaction.id);
}
/// OPENBLACK_VILLAGER_TRACE: one line per call of the fire's entry / exit functions, with what they return
void TraceCall(entt::entity villager, const char* function, VillagerStates a, VillagerStates b, uint32_t result)
{
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("{}({}, {}) = {}", function, static_cast<int>(a), static_cast<int>(b), result));
	}
}
} // namespace

uint32_t villager_fire::FireOf(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() ? it->second.fire : 0;
}

bool villager_fire::IsFireMan(entt::entity object)
{
	const auto* action = ActionOf(object);
	if (action == nullptr || !Locator::entitiesRegistry::value().AllOf<Villager>(object))
	{
		return false; // Object::IsFireMan (vt 0x7CC) = 0
	}
	const auto final = FinalState(*action);
	switch (final)
	{
	case VillagerStates::PutOutFireByBeating: // exit function ExitPutOutFire 0x75AE80
	case VillagerStates::PutOutFireWithWater:
	case VillagerStates::GetWaterToPutOutFire:
	case VillagerStates::MoveAroundFire:
	case VillagerStates::ReactToFire:
		return true;
	default:
		return false;
	}
}

bool villager_fire::IsInOnFireState(entt::entity villager)
{
	const auto* action = ActionOf(villager);
	return action != nullptr && FinalState(*action) == VillagerStates::OnFire;
}

void villager_fire::SetupOnFire(entt::entity villager, uint32_t fire)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || fire::traits::InHand(villager) || !Locator::entitiesRegistry::value().Valid(villager))
	{
		return; // +0x24 & 0x44 (in the hand or thrown), not available; +0xB4 bit 0 (not ported)
	}
	const auto top = Get(*action, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return;
	}
	StorePreviousState(*action);
	auto& state = StateOf(villager);
	if (const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager))
	{
		state.savedDestination = wallHug->goal; // GetDestPos (vt 0x860)
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	state.fire = fire;
	SetTopState(villager, VillagerStates::OnFire);
	if (fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: villager {} on fire (fleeing fire {})", static_cast<int>(villager), fire);
	}
}

void villager_fire::StopFireFighting(entt::entity villager)
{
	auto* action = ActionOf(villager);
	auto& state = StateOf(villager);
	if (action == nullptr || state.fire == 0)
	{
		return;
	}
	auto* fire = fire::Get(state.fire);
	if (FinalState(*action) == VillagerStates::MoveAroundFire)
	{
		// moving around a fire towards a fire-fighting state (the stored one): it resumes, or decides
		auto next = Get(*action, LivingAction::Index::Previous);
		const auto* info = TableOf(next);
		next = info != nullptr ? static_cast<VillagerStates>(info->field0x20) : VillagerStates::DecideWhatToDo;
		const auto* nextInfo = TableOf(next);
		if (next == VillagerStates::PutOutFireByBeating || next == VillagerStates::PutOutFireWithWater ||
		    next == VillagerStates::GetWaterToPutOutFire || next == VillagerStates::MoveAroundFire || nextInfo == nullptr)
		{
			next = VillagerStates::DecideWhatToDo;
		}
		if (fire != nullptr)
		{
			RemoveFireman(*fire, villager);
		}
		state.fire = 0;
		SetTopState(villager, next);
		action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
		return;
	}
	if (fire != nullptr)
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	SetTopState(villager, VillagerStates::DecideWhatToDo);
}

uint8_t villager_fire::ReactToFirePriority(entt::entity villager, uint32_t reaction, uint32_t currentReaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !Locator::entitiesRegistry::value().Valid(found->initiator))
	{
		return 0;
	}
	auto* fire = fire::Find(found->initiator);
	if (fire == nullptr || IsInOnFireState(villager))
	{
		return 0;
	}
	const auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return 0;
	}
	const auto& info = FireReactionInfo();
	const float distance = Distance2D(PositionOf(villager), PositionOf(found->initiator)); // fn_0074CD50 at 0x76567E
	// ReactionInfo[10].priority (0xD4FAA8: 210) x (1 + 0.5 x fire radius / max radius), at most 255
	const float ratio = fire->FireRadius() / fire->MaxFireRadius();
	const float value = (ratio * 0.5f + 1.0f) * static_cast<float>(info.priority);
	const auto priority = static_cast<uint8_t>(value < 255.0f ? value : 255.0f);
	// recent (under 25 turns: the immediate 0x19 at 0x76571C) and too near, or inside the safe radius: at that priority
	// (it flees)
	if ((effects::reactions::Turn() - found->turnCreated < 25 && distance < info.minDistanceToRunAwayFromObject) ||
	    fire->SafeFireRadius() > distance)
	{
		return priority;
	}
	// reacting to something else than a fire: the priority
	if (const auto* current = effects::reactions::Find(currentReaction);
	    current != nullptr && current->type != openblack::Reaction::ReactToFire)
	{
		return priority;
	}
	// not fighting a fire (the final state's exit is not ExitPutOutFire, and not MOVE_AROUND_FIRE): the priority
	const auto final = FinalState(*action);
	const bool fighting = final == VillagerStates::PutOutFireByBeating || final == VillagerStates::PutOutFireWithWater ||
	                      final == VillagerStates::GetWaterToPutOutFire || final == VillagerStates::MoveAroundFire;
	if (!fighting)
	{
		return priority;
	}
	auto* mine = fire::Get(StateOf(villager).fire);
	if (mine == nullptr || mine->object == entt::null)
	{
		return priority;
	}
	// the fire it fights is of the same group: nothing new
	for (const auto* member = fire->root; member != nullptr; member = member->next)
	{
		if (member == mine)
		{
			return 0;
		}
	}
	// within 2 x (10 + maxReactionDistance (0xD4FAC4)) of the fire it fights (GetDistanceInMetres 0x74CD70 of the two
	// fires' objects at 0x76582B): that fire's group takes this one in
	if (!(2.0f * (10.0f + info.maxReactionDistance) < Distance2D(PositionOf(mine->object), PositionOf(fire->object))))
	{
		fire::AddToFireGroup(*mine, *fire);
		return 0;
	}
	return priority;
}

void villager_fire::SetupReactToFire(entt::entity villager, entt::entity object, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !Locator::entitiesRegistry::value().Valid(object))
	{
		return;
	}
	auto& state = StateOf(villager);
	state.object = object;
	state.reaction = reaction;
	// AddReaction (vt 0x990 -> Living::AddReaction 0x5F0F30): the reaction is kept and the state stored, then REACT_TO_FIRE
	StorePreviousState(*action);
	SetTopState(villager, VillagerStates::ReactToFire);
	if (fire::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: villager {} reacts to the fire of object {}", static_cast<int>(villager),
		                   static_cast<int>(object));
	}
}

uint32_t villager_fire::ReactToFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto& state = StateOf(villager);
	// 0x765870..0x7658B2: dynamic_cast<Object*>(+0xBC) null -> return 0; its fire (+0x44) null -> return 0, both with
	// no state change (0x76589A / 0x7658A9). What takes the villager out of 215 is elsewhere: the object going is the
	// validate slot's (+0x80 of 215: villager_reactions::ReactionValidate 0x756A00, run by ProcessState 0x74FF91 before
	// the state: no object or not available -> PopFromPrevious 0x751E50); the fire going out (below its reaction
	// temperature, deleted, or moved) removes its REACT_TO_FIRE (RemoveAllReactionsOfTypeInitiatedByObject 0x6E4780),
	// whose Reaction::ShutDown 0x6E4720 runs StopReactingAndSetState (vt +0x99C) on every follower (ShutDownReaction,
	// called by ECS/Fire/FireEffect.cpp before it removes the reaction)
	if (state.reaction != 0 && effects::reactions::Find(state.reaction) == nullptr)
	{
		// (aproximado) a REACT_TO_FIRE removed by a path that does not go through the fire (Pot::RemoveReaction 0x66D6A0
		// takes all of an object's reactions; openblack's reactions keep no list of followers): the same ShutDown,
		// one turn later
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	if (!registry.Valid(state.object))
	{
		return 0;
	}
	auto* fire = fire::Find(state.object);
	if (fire == nullptr)
	{
		return 0;
	}
	state.fire = fire->id;
	// Living::LookAtObject(object, 2) (inf: faces it at once)
	const auto& info = FireReactionInfo();
	const float distance = Distance2D(PositionOf(villager), PositionOf(state.object)); // fn_0074CD50 at 0x7658D9
	const auto* reaction = effects::reactions::Find(state.reaction);
	const bool recent = reaction != nullptr && effects::reactions::Turn() - reaction->turnCreated < 25; // 0x7658FC
	if ((recent && distance < info.minDistanceToRunAwayFromObject) || !(fire->SafeFireRadius() <= distance))
	{
		// too near: away from it, and then look again (0x765937: in the run-away band if recent, else the safe radius
		// plus GameFloatRand(2.0), the immediate at 0x76599C)
		const float away = recent ? GameFloatRand(info.maxDistanceToRunAwayFromObject - info.minDistanceToRunAwayFromObject) +
		                                info.minDistanceToRunAwayFromObject
		                          : fire->SafeFireRadius() + GameFloatRand(2.0f);
		SetupMoveToWithHug(villager, FleeingPosition(villager, state.object, away), VillagerStates::ReactToFire);
		return 1;
	}
	// 0x765A05: its final state already fights a fire (exit ExitPutOutFire 0x75AE80): StopReactingAndSetState (vt 0x99C
	// 0x5F11C0: ResetStateAfterReacting 0x751E10, then StopReacting; 0x765A48)
	if (const auto final = FinalState(action);
	    final == VillagerStates::PutOutFireByBeating || final == VillagerStates::PutOutFireWithWater ||
	    final == VillagerStates::GetWaterToPutOutFire || final == VillagerStates::MoveAroundFire)
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// 0x765A5B: a villager with a town that is not on its way to worship (+0xE0 0x10) may fight it. The score is the
	// group's burning priority (fn_007302E0) x the room left around it x the town distance term (GetDistanceModifier
	// 0x74F290, 400 m: the immediate at 0x765A77); above 0.1 (0x8AB22C) it goes to beat the fire. The room is
	// fn_00730290 x 2pi (0x8AB210) / (GetRadius (vt 0x60) x 4 (0x8AB418)), 0 when 0, else (room - the firemen count,
	// GetFirstCaused +0x4C) / room. No random term (fn_00730360 IsOnFire is called at 0x765B09, its result unused).
	// (aproximado): fn_00730290, fn_007302E0 and the +0x4C count are taken as GroupBurningRadius, GroupBurningPriority
	// and the size of the firemen list without tracing them; destructive.md §2.5 lists the decision as UNVERIFIED.
	const auto* worshipper = registry.TryGet<const WorshipVillager>(villager);
	if (const auto* v = registry.TryGet<const Villager>(villager);
	    v != nullptr && registry.Valid(v->town) && (worshipper == nullptr || !worshipper->onWay))
	{
		// GetDistanceInMetres 0x74CD70 at 0x765A81, then GUtils::GetDistanceModifier 0x74F290 = SigmoidThreshold(0.5,
		// 1 - min(d, 400) / 400) 0x74F170 over the 41-step table 0xC23284 (ECS/GUtilsDistance): openblack used to
		// approximate it with a smoothstep, which is 0.156 at 250 m where the original gives 0.0444, and saturates to
		// 1 / 0 at the ends instead of 0.99996 / 3.6e-5
		const float townModifier =
		    gutils::GetDistanceModifier(Distance2D(PositionOf(v->town), PositionOf(villager)), 400.0f);
		float room = fire->GroupBurningRadius() * glm::two_pi<float>() / (4.0f * Radius2D(villager));
		if (room != 0.0f)
		{
			room = (room - static_cast<float>(fire->root->firemen.size())) / room;
		}
		const float score = fire->GroupBurningPriority() * room * townModifier;
		if (score > 0.1f && DecideHowToPutOutFire(villager, *fire))
		{
			return 1;
		}
	}
	// else around the fire towards where it was going
	glm::vec2 destination(PositionOf(villager).x, PositionOf(villager).z);
	if (const auto* wallHug = registry.TryGet<const WallHug>(villager))
	{
		destination = wallHug->goal;
	}
	SetupMoveAroundFire(villager, destination, Get(action, LivingAction::Index::Previous));
	return 1;
}

uint32_t villager_fire::PutOutFireByBeating(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* fire = fire::Get(StateOf(villager).fire);
	if (fire != nullptr && IsBesideFire(villager, *fire, 2.0f))
	{
		// LookAtObject(fire, 2) (inf: at once); IsReadyForNewAnimation(1): once the clip played once, every turn
		if (!VillagerAnimationDone(villager, action.turnsSinceStateChange))
		{
			return 1;
		}
		if (fire->IsAboveReactionTemperature() && registry.Valid(fire->object))
		{
			// EffectValues(BURN, -8 (0x99A968), NULL, 1.0, NULL)
			effects::EffectValues values;
			values.numbers[effects::EffectValues::Burn] = -8.0f;
			fire::ApplyEffectToFireEffectIfNecessary(fire->object, values);
			return 1;
		}
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		return 1;
	}
	glm::vec2 position;
	if (fire != nullptr && FireFightingPosition(villager, *fire, position))
	{
		SetupMoveAroundFire(villager, position, VillagerStates::PutOutFireByBeating);
		return 1;
	}
	SetTopState(villager, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_fire::PutOutFireWithWater(LivingAction& action)
{
	SetTopState(Locator::entitiesRegistry::value().ToEntity(action), VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_fire::OnFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* own = fire::Find(villager);
	if (own == nullptr)
	{
		FinishBeingOnFire(villager);
		return 0;
	}
	auto& state = StateOf(villager);
	glm::vec2 target;
	if (state.fire != 0)
	{
		auto* other = fire::Get(state.fire);
		if (other == nullptr || other->object == entt::null)
		{
			return 0; // 0x75B22A: the fire's object is gone: nothing this turn (no FinishBeingOnFire)
		}
		float distance = 0.0f;
		if (own->IsOnFire())
		{
			distance = other->MaxFireRadius() + GameFloatRand(10.0f);
		}
		else
		{
			// GetDistanceInMetres 0x74CD70 of the villager and the fire's object at 0x75B27B
			if (other->MaxFireRadius() < Distance2D(PositionOf(villager), PositionOf(other->object)))
			{
				FinishBeingOnFire(villager);
				return 0;
			}
			distance = other->FireRadius() + GameFloatRand(other->MaxFireRadius() - other->FireRadius() + 1.0f);
		}
		target = FleeingPosition(villager, other->object, distance);
	}
	else
	{
		if (!own->IsOnFire())
		{
			FinishBeingOnFire(villager);
			return 0;
		}
		const auto at = PositionOf(villager);
		// 0x75B32D..0x75B379: GameFloatRand(2 pi) first, then GameFloatRand(6) + 4 [0x8AB418], then me +
		// GetPosFromAngle(angle, distance)
		const float angle = GameFloatRand(glm::two_pi<float>());
		const float distance = GameFloatRand(6.0f) + 4.0f;
		target = PosFromAngle(at, angle, distance);
	}
	SetupMoveToWithHug(villager, target, VillagerStates::OnFire);
	if (Get(action, LivingAction::Index::Previous) == VillagerStates::InvalidState)
	{
		SetStoredState(villager, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_fire::MoveAroundFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto destination = StateOf(villager).savedDestination;
	const auto at = PositionOf(villager);
	// AreWeThere(destination, 0) (vt 0x85C -> MobileWallHug::AreWeThere 0x60AD60): arrived -> the stored state, and
	// DECIDE_WHAT_TO_DO after it
	if (AreWeThere(villager, glm::vec2(at.x, at.z), destination))
	{
		// 0x75A815 PopFromPrevious; 0x75A81A..0x75A827: raw LivingAction::SetState(2, 163) 0x5ECC90 (ecx = +0x8C: not
		// Villager::SetState, so no table +0x10 skip and no town modifiers)
		PopFromPrevious(villager);
		if (auto* again = ActionOf(villager); again != nullptr)
		{
			again->states.at(static_cast<size_t>(LivingAction::Index::Previous)) =
			    static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
		}
		return 1;
	}
	auto* fire = fire::Get(StateOf(villager).fire);
	if (fire == nullptr || fire->object == entt::null)
	{
		return 0;
	}
	// (aproximado) GetViaPoint 0x75A440 (a point around each burning member of the group, up to 1000 tries) is not
	// ported: it walks straight on to the destination
	SetupMoveToWithHug(villager, destination, VillagerStates::MoveAroundFire);
	return 1;
}

uint32_t villager_fire::EnterPutOutFire(LivingAction& action, VillagerStates final, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto result = [&]() -> uint32_t {
		// 0x75ADD9: IsStateEntryFunctionSameAs(final, next) 0x7524D0 (the two rows' +0x10 functions are the same: 216,
		// 217, 218 and 220 all have EnterPutOutFire) -> 1 (0x75AE41)
		const auto same = [](VillagerStates s) {
			return s == VillagerStates::PutOutFireByBeating || s == VillagerStates::PutOutFireWithWater ||
			       s == VillagerStates::GetWaterToPutOutFire || s == VillagerStates::MoveAroundFire;
		};
		if (same(final) && same(next))
		{
			return 1;
		}
		auto& state = StateOf(villager);
		// 0x75ADE2: +0x114 0 -> the refusal (0x75AE55)
		if (state.fire != 0)
		{
			auto* fire = fire::Get(state.fire);
			if (fire == nullptr)
			{
				// 0x75ADEF fn_0075AD90: not in the game's fire list (GGame +0x205C14) -> +0x114 = 0 (0x75AE4B)
				state.fire = 0;
			}
			// 0x75AE00 the fire's vt +0x2C (aproximado: it still has its object), 0x75AE07 a reaction (+0x94) that is
			// not shut down (Reaction +0x34, set by Reaction::ShutDown 0x6E4723; aproximado: openblack still has it)
			else if (fire->object != entt::null && effects::reactions::Find(state.reaction) != nullptr)
			{
				// 0x75AE1E..0x75AE33: already in the group root's fireman list -> 0 (0x75AE75); else AddFireman
				// 0x7309A0 and 1
				if (IsFireman(*fire, villager))
				{
					return 0;
				}
				AddFireman(*fire, villager);
				return 1;
			}
		}
		// 0x75AE55..0x75AE6F: the final state a reactive one (table +0xB8, Infos +0xC8 0xDB9F30) -> StopReacting (vt
		// +0x998); 0 (refused: 0x2F, and Villager::SetTopState enters DECIDE_WHAT_TO_DO)
		if (const auto* info = TableOf(final); info != nullptr && info->field0xb8 != 0)
		{
			villager_reactions::StopReacting(villager);
		}
		return 0;
	}();
	TraceCall(villager, "EnterPutOutFire", final, next, result);
	return result;
}

uint32_t villager_fire::ExitPutOutFire(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	TraceCall(villager, "ExitPutOutFire", FinalState(action), next, 1);
	auto& state = StateOf(villager);
	// 0x75AE95 IsStateExitFunctionSameAs (vt 0x96C) 0x752530: into another state with ExitPutOutFire (216..218, 220) or
	// into a state that is not a final one (table +0x0C) it stays a fireman
	if (!villager::IsStateExitFunctionSameAs(villager, next))
	{
		if (auto* fire = fire::Get(state.fire); fire != nullptr)
		{
			if (!IsFireman(*fire, villager))
			{
				state.fire = 0; // 0x75AEC0: not in the list: nothing more (no ExitReaction), 1
				return 1;
			}
			RemoveFireman(*fire, villager); // 0x75AEDB RemoveFireman 0x7309E0
		}
		state.fire = 0;
		// 0x75AEEE: off its town's way list (Town::RemoveVillagerOnWayToWorshipSite 0x73E360)
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr && registry.Valid(v->town))
		{
			worship::percentage::RemoveVillagerOnWay(v->town, villager);
			if (auto* worshipper = registry.TryGet<WorshipVillager>(villager))
			{
				worshipper->onWayInTown = false;
			}
		}
	}
	// 0x75AF19 ExitReaction (vt 0x910) 0x7527A0: the circle hug reset, and the reaction ends (StopReacting, vt 0x998)
	// unless the next state is a reactive one (table +0xB8)
	villager_reactions::ExitReaction(action, next);
	return 1; // 0x75AF20: always 1, it may leave
}

uint32_t villager_fire::EnterOnFire(LivingAction& action, VillagerStates final, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto result = [&]() -> uint32_t {
		// 0x75AF33..0x75AF44: no fire (+0x114 0) or its vt +0x2C 0 -> 1. (aproximado) a fire openblack no longer has
		// counts as the vt +0x2C test failing
		auto* fire = fire::Get(StateOf(villager).fire);
		if (fire == nullptr)
		{
			return 1;
		}
		// 0x75AF4C..0x75AF61: already in the group root's fireman list -> 0 (0x75AF78); else AddFireman 0x7309A0, 1
		if (IsFireman(*fire, villager))
		{
			return 0;
		}
		AddFireman(*fire, villager);
		return 1;
	}();
	TraceCall(villager, "EnterOnFire", final, next, result);
	return result;
}

uint32_t villager_fire::ExitOnFire(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	TraceCall(villager, "ExitOnFire", FinalState(action), next, 1);
	// 0x75AF83..0x75AFBF: out of the group root's fireman list (RemoveFireman 0x7309E0) if in it; +0x114 = 0 on every
	// path (0x75AFA4 stores the list's end, 0); 1 (it may leave, whatever the next state: no IsStateExitFunctionSameAs)
	auto& state = StateOf(villager);
	if (auto* fire = fire::Get(state.fire); fire != nullptr && IsFireman(*fire, villager))
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	return 1;
}

void villager_fire::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyFireReaction(villager, reaction);
}

void villager_fire::ShutDownReaction(uint32_t reaction)
{
	if (reaction == 0)
	{
		return;
	}
	// Reaction::ShutDown 0x6E4720: +0x34 = 1 (0x6E4723), then while the follower count +0x1C is not 0, the first
	// follower's (+0x18) StopReactingAndSetState (vt +0x99C, 0x6E4731..0x6E4743), whose Living::StopReacting 0x5F1140
	// takes it off the list; then ToBeDeleted (vt +0xC, 0x6E474B). (inferido) the list's order is not kept here: the
	// followers go by entity
	std::vector<entt::entity> followers;
	for (const auto& [villager, state] : g_States)
	{
		if (state.reaction == reaction)
		{
			followers.push_back(villager);
		}
	}
	std::sort(followers.begin(), followers.end());
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto villager : followers)
	{
		const auto it = g_States.find(villager);
		if (it == g_States.end() || it->second.reaction != reaction || !registry.Valid(villager))
		{
			continue;
		}
		if (fire::TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fire: reaction {} shut down: villager {} stops reacting", reaction,
			                   static_cast<int>(villager));
		}
		villager_reactions::StopReactingAndSetState(villager);
	}
}

void villager_fire::Clear()
{
	g_States.clear();
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}

bool villager_fire::IsReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() && it->second.reaction != 0;
}

entt::entity villager_fire::ReactionObject(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() ? it->second.object : entt::entity(entt::null);
}

void villager_fire::StopReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	if (it == g_States.end())
	{
		return;
	}
	// Living::StopReacting 0x5F1140: with a reaction (+0x94): off the reaction's list (+0x18, --count +0x1C; openblack's
	// reactions keep no list of followers), fn_005F0FE0(its type +0x24) = its record gets the turn, +0x94 = 0
	if (it->second.reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToFire),
		                                  effects::reactions::Turn());
		it->second.reaction = 0;
	}
	// 0x5F11A6 / 0x5F11B3: +0xBC = 0 on both paths
	it->second.object = entt::null;
}
