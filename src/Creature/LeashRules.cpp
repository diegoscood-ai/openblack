/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashRules.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "Creature/CreatureRig.h"

using namespace openblack;
using namespace openblack::creature_leash;
using creature_desires::Desire;

namespace
{
/// A creature of size 1 is fifteen units tall, which is what the hand's leash length goes by
constexpr float k_SizeToScale = 15.0f;
constexpr float k_HandSlackScale = 0.7f;
constexpr float k_HandSlackBase = 22.0f;
constexpr float k_HandMaxScale = 3.0f;
constexpr float k_HandMaxBase = 32.0f;
constexpr float k_TiedTreeHeights = 6.0f;
constexpr float k_TiedTreeMax = 40.0f;
constexpr float k_TiedObjectReach = 1.5f;
constexpr float k_TiedObjectMin = 180.0f;
constexpr float k_TiedObjectMax = 360.0f;
constexpr float k_SlackShare = 0.5f;
} // namespace

std::optional<size_t> creature_leash::IndexOf(LeashType type)
{
	const auto it = std::ranges::find(k_Types, type);
	if (it == k_Types.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(k_Types.begin(), it));
}

const char* creature_leash::Name(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return "Aggression";
	case LeashType::Rope:
		return "Learning";
	case LeashType::Good:
		return "Compassion";
	case LeashType::None:
		break;
	}
	return "None";
}

Lengths creature_leash::InHand(float creatureSize)
{
	const auto s = k_SizeToScale * creatureSize;
	return {.slack = (k_HandSlackScale * s) + k_HandSlackBase, .max = (k_HandMaxScale * s) + k_HandMaxBase};
}

Lengths creature_leash::TiedToTree(float creatureHeight)
{
	const auto max = std::min(creatureHeight * k_TiedTreeHeights, k_TiedTreeMax);
	return {.slack = k_SlackShare * max, .max = max};
}

Lengths creature_leash::TiedToObject(float distance)
{
	auto max = distance * k_TiedObjectReach;
	// Anything short of the shortest, a distance that is not a number too, gives the shortest
	if (!(max >= k_TiedObjectMin))
	{
		max = k_TiedObjectMin;
	}
	else if (max > k_TiedObjectMax)
	{
		max = k_TiedObjectMax;
	}
	return {.slack = k_SlackShare * max, .max = max};
}

glm::vec3 creature_leash::CollarAt(const glm::mat4& drawn, std::span<const glm::mat4> bones, std::optional<uint32_t> bone,
                                   float height, float sizeShare)
{
	if (!bone.has_value() || *bone >= bones.size())
	{
		return glm::vec3(drawn[3]) + glm::vec3(0.0f, height * sizeShare * k_CollarHeightShare, 0.0f);
	}
	return glm::vec3(creature::PosedBone(*bone, bones, drawn)[3]);
}

leash_rope::Look creature_leash::LookFor(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return {.halfWidth = 0.15f, .v0 = 0.375f, .v1 = 0.5f, .uScale = 2.5f};
	case LeashType::Good:
		return {.halfWidth = 0.225f, .v0 = 0.25f, .v1 = 0.375f, .uScale = 2.5f};
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return {.halfWidth = 0.15f, .v0 = 0.125f, .v1 = 0.25f, .uScale = 2.5f};
}

std::optional<Desire> creature_leash::ForcedDesireFor(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return Desire::Anger;
	case LeashType::Good:
		return Desire::Compassion;
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return std::nullopt;
}

bool creature_leash::WantsToImpress(const TiedTown& town)
{
	return town.mostInAnother * 0.5f >= town.beliefInPlayer || !town.playersOwn;
}

uint32_t creature_leash::MiracleSightingWeight(bool learningLeash)
{
	return learningLeash ? 3 : 1;
}

std::optional<float> creature_leash::RecordPull(PullMemory& memory, Desire desire)
{
	auto& count = memory.counts.at(static_cast<size_t>(desire));
	count = static_cast<uint8_t>(std::min<int>(count + 1, UINT8_MAX));
	if (count < 2)
	{
		return std::nullopt;
	}
	const auto seconds = static_cast<float>(count) * k_SuppressSecondsPerPull;
	count = 0;
	return seconds;
}

Tug creature_leash::DecideTug(const TugCheck& check)
{
	if (check.clearingWay || check.walkingBack || check.controlledByScript)
	{
		return {};
	}
	Tug tug {.pulledAway = !check.led};
	if (check.led && check.planOnOther && (check.tied || check.bodyToHand < k_CloseToHand))
	{
		tug.lead = Lead::Stay;
		return tug;
	}
	if (check.led && check.creatureToHand > 0.0f && check.routeEndToHand.has_value() &&
	    *check.routeEndToHand / check.creatureToHand < k_CloserShare)
	{
		tug.lead = Lead::KeepGoing;
		return tug;
	}
	tug.lead = check.sentToFromHand > k_HandMoved ? Lead::GoToHand : Lead::Stay;
	return tug;
}

float creature_leash::FadePull(float pull)
{
	if (!(pull > k_PullFadesAbove))
	{
		return pull;
	}
	const auto faded = pull * k_PullFade;
	return faded < k_PullGone ? 0.0f : faded;
}

bool creature_leash::FreeOfHome(float distanceFromHome, bool playerHasTemple)
{
	return playerHasTemple && distanceFromHome < k_HomeRange;
}

bool creature_leash::KeptAtHome(const HomeKeeping& keeping)
{
	return !keeping.leashed && keeping.developmentPhase < k_YoungUntilPhase && keeping.localPlayers &&
	       !keeping.computerPlayer && !keeping.multiplayer && keeping.landNumber == k_YoungHomeLand;
}

bool creature_leash::IsConfined(float radius, bool leashed, bool leashWorks)
{
	return radius > 0.0f && !(leashed && !leashWorks);
}

bool creature_leash::ShouldWalkBack(const WalkBackCheck& check)
{
	if (check.clearingWay || !check.confined || check.sentByLeash || check.teleporting || check.sentToPoint ||
	    check.controlledByScript || !check.pointOnMap || !(check.distance > check.radius))
	{
		return false;
	}
	// Walking back to a place near enough the point already, it carries on
	if (check.walkingBack && !(check.walkingToFromPoint > WalkBackRestartDistance(check.radius)))
	{
		return false;
	}
	return check.pointOnLand;
}

float creature_leash::WalkBackRestartDistance(float radius)
{
	const auto half = radius * 0.5f;
	return k_WalkBackRestartFloor <= half ? half : k_WalkBackRestartFloor;
}

float creature_leash::WalkBackHurry(float distance, float radius)
{
	const auto across = radius + radius;
	auto strayed = distance;
	if (!(strayed >= 0.0f))
	{
		strayed = 0.0f;
	}
	else if (strayed > across)
	{
		strayed = across;
	}
	return (strayed * k_WalkBackHurry) / across;
}

float creature_leash::WalkBackArrival(float height, float radius)
{
	return height < radius ? radius : height;
}

bool creature_leash::AttitudeStepDue(uint32_t lastStep, uint32_t turn)
{
	// Counted without sign, so a turn before the last step (a clock started again) is long past it
	return lastStep == 0 || turn - lastStep > k_AttitudeTurns;
}

float creature_leash::AttitudeStep(LeashType type)
{
	switch (type)
	{
	case LeashType::Evil:
		return -k_AttitudeStep;
	case LeashType::Good:
		return k_AttitudeStep;
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return 0.0f;
}

std::vector<Lesson> creature_leash::LessonsFor(LeashType type, bool objectIsCreature)
{
	const auto kind = objectIsCreature ? Desire::BeFriends : Desire::Compassion;
	switch (type)
	{
	case LeashType::Evil:
		return {{.desire = Desire::Anger, .change = 1.0f}, {.desire = kind, .change = -1.0f}};
	case LeashType::Good:
		return {{.desire = Desire::Anger, .change = -1.0f}, {.desire = kind, .change = 1.0f}};
	case LeashType::Rope:
	case LeashType::None:
		break;
	}
	return {};
}
