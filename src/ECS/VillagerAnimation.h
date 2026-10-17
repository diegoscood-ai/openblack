/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// Which clip a villager plays in each of its states, and the clips that take it into and out of some states. A state's
/// clip is chosen as the villager goes into it, not every frame: the random choices are made then, on the game's shared
/// stream, and walking or running is decided by the speed it was given. Pure, tested on made-up villagers.
namespace openblack::ecs::villager_animation
{

/// Draws a whole number below a limit on the game's shared stream
using IntRandom = std::function<uint32_t(uint32_t)>;

/// A table clip that hides the villager: it is in a building or out of sight
constexpr int32_t k_HiddenClip = -4;
/// How far above the land under it a flying thing must be, in metres, for a villager to look up at it
constexpr float k_LookUpHeight = 3.0f;

/// How a state chooses its clip when its table's clip is not enough
enum class ClipChoice : uint8_t
{
	Table,
	Walk,
	Thrown,
	Landed,
	Dying,
	Dead,
	Kissing,
	Forestering,
	Yawn,
	PauseForASecond,
	LookAtLargeObject,
	LookAtFlyingObject,
	PointAtFlyingObject,
	InspectCreature,
	RespectCreature,
	ControlledByCreature,
	AmazedByShield,
	TownEmergency,
	RandomCrowd,
	SitDown,
	Building,
	WatchFight,
	FootballWaitForKickOff,
	FootballMatchPaused,
	FootballGoalKeeper,
	FootballOutfield,
	FootballWatchMatch,
	Dance,
	Script,
};

/// The clips that take a villager into a state and out of it again
enum class Transition : uint8_t
{
	None,
	SleepOnTheGround,
	Mourn,
	Pray,
	ArrivesAtResource,
	Walk,
	SitDown,
	Building,
};

[[nodiscard]] ClipChoice ClipChoiceOf(VillagerStates state);
[[nodiscard]] Transition TransitionOf(VillagerStates state);
/// The states of dying that a crawling villager does not get up into: being set to die, dying, dead, drowning and
/// downed
[[nodiscard]] constexpr bool IsDeathState(VillagerStates state)
{
	return state >= VillagerStates::SetDying && state <= VillagerStates::Downed;
}
/// The states a hand lets a held villager go into: a flight, a landing or one of the states of dying. Leaving the hand
/// for any other state is refused, and the villager stays held
[[nodiscard]] constexpr bool HandLetsGoInto(VillagerStates next)
{
	return next == VillagerStates::Flying || next == VillagerStates::Landed || IsDeathState(next);
}

/// What the walking clips depend on
struct WalkInputs
{
	CarriedObject carried {CarriedObject::None};
	float life {1.0f};
	float lifeWhenCrawlsWounded {0.15f};
	float lifeWhenWalksWounded {0.3f};
	bool female {false};
	/// Its whole speed (map units a turn) and the info's fastest walk and run in the same units
	uint16_t speed {0};
	int32_t walkMax {0};
	int32_t runMax {0};
	/// A script controls it
	bool controlledByScript {false};
};
/// The badly hurt crawl and the hurt limp; none for the rest, nor for a villager a script controls, which goes by its
/// gait however hurt it is
[[nodiscard]] std::optional<AnimId> HurtWalkClip(bool controlledByScript, float life, float lifeWhenCrawlsWounded,
                                                 float lifeWhenWalksWounded);
enum class Gait : uint8_t
{
	Walk,
	Run,
	Sprint,
};
/// Walks up to the walking limit, runs up to the running limit, both included, and sprints above them. The whole speed
/// is compared with the limits as whole numbers
[[nodiscard]] constexpr Gait GaitOf(uint16_t speed, int32_t walkMax, int32_t runMax)
{
	const auto whole = static_cast<int32_t>(speed);
	if (whole <= walkMax)
	{
		return Gait::Walk;
	}
	return whole <= runMax ? Gait::Run : Gait::Sprint;
}
/// Which speed limits a villager walks by, from its info's sex: the men's (index 1) only for the men's sex, the
/// women's (index 0) for any other, an info of no sex included
[[nodiscard]] constexpr std::size_t SpeedLimitsIndex(SexType infoSex)
{
	return infoSex == SexType::Male ? 1 : 0;
}
/// Walking, running or sprinting by the speed (GaitOf), men and women in their own way; anyone carrying something big carries
/// it walking or running. The hurt are not looked at
[[nodiscard]] AnimId GaitClip(const WalkInputs& in);
/// The hurt crawl or limp unless a script controls them, the rest go by their gait
[[nodiscard]] AnimId WalkClip(const WalkInputs& in);

/// Whether a walking villager carries its load in its arms
[[nodiscard]] bool CarriesInArms(CarriedObject carried);

/// Thrown, or thrown dead with no life left
[[nodiscard]] AnimId ThrownClip(float life);
[[nodiscard]] AnimId KissingClip(bool female);
/// Yawns one of two ways
[[nodiscard]] AnimId YawnClip(const IntRandom& random);
/// Looking at a large object (the magic tree) the state counter is split in two bytes: the turns left in the low one
/// and the turns the look started with in the high one. It looks at its hand while more than half the look is left,
/// and looks about for the rest
[[nodiscard]] AnimId LookAtLargeObjectClip(uint16_t turnsUntilStateChange);
/// Watching a flying thing: hidden when the table clip of the state it came from hides it; else it looks up at its hand
/// while the thing is more than k_LookUpHeight above the land under it, and stands when it is lower or there is nothing
/// to watch
[[nodiscard]] int32_t LookAtFlyingObjectClip(int32_t previousStateClip, std::optional<float> heightAboveLand);
/// A thing's height above the land under it, rounded to a float as the game's maths does
[[nodiscard]] float HeightAboveLand(float altitude, float land);
/// A poisoned villager looks poisoned, with no draw; the rest look worn out one of two ways
[[nodiscard]] AnimId PauseForASecondClip(bool poisoned, const IntRandom& random);
/// Women and children are scared stiff one time in three; else they talk and point or stand
[[nodiscard]] AnimId InspectCreatureClip(bool womanOrChild, const IntRandom& random);
/// Women and children are scared stiff one time in three; else they look about, stand, or talk and point
[[nodiscard]] AnimId PointAtFlyingObjectClip(bool womanOrChild, const IntRandom& random);
[[nodiscard]] AnimId RespectCreatureClip(const IntRandom& random);
[[nodiscard]] AnimId ControlledByCreatureClip(const IntRandom& random);
[[nodiscard]] AnimId AmazedByShieldClip(const IntRandom& random);
[[nodiscard]] AnimId TownEmergencyClip(bool female, const IntRandom& random);
[[nodiscard]] AnimId RandomCrowdClip(const IntRandom& random);

/// The clip a sitting villager sits in, matched to the way it sat down when that clip is playing, else one of two ways
[[nodiscard]] AnimId SitDownClip(bool transitionPlaying, AnimId current, const IntRandom& random);

/// A builder hammers, saws or swings a mallet, carrying the tool of it
struct BuildingClipChoice
{
	AnimId clip;
	CarriedObject tool;
};
/// The working clip that follows the into clip it plays, but only while a clip into or out of the state plays; else
/// one of the three at random
[[nodiscard]] BuildingClipChoice BuildingClip(bool transitionPlaying, AnimId current, const IntRandom& random);

[[nodiscard]] AnimId FootballWaitForKickOffClip(const IntRandom& random);
[[nodiscard]] AnimId FootballMatchPausedClip(const IntRandom& random);
/// The crowd at a match talks and points, talks to a neighbour or gossips
[[nodiscard]] AnimId FootballWatchMatchClip(const IntRandom& random);

/// The clip that takes a villager into a state (or out of it), if the state has one: by the state's transition and the
/// clip it plays now
[[nodiscard]] std::optional<AnimId> TransitionClip(Transition transition, bool into, bool destinationIsCurrent, bool deathState,
                                                   AnimId current);

} // namespace openblack::ecs::villager_animation
