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
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/VillagerSpeed.h"
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

/// MobileWallHug +0x5A: the speed as a whole MapCoords distance a turn (GetSpeedInMetres 0x60C070 =
/// ConvertWholeDistanceToMeters of it). openblack keeps it in metres a turn (WallHug::speed = the u16 / 6553.6,
/// ECS/VillagerSpeed.cpp), so the u16 comes back by the same factor, rounded: a plain truncation of u16 / 6553.6 x
/// 6553.6 in float can give u16 - 1 and turn a word one above the threshold into "not faster"
int32_t SpeedUnits(entt::entity villager)
{
	const auto* wallHug = Reg().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? static_cast<int32_t>(std::lround(wallHug->speed * ecs::MapInterface::k_PositionToGridFactor))
	                          : 0;
}

/// GMobileWallHugInfo +0x10C, in the same units: openblack's speedGroup.speed2 (the third dword of the group, as
/// Living::FleeFromPredatorPriority 0x5F15ED reads it for the same comparison)
int32_t SpeedThresholdUnits(entt::entity villager)
{
	const auto* info = VillagerInfoOf(villager);
	return info != nullptr ? static_cast<int32_t>(info->speedGroup.speed2) : 0;
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

/// The REACT_TO_TELEPORT part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell (as the
/// fire's: with no reaction of its own, fn_006E4620's score above 0 and not reacted to a teleport lately)
void ApplyTeleportReaction(entt::entity villager, const effects::reactions::Reaction& reaction)
{
	auto& registry = Reg();
	// the check of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 (0x6E401D) for every reaction type: vt +0x984 =
	// Villager::IsAvailableForReaction 0x763390, the common one
	if (!registry.AllOf<Villager, LivingAction>(villager) || villager == reaction.initiator ||
	    !villager::IsAvailableForReaction(villager))
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

entt::entity villager_teleport::ReactionObject(entt::entity villager)
{
	const auto it = g_States.find(villager);
	return it != g_States.end() ? it->second.stone : entt::entity(entt::null);
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
	if (!registry.AllOf<WallHug>(living))
	{
		return false;
	}
	// Object::IsMoving 0x402710 (Villager vt 0x174): the thing's position (GameThingWithPos::Pos +0x14 x, +0x18 z) is
	// not the one in Object::coords (+0x2C, +0x30), the position of the turn before, so it moved during the last turn.
	// (aproximado) openblack keeps no previous-turn position: a Living with a move state that is not ARRIVED and a step
	// to take stands for it. That covers any walking state, not only MOVE_TO_POS (the walk to the worship site, a
	// reaction's walk...), which is what the original's test does.
	if (registry.AllOf<MoveStateArrivedTag>(living))
	{
		return false;
	}
	if (!registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                    MoveStateFinalStepTag>(living))
	{
		return false;
	}
	return registry.Get<const WallHug>(living).speed > 0.0f;
}

glm::vec3 villager_teleport::FinalDestination(entt::entity living)
{
	// Villager::GetFinalDestPos 0x756AD0 -> Living::GetFinalDestPos 0x5EC1E0: with a footpath and a node on it (+0xC8,
	// +0xCC) the last non-hidden node of the path (GFootpath::GetEndNonHiddenNode 0x535120 with the direction flag
	// (status +0xB4 >> 3) & 1), else MobileWallHug::GetDestPos (vt 0x860, 0x416F70) = the goal (+0x80), whether it is
	// moving or not. (pendiente) the footpath branch: no openblack Living walks on a components::Footpath, so none has
	// the original's +0xC8 / +0xCC
	auto& registry = Reg();
	if (const auto* wallHug = registry.TryGet<const WallHug>(living); wallHug != nullptr)
	{
		return {wallHug->goal.x, 0.0f, wallHug->goal.y};
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
	// the stone keeps where it is going (GetFinalDestPos vt 0x884 at 0x766297, fn_005FC6A0 at 0x7662A5), +0xBC = the
	// stone (0x7662AC)
	magic::teleport::RegisterDestination(stone, villager, FinalDestination(villager));
	auto& state = g_States[villager];
	state.stone = stone;
	state.reaction = reaction;
	// 0x7662B2..0x7662D8: AddReaction(reaction, 0xC9 + 0x32 x (speed > threshold)) (vt 0x990). The number compared is
	// the villager's own speed (MobileWallHug +0x5A, the u16 GetSpeedInMetres 0x60C070 turns into metres) against its
	// info's speed2 (GMobileWallHugInfo +0x10C, the same field AnimalFlee reads at 0x5F15ED): `setle` on the
	// zero-extended word, so 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY only when it is strictly faster, else 201. Both
	// rows run the same state function (251 jumps to 201's): only the state table row changes (animation, speed index)
	const bool quick = SpeedUnits(villager) > SpeedThresholdUnits(villager);
	StorePreviousState(*action);
	SetTopState(villager, quick ? VillagerStates::GoTowardsTeleportReactionQuickly : VillagerStates::GoTowardsTeleportReaction);
	if (magic::teleport::TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Teleport: villager {} reacts to stone {} (reaction {}), state {}",
		                   static_cast<uint32_t>(villager), static_cast<uint32_t>(stone), reaction, quick ? 251 : 201);
	}
}

uint32_t villager_teleport::GoToTeleportReaction(LivingAction& action)
{
	auto& registry = Reg();
	const auto villager = registry.ToEntity(action);
	const auto it = g_States.find(villager);
	// 0x7662F0 checks nothing: the validate slot (+0x80) of 201/202/251, villager_reactions::ReactionValidate 0x756A00,
	// pops the state (PopFromPrevious 0x751E50) once the stone goes, before the state runs (ProcessState 0x74FF91).
	// 0x7662F6 GetReaction 0x5ECA60 -> its object's position, which the original reads with no null test; here, with
	// no stone kept, the state only returns 0 (a guard against reading nothing; ReactionValidate has popped by then)
	if (it == g_States.end() || !registry.Valid(it->second.stone))
	{
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
	if (!Reg().AllOf<Transform>(villager))
	{
		return;
	}
	// fn_005FC4F0, in its order: SetTopState(FLYING 10) 0x5FC4FD, the interface puts the villager down at the stone
	// (fn_005DA0C0 0x5FC517: the hand took it out first, HandApplyToObject.cpp), SetTopState(LANDED 11) 0x5FC51E and
	// DecideWhatToDo (vt 0x8C8) 0x5FC52C. (aproximado) LANDED's own state function (the landing animation) only runs
	// until DecideWhatToDo replaces it in the same turn, as in the original
	SetTopState(villager, VillagerStates::Flying);
	// the Transform is taken again: a state change may have moved the registry's storage
	if (auto* transform = Reg().TryGet<Transform>(villager); transform != nullptr)
	{
		const auto world = magic::ToWorld(glm::vec3(mapPosition.x, 0.0f, mapPosition.z));
		// fn_005DA0C0: +0x14 = the position, then InsertMapObject (vt +0x544) at 0x5DA0E7 (the hand took it out of the
		// map, HandSystem::PickUp). (openblack) one that is still in the map moves there, so its lists stay right
		if (map_cells::IsObjectInMap(villager))
		{
			map_cells::MoveMapObject(villager, world);
		}
		else
		{
			transform->position = world;
			map_cells::InsertMapObject(villager);
		}
	}
	SetTopState(villager, VillagerStates::Landed);
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
