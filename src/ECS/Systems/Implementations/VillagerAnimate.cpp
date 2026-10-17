/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimate.h"

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Animations.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Life.h"
#include "ECS/LivingFirstClip.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SeaCells.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerDance.h"
#include "VillagerReactions.h"
#include "VillagerScript.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace animation = openblack::ecs::villager_animation;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// Switches its clip at once (no blending): nothing for a negative clip, one it already plays, or a villager without a
/// clip yet; from the start when asked to
void SwitchClip(entt::entity villager, int32_t clip, bool restart)
{
	auto* skeletal = Entities().TryGet<SkeletalAnimation>(villager);
	if (skeletal == nullptr || clip < 0 || skeletal->locked || (skeletal->hasClip && skeletal->clipIndex == clip))
	{
		return;
	}
	skeletal->clip = ClipId(static_cast<uint32_t>(clip));
	skeletal->clipIndex = clip;
	skeletal->hasClip = true;
	if (restart)
	{
		skeletal->time = 0.0f;
	}
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates TopState(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	return action != nullptr ? static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)])
	                         : VillagerStates::InvalidState;
}

/// The table clip of the state the villager came from; a state outside the table (none yet) does not hide it
int32_t PreviousStateClip(entt::entity villager)
{
	constexpr auto k_NotHidden = static_cast<int32_t>(AnimId::PStand);
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	if (action == nullptr)
	{
		return k_NotHidden;
	}
	const auto previous = static_cast<size_t>(action->states[static_cast<size_t>(LivingAction::Index::Previous)]);
	const auto& table = Locator::infoConstants::value().villagerStateTable;
	return previous < table.size() ? static_cast<int32_t>(table.at(previous).animation) : k_NotHidden;
}

/// How high the thing the villager reacts to is above the land under its map point, in float; none without one
std::optional<float> ReactionObjectHeightAboveLand(entt::entity villager)
{
	const auto object = villager_reactions::SlotObject(villager);
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return std::nullopt;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return std::nullopt;
	}
	const auto coords = map_coords::FromWorld(transform->position);
	const float land =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(map_coords::ToMetres(coords)) : 0.0f;
	return animation::HeightAboveLand(transform->position.y, land);
}

/// The game's shared stream, for the clip rules
animation::IntRandom GameRandom()
{
	return [](uint32_t limit) { return game_random::GameRand(limit); };
}

int32_t Clip(AnimId clip)
{
	return static_cast<int32_t>(clip);
}

bool IsMale(const Villager& villager)
{
	return villager.sex == Villager::Sex::MALE;
}

bool IsWomanOrChild(const Villager& villager)
{
	return !IsMale(villager) || villager.lifeStage == Villager::LifeStage::Child;
}

/// Whether the villager's position is in the water
bool IsInWater(entt::entity villager)
{
	const auto* transform = Entities().TryGet<const Transform>(villager);
	return transform != nullptr && sea_cells::IsWater(transform->position);
}

/// The landType of the last landing (set at the end of its physics; dying ors in 3)
uint8_t LandType(const Villager& villager)
{
	return static_cast<uint8_t>((villager.status & Villager::k_StatusLandTypeMask) >> Villager::k_LandTypeShift);
}

/// The carried object: the load (wood log / bag), the life, the builders' final states and what
/// the state rows force (ecs::villager::SetStateCarriedObject), from the current one (kept by the script states)
int32_t CarriedNow(entt::entity villager)
{
	const auto* animation = Entities().TryGet<SkeletalAnimation>(villager);
	const int32_t previous = animation != nullptr ? animation->carriedObject : static_cast<int32_t>(CarriedObject::None);
	return villager::SetStateCarriedObject(villager, previous);
}

/// The move clip: wounded (unless a script controls it), else walk, run or sprint by the speed, and the carry clips.
/// What it carries is only worked out for one that is not hurt
AnimId WalkClipOf(entt::entity entity, const Villager& villager)
{
	const auto& info = Locator::infoConstants::value();
	const auto& villagerInfo = info.villager.at(0);
	if (const auto hurt = animation::HurtWalkClip(script_held::IsControlledByScript(entity), villager.life,
	                                              villagerInfo.lifeWhenCrawlsWounded, villagerInfo.lifeWhenWalksWounded);
	    hurt.has_value())
	{
		return *hurt;
	}
	const auto carried = static_cast<CarriedObject>(CarriedNow(entity));
	// SPEED_THRESHOLD_VILLAGER_MAN / _NORMAL, and the men's or the women's clips, both by the villager info's own sex
	const auto limits = animation::SpeedLimitsIndex(openblack::ecs::villager::InfoOf(entity).sex);
	const auto& threshold = info.speedThreshold.at(limits);
	const auto* wallHug = Entities().TryGet<const WallHug>(entity);
	return animation::GaitClip({
	    .carried = carried,
	    .life = villager.life,
	    .lifeWhenCrawlsWounded = villagerInfo.lifeWhenCrawlsWounded,
	    .lifeWhenWalksWounded = villagerInfo.lifeWhenWalksWounded,
	    .female = limits == 0,
	    // the whole speed against the info's limits, all in map units a turn
	    .speed = wallHug != nullptr ? WholeSpeed(*wallHug) : uint16_t {0},
	    .walkMax = static_cast<int32_t>(threshold.speedMaxWalk),
	    .runMax = static_cast<int32_t>(threshold.speedMaxRun),
	});
}

/// The state's clip choice. The choices that need what openblack doesn't have yet (vortices, dance groups, fights,
/// football, creatures) take the branch of the original for its absence.
int32_t ChosenClip(animation::ClipChoice choice, entt::entity entity, const Villager& villager, int32_t fallback)
{
	using animation::ClipChoice;
	const auto random = GameRandom();
	switch (choice)
	{
	case ClipChoice::Table:
		return fallback;
	case ClipChoice::Walk:
		return Clip(WalkClipOf(entity, villager));
	case ClipChoice::FootballOutfield: // STAND; the move clip comes from the distance sync
		return Clip(AnimId::PStand);
	case ClipChoice::Landed: // by the landType of the last landing
		return VillagerLandedClip(LandType(villager), CarriedNow(entity) == static_cast<int32_t>(CarriedObject::None));
	case ClipChoice::Dying: // in the water P_INTO_DEAD_DROWNED; landType 2 P_DEAD2
		return villager::DyingClip(IsInWater(entity), LandType(villager));
	case ClipChoice::Dead: // in the water P_DEAD_DROWNED; landType 2 P_DEAD2; else P_DEAD1
		return villager::DeadClip(IsInWater(entity), LandType(villager));
	case ClipChoice::Thrown: // not in a vortex
		return Clip(animation::ThrownClip(villager.life));
	case ClipChoice::Kissing:
		return Clip(animation::KissingClip(!IsMale(villager)));
	case ClipChoice::Forestering:
		return Clip(AnimId::PChoppingTree);
	case ClipChoice::Building:
	{
		// the working clip after its into clip while that plays (flag 0x800), else one of the three at random; it sets
		// the carried object: hammer, saw or heavy mallet
		auto* skeletal = Entities().TryGet<SkeletalAnimation>(entity);
		const bool transitionPlaying = skeletal != nullptr && (skeletal->transitionFlags & 0x800) != 0;
		const auto building =
		    animation::BuildingClip(transitionPlaying, static_cast<AnimId>(villager_animate::CurrentClip(entity)), random);
		if (skeletal != nullptr && !skeletal->carriedLocked)
		{
			skeletal->carriedObject = static_cast<int32_t>(building.tool);
		}
		return Clip(building.clip);
	}
	case ClipChoice::Script: // the clip SET_SCRIPT_ULONG gave
		return villager_script::ScriptClip(entity);
	case ClipChoice::Dance: // by its group's move in its dance (the stand in no group)
		return villager_dance::DanceClip(entity, Clip(WalkClipOf(entity, villager)), random);
	case ClipChoice::WatchFight: // no arena
		return Clip(AnimId::PStand);
	case ClipChoice::LookAtFlyingObject: // by the height of the thing it reacts to
		return animation::LookAtFlyingObjectClip(PreviousStateClip(entity), ReactionObjectHeightAboveLand(entity));
	case ClipChoice::LookAtLargeObject:
	{
		const auto* action = Entities().TryGet<const LivingAction>(entity);
		return Clip(animation::LookAtLargeObjectClip(action != nullptr ? action->turnsUntilStateChange : 0));
	}
	case ClipChoice::InspectCreature:
		return Clip(animation::InspectCreatureClip(IsWomanOrChild(villager), random));
	case ClipChoice::RespectCreature:
		return Clip(animation::RespectCreatureClip(random));
	case ClipChoice::ControlledByCreature:
		return Clip(animation::ControlledByCreatureClip(random));
	case ClipChoice::PointAtFlyingObject:
		return Clip(animation::PointAtFlyingObjectClip(IsWomanOrChild(villager), random));
	case ClipChoice::FootballWaitForKickOff:
		return Clip(animation::FootballWaitForKickOffClip(random));
	case ClipChoice::FootballGoalKeeper:
		return Clip(AnimId::PGoalkeeper);
	case ClipChoice::FootballWatchMatch:
		return Clip(animation::FootballWatchMatchClip(random));
	case ClipChoice::FootballMatchPaused:
		return Clip(animation::FootballMatchPausedClip(random));
	case ClipChoice::Yawn:
		return Clip(animation::YawnClip(random));
	case ClipChoice::PauseForASecond:
		return Clip(animation::PauseForASecondClip(life::IsPoisoned(entity), random));
	case ClipChoice::AmazedByShield:
		return Clip(animation::AmazedByShieldClip(random));
	case ClipChoice::TownEmergency:
		return Clip(animation::TownEmergencyClip(!IsMale(villager), random));
	case ClipChoice::RandomCrowd:
		return Clip(animation::RandomCrowdClip(random));
	case ClipChoice::SitDown:
	{
		// only while an into / out-of clip plays (flag 0x800) the current clip decides (367 -> 369, 370 -> 372);
		// else GameRand(2): 0 -> 369, 1 -> 372
		const auto* skeletal = Entities().TryGet<const SkeletalAnimation>(entity);
		const bool transitionPlaying = skeletal != nullptr && (skeletal->transitionFlags & 0x800) != 0;
		return Clip(
		    animation::SitDownClip(transitionPlaying, static_cast<AnimId>(villager_animate::CurrentClip(entity)), random));
	}
	}
	return fallback;
}
} // namespace

int32_t villager_animate::StateClip(entt::entity entity)
{
	// the carried object first
	if (auto* skeletal = Entities().TryGet<SkeletalAnimation>(entity); skeletal != nullptr && !skeletal->carriedLocked)
	{
		skeletal->carriedObject = CarriedNow(entity);
	}
	const auto state = TopState(entity);
	if (state == VillagerStates::InvalidState || static_cast<size_t>(state) >= 255)
	{
		return Clip(AnimId::PStand);
	}
	const auto* villager = Entities().TryGet<const Villager>(entity);
	const auto fallback = static_cast<int32_t>(StateInfo(state).animation);
	if (villager == nullptr)
	{
		return fallback;
	}
	return ChosenClip(animation::ClipChoiceOf(state), entity, *villager, fallback);
}

void villager_animate::SetAnim(entt::entity villager, int32_t clip, bool restart)
{
	// A dancer's new clip takes over in time with the dance
	const bool sameClip = CurrentClip(villager) == clip;
	const bool dancing = dances::DanceOf(Entities(), villager) != entt::null;
	SwitchClip(villager, clip,
	           villager_clips::PlaceOnSetClip(sameClip, restart, dancing) == villager_clips::ClipPlace::Restart);
}

void villager_animate::PlayStateClip(entt::entity villager, bool restart)
{
	if (!Entities().AllOf<SkeletalAnimation>(villager))
	{
		return;
	}
	SetAnim(villager, StateClip(villager), restart);
}

void villager_animate::SetStateAnim(entt::entity villager)
{
	const auto clip = StateClip(villager);
	if (clip < 0 || clip == CurrentClip(villager))
	{
		return;
	}
	if (!Entities().AllOf<SkeletalAnimation>(villager))
	{
		Entities().Assign<SkeletalAnimation>(villager);
	}
	SwitchClip(villager, clip, true);
}

void villager_animate::SetFirstClip(entt::entity villager)
{
	if (!Entities().AllOf<SkeletalAnimation>(villager))
	{
		Entities().Assign<SkeletalAnimation>(villager);
	}
	// the born state's clip (what it carries is worked out first), put on as any other clip change from the start
	if (const auto first = living::VillagerFirstClip(StateClip(villager)); first.has_value())
	{
		SetAnim(villager, first->clip, first->fromStart);
	}
}

void villager_animate::OverrideAnim(entt::entity villager, int32_t clip)
{
	SwitchClip(villager, clip, true);
}

int32_t villager_animate::CurrentClip(entt::entity villager)
{
	const auto* skeletal = Entities().TryGet<const SkeletalAnimation>(villager);
	return skeletal != nullptr && skeletal->hasClip ? skeletal->clipIndex : -1;
}

bool villager_animate::IsHiddenByState(VillagerStates state)
{
	return static_cast<size_t>(state) < 255 && static_cast<int32_t>(StateInfo(state).animation) == animation::k_HiddenClip;
}
