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

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ObjectMetrics.h"
#include "ECS/PotResource.h"
#include "ECS/ResourceStores.h"
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

namespace
{
/// An object with a 2D radius of 10 at (100, 0, 100) that answers it stores the type when `stores`, as the put-down
/// asks it, with the reach multiplier of anything that is not a pot
ecs::pot_resource::PutDownCandidate StoreAt100(bool stores)
{
	return {.storesType = stores, .centre = {100.0f, 0.0f, 100.0f}, .radius2D = 10.0f};
}
} // namespace

TEST(FoodWood, aPutDownReachesByTheOriginalsMultipliersAndCentre)
{
	// the original's answers, read from the exe: a pot's or a pile's reach multiplier is 2.0, every other object's
	// (a worship site's too) 1.2 as a float; a worship site measures from its local point 12.55 to the side and 26.1 back
	// (docs/wiki/world/objects.md#where-a-resource-put-down-goes)
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::pot_resource::k_PotReachMultiplier), 0x40000000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::pot_resource::k_ObjectReachMultiplier), 0x3F99999Au);
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::object::k_WorshipSiteCentreRight), 0x4148CCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::object::k_WorshipSiteCentreBack), 0x41D0CCCDu);
}

TEST(FoodWood, aPutDownGoesIntoEveryStoreOfItsType)
{
	using ecs::pot_resource::TakesPutDown;
	using ecs::resource_stores::IsStoreForType;
	using ecs::resource_stores::StoreKind;
	const glm::vec3 near {108.0f, 0.0f, 104.0f};
	// wood landing on a workshop goes in, with or without a pile there
	EXPECT_TRUE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::Workshop, ResourceType::Wood, false)), near, ResourceType::Wood));
	// food on a worship site
	EXPECT_TRUE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::WorshipSite, ResourceType::Food, false)), near, ResourceType::Food));
	// wood on a building going up, and on a scaffold of its site
	EXPECT_TRUE(TakesPutDown(StoreAt100(IsStoreForType(StoreKind::Abode, ResourceType::Wood, true)), near, ResourceType::Wood));
	EXPECT_TRUE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::Scaffold, ResourceType::Wood, true)), near, ResourceType::Wood));
	// and anything on a storage pit
	EXPECT_TRUE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::StoragePit, ResourceType::Food, false)), near, ResourceType::Food));
}

TEST(FoodWood, aPutDownOfAnotherTypeOrOutOfReachIsLeftForAPile)
{
	using ecs::pot_resource::PutDownCandidate;
	using ecs::pot_resource::TakesPutDown;
	using ecs::resource_stores::IsStoreForType;
	using ecs::resource_stores::StoreKind;
	const glm::vec3 near {108.0f, 0.0f, 104.0f};
	// the wrong type: food on a workshop, wood on a worship site or a finished building
	EXPECT_FALSE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::Workshop, ResourceType::Food, false)), near, ResourceType::Food));
	EXPECT_FALSE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::WorshipSite, ResourceType::Wood, false)), near, ResourceType::Wood));
	EXPECT_FALSE(
	    TakesPutDown(StoreAt100(IsStoreForType(StoreKind::Abode, ResourceType::Wood, false)), near, ResourceType::Wood));
	// a pot or a pile that stores nothing takes only its own type
	const PutDownCandidate foodPile {.isPot = true,
	                                 .potType = ResourceType::Food,
	                                 .centre = {100.0f, 0.0f, 100.0f},
	                                 .radius2D = 10.0f,
	                                 .reachMultiplier = ecs::pot_resource::k_PotReachMultiplier};
	EXPECT_TRUE(TakesPutDown(foodPile, near, ResourceType::Food));
	EXPECT_FALSE(TakesPutDown(foodPile, near, ResourceType::Wood));
	// the reach is 1.2 x the 2D radius for a store, 12 m here: 11.9 m in, 12.1 m out
	const auto store = StoreAt100(true);
	EXPECT_TRUE(TakesPutDown(store, {111.9f, 0.0f, 100.0f}, ResourceType::Wood));
	EXPECT_FALSE(TakesPutDown(store, {112.1f, 0.0f, 100.0f}, ResourceType::Wood));
	// and twice it for a pot, 20 m
	EXPECT_TRUE(TakesPutDown(foodPile, {100.0f, 0.0f, 119.9f}, ResourceType::Food));
	EXPECT_FALSE(TakesPutDown(foodPile, {100.0f, 0.0f, 120.1f}, ResourceType::Food));
}

TEST(FoodWood, aPutDownIsOfferedToTheObjectsInTheirOrderWhileSomeIsLeft)
{
	using ecs::pot_resource::OfferToEach;
	using ecs::pot_resource::PutDownCandidate;
	// four objects of a cell in their list's order: 0 takes nothing (a villager), 1 is a store that takes 30, 2 a store
	// that takes all it is given, 3 a store after it
	const std::array<int, 4> objects {0, 1, 2, 3};
	const auto describe = [](int object) -> std::optional<PutDownCandidate> {
		if (object == 0)
		{
			return std::nullopt;
		}
		return StoreAt100(true);
	};
	std::vector<std::pair<int, uint32_t>> given;
	const auto give = [&given](int object, uint32_t left) -> uint32_t {
		given.emplace_back(object, left);
		return object == 1 ? std::min(left, 30u) : left;
	};
	const glm::vec3 near {104.0f, 0.0f, 100.0f};
	EXPECT_EQ(OfferToEach(objects, near, ResourceType::Wood, 100, describe, give), 0u);
	// the first store has the whole amount, the second what is left, and the third is never asked
	ASSERT_EQ(given.size(), 2u);
	EXPECT_EQ(given[0], std::make_pair(1, 100u));
	EXPECT_EQ(given[1], std::make_pair(2, 70u));
	// out of every object's reach nothing is given, and all of it is left for a new pile
	given.clear();
	EXPECT_EQ(OfferToEach(objects, {200.0f, 0.0f, 200.0f}, ResourceType::Wood, 100, describe, give), 100u);
	EXPECT_TRUE(given.empty());
}

namespace
{
/// A map cell's two lists as the put-down walks them: the fixed list and the mobile one, each a chain read as it is
/// at the moment (a thing is a number, 0 ends a list)
struct FakeCellLists
{
	std::vector<int> fixed;
	std::vector<int> mobile;
	std::vector<int> reads; ///< every head or next read, in order

	[[nodiscard]] int Next(int object)
	{
		for (const auto* list : {&fixed, &mobile})
		{
			const auto at = std::ranges::find(*list, object);
			if (at != list->end())
			{
				const int next = std::next(at) != list->end() ? *std::next(at) : 0;
				reads.push_back(next);
				return next;
			}
		}
		return 0;
	}
};
} // namespace

TEST(FoodWood, aPileMadeWhileAPutDownWalksACellIsOfferedInItsTurn)
{
	// The original reads the next object of the cell's list only after the one before has been given its part, and
	// the mobile list's head only at the end of the fixed list. A store that makes its pile as it takes (a workshop
	// with none, a worship site without its food pot) puts the pile at the tail of the fixed list, so the pile is
	// offered in its turn, after the fixed objects and before the mobile ones
	// (docs/wiki/world/objects.md#where-a-resource-put-down-goes)
	using ecs::pot_resource::OfferAlongCell;
	using ecs::pot_resource::PutDownCandidate;
	FakeCellLists cell {.fixed = {1, 2}, .mobile = {5}};
	std::vector<int> given;
	const auto describe = [](int) -> std::optional<PutDownCandidate> { return StoreAt100(true); };
	// 1 makes pile 9 and takes 10; the others take 10 each
	const auto give = [&](int object, uint32_t left) -> uint32_t {
		given.push_back(object);
		if (object == 1)
		{
			cell.fixed.push_back(9);
		}
		return std::min(left, 10u);
	};
	const glm::vec3 near {104.0f, 0.0f, 100.0f};
	const auto first = [&cell] { return cell.fixed.empty() ? 0 : cell.fixed.front(); };
	const auto firstMobile = [&cell] { return cell.mobile.empty() ? 0 : cell.mobile.front(); };
	const auto next = [&cell](int object) { return cell.Next(object); };
	EXPECT_EQ(OfferAlongCell(0, first, firstMobile, next, near, ResourceType::Wood, 100, describe, give), 60u);
	EXPECT_EQ(given, (std::vector<int> {1, 2, 9, 5}));

	// a pile made while the walk is already in the mobile list is not reached in this cell
	cell = {.fixed = {}, .mobile = {5, 6}};
	given.clear();
	const auto giveInMobile = [&](int object, uint32_t left) -> uint32_t {
		given.push_back(object);
		if (object == 5)
		{
			cell.fixed.push_back(9);
		}
		return std::min(left, 10u);
	};
	EXPECT_EQ(OfferAlongCell(0, first, firstMobile, next, near, ResourceType::Wood, 100, describe, giveInMobile), 80u);
	EXPECT_EQ(given, (std::vector<int> {5, 6}));

	// the walk stops as soon as nothing is left: the next one is still read after the last give, as the original's
	cell = {.fixed = {1, 2}, .mobile = {}};
	given.clear();
	EXPECT_EQ(OfferAlongCell(0, first, firstMobile, next, near, ResourceType::Wood, 10, describe, give), 0u);
	EXPECT_EQ(given, (std::vector<int> {1}));
	EXPECT_EQ(cell.reads, (std::vector<int> {2}));
}

TEST(FoodWood, aPotOverItsMaximumThatAnswersTheWrappedRoomMakesWhatIsLeftWrapRound)
{
	using ecs::pot_resource::OfferToEach;
	using ecs::pot_resource::PutDownCandidate;
	// the original's pot of maximum 100 holding 250, given 10, answers the wrapped room 100 - 250 (the oracle:
	// 0xFFFFFF6A); 10 less that, as 32 bits, leaves 160 for the next object
	// (docs/wiki/world/objects.md#where-a-resource-put-down-goes)
	constexpr uint32_t k_WrappedRoom = 0xFFFFFF6Au;
	const std::array<int, 2> objects {1, 2};
	const auto describe = [](int) -> std::optional<PutDownCandidate> { return StoreAt100(true); };
	std::vector<std::pair<int, uint32_t>> given;
	const auto give = [&given](int object, uint32_t left) -> uint32_t {
		given.emplace_back(object, left);
		return object == 1 ? k_WrappedRoom : 0u;
	};
	EXPECT_EQ(OfferToEach(objects, {104.0f, 0.0f, 100.0f}, ResourceType::Wood, 10, describe, give), 160u);
	ASSERT_EQ(given.size(), 2u);
	EXPECT_EQ(given[1], std::make_pair(2, 160u));
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

/// With OPENBLACK_GAME_PATH set to the install: the three loose mushrooms' rows and the pot rows' reactions and kinds
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatMushroomsAndPiles)
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
	const auto mobile = [&info](MobileObjectInfo row) -> const GMobileObjectInfo& {
		return info->mobileObject.at(static_cast<size_t>(row));
	};
	// each mushroom is a row of its own kind with a model of its own
	EXPECT_EQ(mobile(MobileObjectInfo::Champi).mobileType, MobileObjectInfo::Champi);
	EXPECT_EQ(mobile(MobileObjectInfo::MagicMushroom).mobileType, MobileObjectInfo::MagicMushroom);
	EXPECT_EQ(mobile(MobileObjectInfo::Toadstool).mobileType, MobileObjectInfo::Toadstool);
	EXPECT_EQ(mobile(MobileObjectInfo::Champi).meshId, MeshId::ObjectChampi);
	EXPECT_EQ(mobile(MobileObjectInfo::MagicMushroom).meshId, MeshId::ObjectMagicMushroom);
	EXPECT_EQ(mobile(MobileObjectInfo::Toadstool).meshId, MeshId::ObjectToadstool);
	// the plain and the magic mushroom weigh 70, the toadstool 200; worth 99, 50 and 150 food
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::Champi).weight, 70.0f);
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::MagicMushroom).weight, 70.0f);
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::Toadstool).weight, 200.0f);
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::Champi).foodValue, 99.0f);
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::MagicMushroom).foodValue, 50.0f);
	EXPECT_FLOAT_EQ(mobile(MobileObjectInfo::Toadstool).foodValue, 150.0f);
	// every pot row starts the food reaction or the wood reaction, by its resource
	for (const auto& row : info->pot)
	{
		const auto expected = row.resourceType == ResourceType::Food ? Reaction::ReactToFood : Reaction::ReactToWood;
		EXPECT_EQ(row.associatedReaction, expected);
	}
	// the piles a put-down makes are food and wood piles; the food and wood pots are plain pots
	const auto pot = [&info](PotInfo row) -> const GPotInfo& { return info->pot.at(static_cast<size_t>(row)); };
	EXPECT_EQ(pot(PotInfo::MagicFood).potType, PotType::PileFood);
	EXPECT_EQ(pot(PotInfo::MagicFood).resourceType, ResourceType::Food);
	EXPECT_EQ(pot(PotInfo::MagicWood).potType, PotType::PileWood);
	EXPECT_EQ(pot(PotInfo::MagicWood).resourceType, ResourceType::Wood);
	EXPECT_EQ(pot(PotInfo::FoodPile).potType, PotType::PileFood);
	EXPECT_EQ(pot(PotInfo::FoodPot).potType, PotType::Pot);
	EXPECT_EQ(pot(PotInfo::WoodPot).potType, PotType::Pot);
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

namespace
{
/// The original game's install, from OPENBLACK_GAME_PATH; absent when it is not set
std::optional<std::filesystem::path> RealGamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// info.dat's table as realInfoDat reads it (after the file's header), or nothing when the file is missing or of
/// another size
std::unique_ptr<InfoConstants> ReadRealInfoDat(const std::filesystem::path& game)
{
	std::ifstream file(game / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	if (data.size() != 0x2C + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	return info;
}

/// Each tribe has sixteen abode rows in the same order, the Celtic ones first
constexpr size_t k_AbodeRowsPerTribe = static_cast<size_t>(AbodeInfo::AfricanHut) - static_cast<size_t>(AbodeInfo::CelticHut);
constexpr size_t k_Tibetan = static_cast<size_t>(Tribe::TIBETAN);

/// The abode row of one kind (named by its Celtic row) for one tribe
const GAbodeInfo& AbodeRow(const InfoConstants& info, size_t tribe, AbodeInfo celticKind)
{
	return info.abode.at(tribe * k_AbodeRowsPerTribe + static_cast<size_t>(celticKind));
}

/// The six house kinds, smallest first
constexpr std::array k_HouseKinds = {AbodeInfo::CelticHut,    AbodeInfo::CelticTent,    AbodeInfo::CelticShackX,
                                     AbodeInfo::CelticShackY, AbodeInfo::CelticTempleX, AbodeInfo::CelticTempleY};

/// The civic kinds that have no cost of their own in the rows below: the totem, storage pit, creche, workshop and
/// graveyard
constexpr std::array k_SmallCivicKinds = {AbodeInfo::CelticTotem, AbodeInfo::CelticStoragePit, AbodeInfo::CelticCreche,
                                          AbodeInfo::CelticWorkshop, AbodeInfo::CelticGraveyard};

/// The number of each mushroom a script file creates: the plain mushroom, the magic mushroom and the toadstool. Each
/// is a CREATE_MOBILEOBJECT("x,z", type, ...) line whose type is the mushroom's mobile object row; a commented-out line
/// is not counted. Nothing when the file is missing.
std::optional<std::array<int, 3>> CountScriptMushrooms(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open())
	{
		return std::nullopt;
	}
	const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	constexpr std::string_view k_Command = "CREATE_MOBILEOBJECT(\"";
	std::array<int, 3> counts {};
	for (size_t at = text.find(k_Command); at != std::string::npos; at = text.find(k_Command, at + 1))
	{
		const size_t lineStart = text.find_last_of('\n', at);
		const size_t from = lineStart == std::string::npos ? 0 : lineStart + 1;
		if (text.substr(from, at - from).find("//") != std::string::npos)
		{
			continue;
		}
		const size_t positionEnd = text.find('"', at + k_Command.size());
		const size_t comma = positionEnd == std::string::npos ? std::string::npos : text.find(',', positionEnd);
		if (comma == std::string::npos)
		{
			continue;
		}
		const int type = std::atoi(text.c_str() + comma + 1);
		const auto first = static_cast<int>(MobileObjectInfo::Champi);
		if (type >= first && type <= static_cast<int>(MobileObjectInfo::Toadstool))
		{
			++counts.at(static_cast<size_t>(type - first));
		}
	}
	return counts;
}

/// The land scripts that place mushrooms, and the ones that place none, with their counts of the plain mushroom, the
/// magic mushroom and the toadstool as the progress rows state them
struct ScriptMushrooms
{
	std::string_view file;
	std::array<int, 3> counts;
};
constexpr std::array k_ScriptMushrooms = {
    ScriptMushrooms {.file = "Scripts/Land1.txt", .counts = {2, 36, 9}},
    ScriptMushrooms {.file = "Scripts/Land2.txt", .counts = {0, 0, 0}},
    ScriptMushrooms {.file = "Scripts/Land3.txt", .counts = {7, 13, 6}},
    ScriptMushrooms {.file = "Scripts/Land4.txt", .counts = {0, 0, 0}},
    ScriptMushrooms {.file = "Scripts/Land5.txt", .counts = {0, 17, 0}},
    ScriptMushrooms {.file = "Scripts/Playgrounds/TwoGods.txt", .counts = {12, 0, 0}},
    // the Firestorm playground's script
    ScriptMushrooms {.file = "Online Maps/mpm_3p_2.map", .counts = {0, 27, 6}},
};

void ExpectScriptMushrooms(const std::filesystem::path& game, size_t kind)
{
	for (const auto& script : k_ScriptMushrooms)
	{
		const auto counts = CountScriptMushrooms(game / script.file);
		ASSERT_TRUE(counts.has_value()) << script.file;
		EXPECT_EQ(counts->at(kind), script.counts.at(kind)) << script.file;
	}
}
} // namespace

/// With OPENBLACK_GAME_PATH set to the install: each tribe's workshop row resolves to the model its data names. There
/// are eight workshop models: the Indian row uses the one stored under an "American" name, and the African row shares
/// the Egyptian one, as the tribe's other African rows share the Egyptian models
/// (docs/wiki/rendering/transparency-and-render-modes.md#a-workshops-flat-fence-drawn-from-one-side-fork-issue-189).
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatWorkshopModels)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	constexpr std::array k_Models = {
	    MeshId::BuildingCelticWorkshop,   MeshId::BuildingEgyptianWorkshop, MeshId::BuildingAztecWorkshop,
	    MeshId::BuildingJapaneseWorkshop, MeshId::BuildingAmericanWorkshop, MeshId::BuildingEgyptianWorkshop,
	    MeshId::BuildingGreekWorkshop,    MeshId::BuildingNorseWorkshop,    MeshId::BuildingTibetanWorkshop};
	for (size_t tribe = 0; tribe < k_Models.size(); ++tribe)
	{
		const auto& row = AbodeRow(*info, tribe, AbodeInfo::CelticWorkshop);
		EXPECT_EQ(row.tribeType, static_cast<Tribe>(tribe)) << tribe;
		EXPECT_EQ(row.meshId, k_Models.at(tribe)) << tribe;
	}
}

/// With OPENBLACK_GAME_PATH set to the install: every tribe's workshop is a civic building of 3 scaffolds and 3500 wood
/// (the Tibetan one 7000), built by up to 6 villagers. A site's wood is the row's wood value
/// (docs/wiki/world/building-sites.md#wood), its builders the row's maximum
/// (docs/wiki/world/building-sites.md#desire-and-progress) and the scaffolds a plan needs the row's count
/// (docs/wiki/world/building-sites.md#the-building-a-scaffold-chooses).
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatWorkshopCost)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	for (size_t tribe = 0; tribe < static_cast<size_t>(Tribe::_COUNT); ++tribe)
	{
		const auto& row = AbodeRow(*info, tribe, AbodeInfo::CelticWorkshop);
		EXPECT_EQ(row.abodeType, AbodeType::Workshop) << tribe;
		EXPECT_NE(static_cast<uint32_t>(row.abodeType) & static_cast<uint32_t>(AbodeType::Civic), 0u) << tribe;
		EXPECT_EQ(row.scaffoldsRequired, 3u) << tribe;
		EXPECT_EQ(row.woodValue, tribe == k_Tibetan ? 7000u : 3500u) << tribe;
		EXPECT_EQ(row.maxVillagerNeededToBuild, 6u) << tribe;
	}
}

/// With OPENBLACK_GAME_PATH set to the install: the scaffolds each house kind needs, by tribe (the hut and the tent one
/// everywhere), as a plan reads them (docs/wiki/world/building-sites.md#the-building-a-scaffold-chooses)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatHouseScaffolds)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	// by tribe: Celtic, African, Aztec, Japanese, Indian, Egyptian, Greek, Norse, Tibetan
	constexpr std::array<std::array<uint32_t, k_HouseKinds.size()>, static_cast<size_t>(Tribe::_COUNT)> k_Scaffolds = {{
	    {1, 1, 1, 2, 2, 2},
	    {1, 1, 1, 1, 2, 2},
	    {1, 1, 1, 2, 2, 1},
	    {1, 1, 2, 2, 2, 1},
	    {1, 1, 2, 2, 2, 1},
	    {1, 1, 2, 1, 2, 2},
	    {1, 1, 1, 2, 2, 2},
	    {1, 1, 2, 2, 2, 1},
	    {1, 1, 1, 2, 2, 2},
	}};
	for (size_t tribe = 0; tribe < k_Scaffolds.size(); ++tribe)
	{
		for (size_t kind = 0; kind < k_HouseKinds.size(); ++kind)
		{
			EXPECT_EQ(AbodeRow(*info, tribe, k_HouseKinds.at(kind)).scaffoldsRequired, k_Scaffolds.at(tribe).at(kind))
			    << tribe << " " << kind;
		}
	}
}

/// With OPENBLACK_GAME_PATH set to the install: every tribe's storage pit, creche, workshop and graveyard need 3
/// scaffolds, a field 4, a village centre 5, a miracle dispenser 6 and a wonder 7
/// (docs/wiki/world/building-sites.md#the-building-a-scaffold-chooses)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatCivicScaffolds)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	constexpr std::array<std::pair<AbodeInfo, uint32_t>, 8> k_Scaffolds = {{
	    {AbodeInfo::CelticStoragePit, 3},
	    {AbodeInfo::CelticCreche, 3},
	    {AbodeInfo::CelticWorkshop, 3},
	    {AbodeInfo::CelticGraveyard, 3},
	    {AbodeInfo::CelticField, 4},
	    {AbodeInfo::CelticTownCentre, 5},
	    {AbodeInfo::CelticSpellDispenser, 6},
	    {AbodeInfo::CelticWonder, 7},
	}};
	for (size_t tribe = 0; tribe < static_cast<size_t>(Tribe::_COUNT); ++tribe)
	{
		for (const auto& [kind, scaffolds] : k_Scaffolds)
		{
			EXPECT_EQ(AbodeRow(*info, tribe, kind).scaffoldsRequired, scaffolds) << tribe << " " << static_cast<int>(kind);
		}
	}
}

/// With OPENBLACK_GAME_PATH set to the install: each tribe's buildings cost their own wood, the row's wood value a site
/// reads (docs/wiki/world/building-sites.md#wood). Houses cost 1000 to 4800, each Tibetan house dearer than the same
/// house of any other tribe; the other tribes' small civic buildings 2000 to 4000; a village centre 6000 (the Tibetan
/// one 12000); a field 2000; a miracle dispenser 5500; a football pitch 6000; a wonder 24000.
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatBuildingWood)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	constexpr auto k_Tribes = static_cast<size_t>(Tribe::_COUNT);
	uint32_t cheapestHouse = UINT32_MAX;
	uint32_t dearestHouse = 0;
	for (const auto kind : k_HouseKinds)
	{
		const uint32_t tibetan = AbodeRow(*info, k_Tibetan, kind).woodValue;
		for (size_t tribe = 0; tribe < k_Tribes; ++tribe)
		{
			const uint32_t wood = AbodeRow(*info, tribe, kind).woodValue;
			cheapestHouse = std::min(cheapestHouse, wood);
			dearestHouse = std::max(dearestHouse, wood);
			if (tribe != k_Tibetan)
			{
				EXPECT_GT(tibetan, wood) << tribe << " " << static_cast<int>(kind);
			}
		}
	}
	EXPECT_EQ(cheapestHouse, 1000u);
	EXPECT_EQ(dearestHouse, 4800u);
	uint32_t cheapestCivic = UINT32_MAX;
	uint32_t dearestCivic = 0;
	for (size_t tribe = 0; tribe < k_Tribes; ++tribe)
	{
		if (tribe != k_Tibetan)
		{
			for (const auto kind : k_SmallCivicKinds)
			{
				const auto& row = AbodeRow(*info, tribe, kind);
				EXPECT_NE(static_cast<uint32_t>(row.abodeType) & static_cast<uint32_t>(AbodeType::Civic), 0u);
				cheapestCivic = std::min(cheapestCivic, row.woodValue);
				dearestCivic = std::max(dearestCivic, row.woodValue);
			}
		}
		EXPECT_EQ(AbodeRow(*info, tribe, AbodeInfo::CelticTownCentre).woodValue, tribe == k_Tibetan ? 12000u : 6000u) << tribe;
		EXPECT_EQ(AbodeRow(*info, tribe, AbodeInfo::CelticField).woodValue, 2000u) << tribe;
		EXPECT_EQ(AbodeRow(*info, tribe, AbodeInfo::CelticSpellDispenser).woodValue, 5500u) << tribe;
		EXPECT_EQ(AbodeRow(*info, tribe, AbodeInfo::CelticFootballPitch).woodValue, 6000u) << tribe;
		EXPECT_EQ(AbodeRow(*info, tribe, AbodeInfo::CelticWonder).woodValue, 24000u) << tribe;
	}
	EXPECT_EQ(cheapestCivic, 2000u);
	EXPECT_EQ(dearestCivic, 4000u);
}

/// With OPENBLACK_GAME_PATH set to the install: every tribe's football pitch row is the same, one pitch model that is
/// not in the tribe's style, 6000 wood (docs/wiki/world/building-sites.md#wood), 8 scaffolds
/// (docs/wiki/world/building-sites.md#the-building-a-scaffold-chooses) and up to 6 builders
/// (docs/wiki/world/building-sites.md#desire-and-progress)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realInfoDatFootballPitch)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto info = ReadRealInfoDat(*game);
	ASSERT_NE(info, nullptr);
	for (size_t tribe = 0; tribe < static_cast<size_t>(Tribe::_COUNT); ++tribe)
	{
		const auto& row = AbodeRow(*info, tribe, AbodeInfo::CelticFootballPitch);
		EXPECT_EQ(row.abodeType, AbodeType::FootballPitch) << tribe;
		EXPECT_EQ(row.meshId, MeshId::BuildingFootballPitch) << tribe;
		EXPECT_EQ(row.woodValue, 6000u) << tribe;
		EXPECT_EQ(row.scaffoldsRequired, 8u) << tribe;
		EXPECT_EQ(row.maxVillagerNeededToBuild, 6u) << tribe;
	}
}

/// With OPENBLACK_GAME_PATH set to the install: only the land scripts place plain mushrooms, through the script's
/// mobile object command (docs/wiki/scripts/map-script-reader.md#the-class-each-mobile-type-makes): 2 on Land 1, 7 on
/// Land 3 and 12 in the Two Gods playground
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realLandScriptsPlainMushrooms)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectScriptMushrooms(*game, 0);
}

/// With OPENBLACK_GAME_PATH set to the install: the land scripts place magic mushrooms through the script's mobile
/// object command (docs/wiki/scripts/map-script-reader.md#the-class-each-mobile-type-makes): 36 on Land 1, 13 on Land
/// 3, 17 on Land 5 and 27 in the Firestorm playground
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realLandScriptsMagicMushrooms)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectScriptMushrooms(*game, 1);
}

/// With OPENBLACK_GAME_PATH set to the install: the land scripts place toadstools through the script's mobile object
/// command (docs/wiki/scripts/map-script-reader.md#the-class-each-mobile-type-makes): 9 on Land 1, 6 on Land 3 and 6 in
/// the Firestorm playground
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(FoodWood, realLandScriptsToadstools)
{
	const auto game = RealGamePath();
	if (!game)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	ExpectScriptMushrooms(*game, 2);
}

/// The pour's raise of the hand starts only for the local player's own casting, with a height to raise by, while the
/// effect is not closing
TEST(FoodWood, onlyTheLocalPlayersOwnPourRaisesTheHand)
{
	using particles::sprinkle::StartsHandRaise;
	EXPECT_TRUE(StartsHandRaise(true, 10.0f, false));
	EXPECT_FALSE(StartsHandRaise(false, 10.0f, false));
	EXPECT_FALSE(StartsHandRaise(true, 0.0f, false));
	EXPECT_FALSE(StartsHandRaise(true, 10.0f, true));
}
