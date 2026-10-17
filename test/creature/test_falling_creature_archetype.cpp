/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The falling spell film's copy of the creature: what it is made of and where it stands, and that making it draws no
// random numbers (its start, in the creature animation system, does)

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Archetypes/FallingCreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/FallingCreature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
class FallingCreatureArchetypeTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<Registry>(); }

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A creature with a body, as the land has it, mid-way through an action and looking at something
	static entt::entity MakeCreature(float size, float alignment, bool withMesh = true)
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
		animation.body = {.kind = creature_layers::BodyAction::Kind::Once, .animations = {60, 60, 60}, .timeMs = 120.0f};
		animation.gesture.animation = creature_layers::animations::k_FirstGesture;
		animation.skeleton.parents.assign(12, 0);
		auto& eyes = registry.AssignState<CreatureEyes>(entity);
		eyes.openness = -0.5f;
		eyes.lookAt = glm::vec3(1.0f, 2.0f, 3.0f);
		eyes.blink.timerMs = 3;
		registry.Assign<Transform>(entity, glm::vec3(100.0f, 5.0f, 200.0f), affine::AngleY(1.0f), glm::vec3(0.75f));
		return entity;
	}

private:
	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(FallingCreatureArchetypeTest, TheCopyIsAFreshBodyOfTheCreatureAtTheOriginWithoutBeingACreature)
{
	const game_random::testing::ScopedState state;
	const auto creature = MakeCreature(1.0f, 0.5f);
	const auto copy = FallingCreatureArchetype::Create(creature);
	ASSERT_TRUE(copy.has_value());
	const auto& registry = std::as_const(Reg());
	ASSERT_TRUE((registry.AllOf<FallingCreature, CreatureMorph, CreatureAnimation, CreatureEyes, Transform>(*copy)));
	// nothing that looks for creatures, or for meshes to draw or pick, finds it
	EXPECT_FALSE(registry.AnyOf<Creature>(*copy));
	EXPECT_FALSE(registry.AnyOf<Mesh>(*copy));
	const auto& made = registry.Get<FallingCreature>(*copy);
	EXPECT_EQ(made.source, creature);
	EXPECT_EQ(made.species, CreatureType::Tiger);
	EXPECT_EQ(made.size, 1.0f);
	EXPECT_EQ(made.alignment, std::clamp(creature_pose::TargetMorph(registry, creature).evilGood, -1.0f, 1.0f));
	// its time is not the film's yet, and it is the body that sparkles
	EXPECT_FALSE(made.filmTimed);
	EXPECT_TRUE(made.sparkling);
	// its body's shape as the creature's
	EXPECT_EQ(registry.Get<CreatureMorph>(*copy).shownFatness, 0.25f);
	EXPECT_EQ(registry.Get<CreatureMorph>(*copy).revision, 3u);
	EXPECT_EQ(registry.Get<CreatureAnimation>(*copy).breathPhase, 0.5f);
	EXPECT_EQ(registry.Get<CreatureAnimation>(*copy).skeleton.parents.size(), 12u);
	EXPECT_EQ(registry.Get<CreatureEyes>(*copy).openness, -0.5f);
	// made afresh: it stands, plays nothing, looks nowhere and blinks from the start
	EXPECT_FALSE(creature_layers::IsPlaying(registry.Get<CreatureAnimation>(*copy).body));
	EXPECT_FALSE(registry.Get<CreatureAnimation>(*copy).lookAt.has_value());
	EXPECT_FALSE(registry.Get<CreatureAnimation>(*copy).gesture.animation.has_value());
	EXPECT_FALSE(registry.Get<CreatureEyes>(*copy).lookAt.has_value());
	EXPECT_EQ(registry.Get<CreatureEyes>(*copy).blink.timerMs, creature_eyes::Blink {}.timerMs);
	// at the world's origin facing heading 0, at the creature's scale
	const auto& transform = registry.Get<Transform>(*copy);
	EXPECT_EQ(transform.position, glm::vec3(0.0f));
	EXPECT_EQ(transform.rotation, affine::AngleY(0.0f));
	EXPECT_EQ(transform.scale, glm::vec3(0.75f));
	// the creature itself is left as it was
	EXPECT_EQ(registry.Get<Transform>(creature).position, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_TRUE(creature_layers::IsPlaying(registry.Get<CreatureAnimation>(creature).body));
}

TEST_F(FallingCreatureArchetypeTest, ItsSizeAndAlignmentAreHeldWithinTheirLimitsAndNothingIsDrawn)
{
	const game_random::testing::ScopedState state;
	const auto before = game_random::LocalRand(1000);
	const game_random::testing::ScopedState again;
	for (const auto& [size, alignment] : {std::pair {0.01f, 3.0f}, std::pair {9.0f, -3.0f}})
	{
		const auto copy = FallingCreatureArchetype::Create(MakeCreature(size, alignment));
		ASSERT_TRUE(copy.has_value());
		const auto& made = std::as_const(Reg()).Get<FallingCreature>(*copy);
		EXPECT_EQ(made.size, creature_morph::ClampScale(size));
		EXPECT_GE(made.alignment, -1.0f);
		EXPECT_LE(made.alignment, 1.0f);
	}
	// the local stream is where it was: making the copies drew nothing
	EXPECT_EQ(game_random::LocalRand(1000), before);
}

TEST_F(FallingCreatureArchetypeTest, ACreatureWithoutABodyHasNoCopy)
{
	const auto creature = MakeCreature(1.0f, 0.0f, false);
	EXPECT_FALSE(FallingCreatureArchetype::Create(creature).has_value());
	EXPECT_EQ(std::as_const(Reg()).Size<const FallingCreature>(), 0u);
}
