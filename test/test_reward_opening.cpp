/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/RewardOpening.h"
#include "Magic/Gestures/GestureShapes.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// A seed of fireball with two power-ups, and one of storm with three
constexpr auto k_Fireball = SpellSeedType::Fire;
constexpr auto k_Storm = SpellSeedType::Storm;

/// Records what the chest did to the world
class FakeWorld final: public reward::OpeningWorld
{
public:
	void Pour(ResourceType type, uint32_t amount) override { events.push_back(fmt("pour", static_cast<int>(type), amount)); }
	[[nodiscard]] bool HasTown() const override { return town; }
	void GiveTownBelief(float belief) override { events.push_back("belief " + std::to_string(static_cast<int>(belief))); }
	void MakeObject(MobileStaticInfo object) override { events.push_back(fmt("object", static_cast<int>(object), 0)); }
	void MakeScaffold() override { events.emplace_back("scaffold"); }
	void FindTownForMiracle() override
	{
		events.emplace_back("find town");
		town = townToFind;
	}
	[[nodiscard]] MagicType NextMiracle() override { return next; }
	[[nodiscard]] MagicType RandomMiracle() override { return random; }
	[[nodiscard]] SpellSeedType FirstSeedOf(MagicType type) const override
	{
		return type == MagicType::StormWindRain ? k_Storm : k_Fireball;
	}
	[[nodiscard]] int LevelCount(SpellSeedType seed) const override { return seed == k_Storm ? 4 : 3; }
	[[nodiscard]] MagicType MagicAt(SpellSeedType seed, int level) const override
	{
		if (seed == k_Storm)
		{
			return reward::MagicAtLevel(
			    {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado, MagicType::Shield},
			    {GestureType::Spiral, GestureType::Circle, GestureType::Three}, level);
		}
		return reward::MagicAtLevel(
		    {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo, MagicType::None},
		    {GestureType::Spiral, GestureType::Circle, GestureType::None}, level);
	}
	[[nodiscard]] bool PlayerMay(MagicType type) const override { return may.contains(type); }
	void PlayerLearns(MagicType type) override { events.push_back(fmt("player learns", static_cast<int>(type), 0)); }
	bool TownLearns(MagicType type) override
	{
		events.push_back(fmt("town learns", static_cast<int>(type), 0));
		return townLearnsNew;
	}
	void ShowTownSeed(SpellSeedType seed) override { events.push_back(fmt("town seed", static_cast<int>(seed), 0)); }
	void OneShotGlobe(SpellSeedType seed) override { events.push_back(fmt("globe", static_cast<int>(seed), 0)); }
	void ShowGesture(MagicType type) override { events.push_back(fmt("gesture", static_cast<int>(type), 0)); }

	static std::string fmt(const char* what, int a, uint32_t b)
	{
		return std::string(what) + " " + std::to_string(a) + (b != 0 ? " " + std::to_string(b) : "");
	}

	bool town {false};
	bool townToFind {false};
	bool townLearnsNew {true};
	MagicType next {MagicType::None};
	MagicType random {MagicType::None};
	std::set<MagicType> may;
	std::vector<std::string> events;
};

std::string Globe(SpellSeedType seed)
{
	return FakeWorld::fmt("globe", static_cast<int>(seed), 0);
}
} // namespace

TEST(RewardOpening, FoodComesFirstThenWoodPouredAtTheChest)
{
	FakeWorld world;
	reward::Open({.food = 750.5f, .wood = 300}, world);
	ASSERT_EQ(world.events.size(), 1u);
	EXPECT_EQ(world.events[0], "pour 0 750");

	world.events.clear();
	reward::Open({.wood = 300}, world);
	EXPECT_EQ(world.events, std::vector<std::string> {"pour 1 300"});
}

TEST(RewardOpening, BeliefGoesOnlyToATown)
{
	FakeWorld world;
	reward::Open({.belief = 500.0f, .seed = k_Fireball}, world);
	EXPECT_TRUE(world.events.empty());

	world.town = true;
	reward::Open({.belief = 500.0f}, world);
	EXPECT_EQ(world.events, std::vector<std::string> {"belief 500"});
}

TEST(RewardOpening, AnObjectOrAScaffoldIsMadeAtTheChest)
{
	FakeWorld world;
	reward::Open({.object = MobileStaticInfo::ToyBall}, world);
	reward::Open({.scaffold = true}, world);
	EXPECT_EQ(world.events, (std::vector<std::string> {FakeWorld::fmt("object", static_cast<int>(MobileStaticInfo::ToyBall), 0),
	                                                   "scaffold"}));
}

TEST(RewardOpening, ASeedThePlayerCantCastIsAGlobeWithoutATown)
{
	FakeWorld world;
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events, std::vector<std::string> {Globe(k_Fireball)});
}

TEST(RewardOpening, ATownLearnsASeedThePlayerCantCastAndShowsIt)
{
	FakeWorld world;
	world.town = true;
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events, (std::vector<std::string> {FakeWorld::fmt("town learns", static_cast<int>(MagicType::Fireball), 0),
	                                                   FakeWorld::fmt("town seed", static_cast<int>(k_Fireball), 0)}));

	// A town that knew it already gives a globe instead
	world.events.clear();
	world.townLearnsNew = false;
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events.back(), Globe(k_Fireball));
}

TEST(RewardOpening, ASeedThePlayerCastsGivesTheFirstPowerUpLackedWithItsGesture)
{
	FakeWorld world;
	world.may = {MagicType::Fireball};
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events,
	          (std::vector<std::string> {FakeWorld::fmt("player learns", static_cast<int>(MagicType::FireballPowerUpOne), 0),
	                                     FakeWorld::fmt("gesture", static_cast<int>(MagicType::FireballPowerUpOne), 0)}));

	// With a town, the town learns the power-up
	world.events.clear();
	world.town = true;
	world.may = {MagicType::Fireball, MagicType::FireballPowerUpOne};
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events,
	          (std::vector<std::string> {FakeWorld::fmt("town learns", static_cast<int>(MagicType::FireballPowerUpTwo), 0),
	                                     FakeWorld::fmt("gesture", static_cast<int>(MagicType::FireballPowerUpTwo), 0)}));
}

TEST(RewardOpening, WithEveryPowerUpTheSeedIsAGlobe)
{
	FakeWorld world;
	world.may = {MagicType::Fireball, MagicType::FireballPowerUpOne, MagicType::FireballPowerUpTwo};
	reward::Open({.seed = k_Fireball}, world);
	EXPECT_EQ(world.events, std::vector<std::string> {Globe(k_Fireball)});
}

TEST(RewardOpening, ThreePowerUpsReadOnIntoTheGestures)
{
	// The game reads one level past the last power-up: for a seed with three, its first power-up gesture as a magic type
	FakeWorld world;
	world.may = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado, MagicType::Shield};
	reward::Open({.seed = k_Storm}, world);
	const auto misread = static_cast<MagicType>(GestureType::Spiral);
	EXPECT_EQ(world.events, (std::vector<std::string> {FakeWorld::fmt("player learns", static_cast<int>(misread), 0),
	                                                   FakeWorld::fmt("gesture", static_cast<int>(misread), 0)}));
}

TEST(RewardOpening, APowerUpWithNoSeedOnlyShowsItsGesture)
{
	FakeWorld world;
	reward::Open({.powerUp = MagicType::HealPowerUpOne}, world);
	EXPECT_EQ(world.events,
	          std::vector<std::string> {FakeWorld::fmt("gesture", static_cast<int>(MagicType::HealPowerUpOne), 0)});
}

TEST(RewardOpening, TheNextMiracleChestFindsATownAndGivesTheNextMiracle)
{
	FakeWorld world;
	world.townToFind = true;
	world.next = MagicType::StormWindRain;
	reward::Open({.nextMiracle = true}, world);
	EXPECT_EQ(world.events, (std::vector<std::string> {
	                            "find town", FakeWorld::fmt("town learns", static_cast<int>(MagicType::StormWindRain), 0),
	                            FakeWorld::fmt("town seed", static_cast<int>(k_Storm), 0)}));

	// With nothing left to learn, a random one as a globe
	FakeWorld done;
	done.random = MagicType::StormWindRain;
	reward::Open({.nextMiracle = true}, done);
	EXPECT_EQ(done.events, (std::vector<std::string> {"find town", Globe(k_Storm)}));
}

TEST(RewardOpening, MagicAtLevelReadsThePlainMiracleThenThePowerUps)
{
	const std::array<MagicType, 4> types {MagicType::Heal, MagicType::HealPowerUpOne, MagicType::None, MagicType::None};
	const std::array<GestureType, 3> gestures {GestureType::Circle, GestureType::None, GestureType::None};
	EXPECT_EQ(reward::MagicAtLevel(types, gestures, -1), MagicType::Heal);
	EXPECT_EQ(reward::MagicAtLevel(types, gestures, 0), MagicType::HealPowerUpOne);
	EXPECT_EQ(reward::MagicAtLevel(types, gestures, 1), MagicType::None);
	EXPECT_EQ(reward::MagicAtLevel(types, gestures, 3), static_cast<MagicType>(GestureType::Circle));
}

TEST(RewardOpening, TheProgressListIsChosenByAlignment)
{
	const auto heads = [] { return true; };
	const auto tails = [] { return false; };
	EXPECT_TRUE(reward::TakesGoodList(0.1f, tails));
	EXPECT_FALSE(reward::TakesGoodList(-0.1f, heads));
	EXPECT_TRUE(reward::TakesGoodList(0.0f, heads));
	EXPECT_FALSE(reward::TakesGoodList(0.0f, tails));
}

TEST(RewardOpening, TheNextMiracleIsTheFirstAllowedOnTheLandThePlayerLacks)
{
	const std::vector<reward::ProgressEntry> list {
	    {.magic = MagicType::Fireball, .onLand = {0, 1, 1, 1, 1, 1}},
	    {.magic = MagicType::Heal, .onLand = {0, 0, 1, 1, 1, 1}},
	    {.magic = MagicType::StormWindRain, .onLand = {0, 1, 1, 1, 1, 1}},
	};
	const auto may = [](MagicType type) { return type == MagicType::Fireball; };
	EXPECT_EQ(reward::NextMiracle(list, 1, may), MagicType::StormWindRain);
	EXPECT_EQ(reward::NextMiracle(list, 2, may), MagicType::Heal);
	EXPECT_EQ(reward::NextMiracle(list, 0, may), MagicType::None);
}

TEST(RewardOpening, ARandomMiracleIsDrawnUntilOneIsAllowed)
{
	const std::vector<reward::ProgressEntry> list {
	    {.magic = MagicType::Fireball, .onLand = {0, 0, 1, 1, 1, 1}},
	    {.magic = MagicType::Heal, .onLand = {0, 1, 1, 1, 1, 1}},
	};
	std::vector<uint32_t> draws {0, 0, 1};
	size_t drawn = 0;
	const auto draw = [&](uint32_t n) {
		EXPECT_EQ(n, 2u);
		return draws.at(drawn++);
	};
	EXPECT_EQ(reward::RandomMiracle(list, 1, draw), MagicType::Heal);
	EXPECT_EQ(drawn, 3u);
	// Nothing allowed on the land: none, where the game would draw for ever
	EXPECT_EQ(reward::RandomMiracle(list, 0, draw), MagicType::None);
}

TEST(RewardOpening, TheOpenedChestsClipHoldsAtItsEndUnlessItLoops)
{
	EXPECT_EQ(reward::StepOpenedClip(0, 16, 1000, false), 16u);
	EXPECT_EQ(reward::StepOpenedClip(990, 16, 1000, false), 999u);
	EXPECT_EQ(reward::StepOpenedClip(990, 16, 1000, true), 6u);
	EXPECT_EQ(reward::StepOpenedClip(5, 16, 0, false), 0u);
}

TEST(RewardOpening, AGestureShownOnTheLandIsLaidFromTheChestEastAndSouth)
{
	magic::gestures::Shape shape;
	shape.points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	const auto trail = magic::gestures::LandGestureTrail(
	    shape, {100.0f, 5.0f, 200.0f}, 30.0f, [](float x, float /*z*/) { return x * 0.1f; }, false);
	ASSERT_EQ(trail.ideal.Size(), magic::gestures::k_LandGesturePoints);
	ASSERT_EQ(trail.stroke.Size(), magic::gestures::k_LandGesturePoints);
	EXPECT_FLOAT_EQ(trail.ideal[0].x, 100.0f);
	EXPECT_FLOAT_EQ(trail.ideal[0].y, 10.0f);
	EXPECT_FLOAT_EQ(trail.ideal[0].z, 200.0f);
	const auto last = trail.ideal.Size() - 1;
	EXPECT_FLOAT_EQ(trail.ideal[last].x, 130.0f);
	EXPECT_FLOAT_EQ(trail.ideal[last].z, 230.0f);
	// The path and the shape are the same
	for (size_t i = 0; i < trail.ideal.Size(); ++i)
	{
		EXPECT_EQ(trail.stroke[i], trail.ideal[i]) << i;
	}
	EXPECT_FALSE(trail.fromInterface);
}

TEST(RewardOpening, AGestureShownOnTheLandIsTheTappingInterfacesOwn)
{
	// The game hands the gesture the interface that tapped the chest: this computer's is heard as its own
	magic::gestures::Shape shape;
	shape.points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	const auto mine = magic::gestures::LandGestureTrail(shape, glm::vec3(0.0f), 30.0f, nullptr, true);
	const auto theirs = magic::gestures::LandGestureTrail(shape, glm::vec3(0.0f), 30.0f, nullptr, false);
	EXPECT_TRUE(mine.fromInterface);
	EXPECT_FALSE(theirs.fromInterface);
	// Without a land the points stay at the corner's height
	EXPECT_FLOAT_EQ(mine.ideal[1].y, 0.0f);
}

TEST(RewardOpening, AGestureShownOnTheLandAsksTheHeightAtTheMapPointItFallsIn)
{
	// The game looks the height up at the map point (a 6553.6th of a metre) the laid point falls in
	magic::gestures::Shape shape;
	shape.points = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}};
	std::vector<std::pair<float, float>> asked;
	const auto trail = magic::gestures::LandGestureTrail(
	    shape, {0.00005f, 0.0f, 0.00005f}, 30.0f,
	    [&asked](float x, float z) {
		    asked.emplace_back(x, z);
		    return 1.0f;
	    },
	    true);
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[0], std::make_pair(0.0f, 0.0f));
	EXPECT_EQ(asked[1], std::make_pair(30.0f, 30.0f));
	// the laid point keeps where it was laid
	EXPECT_EQ(trail.ideal[0].x, 0.00005f);
	EXPECT_EQ(trail.ideal[0].y, 1.0f);
}

TEST(RewardOpening, AGestureShownOnTheLandIsTakenInFloatSteps)
{
	// Every step of the game's arithmetic rounds to a float: where each of the 80 points falls along the shape's seven
	// points, and its share between the two either side. Steps kept in double give 0x42EC3333 and 0x42F10000 here
	magic::gestures::Shape shape;
	for (const float x : {100.0f, 101.7f, 133.3f, 97.1f, 120.9f, 131.0f, 99.9f})
	{
		shape.points.emplace_back(x, 0.0f, 0.0f);
	}
	const auto trail = magic::gestures::LandGestureTrail(shape, glm::vec3(0.0f), 1.0f, nullptr, true);
	ASSERT_EQ(trail.ideal.Size(), magic::gestures::k_LandGesturePoints);
	EXPECT_EQ(std::bit_cast<uint32_t>(trail.ideal[20].x), 0x42EC3334u);
	EXPECT_EQ(std::bit_cast<uint32_t>(trail.ideal[21].x), 0x42F10001u);
	EXPECT_EQ(trail.ideal[0].x, 100.0f);
	EXPECT_EQ(trail.ideal[79].x, 99.9f);
}
