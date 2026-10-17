/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// MistSystem: only the mists on screen move their animation on, by the game time of the frame, keeping the fraction of
// a step; a mist waiting to be deleted is left as it is.

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MistSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::components::Mist;
using openblack::ecs::components::Transform;
using openblack::ecs::components::Unavailable;
using openblack::ecs::systems::MistSystem;
using Milliseconds = std::chrono::duration<float, std::milli>;

namespace
{
class MistSystemTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	static entt::entity MakeMist(const glm::vec3& position, float size = 10.0f)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Mist>(entity, size, 0xFFFFFFFFu, false, 3.0f, 0, 0.0f);
		return entity;
	}

	static const Mist& MistOf(entt::entity entity) { return Locator::entitiesRegistry::value().Get<const Mist>(entity); }

private:
	test::RestoreService<Locator::entitiesRegistry> _registry;
};

/// The counter and remainder the shared mist clock gives for these steps
graphics::frame_anim::MistClock Expected(const std::vector<float>& steps)
{
	graphics::frame_anim::MistClock clock {0, 0.0f};
	for (const float step : steps)
	{
		graphics::frame_anim::MistAdvance(clock, step);
	}
	return clock;
}
} // namespace

TEST_F(MistSystemTest, OnlyTheMistsOnScreenMoveOn)
{
	const auto near = MakeMist({50.0f, 0.0f, 0.0f});
	const auto far = MakeMist({500.0f, 0.0f, 0.0f});
	MistSystem mists([](const glm::vec3& position, float /*size*/) { return position.x < 100.0f; });

	mists.Update(Milliseconds(1000.0f));

	EXPECT_EQ(MistOf(near).counter, Expected({1000.0f}).counter);
	EXPECT_GT(MistOf(near).counter, 0);
	EXPECT_EQ(MistOf(far).counter, 0);
	EXPECT_EQ(MistOf(far).counterRemainder, 0.0f);
}

TEST_F(MistSystemTest, TheTestGetsEachMistsPositionAndSize)
{
	MakeMist({1.0f, 2.0f, 3.0f}, 25.0f);
	glm::vec3 seenPosition {0.0f};
	float seenSize = 0.0f;
	MistSystem mists([&](const glm::vec3& position, float size) {
		seenPosition = position;
		seenSize = size;
		return true;
	});

	mists.Update(Milliseconds(16.0f));

	EXPECT_EQ(seenPosition, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(seenSize, 25.0f);
}

TEST_F(MistSystemTest, TheFractionOfAStepIsKept)
{
	const auto mist = MakeMist({0.0f, 0.0f, 0.0f});
	MistSystem mists([](const glm::vec3& /*position*/, float /*size*/) { return true; });

	// 16 ms frames move it 4.08 steps each: after 13 frames the fractions have added up to a whole step more than 13
	// whole steps of 4
	constexpr int k_Frames = 13;
	for (int i = 0; i < k_Frames; ++i)
	{
		mists.Update(Milliseconds(16.0f));
	}

	const auto expected = Expected(std::vector<float>(k_Frames, 16.0f));
	EXPECT_EQ(MistOf(mist).counter, expected.counter);
	EXPECT_EQ(MistOf(mist).counterRemainder, expected.remainder);
	EXPECT_EQ(MistOf(mist).counter, 53);
}

TEST_F(MistSystemTest, NothingMovesWhileTheGameIsPaused)
{
	const auto mist = MakeMist({0.0f, 0.0f, 0.0f});
	MistSystem mists([](const glm::vec3& /*position*/, float /*size*/) { return true; });

	mists.Update(Milliseconds(0.0f));

	EXPECT_EQ(MistOf(mist).counter, 0);
	EXPECT_EQ(MistOf(mist).counterRemainder, 0.0f);
}

TEST_F(MistSystemTest, AMistWaitingToBeDeletedIsLeftAsItIs)
{
	const auto mist = MakeMist({0.0f, 0.0f, 0.0f});
	Locator::entitiesRegistry::value().Assign<Unavailable>(mist);
	MistSystem mists([](const glm::vec3& /*position*/, float /*size*/) { return true; });

	mists.Update(Milliseconds(1000.0f));

	EXPECT_EQ(MistOf(mist).counter, 0);
}
