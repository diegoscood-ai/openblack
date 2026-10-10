/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature thinks its player wants (Creature/PerceivedDesires.h): the clamped add and its guards, the fade, the
// dominant desire taken and forgotten, whether it sees a point; and the mind's side with a fake leash:
// only the player's own creature, and only when it sees the point, the feedback's desires, and the town needs passed on
// by the game's handler

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <limits>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/PerceivedDesires.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "Enums.h"
#include "Locator.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::creature_perceived_desires;
using openblack::ecs::components::CreatureLocomotion;
using openblack::ecs::components::CreatureMindState;
using openblack::ecs::components::Transform;
using openblack::test::creature_loop_fakes::CallLog;
using openblack::test::creature_loop_fakes::FakeLeash;
using openblack::test::creature_loop_fakes::FakeMind;

namespace
{
map_coords::MapCoords At(float x, float z)
{
	return map_coords::FromMetres(glm::vec2(x, z));
}
} // namespace

TEST(PerceivedDesires, TheWeightIsAddedThenHeldBetweenNoneAndOne)
{
	PerceivedDesires desires;
	Increase(desires, 2, 0.75f);
	EXPECT_FLOAT_EQ(desires.player[2], 0.75f);
	Increase(desires, 2, 0.75f);
	EXPECT_EQ(desires.player[2], 1.0f);
	Increase(desires, 2, -1.5f);
	EXPECT_EQ(desires.player[2], 0.0f);
	// not a number is none
	Increase(desires, 3, std::numeric_limits<float>::quiet_NaN());
	EXPECT_EQ(desires.player[3], 0.0f);
	IncreaseTown(desires, 0, 0.5f);
	IncreaseTown(desires, 0, 0.75f);
	EXPECT_EQ(desires.town[0], 1.0f);
	IncreaseTown(desires, 0, -2.0f);
	EXPECT_EQ(desires.town[0], 0.0f);
}

TEST(PerceivedDesires, ADesireOutOfItsArrayIsNothing)
{
	PerceivedDesires desires;
	Increase(desires, 39, 0.5f);
	Increase(desires, 40, 0.5f);
	Increase(desires, -1, 0.5f);
	IncreaseTown(desires, 16, 0.25f);
	IncreaseTown(desires, 17, 0.25f);
	IncreaseTown(desires, -1, 0.25f);
	PerceivedDesires expected;
	expected.player[39] = 0.5f;
	expected.town[16] = 0.25f;
	EXPECT_EQ(desires.player, expected.player);
	EXPECT_EQ(desires.town, expected.town);
}

TEST(PerceivedDesires, EveryValueFadesEachTurn)
{
	PerceivedDesires desires;
	desires.player.fill(1.0f);
	desires.town.fill(0.5f);
	Fade(desires);
	for (const auto value : desires.player)
	{
		EXPECT_EQ(value, k_TurnFade);
	}
	for (const auto value : desires.town)
	{
		EXPECT_EQ(value, 0.5f * k_TurnFade);
	}
}

TEST(PerceivedDesires, TheDominantIsTheLastActivatedOneWantedAndThoseFoundAreForgotten)
{
	PerceivedDesires desires;
	Increase(desires, 1, 0.2f);
	Increase(desires, 2, 0.9f);
	Increase(desires, 5, 0.3f);
	desires.player[7] = -0.5f;
	const auto activated = [](size_t desire) { return desire != 5; };
	EXPECT_EQ(TakeDominant(desires, activated), 2u);
	EXPECT_EQ(desires.player[1], 0.0f);
	EXPECT_EQ(desires.player[2], 0.0f);
	// not activated, or not wanted at all: kept and not taken
	EXPECT_FLOAT_EQ(desires.player[5], 0.3f);
	EXPECT_EQ(desires.player[7], -0.5f);
	EXPECT_FALSE(TakeDominant(desires, activated).has_value());
}

TEST(PerceivedDesires, ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell)
{
	EXPECT_TRUE(CanSeePos(0, 0x2AA, false));
	EXPECT_FALSE(CanSeePos(0, 0x2AB, false));
	EXPECT_TRUE(CanSeePos(0x2AA, 0, false));
	// across the turn's end: the difference folds once past a half turn
	EXPECT_TRUE(CanSeePos(0x7FF, 0x2A9, false));
	EXPECT_TRUE(CanSeePos(0, 0x556, false));
	EXPECT_FALSE(CanSeePos(0, 0x555, false));
	// a half turn exactly does not fold
	EXPECT_FALSE(CanSeePos(0, 0x400, false));
	EXPECT_TRUE(CanSeePos(0, 0x400, true));
}

TEST(PerceivedDesires, WhereItLooksOnTheMap)
{
	const float alongX = std::numbers::pi_v<float> / 2.0f;
	EXPECT_TRUE(CanSeePos(alongX, At(105.0f, 105.0f), At(205.0f, 105.0f)));
	EXPECT_FALSE(CanSeePos(alongX, At(105.0f, 105.0f), At(5.0f, 105.0f)));
	// behind it, in its own cell
	EXPECT_TRUE(CanSeePos(alongX, At(105.0f, 105.0f), At(101.0f, 105.0f)));
	EXPECT_FALSE(CanSeePos(alongX, At(105.0f, 105.0f), At(99.0f, 105.0f)));
	// a body yaw of 0 looks along -z
	EXPECT_TRUE(CanSeePos(0.0f, At(105.0f, 105.0f), At(105.0f, 5.0f)));
	EXPECT_FALSE(CanSeePos(0.0f, At(105.0f, 105.0f), At(105.0f, 205.0f)));
}

TEST(PerceivedDesires, ItLooksWhereItsHeadLooksUnlessAtNothingOrWhereItStands)
{
	const glm::vec3 here(100.0f, 3.0f, 100.0f);
	EXPECT_EQ(LookYaw(std::nullopt, here, 0.25f), 0.25f);
	EXPECT_EQ(LookYaw(glm::vec3(0.0f), here, 0.25f), 0.25f);
	EXPECT_EQ(LookYaw(here, here, 0.25f), 0.25f);
	EXPECT_FLOAT_EQ(LookYaw(glm::vec3(150.0f, 0.0f, 100.0f), here, 0.25f), std::numbers::pi_v<float> / 2.0f);
	EXPECT_FLOAT_EQ(LookYaw(glm::vec3(100.0f, 9.0f, 50.0f), here, 0.25f), 0.0f);
}

namespace
{
/// A registry of its own, the player's creature behind a fake leash, and the minds made by the test
class PerceivedDesiresMindTest: public ::testing::Test
{
protected:
	PerceivedDesiresMindTest()
	{
		leash = &static_cast<FakeLeash&>(Locator::leashSystem::emplace<FakeLeash>(log));
		auto& registry = test::creature_world::World::Registry();
		creature = registry.Create();
		registry.Assign<Transform>(creature, glm::vec3(105.0f, 0.0f, 105.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<CreatureMindState>(creature);
		auto& moving = registry.Assign<CreatureLocomotion>(creature);
		// a heading of minus a quarter turn faces +x
		moving.started = true;
		moving.heading = -std::numbers::pi_v<float> / 2.0f;
		leash->playersCreature = creature;
	}

	[[nodiscard]] const CreatureMindState& Mind() const
	{
		return std::as_const(test::creature_world::World::Registry()).Get<CreatureMindState>(creature);
	}

	test::creature_world::World world;
	CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	FakeLeash* leash {nullptr};
	entt::entity creature {entt::null};
	ecs::systems::CreatureMindSystem minds;
};
} // namespace

TEST_F(PerceivedDesiresMindTest, ThePlayersCreatureTakesItTheWantsWhatItSees)
{
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Compassion, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	minds.EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player[1], 0.5f);
	EXPECT_EQ(Mind().perceivedDesires.town[1], 0.5f);
}

TEST_F(PerceivedDesiresMindTest, WhatItCannotSeeChangesNothing)
{
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Compassion, 0.5f, glm::vec3(5.0f, 0.0f, 105.0f));
	minds.EmpathiseWithTownDesire(PlayerNames::PLAYER_ONE, TownDesireInfo::ForWood, 0.5f, glm::vec3(5.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player, PerceivedDesires {}.player);
	EXPECT_EQ(Mind().perceivedDesires.town, PerceivedDesires {}.town);
}

TEST_F(PerceivedDesiresMindTest, APlayerWithNoCreatureOrNoLeashChangesNothing)
{
	leash->playersCreature = std::nullopt;
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Anger, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	Locator::leashSystem::reset();
	minds.EmpathiseWithPlayer(PlayerNames::PLAYER_ONE, CreatureDesires::Anger, 0.5f, glm::vec3(205.0f, 0.0f, 105.0f));
	EXPECT_EQ(Mind().perceivedDesires.player[2], 0.0f);
}

TEST_F(PerceivedDesiresMindTest, AStrokeShowsCompassionAndASlapAngerByHowHard)
{
	minds.ReceiveFeedback(creature, 0.25f);
	minds.ReceiveFeedback(creature, -0.75f);
	// too slight to count
	minds.ReceiveFeedback(creature, 0.01f);
	EXPECT_EQ(Mind().perceivedDesires.player[1], 0.25f);
	EXPECT_EQ(Mind().perceivedDesires.player[2], 0.75f);
}

TEST_F(PerceivedDesiresMindTest, TheGamesHandlerPassesTheTownNeedOnToTheMinds)
{
	const test::RestoreService<Locator::creatureMindSystem> restoreMind;
	CallLog heard;
	Locator::creatureMindSystem::emplace<FakeMind>(heard);
	ecs::creature_mimic::AddMimicEventHandlers(Locator::events::value());
	auto& registry = test::creature_world::World::Registry();
	const auto villager = registry.Create();
	registry.Assign<Transform>(villager, glm::vec3(4.0f, 0.5f, 8.0f), glm::mat3(1.0f), glm::vec3(1.0f));

	ecs::creature_mimic::EmpathiseWithTownDesire(PlayerNames::PLAYER_TWO, TownDesireInfo::ForFood, 0.5f, villager);
	ecs::creature_mimic::EmpathiseWithTownDesire(std::nullopt, TownDesireInfo::ForFood, 0.5f, villager);

	ASSERT_EQ(heard.size(), 1u);
	EXPECT_EQ(heard.front().name, "mind.EmpathiseWithTownDesire");
	EXPECT_EQ(heard.front().args,
	          (std::vector<float> {static_cast<float>(PlayerNames::PLAYER_TWO), 0.0f, 0.5f, 4.0f, 0.5f, 8.0f}));
}
