/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/ComputerPlayerStart.h"

using namespace openblack;
using namespace openblack::computer_player;
using openblack::ecs::components::ComputerPlayer;

namespace
{

/// A world of players, temples and creatures that the test sets
class FakeWorld final: public World
{
public:
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	[[nodiscard]] bool IsComputerPlayer(PlayerNames player) const override { return computerPlayers.contains(player); }
	[[nodiscard]] std::optional<entt::entity> Creature(PlayerNames player) const override
	{
		const auto found = creatures.find(player);
		return found != creatures.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] float DrawnSize(entt::entity creature) const override { return sizes.at(creature); }
	void SetSize(entt::entity creature, float size) override { sizes[creature] = size; }
	[[nodiscard]] bool HasTemple(PlayerNames player) const override { return temples.contains(player); }
	[[nodiscard]] std::optional<glm::vec3> Pen(PlayerNames player) const override
	{
		const auto found = pens.find(player);
		return found != pens.end() ? std::optional(found->second) : std::nullopt;
	}
	[[nodiscard]] bool Skirmish() const override { return skirmish; }
	[[nodiscard]] CanCast Miracles(PlayerNames /*player*/) const override { return miracles; }
	void MakeCreature(PlayerNames player, CreatureType species, const glm::vec3& position) override
	{
		made.emplace_back(species, position);
		const auto creature = static_cast<entt::entity>(100 + made.size());
		creatures[player] = creature;
		sizes[creature] = 1.0f;
	}
	void Teach(entt::entity creature, MagicType miracle) override { taught.emplace_back(creature, miracle); }
	uint32_t GameRand(uint32_t range) override
	{
		rangesAsked.push_back(range);
		return roll;
	}
	float GameFloatRand(float most) override
	{
		mostsAsked.push_back(most);
		return share;
	}

	std::map<PlayerNames, entt::entity> creatures;
	std::map<entt::entity, float> sizes;
	std::set<PlayerNames> temples;
	std::set<PlayerNames> computerPlayers {PlayerNames::PLAYER_TWO};
	std::map<PlayerNames, glm::vec3> pens;
	bool skirmish {true};
	CanCast miracles {};
	std::vector<std::pair<CreatureType, glm::vec3>> made;
	std::vector<std::pair<entt::entity, MagicType>> taught;
	std::vector<uint32_t> rangesAsked;
	std::vector<float> mostsAsked;
	uint32_t roll {0};
	float share {0.0f};
};

constexpr auto k_Person = static_cast<entt::entity>(1);
constexpr auto k_GodsCreature = static_cast<entt::entity>(2);

/// A mind that a land script switched on, with the local player named for the size of its creature
ComputerPlayer SwitchedOnLikeThePerson()
{
	ComputerPlayer mind;
	SwitchOn(mind, true);
	mind.creatureLike = PlayerNames::PLAYER_ONE;
	return mind;
}

} // namespace

TEST(ComputerPlayerStart, SwitchingOnMakesTheMindDueAndSwitchingOffStopsIt)
{
	ComputerPlayer mind;
	EXPECT_FALSE(DueToStart(mind));
	SwitchOn(mind, true);
	EXPECT_TRUE(mind.switchedOn);
	EXPECT_TRUE(DueToStart(mind));
	SwitchOn(mind, false);
	EXPECT_FALSE(mind.switchedOn);
	EXPECT_FALSE(DueToStart(mind));
}

TEST(ComputerPlayerStart, APausedMindWaitsToStartUntilItGoesOn)
{
	ComputerPlayer mind;
	SwitchOn(mind, true);
	Pause(mind, true);
	EXPECT_TRUE(mind.paused);
	EXPECT_FALSE(DueToStart(mind));
	Pause(mind, false);
	EXPECT_TRUE(DueToStart(mind));
}

TEST(ComputerPlayerStart, TheWideScreenHoldsTheMindUnlessAScriptMovesItsHand)
{
	EXPECT_FALSE(HeldByWideScreen(false, false));
	EXPECT_TRUE(HeldByWideScreen(true, false));
	EXPECT_FALSE(HeldByWideScreen(true, true));
	EXPECT_FALSE(HeldByWideScreen(false, true));
}

TEST(ComputerPlayerStart, TheMindStartsOneTimeOnTheLand)
{
	FakeWorld world;
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	EXPECT_TRUE(mind.started);
	EXPECT_FALSE(DueToStart(mind));
}

TEST(ComputerPlayerStart, TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize)
{
	EXPECT_FLOAT_EQ(SizeLike(2.0f, 0.0f), 1.8f);
	EXPECT_FLOAT_EQ(SizeLike(2.0f, 0.3f), 2.4f);
	FakeWorld world;
	world.creatures = {{PlayerNames::PLAYER_ONE, k_Person}, {PlayerNames::PLAYER_TWO, k_GodsCreature}};
	world.sizes = {{k_Person, 1.5f}, {k_GodsCreature, 0.4f}};
	world.share = 0.1f;
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	ASSERT_EQ(world.mostsAsked, std::vector<float> {0.3f});
	EXPECT_FLOAT_EQ(world.sizes.at(k_GodsCreature), 1.5f);
	EXPECT_FLOAT_EQ(world.sizes.at(k_Person), 1.5f);
}

TEST(ComputerPlayerStart, NoNameOrNoCreatureOfThePersonKeepsTheSize)
{
	FakeWorld world;
	world.creatures = {{PlayerNames::PLAYER_TWO, k_GodsCreature}};
	world.sizes = {{k_GodsCreature, 0.4f}};
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	EXPECT_FLOAT_EQ(world.sizes.at(k_GodsCreature), 0.4f);

	FakeWorld named;
	named.creatures = {{PlayerNames::PLAYER_ONE, k_Person}, {PlayerNames::PLAYER_TWO, k_GodsCreature}};
	named.sizes = {{k_Person, 1.5f}, {k_GodsCreature, 0.4f}};
	ComputerPlayer unnamed;
	SwitchOn(unnamed, true);
	Start(named, PlayerNames::PLAYER_TWO, unnamed);
	EXPECT_FLOAT_EQ(named.sizes.at(k_GodsCreature), 0.4f);
	EXPECT_TRUE(named.mostsAsked.empty());
}

TEST(ComputerPlayerStart, AnyNamedPlayerSizesTheCreatureFromTheLocalPlayersCreature)
{
	FakeWorld world;
	world.creatures = {{PlayerNames::PLAYER_ONE, k_Person}, {PlayerNames::PLAYER_TWO, k_GodsCreature}};
	world.sizes = {{k_Person, 1.0f}, {k_GodsCreature, 0.4f}};
	auto mind = SwitchedOnLikeThePerson();
	mind.creatureLike = PlayerNames::NEUTRAL;
	Start(world, PlayerNames::PLAYER_TWO, mind);
	EXPECT_FLOAT_EQ(world.sizes.at(k_GodsCreature), 0.9f);
}

TEST(ComputerPlayerStart, TheCreatureLearnsThePlayersMiraclesAndHealFoodWoodAndWater)
{
	FakeWorld world;
	world.creatures = {{PlayerNames::PLAYER_TWO, k_GodsCreature}};
	world.sizes = {{k_GodsCreature, 0.4f}};
	world.temples = {PlayerNames::PLAYER_TWO};
	world.miracles.at(static_cast<size_t>(MagicType::Fireball)) = true;
	world.miracles.at(static_cast<size_t>(MagicType::Water)) = true;
	world.miracles.at(static_cast<size_t>(MagicType::CreatureSpellItchy)) = true;
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	const std::vector<std::pair<entt::entity, MagicType>> expected {
	    {k_GodsCreature, MagicType::Fireball}, {k_GodsCreature, MagicType::Heal},
	    {k_GodsCreature, MagicType::Food},     {k_GodsCreature, MagicType::Wood},
	    {k_GodsCreature, MagicType::Water},    {k_GodsCreature, MagicType::CreatureSpellItchy},
	};
	EXPECT_EQ(world.taught, expected);
}

TEST(ComputerPlayerStart, EveryMiracleIsLearntWhenThePlayerCanCastEveryMiracle)
{
	CanCast canCast {};
	canCast.fill(true);
	EXPECT_EQ(MiraclesToLearn(canCast, true).size(), canCast.size() - 1);
	EXPECT_TRUE(MiraclesToLearn(canCast, false).empty());
}

TEST(ComputerPlayerStart, NoTempleOrNotAComputerPlayerOrTheLocalPlayerTeachesNothing)
{
	FakeWorld world;
	world.creatures = {{PlayerNames::PLAYER_ONE, k_Person}, {PlayerNames::PLAYER_TWO, k_GodsCreature}};
	world.sizes = {{k_Person, 1.0f}, {k_GodsCreature, 0.4f}};
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	EXPECT_TRUE(world.taught.empty());

	world.temples = {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO};
	world.computerPlayers.clear();
	ComputerPlayer human {.switchedOn = true};
	Start(world, PlayerNames::PLAYER_TWO, human);
	EXPECT_TRUE(world.taught.empty());

	world.computerPlayers = {PlayerNames::PLAYER_ONE};
	auto local = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_ONE, local);
	EXPECT_TRUE(world.taught.empty());
	EXPECT_FLOAT_EQ(world.sizes.at(k_Person), 1.0f);
}

TEST(ComputerPlayerStart, InASkirmishAPlayerWithATempleButNoCreatureGetsOneOfSeventeenSpeciesAtThePen)
{
	FakeWorld world;
	world.temples = {PlayerNames::PLAYER_TWO};
	world.pens = {{PlayerNames::PLAYER_TWO, glm::vec3(10.0f, 2.0f, 20.0f)}};
	world.roll = 6;
	auto mind = SwitchedOnLikeThePerson();
	Start(world, PlayerNames::PLAYER_TWO, mind);
	ASSERT_EQ(world.rangesAsked, std::vector<uint32_t> {17});
	ASSERT_EQ(world.made.size(), 1);
	EXPECT_EQ(world.made[0].first, CreatureType::Horse);
	EXPECT_EQ(world.made[0].second, glm::vec3(10.0f, 2.0f, 20.0f));
	// The new creature learns the miracles that it always knows
	EXPECT_EQ(world.taught.size(), k_AlwaysKnown.size());
}

TEST(ComputerPlayerStart, OutsideASkirmishOrWithNoTempleNoCreatureIsMade)
{
	FakeWorld story;
	story.skirmish = false;
	story.temples = {PlayerNames::PLAYER_TWO};
	story.pens = {{PlayerNames::PLAYER_TWO, glm::vec3(0.0f)}};
	auto mind = SwitchedOnLikeThePerson();
	Start(story, PlayerNames::PLAYER_TWO, mind);
	EXPECT_TRUE(story.made.empty());

	FakeWorld noTemple;
	auto other = SwitchedOnLikeThePerson();
	Start(noTemple, PlayerNames::PLAYER_TWO, other);
	EXPECT_TRUE(noTemple.made.empty());
	EXPECT_TRUE(noTemple.rangesAsked.empty());
}

TEST(ComputerPlayerStart, TheSpeciesNumberZeroIsTheApe)
{
	EXPECT_EQ(SpeciesFor(0), CreatureType::GiantApe);
	EXPECT_EQ(SpeciesFor(16), CreatureType::Gorilla);
}
