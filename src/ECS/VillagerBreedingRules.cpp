/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerBreedingRules.h"

#include "Common/GUtilsAngle.h"
#include "ECS/VillagerRoutine.h"

using namespace openblack;
namespace villager_breeding = openblack::ecs::villager_breeding;
namespace routine = openblack::ecs::villager_routine;

uint16_t villager_breeding::LoveTurns(uint32_t mateTime, uint32_t ownTime)
{
	return static_cast<uint16_t>((mateTime + ownTime + ownTime) / 2 + 1);
}

bool villager_breeding::BreederMayBreed(const TownHeadcount& town, float overAllowance)
{
	const auto over = [](uint32_t count, uint32_t places) {
		return static_cast<float>(static_cast<int32_t>(count) - static_cast<int32_t>(places));
	};
	const float adultLimit = overAllowance * k_BreederAdultShare;
	return over(town.children, town.childPlaces) < overAllowance && over(town.adults, town.adultPlaces) < adultLimit;
}

glm::ivec2 villager_breeding::BreederSpotByHome(glm::ivec2 home, glm::ivec2 door, float homeRadius, const IntRandom& intRandom,
                                                const FloatRandom& floatRandom)
{
	const float out = gutils::Get3DAngleFromXZ(home, door);
	const float side = intRandom(2) != 0 ? 1.0f : -1.0f;
	// each step rounded to a float, as the game's FPU does
	const float turn = floatRandom(k_BreederFan) * side;
	const float angle = turn + out;
	const float half = homeRadius * k_BreederHomeShare;
	const float distance = floatRandom(half) + half;
	return routine::Polar(door, angle, distance);
}

glm::ivec2 villager_breeding::BreederSpotByStore(glm::ivec2 store, const FloatRandom& random)
{
	const float distance = random(k_BreederStoreRange) + k_BreederStoreLeast;
	const float angle = random(routine::k_TwoPi);
	return routine::Polar(store, angle, distance);
}

map_coords::MapCoords villager_breeding::WanderGoal(const map_coords::MapCoords& area, const FloatRandom& random)
{
	auto goal = map_coords::MapCoords {area.x, area.z, 0.0f};
	const float angle = random(routine::k_TwoPi);
	const float distance = random(k_WanderReach);
	gutils::AddDistanceFromAngle(goal, angle, distance);
	return goal;
}

uint16_t villager_breeding::WanderPause(const IntRandom& random)
{
	return static_cast<uint16_t>(random(k_WanderPauseRange) + k_WanderPauseLeast);
}
