/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerRoutine.h"

#include <glm/mat4x4.hpp>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Villager/VillagerStateFunctions.h"

using namespace openblack;
namespace villager_routine = openblack::ecs::villager_routine;

glm::ivec2 villager_routine::Polar(glm::ivec2 from, float angle, float metres)
{
	const auto step = gutils::GetPosFromAngle(angle, metres);
	return from + glm::ivec2(step.x, step.z);
}

glm::ivec2 villager_routine::DoorPoint(glm::vec3 meshDoor, glm::ivec2 building, float yAngle, float scale)
{
	// The building's matrix as it was placed, at its map position (its height does not reach x or z)
	const glm::vec3 at(map_coords::ToMetres(building.x), 0.0f, map_coords::ToMetres(building.y));
	const glm::mat4 m = affine::PlacementMatrix(at, yAngle, scale);
	const auto axis = [&m, &meshDoor](int k) {
		return ((meshDoor.z * m[2][k] + meshDoor.y * m[1][k]) + meshDoor.x * m[0][k]) + m[3][k];
	};
	return {map_coords::ToFixed(axis(0)), map_coords::ToFixed(axis(2))};
}

glm::ivec2 villager_routine::PosOutsideDoor(glm::ivec2 abode, glm::ivec2 door, const FloatRandom& random)
{
	const float angle = gutils::Get3DAngleFromXZ(abode, door);
	const float distance = random(k_OutsideDoorRange) + k_OutsideDoorLeast;
	const float jitter = k_EighthPi - random(k_QuarterPi);
	return Polar(door, jitter + angle, distance);
}

glm::ivec2 villager_routine::PosOutside(glm::ivec2 abode, glm::ivec2 door, float divisions, float leastDistance, float range,
                                        const FloatRandom& random)
{
	const float spread = random(k_TwoPi / divisions) - k_TwoPi / (divisions + divisions);
	const float angle = gutils::Get3DAngleFromXZ(abode, door) + spread;
	const float distance = random(range) + leastDistance;
	return Polar(door, angle, distance);
}

glm::ivec2 villager_routine::ChillOutPos(glm::ivec2 congregation, glm::ivec2 villager, float chillOutDistance,
                                         const FloatRandom& random)
{
	const float radius = chillOutDistance * k_ChillOutRadiusShare;
	const float angle = gutils::Get3DAngleFromXZ(congregation, villager);
	const float jitter = random(k_QuarterPi) - k_EighthPi;
	const float distance = random(radius * k_ChillOutRadiusSpan) + radius;
	return Polar(congregation, jitter + angle, distance);
}

villager_routine::Idle villager_routine::NothingToDo(bool hasAbode, bool abodeFunctional, bool hasTown, const IntRandom& random)
{
	constexpr uint32_t k_Ways = 9;
	constexpr uint32_t k_LastGoHome = 0;
	constexpr uint32_t k_LastOutsideHome = 3;
	constexpr uint32_t k_HomelessGoHomeChance = 100;
	constexpr uint32_t k_HomelessGoHomeUnder = 10;
	const uint32_t way = random(k_Ways);
	if (way <= k_LastGoHome && ((hasAbode && abodeFunctional) || random(k_HomelessGoHomeChance) < k_HomelessGoHomeUnder))
	{
		return Idle::GoHome;
	}
	if (way <= k_LastOutsideHome && hasAbode)
	{
		return Idle::ChillOutsideHome;
	}
	return hasTown ? Idle::SitInTown : Idle::GoHome;
}

villager_routine::SleepResult villager_routine::CheckSleep(const Sleep& sleep)
{
	if (sleep.poisoned)
	{
		return {};
	}
	SleepResult result;
	float life = sleep.life;
	if (life < sleep.fullLife)
	{
		result.heals = true;
		result.gain = sleep.multiplier * sleep.restores;
		// Life never goes over full: what would take it over makes it up to full
		life = life + result.gain > 1.0f ? life + (1.0f - life) : life + result.gain;
	}
	result.sleepsOn = sleep.townWantsSleep || life < sleep.sleepUntil;
	return result;
}

glm::ivec2 villager_routine::TowardsTown(glm::ivec2 town, glm::ivec2 villager, const FloatRandom& random)
{
	const float spread = random(k_HalfPi) - k_QuarterPi;
	const float angle = gutils::Get3DAngleFromXZ(town, villager) + spread;
	const float distance = random(k_TowardsTownRange) + k_TowardsTownLeast;
	return Polar(town, angle, distance);
}

glm::ivec2 villager_routine::RoundAbout(glm::ivec2 from, float least, float range, const FloatRandom& random)
{
	const float distance = random(range) + least;
	const float angle = random(k_TwoPi);
	return Polar(from, angle, distance);
}

glm::ivec2 villager_routine::VagrantWander(glm::ivec2 villager, float facing, const FloatRandom& random)
{
	const float spread = random(k_QuarterPi) - k_EighthPi;
	const float angle = facing + spread;
	const float distance = random(k_VagrantWanderRange) + k_VagrantWanderLeast;
	return Polar(villager, angle, distance);
}

glm::ivec2 villager_routine::VagrantTentSearch(glm::ivec2 villager, const FloatRandom& random)
{
	// The distance has no least: the draw alone
	const float distance = random(k_VagrantTentRange);
	const float angle = random(k_TwoPi);
	return Polar(villager, angle, distance);
}

glm::ivec2 villager_routine::BrokenHomeTentSearch(glm::ivec2 abode, glm::ivec2 villager, const FloatRandom& random)
{
	const float jitter = k_EighthPi - random(k_QuarterPi);
	const float angle = gutils::Get3DAngleFromXZ(abode, villager) + jitter;
	const float distance = random(k_BrokenHomeTentRange) + k_BrokenHomeTentLeast;
	return Polar(villager, angle, distance);
}

glm::ivec2 villager_routine::TentRetry(glm::ivec2 spot, const FloatRandom& random)
{
	return RoundAbout(spot, k_TentRetryLeast, k_TentRetryRange, random);
}

bool villager_routine::AnswersTownEmergency(VillagerStates state, VillagerStates previous)
{
	// The game's table of the states' answers, read at the state's low byte
	using ecs::villager::TownEmergencyReaction;
	const auto index = static_cast<size_t>(static_cast<uint32_t>(state) & 0xFF);
	if (index >= ecs::villager::k_TownEmergencyReaction.size())
	{
		return false;
	}
	switch (ecs::villager::k_TownEmergencyReaction.at(index))
	{
	case TownEmergencyReaction::Always:
		return true;
	case TownEmergencyReaction::PreviousState:
		return previous != VillagerStates::InvalidState;
	case TownEmergencyReaction::None:
	default:
		return false;
	}
}
