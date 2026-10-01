/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScript.h"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

// Disassembly: dev\tmp_dis\miracles\all.asm (GScript::MoveGameThing 0x6F8E80, SetScriptState 0x6F8370 / 0x6F82E0,
// SetScriptUlong 0x6F8770, Played 0x6F9DC0) and `bwdis.py` on 0x5F2830, 0x60AAD0, 0x60AD40, 0x60AD60, 0x5ED7E0,
// 0x5ED9A0, 0x5ED9C0, 0x768780, 0x768840, 0x768970, 0x7689C0, 0x7689D0, 0x768A00, 0x5ECAC0, 0x5EC990, 0x5ECB80.

namespace openblack::ecs::villager
{
using namespace components;
using state_info::StateInfo;

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

/// Villager::IsStateEntryFunctionSameAs 0x7524D0 (a, b): the entry functions (+0x10 of the 0x90-byte rows, 0xD091A8)
/// of both states are the same (all four dwords; the first 0 -> the same at once, 0x7524F4)
bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b)
{
	const auto entryOf = [](VillagerStates s) {
		const auto i = static_cast<size_t>(static_cast<uint8_t>(s));
		return i < k_OriginalStateFns.size() ? k_OriginalStateFns.at(i).entry : 0u;
	};
	return entryOf(a) == entryOf(b);
}

/// Living::IsStateForInterface 0x417070 (vt +0x968): the state is 0x18 IN_HAND
bool IsStateForInterface(VillagerStates state)
{
	return state == VillagerStates::InHand;
}

/// Living::ExitNoChangeState 0x768780 (vt +0x928): 1 if `next` is script-interruptable (vt +0x964, table file 0x1C,
/// 0xDB9E94), is IN_HAND (vt +0x968) or IsStateExitFunctionSameAs (vt +0x96C); else 0
uint32_t ExitNoChangeState(entt::entity villager, VillagerStates next)
{
	if (state_info::IsScriptInterruptable(StateInfo(next)) || IsStateForInterface(next) ||
	    IsStateExitFunctionSameAs(villager, next))
	{
		return 1;
	}
	return 0;
}

/// MobileWallHug::InitStepsXZ 0x60BFA0 for openblack's WallHug: the heading at the goal and a step of its speed along
/// it (as PathfindingSystem's InitializeStepToGoal). (aproximado: SetTowardsAngle's turn limit is not applied, as in
/// openblack's other walks)
void InitStepsXZ(Transform& transform, WallHug& wallHug)
{
	const auto diff = wallHug.goal - glm::xz(transform.position);
	const auto angle = glm::atan(diff.y, diff.x);
	transform.rotation = glm::eulerAngleY(-angle - glm::radians(90.0f));
	wallHug.step = glm::vec2(glm::cos(angle), glm::sin(angle)) * wallHug.speed;
	wallHug.yAngle = angle;
}

/// MobileWallHug::SetupMobileMoveToPos 0x60AAD0 (pos): +0x80 = pos (0x60AADC..0x60AAED), InitStepsXZ (0x60AAF2), off
/// the circle-hug lists (fn_00611AC0 / fn_00611610 / fn_00612BB0 / fn_00610590, +0x76 = 0: openblack's WallHug keeps
/// none), then AreWeThere(0) (0x60AB87) -> +0x5E = 1 ARRIVED (0x60AB91); else CircleHugInfo::Reset (0x60AB9F), +0x78 = 1
/// and +0x5E = 0xB STEP_THROUGH (0x60ABA4..0x60ABA8)
void SetupMobileMoveToPos(entt::entity villager, const glm::vec2& goal)
{
	auto& registry = Entities();
	auto& wallHug = registry.Get<WallHug>(villager);
	auto& transform = registry.Get<Transform>(villager);
	wallHug.goal = goal;
	InitStepsXZ(transform, wallHug);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	if (AreWeThere(villager, goal, 0.0f))
	{
		// ARRIVED: PathfindingSystem puts it on the goal and VillagerMoveToPos sees 0xA (MoveTo's ARRIVED 0x60AFC0)
		registry.Assign<MoveStateArrivedTag>(villager, MoveStateClockwise::Undefined, goal);
		return;
	}
	registry.Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform.position));
}

/// Living::SetAnim(n) 0x5ECB80 (vt +0x8FC): SetAnim(GetAnimId() (vt +0x900), n) 0x5ECBA0: the state's clip if it is
/// another one, from its start when n != 0 and not dancing (vt +0x978; IN_SCRIPT and the script states are never a
/// dance: not tested)
void SetAnim(entt::entity villager, int32_t n)
{
	VillagerSetStateClip(villager, n != 0);
}
} // namespace

bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r)
{
	const auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	if (transform == nullptr || wallHug == nullptr)
	{
		return false;
	}
	// 0x60AD64..0x60ADB0: dx = Pos.x - pos.x, dz = Pos.z - pos.z, R = (float)(u16 +0x5A) + r; dx^2 + dz^2 < R^2 -> 1
	// (fcompp then `test ah, 0x41; jne` 0x60ADAB: R^2 <= d^2 -> 0)
	const float radius = wallHug->speed + r;
	return glm::distance2(glm::xz(transform->position), pos) < radius * radius;
}

std::optional<glm::vec2> GetDestPos(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	if (wallHug == nullptr)
	{
		return std::nullopt;
	}
	return wallHug->goal;
}

bool AreWeThereAtDestination(entt::entity villager, float r)
{
	// 0x60AD4B: GetDestPos (vt +0x860), 0x60AD54: AreWeThere(dest, r) (vt +0x85C)
	const auto dest = GetDestPos(villager);
	return dest.has_value() && AreWeThere(villager, *dest, r);
}

uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction, WallHug, Transform>(villager))
	{
		return 0;
	}
	// 0x5F2834..0x5F2841: the byte at GLivingInfo +0x124 (moveState; 1 MOVE_TO_POS in all the villager rows of info.dat);
	// 0x5F283D..0x5F2847: 3 MOVE_ON_STRUCTURE instead when GameThingWithPos +0x24 & 0x80, a flag only
	// Living::MoveOnStructure sets (state 3, not ported in openblack: never set here)
	const auto moveState = static_cast<VillagerStates>(static_cast<uint8_t>(InfoOf(villager).moveState));
	// 0x5F285A: SetCurrentAndDestinationState (vt +0x8DC) (moveState, final); not 1 -> 0 (0x5F287B)
	if (SetCurrentAndDestinationState(villager, moveState, final) != 1)
	{
		return 0;
	}
	// 0x5F286C: SetupMobileMoveToPos(pos) (the one without a hug state, 0x60AAD0), then 1
	SetupMobileMoveToPos(villager, goal);
	return 1;
}

void StorePreviousState(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// 0x763470: GetFinalState; a passing one (table +0x10 never kept in PREVIOUS, or +0xB8 a reaction) keeps what
	// PREVIOUS had; raw LivingAction::SetState 0x5ECC90 (0x7634B5), no town modifiers
	const auto final = GetFinalState(villager);
	const auto& info = StateInfo(final);
	auto stored = final;
	if (state_info::NotStoredAsPrevious(info) || state_info::IsReactive(info))
	{
		stored = GetState(villager, Index::Previous);
	}
	action->states.at(static_cast<size_t>(Index::Previous)) = static_cast<uint8_t>(stored);
}

bool IsAvailable(entt::entity villager)
{
	// 0x751D50: +0xA & 1 (the thing is being deleted: openblack's entity is gone instead) -> 0; GetFinalState (vt +0xB04)
	// == 0xE DYING -> 0; else 1
	return Entities().Valid(villager) && ActionOf(villager) != nullptr && GetFinalState(villager) != VillagerStates::Dying;
}

bool IsObjectInMap(entt::entity villager)
{
	return GetState(villager, Index::Top) != VillagerStates::InHand;
}

void SetScriptState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// 0x6F831E..0x6F8335: IsAvailable (vt +0x2C) and IsObjectInMap (vt +0x178), else nothing
	if (!IsAvailable(villager) || !IsObjectInMap(villager))
	{
		return;
	}
	// 0x6F833B: StorePreviousState (vt +0x8EC)
	StorePreviousState(villager);
	// 0x6F834A: CallExitStateFunction(state) (vt +0x904), 0x6F8355: CallEntryStateFunction(state) (vt +0x90C); both
	// results are ignored
	CallExitStateFunction(villager, state);
	CallEntryStateFunction(villager, state);
	// (aproximado) The original's walk only advances from MOVE_TO_POS's state function (Living::MoveToPos 0x5EC270 ->
	// MobileWallHug::MoveTo 0x60AF20), so a villager the script takes out of it stops where it is; openblack's
	// PathfindingSystem steps every entity with a move tag whatever its state, so the tags go here
	if (GetState(villager, Index::Top) != VillagerStates::MoveToPos)
	{
		Entities().Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		                  MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		Entities().Remove<WallHugObjectReference>(villager);
	}
	// 0x6F8361: SetAnim(1) (vt +0x8FC)
	SetAnim(villager, 1);
	// 0x6F8367: +0x58 = 0 (word)
	action->turnsUntilStateChange = 0;
}

void SetScriptAnimation(entt::entity villager, uint32_t anim, uint32_t loops)
{
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return;
	}
	// 0x6F884D: +0x120 = the times (the first pop), 0x6F8855: +0x11C = the clip (the second pop)
	v->scriptAnimLoops = loops;
	v->scriptAnim = anim;
}

int32_t ScriptAnimation(entt::entity villager)
{
	const auto* v = Entities().TryGet<const Villager>(villager);
	// 0x768A00: mov eax, [ecx + 0x11C]
	return v != nullptr ? static_cast<int32_t>(v->scriptAnim) : 0;
}

bool IsScriptAnimationComplete(entt::entity villager)
{
	// 0x7689D3: GetTopState 0x5F27F0; 0x17 WAIT_FOR_ANIMATION -> 0 (0x7689D8)
	const auto top = GetState(villager, Index::Top);
	if (top == VillagerStates::WaitForAnimation)
	{
		return false;
	}
	// 0x7689E0: 0xC8 SCRIPT_PLAY_ANIM -> +0x120 == 0
	if (top == VillagerStates::ScriptPlayAnim)
	{
		const auto* v = Entities().TryGet<const Villager>(villager);
		return v == nullptr || v->scriptAnimLoops == 0;
	}
	// 0x7689F3: 1
	return true;
}

void PlayAnimThenSetState(entt::entity villager, VillagerStates state)
{
	// 0x5ECACB: CallExitStateFunction(state) (vt +0x904); 0 -> nothing (0x5ECAD3)
	if (CallExitStateFunction(villager, state) == 0)
	{
		return;
	}
	// 0x5ECADC: CallEntryStateFunction(0x17, state) (vt +0x908): TOP 23 WAIT_FOR_ANIMATION, FINAL state; no clip
	CallEntryStateFunction(villager, VillagerStates::WaitForAnimation, state);
}

uint32_t StateInScript([[maybe_unused]] LivingAction& action)
{
	// 0x5ED9A0: +0xB0 == 0 -> DataForScriptRemind::Create 0x5EF190 (not ported); 1
	return 1;
}

uint32_t EnterInScript(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// 0x5ED7F7: dynamic_cast<Villager*>: always one here. 0x5ED821: IsStateEntryFunctionSameAs(final, next) -> 1
	if (IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// 0x5ED82E..0x5ED83F: no DataForScriptRemind (+0xB0), or its +0x44 is not `next` -> 1. The remind data is not ported
	// (inferido: without it the villager has no walk to resume), so the branch that resumes it (0x5ED845..0x5ED989:
	// fn_005EF2A0, then SetupMoveToPos(+0x80 or the remind's point, next) and 0x23) is never taken
	(void)action;
	return 1;
}

uint32_t ExitInScript(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	// 0x5ED9C8: CircleHugInfo::Reset (openblack's WallHug has no circle-hug info); 0x5ED9D1: IsDancing (vt +0x978),
	// result unused
	// 0x5ED9E7: IsScriptState(next) (vt +0x960, table file 0x18, 0xDB9E90) -> 1
	if (state_info::IsScriptState(StateInfo(next)))
	{
		return 1;
	}
	// 0x5ED9FB..0x5EDA33: DataForScriptRemind::Create if none, +0x44 = GetFinalState, KeepThatInMind 0x5EF1D0: not ported
	// 0x5EDA3D: ExitNoChangeState(next) (vt +0x928)
	return ExitNoChangeState(villager, next);
}

uint32_t ScriptPlayAnim(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* v = Entities().TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return 1;
	}
	// 0x768973..0x76897B: +0x120 == 0 -> nothing
	if (v->scriptAnimLoops > 0)
	{
		// 0x76897D..0x768980: one time less
		--v->scriptAnimLoops;
		// 0x768986..0x768995: PlayAnimThenSetState(times left ? 0xC8 SCRIPT_PLAY_ANIM : 4 IN_SCRIPT, 1)
		PlayAnimThenSetState(villager, v->scriptAnimLoops > 0 ? VillagerStates::ScriptPlayAnim : VillagerStates::InScript);
		// 0x76899A..0x7689A9: DataForScriptRemind::Create if none (not ported)
	}
	return 1;
}

uint32_t EnterPlayAnim(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// 0x768858: dynamic_cast<Villager*>: always one here. 0x768881: IsStateEntryFunctionSameAs(final, next) -> 1
	if (IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// 0x76888E..0x76889F: no DataForScriptRemind or its +0x44 is not `next` -> 1; the remind branch (0x7688A5..0x768957:
	// back to its walk, or its clip +0x3C again) is not ported (inferido: never taken without the remind data)
	(void)action;
	return 1;
}

uint32_t ExitPlayAnim(LivingAction& action, VillagerStates next)
{
	// 0x7689C7: vt +0x914 (Villager: ExitInScript 0x5ED9C0)
	return ExitInScript(action, next);
}

uint32_t WaitForAnimation(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// 0x5EC995: IsReadyForNewAnimation(1) 0x5EC960 -> SetTopStateToFinal 0x5ECA80 and 0; else 1
	if (VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		SetTopStateToFinal(villager);
		return 0;
	}
	return 1;
}
} // namespace openblack::ecs::villager
