/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScript.h"

#include <optional>

#include <fmt/format.h>

#include "3D/CameraTrack.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingWalkPath.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/ScriptStateArguments.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Flocks.h"
#include "ECS/LivingAngle.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/Implementations/VillagerHome.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerScriptMemory.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/WalkGoal.h"
#include "ECS/WalkStateOf.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::villager_script
{
using components::LivingAction;
using components::ScriptAnimation;
using components::ScriptStateArguments;
using components::Villager;
using villager::Index;
using villager::state_info::StateInfo;
namespace rules = villager_script_rules;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

/// SetAnim(n): the state's clip if it is another one, from its start when n != 0 and not dancing (IN_SCRIPT and the
/// script states are never a dance: not tested)
void SetAnim(entt::entity villager, int32_t n)
{
	villager_animate::PlayStateClip(villager, n != 0);
}
} // namespace

bool CanBeDirected(entt::entity villager)
{
	// IsAvailable and IsObjectInMap
	return villager::IsAvailable(villager) && villager::IsObjectInMap(villager);
}

void SetScriptState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// IsAvailable and IsObjectInMap, else nothing
	if (!CanBeDirected(villager))
	{
		return;
	}
	// StorePreviousState
	villager::StorePreviousState(villager);
	// CallExitStateFunction(state), CallEntryStateFunction(state); both
	// results are ignored
	villager::CallExitStateFunction(villager, state);
	villager::CallEntryStateFunction(villager, state);
	// the walk it was on is kept: it advances only from the walking states' functions, so the villager stops where it
	// is and goes on if it comes back to it; the exit let go of the circle it was hugging
	// SetAnim(1)
	SetAnim(villager, 1);
	// The counter = 0
	action->turnsUntilStateChange = 0;
}

void MoveTo(entt::entity villager, glm::vec2 goal, float goalAltitude)
{
	// only when it is in the map and not drowning, else nothing
	if (!villager::IsObjectInMap(villager) || IsDrowning(villager))
	{
		return;
	}
	// not there yet -> SetupMoveToPos(coords, IN_SCRIPT 4); else SetScriptState(IN_SCRIPT 4)
	if (!villager::AreWeThere(villager, goal, 0.0f))
	{
		// the goal is the point's map coordinates, its height above the land kept
		if (villager::SetupMoveToPos(villager, goal, VillagerStates::InScript) == 1)
		{
			walk_goal::SetAltitude(villager, goalAltitude);
		}
	}
	else
	{
		SetScriptState(villager, VillagerStates::InScript);
	}
}

void SetScriptAnimation(entt::entity villager, uint32_t clip, uint32_t plays)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	// The times (the first pop), then the clip (the second pop); only the simulation reads them
	Entities().AssignOrReplaceState<ScriptAnimation>(villager, static_cast<AnimId>(clip), plays);
}

void SetScriptPlace(entt::entity villager, glm::vec2 place)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	// The number stays as the script last gave it
	const auto* found = Entities().TryGet<const ScriptStateArguments>(villager);
	const float number = found != nullptr ? found->number : 0.0f;
	Entities().AssignOrReplaceState<ScriptStateArguments>(villager, place, number);
}

void SetScriptNumber(entt::entity villager, float number)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	// The place stays as the script last gave it
	const auto* found = Entities().TryGet<const ScriptStateArguments>(villager);
	const glm::vec2 place = found != nullptr ? found->place : glm::vec2 {0.0f};
	Entities().AssignOrReplaceState<ScriptStateArguments>(villager, place, number);
}

void SetScriptPlaceX(entt::entity villager, int32_t x)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	const auto* found = Entities().TryGet<const ScriptStateArguments>(villager);
	auto arguments = found != nullptr ? *found : ScriptStateArguments {};
	arguments.place.x = map_coords::ToMetres(x);
	Entities().AssignOrReplaceState<ScriptStateArguments>(villager, arguments.place, arguments.number);
}

int32_t ScriptClip(entt::entity villager)
{
	// clip 0 for none asked for
	const auto* animation = Entities().TryGet<const ScriptAnimation>(villager);
	return animation != nullptr ? static_cast<int32_t>(animation->clip) : 0;
}

bool HasPlayedScriptAnimation(entt::entity villager)
{
	// TOP 23 WAIT_FOR_ANIMATION -> 0; TOP 200 SCRIPT_PLAY_ANIM -> the times left == 0 (none without a clip asked for);
	// else 1
	const auto top = villager::GetState(villager, Index::Top);
	const auto* animation = Entities().TryGet<const ScriptAnimation>(villager);
	return rules::HasPlayedScriptClip(top, animation != nullptr ? animation->playsLeft : 0);
}

uint32_t InScript(LivingAction& action)
{
	// Its script memory, made when it has none; 1
	script_memory::Make(EntityOf(action));
	return 1;
}

namespace
{
/// A walk back to where its memory says its walk was going, its height with it, then `next`
void WalkBackTo(entt::entity villager, VillagerStates next)
{
	const auto* memory = script_memory::Find(villager);
	const auto destination = memory != nullptr ? memory->destination : map_coords::MapCoords {};
	if (villager::SetupMoveToPos(villager, map_coords::ToMetres(destination), next) == 1)
	{
		walk_goal::SetAltitude(villager, destination.altitude);
	}
}
} // namespace

uint32_t EnterInScript(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (villager::IsStateEntryFunctionSameAs(final, next))
	{
		if (script_memory::DebugOn() && script_memory::Find(EntityOf(action)) != nullptr)
		{
			script_memory::Debug(EntityOf(action), fmt::format("entering {} from {}, the same entry: its script memory "
			                                                   "not taken up",
			                                                   static_cast<int>(next), static_cast<int>(final)));
		}
		return 1;
	}
	// No script memory, or not one kept from `next` -> 1
	const auto villager = EntityOf(action);
	auto* memory = script_memory::Of(villager);
	if (memory == nullptr || !script_memory::Resumes(false, memory->state, next))
	{
		if (memory != nullptr && script_memory::DebugOn())
		{
			script_memory::Debug(villager,
			                     fmt::format("entering {} from {}: its script memory of {} not taken up",
			                                 static_cast<int>(next), static_cast<int>(final), static_cast<int>(memory->state)));
		}
		return 1;
	}
	// It takes up what it kept, once: the memory is given back
	memory->state = VillagerStates::InvalidState;
	script_memory::GiveBack(villager);
	// On a camera track's walk it goes back to where the walk has got to on the land, in SCRIPT_GO_AND_MOVE_ALONG_PATH,
	// on the land (height 0); else back to its walk's destination, in `next`. Either way it set its own states
	if (auto* walk = Entities().TryGet<components::LivingWalkPath>(villager); walk != nullptr && walk->path.track != nullptr)
	{
		const auto point = camera_track::LivingWalkPoint(walk->path.walk, *walk->path.track);
		static_cast<void>(villager::SetupMoveToPos(villager, point, VillagerStates::ScriptGoAndMoveAlongPath));
		return villager::k_EntryNoSet;
	}
	WalkBackTo(villager, next);
	return villager::k_EntryNoSet;
}

uint32_t ScriptGoAndMoveAlongPath(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* walk = Entities().TryGet<components::LivingWalkPath>(villager);
	if (walk == nullptr || walk->path.track == nullptr)
	{
		return 1;
	}
	// There: it walks the track again from the share it has walked, to the same end and the same way, into IN_SCRIPT
	if (villager::AreWeThereAtDestination(villager, 0.0f))
	{
		const auto from = living::GetWalkPathPercentage(villager).value_or(0.0f);
		const auto track = walk->trackNumber;
		const auto to = walk->path.walk.to;
		const auto forward = walk->path.walk.forward;
		static_cast<void>(living::StartWalkPath(villager, track, VillagerStates::InScript, from, to, forward));
		return 1;
	}
	// Else on its way again to where the walk has got to
	const auto point = camera_track::LivingWalkPoint(walk->path.walk, *walk->path.track);
	static_cast<void>(villager::SetupMoveToPos(villager, point, VillagerStates::ScriptGoAndMoveAlongPath));
	return 1;
}

uint32_t ExitInScript(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	// First, whatever comes next and whatever this returns, the walker lets go of the circle it went round or was
	// heading for: the circle, its turns to it and its entry distance all go with it. Then IsDancing, result unused
	ecs::wall_hug::ResetCircleHug(Entities(), villager);
	// IsScriptState(next) (state info table) -> 1. Else its script memory, made when it has none, keeps the state it
	// leaves (GetFinalState) and what it had of it. Then ExitNoChangeState(next): 1 if `next` is script-interruptable,
	// is IN_HAND or IsStateExitFunctionSameAs (the same exit as GetFinalState's, or not a final state); else 0
	const auto& info = StateInfo(next);
	if (!villager::state_info::IsScriptState(info))
	{
		script_memory::Make(villager).state = villager::GetFinalState(villager);
		script_memory::KeepInMind(villager);
	}
	const rules::StateRules nextRules {.isScriptState = villager::state_info::IsScriptState(info),
	                                   .isScriptInterruptable = villager::state_info::IsScriptInterruptable(info),
	                                   .isFinalState = villager::state_info::IsFinal(info)};
	const bool sameExit = villager::HaveSameExitFunction(villager::GetFinalState(villager), next);
	return rules::ScriptLetsGo(next, nextRules, sameExit) ? 1 : 0;
}

uint32_t ScriptPlayAnim(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* animation = Entities().TryGet<ScriptAnimation>(villager);
	// None asked for -> nothing
	if (animation == nullptr)
	{
		return 1;
	}
	// No times left -> nothing; else one time less and PlayAnimThenSetState(times left ? 200 SCRIPT_PLAY_ANIM : 4
	// IN_SCRIPT, 1)
	const auto step = rules::StepScriptClip(animation->playsLeft);
	animation->playsLeft = step.playsLeft;
	if (step.then.has_value())
	{
		villager::PlayAnimThenSetState(villager, *step.then);
		// its script memory, made when it has none
		script_memory::Make(villager);
	}
	return 1;
}

uint32_t ScriptWanderAroundPosition(LivingAction& action)
{
	// Its script memory, made when it has none, first
	script_memory::Make(EntityOf(action));
	// One turn less to stand at this stop; once none was left the villager sets off for the next one
	const auto step = rules::StepWander(action.turnsUntilStateChange);
	action.turnsUntilStateChange = step.turnsLeft;
	if (!step.setsOff)
	{
		return 1;
	}
	const auto villager = EntityOf(action);
	const auto* found = Entities().TryGet<const ScriptStateArguments>(villager);
	const auto arguments = found != nullptr ? *found : ScriptStateArguments {};
	// The next stop, round the script's place and within its distance; the walk there goes round what is in the way
	// and comes back into this state
	const auto stop =
	    rules::NextWanderStop(arguments.place, arguments.number, [](float most) { return game_random::GameFloatRand(most); });
	villager::SetupMoveToWithHug(villager, map_coords::ToMetres(stop), VillagerStates::ScriptWanderAroundPosition);
	// The turns to stand there, from the pair of whole numbers the script gave (the same pair a script's clip and its
	// plays are kept in): the fewest, up to one under the most
	const auto* pair = Entities().TryGet<const ScriptAnimation>(villager);
	const uint32_t fewest = pair != nullptr ? static_cast<uint32_t>(pair->clip) : 0;
	const uint32_t most = pair != nullptr ? pair->playsLeft : 0;
	action.turnsUntilStateChange =
	    rules::WanderPause(fewest, most, [](uint32_t range) { return game_random::GameRand(range); });
	return 1;
}

uint32_t EnterPlayAnim(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (villager::IsStateEntryFunctionSameAs(final, next))
	{
		if (script_memory::DebugOn() && script_memory::Find(EntityOf(action)) != nullptr)
		{
			script_memory::Debug(EntityOf(action), fmt::format("entering {} from {}, the same entry: its script memory "
			                                                   "not taken up",
			                                                   static_cast<int>(next), static_cast<int>(final)));
		}
		return 1;
	}
	// No script memory, or not one kept from `next` -> 1
	const auto villager = EntityOf(action);
	auto* memory = script_memory::Of(villager);
	if (memory == nullptr || !script_memory::Resumes(false, memory->state, next))
	{
		return 1;
	}
	// It gets back what it kept; with a destination it is not at, it walks back there first, in `next`
	script_memory::GiveBack(villager);
	memory->state = VillagerStates::InvalidState;
	if (script_memory::HasDestination(memory->destination) && !villager::AreWeThereAtDestination(villager, 0.0f))
	{
		WalkBackTo(villager, next);
		return villager::k_EntryNoSet;
	}
	// Else the clip its state last put on it, from the start when it plays another (one out of the list: the first)
	const auto clip = script_memory::ReplayedClip(memory->stateClip, living::k_ForcedClipCount);
	if (villager_animate::CurrentClip(villager) != clip)
	{
		villager_animate::OverrideAnim(villager, clip);
	}
	return 1;
}

uint32_t ExitPlayAnim(LivingAction& action, VillagerStates next)
{
	// ExitInScript
	return ExitInScript(action, next);
}

void ReleaseFromScript(entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return;
	}
	// While the map is being cleared nothing happens; openblack's map change releases nothing, so this never arises
	// here. Its flock: the flock's shepherd cleared when it is this one (openblack's flocks have no shepherd yet), then
	// it leaves the flock
	if (const auto flock = flocks::FlockOf(villager); flock != entt::null)
	{
		flocks::RemoveLiving(flock, villager, false);
	}
	const bool hasTown = v->town != entt::null && registry.Valid(v->town);
	// In the physics or in the hand: without a town, the vagrants' head
	if (Locator::dynamicsSystem::value().IsFlying(villager) || fire::traits::InHand(villager))
	{
		if (!hasTown)
		{
			town_villagers::AddToVagrants(villager);
		}
		return;
	}
	// life <= 0 -> VillagerDead(0, the player who last dropped it, which for a villager is always none, 0, 1); not a
	// death state -> SetTopState(15)
	if (!(v->life > 0.0f))
	{
		villager::VillagerDead(villager, DeathReason::None, std::nullopt, 0.0f, 1);
		const auto* action = registry.TryGet<const LivingAction>(villager);
		const auto top = action != nullptr ? action->states[static_cast<size_t>(LivingAction::Index::Top)] : 0;
		if (!(top >= 13 && top <= 17))
		{
			villager::SetTopState(villager, VillagerStates::Dead);
		}
		return;
	}
	// A town -> DecideWhatToDo
	if (hasTown)
	{
		if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr)
		{
			villager::DecideWhatToDo(*action);
		}
		return;
	}
	// The vagrants (unless there already), SetTopState(130), VagrantStart
	town_villagers::AddToVagrants(villager);
	villager::SetTopState(villager, VillagerStates::VagrantStart);
	villager::VagrantStart(villager);
}

} // namespace openblack::ecs::villager_script
