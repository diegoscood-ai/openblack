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
#include <spdlog/spdlog.h>

#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MapShield.h"
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

auto& System()
{
	return Locator::livingActionSystem::value();
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

float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

/// GRand::GameFloatRand
float GameFloatRand(float max)
{
	return Locator::rng::value().NextValue(0.0f, max);
}

/// The fire's state functions, by the state they belong to (the entry and exit of a final state change), and the
/// worship exits a reaction leaves (58, 59, 60, 213: VillagerWorship.cpp); the exit functions take the next state, as
/// the original's (ExitPutOutFire(E) 0x75AE80)
bool CallExit(entt::entity villager, VillagerStates state, VillagerStates next)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return false;
	}
	switch (state)
	{
	case VillagerStates::PutOutFireByBeating:
	case VillagerStates::PutOutFireWithWater:
	case VillagerStates::GetWaterToPutOutFire:
	case VillagerStates::MoveAroundFire:
		return villager_fire::ExitPutOutFire(*action, next);
	case VillagerStates::OnFire:
		return villager_fire::ExitOnFire(*action);
	case VillagerStates::GotoWorshipSiteForWorship:
	case VillagerStates::ArrivesAtWorshipSiteForWorship:
		return villager_worship::ExitMoveToWorshipSite(*action, next);
	case VillagerStates::WorshippingAtWorshipSite:
	case VillagerStates::HidingAtWorshipSite:
		return villager_worship::ExitAtWorshipSite(*action, next);
	default:
		return false;
	}
}

void CallEntry(entt::entity villager, VillagerStates from, VillagerStates to)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	switch (to)
	{
	case VillagerStates::PutOutFireByBeating:
	case VillagerStates::PutOutFireWithWater:
	case VillagerStates::GetWaterToPutOutFire:
	case VillagerStates::MoveAroundFire:
		villager_fire::EnterPutOutFire(*action, from, to);
		break;
	case VillagerStates::OnFire:
		villager_fire::EnterOnFire(*action, from, to);
		break;
	default:
		break;
	}
}

/// Villager::SetTopState (vt 0x8E8): the old final state's exit, the new state, its entry (inf: the entry and exit
/// functions go with the final state, so walking to a point on the way does not leave the state)
void SetTopState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	const auto previous = FinalState(*action);
	if (previous != state)
	{
		CallExit(villager, previous, state);
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	System().VillagerSetState(*action, LivingAction::Index::Final, VillagerStates::InvalidState, true);
	System().VillagerSetState(*action, LivingAction::Index::Top, state, true);
	if (previous != state)
	{
		CallEntry(villager, previous, state);
	}
}

/// Villager::SetState (vt 0x938) for the stored state (index 2): not for a state with table +0x10 set
void SetStoredState(LivingAction& action, VillagerStates state)
{
	const auto* info = TableOf(state);
	if (info != nullptr && info->field0x10 != 0)
	{
		return;
	}
	action.states.at(static_cast<size_t>(LivingAction::Index::Previous)) = static_cast<uint8_t>(state);
}

/// Villager::StorePreviousState 0x763470: the final state is kept in index 2, unless it is a passing one (table +0x10
/// or +0xB8), which keeps what was stored
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

/// Villager::PopFromPrevious 0x751E50: back to the stored state's resume state (table +0x20); nothing stored: 0 ->
/// DECIDE_WHAT_TO_DO (inf: SetTopState of 0 falls through to deciding)
void PopFromPrevious(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	const auto stored = Get(*action, LivingAction::Index::Previous);
	const auto* info = TableOf(stored);
	auto next = info != nullptr ? static_cast<VillagerStates>(info->field0x20) : VillagerStates::DecideWhatToDo;
	if (next == VillagerStates::InvalidState)
	{
		next = VillagerStates::DecideWhatToDo;
	}
	SetTopState(villager, next);
	action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
}

/// Living::SetupMoveToWithHug 0x5F2890: the villagers' shared one (VillagerMove.cpp: TOP, then FINAL)
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

/// GUtils::GetPosFromAngle 0x74D580 (inf: x = cos, z = sin; the angle is random anyway)
glm::vec2 FromAngle(float angle, float radius)
{
	return glm::vec2(std::cos(angle), std::sin(angle)) * radius;
}

float Radius2D(entt::entity object)
{
	return fire::traits::Radius(object);
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
	const float angle = std::atan2(at.z - centre.z, at.x - centre.x); // Get3DAngleFromXZ(fire, villager)
	const float radius = Radius2D(fire.object);
	const float safe = fire.SafeFireRadius();
	const float keep = safe < radius ? safe : radius;
	const float distance = keep + Radius2D(villager) + GameFloatRand(1.0f);
	const auto object = PositionOf(fire.object);
	out = glm::vec2(object.x, object.z) + FromAngle(angle, distance);
	return true;
}

/// Villager::SetupMoveAroundFire 0x75A770: MOVE_AROUND_FIRE towards `destination`, then `after`
bool SetupMoveAroundFire(entt::entity villager, const glm::vec2& destination, VillagerStates after)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return false;
	}
	SetTopState(villager, VillagerStates::MoveAroundFire);
	StateOf(villager).savedDestination = destination;
	SetStoredState(*action, after);
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
/// fire's part is max(safe radius, the object's radius) (read at 0x75ABE0..0x75ABFB), unlike GetFireFightingPos' min
bool IsBesideFire(entt::entity villager, const fire::FireEffect& fire, float band)
{
	if (fire.object == entt::null)
	{
		return false;
	}
	const float distance = Distance2D(PositionOf(villager), PositionOf(fire.object));
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

/// GUtils::GetDistanceModifier 0x74F290 -> SigmoidThreshold(1 - min(d, max) / max, 0.5) 0x74F170 (inf: a smooth step
/// around 0.5)
float DistanceModifier(float distance, float maximum)
{
	const float x = 1.0f - (distance < maximum ? distance : maximum) / maximum;
	const float t = std::clamp((x - 0.25f) / 0.5f, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
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
	// fn_0072B990 (0x6E4031): not under a shield the fire is not inside
	if (magic::map_shield::IsReactionBlockedByShield(magic::ToMap(at), magic::ToMap(from)))
	{
		return;
	}
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
	const float distance = Distance2D(PositionOf(villager), PositionOf(found->initiator)); // fn_0074CD50
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
	// within 2 x (10 + maxReactionDistance (0xD4FAC4)) of the fire it fights: that fire's group takes this one in
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
	if (!registry.Valid(state.object))
	{
		PopFromPrevious(villager);
		return 0;
	}
	auto* fire = fire::Find(state.object);
	if (fire == nullptr)
	{
		PopFromPrevious(villager);
		return 0;
	}
	state.fire = fire->id;
	// Living::LookAtObject(object, 2) (inf: faces it at once)
	const auto& info = FireReactionInfo();
	const float distance = Distance2D(PositionOf(villager), PositionOf(state.object));
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
	// 0x5F11C0: ResetStateAfterReacting, then StopReacting)
	if (const auto final = FinalState(action);
	    final == VillagerStates::PutOutFireByBeating || final == VillagerStates::PutOutFireWithWater ||
	    final == VillagerStates::GetWaterToPutOutFire || final == VillagerStates::MoveAroundFire)
	{
		PopFromPrevious(villager);
		state.reaction = 0;
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
		const float townModifier = DistanceModifier(Distance2D(PositionOf(v->town), PositionOf(villager)), 400.0f);
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
		target = glm::vec2(at.x, at.z) + FromAngle(GameFloatRand(glm::two_pi<float>()), GameFloatRand(6.0f) + 4.0f);
	}
	SetupMoveToWithHug(villager, target, VillagerStates::OnFire);
	if (Get(action, LivingAction::Index::Previous) == VillagerStates::InvalidState)
	{
		SetStoredState(action, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_fire::MoveAroundFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	const auto destination = StateOf(villager).savedDestination;
	const auto at = PositionOf(villager);
	// AreWeThere (vt 0x85C): arrived -> the stored state, and DECIDE_WHAT_TO_DO after it. (inferido) the 1 m radius
	// stands in for AreWeThere, as in VillagerTeleport.cpp
	if (glm::length(glm::vec2(at.x, at.z) - destination) < 1.0f)
	{
		PopFromPrevious(villager);
		SetStoredState(action, VillagerStates::DecideWhatToDo);
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

bool villager_fire::EnterPutOutFire(LivingAction& action, VillagerStates from, VillagerStates to)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// IsStateEntryFunctionSameAs 0x7524D0: from one fire-fighting state to another, nothing to do
	const auto same = [](VillagerStates s) {
		return s == VillagerStates::PutOutFireByBeating || s == VillagerStates::PutOutFireWithWater ||
		       s == VillagerStates::GetWaterToPutOutFire || s == VillagerStates::MoveAroundFire;
	};
	if (same(from) && same(to))
	{
		return true;
	}
	auto& state = StateOf(villager);
	auto* fire = fire::Get(state.fire);
	if (fire != nullptr && fire->object != entt::null)
	{
		if (!IsFireman(*fire, villager))
		{
			AddFireman(*fire, villager);
		}
		return true;
	}
	state.fire = 0;
	return false;
}

bool villager_fire::ExitPutOutFire(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto& state = StateOf(villager);
	const auto* nextInfo = TableOf(next);
	// IsStateExitFunctionSameAs (vt 0x96C) 0x752530: into another fire-fighting state (216..218, 220: the same exit) or
	// into a state that is not a final one (table +0x0C) it stays a fireman
	const bool same = next == VillagerStates::PutOutFireByBeating || next == VillagerStates::PutOutFireWithWater ||
	                  next == VillagerStates::GetWaterToPutOutFire || next == VillagerStates::MoveAroundFire ||
	                  nextInfo == nullptr || nextInfo->isFinalState == 0;
	if (!same)
	{
		if (auto* fire = fire::Get(state.fire); fire != nullptr)
		{
			if (!IsFireman(*fire, villager))
			{
				state.fire = 0; // 0x75AEC0: not in the list: nothing more (no ExitReaction)
				return true;
			}
			RemoveFireman(*fire, villager);
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
	// ExitReaction (vt 0x910) 0x7527A0: the reaction ends (StopReacting, vt 0x998) unless the next state is a reactive
	// one (table +0xB8, IsReactiveState 0x7525B0)
	if (nextInfo == nullptr || nextInfo->field0xb8 == 0)
	{
		state.reaction = 0;
	}
	return true;
}

bool villager_fire::EnterOnFire(LivingAction& action, [[maybe_unused]] VillagerStates from, [[maybe_unused]] VillagerStates to)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto* fire = fire::Get(StateOf(villager).fire);
	if (fire != nullptr && !IsFireman(*fire, villager))
	{
		AddFireman(*fire, villager);
	}
	return true;
}

bool villager_fire::ExitOnFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto& state = StateOf(villager);
	if (auto* fire = fire::Get(state.fire); fire != nullptr && IsFireman(*fire, villager))
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	return true;
}

void villager_fire::CallFinalStateExit(entt::entity villager, VillagerStates state, VillagerStates next)
{
	CallExit(villager, state, next);
}

void villager_fire::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	ApplyFireReaction(villager, reaction);
}

void villager_fire::Clear()
{
	g_States.clear();
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
