/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature fizz system over a registry of its own, with a fake of the sound tags that records the sounds made at a
// point: the teleport's energise sounded only for going right out of sight, the fizz shown on the creature a turn
// behind the one that moves, the component gone the turn after it is back in full sight, one that has just fizzed right
// out for good taken away and one put out of sight at once kept, and the eyes' static sliding on every frame

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Audio/Game/BankTables.h"
#include "Creature/CreatureFizz.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/CreatureFizzSystem.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureFizzSystem;

namespace
{
/// The sound tags: each sound made at a point is kept with its sample, mode, loops, 3D and bank; everything else does
/// nothing
class FakeSoundTags final: public ecs::systems::SoundTagSystemInterface
{
public:
	struct Point
	{
		glm::vec3 point;
		int sample;
		int mode;
		int loops;
		bool is3D;
		audio::SfxBank bank;
	};
	std::vector<Point> points;

	void ProcessTurn() override {}
	void SetActive(TagId, bool) override {}
	TagId CreatePointSound(const glm::vec3& point, int sample, int mode, int loops, bool, bool is3D, audio::SfxBank bank,
	                       int) override
	{
		points.push_back({.point = point, .sample = sample, .mode = mode, .loops = loops, .is3D = is3D, .bank = bank});
		return static_cast<TagId>(points.size());
	}
	TagId CreatePointSound(const glm::vec3&, int, int, int, bool, bool, audio::AtmosType, int) override { return 0; }
	TagId Create(entt::entity, const glm::vec3&, int, bool, int, int, bool, bool, audio::SfxBank, int) override { return 0; }
	TagId CreateAtMarker(const glm::vec3&, int, int, int, bool, audio::SfxBank) override { return 0; }
	void Remove(entt::entity, int, audio::SfxBank, bool) override {}
	void Delete(TagId) override {}
	[[nodiscard]] bool Exists(TagId) const override { return false; }
	void Clear() override {}
	[[nodiscard]] std::optional<glm::vec3> TagSoundPoint(TagId) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> Point(TagId) const override { return std::nullopt; }
};

class CreatureFizzSystemTest: public ::testing::Test
{
protected:
	CreatureFizzSystemTest()
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_sounds = &static_cast<FakeSoundTags&>(Locator::soundTagSystem::emplace<FakeSoundTags>());
	}

	/// A thing standing at a point; it is not a creature, so taking it away for good only destroys it
	static entt::entity Make(glm::vec3 position = glm::vec3(120.0f, 0.0f, 80.0f))
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity,
		                           Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
		return entity;
	}

	/// How fizzed it is drawn
	static float Shown(entt::entity entity)
	{
		const auto* spells = std::as_const(Locator::entitiesRegistry::value()).TryGet<const CreatureSpells>(entity);
		return spells != nullptr ? spells->fizz : 0.0f;
	}

	static bool HasFizz(entt::entity entity)
	{
		return std::as_const(Locator::entitiesRegistry::value()).AllOf<CreatureFizz>(entity);
	}

	const test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	const test::RestoreService<Locator::soundTagSystem> _restoreSounds;
	FakeSoundTags* _sounds {nullptr};
	CreatureFizzSystem _system;
};
} // namespace

TEST_F(CreatureFizzSystemTest, OnlyGoingRightOutOfSightSoundsTheEnergiseWhereItStands)
{
	const auto creature = Make(glm::vec3(120.0f, 0.0f, 80.0f));
	_system.SetFizz(creature, 1.0f, 2.0f, false);
	ASSERT_EQ(_sounds->points.size(), 1u);
	const auto& sound = _sounds->points.front();
	EXPECT_NEAR(sound.point.x, 120.0f, 0.01f);
	EXPECT_NEAR(sound.point.z, 80.0f, 0.01f);
	EXPECT_EQ(sound.sample, 39);
	EXPECT_EQ(sound.mode, 2);
	EXPECT_EQ(sound.loops, 0);
	EXPECT_TRUE(sound.is3D);
	EXPECT_EQ(sound.bank, audio::SfxBank::InGame);
	// back in, part way, or beyond before it is kept within bounds: no sound
	_system.SetFizz(creature, 0.0f, 2.0f, false);
	_system.SetFizz(creature, 0.75f, 0.0f, false);
	_system.SetFizz(creature, 1.5f, 0.0f, false);
	EXPECT_EQ(_sounds->points.size(), 1u);
}

TEST_F(CreatureFizzSystemTest, WithoutSoundTagsItFizzesSilently)
{
	const auto creature = Make();
	Locator::soundTagSystem::reset();
	_system.SetFizz(creature, 1.0f, 0.0f, false);
	EXPECT_FLOAT_EQ(_system.FizzOf(creature), 1.0f);
}

TEST_F(CreatureFizzSystemTest, ItGoesOnceBackInFullSight)
{
	const auto creature = Make();
	_system.SetFizz(creature, 1.0f, 0.0f, false);
	_system.SetFizz(creature, 0.0f, 2.0f, false);
	ASSERT_TRUE(HasFizz(creature));
	for (int turn = 0; turn < 19; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_TRUE(HasFizz(creature));
	EXPECT_GT(_system.FizzOf(creature), 0.0f);
	// a twentieth a turn: past full sight on the twentieth turn, still drawn as it was before that step
	_system.ProcessTurn();
	EXPECT_EQ(_system.FizzOf(creature), 0.0f);
	EXPECT_GT(Shown(creature), 0.0f);
	EXPECT_TRUE(HasFizz(creature));
	// drawn in full sight the turn after, and then nothing is left of it
	_system.ProcessTurn();
	EXPECT_FALSE(HasFizz(creature));
	EXPECT_EQ(Shown(creature), 0.0f);
}

TEST_F(CreatureFizzSystemTest, ItIsDrawnAsItWasBeforeTheTurnsStep)
{
	const auto creature = Make();
	_system.SetFizz(creature, 1.0f, 2.0f, false);
	// set going, it is not drawn fizzed until its next turn
	EXPECT_EQ(Shown(creature), 0.0f);
	auto expected = creature_fizz::SetFizz({}, 1.0f, 2.0f, false).fizz;
	for (int turn = 1; turn <= 21; ++turn)
	{
		const float before = expected.now;
		expected = creature_fizz::Step(expected, game_clock::MsPerTurn());
		_system.ProcessTurn();
		EXPECT_EQ(Shown(creature), before) << "turn " << turn;
		EXPECT_EQ(_system.FizzOf(creature), expected.now) << "turn " << turn;
	}
	// right out on the twentieth turn's step, it is first drawn so on the twenty-first
	EXPECT_EQ(Shown(creature), 1.0f);
}

TEST_F(CreatureFizzSystemTest, FizzedRightOutItStaysUnlessItGoesForGood)
{
	const auto stays = Make();
	const auto goes = Make();
	_system.SetFizz(stays, 1.0f, 2.0f, false);
	_system.SetFizz(goes, 1.0f, 2.0f, true);
	for (int turn = 0; turn < 25; ++turn)
	{
		_system.ProcessTurn();
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	ASSERT_TRUE(registry.Valid(stays));
	EXPECT_EQ(_system.FizzOf(stays), 1.0f);
	EXPECT_TRUE(HasFizz(stays));
	EXPECT_FALSE(registry.Valid(goes));
	EXPECT_EQ(_system.FizzOf(goes), 0.0f);
}

TEST_F(CreatureFizzSystemTest, PutRightOutOfSightAtOnceItIsNotTakenAway)
{
	// already right out and still, then set to go for good: the fizz does not move, so it never goes
	const auto still = Make();
	_system.SetFizz(still, 1.0f, 0.0f, false);
	_system.SetFizz(still, 1.0f, 2.0f, true);
	// put right out for good at once
	const auto atOnce = Make();
	_system.SetFizz(atOnce, 1.0f, 0.0f, true);
	for (int turn = 0; turn < 30; ++turn)
	{
		_system.ProcessTurn();
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	ASSERT_TRUE(registry.Valid(still));
	EXPECT_EQ(_system.FizzOf(still), 1.0f);
	EXPECT_EQ(Shown(still), 1.0f);
	ASSERT_TRUE(registry.Valid(atOnce));
	EXPECT_EQ(_system.FizzOf(atOnce), 1.0f);
}

TEST_F(CreatureFizzSystemTest, WithoutAFizzItIsInFullSight)
{
	const auto creature = Make();
	EXPECT_EQ(_system.FizzOf(creature), 0.0f);
	EXPECT_FALSE(HasFizz(creature));
	// nor does a turn give it one
	_system.ProcessTurn();
	EXPECT_FALSE(HasFizz(creature));
	EXPECT_EQ(_system.FizzOf(entt::null), 0.0f);
}

TEST_F(CreatureFizzSystemTest, TheEyesStaticSlidesOnEveryFrameWithNothingFizzing)
{
	EXPECT_EQ(_system.EyeStaticScroll(), glm::vec2(0.0f));
	_system.UpdateFrame(1.0f);
	const auto once = _system.EyeStaticScroll();
	EXPECT_NEAR(once.x, 0.1f, 1e-6f);
	EXPECT_NEAR(once.y, 0.2f, 1e-6f);
	_system.UpdateFrame(0.0f);
	EXPECT_EQ(_system.EyeStaticScroll(), once);
}
