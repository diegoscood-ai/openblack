/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerFire.h"

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

/// The fire's state functions, by the state they belong to (the entry and exit of a final state change)
bool CallExit(entt::entity villager, VillagerStates state)
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
		return villager_fire::ExitPutOutFire(*action);
	case VillagerStates::OnFire:
		return villager_fire::ExitOnFire(*action);
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
		CallExit(villager, previous);
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

/// Living::SetupMoveToWithHug 0x5F2890: the move state (GLivingInfo +0x124: MOVE_TO_POS) with `final` as the
/// destination state (SetCurrentAndDestinationState), then MobileWallHug::SetupMobileMoveToPos
void SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto* action = ActionOf(villager);
	auto& registry = Locator::entitiesRegistry::value();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (action == nullptr || wallHug == nullptr)
	{
		return;
	}
	wallHug->goal = goal;
	wallHug->step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	System().VillagerSetState(*action, LivingAction::Index::Final, final, true);
	System().VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::MoveToPos, true);
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
	// (state 59, a worshipper on its way: Town::AddVillagerOnWayToWorshipSite, M7)
	return true;
}

/// fn_0075ABA0: the villager stands in the band just outside the fire (its radius plus the fire's) and 2 m further
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

/// Living +0x98: the reactions it reacted to lately, {type, turn} (fn_006E4340 forgets the others after 1800 turns and
/// keeps at most 3)
std::unordered_map<entt::entity, std::vector<std::pair<openblack::Reaction, uint32_t>>> g_Memory;

/// fn_006E4340: may it react to this type again (more than `again` turns since the last time)? Remembers it.
bool MayReactAgain(entt::entity villager, openblack::Reaction type, uint32_t again)
{
	const uint32_t turn = effects::reactions::Turn();
	auto& memory = g_Memory[villager];
	std::erase_if(memory, [turn, type](const auto& entry) { return entry.first != type && turn - entry.second > 0x708; });
	for (auto& entry : memory)
	{
		if (entry.first == type)
		{
			if (turn - entry.second > again)
			{
				entry.second = turn;
				return true;
			}
			return false;
		}
	}
	if (memory.size() >= 3)
	{
		memory.erase(memory.begin());
	}
	memory.emplace_back(type, turn);
	return true;
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
	// TODO(M6): not inside a magic shield (fn_0072B990)
	const auto at = PositionOf(villager);
	const auto from = PositionOf(reaction.initiator);
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
	const auto priority = static_cast<float>(villager_fire::ReactToFirePriority(villager, reaction.id, 0));
	float score = priority * (0.5f * (info.howImportantIsDistance * (info.maxReactionDistance - distance) /
	                                  info.maxReactionDistance) +
	                          1.0f);
	score = score < 255.0f ? score : 255.0f;
	if (static_cast<uint8_t>(score) == 0 ||
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
	// recent (under 25 turns) and too near, or inside the safe radius: at that priority (it flees)
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
	// (a worshipper, +0xE0 bit 4, also goes around the fire towards its destination: M7)
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
	const bool recent = reaction != nullptr && effects::reactions::Turn() - reaction->turnCreated < 25;
	if ((recent && distance < info.minDistanceToRunAwayFromObject) || !(fire->SafeFireRadius() <= distance))
	{
		// too near: away from it, and then look again
		const float away = recent ? GameFloatRand(info.maxDistanceToRunAwayFromObject - info.minDistanceToRunAwayFromObject) +
		                                info.minDistanceToRunAwayFromObject
		                          : fire->SafeFireRadius() + GameFloatRand(2.0f);
		SetupMoveToWithHug(villager, FleeingPosition(villager, state.object, away), VillagerStates::ReactToFire);
		return 1;
	}
	// its town's villagers may fight it: the score is the group's burning priority x the room left around it x the town
	// distance term (400 m); above 0.1 it goes to beat the fire
	if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr && registry.Valid(v->town))
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
			return 0;
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
	// AreWeThere (vt 0x85C): arrived -> the stored state, and DECIDE_WHAT_TO_DO after it
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
	// TODO(M5): GetViaPoint 0x75A440 (a point around each burning member of the group, up to 1000 tries); straight on
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

bool villager_fire::ExitPutOutFire(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	auto& state = StateOf(villager);
	if (auto* fire = fire::Get(state.fire); fire != nullptr && IsFireman(*fire, villager))
	{
		RemoveFireman(*fire, villager);
	}
	state.fire = 0;
	// ExitReaction 0x7527A0: the reaction ends unless the next state is a fire-fighting one (table +0xB8)
	state.reaction = 0;
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

void villager_fire::SpreadReaction(const effects::reactions::Reaction& reaction)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::entitiesMap::has_value() || !registry.Valid(reaction.initiator))
	{
		return;
	}
	const auto& info = FireReactionInfo();
	// Reaction::GetRadius (vt 0x60, +0x3C; inf: the info's maxReactionDistance) and GetMapCellSpiralSizeFromRadius
	// 0x74F520 (max(int(0.2 R), 1)^2 cells) x GetReactionPower (1)
	const float radius = info.maxReactionDistance;
	int count = std::max(static_cast<int>(0.2f * radius), 1);
	count *= count;
	const auto origin = PositionOf(reaction.initiator); // Reaction::GetPos 0x6E45C0 (inf: the initiator's position)
	const auto start = glm::ivec2(MapInterface::GetGridCell(glm::vec2(origin.x, origin.z)));
	glm::ivec2 cell = start;
	int direction = 1;
	int steps = 1;
	const auto& map = Locator::entitiesMap::value();
	for (int i = 0; i < count; ++i)
	{
		const glm::vec3 at(origin.x + static_cast<float>(cell.x - start.x) * 10.0f, 0.0f,
		                   origin.z + static_cast<float>(cell.y - start.y) * 10.0f);
		if (cell.x >= 0 && cell.y >= 0 && cell.x < MapInterface::k_GridSize.x && cell.y < MapInterface::k_GridSize.y &&
		    !(radius < Distance2D(origin, at)))
		{
			// ApplyReactionToLivingObjectsAtSquare 0x6E3F90: the villagers of the cell
			const MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
			std::vector<entt::entity> living(map.GetMobileInGridCell(id).begin(), map.GetMobileInGridCell(id).end());
			std::sort(living.begin(), living.end());
			for (const auto villager : living)
			{
				ApplyFireReaction(villager, reaction);
			}
		}
		// GUtils::Spiral 0x74D7E0
		static constexpr glm::ivec2 k_Steps[4] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
		if (--steps == 0)
		{
			++direction;
			steps = direction / 2;
		}
		cell += k_Steps[direction & 3];
	}
}

void villager_fire::Clear()
{
	g_States.clear();
	g_Memory.clear();
	effects::reactions::SetSpreadHandler(openblack::Reaction::ReactToFire, &SpreadReaction);
}
