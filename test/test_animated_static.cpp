/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <initializer_list>
#include <memory>
#include <optional>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Scenery/AnimatedStaticRules.h"
#include "support/MapFakes.h"

// Enable this define because we use a custom locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/AnimatedStaticSystem.h"

using namespace openblack;
using namespace openblack::animated_static;
using namespace openblack::ecs::components;

// The rules' and the system's cases of raffclar's test/test_animated_static.cpp (0222f9f5); his ObjectDrawList is not
// ported (ours is the land's object draw list)

namespace
{
// A clip like the gate's: a hundred keyframes over ten seconds
constexpr uint32_t k_PlayTime = 10000;
constexpr size_t k_Frames = 100;
} // namespace

TEST(AnimatedStaticRules, AnOpenClipRestsTwoKeyframesShortOfItsEnd)
{
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, k_Frames), 9800u);
	// The phone box's ten keyframes over a second
	EXPECT_EQ(OpenRestingPlace(1000, 10), 800u);
	// The cave entrance's 21 over 2133 ms, in whole milliseconds
	EXPECT_EQ(OpenRestingPlace(2133, 21), 1929u);
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, 0), 0u);
	EXPECT_EQ(OpenRestingPlace(k_PlayTime, 1), 0u);
}

TEST(AnimatedStaticRules, OpenItPlaysOnToItsRestAndClosedBackToTheStart)
{
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_EQ(StepClip(k_Open, 0, 30, rest), 30u);
	EXPECT_EQ(StepClip(k_Open, 9790, 30, rest), rest);
	EXPECT_EQ(StepClip(k_Open, rest, 30, rest), rest);
	EXPECT_EQ(StepClip(k_Closed, rest, 30, rest), rest - 30);
	EXPECT_EQ(StepClip(k_Closed, 20, 30, rest), 0u);
	EXPECT_EQ(StepClip(k_Closed, 0, 30, rest), 0u);
	// Any other word plays it back as closing does
	EXPECT_EQ(StepClip(2, 100, 30, rest), 70u);
	// Paused, nothing moves
	EXPECT_EQ(StepClip(k_Open, 500, 0, rest), 500u);
}

TEST(AnimatedStaticRules, ItIsMovingUnlessAtRestForItsState)
{
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_FALSE(IsMoving(k_Closed, 0, rest));
	EXPECT_TRUE(IsMoving(k_Closed, 500, rest));
	// Closed but still standing open: about to close
	EXPECT_TRUE(IsMoving(k_Closed, rest, rest));
	EXPECT_FALSE(IsMoving(k_Open, rest, rest));
	EXPECT_TRUE(IsMoving(k_Open, 500, rest));
	// Opened but not yet begun
	EXPECT_TRUE(IsMoving(k_Open, 0, rest));
	// Another word rests at either end
	EXPECT_FALSE(IsMoving(2, 0, rest));
	EXPECT_FALSE(IsMoving(2, rest, rest));
	EXPECT_TRUE(IsMoving(2, 500, rest));
}

TEST(AnimatedStaticRules, TheCarvedStonesAreGateStonesAndTheRockIsNot)
{
	EXPECT_TRUE(IsGateStoneKind(MobileStaticInfo::GateTotemApe));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::GateTotemBlank));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::GateTotemCow));
	EXPECT_FALSE(IsGateStoneKind(MobileStaticInfo::RockChalk));
}

TEST(AnimatedStaticRules, StonesFillTheSlotsInOrderAndCountByTheirCarving)
{
	GateStones stones {};
	EXPECT_EQ(GateStoneValue(stones), 0u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemTiger));
	EXPECT_EQ(GateStoneValue(stones), 2u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemApe));
	EXPECT_EQ(GateStoneValue(stones), 3u);
	EXPECT_TRUE(AddGateStone(stones, MeshId::ObjectGateTotemCow));
	EXPECT_EQ(GateStoneValue(stones), 7u);
	EXPECT_EQ(stones[0], MeshId::ObjectGateTotemTiger);
	EXPECT_EQ(stones[1], MeshId::ObjectGateTotemApe);
	EXPECT_EQ(stones[2], MeshId::ObjectGateTotemCow);
	// Full, it takes no more
	EXPECT_FALSE(AddGateStone(stones, MeshId::ObjectGateTotemApe));
	EXPECT_EQ(GateStoneValue(stones), 7u);
	// A model that isn't a carved stone is worth nothing
	GateStones blank {MeshId::ObjectGateTotemBlank};
	EXPECT_EQ(GateStoneValue(blank), 0u);
}

TEST(AnimatedStaticRules, EachThingCollidesWithTheModelForItsState)
{
	const GateStones none {};
	const GateStones one {MeshId::ObjectGateTotemTiger};
	const GateStones two {MeshId::ObjectGateTotemTiger, MeshId::ObjectGateTotemApe};
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::NorseGate, k_Closed, none), MeshId::NorseGatePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::NorseGate, k_Open, none), MeshId::NorseGatePhys2);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, none), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, one), MeshId::GateTotemPlinthePhys2);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Closed, two), MeshId::GateTotemPlinthePhys3);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::GateStonePlinth, k_Open, two), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::PhoneBox, k_Closed, none), MeshId::GateTotemPlinthePhys1);
	EXPECT_EQ(CollisionModel(AnimatedStaticInfo::PiperCaveEntrance, k_Open, none), MeshId::PiperEntrancePhys1);
	EXPECT_FALSE(CollisionModel(AnimatedStaticInfo::ChessKingTeamA, k_Closed, none).has_value());
}

TEST(AnimatedStaticRules, WhatFliesHitsTheGateThePlinthTheCaveAndThePhoneBoxOnly)
{
	// the first three rows and the phone box's (15), whatever their state; the twelve chess pieces never
	EXPECT_TRUE(InTheWayOfWhatFlies(AnimatedStaticInfo::NorseGate));
	EXPECT_TRUE(InTheWayOfWhatFlies(AnimatedStaticInfo::GateStonePlinth));
	EXPECT_TRUE(InTheWayOfWhatFlies(AnimatedStaticInfo::PiperCaveEntrance));
	EXPECT_TRUE(InTheWayOfWhatFlies(AnimatedStaticInfo::PhoneBox));
	for (auto row = static_cast<int32_t>(AnimatedStaticInfo::ChessPionTeamA);
	     row <= static_cast<int32_t>(AnimatedStaticInfo::ChessKingTeamB); ++row)
	{
		EXPECT_FALSE(InTheWayOfWhatFlies(static_cast<AnimatedStaticInfo>(row)));
	}
	// its body's mass is 10000 for every kind
	EXPECT_FLOAT_EQ(k_BodyMass, 10000.0f);
}

TEST(AnimatedStaticRules, ClosedThePlinthStacksItsStonesOnItsTop)
{
	const PlinthLook look {.openState = k_Closed,
	                       .moving = false,
	                       .place = 0,
	                       .playTime = k_PlayTime,
	                       .plinthHalfHeight = 1.5f,
	                       .stoneHalfHeights = {1.0f, std::nullopt, 0.5f}};
	const auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	// On the plinth's top, sat 0.2 into it; the third slot rises by twice its own height
	EXPECT_EQ(draws[0].slot, 0u);
	EXPECT_FLOAT_EQ(draws[0].lift, 2.8f);
	EXPECT_TRUE(draws[0].pickable);
	EXPECT_EQ(draws[1].slot, 2u);
	EXPECT_FLOAT_EQ(draws[1].lift, 4.8f);
	EXPECT_TRUE(draws[1].pickable);
}

TEST(AnimatedStaticRules, OpeningThePlinthSinksItsStonesWithItsClip)
{
	PlinthLook look {.openState = k_Open,
	                 .moving = true,
	                 .place = 0,
	                 .playTime = k_PlayTime,
	                 .plinthHalfHeight = 1.5f,
	                 .stoneHalfHeights = {1.0f, 1.0f, std::nullopt}};
	auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	EXPECT_FLOAT_EQ(draws[0].lift, 2.8f);
	EXPECT_FLOAT_EQ(draws[1].lift, 4.8f);
	EXPECT_FALSE(draws[0].pickable);
	// Half way through the sinking, each has gone down by 7.15 of its own heights
	look.place = static_cast<uint32_t>((k_PlayTime - 3) / 2);
	draws = PlinthStoneDraws(look);
	const float half = static_cast<float>(look.place) / static_cast<float>(k_PlayTime - 3);
	EXPECT_FLOAT_EQ(draws[0].lift, 3.0f - (7.15f * 2.0f * half) - 0.2f);
	EXPECT_FLOAT_EQ(draws[1].lift, (3.0f + (2.0f - (7.15f * 2.0f * half))) - 0.2f);
	// At rest open, they are gone
	look.moving = false;
	look.place = OpenRestingPlace(k_PlayTime, k_Frames);
	EXPECT_TRUE(PlinthStoneDraws(look).empty());
}

TEST(AnimatedStaticRules, TheGateIsARowOfFifteenSmallCirclesFromItsMiddleOutwards)
{
	// A gate 30 wide (half width 15) of size 1, across the x axis: a step of 2 between circles
	const auto circles = GateRouteCircles(glm::vec2(100.0f, 50.0f), glm::vec2(1.0f, 0.0f), 15.0f, 1.0f, false);
	ASSERT_EQ(circles.size(), k_GateRouteCircles);
	const std::array<float, k_GateRouteCircles> along {0.0f,  2.0f,  -2.0f,  4.0f,  -4.0f,  6.0f,  -6.0f, 8.0f,
	                                                   -8.0f, 10.0f, -10.0f, 12.0f, -12.0f, 14.0f, -14.0f};
	for (size_t i = 0; i < circles.size(); ++i)
	{
		EXPECT_NEAR(circles[i].centre.x, 100.0f + along.at(i), 1e-4f) << i;
		EXPECT_FLOAT_EQ(circles[i].centre.y, 50.0f) << i;
		EXPECT_FLOAT_EQ(circles[i].radius, k_GateRouteRadius) << i;
	}
}

TEST(AnimatedStaticRules, AnOpenStillGateLeavesOutItsMiddleFive)
{
	const auto circles = GateRouteCircles(glm::vec2(0.0f), glm::vec2(0.0f, 1.0f), 15.0f, 1.0f, true);
	ASSERT_EQ(circles.size(), k_GateRouteCircles - k_GateRouteGap);
	// The first left is the third step out, then they alternate on outwards
	EXPECT_NEAR(circles.front().centre.y, 6.0f, 1e-4f);
	EXPECT_NEAR(circles.at(1).centre.y, -6.0f, 1e-4f);
	EXPECT_NEAR(circles.back().centre.y, -14.0f, 1e-4f);
	for (const auto& circle : circles)
	{
		EXPECT_GE(std::abs(circle.centre.y), 6.0f - 1e-4f);
	}
}

TEST(AnimatedStaticRules, AResizedGateSpreadsItsCirclesByItsSizeTwice)
{
	// The across direction comes sized as the gate is, and the step is sized again
	const auto circles = GateRouteCircles(glm::vec2(0.0f), glm::vec2(2.0f, 0.0f), 15.0f, 2.0f, false);
	ASSERT_EQ(circles.size(), k_GateRouteCircles);
	EXPECT_NEAR(circles.at(1).centre.x, 8.0f, 1e-4f);
	EXPECT_NEAR(circles.back().centre.x, -56.0f, 1e-3f);
	EXPECT_FLOAT_EQ(circles.back().radius, k_GateRouteRadius);
}

TEST(AnimatedStaticRules, TheGatesMiddleIsItsPositionOnTheMapsFixedGrid)
{
	// 1000.00005 m is 6553600.5 units, truncated to 6553600, which is 1000 m; 123.45678 m is 809086.375 units, so
	// 809086, which is 123.456726 m. The height is not part of it
	const auto middle = GateRouteMiddle(glm::vec3(1000.00005f, 37.0f, 123.45678f));
	EXPECT_EQ(middle.x, 1000.0f);
	EXPECT_EQ(middle.y, 123.45672607421875f);
	// the fraction of a unit is dropped towards 0: 0.0001 m is 0.65536 units, so 0, on either side
	EXPECT_EQ(GateRouteMiddle(glm::vec3(0.0001f, 0.0f, -0.0001f)), glm::vec2(0.0f));
	// the circles start from it
	const auto circles = GateRouteCircles(middle, glm::vec2(1.0f, 0.0f), 15.0f, 1.0f, false);
	EXPECT_EQ(circles.front().centre, middle);
}

TEST(AnimatedStaticRules, AnyOtherAnimatedStaticIsOneCircleOfSevenTenthsOfItsModelsHalfDiagonal)
{
	const auto circle = ModelRouteCircle(glm::vec2(120.0f, -35.5f), 10.0f);
	EXPECT_FLOAT_EQ(circle.centre.x, 120.0f);
	EXPECT_FLOAT_EQ(circle.centre.y, -35.5f);
	EXPECT_FLOAT_EQ(circle.radius, 7.0f);
	// the product as the game makes it, in single precision
	EXPECT_EQ(ModelRouteCircle(glm::vec2(0.0f), 3.3f).radius, 3.3f * 0.7f);
}

TEST(AnimatedStaticRules, AClosingPlinthDrawsItsStonesBothSinkingAndStacked)
{
	const PlinthLook look {.openState = k_Closed,
	                       .moving = true,
	                       .place = 5000,
	                       .playTime = k_PlayTime,
	                       .plinthHalfHeight = 1.0f,
	                       .stoneHalfHeights = {1.0f, std::nullopt, std::nullopt}};
	const auto draws = PlinthStoneDraws(look);
	ASSERT_EQ(draws.size(), 2u);
	EXPECT_FALSE(draws[0].pickable);
	EXPECT_TRUE(draws[1].pickable);
	EXPECT_LT(draws[0].lift, draws[1].lift);
}

class AnimatedStaticSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		// As in the game's tables, the carved stones share the ape stone's kind; the rock has its own
		for (const auto stone :
		     {MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemCow, MobileStaticInfo::GateTotemTiger})
		{
			_info->mobileStatic.at(static_cast<size_t>(stone)).mobileType = MobileStaticInfo::GateTotemApe;
		}
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemBlank)).mobileType =
		    MobileStaticInfo::GateTotemBlank;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemApe)).meshId = MeshId::ObjectGateTotemApe;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemCow)).meshId = MeshId::ObjectGateTotemCow;
		_info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::GateTotemTiger)).meshId = MeshId::ObjectGateTotemTiger;
		Locator::infoConstants::emplace(*_info);
	}
	void TearDown() override
	{
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity Still(AnimatedStaticInfo type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<AnimatedStatic>(entity, AnimatedStatic {.type = type});
		return entity;
	}
	/// An animated static as the archetype makes one with a clip: at the clip's start, resting at `restingPlace` when open
	entt::entity Posed(AnimatedStaticInfo type, uint32_t restingPlace)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = Still(type);
		registry.Assign<AnimatedStaticPose>(entity, AnimatedStaticPose {.restingPlace = restingPlace});
		auto& animation = registry.Assign<SkeletalAnimation>(entity);
		animation.speed = 0.0f;
		return entity;
	}
	entt::entity Stone(MobileStaticInfo type)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<MobileStatic>(entity, type);
		return entity;
	}

	std::unique_ptr<InfoConstants> _info;
	ecs::systems::AnimatedStaticSystem _system;
};

TEST_F(AnimatedStaticSystemTest, AScriptOpensAndClosesOnlyAnimatedStatics)
{
	const auto gate = Still(AnimatedStaticInfo::NorseGate);
	EXPECT_TRUE(_system.SetOpenState(gate, k_Open));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimatedStatic>(gate).openState, k_Open);
	EXPECT_TRUE(_system.SetOpenState(gate, k_Closed));
	EXPECT_EQ(Locator::entitiesRegistry::value().Get<const AnimatedStatic>(gate).openState, k_Closed);
	EXPECT_FALSE(_system.SetOpenState(Stone(MobileStaticInfo::GateTotemApe), k_Open));
	EXPECT_FALSE(_system.SetOpenState(entt::null, k_Open));
}

TEST_F(AnimatedStaticSystemTest, ThePlinthTakesTheCarvedStonesAndCountsThem)
{
	const auto plinth = Still(AnimatedStaticInfo::GateStonePlinth);
	EXPECT_EQ(_system.GateStoneValue(plinth), 0u);
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemTiger)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 2u);
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemApe)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 3u);
	// The uncarved rock isn't taken
	EXPECT_FALSE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemBlank)));
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemCow)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 7u);
	// A full plinth still takes a stone, which is lost
	EXPECT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemApe)));
	EXPECT_EQ(_system.GateStoneValue(plinth), 7u);
}

TEST_F(AnimatedStaticSystemTest, OnlyThePlinthTakesStones)
{
	const auto gate = Still(AnimatedStaticInfo::NorseGate);
	EXPECT_FALSE(_system.LayGateStone(gate, Stone(MobileStaticInfo::GateTotemTiger)));
	EXPECT_EQ(_system.GateStoneValue(gate), 0u);
	EXPECT_FALSE(_system.GateStoneValue(Stone(MobileStaticInfo::GateTotemTiger)).has_value());
}

TEST_F(AnimatedStaticSystemTest, ADrawnOpenGatePlaysOnToItsRestAndAnUndrawnOneKeepsItsPlace)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	const auto drawn = Posed(AnimatedStaticInfo::NorseGate, rest);
	const auto undrawn = Posed(AnimatedStaticInfo::NorseGate, rest);
	ASSERT_TRUE(_system.SetOpenState(drawn, k_Open));
	ASSERT_TRUE(_system.SetOpenState(undrawn, k_Open));

	_system.Draw(drawn, 30);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(drawn).place, 30u);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(drawn).time, 30.0f);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(undrawn).place, 0u);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(undrawn).time, 0.0f);

	// It comes to rest two keyframes short of the clip's end, and stays there while it is drawn
	_system.Draw(drawn, rest);
	_system.Draw(drawn, 30);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(drawn).place, rest);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(drawn).time, static_cast<float>(rest));
	EXPECT_FALSE(IsMoving(k_Open, registry.Get<const AnimatedStaticPose>(drawn).place, rest));
}

TEST_F(AnimatedStaticSystemTest, ADrawnClosedPlinthPlaysBackToTheStart)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto rest = OpenRestingPlace(k_PlayTime, k_Frames);
	const auto plinth = Posed(AnimatedStaticInfo::GateStonePlinth, rest);
	registry.Get<AnimatedStaticPose>(plinth).place = 100;

	_system.Draw(plinth, 30);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(plinth).place, 70u);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(plinth).time, 70.0f);
	_system.Draw(plinth, 100);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(plinth).place, 0u);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(plinth).time, 0.0f);
	// A frame of no game time (paused) leaves it where it is
	_system.Draw(plinth, 0);
	EXPECT_EQ(registry.Get<const AnimatedStaticPose>(plinth).place, 0u);
}

TEST_F(AnimatedStaticSystemTest, DrawingAnythingElseDoesNothing)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = Stone(MobileStaticInfo::GateTotemApe);
	registry.Assign<SkeletalAnimation>(stone);
	_system.Draw(stone, 30);
	EXPECT_FLOAT_EQ(registry.Get<const SkeletalAnimation>(stone).time, 0.0f);
	_system.Draw(entt::null, 30);
}

// The stones a plinth draws, as models of their own (the gate stone plinth's case of the animated statics' Draw)
class PlinthStonesTest: public AnimatedStaticSystemTest
{
protected:
	static inline const glm::vec3 k_At {100.0f, 20.0f, -40.0f};
	static constexpr float k_Size = 1.5f;

	void SetUp() override
	{
		AnimatedStaticSystemTest::SetUp();
		test::FakeMeshBoxProvider boxes;
		boxes.meshBox = [](entt::id_type id) -> std::optional<AxisAlignedBoundingBox> {
			// The plinth 3 high, the ape stone 2, the cow stone 2 and the tiger stone 1
			if (id == resources::HashIdentifier(MeshId::ObjectGateTotemPlinthe))
			{
				return AxisAlignedBoundingBox {{-2.0f, 0.0f, -2.0f}, {2.0f, 3.0f, 2.0f}};
			}
			if (id == resources::HashIdentifier(MeshId::ObjectGateTotemApe) ||
			    id == resources::HashIdentifier(MeshId::ObjectGateTotemCow))
			{
				return AxisAlignedBoundingBox {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
			}
			if (id == resources::HashIdentifier(MeshId::ObjectGateTotemTiger))
			{
				return AxisAlignedBoundingBox {{-1.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
			}
			return std::nullopt;
		};
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(boxes);
	}
	void TearDown() override
	{
		Locator::meshBoxProvider::reset();
		AnimatedStaticSystemTest::TearDown();
	}

	/// A plinth as the archetype makes one, at its clip's start, with so many stones laid in it
	entt::entity Plinth(std::initializer_list<MobileStaticInfo> stones)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto plinth = Posed(AnimatedStaticInfo::GateStonePlinth, OpenRestingPlace(k_PlayTime, k_Frames));
		registry.Get<AnimatedStaticPose>(plinth).playTime = k_PlayTime;
		registry.Assign<Transform>(plinth, k_At, glm::mat3(glm::rotate(glm::mat4(1.0f), 0.5f, glm::vec3(0.0f, 1.0f, 0.0f))),
		                           glm::vec3(k_Size));
		registry.Assign<Mesh>(plinth, resources::HashIdentifier(MeshId::ObjectGateTotemPlinthe), static_cast<int8_t>(0),
		                      static_cast<int8_t>(1));
		for (const auto stone : stones)
		{
			EXPECT_TRUE(_system.LayGateStone(plinth, Stone(stone)));
		}
		return plinth;
	}
	/// The look PlinthStoneDraws is given for the plinth as it stands, with the fake boxes' half heights
	static PlinthLook LookOf(entt::entity plinth)
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto& still = registry.Get<const AnimatedStatic>(plinth);
		const auto& pose = registry.Get<const AnimatedStaticPose>(plinth);
		PlinthLook look {.openState = still.openState,
		                 .moving = IsMoving(still.openState, pose.place, pose.restingPlace),
		                 .place = pose.place,
		                 .playTime = pose.playTime,
		                 .plinthHalfHeight = 1.5f};
		for (size_t slot = 0; slot < k_GateStoneSlots; ++slot)
		{
			if (const auto& stone = still.gateStones.at(slot); stone.has_value())
			{
				look.stoneHalfHeights.at(slot) = *stone == MeshId::ObjectGateTotemTiger ? 0.5f : 1.0f;
			}
		}
		return look;
	}
	/// Each drawn stone is a model of its own: its slot's stone, raised by its lift over the plinth, turned and sized
	/// as the plinth is
	void ExpectStonesAsDrawn(entt::entity plinth)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto draws = PlinthStoneDraws(LookOf(plinth));
		const auto& stones = registry.Get<const AnimatedStaticPose>(plinth).stones;
		ASSERT_EQ(stones.size(), draws.size());
		EXPECT_EQ(registry.Size<PlinthStone>(), draws.size());
		const auto& still = registry.Get<const AnimatedStatic>(plinth);
		const auto& at = registry.Get<const Transform>(plinth);
		for (size_t i = 0; i < draws.size(); ++i)
		{
			ASSERT_TRUE(registry.Valid(stones[i])) << i;
			EXPECT_EQ(registry.Get<const PlinthStone>(stones[i]).plinth, plinth) << i;
			EXPECT_FALSE(registry.Get<const PlinthStone>(stones[i]).pickable) << i;
			EXPECT_EQ(registry.Get<const Mesh>(stones[i]).id, resources::HashIdentifier(*still.gateStones.at(draws[i].slot)))
			    << i;
			const auto& transform = registry.Get<const Transform>(stones[i]);
			EXPECT_EQ(transform.position, at.position + glm::vec3(0.0f, draws[i].lift, 0.0f)) << i;
			EXPECT_EQ(transform.rotation, at.rotation) << i;
			EXPECT_EQ(transform.scale, at.scale) << i;
		}
	}
	[[nodiscard]] static float LiftOf(entt::entity stone)
	{
		return Locator::entitiesRegistry::value().Get<const Transform>(stone).position.y - k_At.y;
	}
};

TEST_F(PlinthStonesTest, ClosedItShowsEachStoneLaidStackedOnItsTop)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto one = Plinth({MobileStaticInfo::GateTotemApe});
	_system.Draw(one, 30);
	ExpectStonesAsDrawn(one);
	ASSERT_EQ(registry.Get<const AnimatedStaticPose>(one).stones.size(), 1u);
	// On the plinth's top (its whole height, unscaled), sat 0.2 into it
	EXPECT_NEAR(LiftOf(registry.Get<const AnimatedStaticPose>(one).stones[0]), 2.8f, 1e-5f);
	_system.EndDrawnFrame();
	registry.Destroy(one);
	_system.EndDrawnFrame();
	EXPECT_EQ(registry.Size<PlinthStone>(), 0u);

	const auto three =
	    Plinth({MobileStaticInfo::GateTotemTiger, MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemCow});
	_system.Draw(three, 30);
	ExpectStonesAsDrawn(three);
	const auto& stones = registry.Get<const AnimatedStaticPose>(three).stones;
	ASSERT_EQ(stones.size(), 3u);
	// Each slot rises by twice its own stone's height
	EXPECT_NEAR(LiftOf(stones[0]), 2.8f, 1e-5f);
	EXPECT_NEAR(LiftOf(stones[1]), 4.8f, 1e-5f);
	EXPECT_NEAR(LiftOf(stones[2]), 6.8f, 1e-5f);
}

TEST_F(PlinthStonesTest, ALaidStoneIsShownFromTheNextDraw)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth = Plinth({MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemTiger});
	_system.Draw(plinth, 30);
	ExpectStonesAsDrawn(plinth);
	const auto first = registry.Get<const AnimatedStaticPose>(plinth).stones;
	_system.EndDrawnFrame();
	ASSERT_TRUE(_system.LayGateStone(plinth, Stone(MobileStaticInfo::GateTotemCow)));
	_system.Draw(plinth, 30);
	ExpectStonesAsDrawn(plinth);
	// The stones already shown stay the same models
	const auto& now = registry.Get<const AnimatedStaticPose>(plinth).stones;
	ASSERT_EQ(now.size(), 3u);
	EXPECT_EQ(now[0], first[0]);
	EXPECT_EQ(now[1], first[1]);
}

TEST_F(PlinthStonesTest, OpeningItSinksTheStonesWithItsClip)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth = Plinth({MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemCow});
	ASSERT_TRUE(_system.SetOpenState(plinth, k_Open));
	// At the clip's start an open plinth counts as moving: the stones start where the stacked ones stand
	_system.Draw(plinth, 0);
	ExpectStonesAsDrawn(plinth);
	const auto& stones = registry.Get<const AnimatedStaticPose>(plinth).stones;
	ASSERT_EQ(stones.size(), 2u);
	EXPECT_NEAR(LiftOf(stones[0]), 2.8f, 1e-5f);
	EXPECT_NEAR(LiftOf(stones[1]), 4.8f, 1e-5f);
	_system.EndDrawnFrame();
	// Further through the clip they have sunk by 7.15 of their own heights over its play time less 3 ms
	_system.Draw(plinth, 4000);
	ExpectStonesAsDrawn(plinth);
	ASSERT_EQ(stones.size(), 2u);
	const float sunk = 4000.0f / static_cast<float>(k_PlayTime - 3);
	EXPECT_NEAR(LiftOf(stones[0]), (3.0f - ((7.15f * 2.0f) * sunk)) - 0.2f, 1e-5f);
	EXPECT_NEAR(LiftOf(stones[1]), (3.0f + (2.0f - ((7.15f * 2.0f) * sunk))) - 0.2f, 1e-5f);
}

TEST_F(PlinthStonesTest, OpenAtRestItShowsNone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth =
	    Plinth({MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemTiger, MobileStaticInfo::GateTotemCow});
	_system.Draw(plinth, 0);
	const auto shown = registry.Get<const AnimatedStaticPose>(plinth).stones;
	ASSERT_EQ(shown.size(), 3u);
	_system.EndDrawnFrame();
	ASSERT_TRUE(_system.SetOpenState(plinth, k_Open));
	_system.Draw(plinth, k_PlayTime);
	ASSERT_EQ(registry.Get<const AnimatedStaticPose>(plinth).place, OpenRestingPlace(k_PlayTime, k_Frames));
	EXPECT_TRUE(registry.Get<const AnimatedStaticPose>(plinth).stones.empty());
	EXPECT_EQ(registry.Size<PlinthStone>(), 0u);
	for (const auto stone : shown)
	{
		EXPECT_FALSE(registry.Valid(stone));
	}
}

TEST_F(PlinthStonesTest, ClosingItShowsTheStonesBothSinkingAndStacked)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth = Plinth({MobileStaticInfo::GateTotemApe});
	registry.Get<AnimatedStaticPose>(plinth).place = 5000;
	_system.Draw(plinth, 30);
	ExpectStonesAsDrawn(plinth);
	const auto& stones = registry.Get<const AnimatedStaticPose>(plinth).stones;
	ASSERT_EQ(stones.size(), 2u);
	EXPECT_LT(LiftOf(stones[0]), LiftOf(stones[1]));
	EXPECT_NEAR(LiftOf(stones[1]), 2.8f, 1e-5f);
}

TEST_F(PlinthStonesTest, AFrameThatDoesNotDrawThePlinthShowsNoneOfItsStones)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto drawn = Plinth({MobileStaticInfo::GateTotemApe});
	const auto undrawn = Plinth({MobileStaticInfo::GateTotemCow, MobileStaticInfo::GateTotemTiger});
	_system.Draw(drawn, 30);
	_system.Draw(undrawn, 30);
	_system.EndDrawnFrame();
	EXPECT_EQ(registry.Size<PlinthStone>(), 3u);
	// The next frame draws only the first: the second's stones go at the frame's end
	_system.Draw(drawn, 30);
	_system.EndDrawnFrame();
	ExpectStonesAsDrawn(drawn);
	EXPECT_TRUE(registry.Get<const AnimatedStaticPose>(undrawn).stones.empty());
	EXPECT_EQ(registry.Size<PlinthStone>(), 1u);
	// A frame that draws neither shows none, and a plinth with no stones makes none
	_system.EndDrawnFrame();
	EXPECT_EQ(registry.Size<PlinthStone>(), 0u);
	const auto empty = Plinth({});
	_system.Draw(empty, 30);
	EXPECT_TRUE(registry.Get<const AnimatedStaticPose>(empty).stones.empty());
	EXPECT_EQ(registry.Size<PlinthStone>(), 0u);
}

TEST_F(PlinthStonesTest, AResetTakesTheStonesAway)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto plinth = Plinth({MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemCow});
	_system.Draw(plinth, 30);
	ASSERT_EQ(registry.Size<PlinthStone>(), 2u);
	_system.Reset();
	EXPECT_EQ(registry.Size<PlinthStone>(), 0u);
	EXPECT_TRUE(registry.Get<const AnimatedStaticPose>(plinth).stones.empty());
	// A cleared land's stones are only forgotten
	_system.Draw(plinth, 30);
	registry.Reset();
	_system.EndDrawnFrame();
	_system.Reset();
}
