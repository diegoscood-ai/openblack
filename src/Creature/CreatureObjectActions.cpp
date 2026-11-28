/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectActions.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "3D/ObjectMatrix.h"
#include "Common/X87Precision.h"
#include "Creature/CreatureLean.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureThrow.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_object_actions;

bool creature_object_actions::LeavesThingAlone(Kind kind, Failure failure)
{
	return (kind == Kind::PickUp || kind == Kind::Destroy) && failure == Failure::NoWay;
}

creature_object_actions::ApproachStep creature_object_actions::NextApproachStep(bool inBlend, bool nearerThanItReaches)
{
	if (inBlend)
	{
		return ApproachStep::Begin;
	}
	return nearerThanItReaches ? ApproachStep::TurnThenBegin : ApproachStep::Walk;
}

float creature_object_actions::WalkUpToWithin(float furthestReach)
{
	constexpr float k_Least = 0.01f;
	return furthestReach <= 0.0f ? k_Least : furthestReach;
}

bool creature_object_actions::TooHighToPickUp(float rise, float distance)
{
	return distance < rise && rise > k_MostPickUpRise;
}

bool creature_object_actions::HurriedByForcedFinish(Kind kind)
{
	switch (kind)
	{
	case Kind::Keep:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Eat:
	case Kind::PickUp:
	case Kind::Destroy:
	case Kind::Point:
		return true;
	case Kind::Throw:
	case Kind::TakeFromHand:
	case Kind::Kick:
	case Kind::Kiss:
		break;
	}
	return false;
}

bool creature_object_actions::PlaysAtRealSpeed(Kind kind)
{
	return kind == Kind::Kiss;
}

std::string_view creature_object_actions::Name(Kind kind)
{
	switch (kind)
	{
	case Kind::PickUp:
		return "Picking up";
	case Kind::PutDown:
		return "Putting down";
	case Kind::Discard:
		return "Tossing away";
	case Kind::Lob:
		return "Lobbing";
	case Kind::Eat:
		return "Eating what it holds";
	case Kind::Keep:
		return "Looking over what it holds";
	case Kind::Throw:
		return "Throwing";
	case Kind::Destroy:
		return "Knocking down";
	case Kind::Point:
		return "Pointing";
	case Kind::TakeFromHand:
		return "Taking from the hand";
	case Kind::Kick:
		return "Kicking";
	case Kind::Kiss:
		return "Kissing";
	}
	return "Nothing";
}

bool creature_object_actions::ShowsHoldPose(std::optional<Kind> playing)
{
	if (!playing.has_value())
	{
		return true;
	}
	switch (*playing)
	{
	case Kind::PickUp:
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Eat:
	case Kind::Keep:
	case Kind::Throw:
	case Kind::TakeFromHand:
		return false;
	case Kind::Destroy:
	case Kind::Point:
	case Kind::Kick:
	case Kind::Kiss:
		return true;
	}
	return true;
}

bool creature_object_actions::DiscardPutsDown(size_t animation)
{
	return animation == creature_throw::k_PutDown;
}

Hands creature_object_actions::HandsFor(Kind kind)
{
	switch (kind)
	{
	case Kind::PutDown:
	case Kind::Discard:
	case Kind::Lob:
	case Kind::Eat:
	case Kind::Keep:
	case Kind::Throw:
		return Hands::Holding;
	case Kind::PickUp:
	case Kind::Destroy:
		return Hands::Empty;
	case Kind::Point:
	case Kind::Kick:
	case Kind::Kiss:
	// The take from the hand starts with something held too: it then takes nothing, and is done
	case Kind::TakeFromHand:
		return Hands::Either;
	}
	return Hands::Either;
}

glm::vec2 creature_object_actions::LeanOf(float plusX, float minusX, float plusZ, float minusZ, float radius)
{
	// The lean the body's placement takes, so a body of no radius on flat land leans the lowest, as there
	const auto lean =
	    creature_lean::FromHeights({.xAhead = plusX, .xBehind = minusX, .zAhead = plusZ, .zBehind = minusZ}, radius);
	return {lean.x, lean.z};
}

glm::vec3 creature_object_actions::KickDirection(glm::vec2 forward, glm::vec2 lean)
{
	// Straight up plus straight ahead, the up part changed by how the creature leans the way it faces
	const glm::vec3 direction {forward.x, 1.0f + glm::dot(forward, lean), forward.y};
	const auto length = glm::length(direction);
	return length > 0.0f ? direction / length : direction;
}

glm::vec3 creature_object_actions::KickVelocity(glm::vec2 forward, glm::vec2 lean, float size)
{
	return KickDirection(forward, lean) * (k_KickSpeedPerSize * size);
}

float creature_object_actions::KickCrushShare(float kickerHeight, float kickedHeight)
{
	constexpr float k_Share = 0.1f;
	float ratio = kickerHeight / kickedHeight;
	// below nothing, or no number, it is nothing; above one it is one, as the game compares
	if (!(ratio >= 0.0f))
	{
		ratio = 0.0f;
	}
	else if (ratio > 1.0f)
	{
		ratio = 1.0f;
	}
	return (ratio + 1.0f) * k_Share;
}

bool creature_object_actions::KnockDownCrushes(Kind kind, Status status, bool living, bool inMap)
{
	return kind == Kind::Destroy && status == Status::Contact && living && inMap;
}

bool creature_object_actions::KnockDownKills(Kind kind, Status status, bool inMap, bool lastStep, float life)
{
	return kind == Kind::Destroy && status == Status::Contact && inMap && lastStep && !(life > 0.0f);
}

std::optional<PlayerNames> creature_object_actions::ThrowerPlayerOf(PlayerNames creaturePlayer)
{
	return creaturePlayer != PlayerNames::NEUTRAL ? std::optional(creaturePlayer) : std::nullopt;
}

bool creature_object_actions::CanBeKicked(const KickFacts& facts)
{
	if (facts.leashTiedTo || facts.bowlingBall)
	{
		return false;
	}
	if (facts.creature)
	{
		return true;
	}
	if (!facts.canBecomePhysicsObject || facts.immovable)
	{
		return false;
	}
	// Twice the creature's weight, which no weight that is no number is below
	if (!(facts.thingWeight < facts.creatureWeight + facts.creatureWeight))
	{
		return false;
	}
	return !facts.indestructible && !facts.carriedByIt && !facts.inHand;
}

bool creature_object_actions::CanHomeBeKicked(bool onFire, bool built)
{
	return !onFire && built;
}

namespace
{
/// The way a direction faces on the ground as a body's yaw (0 along -z, a quarter turn along +x), the way the
/// pointing's heading is given: the original's heading of a direction, 0 when x x + z z is no more than 1e-6
float YawOf(glm::vec3 direction)
{
	return static_cast<float>(affine::GetYAngle(glm::vec3(direction.x, 0.0f, direction.z)));
}
} // namespace

creature_object_actions::PointAim creature_object_actions::AimOfPoint(glm::vec3 offset, float heading)
{
	const float distance = std::sqrt(((offset.z * offset.z) + (offset.x * offset.x)) + (offset.y * offset.y));
	if (distance < k_PointNearest)
	{
		return {};
	}
	const float across = affine::WrapAngle(heading - YawOf(offset));
	const float rise = offset.y / distance;
	// In this order, so that what is not a number goes to the lower limit
	return {
	    .across =
	        across > -k_PointAcrossLimit ? (across < k_PointAcrossLimit ? across : k_PointAcrossLimit) : -k_PointAcrossLimit,
	    .rise = rise > -k_PointRiseLimit ? (rise < k_PointRiseLimit ? rise : k_PointRiseLimit) : -k_PointRiseLimit,
	};
}

std::array<float, 4> creature_object_actions::PointWeights(PointAim aim, bool mirrored)
{
	const float across = mirrored ? -aim.across : aim.across;
	const float right = (across + k_PointAcrossLimit) * k_PointAcrossScale;
	const float high = aim.rise + 0.5f;
	const float left = 1.0f - right;
	const float low = 1.0f - high;
	return {left * low, low * right, left * high, high * right};
}

bool creature_object_actions::PointsMirroredFacing(glm::vec3 fromHead, float heading)
{
	return PointTurnAngle(fromHead, heading) > 0.0f;
}

float creature_object_actions::PointTurnAngle(glm::vec3 fromHead, float heading)
{
	return affine::WrapAngle(YawOf(fromHead) - heading);
}

bool creature_object_actions::TurnsToPoint(float turnAngle)
{
	// Written so that what is not a number turns
	return !(std::abs(turnAngle) < k_PointTurnRadians);
}

bool creature_object_actions::PointEnds(float timeMs, float stepMs, float highLeftMs)
{
	return timeMs + stepMs >= highLeftMs;
}

float creature_object_actions::PointClipTime(float timeMs, float stepMs, float pointMs, float durationMs)
{
	if (!(timeMs < pointMs) || stepMs == 0.0f)
	{
		return timeMs;
	}
	float time = timeMs + stepMs;
	// an animation not loaded has no end to go round
	if (durationMs <= 0.0f)
	{
		return time;
	}
	if (time >= durationMs)
	{
		time -= durationMs;
	}
	return time >= durationMs ? durationMs - 1.0f : time;
}

glm::vec3 creature_object_actions::HomeKickCentre(glm::vec3 position, glm::vec3 kickPoint)
{
	return position + (kickPoint - position) * 0.5f;
}

HomeKick creature_object_actions::HomeKickBlows(glm::vec3 centre, float altitude, glm::vec3 direction, float gameHeading,
                                                float size, float homeRadius, uint32_t turnDraw, uint32_t firstHeightDraw,
                                                uint32_t secondHeightDraw)
{
	namespace x87 = openblack::game_math::x87;
	// The turn's draw less five, unsigned, then a tenth of it, rounded once to a float as the game's FPU rounds it
	const auto turnSteps = static_cast<double>(turnDraw - 5u);
	const float turn = x87::Store(x87::Mul(x87::Extended {turnSteps, 0.0}, static_cast<double>(0.1f)));
	const float firstShare = static_cast<float>(firstHeightDraw) * 0.01f;
	const float secondShare = static_cast<float>(secondHeightDraw) * 0.01f;
	// The point is no lower than the land, and the blows no higher above it than the reach
	const float y = std::max(centre.y, altitude);
	const float reach = size * k_HomeKickReachPerSize;
	const float above = std::min(y - altitude, reach);
	// The second blow's way: ahead along the land, turned by the draw
	constexpr float k_QuarterTurn = std::numbers::pi_v<float> / 2.0f;
	const float angle = turn * k_QuarterTurn + gameHeading;
	const auto [sine, cosine] = x87::SinCos(static_cast<double>(angle));
	const float homeShare = homeRadius * k_HomeKickRadiusShare;
	return {
	    .firstPoint = {centre.x, above * firstShare + altitude, centre.z},
	    .firstVelocity = direction * k_HomeKickBlowSpeed,
	    .secondPoint = {centre.x, above * secondShare + altitude, centre.z},
	    .secondVelocity = {k_HomeKickBlowSpeed * x87::Store(sine), 0.0f, -k_HomeKickBlowSpeed * x87::Store(cosine)},
	    .radius = homeShare < reach ? homeShare : reach,
	};
}

KindRule creature_object_actions::RuleOf(Thing thing)
{
	switch (thing)
	{
	case Thing::Other:
	case Thing::Fixture:
	case Thing::Creature:
	case Thing::Lantern:
	case Thing::Shield:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return KindRule::Never;
	case Thing::Static:
		return KindRule::Toy;
	case Thing::Ball:
	case Thing::Creed:
		return KindRule::Always;
	case Thing::Bird:
		return KindRule::Bird;
	case Thing::Pile:
		return KindRule::Pile;
	case Thing::MagicMushroom:
		return KindRule::Mushroom;
	case Thing::Villager:
	case Thing::Animal:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Tree:
		return KindRule::Whole;
	}
	return KindRule::Never;
}

bool creature_object_actions::Holdable(Thing thing)
{
	return thing != Thing::Other && thing != Thing::Fixture && thing != Thing::Creature && thing != Thing::Lantern &&
	       thing != Thing::Shield && thing != Thing::MagicMushroom;
}

bool creature_object_actions::IsLiving(Thing thing)
{
	return thing == Thing::Villager || thing == Thing::Animal || thing == Thing::Bird || thing == Thing::Creature;
}

bool creature_object_actions::CanBeStompedOnWhileAlive(Thing thing, bool dead)
{
	return (thing == Thing::Villager || thing == Thing::Animal) && !dead;
}

bool creature_object_actions::CanThingOnTheLandBeStompedOn(bool inMap, float stomperSize, float height)
{
	constexpr float k_HeightPerSize = 15.0f;
	constexpr float k_LowShare = 0.2f;
	return inMap && height < k_LowShare * (stomperSize * k_HeightPerSize);
}

bool creature_object_actions::CanBuildingBeStompedOn(float stomperHeight, float height)
{
	constexpr float k_LowShare = 0.25f;
	return height < stomperHeight * k_LowShare;
}

bool creature_object_actions::CanCreatureBeStompedOn(bool inMap, float stomperSize, float stomperHeight, float height,
                                                     bool recovering)
{
	constexpr float k_ShorterBy = 4.0f;
	if (!CanThingOnTheLandBeStompedOn(inMap, stomperSize, height))
	{
		return false;
	}
	// the share is taken as a float before it is compared
	const float share = stomperHeight / height;
	return share > k_ShorterBy || recovering;
}

creature_object_actions::StompRule creature_object_actions::StompRuleOf(Thing thing, StompGround ground)
{
	switch (thing)
	{
	case Thing::Villager:
	case Thing::Animal:
		return StompRule::WhileAlive;
	case Thing::Bird:
		return StompRule::LowBird;
	case Thing::Creature:
		return StompRule::Creature;
	case Thing::Ball:
	case Thing::Creed:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Pile:
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
		return StompRule::Always;
	case Thing::Static:
		// The singing stones' base is a thing set into the land; every other static lies about
		return ground == StompGround::SetIntoLand ? StompRule::OnTheLand : StompRule::Always;
	case Thing::Shield:
		// A spell's shield stands on the land; a static of the shield's row lies about as the other statics
		return ground == StompGround::LyingAbout ? StompRule::Always : StompRule::OnTheLand;
	case Thing::Tree:
	case Thing::MagicMushroom:
	case Thing::Lantern:
		return StompRule::OnTheLand;
	case Thing::Vortex:
	case Thing::TeleportStone:
		return StompRule::Never;
	case Thing::Fixture:
	case Thing::Other:
		break;
	}
	switch (ground)
	{
	case StompGround::Building:
		return StompRule::LowBuilding;
	case StompGround::SetIntoLand:
		return StompRule::OnTheLand;
	case StompGround::LyingAbout:
		return StompRule::Always;
	case StompGround::None:
	case StompGround::Field:
	case StompGround::FishFarm:
		break;
	}
	return StompRule::Never;
}

bool creature_object_actions::CanBeStompedOn(const StompTarget& target, float stomperSize, float stomperHeight)
{
	switch (target.rule)
	{
	case StompRule::Never:
		return false;
	case StompRule::Always:
		return true;
	case StompRule::WhileAlive:
		return !target.dead;
	case StompRule::LowBird:
		return target.lowBird;
	case StompRule::Creature:
		return CanCreatureBeStompedOn(target.inMap, stomperSize, stomperHeight, target.height, target.recovering);
	case StompRule::OnTheLand:
		return CanThingOnTheLandBeStompedOn(target.inMap, stomperSize, target.height);
	case StompRule::LowBuilding:
		return CanBuildingBeStompedOn(stomperHeight, target.height);
	}
	return false;
}

bool creature_object_actions::CanBeStoned(Thing thing, bool dead)
{
	switch (thing)
	{
	case Thing::Villager:
	case Thing::Animal:
	case Thing::Bird:
	case Thing::Creature:
		return !dead;
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::MagicMushroom:
	case Thing::Shield:
	case Thing::Fixture:
		return true;
	case Thing::Other:
	case Thing::Ball:
	case Thing::Creed:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Pile:
	case Thing::Tree:
	case Thing::Lantern:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return false;
	}
	return false;
}

bool creature_object_actions::DoesThrowingDamage(Thing thing, bool toy)
{
	switch (thing)
	{
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::Vortex:
	case Thing::TeleportStone:
		return !toy;
	default:
		return false;
	}
}

bool creature_object_actions::CanBeThrownToImpress(Thing thing, float thingHeight, float creatureWeight)
{
	if (IsLiving(thing) || thing == Thing::Vortex || thing == Thing::TeleportStone)
	{
		return false;
	}
	return thingHeight < creatureWeight * k_ThrowToImpressWeightShare;
}

bool creature_object_actions::CanBeExaminedByLooking(Thing thing)
{
	return thing != Thing::Creature && thing != Thing::Vortex && thing != Thing::TeleportStone;
}

bool creature_object_actions::CanBeThrownInTheSea(Thing thing)
{
	switch (thing)
	{
	case Thing::Ball:
	case Thing::Creed:
	case Thing::MobileObject:
	case Thing::Pot:
	case Thing::Pile:
	case Thing::Rock:
	case Thing::Bonfire:
	case Thing::DeadTree:
	case Thing::Static:
	case Thing::Vortex:
	case Thing::TeleportStone:
	case Thing::Tree:
		return true;
	default:
		return false;
	}
}

std::optional<Toy> creature_object_actions::ToyOf(MeshId mesh)
{
	switch (mesh)
	{
	case MeshId::ObjectToyBall:
		return Toy::Ball;
	case MeshId::ObjectToyBowlingBall:
		return Toy::BowlingBall;
	case MeshId::ObjectToyCuddly:
		return Toy::Cuddly;
	case MeshId::ObjectToyDice:
		return Toy::Die;
	case MeshId::ObjectToySkittle:
		return Toy::Skittle;
	default:
		return std::nullopt;
	}
}

std::optional<bool> creature_object_actions::DecidedByKind(const KindFacts& kind, bool thingAllows)
{
	switch (kind.rule)
	{
	case KindRule::Never:
		return false;
	case KindRule::Always:
		return true;
	case KindRule::Toy:
		return kind.toy;
	case KindRule::Bird:
		return kind.low;
	case KindRule::Mushroom:
		return thingAllows;
	case KindRule::Pile:
		return thingAllows && kind.mayFly;
	case KindRule::Whole:
		// What no hand holds is never picked up; the rest go by the test of things in general
		if (!kind.holdable)
		{
			return false;
		}
		return std::nullopt;
	}
	return false;
}
