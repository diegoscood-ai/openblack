/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <deque>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/VillagerRoutine.h"

using namespace openblack;
using namespace openblack::ecs::villager_routine;

namespace
{
/// Hands out set answers in turn and keeps the limits it was asked for
struct FakeFloats
{
	std::deque<float> answers;
	std::vector<float> limits;

	[[nodiscard]] FloatRandom Random()
	{
		return [this](float limit) {
			limits.push_back(limit);
			const float answer = answers.front();
			answers.pop_front();
			return answer;
		};
	}
};

struct FakeInts
{
	std::deque<uint32_t> answers;
	std::vector<uint32_t> limits;

	[[nodiscard]] IntRandom Random()
	{
		return [this](uint32_t limit) {
			limits.push_back(limit);
			const uint32_t answer = answers.front();
			answers.pop_front();
			return answer;
		};
	}
};

/// A point in metres in whole map units, and back
glm::ivec2 At(glm::vec2 metres)
{
	return {map_coords::ToFixed(metres.x), map_coords::ToFixed(metres.y)};
}

glm::vec2 Metres(glm::ivec2 at)
{
	return {map_coords::ToMetres(at.x), map_coords::ToMetres(at.y)};
}

/// The map's grid rounds positions a little
constexpr float k_GridLeeway = 0.05f;
} // namespace

TEST(VillagerRoutine, OutOfTheDoorTheDistanceIsDrawnBeforeTheAngle)
{
	FakeFloats random {.answers = {0.0f, k_EighthPi}};
	const glm::vec2 abode {100.0f, 100.0f};
	const glm::vec2 door {100.0f, 104.0f};
	const auto spot = Metres(PosOutsideDoor(At(abode), At(door), random.Random()));
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_OutsideDoorRange);
	EXPECT_FLOAT_EQ(random.limits[1], k_QuarterPi);
	// No jitter and the least distance: straight out from the home, a step and a half past the door
	EXPECT_NEAR(glm::distance(spot, door), k_OutsideDoorLeast, k_GridLeeway);
	EXPECT_GT(glm::distance(spot, abode), glm::distance(door, abode));
}

TEST(VillagerRoutine, SittingAboutTownIsWithinTheChillOutDistance)
{
	FakeFloats random {.answers = {k_EighthPi, 0.0f}};
	const glm::vec2 congregation {200.0f, 200.0f};
	const glm::vec2 villager {200.0f, 260.0f};
	const auto spot = Metres(ChillOutPos(At(congregation), At(villager), 30.0f, random.Random()));
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_QuarterPi);
	EXPECT_FLOAT_EQ(random.limits[1], 27.0f);
	// The least distance, a tenth of the chill-out distance, on the villager's side
	EXPECT_NEAR(glm::distance(spot, congregation), 3.0f, k_GridLeeway);
	EXPECT_LT(glm::distance(spot, villager), glm::distance(congregation, villager));
}

TEST(VillagerRoutine, NothingToDoIsOneInNineHomeThreeOutsideFiveInTown)
{
	FakeInts home {.answers = {0}};
	EXPECT_EQ(NothingToDo(true, true, true, home.Random()), Idle::GoHome);
	FakeInts outside {.answers = {3}};
	EXPECT_EQ(NothingToDo(true, true, true, outside.Random()), Idle::ChillOutsideHome);
	FakeInts town {.answers = {4}};
	EXPECT_EQ(NothingToDo(true, true, true, town.Random()), Idle::SitInTown);
	EXPECT_EQ(town.limits, std::vector<uint32_t>({9}));
}

TEST(VillagerRoutine, NothingToDoWithABrokenHomeGoesHomeOneTimeInTen)
{
	FakeInts goes {.answers = {0, 9}};
	EXPECT_EQ(NothingToDo(true, false, true, goes.Random()), Idle::GoHome);
	EXPECT_EQ(goes.limits, std::vector<uint32_t>({9, 100}));
	FakeInts stays {.answers = {0, 10}};
	EXPECT_EQ(NothingToDo(true, false, true, stays.Random()), Idle::ChillOutsideHome);
}

TEST(VillagerRoutine, NothingToDoWithNoTownEndsAtHome)
{
	FakeInts random {.answers = {8}};
	EXPECT_EQ(NothingToDo(false, false, false, random.Random()), Idle::GoHome);
}

TEST(VillagerRoutine, AKnockedVillagerSkipsBedUnlessBadlyHurt)
{
	EXPECT_TRUE(MaySleep(false, 1.0f, 0.3f));
	EXPECT_FALSE(MaySleep(true, 1.0f, 0.3f));
	EXPECT_TRUE(MaySleep(true, 0.2f, 0.3f));
}

TEST(VillagerRoutine, SleepRestoresLifeUpToFull)
{
	// The gain is the whole restore, as the game asks it of the life; the life's own increase stops it at full, so it
	// wakes
	const auto result = CheckSleep({.life = 0.95f, .fullLife = 1.0f, .restores = 0.1f, .sleepUntil = 0.5f});
	EXPECT_TRUE(result.heals);
	EXPECT_EQ(result.gain, 0.1f);
	EXPECT_FALSE(result.sleepsOn);
}

TEST(VillagerRoutine, SleepGoesOnWhileTheTownWantsItOrLifeIsLow)
{
	EXPECT_TRUE(CheckSleep({.life = 1.0f, .townWantsSleep = true}).sleepsOn);
	const auto low = CheckSleep({.life = 0.2f, .fullLife = 1.0f, .restores = 0.01f, .multiplier = 2.0f, .sleepUntil = 0.5f});
	EXPECT_FLOAT_EQ(low.gain, 0.02f);
	EXPECT_TRUE(low.sleepsOn);
}

TEST(VillagerRoutine, PoisonWakesASleeper)
{
	const auto result = CheckSleep({.poisoned = true, .life = 0.2f, .restores = 0.1f, .townWantsSleep = true});
	EXPECT_FALSE(result.heals);
	EXPECT_FLOAT_EQ(result.gain, 0.0f);
	EXPECT_FALSE(result.sleepsOn);
}

TEST(VillagerRoutine, AnEmergencyLastsItsTurnsFromTheLastCall)
{
	EXPECT_FALSE(IsInStateOfEmergency(0, 500, 1200));
	EXPECT_TRUE(IsInStateOfEmergency(100, 1299, 1200));
	EXPECT_FALSE(IsInStateOfEmergency(100, 1300, 1200));
}

TEST(VillagerRoutine, ThoseInsideOrAsleepDontAnswerAnEmergency)
{
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::AtHome, VillagerStates::InvalidState));
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::SleepingAtHome, VillagerStates::InvalidState));
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::SitAndChillout, VillagerStates::InvalidState));
	EXPECT_TRUE(AnswersTownEmergency(VillagerStates::MoveToPos, VillagerStates::InvalidState));
	// Moving about a fire, it answers only with a state to go back to
	EXPECT_FALSE(AnswersTownEmergency(VillagerStates::MoveAroundFire, VillagerStates::InvalidState));
	EXPECT_TRUE(AnswersTownEmergency(VillagerStates::MoveAroundFire, VillagerStates::DecideWhatToDo));
}

TEST(VillagerRoutine, ARoamingSpotIsDrawnDistanceFirst)
{
	FakeFloats random {.answers = {0.0f, 0.0f}};
	const glm::vec2 from {50.0f, 50.0f};
	const auto spot = Metres(TentRetry(At(from), random.Random()));
	ASSERT_EQ(random.limits.size(), 2u);
	EXPECT_FLOAT_EQ(random.limits[0], k_TentRetryRange);
	EXPECT_FLOAT_EQ(random.limits[1], k_TwoPi);
	EXPECT_NEAR(glm::distance(spot, from), k_TentRetryLeast, k_GridLeeway);
}

namespace
{
/// A building's door as the original game places it: the building's map position, its y angle and scale, the mesh's
/// door point (float bits) and the door's map position it gives
struct DoorCase
{
	int32_t x, z;
	uint32_t yAngle, scale;
	uint32_t doorX, doorY, doorZ;
	int32_t doorMapX, doorMapZ;
};

constexpr std::array<DoorCase, 10> k_DoorCases {{
    {11502117, 17646215, 0x4096B852, 0x3F7D70A4, 0x3F9DF3B6, 0x3F000000, 0xBFC89375, 11491931, 17638232},
    {11667903, 17782681, 0x40C90625, 0x3F800000, 0xBF400000, 0x3E4CCCCD, 0x40066666, 11663004, 17796450},
    {12175255, 17469350, 0x3F96A7F0, 0x3F6353F8, 0x00000000, 0x00000000, 0x3FC00000, 12167194, 17472700},
    {11827532, 17373911, 0x40C6F1AA, 0x3F800000, 0x40100000, 0x3F800000, 0x3E000000, 11842299, 17373752},
    {11699624, 17384462, 0x40B83127, 0x3F6353F8, 0xBFCCCCCD, 0x00000000, 0xBF666666, 11688942, 17384622},
    {10834069, 17394357, 0x00000000, 0x3F800000, 0x3F8CCCCD, 0x3E99999A, 0x3FA66666, 10841278, 17402878},
    {10834069, 17394357, 0x00000000, 0x3F483127, 0x3F8CCCCD, 0x3E99999A, 0x3FA66666, 10839707, 17401020},
    {11231870, 16990926, 0x3FCB4396, 0x3F800000, 0x3E99999A, 0x00000000, 0xC02CCCCD, 11249529, 16993198},
    {11231870, 16990926, 0x406EB852, 0x3F483127, 0x40400000, 0x00000000, 0x00000000, 11219082, 16982394},
    {11231870, 16990926, 0x40BE7EFA, 0x3F483127, 0x38D1B717, 0x00000000, 0xB8D1B717, 11231871, 16990926},
}};
} // namespace

TEST(VillagerRoutine, TheDoorIsTheMeshDoorPlacedWithTheBuilding)
{
	for (const auto& c : k_DoorCases)
	{
		const glm::vec3 door(std::bit_cast<float>(c.doorX), std::bit_cast<float>(c.doorY), std::bit_cast<float>(c.doorZ));
		const auto at = DoorPoint(door, {c.x, c.z}, std::bit_cast<float>(c.yAngle), std::bit_cast<float>(c.scale));
		EXPECT_EQ(at.x, c.doorMapX) << c.x << ", " << c.z << " angle " << std::bit_cast<float>(c.yAngle);
		EXPECT_EQ(at.y, c.doorMapZ) << c.x << ", " << c.z << " angle " << std::bit_cast<float>(c.yAngle);
	}
}
