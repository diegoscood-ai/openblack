/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The services in front of our miracles: the teleport cast rule asks the teleport stones whether a stone may go at the
// point, and both flock classes' own steps (the particle effect, the start and the turn) go to the flock miracles the
// miracles' service holds. The locator only injects the fakes; each test reads the fake it holds.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/TeleportSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/TeleportSystemInterface.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/FlockMiracle.h"
#include "Magic/FlockMiracleInterface.h"
#include "support/MagicFakes.h"

using namespace openblack;
using openblack::ecs::components::SpellClass;

namespace
{
/// A fake of the teleport stones that records the point the cast rule asks about
class FakeTeleportSystem final: public ecs::systems::TeleportSystemInterface
{
public:
	[[nodiscard]] bool CanPlaceStone(glm::vec3 point) const override
	{
		asked = point;
		return canPlace;
	}
	[[nodiscard]] bool ShouldReact(entt::entity /*stone*/, entt::entity /*living*/) const override { return false; }
	bool DoTeleport(entt::entity /*stone*/, entt::entity /*living*/, bool /*forced*/) override { return false; }
	bool DropOnStone(entt::entity /*villager*/, entt::entity /*stone*/, PlayerNames /*dropper*/) override { return false; }
	[[nodiscard]] std::optional<entt::entity> RouteStoneFor(PlayerNames /*player*/, glm::vec3 /*worshipper*/,
	                                                        glm::vec3 /*site*/, float /*maxDistance*/) const override
	{
		return std::nullopt;
	}
	void ProcessTurn() override {}
	void Reset() override {}
	[[nodiscard]] bool CanDropOnStone(entt::entity /*villager*/, entt::entity /*stone*/) const override { return false; }
	void RegisterDestination(entt::entity /*stone*/, entt::entity /*living*/, glm::vec3 /*destination*/) override {}
	void Update(float /*seconds*/) override {}
	void RunDebugHooks() override {}

	mutable std::optional<glm::vec3> asked;
	bool canPlace {false};
};

/// What the spells last asked of the flock miracles
struct FlockCall
{
	entt::entity spell {entt::null};
	glm::vec3 position {0.0f};
	magic::SpellCastData* castData {nullptr};
	glm::vec3 handPos {0.0f};
};

/// A fake of the flock miracles that records the spells' calls and answers with fixed values
class FakeFlocks final: public magic::FlockMiracleInterface
{
public:
	[[nodiscard]] ParticleType ParticleTypeOf(entt::entity spell) const override
	{
		particleTypeOf = FlockCall {.spell = spell};
		return ParticleType::FlockGroundDust;
	}
	int Start(entt::entity spell, const glm::vec3& position, magic::SpellCastData* castData,
	          const psys::ProcessInfo& info) override
	{
		start = FlockCall {.spell = spell, .position = position, .castData = castData, .handPos = info.handPos};
		return 1;
	}
	int ProcessTurn(entt::entity spell) override
	{
		processTurn = FlockCall {.spell = spell};
		return 5;
	}

	mutable std::optional<FlockCall> particleTypeOf;
	std::optional<FlockCall> start;
	std::optional<FlockCall> processTurn;
};

/// A fake of the miracles' service that only holds the fake flock miracles; the rest is inert
class FakeMagicSystem final: public test::InertMagicSystem
{
public:
	[[nodiscard]] magic::FlockMiracleInterface* Flocks() override { return &flocks; }

	FakeFlocks flocks;
};

class TeleportCastRuleTest: public ::testing::Test
{
protected:
	void SetUp() override { _fake = &static_cast<FakeTeleportSystem&>(Locator::teleportSystem::emplace<FakeTeleportSystem>()); }
	void TearDown() override
	{
		Locator::teleportSystem::reset();
		_fake = nullptr;
	}

	[[nodiscard]] FakeTeleportSystem& Fake() const { return *_fake; }

private:
	FakeTeleportSystem* _fake {nullptr};
};

class FlockOpsTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the flock animals' dying the spells' class table registers, and the fake's own stores, where the class table
		// is registered on its first use
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		_fake = &static_cast<FakeMagicSystem&>(Locator::magicSystem::emplace<FakeMagicSystem>());
	}
	void TearDown() override
	{
		// the listener resets the fake
		Locator::animalSystem::reset();
		_fake = nullptr;
	}

	[[nodiscard]] FakeFlocks& Flocks() const { return _fake->flocks; }

private:
	FakeMagicSystem* _fake {nullptr};
};

} // namespace

TEST_F(TeleportCastRuleTest, AStoneMayGoOnlyWhereTheStonesSay)
{
	const glm::vec3 point(120.5f, 3.0f, 340.25f);
	Fake().canPlace = false;
	EXPECT_FALSE(magic::cast_rules::CanCastAt(MagicType::Teleport, point));
	ASSERT_TRUE(Fake().asked.has_value());
	EXPECT_EQ(*Fake().asked, point);

	Fake().asked.reset();
	Fake().canPlace = true;
	EXPECT_TRUE(magic::cast_rules::CanCastAt(MagicType::Teleport, point));
	ASSERT_TRUE(Fake().asked.has_value());
	EXPECT_EQ(*Fake().asked, point);
}

TEST_F(TeleportCastRuleTest, OtherMiraclesDoNotAskTheStones)
{
	EXPECT_TRUE(magic::cast_rules::CanCastAt(MagicType::FlockFlying, glm::vec3(10.0f, 0.0f, 10.0f)));
	EXPECT_FALSE(Fake().asked.has_value());
}

TEST_F(FlockOpsTest, BothFlockClassesHandTheirStepsToTheFlockMiracles)
{
	for (const auto spellClass : {SpellClass::FlockFlying, SpellClass::FlockGround})
	{
		Flocks().particleTypeOf.reset();
		Flocks().start.reset();
		Flocks().processTurn.reset();
		const auto spell = entt::entity {spellClass == SpellClass::FlockFlying ? 21u : 22u};
		const auto& ops = magic::OpsOf(spellClass);

		EXPECT_EQ(ops.particleType(spell), ParticleType::FlockGroundDust);
		ASSERT_TRUE(Flocks().particleTypeOf.has_value());
		EXPECT_EQ(Flocks().particleTypeOf->spell, spell);

		magic::SpellCastData cast;
		psys::ProcessInfo info;
		info.handPos = glm::vec3(1.0f, 2.0f, 3.0f);
		const glm::vec3 position(400.0f, 5.0f, 600.0f);
		EXPECT_EQ(ops.initWithPos(spell, position, &cast, info), 1);
		ASSERT_TRUE(Flocks().start.has_value());
		EXPECT_EQ(Flocks().start->spell, spell);
		EXPECT_EQ(Flocks().start->position, position);
		EXPECT_EQ(Flocks().start->castData, &cast);
		EXPECT_EQ(Flocks().start->handPos, info.handPos);

		EXPECT_EQ(ops.process(spell), 5);
		ASSERT_TRUE(Flocks().processTurn.has_value());
		EXPECT_EQ(Flocks().processTurn->spell, spell);
	}
}

TEST(FlockMiracle, TheGroundFlocksEffectIsTheWolvesDust)
{
	// the ground flock's own effect does not depend on its caster: the class alone picks it
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto spell = registry.Create();
	registry.Assign<ecs::components::Spell>(spell).spellClass = SpellClass::FlockGround;
	const magic::FlockMiracle flocks;
	EXPECT_EQ(flocks.ParticleTypeOf(spell), ParticleType::FlockGroundDust);
	Locator::entitiesRegistry::reset();
}

TEST(TeleportSystem, TheRouteStoneIsThePlayersStoneAtTheFoundPlace)
{
	// the worshipper's route goes through the player's own stone list (newest first), each stone at its map position,
	// and gives back the stone at the place the route search found; with no land the map position is the world point
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto stoneAt = [&registry](float x) {
		const auto stone = registry.Create();
		registry.Assign<ecs::components::Transform>(stone, glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return stone;
	};
	const auto player = [&registry](PlayerNames name, std::vector<entt::entity> stones) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Player>(entity, name);
		registry.Assign<ecs::components::PlayerMagic>(entity).teleportStones = std::move(stones);
	};
	const auto farStone = stoneAt(60.0f);
	const auto nearStone = stoneAt(10.0f);
	const auto other = stoneAt(5.0f);
	player(PlayerNames::PLAYER_ONE, {farStone, nearStone});
	player(PlayerNames::PLAYER_TWO, {other});
	const ecs::systems::TeleportSystem stones;

	// the stone nearest the worshipper is the list's second: that stone, not the first
	EXPECT_EQ(stones.RouteStoneFor(PlayerNames::PLAYER_ONE, glm::vec3(0.0f), glm::vec3(65.0f, 0.0f, 0.0f), 100.0f), nearStone);
	// too far through the stones: none
	EXPECT_EQ(stones.RouteStoneFor(PlayerNames::PLAYER_ONE, glm::vec3(0.0f), glm::vec3(300.0f, 0.0f, 0.0f), 100.0f),
	          std::nullopt);
	// another player's stones are its own
	EXPECT_EQ(stones.RouteStoneFor(PlayerNames::PLAYER_TWO, glm::vec3(0.0f), glm::vec3(65.0f, 0.0f, 0.0f), 100.0f), other);
	Locator::entitiesRegistry::reset();
}
