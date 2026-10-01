/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerTeleport.h"
#include "VillagerFire.h"
#include "VillagerMove.h"
#include "VillagerReactions.h"

#include <cmath>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/TownMagic.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Villager +0x94 (the reaction it follows) and +0xBC (the object of it), for the teleport reaction
struct TeleportState
{
	entt::entity stone {entt::null};
	uint32_t reaction {0};
};
std::unordered_map<entt::entity, TeleportState> g_States;
auto& Reg()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Reg().TryGet<LivingAction>(villager);
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

const ReactionInfo& TeleportReactionInfo()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(openblack::Reaction::ReactToTeleport));
}

glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Reg().TryGet<const Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

float Distance2D(const glm::vec3& a, const glm::vec3& b)
{
	return glm::length(glm::vec2(a.x - b.x, a.z - b.z));
}

void RemoveMoveTags(entt::entity villager)
{
	Reg().Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                  MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
}

/// Villager::GetFinalState 0x751DD0: the top state if it is a final one (table +0x0C), else the destination state
VillagerStates FinalState(const LivingAction& action)
{
	const auto top = Get(action, LivingAction::Index::Top);
	const auto* info = TableOf(top);
	return info != nullptr && info->isFinalState != 0 ? top : Get(action, LivingAction::Index::Final);
}

/// Villager::SetTopState (vt 0x8E8), as VillagerFire.cpp does it (the move ends, no destination state): the old final
/// state's exit (VillagerFire.cpp) runs with the new state; the teleport states have no entry function
void SetTopState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	if (const auto previous = FinalState(*action); previous != state)
	{
		villager_fire::CallFinalStateExit(villager, previous, state);
	}
	RemoveMoveTags(villager);
	System().VillagerSetState(*action, LivingAction::Index::Final, VillagerStates::InvalidState, true);
	System().VillagerSetState(*action, LivingAction::Index::Top, state, true);
}

/// Villager::StorePreviousState 0x763470 (AddReaction's vt 0x8EC when it had no reaction): the final state goes to
/// index 2, unless it is a passing one (table +0x10 or +0xB8)
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

/// Villager::PopFromPrevious 0x751E50: the stored state's resume state (table +0x20); nothing stored: DECIDE_WHAT_TO_DO
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

/// fn_006E4340 on the villager's records (Living +0x98, common to the Living: ECS/Effects/Reactions): may it react to
/// this type again (more than `again` turns since the last time)? Remembers it.
bool MayReactAgain(entt::entity villager, openblack::Reaction type, uint32_t again)
{
	return effects::reactions::Records(villager, static_cast<uint8_t>(type), again, effects::reactions::Turn());
}

/// Villager::IsAvailableForReaction 0x763390: its final state takes reactions (table +0xEC), not held or thrown
bool IsAvailableForReaction(entt::entity villager)
{
	const auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return false;
	}
	if (Locator::handSystem::has_value())
	{
		const auto held = Locator::handSystem::value().GetHeldObject();
		if (held.has_value() && *held == villager)
		{
			return false;
		}
	}
	const auto top = Get(*action, LivingAction::Index::Top);
	if (top == VillagerStates::Flying || top == VillagerStates::InHand)
	{
		return false;
	}
	const auto* info = TableOf(FinalState(*action));
	return info != nullptr && info->field0xec != 0;
}

/// The REACT_TO_TELEPORT part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell (as the
/// fire's: with no reaction of its own, fn_006E4620's score above 0 and not reacted to a teleport lately)
void ApplyTeleportReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	if (!registry.AllOf<Villager>(villager) || villager == reaction.initiator || !IsAvailableForReaction(villager))
	{
		return;
	}
	if (effects::reactions::Find(g_States[villager].reaction) != nullptr)
	{
		return;
	}
	const auto& info = TeleportReactionInfo();
	const auto at = PositionOf(villager);
	const auto from = PositionOf(reaction.initiator);
	const float distance = 0.5f * (std::abs(at.x - from.x) + std::abs(at.z - from.z));
	if (distance > info.maxReactionDistance)
	{
		return;
	}
	// fn_006E4620 (ECS/Effects/Reactions: Score)
	const auto priority = villager_teleport::ReactToTeleportPriority(villager, reaction.id);
	const auto score =
	    effects::reactions::Score(static_cast<uint8_t>(openblack::Reaction::ReactToTeleport), true, priority, distance);
	if (score == 0 ||
	    !MayReactAgain(villager, openblack::Reaction::ReactToTeleport, info.numGameTurnsForNormalThingsBeforeReactingAgain))
	{
		return;
	}
	villager_teleport::SetupReactToTeleport(villager, reaction.initiator, reaction.id);
}

/// Villager::ExitReactToTeleport 0x766390 / Living::StopReacting: the reaction ends (the worship-site bookkeeping of
/// +0xE0 bit 4 belongs to VillagerWorship)
void StopReacting(entt::entity villager)
{
	g_States.erase(villager);
}
} // namespace

bool villager_teleport::IsMoving(entt::entity living)
{
	auto& registry = Reg();
	const auto* action = ActionOf(living);
	if (action == nullptr || !registry.AllOf<WallHug>(living))
	{
		return false;
	}
	return Get(*action, LivingAction::Index::Top) == VillagerStates::MoveToPos && !registry.AllOf<MoveStateArrivedTag>(living);
}

glm::vec3 villager_teleport::FinalDestination(entt::entity living)
{
	auto& registry = Reg();
	if (IsMoving(living))
	{
		const auto& goal = registry.Get<const WallHug>(living).goal;
		return {goal.x, 0.0f, goal.y};
	}
	const auto at = PositionOf(living);
	return {at.x, 0.0f, at.z};
}

uint32_t villager_teleport::CurrentReaction(entt::entity living)
{
	const auto it = g_States.find(living);
	return it != g_States.end() ? it->second.reaction : 0;
}

std::optional<PlayerNames> villager_teleport::PlayerOf(entt::entity villager)
{
	const auto* component = Reg().TryGet<const Villager>(villager);
	if (component == nullptr)
	{
		return std::nullopt;
	}
	return worship::town::OwnerOf(component->town);
}

uint8_t villager_teleport::ReactToTeleportPriority(entt::entity villager, uint32_t reaction)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !Reg().Valid(found->initiator) || !Reg().AllOf<MagicTeleport>(found->initiator))
	{
		return 0; // __RTDynamicCast to MagicTeleport failed
	}
	const bool react = magic::teleport::ShouldLivingThingReact(found->initiator, villager);
	return static_cast<uint8_t>((react ? 0xFFu : 0u) & (TeleportReactionInfo().priority & 0xFFu));
}

void villager_teleport::SetupReactToTeleport(entt::entity villager, entt::entity stone, uint32_t reaction)
{
	auto* action = ActionOf(villager);
	if (action == nullptr || !Reg().AllOf<MagicTeleport>(stone))
	{
		return;
	}
	// the stone keeps where it is going (fn_005FC6A0), +0xBC = the stone
	magic::teleport::RegisterDestination(stone, villager, FinalDestination(villager));
	auto& state = g_States[villager];
	state.stone = stone;
	state.reaction = reaction;
	// AddReaction(reaction, 0xC9 + (([+0x5A] > villagerInfo +0x10C) ? 0x32 : 0)): GO_TOWARDS_TELEPORT_REACTION or its
	// _QUICKLY twin (251, the same function). UNVERIFIED: what +0x5A is; 201 is used.
	StorePreviousState(*action);
	SetTopState(villager, VillagerStates::GoTowardsTeleportReaction);
	if (magic::teleport::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: villager {} reacts to stone {} (reaction {})",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(stone), reaction);
	}
}

uint32_t villager_teleport::GoToTeleportReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const auto it = g_States.find(villager);
	if (it == g_States.end() || !registry.Valid(it->second.stone) || effects::reactions::Find(it->second.reaction) == nullptr)
	{
		// the stone (and its reaction) went: Living::StopReactingAndSetState
		StopReacting(villager);
		PopFromPrevious(villager);
		return 0;
	}
	const auto stone = PositionOf(it->second.stone);
	// AreWeThere(stone pos, 0) (vt 0x85C): (inf) where MOVE_TO_POS left it, within the wall hug's arrive step
	if (Distance2D(PositionOf(villager), stone) < 1.0f)
	{
		SetTopState(villager, VillagerStates::TeleportReaction);
		return 1;
	}
	// SetupMoveToWithHug(stone, GetFinalState()): back to this state (201 or 251) once there
	const auto final = FinalState(action);
	SetupMoveToWithHug(villager, glm::vec2(stone.x, stone.z),
	                   final == VillagerStates::GoTowardsTeleportReactionQuickly ? final : VillagerStates::GoTowardsTeleportReaction);
	return 1;
}

uint32_t villager_teleport::TeleportReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	// 0x7663FB / 0x76641C: no reaction, or its initiator not a MagicTeleport: nothing (the state stays, as in the
	// original)
	const auto it = g_States.find(villager);
	if (it == g_States.end())
	{
		return 0;
	}
	const auto stone = it->second.stone;
	if (!registry.Valid(stone) || !registry.AllOf<MagicTeleport>(stone))
	{
		return 0;
	}
	magic::teleport::DoTeleport(stone, villager, false);
	// StopReactingAndSetState (vt 0x99C): ResetStateAfterReacting 0x751E10 (PopFromPrevious) and StopReacting
	StopReacting(villager);
	PopFromPrevious(villager);
	return 1;
}

void villager_teleport::ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	if (magic::teleport::TraceEnabled())
	{
		const auto& info = TeleportReactionInfo();
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Teleport: REACT_TO_TELEPORT {} of stone {} to villager {}: priority {} radius {:.1f} distance "
		                   "weight {:.2f} again {}",
		                   reaction.id, static_cast<uint32_t>(reaction.initiator), static_cast<uint32_t>(villager),
		                   info.priority, reaction.radius, info.howImportantIsDistance,
		                   info.numGameTurnsForNormalThingsBeforeReactingAgain);
	}
	ApplyTeleportReaction(villager, reaction);
}

void villager_teleport::LandAt(entt::entity villager, const glm::vec3& mapPosition)
{
	auto* transform = Reg().TryGet<Transform>(villager);
	if (transform == nullptr)
	{
		return;
	}
	// fn_005DA0C0: the interface puts the villager down at the stone; FLYING then LANDED are its landing, and then
	// DecideWhatToDo (vt 0x8C8) chooses where it goes. (aproximado) FLYING and LANDED are not run: it is put at the
	// stone and decides at once
	transform->position = magic::ToWorld(glm::vec3(mapPosition.x, 0.0f, mapPosition.z));
	DecideWhatToDo(villager);
	Reg().SetDirty();
}

void villager_teleport::DecideWhatToDo(entt::entity villager)
{
	SetTopState(villager, VillagerStates::DecideWhatToDo);
}

void villager_teleport::OnMoved(entt::entity living)
{
	// MoveMapObject leaves the walk as it was: the step is made again from the new position
	auto& registry = Reg();
	if (auto* wallHug = registry.TryGet<WallHug>(living); wallHug != nullptr && IsMoving(living))
	{
		wallHug->step = glm::vec2(0.0f);
		RemoveMoveTags(living);
		registry.Remove<WallHugObjectReference>(living);
		registry.Assign<MoveStateLinearTag>(living);
	}
}

void villager_teleport::Clear()
{
	g_States.clear();
	villager_reactions::Register(); // the Villager handler of ECS/Effects/Reactions
}
