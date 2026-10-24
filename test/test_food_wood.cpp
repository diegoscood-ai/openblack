/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The food and wood miracles: the hand's grain raise (HandStateGrain), SpellResource's cost per grain, the pile
// helpers of Pot::AddResourceToPos and the sprinkle effect's emission (UR_HandSprinkle + UR_WillowWisp).

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/PotResource.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "InfoConstants.h"
#include "Magic/HandMotion.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellResource.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellFile.h"
#include "Particles/Rules/Sprinkle.h"

using namespace openblack;

namespace
{
/// SF_Food.txt without its sound and its sprite details (the classes that make and move the grains)
constexpr std::string_view k_Sprinkle = R"(BEGINPROPERTIES
PROPERTY DeleteOnCloseDown BOOL 0
PROPERTY Hierarchies ARRAY SIZE 25 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY InitiallyCreated ARRAY SIZE 25 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
PROPERTY MaxSpellAge FLOAT -1
ENDPROPERTIES
BEGINCLASS ParticlePointCreator ParticlePointCreator0
BEGINPROPERTIES
PROPERTY InitialScale FLOAT 1
ENDPROPERTIES
ENDCLASS
BEGINCLASS EventConditionTrueWhenEnabled EventConditionTrueWhenEnabled0
BEGINPROPERTIES
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_WillowWisp UR_WillowWisp_HandGlow
BEGINPROPERTIES
PROPERTY AddCastVelToInitPos BOOL 1
PROPERTY DieAge FLOAT 2
PROPERTY EmitConditionOfParent PERSIS_PNTR EventConditionTrueWhenEnabled0
PROPERTY EmitDueToMoving BOOL 1
PROPERTY EmitDueToMovingDist FLOAT 2.5
PROPERTY Group INTEGER 1
PROPERTY MaxAtoms INTEGER 36
PROPERTY MaxSpeed FLOAT 30
PROPERTY NextGroups ARRAY SIZE 0
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY RandomSpeed FLOAT 0
PROPERTY Speed FLOAT 1.0
ENDPROPERTIES
ENDCLASS
BEGINCLASS UR_HandSprinkle UR_HandSprinkle0
BEGINPROPERTIES
PROPERTY NextGroups ARRAY SIZE 1 1
PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0
PROPERTY InitSpeedYHumanPlayerCasting FLOAT -20.0
PROPERTY AngleToRaise FLOAT 1.07257
PROPERTY KeyPoints ARRAY SIZE 8 0 0 0.2 1 0.8 1 1 0
PROPERTY Group INTEGER 0
PROPERTY HeightToRaise FLOAT 10
PROPERTY ClampHand BOOL 1
PROPERTY TotalTime FLOAT 4
ENDPROPERTIES
ENDCLASS
)";

/// A spell behind the effect: a human player's hand cast (not this computer's interface, so the hand is left alone)
class HumanSink final: public particles::SpellSink
{
public:
	int SpellEvent(const particles::SpellEventInfo& /*event*/) override { return 1; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
	[[nodiscard]] bool IsHumanPlayerCasting() const override { return true; }
};

size_t Grains(const particles::Effect& effect)
{
	// every atom but the source one (group 0)
	return effect.AtomCount() - 1;
}
} // namespace

TEST(FoodWood, grainSplineKeyPoints)
{
	// the ctor's ends are natural (1e30): see Magic/HandMotion.h
	const magic::CubicSpline spline(magic::k_PourKeyPoints, 1e30f, 1e30f);
	EXPECT_NEAR(spline(0.0f), 0.0f, 1e-6f);
	EXPECT_NEAR(spline(0.2f), 1.0f, 1e-6f);
	EXPECT_NEAR(spline(0.8f), 1.0f, 1e-6f);
	EXPECT_NEAR(spline(1.0f), 0.0f, 1e-6f);
	// symmetric, and a little over 1 between the two middle key points
	EXPECT_NEAR(spline(0.1f), spline(0.9f), 1e-5f);
	EXPECT_NEAR(spline(0.5f), 1.6136f, 1e-3f);
	// natural ends: the second derivative there is 0, so the slope is not
	EXPECT_NEAR(spline(0.001f) / 0.001f, 5.455f, 0.05f);
}

TEST(FoodWood, grainRaiseLoopsOverTotalTime)
{
	using namespace ecs::systems;
	hand_grain::Reset();
	hand_grain::Start(true, 4.0f, 10.0f, 1.07257f, true);
	const magic::CubicSpline spline(magic::k_PourKeyPoints);
	for (int turn = 1; turn <= 20; ++turn)
	{
		hand_grain::GameTurnUpdate(0.1f);
	}
	// t = 20 x 0.1 / 4 = 0.5 after 2 s: the peak of the raise
	auto state = hand_grain::Debug();
	EXPECT_NEAR(state.x, 0.5f, 1e-4f);
	EXPECT_NEAR(state.y, 10.0f * spline(0.5f), 1e-3f);
	EXPECT_NEAR(state.z, 1.07257f * spline(0.5f), 1e-4f);
	ASSERT_TRUE(hand_grain::ClampedPosition().has_value());
	// past t = 1 it starts again at 0 (loop), height 0
	for (int turn = 21; turn <= 41; ++turn)
	{
		hand_grain::GameTurnUpdate(0.1f);
	}
	state = hand_grain::Debug();
	EXPECT_TRUE(hand_grain::Active());
	EXPECT_NEAR(state.x, 0.0f, 1e-6f);
	EXPECT_NEAR(state.y, 0.0f, 1e-6f);
	hand_grain::Stop();
	EXPECT_FALSE(hand_grain::Active());
	EXPECT_FALSE(hand_grain::ClampedPosition().has_value());
	hand_grain::Reset();
}

TEST(FoodWood, resourceEventCosts)
{
	GMagicResourceInfo food {};
	food.resourceType = ResourceType::Food;
	food.resourceAmountFirstEvent = 200;
	food.resourceAmountPerEvent = 18;
	food.costPerUnit = 7;
	const auto first = magic::ResourceEvent(food, false);
	EXPECT_EQ(first.units, 200u);
	EXPECT_FLOAT_EQ(first.chants, 1400.0f);
	const auto next = magic::ResourceEvent(food, true);
	EXPECT_EQ(next.units, 18u);
	EXPECT_FLOAT_EQ(next.chants, 126.0f);
	EXPECT_TRUE(magic::HasEnoughChantsForResourceRecast(food, 1400.0f));
	EXPECT_FALSE(magic::HasEnoughChantsForResourceRecast(food, 1399.5f));
	GMagicResourceInfo wood {};
	wood.resourceType = ResourceType::Wood;
	wood.resourceAmountFirstEvent = 500;
	wood.resourceAmountPerEvent = 20;
	wood.costPerUnit = 3;
	EXPECT_FLOAT_EQ(magic::ResourceEvent(wood, false).chants, 1500.0f);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(wood, true).chants, 60.0f);
}

TEST(FoodWood, pileHelpers)
{
	using namespace ecs::pot_resource;
	// the pile's sound sample
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 199, 5), 82);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 200, 3), 76);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 18, 6), 77);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 100, 7), 93);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 500, 11), 91);
	// read twice: the food sample takes the first reading and the wood sample the second
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 199, 5, 6), 82);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 199, 5, 6), 92);
	EXPECT_EQ(PileSoundSample(ResourceType::Food, 200, 3, 4), 76);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 200, 3, 4), 90);
	EXPECT_EQ(PileSoundSample(ResourceType::Wood, 500, 2, 11), 91);
	// PileFoodProportionRaised: 200 of 1000 -> p = 0.24 -> 1 - 0.76^2
	EXPECT_NEAR(PileFoodProportionRaised(200, 1000), 1.0f - 0.76f * 0.76f, 1e-6f);
	// an empty pile: p == 0 skips the floor, so 0
	EXPECT_EQ(PileFoodProportionRaised(0, 1000), 0.0f);
	EXPECT_NEAR(PileFoodProportionRaised(5000, 1000), 1.0f, 1e-6f);
}

TEST(FoodWood, sprinkleEmitsAtTheRateEvenWhenStill)
{
	// UR_WillowWisp: max(dt x MaxAtoms / DieAge, moved / EmitDueToMovingDist) atoms per step: 18 per second
	// from a hand that does not move, while the spell is enabled
	const auto file = particles::File::Parse(k_Sprinkle, "SF_FoodTest");
	ASSERT_TRUE(file.has_value());
	// the float array (KeyPoints) does not stop the parse, and the objects after it are read
	const auto* sprinkle = file->Find("UR_HandSprinkle0");
	ASSERT_NE(sprinkle, nullptr);
	const auto& keyPoints = sprinkle->properties.at("KeyPoints").numbers;
	ASSERT_EQ(keyPoints.size(), 8u);
	EXPECT_FLOAT_EQ(keyPoints[2], 0.2f);
	EXPECT_FLOAT_EQ(sprinkle->Float("TotalTime", 0.0f), 4.0f);
	auto shared = std::make_shared<const particles::File>(*file);
	HumanSink sink;
	particles::Effect effect(shared, glm::vec3(100.0f, 30.0f, 100.0f), 1.0f);
	effect.SetSink(&sink);
	particles::ProcessInfo info;
	info.handPos = glm::vec3(100.0f, 30.0f, 100.0f);
	info.enabled = true;
	for (int step = 0; step < 10; ++step)
	{
		effect.SetProcessInfo(info);
		effect.Step(0.1f);
	}
	// 10 x 1.8 = 18 emitted; an atom is made while the total - 1 is above the count
	EXPECT_GE(Grains(effect), 16u);
	EXPECT_LE(Grains(effect), 17u);
	// not enabled (the cast button let go): nothing more
	const auto before = Grains(effect);
	info.enabled = false;
	for (int step = 0; step < 10; ++step)
	{
		effect.SetProcessInfo(info);
		effect.Step(0.1f);
	}
	EXPECT_EQ(Grains(effect), before);
	// moving 25 m in a step: 10 grains in it (25 / 2.5)
	info.enabled = true;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	const auto still = Grains(effect);
	info.handPos.x += 25.0f;
	effect.SetProcessInfo(info);
	effect.Step(0.1f);
	EXPECT_GE(Grains(effect) - still, 9u);
	EXPECT_LE(Grains(effect) - still, 11u);
}

/// With OPENBLACK_GAME_PATH set to the install: the real food and wood rows
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
/// A falling log tumbles by its speed times its tumble speed, kept within its most when it has one, about z when it moves
/// more along x than along z, else about x
TEST(FoodWood, aLogTumblesByItsSpeedAboutTheAxisAcrossItsMovement)
{
	using particles::sprinkle::TumbleAngle;
	using particles::sprinkle::TumblesAboutZ;
	const glm::vec3 v(3.0f, 0.0f, 4.0f); // |v| = 5
	EXPECT_FLOAT_EQ(TumbleAngle(v, 2.0f, std::nullopt, 0.1f), 1.0f);
	EXPECT_FLOAT_EQ(TumbleAngle(v, 2.0f, 4.0f, 0.1f), 0.4f);
	EXPECT_FLOAT_EQ(TumbleAngle(v, -2.0f, 4.0f, 0.1f), -0.4f);
	EXPECT_FLOAT_EQ(TumbleAngle(glm::vec3(0.0f, 2.0f, 0.0f), 1.0f, std::nullopt, 0.5f), 1.0f); // the fall counts too
	EXPECT_FALSE(TumblesAboutZ(v));
	EXPECT_TRUE(TumblesAboutZ(glm::vec3(4.0f, 0.0f, 3.0f)));
	EXPECT_TRUE(TumblesAboutZ(glm::vec3(-4.0f, 0.0f, 3.0f)));
	EXPECT_FALSE(TumblesAboutZ(glm::vec3(3.0f, 0.0f, -3.0f))); // equal: about x
}

TEST(FoodWood, realInfoDat)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto* food = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Food);
	ASSERT_NE(food, nullptr);
	EXPECT_EQ(food->resourceType, ResourceType::Food);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, false).chants, 1400.0f);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, true).chants, 126.0f);
	EXPECT_EQ(food->speedUp, 0u);
	const auto* foodPowerUp = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::FoodPowerUpOne);
	ASSERT_NE(foodPowerUp, nullptr);
	EXPECT_EQ(foodPowerUp->resourceAmountPerEvent, 20u);
	EXPECT_EQ(foodPowerUp->speedUp, 1u);
	const auto* wood = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Wood);
	ASSERT_NE(wood, nullptr);
	EXPECT_EQ(wood->resourceType, ResourceType::Wood);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*wood, false).chants, 1500.0f);
	EXPECT_EQ(magic::ResourceEvent(*wood, true).units, 20u);
	EXPECT_EQ(info->pot[static_cast<size_t>(PotInfo::MagicFood)].maxAmountInPot, 1000u);
	EXPECT_EQ(static_cast<int>(info->pot[static_cast<size_t>(PotInfo::MagicWood)].nextPotForResource), 19);
	// the magic piles a script's create of a store makes never start their physics there
	EXPECT_EQ(info->pot[static_cast<size_t>(PotInfo::MagicFood)].canBecomeAPhysicsObject, 0u);
	EXPECT_EQ(info->pot[static_cast<size_t>(PotInfo::MagicWood)].canBecomeAPhysicsObject, 0u);
}

/// realInfoDat on an info table built here: the food row, its power-up and the wood row are found by their magic type,
/// and each event's units and chants come from the row's own fields. The pot rows' values are the game data's own, so
/// only the original test checks them
TEST(FoodWood, realInfoDatSynthetic)
{
	auto info = std::make_unique<InfoConstants>(); // every row zeroed
	auto& foodRow = info->magicFood[0];
	foodRow.resourceType = ResourceType::Food;
	foodRow.resourceAmountFirstEvent = 100;
	foodRow.resourceAmountPerEvent = 9;
	foodRow.costPerUnit = 4;
	auto& foodPowerUpRow = info->magicFood[1];
	foodPowerUpRow.resourceType = ResourceType::Food;
	foodPowerUpRow.resourceAmountPerEvent = 11;
	foodPowerUpRow.speedUp = 1;
	auto& woodRow = info->magicWood[0];
	woodRow.resourceType = ResourceType::Wood;
	woodRow.resourceAmountFirstEvent = 300;
	woodRow.resourceAmountPerEvent = 13;
	woodRow.costPerUnit = 2;

	const auto* food = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Food);
	ASSERT_EQ(food, &foodRow);
	EXPECT_EQ(food->resourceType, ResourceType::Food);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, false).chants, 400.0f); // 100 x 4
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*food, true).chants, 36.0f);   // 9 x 4
	EXPECT_EQ(food->speedUp, 0u);
	const auto* foodPowerUp = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::FoodPowerUpOne);
	ASSERT_EQ(foodPowerUp, &foodPowerUpRow);
	EXPECT_EQ(foodPowerUp->resourceAmountPerEvent, 11u);
	EXPECT_EQ(foodPowerUp->speedUp, 1u);
	const auto* wood = magic::GetMagicInfoAs<GMagicResourceInfo>(*info, MagicType::Wood);
	ASSERT_EQ(wood, &woodRow);
	EXPECT_EQ(wood->resourceType, ResourceType::Wood);
	EXPECT_FLOAT_EQ(magic::ResourceEvent(*wood, false).chants, 600.0f); // 300 x 2
	EXPECT_EQ(magic::ResourceEvent(*wood, true).units, 13u);
}

namespace
{
/// What a script's create of a store asked for, in order
struct StoreCalls
{
	std::vector<std::pair<ResourceType, uint32_t>> made;
	std::vector<std::pair<ResourceType, uint32_t>> heard;
	int physicsStarted {0};
};

int CreateStore(uint32_t subtype, StoreCalls& calls, bool pileMade, bool canBecomePhysicsObject)
{
	constexpr int k_None = 0;
	constexpr int k_Pile = 42;
	return ecs::pot_resource::CreateScriptStore(
	    k_None, subtype,
	    [&](ResourceType type, uint32_t amount) {
		    calls.made.emplace_back(type, amount);
		    return pileMade ? k_Pile : k_None;
	    },
	    [&](int pile, ResourceType type, uint32_t amount) {
		    EXPECT_EQ(pile, k_Pile);
		    calls.heard.emplace_back(type, amount);
	    },
	    [&](int /*pile*/) { return canBecomePhysicsObject; }, [&](int /*pile*/) { ++calls.physicsStarted; });
}
} // namespace

TEST(FoodWood, aScriptsStoreIsAPileOfOneUnitHeardAsItIsMade)
{
	// sub-type 0 food, 1 wood: one unit, then the pile sound of one unit (a small pile's)
	StoreCalls food;
	EXPECT_EQ(CreateStore(0, food, true, false), 42);
	EXPECT_EQ(food.made, (std::vector<std::pair<ResourceType, uint32_t>> {{ResourceType::Food, 1}}));
	EXPECT_EQ(food.heard, food.made);
	EXPECT_EQ(food.physicsStarted, 0);
	StoreCalls wood;
	EXPECT_EQ(CreateStore(1, wood, true, false), 42);
	EXPECT_EQ(wood.made, (std::vector<std::pair<ResourceType, uint32_t>> {{ResourceType::Wood, 1}}));
	EXPECT_EQ(wood.heard, wood.made);
}

TEST(FoodWood, aScriptsStoreOfAnotherResourceMakesNothing)
{
	StoreCalls calls;
	EXPECT_EQ(CreateStore(2, calls, true, true), 0);
	EXPECT_EQ(CreateStore(5000, calls, true, true), 0);
	EXPECT_TRUE(calls.made.empty());
	EXPECT_TRUE(calls.heard.empty());
	EXPECT_EQ(calls.physicsStarted, 0);
}

TEST(FoodWood, aScriptsStoreStartsItsPhysicsOnlyWhenItsRowLetsIt)
{
	StoreCalls still;
	CreateStore(1, still, true, false);
	EXPECT_EQ(still.physicsStarted, 0);
	StoreCalls moving;
	CreateStore(1, moving, true, true);
	EXPECT_EQ(moving.physicsStarted, 1);
	// no pile made: no sound and no physics
	StoreCalls none;
	EXPECT_EQ(CreateStore(0, none, false, true), 0);
	EXPECT_EQ(none.made.size(), 1u);
	EXPECT_TRUE(none.heard.empty());
	EXPECT_EQ(none.physicsStarted, 0);
}

TEST(HandMultiPickUp, RampsWithTheSquareOfTheTurns)
{
	using openblack::ecs::systems::hand_detail::PickUpAmount;
	const auto at = [](uint32_t perTurn, uint32_t perTurnEnd, float ticks, uint32_t turns) {
		const auto step = PickUpAmount(perTurn, perTurnEnd, ticks, turns);
		return std::pair {step.t, step.amount};
	};
	EXPECT_EQ(at(10, 50, 10.0f, 0), std::pair(0.0f, 10u));
	EXPECT_EQ(at(10, 50, 10.0f, 5), std::pair(0.5f, 20u));
	EXPECT_EQ(at(10, 50, 8.0f, 3), std::pair(0.375f, 15u)); // 10 + 40 x 0.140625, truncated
	EXPECT_EQ(at(10, 50, 10.0f, 10), std::pair(1.0f, 50u));
	EXPECT_EQ(at(10, 50, 10.0f, 20), std::pair(1.0f, 50u));        // t stops at 1
	EXPECT_EQ(at(10, 50, 0.0f, 5), std::pair(1.0f, 50u));          // no ramp time
	EXPECT_EQ(at(10, 50, 0.0f, 0), std::pair(0.0f, 10u));          // 0 / 0 gives 0
	EXPECT_EQ(at(50, 10, 10.0f, 5), std::pair(0.5f, 1073741824u)); // the unsigned difference wraps, as before
}

TEST(HandMultiPickUp, APileGivesEightATurnRisingToSeventyOverSixSeconds)
{
	using openblack::ecs::systems::hand_detail::PickUpAmount;
	// The hand pots' rows in the game's data: 25 taken at the press, then 8 a turn rising to 70 a turn over a ramp of
	// 6 s, 60 turns of 0.1 s
	constexpr uint32_t k_Initially = 25;
	constexpr uint32_t k_PerTurn = 8;
	constexpr uint32_t k_PerTurnEnd = 70;
	constexpr float k_RampTicks = 60.0f;
	struct Step
	{
		uint32_t turn;
		uint32_t taken;
		uint32_t held;
	};
	// what the original takes on those turns, and what the hand holds then: 413 at 3 s, 1746 at 6 s
	constexpr std::array<Step, 8> k_Steps {{
	    {.turn = 1, .taken = 8, .held = 33},
	    {.turn = 10, .taken = 9, .held = 108},
	    {.turn = 20, .taken = 14, .held = 225},
	    {.turn = 30, .taken = 23, .held = 413},
	    {.turn = 40, .taken = 35, .held = 706},
	    {.turn = 50, .taken = 51, .held = 1139},
	    {.turn = 60, .taken = 70, .held = 1746},
	    {.turn = 100, .taken = 70, .held = 4546},
	}};
	uint32_t held = k_Initially;
	size_t next = 0;
	for (uint32_t turn = 1; turn <= k_Steps.back().turn; ++turn)
	{
		const auto step = PickUpAmount(k_PerTurn, k_PerTurnEnd, k_RampTicks, turn);
		held += step.amount;
		if (turn == k_Steps.at(next).turn)
		{
			EXPECT_EQ(step.amount, k_Steps.at(next).taken) << turn;
			EXPECT_EQ(held, k_Steps.at(next).held) << turn;
			++next;
		}
	}
	EXPECT_EQ(next, k_Steps.size());
}
