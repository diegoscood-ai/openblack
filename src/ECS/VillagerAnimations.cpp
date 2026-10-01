/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimations.h"

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/L3DAnim.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAnimationTable.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

// Research: dev\tmp_dis\anim\villager_anims.md (runblack.exe: Villager::GetAnimId 0x750110, the animation functions
// 0x423400..0x424442, Living::SetTopState 0x5F28E0, Villager::FinishedIntoOutOfAnimation 0x750060).

namespace openblack::ecs
{
using namespace components;
using namespace villager_animation;

namespace
{
// ANM_ indices of Data\AllMeshes.h (= AllAnims.anm)
enum Anim : int32_t
{
	k_DontDraw = -4,
	k_AttractYourAttention = 207,
	k_Beckon = 210,
	k_CarryAxe = 215,
	k_CarryObjectRun = 216,
	k_ChoppingTree = 217,
	k_ConductMeeting = 222,
	k_CoupleKissMan = 223,
	k_CoupleKissWoman = 224,
	k_CrawlInjured = 225,
	k_CrawlInjuredInto = 226,
	k_CrowdImpressed1 = 227,
	k_CrowdWon = 234,
	k_Dead1 = 243,
	k_Dead2 = 246,
	k_DeadDrowned = 249,
	k_Dying = 253,
	k_Hammering = 276,
	k_IntoDeadDrowned = 283,
	k_IntoHammering = 284,
	k_IntoMourning = 285,
	k_IntoPointing = 286,
	k_IntoPray = 287,
	k_IntoSawWood = 292,
	k_IntoSledgehammer = 293,
	k_IntoSleep = 294,
	k_Landed = 306,
	k_LandedFromBack = 307,
	k_LandedFromFeet = 308,
	k_LandedFromFeetCarryObject = 309,
	k_LookingForSomething = 310,
	k_LookAtHand = 311,
	k_OutOfHammering = 320,
	k_OutOfMourning = 321,
	k_OutOfPray = 323,
	k_OutOfSawWood = 328,
	k_OutOfSledgehammer = 329,
	k_OutOfSleep = 330,
	k_Overworked1 = 332,
	k_Overworked2 = 333,
	k_PanicMan = 334,
	k_PanicWoman = 335,
	k_PickUpSticks = 340,
	k_Pray = 343,
	k_RunMan = 351,
	k_RunWoman = 353,
	k_SawWood = 354,
	k_ScaredStiff = 355,
	k_ScaredStiff2 = 356,
	k_SittingDown1Into = 367,
	k_SittingDown1Out = 368,
	k_SittingDown1 = 369,
	k_SittingDown2Into = 370,
	k_SittingDown2Out = 371,
	k_SittingDown2 = 372,
	k_Sledgehammer = 380,
	k_SprintRunMan = 383,
	k_SprintRunWoman = 384,
	k_Stand = 385,
	k_StandDespair1 = 386,
	k_TalkingAndPointing = 395,
	k_Thrown = 399,
	k_ThrownDead = 400,
	k_WaitingImpatiently = 418,
	k_WalkInjured = 426,
	k_WalkMan = 427,
	k_WalkWoman = 431,
	k_Yawn = 437,
	k_Yawn2 = 438,
};

enum Carried : int32_t
{
	k_CarriedNoChange = 0,
	k_CarriedNone = 1,
	k_CarriedAxe = 2,
	k_CarriedSaw = 5,
	k_CarriedBall = 7,
	k_CarriedHammer = 8,
	k_CarriedMalletHeavy = 9,
};

int32_t Random(int32_t count)
{
	return Locator::rng::value().NextValue<int32_t>(0, count - 1);
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates TopState(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	return action != nullptr ? static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)])
	                         : VillagerStates::InvalidState;
}

int32_t CurrentClip(entt::entity villager)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(villager);
	return animation != nullptr && animation->hasClip ? animation->clipIndex : -1;
}

bool IsMale(const Villager& villager)
{
	return villager.sex == Villager::Sex::MALE;
}

bool IsWomanOrChild(const Villager& villager)
{
	return !IsMale(villager) || villager.lifeStage == Villager::LifeStage::Child;
}

/// MapCoords::IsWater (0x6035B0) of the villager's Pos (+0x14)
bool IsInWater(entt::entity villager)
{
	const auto* transform = Locator::entitiesRegistry::value().TryGet<const Transform>(villager);
	return transform != nullptr && sea_cells::IsWater(transform->position);
}

float Life(const Villager& villager)
{
	return villager.life;
}

/// Villager::SetStateCarriedObject (0x7501A0), the part openblack has: what the states force (villagers carry no wood or
/// food yet)
int32_t CarriedObject(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	int32_t carried = k_CarriedNone;
	if (action == nullptr)
	{
		return carried;
	}
	const auto current = static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)]);
	const auto final = static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Final)]);
	if (static_cast<size_t>(final) < 255 && StateInfo(final).field0xdc != k_CarriedNoChange)
	{
		carried = StateInfo(final).field0xdc;
	}
	if (static_cast<size_t>(current) < 255 && StateInfo(current).field0xdc != k_CarriedNoChange)
	{
		carried = StateInfo(current).field0xdc;
	}
	return carried;
}

/// MoveToPosAnimation (0x423400): walk, run or sprint by the speed; carry clips; wounded
int32_t MoveToPosAnimation(entt::entity entity, const Villager& villager)
{
	const auto& info = Locator::infoConstants::value();
	const auto& villagerInfo = info.villager.at(0);
	if (Life(villager) <= villagerInfo.lifeWhenCrawlsWounded)
	{
		return k_CrawlInjured;
	}
	if (Life(villager) <= villagerInfo.lifeWhenWalksWounded)
	{
		return k_WalkInjured;
	}
	const auto carried = CarriedObject(entity);
	const bool carrying = carried != k_CarriedNone && carried != k_CarriedSaw && carried != k_CarriedBall &&
	                      carried != k_CarriedHammer && carried > k_CarriedNoChange;
	// SPEED_THRESHOLD_VILLAGER_MAN / _NORMAL
	const auto& threshold = info.speedThreshold.at(IsMale(villager) ? 1 : 0);
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(entity);
	// WallHug::speed is per turn (0.1 s)
	const float speed = wallHug != nullptr ? wallHug->speed * 10.0f : 0.0f;
	if (speed <= GetSpeedStateSpeed(threshold.speedMaxWalk))
	{
		return carrying ? k_CarryAxe : IsMale(villager) ? k_WalkMan : k_WalkWoman;
	}
	if (speed <= GetSpeedStateSpeed(threshold.speedMaxRun))
	{
		return carrying ? k_CarryObjectRun : IsMale(villager) ? k_RunMan : k_RunWoman;
	}
	return carrying ? k_CarryObjectRun : IsMale(villager) ? k_SprintRunMan : k_SprintRunWoman;
}

/// The state's animation function (slot 0x60). Functions that need what openblack doesn't have yet (landing type,
/// water, vortices, dance groups, fights, football, creatures) take the branch of the original for its absence.
int32_t StateFunctionAnim(AnimFn function, entt::entity entity, const Villager& villager, int32_t fallback)
{
	switch (function)
	{
	case AnimFn::None:
		return fallback;
	case AnimFn::MoveToPos:
		return MoveToPosAnimation(entity, villager);
	case AnimFn::FootballAttacker: // STAND; the move clip comes from the distance sync
	case AnimFn::FootballDefender:
		return k_Stand;
	case AnimFn::Landed: // landType 0 (on its feet): no landing types yet
		return CarriedObject(entity) == k_CarriedNone ? k_LandedFromFeet : k_LandedFromFeetCarryObject;
	case AnimFn::Dying: // DyingAnimation 0x423770: in the water P_INTO_DEAD_DROWNED; else landType 0 (no landing types yet)
		return IsInWater(entity) ? k_IntoDeadDrowned : k_Dying;
	case AnimFn::Dead: // DeadAnimation 0x4237A0: in the water P_DEAD_DROWNED; else landType 0
		return IsInWater(entity) ? k_DeadDrowned : k_Dead1;
	case AnimFn::Thrown: // not in a vortex
		return Life(villager) <= 0.0f ? k_ThrownDead : k_Thrown;
	case AnimFn::Kissing:
		return IsMale(villager) ? k_CoupleKissMan : k_CoupleKissWoman;
	case AnimFn::Forestering:
		return k_ChoppingTree;
	case AnimFn::Building:
	{
		// BuildingAnimation (0x423E20): the working clip after its into clip, else one of the three at random
		// and it sets the carried object: hammer, saw or heavy mallet
		const auto current = CurrentClip(entity);
		int32_t clip = k_Hammering;
		if (current == k_IntoHammering)
		{
			clip = k_Hammering;
		}
		else if (current == k_IntoSawWood)
		{
			clip = k_SawWood;
		}
		else if (current == k_IntoSledgehammer)
		{
			clip = k_Sledgehammer;
		}
		else
		{
			const std::array<int32_t, 3> clips = {k_Hammering, k_SawWood, k_Sledgehammer};
			clip = clips.at(static_cast<size_t>(Random(3)));
		}
		if (auto* animation = Locator::entitiesRegistry::value().TryGet<SkeletalAnimation>(entity);
		    animation != nullptr && !animation->carriedLocked)
		{
			animation->carriedObject = clip == k_Hammering ? k_CarriedHammer : clip == k_SawWood ? k_CarriedSaw : k_CarriedMalletHeavy;
		}
		return clip;
	}
	case AnimFn::Script: // Villager::ScriptAnimation 0x768A00: +0x11C, the clip SET_SCRIPT_ULONG gave
		return static_cast<int32_t>(villager.scriptAnim);
	case AnimFn::Dance:        // no dance group
	case AnimFn::WatchFight:   // no arena
	case AnimFn::LookAtFlyingObject:
		return k_Stand;
	case AnimFn::LookAtLargeObject:
		return k_LookingForSomething;
	case AnimFn::InspectCreature:
		if (IsWomanOrChild(villager) && Random(3) == 0)
		{
			return k_ScaredStiff;
		}
		return Random(8) > 2 ? k_TalkingAndPointing : k_Stand;
	case AnimFn::RespectCreature:
	{
		const auto r = Random(5);
		return r == 0 ? k_CrowdImpressed1 : r <= 2 ? k_Stand : k_Pray;
	}
	case AnimFn::ControlledByCreature:
		return Random(3) == 2 ? k_WaitingImpatiently : k_Stand;
	case AnimFn::PointAtFlyingObject:
	{
		if (IsWomanOrChild(villager) && Random(3) == 0)
		{
			return k_ScaredStiff;
		}
		const std::array<int32_t, 3> clips = {k_LookingForSomething, k_Stand, k_TalkingAndPointing};
		return clips.at(static_cast<size_t>(Random(3)));
	}
	case AnimFn::FootballWaitForKickOff:
	{
		const auto r = Random(100);
		return r < 25 ? 414 : r < 50 ? 415 : r < 75 ? 416 : 417;
	}
	case AnimFn::FootballGoalKeeper:
		return 269;
	case AnimFn::FootballWatchMatch:
	{
		const std::array<int32_t, 3> clips = {k_TalkingAndPointing, 396, 272};
		return clips.at(static_cast<size_t>(Random(3)));
	}
	case AnimFn::FootballMatchPaused:
	{
		const auto r = Random(100);
		return r < 25 ? k_LookingForSomething : r < 50 ? k_Stand : r < 75 ? 412 : 413;
	}
	case AnimFn::Yawn:
		return Random(2) == 0 ? k_Yawn : k_Yawn2;
	case AnimFn::PauseForASecond: // not poisoned
		return Random(2) == 0 ? k_Overworked1 : k_Overworked2;
	case AnimFn::AmazedByShield:
	{
		const auto r = Random(5);
		return r == 0 ? k_IntoPointing : r <= 2 ? k_LookAtHand : k_Stand;
	}
	case AnimFn::TownEmergency:
	{
		const std::array<int32_t, 10> clips = {k_AttractYourAttention,
		                                       k_Beckon,
		                                       k_ConductMeeting,
		                                       IsMale(villager) ? k_PanicMan : k_PanicWoman,
		                                       k_ScaredStiff,
		                                       k_ScaredStiff2,
		                                       k_StandDespair1,
		                                       k_StandDespair1 + 1,
		                                       k_StandDespair1 + 2,
		                                       k_TalkingAndPointing};
		return clips.at(static_cast<size_t>(Random(10)));
	}
	case AnimFn::RandomCrowd:
	{
		const auto r = Random(25);
		return r == 0 ? 204 : r == 1 ? 203 : r == 2 ? k_WaitingImpatiently : r == 3 ? k_CrowdImpressed1 : r <= 5 ? k_CrowdWon : k_Stand;
	}
	case AnimFn::SitDown:
	{
		// Villager::SitDownAnimation 0x424210: only while an into / out-of clip plays (+0xE1 & 8 = flag 0x800, 0x424213)
		// the current clip decides (367 -> 369, 370 -> 372); else GameRand(2) (Animations.cpp 0x35A): 0 -> 369, 1 -> 372
		const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(entity);
		if (animation != nullptr && (animation->transitionFlags & 0x800) != 0)
		{
			const auto current = CurrentClip(entity);
			if (current == k_SittingDown1Into)
			{
				return k_SittingDown1;
			}
			if (current == k_SittingDown2Into)
			{
				return k_SittingDown2;
			}
		}
		return Random(2) == 0 ? k_SittingDown1 : k_SittingDown2;
	}
	}
	return fallback;
}

/// The into / out-of functions (slot 0x70): into = entering the state; -1 = none
int32_t TransitionAnim(TransitionFn function, entt::entity entity, bool into, VillagerStates other, VillagerStates self)
{
	const auto current = CurrentClip(entity);
	switch (function)
	{
	case TransitionFn::None:
		return -1;
	case TransitionFn::SleepInTent:
		return into ? k_IntoSleep : k_OutOfSleep;
	case TransitionFn::Mourn:
		return into ? k_IntoMourning : k_OutOfMourning;
	case TransitionFn::Pray:
		return into ? k_IntoPray : k_OutOfPray;
	case TransitionFn::SitDown:
		if (into)
		{
			return current == k_SittingDown1 ? k_SittingDown1Into : k_SittingDown2Into;
		}
		return current == k_SittingDown1 ? k_SittingDown1Out : k_SittingDown2Out;
	case TransitionFn::Building:
		if (current == k_Hammering)
		{
			return into ? k_IntoHammering : k_OutOfHammering;
		}
		if (current == k_SawWood)
		{
			return into ? k_IntoSawWood : k_OutOfSawWood;
		}
		if (current == k_Sledgehammer)
		{
			return into ? k_IntoSledgehammer : k_OutOfSledgehammer;
		}
		return -1;
	case TransitionFn::ArrivesAtResource:
		return !into && other != self ? k_PickUpSticks : -1;
	case TransitionFn::MoveToPos:
	{
		// only for a crawling villager that is not going to die
		const bool death = other == VillagerStates::SetDying || other == VillagerStates::Dying ||
		                   other == VillagerStates::Dead || other == VillagerStates::Drowning;
		if (current != k_CrawlInjured || death)
		{
			return -1;
		}
		return into ? k_CrawlInjuredInto : k_OutOfSleep;
	}
	}
	return -1;
}

SkeletalAnimation& AnimationOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(entity); animation != nullptr)
	{
		return *animation;
	}
	return registry.Assign<SkeletalAnimation>(entity);
}

/// Living::SetAnim (0x5ECBA0): switches at once (no blending) and does nothing if that clip already plays
void SetAnim(entt::entity entity, int32_t clip, bool reset)
{
	if (clip < 0)
	{
		return;
	}
	auto& animation = AnimationOf(entity);
	if (animation.locked || (animation.hasClip && animation.clipIndex == clip))
	{
		return;
	}
	animation.clip = ClipId(static_cast<uint32_t>(clip));
	animation.clipIndex = clip;
	animation.hasClip = true;
	if (reset)
	{
		animation.time = 0.0f;
	}
}

/// Living::SetStateAnim (0x5ECB10): the state's clip from the start; negative ids (not drawn) keep the old clip
void SetStateAnim(entt::entity entity)
{
	const auto clip = VillagerAnimId(entity);
	if (clip >= 0 && clip != CurrentClip(entity))
	{
		SetAnim(entity, clip, true);
	}
}

} // namespace

int32_t VillagerAnimId(entt::entity entity)
{
	// Villager::SetStateCarriedObject (0x7501A0) first
	if (auto* animation = Locator::entitiesRegistry::value().TryGet<SkeletalAnimation>(entity);
	    animation != nullptr && !animation->carriedLocked)
	{
		animation->carriedObject = CarriedObject(entity);
	}
	const auto state = TopState(entity);
	if (state == VillagerStates::InvalidState || static_cast<size_t>(state) >= 255)
	{
		return k_Stand;
	}
	const auto* villager = Locator::entitiesRegistry::value().TryGet<const Villager>(entity);
	const auto fallback = static_cast<int32_t>(StateInfo(state).field0x0);
	if (villager == nullptr)
	{
		return fallback;
	}
	return StateFunctionAnim(k_StateAnimFns.at(static_cast<size_t>(state)).anim, entity, *villager, fallback);
}

namespace
{
/// CallOutofAnimationFunction 0x756620 for a given current state
int32_t OutOfClip(entt::entity entity, VillagerStates current, VillagerStates next)
{
	if (StateInfo(next).field0xf0 != 0 || static_cast<size_t>(current) >= 255)
	{
		return -1;
	}
	const auto out = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(current)).transition, entity, false, next, current);
	if (out != -1)
	{
		// Villager +0xE1 |= 0x18 (0x756699): the flags 0x800 / 0x1000 of Villager +0xE0
		AnimationOf(entity).transitionFlags |= 0x1800;
	}
	return out;
}
} // namespace

int32_t VillagerCallOutOfAnimation(entt::entity entity, VillagerStates next)
{
	return OutOfClip(entity, TopState(entity), next);
}

void VillagerApplyStateClips(entt::entity entity, VillagerStates entered, int32_t out)
{
	const auto top = TopState(entity);
	// Living::SetTopState 0x5F291B / SetCurrentAndDestinationState 0x5F29BF: Villager::SetStateSpeed with no test (its
	// own skips, script-controlled or dancing, are inside SetVillagerStateSpeed, 0x753766 / 0x753772)
	SetVillagerStateSpeed(entity);
	if (out != -1)
	{
		SetAnim(entity, out, true);
		return;
	}
	SetStateAnim(entity);
	if (static_cast<size_t>(top) >= 255)
	{
		return;
	}
	// CallIntoAnimationFunction (0x756590): the TOP's function (Villager +0x8C, 0x756598) with (1, entered): SetTopState
	// passes its s (0x5F2947), SetCurrentAndDestinationState its destination d (0x5F29EB `push ebx`)
	const auto into = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(top)).transition, entity, true, entered, top);
	if (into != -1)
	{
		// Villager +0xE0 = (flags | 0x800) & ~0x1000 (0x7565F5)
		auto& flags = AnimationOf(entity).transitionFlags;
		flags = static_cast<uint16_t>((flags | 0x800) & ~0x1000);
		SetAnim(entity, into, true);
	}
}

void OnVillagerStateChanged(entt::entity entity, VillagerStates previous, VillagerStates next)
{
	const auto out = OutOfClip(entity, previous, next);
	VillagerApplyStateClips(entity, next, out);
}

bool VillagerWaitsForTransition(entt::entity entity, uint16_t turnsSinceStateChange)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* animation = registry.TryGet<SkeletalAnimation>(entity);
	if (animation == nullptr || (animation->transitionFlags & 0x800) == 0)
	{
		return false;
	}
	// Living::IsReadyForNewAnimation (0x5EC960): turns in the state * 100 ms >= the clip's duration (no resources, in the
	// unit tests: no duration)
	int32_t duration = 0;
	if (Locator::resources::has_value())
	{
		auto& animations = Locator::resources::value().GetAnimations();
		duration = animation->hasClip && animations.Contains(animation->clip)
		               ? animations.Handle(animation->clip)->GetDurationMs()
		               : 0;
	}
	if (static_cast<int32_t>(turnsSinceStateChange) * 100 < duration)
	{
		return true;
	}
	// Villager::FinishedIntoOutOfAnimation (0x750060), still skipping the state logic this turn
	auto* action = registry.TryGet<LivingAction>(entity);
	const auto state = TopState(entity);
	if ((animation->transitionFlags & 0x1000) != 0 && static_cast<size_t>(state) < 255 &&
	    k_StateAnimFns.at(static_cast<size_t>(state)).transition != TransitionFn::None)
	{
		// an out-of clip ended and the state has an into clip: the original calls SetAnim(GetAnimId(), into), so the
		// state's own clip restarts in "transition" mode for one more cycle and the into clip is never shown
		const auto into = TransitionAnim(k_StateAnimFns.at(static_cast<size_t>(state)).transition, entity, true, state, state);
		if (into != -1)
		{
			const auto clip = VillagerAnimId(entity);
			SetAnim(entity, clip, true);
			AnimationOf(entity).time = 0.0f;
		}
		AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x1000);
	}
	else
	{
		SetStateAnim(entity);
		AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x1800);
	}
	if (action != nullptr)
	{
		action->turnsSinceStateChange = 0;
	}
	return true;
}

void VillagerSetStateClip(entt::entity villager, bool reset)
{
	// 0x5ECB85: GetAnimId (vt +0x900); 0x5ECBA0: a negative id or the clip it has -> nothing; else the clip (LH3D +0x180)
	// and, with n and not dancing, its time from 0 (+0x188(0), 0x5ECBDB..0x5ECBF8)
	if (!Locator::entitiesRegistry::value().AllOf<SkeletalAnimation>(villager))
	{
		return;
	}
	SetAnim(villager, VillagerAnimId(villager), reset);
}

bool VillagerAnimationDone(entt::entity entity, uint16_t turnsSinceStateChange)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(entity);
	if (animation == nullptr || !animation->hasClip || !Locator::resources::has_value())
	{
		return true;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	if (!animations.Contains(animation->clip))
	{
		return true;
	}
	return static_cast<int32_t>(turnsSinceStateChange) * 100 >= animations.Handle(animation->clip)->GetDurationMs();
}

void SetVillagerState(entt::entity entity, VillagerStates state)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(entity);
	if (action == nullptr || !registry.AllOf<Villager>(entity))
	{
		return;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	// Object::EndPhysics / the hand: coords = Pos, no slide from where it was
	SnapDrawPosition(entity);
	Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, true);
}

void UpdateVillagerAnimations()
{
	auto& registry = Locator::entitiesRegistry::value();
	// villagers without a clip yet: their state's clip
	std::vector<entt::entity> fresh;
	registry.Each<const Villager>([&registry, &fresh](entt::entity entity, const Villager&) {
		if (!registry.AllOf<SkeletalAnimation>(entity))
		{
			fresh.push_back(entity);
		}
	});
	for (const auto entity : fresh)
	{
		registry.Assign<SkeletalAnimation>(entity);
		SetStateAnim(entity);
	}
	// not drawn in the states whose clip is -4 (Villager::Draw 0x51B940, the current state)
	std::vector<entt::entity> hide;
	std::vector<entt::entity> show;
	registry.Each<const Villager, const SkeletalAnimation>([&](entt::entity entity, const Villager&, const SkeletalAnimation& animation) {
		const auto state = TopState(entity);
		const bool hidden = static_cast<size_t>(state) < 255 && static_cast<int32_t>(StateInfo(state).field0x0) == k_DontDraw;
		if (hidden && animation.hiddenMesh == 0 && registry.AllOf<Mesh>(entity))
		{
			hide.push_back(entity);
		}
		else if (!hidden && animation.hiddenMesh != 0)
		{
			show.push_back(entity);
		}
	});
	for (const auto entity : hide)
	{
		registry.Get<SkeletalAnimation>(entity).hiddenMesh = registry.Get<Mesh>(entity).id;
		registry.Remove<Mesh>(entity);
	}
	for (const auto entity : show)
	{
		auto& animation = registry.Get<SkeletalAnimation>(entity);
		registry.Assign<Mesh>(entity, animation.hiddenMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		animation.hiddenMesh = 0;
	}
	// moving states (info field0x14, Villager::IsMovingForAnimation) advance the clip with the ground covered
	registry.Each<const Villager, SkeletalAnimation>([&registry](entt::entity entity, const Villager&, SkeletalAnimation& animation) {
		const auto state = TopState(entity);
		const bool movingState = static_cast<size_t>(state) < 255 && StateInfo(state).field0x14 != 0;
		const bool moving = registry.AnyOf<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag>(entity);
		const auto* wallHug = registry.TryGet<const WallHug>(entity);
		// the ground really covered: one step per game turn of 100 ms
		const float stepSpeed = wallHug != nullptr ? glm::length(wallHug->step) * 10.0f : 0.0f;
		animation.distanceSpeed = movingState && wallHug != nullptr ? (moving ? std::max(stepSpeed, 1e-6f) : 1e-6f) : 0.0f;
	});
}

} // namespace openblack::ecs
