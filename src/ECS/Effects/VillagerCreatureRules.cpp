/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerCreatureRules.h"

#include <cstdlib>

#include "Common/GUtilsAngle.h"
#include "ECS/Town/TownCreatureAttitude.h"

using namespace openblack;
using namespace openblack::ecs::effects;
namespace town_attitude = openblack::ecs::town_creature_attitude;

namespace
{
/// The town desires a state may serve: 0 to 16
constexpr int32_t k_DesireCount = 17;
/// A town that has never judged the creature takes it up half as much again
constexpr float k_NewCreatureScale = 1.5f;
/// The attitude no code writes
constexpr uint32_t k_NeverWritten = 2;
/// How much longer an impressive or feared creature keeps a villager reacting, and how much sooner it reacts again
constexpr float k_VeryImpressiveKeep = 3.0f;
constexpr float k_FearKeep = 4.0f;
constexpr float k_QuiteImpressiveAgain = 0.5f;
constexpr float k_VeryImpressiveAgain = 0.33333334f;

/// The low byte of a float truncated to a 64-bit whole number; out of range (or NaN) the conversion gives the
/// smallest 64-bit number, whose low byte is 0
uint8_t LowByteOfTruncation(float value)
{
	constexpr float k_TwoToThe63 = 9223372036854775808.0f;
	if (!(value >= -k_TwoToThe63 && value < k_TwoToThe63))
	{
		return 0;
	}
	return static_cast<uint8_t>(static_cast<uint64_t>(static_cast<int64_t>(value)) & 0xFFu);
}

/// A count of turns times a factor, the product rounded once to a float (exact below 2^29 turns), truncated and kept
/// as an unsigned number
uint32_t ScaledTurns(uint32_t turns, float factor)
{
	const auto product = static_cast<float>(static_cast<double>(turns) * static_cast<double>(factor));
	return static_cast<uint32_t>(map_coords::FtoL(product));
}

bool IsImpressive(uint32_t type)
{
	return type != creature_reaction::k_NotImpressive;
}
} // namespace

uint8_t creature_reaction::AnimalPriority(bool dancing, uint32_t priority)
{
	return dancing ? 0 : static_cast<uint8_t>(priority);
}

uint8_t creature_reaction::VillagerPriority(const PriorityFacts& facts)
{
	if (facts.dancing || !facts.isCreature || facts.ignoredByVillagers || facts.invisible)
	{
		return 0;
	}
	// beyond the reach of a run away (a NaN distance goes on)
	if (facts.distance > facts.maxDistanceToRunAway)
	{
		return 0;
	}
	if (!facts.hasTown)
	{
		return static_cast<uint8_t>(facts.priority & 0xFFu);
	}
	switch (facts.attitude)
	{
	case town_attitude::k_New:
	{
		// the whole priority times one and a half, rounded once to a float
		const auto scaled = static_cast<float>(static_cast<double>(facts.priority) * static_cast<double>(k_NewCreatureScale));
		return LowByteOfTruncation(scaled);
	}
	case town_attitude::k_None:
		return 0;
	case town_attitude::k_Fear:
	{
		// twice the priority of running from a thing, the whole number rounded to a float first
		const auto priority = static_cast<float>(facts.fleePriority);
		return LowByteOfTruncation(priority + priority);
	}
	case town_attitude::k_Respect:
	{
		if (!facts.sameOwner)
		{
			return static_cast<uint8_t>(facts.followPriority & 0xFFu);
		}
		if (facts.disciple)
		{
			return 0;
		}
		// busy with something its town wants done: none
		if (facts.servedDesire >= 0 && facts.servedDesire < k_DesireCount &&
		    facts.townRawDesire * facts.servedDesireAmount > 0.0f)
		{
			return 0;
		}
		return static_cast<uint8_t>(facts.followPriority & 0xFFu);
	}
	default:
		// the attitude no code writes reads as the reaction's own priority; anything beyond respect as none
		return facts.attitude == k_NeverWritten ? static_cast<uint8_t>(facts.priority & 0xFFu) : uint8_t {0};
	}
}

uint32_t creature_reaction::TurnsToReact(const TurnsFacts& facts)
{
	// Each step rounded to a float, as the original computes it
	const float reach = facts.metresPerCell + facts.maxReactionDistance;
	float factor = (reach - facts.distance) * facts.howImportantIsDistance;
	factor = factor / reach;
	factor = factor + (1.0f - facts.howImportantIsDistance);
	if (!facts.isCreature)
	{
		return 0;
	}
	if (facts.impressiveType == k_QuiteImpressive)
	{
		factor = factor + factor;
	}
	else if (facts.impressiveType == k_VeryImpressive)
	{
		factor = factor * k_VeryImpressiveKeep;
	}
	if (facts.townAttitude == town_attitude::k_Fear)
	{
		factor = factor * k_FearKeep;
	}
	return ScaledTurns(facts.turns, factor);
}

uint32_t creature_reaction::TurnsBeforeReactingAgain(const TurnsFacts& facts)
{
	const float reach = facts.metresPerCell + facts.maxReactionDistance;
	float factor = facts.howImportantIsDistance * facts.distance;
	factor = factor / reach;
	factor = factor + (1.0f - facts.howImportantIsDistance);
	if (facts.impressiveType == k_QuiteImpressive)
	{
		factor = factor * k_QuiteImpressiveAgain;
	}
	else if (facts.impressiveType == k_VeryImpressive)
	{
		factor = factor * k_VeryImpressiveAgain;
	}
	// busy with something impressive, or feared or respected: at once
	if (facts.currentActionImpressiveType.has_value() && IsImpressive(*facts.currentActionImpressiveType))
	{
		return 0;
	}
	if (facts.townAttitude.has_value() && *facts.townAttitude >= town_attitude::k_Fear &&
	    *facts.townAttitude <= town_attitude::k_Respect)
	{
		return 0;
	}
	return ScaledTurns(facts.turns, factor);
}

std::optional<VillagerStates> creature_reaction::SetupState(uint32_t attitude)
{
	switch (attitude)
	{
	case town_attitude::k_Fear:
		return VillagerStates::FleeingFromCreatureReaction;
	case town_attitude::k_None:
		return std::nullopt;
	default:
		// new, respect and the attitude no code writes; nothing beyond respect
		return attitude <= town_attitude::k_Respect ? std::optional(VillagerStates::InspectCreatureReaction) : std::nullopt;
	}
}

uint32_t creature_reaction::InspectTurns(int32_t minTurns, uint32_t draw)
{
	return draw + static_cast<uint32_t>(minTurns);
}

creature_reaction::InspectStep creature_reaction::PerformInspectStep(uint32_t turnsLeft, float inspectDistance)
{
	// below the near distance (a NaN distance too)
	if (!(inspectDistance >= k_InspectNearEnough))
	{
		return turnsLeft == 0 ? InspectStep::Stop : InspectStep::Wait;
	}
	return turnsLeft == 0 ? InspectStep::Approach : InspectStep::Look;
}

bool creature_reaction::ApproachFurther(float distance, float inspectDistance)
{
	const float beyond = distance - inspectDistance;
	return beyond > k_ApproachMargin;
}

int32_t creature_reaction::MetresToWholeDistance(float metres)
{
	const auto whole = static_cast<float>(map_coords::FtoL(metres));
	return map_coords::FtoL(whole / 10.0f * 65536.0f);
}

glm::ivec2 creature_reaction::ApproachStep(uint16_t angle, float distance, float inspectDistance)
{
	const float way = distance - inspectDistance;
	return gutils::StepFromAngleCoarse(angle, MetresToWholeDistance(way));
}

float creature_reaction::NextInspectDistance(float inspectDistance)
{
	return inspectDistance - k_InspectCloser;
}

bool creature_reaction::Arrived(float distanceToGoal)
{
	return !(distanceToGoal > k_ArrivedWithin);
}

map_coords::MapCoords creature_reaction::RespectWalkGoal(const map_coords::MapCoords& edge)
{
	return {.x = map_coords::ToFixed(map_coords::ToMetres(edge.x)), .z = map_coords::ToFixed(map_coords::ToMetres(edge.z))};
}

bool creature_reaction::FacingAway(uint16_t angleToCreature, uint16_t gameAngle)
{
	const int32_t difference = static_cast<int32_t>(angleToCreature) - static_cast<int32_t>(gameAngle);
	return static_cast<uint32_t>(std::abs(difference)) > k_RespectFacingSlack;
}

float creature_reaction::FleeDistance(float maxReactionDistance)
{
	return maxReactionDistance * k_FleeReachScale;
}

bool creature_reaction::FleeGivesUp(float fleeDistance, float distance)
{
	const float giveUp = fleeDistance * k_FleeGiveUpScale;
	return !(giveUp >= distance);
}

int32_t creature_reaction::HalfSpeed(int32_t speed)
{
	return speed / 2;
}

creature_reaction::AttitudeStep creature_reaction::StepFromAttitude(uint32_t attitude, VillagerStates finalState)
{
	switch (attitude)
	{
	case town_attitude::k_None:
		return AttitudeStep::Stop;
	case town_attitude::k_Fear:
		return finalState != VillagerStates::FleeingFromCreatureReaction ? AttitudeStep::Flee : AttitudeStep::Keep;
	case town_attitude::k_Respect:
	{
		const bool respecting = finalState >= VillagerStates::InitialiseRespectCreatureReaction &&
		                        finalState <= VillagerStates::FinishRespectCreatureReaction;
		return respecting || finalState == VillagerStates::TurnToFaceCreatureReaction ? AttitudeStep::Keep
		                                                                              : AttitudeStep::Respect;
	}
	default:
		return AttitudeStep::Keep;
	}
}
