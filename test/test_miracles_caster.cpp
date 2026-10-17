/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The fake of the miracles below owns real stores, as the game's service does
#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Debug/MiraclesCaster.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Locator.h"
#include "support/MagicFakes.h"

using namespace openblack;
using namespace openblack::debug::miracles;

namespace
{
/// What the miracles window last asked of the miracles
struct Call
{
	MagicType type {MagicType::None};
	PlayerNames player {PlayerNames::NEUTRAL};
	glm::vec3 point {0.0f};
	entt::entity target {entt::null};
	magic::SpellCastData cast;
	psys::ProcessInfo info;
	SpellSeedType seed {SpellSeedType::None};
	int powerUp {0};
	float multiplier {0.0f};
};

/// A fake of the miracles that records the window's calls and answers with fixed entities; the rest is inert
class FakeMagicSystem final: public test::InertMagicSystem
{
public:
	entt::entity CastAtPoint(MagicType type, PlayerNames player, glm::vec3 point, const magic::SpellCastData& cast,
	                         const psys::ProcessInfo& info) override
	{
		castAtPoint = Call {.type = type, .player = player, .point = point, .cast = cast, .info = info};
		return entt::entity {11};
	}
	entt::entity CastOnObject(MagicType type, PlayerNames player, entt::entity target, const magic::SpellCastData& cast,
	                          const psys::ProcessInfo& info) override
	{
		castOnObject = Call {.type = type, .player = player, .target = target, .cast = cast, .info = info};
		return entt::entity {12};
	}
	bool CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) override
	{
		canCastAt = Call {.type = type, .player = player, .point = point};
		return canCast;
	}
	entt::entity CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier) override
	{
		oneOffSeed = Call {.point = position, .seed = seed, .powerUp = powerUp, .multiplier = multiplier};
		return entt::entity {13};
	}
	entt::entity GiveSeedToHand(PlayerNames player, SpellSeedType seed, int powerUp, float multiplier) override
	{
		seedToHand = Call {.player = player, .seed = seed, .powerUp = powerUp, .multiplier = multiplier};
		return entt::entity {14};
	}

	std::optional<Call> castAtPoint;
	std::optional<Call> castOnObject;
	std::optional<Call> canCastAt;
	std::optional<Call> oneOffSeed;
	std::optional<Call> seedToHand;
	bool canCast {false};
};

class MiraclesCasterTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the locator only hands the fake to the window's caster; the test reads the fake it holds
		_fake = &static_cast<FakeMagicSystem&>(Locator::magicSystem::emplace<FakeMagicSystem>());
	}
	void TearDown() override
	{
		// the listener resets the fake
		_fake = nullptr;
	}

	[[nodiscard]] FakeMagicSystem& Fake() const { return *_fake; }

private:
	FakeMagicSystem* _fake {nullptr};
};

CastPlan MakePlan()
{
	CastPlan plan;
	plan.type = MagicType::Heal;
	plan.cast = magic::SpellCastData {.magnitude = 2.5f, .chants = 40.0f, .duration = 12.0f, .maxObjectsToCreate = 3};
	plan.process.handPos = glm::vec3 {1.0f, 2.0f, 3.0f};
	plan.process.cameraForward = glm::vec3 {0.0f, 0.0f, 1.0f};
	plan.process.power = 0.75f;
	return plan;
}

void ExpectSamePlan(const Call& call, const CastPlan& plan)
{
	EXPECT_EQ(call.type, plan.type);
	EXPECT_EQ(call.cast.magnitude, plan.cast.magnitude);
	EXPECT_EQ(call.cast.chants, plan.cast.chants);
	EXPECT_EQ(call.cast.duration, plan.cast.duration);
	EXPECT_EQ(call.cast.maxObjectsToCreate, plan.cast.maxObjectsToCreate);
	EXPECT_EQ(call.info.handPos, plan.process.handPos);
	EXPECT_EQ(call.info.cameraForward, plan.process.cameraForward);
	EXPECT_EQ(call.info.power, plan.process.power);
}
} // namespace

TEST_F(MiraclesCasterTest, CastAtPointHandsThePlanToTheMiraclesWithTheWorldPoint)
{
	GameSpellCaster caster;
	const auto plan = MakePlan();
	const glm::vec3 point {100.0f, 5.0f, 200.0f};

	EXPECT_EQ(caster.CastAtPoint(plan, PlayerNames::PLAYER_TWO, point), entt::entity {11});

	ASSERT_TRUE(Fake().castAtPoint.has_value());
	ExpectSamePlan(*Fake().castAtPoint, plan);
	EXPECT_EQ(Fake().castAtPoint->player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(Fake().castAtPoint->point, point);
	EXPECT_FALSE(Fake().castOnObject.has_value());
}

TEST_F(MiraclesCasterTest, CastOnObjectHandsThePlanAndTheTarget)
{
	GameSpellCaster caster;
	const auto plan = MakePlan();

	EXPECT_EQ(caster.CastOnObject(plan, PlayerNames::PLAYER_ONE, entt::entity {7}), entt::entity {12});

	ASSERT_TRUE(Fake().castOnObject.has_value());
	ExpectSamePlan(*Fake().castOnObject, plan);
	EXPECT_EQ(Fake().castOnObject->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(Fake().castOnObject->target, entt::entity {7});
	EXPECT_FALSE(Fake().castAtPoint.has_value());
}

TEST_F(MiraclesCasterTest, CanCastAtAnswersAsTheMiraclesDo)
{
	const GameSpellCaster caster;
	const glm::vec3 point {10.0f, 0.0f, 20.0f};

	Fake().canCast = false;
	EXPECT_FALSE(caster.CanCastAt(MagicType::Shield, PlayerNames::PLAYER_THREE, point));
	Fake().canCast = true;
	EXPECT_TRUE(caster.CanCastAt(MagicType::Shield, PlayerNames::PLAYER_THREE, point));

	ASSERT_TRUE(Fake().canCastAt.has_value());
	EXPECT_EQ(Fake().canCastAt->type, MagicType::Shield);
	EXPECT_EQ(Fake().canCastAt->player, PlayerNames::PLAYER_THREE);
	EXPECT_EQ(Fake().canCastAt->point, point);
}

TEST_F(MiraclesCasterTest, OneShotIsABubbleAtThePointAtTheNormalMultiplier)
{
	GameDispenserCreator creator;
	const glm::vec3 point {30.0f, 4.0f, 50.0f};

	EXPECT_EQ(creator.CreateOneShot(SpellSeedType::Water, 1, point), entt::entity {13});

	ASSERT_TRUE(Fake().oneOffSeed.has_value());
	EXPECT_EQ(Fake().oneOffSeed->point, point);
	EXPECT_EQ(Fake().oneOffSeed->seed, SpellSeedType::Water);
	EXPECT_EQ(Fake().oneOffSeed->powerUp, 1);
	EXPECT_EQ(Fake().oneOffSeed->multiplier, 1.0f);
	EXPECT_FALSE(Fake().seedToHand.has_value());
}

TEST_F(MiraclesCasterTest, OneShotInHandGoesToThePlayersHandAtTheNormalMultiplier)
{
	GameDispenserCreator creator;

	EXPECT_EQ(creator.CreateOneShotInHand(SpellSeedType::Heal, -1, PlayerNames::PLAYER_ONE), entt::entity {14});

	ASSERT_TRUE(Fake().seedToHand.has_value());
	EXPECT_EQ(Fake().seedToHand->player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(Fake().seedToHand->seed, SpellSeedType::Heal);
	EXPECT_EQ(Fake().seedToHand->powerUp, -1);
	EXPECT_EQ(Fake().seedToHand->multiplier, 1.0f);
	EXPECT_FALSE(Fake().oneOffSeed.has_value());
}
