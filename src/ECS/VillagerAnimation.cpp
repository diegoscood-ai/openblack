/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimation.h"

#include <array>

using namespace openblack;
using namespace openblack::ecs;
using villager_animation::ClipChoice;
using villager_animation::Transition;

ClipChoice villager_animation::ClipChoiceOf(VillagerStates state)
{
	using S = VillagerStates;
	switch (state)
	{
	case S::MoveToPos:
	case S::MoveToObject:
	case S::MoveOnStructure:
	case S::FleeingFromObjectReaction:
	case S::MoveAlongPath:
	case S::MoveOnPath:
	case S::MoveToWorshipSiteWithSupplies:
	case S::ForesterMoveToForest:
	case S::FootballWalkToPosition:
	case S::StartMoveToPickUpBallForDeadBall:
	case S::ShepherdMoveFlockToWater:
	case S::ShepherdMoveFlockToFood:
	case S::ShepherdMoveFlockBack:
	case S::ApproachObjectReaction:
	case S::ApproachVillagerToTalkTo:
	case S::MoveTowardsObjectToLookAt:
	case S::MoveToDancePos:
	case S::MoveTowardsCreatureReaction:
	case S::FootballMoveToBall:
	case S::FleeingFromPredatorReaction:
		return ClipChoice::Walk;
	case S::Flying:
		return ClipChoice::Thrown;
	case S::Landed:
		return ClipChoice::Landed;
	case S::Dying:
		return ClipChoice::Dying;
	case S::Dead:
		return ClipChoice::Dead;
	case S::HavingSex:
		return ClipChoice::Kissing;
	case S::ForesterChopsTreeForBuilding:
	case S::ArrivesAtRockForWood:
	case S::GotWoodFromRock:
	case S::TakeWoodFromTree:
	case S::TakeWoodFromPot:
	case S::TakeWoodFromTreeForBuilding:
	case S::TakeWoodFromPotForBuilding:
		return ClipChoice::Forestering;
	case S::AfterTapOnAbode:
		return ClipChoice::Yawn;
	case S::PauseForASecond:
		return ClipChoice::PauseForASecond;
	case S::LookAtMagicTree:
		return ClipChoice::LookAtLargeObject;
	case S::LookAtFlyingObjectReaction:
	case S::WatchFlyingObjectReaction:
		return ClipChoice::LookAtFlyingObject;
	case S::PointAtFlyingObjectReaction:
		return ClipChoice::PointAtFlyingObject;
	case S::InspectCreatureReaction:
	case S::PerformInspectCreatureReaction:
	case S::ApproachCreatureReaction:
	case S::TurnToFaceCreatureReaction:
		return ClipChoice::InspectCreature;
	case S::InitialiseRespectCreatureReaction:
	case S::PerformRespectCreatureReaction:
	case S::FinishRespectCreatureReaction:
		return ClipChoice::RespectCreature;
	case S::ControlledByCreature:
		return ClipChoice::ControlledByCreature;
	case S::AmazedByMagicShieldReaction:
		return ClipChoice::AmazedByShield;
	case S::CongregateInTownAfterEmergency:
		return ClipChoice::TownEmergency;
	case S::ScriptInCrowd:
		return ClipChoice::RandomCrowd;
	case S::SitAndChillout:
		return ClipChoice::SitDown;
	case S::Building:
		return ClipChoice::Building;
	case S::PerformFightReaction:
		return ClipChoice::WatchFight;
	case S::FootballWaitForKickOff:
		return ClipChoice::FootballWaitForKickOff;
	case S::FootballMatchPaused:
		return ClipChoice::FootballMatchPaused;
	case S::FootballGoalie:
		return ClipChoice::FootballGoalKeeper;
	case S::FootballAttacker:
	case S::FootballDefender:
		return ClipChoice::FootballOutfield;
	case S::FootballWatchMatch:
		return ClipChoice::FootballWatchMatch;
	case S::InDance:
	case S::WorshippingAtWorshipSite:
	case S::RestartWorshippingAtWorshipSite:
	case S::RestartWorshippingCreature:
	case S::WorshippingCreature:
	case S::DanceForEditingPurposes:
	case S::DanceButNotWorship:
	case S::DanceWhileReacting:
	case S::ArtifactDance:
	case S::WaitForArtifactDance:
		return ClipChoice::Dance;
	case S::ScriptPlayAnim:
		return ClipChoice::Script;
	default:
		return ClipChoice::Table;
	}
}

Transition villager_animation::TransitionOf(VillagerStates state)
{
	using S = VillagerStates;
	switch (state)
	{
	case S::SleepInTent:
	case S::SleepInTentFromWorship:
		return Transition::SleepOnTheGround;
	case S::MornDeath:
	case S::MournDeadPerson:
		return Transition::Mourn;
	case S::AtAltarRest:
	case S::PerformImpressedReaction:
	case S::DiscipleNothingToDo:
		return Transition::Pray;
	case S::ArrivesAtFoodReaction:
	case S::ArrivesAtWoodReaction:
		return Transition::ArrivesAtResource;
	case S::SitAndChillout:
		return Transition::SitDown;
	case S::Building:
		return Transition::Building;
	default:
		return ClipChoiceOf(state) == ClipChoice::Walk ? Transition::Walk : Transition::None;
	}
}

bool villager_animation::CarriesInArms(CarriedObject carried)
{
	// not nothing, nor one of the tools held in a hand: the saw, the ball and the hammer
	return carried != CarriedObject::None && carried != CarriedObject::Saw && carried != CarriedObject::Ball &&
	       carried != CarriedObject::Hammer && static_cast<int32_t>(carried) > static_cast<int32_t>(CarriedObject::NoChange);
}

std::optional<AnimId> villager_animation::HurtWalkClip(bool controlledByScript, float life, float lifeWhenCrawlsWounded,
                                                       float lifeWhenWalksWounded)
{
	// a villager a script controls is not looked at for its wounds
	if (controlledByScript)
	{
		return std::nullopt;
	}
	if (life <= lifeWhenCrawlsWounded)
	{
		return AnimId::PCrawlInjured;
	}
	if (life <= lifeWhenWalksWounded)
	{
		return AnimId::PWalkInjured;
	}
	return std::nullopt;
}

AnimId villager_animation::GaitClip(const WalkInputs& in)
{
	const bool carrying = CarriesInArms(in.carried);
	switch (GaitOf(in.speed, in.walkMax, in.runMax))
	{
	case Gait::Walk:
		return carrying ? AnimId::PCarryAxe : in.female ? AnimId::PWalkWoman : AnimId::PWalkMan;
	case Gait::Run:
		return carrying ? AnimId::PCarryObjectRun : in.female ? AnimId::PRunWoman : AnimId::PRunMan;
	case Gait::Sprint:
		break;
	}
	return carrying ? AnimId::PCarryObjectRun : in.female ? AnimId::PSprintRunWoman : AnimId::PSprintRunMan;
}

AnimId villager_animation::WalkClip(const WalkInputs& in)
{
	if (const auto hurt = HurtWalkClip(in.controlledByScript, in.life, in.lifeWhenCrawlsWounded, in.lifeWhenWalksWounded);
	    hurt.has_value())
	{
		return *hurt;
	}
	return GaitClip(in);
}

AnimId villager_animation::ThrownClip(float life)
{
	return life <= 0.0f ? AnimId::PThrownDead : AnimId::PThrown;
}

AnimId villager_animation::KissingClip(bool female)
{
	return female ? AnimId::PCoupleKissWoman : AnimId::PCoupleKissMan;
}

AnimId villager_animation::YawnClip(const IntRandom& random)
{
	return static_cast<int32_t>(random(2)) == 0 ? AnimId::PYawn : AnimId::PYawn2;
}

AnimId villager_animation::LookAtLargeObjectClip(uint16_t turnsUntilStateChange)
{
	const auto turnsLeft = static_cast<uint8_t>(turnsUntilStateChange & 0xFFU);
	const auto turnsAtStart = static_cast<uint8_t>(turnsUntilStateChange >> 8U);
	return (turnsAtStart >> 1U) < turnsLeft ? AnimId::PLookAtHand : AnimId::PLookingForSomething;
}

int32_t villager_animation::LookAtFlyingObjectClip(int32_t previousStateClip, std::optional<float> heightAboveLand)
{
	if (previousStateClip == k_HiddenClip)
	{
		return k_HiddenClip;
	}
	// a height that is not a number is not above it
	const bool high = heightAboveLand.has_value() && *heightAboveLand > k_LookUpHeight;
	return static_cast<int32_t>(high ? AnimId::PLookAtHand : AnimId::PStand);
}

float villager_animation::HeightAboveLand(float altitude, float land)
{
	return altitude - land;
}

AnimId villager_animation::PauseForASecondClip(bool poisoned, const IntRandom& random)
{
	if (poisoned)
	{
		return AnimId::PPoisoned;
	}
	return static_cast<int32_t>(random(2)) == 0 ? AnimId::POverworked1 : AnimId::POverworked2;
}

AnimId villager_animation::InspectCreatureClip(bool womanOrChild, const IntRandom& random)
{
	if (womanOrChild && static_cast<int32_t>(random(3)) == 0)
	{
		return AnimId::PScaredStiff;
	}
	return static_cast<int32_t>(random(8)) > 2 ? AnimId::PTalkingAndPointing : AnimId::PStand;
}

AnimId villager_animation::PointAtFlyingObjectClip(bool womanOrChild, const IntRandom& random)
{
	if (womanOrChild && static_cast<int32_t>(random(3)) == 0)
	{
		return AnimId::PScaredStiff;
	}
	constexpr std::array<AnimId, 3> k_Clips = {AnimId::PLookingForSomething, AnimId::PStand, AnimId::PTalkingAndPointing};
	return k_Clips.at(static_cast<size_t>(random(3)));
}

AnimId villager_animation::RespectCreatureClip(const IntRandom& random)
{
	const auto roll = static_cast<int32_t>(random(5));
	return roll == 0 ? AnimId::PCrowdImpressed_1 : roll <= 2 ? AnimId::PStand : AnimId::PPray;
}

AnimId villager_animation::ControlledByCreatureClip(const IntRandom& random)
{
	return static_cast<int32_t>(random(3)) == 2 ? AnimId::PWaitingImpateintly : AnimId::PStand;
}

AnimId villager_animation::AmazedByShieldClip(const IntRandom& random)
{
	const auto roll = static_cast<int32_t>(random(5));
	return roll == 0 ? AnimId::PIntoPointing : roll <= 2 ? AnimId::PLookAtHand : AnimId::PStand;
}

AnimId villager_animation::TownEmergencyClip(bool female, const IntRandom& random)
{
	const std::array<AnimId, 10> clips = {AnimId::PAttractYourAttention, AnimId::PBeckon,
	                                      AnimId::PConductMeeting,       female ? AnimId::PPanicWoman : AnimId::PPanicMan,
	                                      AnimId::PScaredStiff,          AnimId::PScaredStiff_2,
	                                      AnimId::PStandDespair_1,       AnimId::PStandDespair_2,
	                                      AnimId::PStandDespair_3,       AnimId::PTalkingAndPointing};
	return clips.at(static_cast<size_t>(random(10)));
}

AnimId villager_animation::RandomCrowdClip(const IntRandom& random)
{
	const auto roll = static_cast<int32_t>(random(25));
	return roll == 0   ? AnimId::PAmbient2
	       : roll == 1 ? AnimId::PAmbient1
	       : roll == 2 ? AnimId::PWaitingImpateintly
	       : roll == 3 ? AnimId::PCrowdImpressed_1
	       : roll <= 5 ? AnimId::PCrowdWon
	                   : AnimId::PStand;
}

AnimId villager_animation::SitDownClip(bool transitionPlaying, AnimId current, const IntRandom& random)
{
	// only while a clip into or out of the state plays does the current clip decide
	if (transitionPlaying)
	{
		if (current == AnimId::PSittingDown1Into)
		{
			return AnimId::PSittingDown1Sitting;
		}
		if (current == AnimId::PSittingDown2Into)
		{
			return AnimId::PSittingDown2Sitting;
		}
	}
	return static_cast<int32_t>(random(2)) == 0 ? AnimId::PSittingDown1Sitting : AnimId::PSittingDown2Sitting;
}

villager_animation::BuildingClipChoice villager_animation::BuildingClip(bool transitionPlaying, AnimId current,
                                                                        const IntRandom& random)
{
	// only while a clip into or out of the state plays does the current clip decide
	auto clip = AnimId::PHammering;
	if (transitionPlaying && current == AnimId::PIntoHammering)
	{
		clip = AnimId::PHammering;
	}
	else if (transitionPlaying && current == AnimId::PIntoSawWood)
	{
		clip = AnimId::PSawWood;
	}
	else if (transitionPlaying && current == AnimId::PIntoSledgehammer)
	{
		clip = AnimId::PSledgehammer;
	}
	else
	{
		constexpr std::array<AnimId, 3> k_Clips = {AnimId::PHammering, AnimId::PSawWood, AnimId::PSledgehammer};
		clip = k_Clips.at(static_cast<size_t>(random(3)));
	}
	const auto tool = clip == AnimId::PHammering ? CarriedObject::Hammer
	                  : clip == AnimId::PSawWood ? CarriedObject::Saw
	                                             : CarriedObject::MalletHeavy;
	return {.clip = clip, .tool = tool};
}

AnimId villager_animation::FootballWaitForKickOffClip(const IntRandom& random)
{
	const auto roll = static_cast<int32_t>(random(100));
	return roll < 25   ? AnimId::PWaitingForKickOff
	       : roll < 50 ? AnimId::PWaitingForKickOff_2
	       : roll < 75 ? AnimId::PWaitingForKickOff_3
	                   : AnimId::PWaitingForKickOff_4;
}

AnimId villager_animation::FootballMatchPausedClip(const IntRandom& random)
{
	const auto roll = static_cast<int32_t>(random(100));
	return roll < 25   ? AnimId::PLookingForSomething
	       : roll < 50 ? AnimId::PStand
	       : roll < 75 ? AnimId::PWaitingForBall
	                   : AnimId::PWaitingForBall_2;
}

AnimId villager_animation::FootballWatchMatchClip(const IntRandom& random)
{
	constexpr std::array<AnimId, 3> k_Clips = {AnimId::PTalkingAndPointing, AnimId::PTalkToNeighbour, AnimId::PGossipMan};
	return k_Clips.at(static_cast<size_t>(random(3)));
}

std::optional<AnimId> villager_animation::TransitionClip(Transition transition, bool into, bool destinationIsCurrent,
                                                         bool deathState, AnimId current)
{
	switch (transition)
	{
	case Transition::None:
		return std::nullopt;
	case Transition::SleepOnTheGround:
		return into ? AnimId::PIntoSleep : AnimId::POutOfSleep;
	case Transition::Mourn:
		return into ? AnimId::PIntoMourning : AnimId::POutOfMourning;
	case Transition::Pray:
		return into ? AnimId::PIntoPray : AnimId::POutOfPray;
	case Transition::SitDown:
		if (into)
		{
			return current == AnimId::PSittingDown1Sitting ? AnimId::PSittingDown1Into : AnimId::PSittingDown2Into;
		}
		return current == AnimId::PSittingDown1Sitting ? AnimId::PSittingDown1OutOf : AnimId::PSittingDown2OutOf;
	case Transition::Building:
		if (current == AnimId::PHammering)
		{
			return into ? AnimId::PIntoHammering : AnimId::POutOfHammering;
		}
		if (current == AnimId::PSawWood)
		{
			return into ? AnimId::PIntoSawWood : AnimId::POutOfSawWood;
		}
		if (current == AnimId::PSledgehammer)
		{
			return into ? AnimId::PIntoSledgehammer : AnimId::POutOfSledgehammer;
		}
		return std::nullopt;
	case Transition::ArrivesAtResource:
		if (!into && !destinationIsCurrent)
		{
			return AnimId::PPickUpSticks;
		}
		return std::nullopt;
	case Transition::Walk:
		// only a crawling villager that is not going to die gets up from its crawl, or goes down into one
		if (current != AnimId::PCrawlInjured || deathState)
		{
			return std::nullopt;
		}
		return into ? AnimId::PCrawlInjuredInto : AnimId::POutOfSleep;
	}
	return std::nullopt;
}
