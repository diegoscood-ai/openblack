/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A thing starting to fly offered to the creatures (creature_catch_walk::Offer), against a fake world that records what
// it is asked, in order: each creature that may try, is heavy enough, may catch the thing, did not throw it, goes for
// another player's throw only on a draw of 2 or less out of a hundred, and reaches where the thing passes, sets about
// catching it; the draw is made only for another player's throw, and only once the weight and the thing's state pass.
// No game data.

#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureCatch.h"
#include "ECS/CreatureCatchWalk.h"
#include "ECS/PhysicsHooksWorld.h"
#include "ECS/Registry.h"
#include "Enums.h"

using namespace openblack;
namespace walk = openblack::ecs::creature_catch_walk;

namespace
{
constexpr auto k_Thing = static_cast<entt::entity>(3);
constexpr auto k_Tiger = static_cast<entt::entity>(7);
constexpr auto k_Ape = static_cast<entt::entity>(9);

class FakeWorld final: public ecs::physics_hooks::World
{
public:
	[[nodiscard]] ecs::Registry& Entities() override { return registry; }
	[[nodiscard]] ecs::systems::HandSystemInterface& HandPart() override
	{
		throw std::logic_error("the catch does not need the hand system");
	}
	[[nodiscard]] ecs::systems::BuildingDamageSystemInterface& BuildingDamage() override
	{
		throw std::logic_error("the catch does not need the broken buildings");
	}
	[[nodiscard]] PlayerNames LocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	[[nodiscard]] bool IsToy(entt::entity /*object*/) const override { return false; }
	[[nodiscard]] std::optional<ecs::effects::EffectValues> Crush() const override { return std::nullopt; }
	void ApplyEffect(entt::entity /*object*/, const ecs::effects::EffectValues& /*values*/,
	                 const magic::EffectSource& /*source*/) override
	{
	}
	void UpdateAttitudeFromFeedback(entt::entity /*creature*/, float /*feedback*/) override {}
	void ChangeDesireSource(entt::entity /*creature*/, uint32_t /*source*/, float /*amount*/) override {}
	void KickSway(entt::entity /*creature*/, glm::vec3 /*force*/, glm::vec3 /*point*/) override {}

	[[nodiscard]] std::vector<entt::entity> Creatures() override { return creatures; }
	[[nodiscard]] creature_catch::Eligibility CatchEligibility(entt::entity creature) override
	{
		asked.push_back("eligibility");
		return eligibility[creature];
	}
	[[nodiscard]] float CreatureWeight(entt::entity /*creature*/) override { return creatureWeight; }
	[[nodiscard]] float ThingWeight(entt::entity /*object*/) override
	{
		asked.push_back("weight");
		return thingWeight;
	}
	[[nodiscard]] bool MayBeCaught(entt::entity /*object*/) override
	{
		asked.push_back("state");
		return thingMayBeCaught;
	}
	[[nodiscard]] PlayerNames OwnerOf(entt::entity creature) override { return owners[creature]; }
	[[nodiscard]] bool IsAllied(PlayerNames player, PlayerNames other) override
	{
		asked.push_back("allied");
		return player == allied.first && other == allied.second;
	}
	[[nodiscard]] uint32_t GameRand(uint32_t range) override
	{
		EXPECT_EQ(range, 100u);
		asked.push_back("draw");
		return draw;
	}
	[[nodiscard]] std::optional<creature_catch::Approach> CatchApproach(entt::entity /*creature*/) override
	{
		asked.push_back("approach");
		return approach;
	}
	void SetAboutCatching(entt::entity creature, entt::entity object) override
	{
		EXPECT_EQ(object, k_Thing);
		catching.push_back(creature);
	}

	ecs::Registry registry;
	std::vector<entt::entity> creatures {k_Tiger};
	std::map<entt::entity, creature_catch::Eligibility> eligibility;
	std::map<entt::entity, PlayerNames> owners {{k_Tiger, PlayerNames::PLAYER_ONE}, {k_Ape, PlayerNames::PLAYER_TWO}};
	float creatureWeight {1000.0f};
	float thingWeight {100.0f};
	bool thingMayBeCaught {true};
	uint32_t draw {50};
	/// The one pair of players allied, the first to the second
	std::pair<PlayerNames, PlayerNames> allied {PlayerNames::_COUNT, PlayerNames::_COUNT};
	/// A creature of size 1 standing at the origin: a step of 10 across that lasts as long as the hand takes to close
	std::optional<creature_catch::Approach> approach {creature_catch::Approach {
	    .size = 1.0f, .modelScale = 1.0f, .catchMs = 1175.0f, .stepMs = 1175.0f, .stepTravel = -10.0f}};
	std::vector<std::string> asked;
	std::vector<entt::entity> catching;
};

/// A thing 20 ahead, coming straight at the creature at 5 a second, thrown by nobody's hand
walk::Flight Coming()
{
	return {.centre = {0.0f, 10.0f, 20.0f}, .velocity = {0.0f, 0.0f, -5.0f}, .thrower = entt::null, .player = std::nullopt};
}
} // namespace

TEST(CreatureCatchWalk, ACreatureThatReachesSetsAboutCatching)
{
	FakeWorld world;
	walk::Offer(world, k_Thing, Coming());
	EXPECT_EQ(world.catching, std::vector {k_Tiger});
	// Nobody's throw: no draw
	EXPECT_EQ(world.asked, (std::vector<std::string> {"eligibility", "weight", "state", "approach"}));
}

TEST(CreatureCatchWalk, OneThatMayNotTryIsNotAskedFurther)
{
	FakeWorld world;
	world.eligibility[k_Tiger] = {.fighting = true};
	walk::Offer(world, k_Thing, Coming());
	EXPECT_TRUE(world.catching.empty());
	EXPECT_EQ(world.asked, std::vector<std::string> {"eligibility"});
}

TEST(CreatureCatchWalk, TooHeavyOrKeptFromBeingCaught)
{
	// At the most it can catch, it is too heavy
	FakeWorld heavy;
	heavy.thingWeight = 800.0f;
	walk::Offer(heavy, k_Thing, Coming());
	EXPECT_TRUE(heavy.catching.empty());
	EXPECT_EQ(heavy.asked, (std::vector<std::string> {"eligibility", "weight"}));

	FakeWorld kept;
	kept.thingMayBeCaught = false;
	walk::Offer(kept, k_Thing, Coming());
	EXPECT_TRUE(kept.catching.empty());
	EXPECT_EQ(kept.asked, (std::vector<std::string> {"eligibility", "weight", "state"}));
}

TEST(CreatureCatchWalk, NotItsOwnThrow)
{
	FakeWorld world;
	auto flight = Coming();
	flight.thrower = k_Tiger;
	walk::Offer(world, k_Thing, flight);
	EXPECT_TRUE(world.catching.empty());
}

TEST(CreatureCatchWalk, AnotherPlayersThrowOnlyNowAndThen)
{
	auto flight = Coming();
	flight.player = PlayerNames::PLAYER_ONE;
	// The player's own creature: no draw
	FakeWorld own;
	walk::Offer(own, k_Thing, flight);
	EXPECT_EQ(own.catching, std::vector {k_Tiger});
	EXPECT_EQ(std::ranges::count(own.asked, "draw"), 0);

	// Another player's creature: a draw, gone for at 2 or less
	for (const auto [draw, catches] : {std::pair {0u, true}, std::pair {2u, true}, std::pair {3u, false}})
	{
		FakeWorld rival;
		rival.creatures = {k_Ape};
		rival.draw = draw;
		walk::Offer(rival, k_Thing, flight);
		EXPECT_EQ(rival.catching.size(), catches ? 1u : 0u) << draw;
		EXPECT_EQ(rival.asked,
		          (catches ? std::vector<std::string> {"eligibility", "weight", "state", "allied", "draw", "approach"}
		                   : std::vector<std::string> {"eligibility", "weight", "state", "allied", "draw"}));
	}
}

TEST(CreatureCatchWalk, AnAllysThrowIsGoneForWithoutADraw)
{
	auto flight = Coming();
	flight.player = PlayerNames::PLAYER_ONE;
	FakeWorld ally;
	ally.creatures = {k_Ape};
	ally.draw = 99;
	// the thrower's player allied to the creature's
	ally.allied = {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO};
	walk::Offer(ally, k_Thing, flight);
	EXPECT_EQ(ally.catching, std::vector {k_Ape});
	EXPECT_EQ(ally.asked, (std::vector<std::string> {"eligibility", "weight", "state", "allied", "approach"}));
	// allied the other way round only: the draw is made
	FakeWorld otherWay;
	otherWay.creatures = {k_Ape};
	otherWay.draw = 99;
	otherWay.allied = {PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE};
	walk::Offer(otherWay, k_Thing, flight);
	EXPECT_TRUE(otherWay.catching.empty());
	EXPECT_EQ(std::ranges::count(otherWay.asked, "draw"), 1);
}

TEST(CreatureCatchWalk, EveryCreatureInTurn)
{
	FakeWorld world;
	world.creatures = {k_Tiger, k_Ape};
	walk::Offer(world, k_Thing, Coming());
	EXPECT_EQ(world.catching, (std::vector {k_Tiger, k_Ape}));
	// Out of reach for both
	FakeWorld far;
	far.creatures = {k_Tiger, k_Ape};
	auto wide = Coming();
	wide.centre.x = 40.0f;
	walk::Offer(far, k_Thing, wide);
	EXPECT_TRUE(far.catching.empty());
	// Without its step or catch moment it can't
	FakeWorld unposed;
	unposed.approach.reset();
	walk::Offer(unposed, k_Thing, Coming());
	EXPECT_TRUE(unposed.catching.empty());
}
