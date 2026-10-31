/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObjectActions.h"

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/GUtilsX87.h"
#include "Creature/CreatureLean.h"
#include "Creature/CreatureThrow.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_object_actions;

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

bool creature_object_actions::KnockDownCrushes(Kind kind, Status status, bool living, bool inMap)
{
	return kind == Kind::Destroy && status == Status::Contact && living && inMap;
}

std::optional<PlayerNames> creature_object_actions::ThrowerPlayerOf(PlayerNames creaturePlayer)
{
	return creaturePlayer != PlayerNames::NEUTRAL ? std::optional(creaturePlayer) : std::nullopt;
}

glm::vec3 creature_object_actions::HomeKickCentre(glm::vec3 position, glm::vec3 kickPoint)
{
	return position + (kickPoint - position) * 0.5f;
}

HomeKick creature_object_actions::HomeKickBlows(glm::vec3 centre, float altitude, glm::vec3 direction, float gameHeading,
                                                float size, float homeRadius, uint32_t turnDraw, uint32_t firstHeightDraw,
                                                uint32_t secondHeightDraw)
{
	namespace x87 = openblack::gutils::x87;
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
