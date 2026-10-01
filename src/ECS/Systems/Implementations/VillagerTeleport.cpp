/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerTeleport.h"
#include "VillagerMove.h"
#include "VillagerReactions.h"

#include <cmath>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Objects/MagicTeleport.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipPercentage.h"

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

/// MobileWallHug::AreWeThere(pos, extra 0) 0x60AD60 (Living vt 0x85C): d^2 < (the wall hug's step +0x5A + extra)^2,
/// strictly (0x60ADAB `test ah, 0x41`); the step is RebuildMoveByStep 0x609D10's, openblack's WallHug::speed
bool AreWeThere(entt::entity villager, const glm::vec3& goal)
{
	const auto* wallHug = Reg().TryGet<const WallHug>(villager);
	const float step = wallHug != nullptr ? wallHug->speed : 0.0f;
	const auto at = PositionOf(villager);
	const glm::vec2 d(at.x - goal.x, at.z - goal.z);
	return glm::dot(d, d) < step * step;
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

/// Villager::SetTopState 0x752010 (vt +0x8E8): the villager core's (ECS/Villager/VillagerCore.h), which runs the exit
/// functions of TOP and of the final state and the entry of the new state from the rows of k_VillagerStateTable (the
/// fire's, the teleport's and the worship ones among them), once each, with the pause roll (only into a state whose
/// row may pause: not 163, 201, 202) and the codes 1 / 0x2E / 0x2F; the walk's end as villager_reactions::SetTopState
uint32_t SetTopState(entt::entity villager, VillagerStates state)
{
	return villager_reactions::SetTopState(villager, state);
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

} // namespace

bool villager_teleport::IsReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() && it->second.reaction != 0;
}

void villager_teleport::StopReacting(entt::entity villager)
{
	const auto it = g_States.find(villager);
	if (it == g_States.end())
	{
		return;
	}
	// Living::StopReacting 0x5F1140: with a reaction (+0x94): fn_005F0FE0(its type +0x24) = its record gets the turn,
	// +0x94 = 0; +0xBC (the stone) = 0 on both paths: nothing of the teleport stays
	if (it->second.reaction != 0)
	{
		effects::reactions::RefreshRecord(villager, static_cast<uint8_t>(openblack::Reaction::ReactToTeleport),
		                                  effects::reactions::Turn());
	}
	g_States.erase(it);
}

uint32_t villager_teleport::ExitReactToTeleport(LivingAction& action, VillagerStates next)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	// 0x76639A..0x7663AD: IsStateExitFunctionSameAs(next) (vt +0x96C, 0x752530)
	const bool same = villager::IsStateExitFunctionSameAs(villager, next);
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("ExitReactToTeleport({}) same {}", static_cast<int>(next), same ? 1 : 0));
	}
	if (!same)
	{
		auto* worshipper = registry.TryGet<WorshipVillager>(villager);
		// 0x7663B3..0x7663C4: GetTown (vt +0x48) -> Town::RemoveVillagerOnWayToWorshipSite 0x73E360
		if (const auto* v = registry.TryGet<const Villager>(villager); v != nullptr && registry.Valid(v->town))
		{
			worship::percentage::RemoveVillagerOnWay(v->town, villager);
			if (worshipper != nullptr)
			{
				worshipper->onWayInTown = false;
			}
		}
		// 0x7663C9: +0xE0 &= ~0x10, with or without a town
		if (worshipper != nullptr)
		{
			worshipper->onWay = false;
		}
	}
	// 0x7663D2..0x7663D7: ExitReaction (vt +0x910) 0x7527A0, its result (1)
	return villager_reactions::ExitReaction(action, next);
}

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
		// (inferido) the stone (and its reaction) went: Living::StopReactingAndSetState 0x5F11C0 (in the original the
		// validate slot ReactionValidate 0x756A00 pops the state when the object goes; not ported)
		villager_reactions::StopReactingAndSetState(villager);
		return 0;
	}
	const auto stone = PositionOf(it->second.stone);
	// 0x76632E AreWeThere(the reaction's object position, 0) (vt 0x85C) -> TELEPORT_REACTION (0x766341)
	if (AreWeThere(villager, stone))
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
	// 0x76642F StopReactingAndSetState (vt 0x99C, 0x5F11C0): ResetStateAfterReacting 0x751E10 (PopFromPrevious, then
	// 163 if the final state is a reactive one), then StopReacting if still reacting
	villager_reactions::StopReactingAndSetState(villager);
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
