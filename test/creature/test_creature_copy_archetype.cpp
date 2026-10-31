/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Creature Cave's copy of the creature: what it is made of, where it stands, and the two tosses it makes on the
// local stream as the room starts

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureMorph.h"
#include "Creature/TattooEditor.h"
#include "ECS/Archetypes/CreatureCopyArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
const CreatureCopyArchetype::CavePlaces k_Places {
    .first = {0.0f, 1.0f, 0.0f},
    .third = {8.0f, 1.0f, -4.0f},
    .fire = {2.0f, 0.0f, 6.0f},
};

class CreatureCopyArchetypeTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<Registry>(); }

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A creature with a body, as the land has it
	static entt::entity MakeCreature(float size, bool withMesh = true, float alignment = 0.0f)
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		registry.Assign<Creature>(entity, Creature {.owner = PlayerNames::PLAYER_ONE,
		                                            .species = CreatureType::Tiger,
		                                            .mind = 0,
		                                            .alignment = alignment,
		                                            .size = size});
		if (withMesh)
		{
			registry.Assign<Mesh>(entity, Mesh {.id = 7, .submeshId = 0, .bbSubmeshId = 0});
		}
		registry.AssignState<CreatureMorph>(entity, CreatureMorph {.shownFatness = 0.25f, .drawn = {}, .revision = 3});
		auto& animation = registry.AssignState<CreatureAnimation>(entity);
		animation.breathPhase = 0.5f;
		animation.lookAt = glm::vec3(1.0f, 2.0f, 3.0f);
		animation.gesture.animation = creature_layers::animations::k_FirstGesture;
		animation.skeleton.parents.assign(12, 0);
		auto& eyes = registry.AssignState<CreatureEyes>(entity);
		eyes.openness = -0.5f;
		eyes.lookAt = glm::vec3(1.0f, 2.0f, 3.0f);
		eyes.blink.timerMs = 3;
		registry.Assign<Transform>(entity, glm::vec3(100.0f, 5.0f, 200.0f), affine::AngleY(1.0f), glm::vec3(0.75f));
		return entity;
	}

	/// The local seed's next two tosses of 2, as the game's generator makes them
	static std::pair<uint32_t, uint32_t> NextTwo(uint32_t seed)
	{
		const auto first = game_random::SeededRandom(2, seed);
		const auto second = game_random::SeededRandom(2, seed);
		return {first, second};
	}

private:
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(CreatureCopyArchetypeTest, TheCopyIsMadeOfTheCreaturesBodyWithoutBeingACreature)
{
	const game_random::testing::ScopedState state;
	const auto creature = MakeCreature(1.0f);
	const auto copy = CreatureCopyArchetype::Create(creature, k_Places);
	ASSERT_TRUE(copy.has_value());
	const auto& registry = std::as_const(Reg());
	ASSERT_TRUE((registry.AllOf<CreatureCopy, CreatureMorph, CreatureAnimation, CreatureEyes, Transform>(*copy)));
	// nothing that looks for creatures, or for meshes to draw or pick, finds it
	EXPECT_FALSE(registry.AnyOf<Creature>(*copy));
	EXPECT_FALSE(registry.AnyOf<Mesh>(*copy));
	const auto& made = registry.Get<CreatureCopy>(*copy);
	EXPECT_EQ(made.source, creature);
	EXPECT_EQ(made.species, CreatureType::Tiger);
	EXPECT_EQ(made.size, 1.0f);
	// its body as the creature's
	EXPECT_EQ(registry.Get<CreatureMorph>(*copy).shownFatness, 0.25f);
	EXPECT_EQ(registry.Get<CreatureMorph>(*copy).revision, 3u);
	EXPECT_EQ(registry.Get<CreatureAnimation>(*copy).breathPhase, 0.5f);
	EXPECT_EQ(registry.Get<CreatureAnimation>(*copy).skeleton.parents.size(), 12u);
	EXPECT_EQ(registry.Get<CreatureEyes>(*copy).openness, -0.5f);
	// made afresh: it plays nothing, looks nowhere and blinks from the start, its rest not started yet
	EXPECT_FALSE(registry.Get<CreatureAnimation>(*copy).lookAt.has_value());
	EXPECT_FALSE(registry.Get<CreatureAnimation>(*copy).gesture.animation.has_value());
	EXPECT_FALSE(registry.Get<CreatureEyes>(*copy).lookAt.has_value());
	EXPECT_EQ(registry.Get<CreatureEyes>(*copy).blink.timerMs, creature_eyes::Blink {}.timerMs);
	EXPECT_FALSE(made.started);
	EXPECT_EQ(made.fire, k_Places.fire);
	// at its place, facing the fire, at the creature's scale
	const auto& transform = registry.Get<Transform>(*copy);
	const auto place = creature_tattoo_editor::CavePlace(k_Places.first, k_Places.third, 1.0f, made.sleeping);
	EXPECT_EQ(transform.position, place);
	EXPECT_EQ(made.heading, creature_tattoo_editor::CopyHeading(place, k_Places.fire));
	EXPECT_EQ(transform.rotation, affine::AngleY(made.heading));
	EXPECT_EQ(transform.scale, glm::vec3(0.75f));
	// the creature itself is left as it was
	EXPECT_EQ(registry.Get<Transform>(creature).position, glm::vec3(100.0f, 5.0f, 200.0f));
}

TEST_F(CreatureCopyArchetypeTest, TheCopyTossesTheCoinThenTheRestsFlagOnTheLocalStream)
{
	// seeds whose next two tosses differ, so that their order shows
	uint32_t shown = 0;
	for (const uint32_t seed : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u})
	{
		const auto [coin, flag] = NextTwo(seed);
		if (coin == flag)
		{
			continue;
		}
		++shown;
		SCOPED_TRACE(testing::Message() << "seed " << seed);
		const game_random::testing::ScopedState state({.synced = 0x1234u, .local = seed});
		const auto copy = CreatureCopyArchetype::Create(MakeCreature(1.0f), k_Places);
		ASSERT_TRUE(copy.has_value());
		const auto& made = Reg().Get<CreatureCopy>(*copy);
		EXPECT_EQ(made.sleeping, coin != 0);
		EXPECT_EQ(made.restFlag, flag != 0);
		// two tosses, on the local stream only
		auto after = seed;
		static_cast<void>(game_random::SeededRandom(2, after));
		static_cast<void>(game_random::SeededRandom(2, after));
		EXPECT_EQ(game_random::Current().local, after);
		EXPECT_EQ(game_random::Current().synced, 0x1234u);
		// the coin moves it towards the first place
		EXPECT_EQ(Reg().Get<Transform>(*copy).position,
		          creature_tattoo_editor::CavePlace(k_Places.first, k_Places.third, 1.0f, coin != 0));
	}
	EXPECT_GE(shown, 2u);
}

TEST_F(CreatureCopyArchetypeTest, ACopyOfACreatureOf125OrMoreNeverSleepsButStillTossesTwice)
{
	for (const uint32_t seed : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u})
	{
		SCOPED_TRACE(testing::Message() << "seed " << seed);
		const game_random::testing::ScopedState state({.synced = 0u, .local = seed});
		const auto copy = CreatureCopyArchetype::Create(MakeCreature(1.25f), k_Places);
		ASSERT_TRUE(copy.has_value());
		EXPECT_FALSE(Reg().Get<CreatureCopy>(*copy).sleeping);
		EXPECT_EQ(Reg().Get<CreatureCopy>(*copy).restFlag, NextTwo(seed).second != 0);
		auto after = seed;
		static_cast<void>(game_random::SeededRandom(2, after));
		static_cast<void>(game_random::SeededRandom(2, after));
		EXPECT_EQ(game_random::Current().local, after);
	}
}

TEST_F(CreatureCopyArchetypeTest, AGoodOrEvilCreaturesCopyDrawsItsSparklesAroundTheRestsFlag)
{
	// the coin, a moment of three sparkles of six draws (good) or five (evil), the rest's flag, and another moment
	for (const auto& [alignment, between] : {std::pair {0.5f, 18}, std::pair {-0.5f, 15}})
	{
		for (const uint32_t seed : {1u, 2u, 3u, 4u})
		{
			SCOPED_TRACE(testing::Message() << "alignment " << alignment << " seed " << seed);
			const game_random::testing::ScopedState state({.synced = 0u, .local = seed});
			const auto copy = CreatureCopyArchetype::Create(MakeCreature(1.0f, true, alignment), k_Places);
			ASSERT_TRUE(copy.has_value());
			auto after = seed;
			const auto coin = game_random::SeededRandom(2, after);
			for (int i = 0; i < between; ++i)
			{
				static_cast<void>(game_random::SeededRandom(2, after));
			}
			const auto flag = game_random::SeededRandom(2, after);
			for (int i = 0; i < between; ++i)
			{
				static_cast<void>(game_random::SeededRandom(2, after));
			}
			EXPECT_EQ(Reg().Get<CreatureCopy>(*copy).sleeping, coin != 0);
			EXPECT_EQ(Reg().Get<CreatureCopy>(*copy).restFlag, flag != 0);
			EXPECT_EQ(game_random::Current().local, after);
		}
	}
}

TEST_F(CreatureCopyArchetypeTest, TheCopyIsGivenTheCreaturesSizeWithinItsLimits)
{
	const game_random::testing::ScopedState state;
	const auto copy = CreatureCopyArchetype::Create(MakeCreature(9.0f), k_Places);
	ASSERT_TRUE(copy.has_value());
	EXPECT_EQ(Reg().Get<CreatureCopy>(*copy).size, creature_morph::ClampScale(9.0f));
	EXPECT_LT(Reg().Get<CreatureCopy>(*copy).size, 9.0f);
	// a size below the least is held to it before the place is found
	const auto small = CreatureCopyArchetype::Create(MakeCreature(0.0f), k_Places);
	ASSERT_TRUE(small.has_value());
	const auto& made = Reg().Get<CreatureCopy>(*small);
	EXPECT_EQ(made.size, creature_morph::k_MinScale);
	EXPECT_EQ(Reg().Get<Transform>(*small).position,
	          creature_tattoo_editor::CavePlace(k_Places.first, k_Places.third, creature_morph::k_MinScale, made.sleeping));
}

TEST_F(CreatureCopyArchetypeTest, ACreatureWithoutABodyHasNoCopyAndNothingIsTossed)
{
	const game_random::testing::ScopedState state({.synced = 0u, .local = 5u});
	EXPECT_FALSE(CreatureCopyArchetype::Create(MakeCreature(1.0f, false), k_Places).has_value());
	EXPECT_FALSE(CreatureCopyArchetype::Create(entt::null, k_Places).has_value());
	EXPECT_EQ(game_random::Current().local, 5u);
	EXPECT_EQ(Reg().Size<CreatureCopy>(), 0u);
}
