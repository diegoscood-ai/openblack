/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The alignment service: the players' alignments go to the player system's, the camera's is the interface alignment,
// and the sky moves toward its target by game time

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>

#include <algorithm>
#include <array>
#include <bit>
#include <functional>
#include <memory>
#include <string_view>

#include <LHVM.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "EngineConfig.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::ecs::components::PlayerMagic;
using openblack::ecs::systems::AlignmentSystem;
using openblack::ecs::systems::PlayerSystemInterface;

namespace
{
/// Hands out one alignment for every player and records which player was asked for
class FakePlayerSystem final: public PlayerSystemInterface
{
public:
	void RegisterPlayers() override {}
	void AddPlayer(entt::entity) override {}
	[[nodiscard]] entt::entity GetPlayer(PlayerNames) const override { return entt::null; }
	void AddCreature(entt::entity) override {}
	// no creatures
	[[nodiscard]] std::optional<entt::entity> GetPrimaryCreature(PlayerNames) const override { return std::nullopt; }
	void SetPrimaryCreature(PlayerNames, std::optional<entt::entity>) override {}
	void KeepForNextLand() override {}
	void TakeUpKept(entt::entity) override {}
	[[nodiscard]] PlayerNames GetLocalPlayer() const override { return PlayerNames::PLAYER_ONE; }
	void ClearPlayers() override {}
	[[nodiscard]] ecs::components::Alignment& Alignment(PlayerNames name) override
	{
		asked = name;
		return alignment;
	}
	[[nodiscard]] PlayerMagic& MagicWithoutEntity(PlayerNames) override { return magic; }
	void ClearMagicWithoutEntity() override {}
	[[nodiscard]] ecs::components::InterfaceAlignment& InterfaceAlignmentWithoutEntity() override { return interfaceAlignment; }
	[[nodiscard]] ecs::components::InfluenceCrossing& InfluenceCrossingWithoutEntity() override { return crossing; }

	ecs::components::Alignment alignment {};
	PlayerMagic magic {};
	ecs::components::InterfaceAlignment interfaceAlignment {};
	ecs::components::InfluenceCrossing crossing {};
	PlayerNames asked {PlayerNames::_COUNT};
};

class Alignment: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_players = &static_cast<FakePlayerSystem&>(Locator::playerSystem::emplace<FakePlayerSystem>());
		Locator::entitiesRegistry::reset();
		Locator::camera::reset();
		Locator::config::reset();
		ecs::effects::alignment::ResetInterfaceAlignment();
	}
	void TearDown() override { ecs::effects::alignment::ResetInterfaceAlignment(); }

	[[nodiscard]] FakePlayerSystem& Players() const { return *_players; }

private:
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::config> _restoreConfig;
	FakePlayerSystem* _players {nullptr};
};
} // namespace

TEST_F(Alignment, readsThePlayerSystemsAlignment)
{
	Players().alignment.value = -0.5f;
	const AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetPlayerAlignment(PlayerNames::PLAYER_TWO), -0.5f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_TWO);
}

TEST_F(Alignment, setHoldsTheAlignmentBetweenMinusOneAndOne)
{
	AlignmentSystem alignment;
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_EQ(Players().alignment.value, 0.25f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_ONE);
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, 3.0f);
	EXPECT_EQ(Players().alignment.value, 1.0f);
	alignment.SetPlayerAlignment(PlayerNames::PLAYER_ONE, -3.0f);
	EXPECT_EQ(Players().alignment.value, -1.0f);
}

TEST_F(Alignment, addHoldsTheSumAndLeavesThePendingChange)
{
	Players().alignment = {.value = 0.75f, .pending = 0.125f};
	AlignmentSystem alignment;
	alignment.AddPlayerAlignment(PlayerNames::PLAYER_THREE, 0.5f);
	EXPECT_EQ(Players().alignment.value, 1.0f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_THREE);
	alignment.AddPlayerAlignment(PlayerNames::PLAYER_THREE, -0.25f);
	EXPECT_EQ(Players().alignment.value, 0.75f);
	EXPECT_EQ(Players().alignment.pending, 0.125f);
}

TEST_F(Alignment, theCameraStartsNeutralAndATurnWithoutACameraKeepsIt)
{
	Players().alignment.value = 1.0f;
	AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetCameraAlignment(), 0.0f);
	alignment.UpdateTurn();
	EXPECT_EQ(alignment.GetCameraAlignment(), 0.0f);
	EXPECT_EQ(ecs::effects::alignment::GetInterfaceAlignment(), 0.5f);
}

TEST_F(Alignment, theSkyTurnsTowardItsTargetByGameTime)
{
	if (std::getenv("OPENBLACK_TEST_SKY_ALIGNMENT") != nullptr)
	{
		GTEST_SKIP() << "the sky's target is set from the environment";
	}
	// the debug slider sets the sky's target, as it does the renderer's
	Locator::config::emplace().skyAlignment = 1.0f;
	AlignmentSystem alignment;
	EXPECT_EQ(alignment.GetSkyAlignment(), 0.0f);
	alignment.Update(250ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.25f);
	// paused: no game time, no turn
	alignment.Update(0ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.25f);
	// it stops at the target
	alignment.Update(1000ms);
	EXPECT_EQ(alignment.GetSkyAlignment(), 1.0f);
	Locator::config::value().skyAlignment = -1.0f;
	alignment.Update(500ms);
	EXPECT_FLOAT_EQ(alignment.GetSkyAlignment(), 0.5f);
}

TEST_F(Alignment, withoutAnOverrideTheSkyFollowsTheCamera)
{
	if (std::getenv("OPENBLACK_TEST_SKY_ALIGNMENT") != nullptr)
	{
		GTEST_SKIP() << "the sky's target is set from the environment";
	}
	AlignmentSystem alignment;
	alignment.Update(1000ms);
	EXPECT_EQ(alignment.GetSkyAlignment(), alignment.GetCameraAlignment());
}

TEST(AlignmentStep, APendingChangeIsFoldedInByTheChangePerTurnAndCleared)
{
	float pending = 0.5f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::StepPending(0.25f, pending, 0.01f), 0.25f + 0.005f);
	EXPECT_FLOAT_EQ(pending, 0.0f);
	// the pending change counts at most a whole one either way
	pending = -3.0f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::StepPending(0.25f, pending, 0.01f), 0.24f);
	EXPECT_FLOAT_EQ(pending, 0.0f);
	// and the result stays between -1 and 1
	pending = 1.0f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::StepPending(0.995f, pending, 0.01f), 1.0f);
	pending = -1.0f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::StepPending(-0.995f, pending, 0.01f), -1.0f);
	// nothing pending leaves the value
	pending = 0.0f;
	EXPECT_FLOAT_EQ(ecs::effects::alignment::StepPending(-0.3f, pending, 0.01f), -0.3f);
}

TEST(CreatureDeedAlignment, ThePlanOfACreatureMovesItsAlignmentEachTurn)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	// The tables are large: made on the heap
	auto info = std::make_unique<InfoConstants>();
	const auto row = static_cast<size_t>(CreatureType::Cow);
	info->creature.at(row).alignmentChangePerTurn = 0.01f;
	// the species scale is read from the balance row
	info->differentCreature.at(row).alignmentChangeScale = 0.2f;
	info->creatureInitialDesire.at(static_cast<size_t>(creature_desires::Desire::Compassion)).alignmentChange = 1.0f;
	// The first row of the alignment table gives the weight of an eaten villager
	info->alignment.at(0).villager = -1.0f;
	for (auto& villagerRow : info->villager)
	{
		villagerRow.alignmentType = AlignmentType::Villager;
	}
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(registry, info.get()));
	const auto villager = registry.Create();
	auto& eaten = registry.Assign<ecs::components::Villager>(villager);
	eaten.tribe = info->villager.at(0).tribeType;
	eaten.number = info->villager.at(0).villagerNumber;
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature).species = CreatureType::Cow;
	auto& mind = registry.Assign<ecs::components::CreatureMindState>(creature);

	// Without a plan the alignment stays
	ecs::effects::alignment::ProcessCreatures();
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Creature>(creature).alignment, 0.0f);

	// A plan to be nice moves it to good
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Compassion};
	ecs::effects::alignment::ProcessCreatures();
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Creature>(creature).alignment, 0.2f * 0.01f);

	// A plan to eat a villager moves it to evil, more strongly against the good alignment
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .object = entt::to_integral(villager)};
	const float before = registry.Get<ecs::components::Creature>(creature).alignment;
	ecs::effects::alignment::ProcessCreatures();
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Creature>(creature).alignment,
	                before - (0.2f * (1.0f + (before * 0.5f)) * 0.01f));

	// A plan that is not active does not count, and nothing is left pending
	mind.planActive = false;
	const float after = registry.Get<ecs::components::Creature>(creature).alignment;
	ecs::effects::alignment::ProcessCreatures();
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Creature>(creature).alignment, after);
	EXPECT_FLOAT_EQ(registry.Get<ecs::components::Creature>(creature).pendingAlignment, 0.0f);
	Locator::dynamicsSystem::reset();
}

TEST(EffectAlignment, APlantTakesThePlantColumn)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	auto info = std::make_unique<InfoConstants>();
	// every tree row of info.dat has the alignment type PLANT (7)
	for (auto& tree : info->tree)
	{
		tree.alignmentType = AlignmentType::Plant;
	}
	// an effect's row of the alignment table is one value per alignment type, in the type's order: the plant's is the
	// eighth
	std::array<float, 14> columns {};
	columns.fill(0.9f);
	columns.at(7) = 0.3f;
	info->alignment.at(3) = std::bit_cast<GAlignmentInfo>(columns);
	Locator::infoConstants::emplace(*info);
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(registry, info.get()));
	const auto tree = registry.Create();
	registry.Assign<ecs::components::Transform>(tree, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Tree>(tree, ecs::components::Tree {.type = TreeInfo::Bush, .maxSize = 1.0f});

	const auto target = ecs::effects::alignment::TargetOf(tree, ecs::effects::EffectValues {});
	EXPECT_EQ(target.type, AlignmentType::Plant);
	EXPECT_FLOAT_EQ(ecs::effects::alignment::AlignmentFactor(*info, 3, target.type), 0.3f);
	Locator::dynamicsSystem::reset();
}

// GET_ALIGNMENT answers the player's alignment as a float; SET_ALIGNMENT, the player on top and the value under it,
// adds the value to the alignment, held between -1 and 1, and refuses a value outside -1..1 without a change
// (docs/wiki/magic/influence-and-alignment.md#player-alignment)
TEST_F(Alignment, scriptsReadTheAlignmentAndAddToItWithinMinusOneAndOne)
{
	const test::RestoreService<Locator::vm> restoreVm;
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	auto& vm = Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto native = [&api](std::string_view name) {
		const auto& table = api.GetFunctionsTable();
		const auto found = std::ranges::find_if(table, [name](const auto& f) { return f.name == name; });
		EXPECT_NE(found, table.end()) << name;
		return found != table.end() ? found->impl : std::function<void()> {};
	};

	Players().alignment.value = 0.5f;
	vm.Pushi(1);
	native("GET_ALIGNMENT")();
	lhvm::DataType type {};
	const auto read = vm.Pop(type);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_EQ(read.floatVal, 0.5f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_TWO);

	vm.Pushf(0.25f);
	vm.Pushi(0);
	native("SET_ALIGNMENT")();
	EXPECT_EQ(Players().alignment.value, 0.75f);
	EXPECT_EQ(Players().asked, PlayerNames::PLAYER_ONE);
	// 1 and -1 are taken, and the sum is held between -1 and 1
	vm.Pushf(1.0f);
	vm.Pushi(0);
	native("SET_ALIGNMENT")();
	EXPECT_EQ(Players().alignment.value, 1.0f);
	vm.Pushf(-1.0f);
	vm.Pushi(0);
	native("SET_ALIGNMENT")();
	EXPECT_EQ(Players().alignment.value, 0.0f);
	// outside -1..1 nothing changes
	vm.Pushf(1.5f);
	vm.Pushi(0);
	native("SET_ALIGNMENT")();
	EXPECT_EQ(Players().alignment.value, 0.0f);
	vm.Pushf(-1.5f);
	vm.Pushi(0);
	native("SET_ALIGNMENT")();
	EXPECT_EQ(Players().alignment.value, 0.0f);
}

// Food or wood given to or taken from a town by hand: the change in the town's desire times the give multiplier for
// an amount above 0 and the take one otherwise, weighed by the alignment (against where it leans counts more), goes to
// the pending change (docs/wiki/world/buildings.md, the hand's branch). The game's data has 0.5 for both multipliers:
// the take one is set apart here to tell which is used
TEST_F(Alignment, aResourceGivenOrTakenByHandMovesThePendingChangeByTheTownsDesire)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->town.giveResourceAligmnetChangeMultiplier = 0.5f;
	info->town.takeResourceAligmnetChangeMultiplier = 0.25f;
	Locator::infoConstants::emplace(*info);
	Players().alignment = {.value = 0.5f, .pending = 0.0f};

	// given: 0.25 x 0.5, toward the good it leans to, x (1 - 0.25)
	ecs::effects::alignment::UpdateForResource(PlayerNames::PLAYER_ONE, entt::null, 3, 0.25f);
	EXPECT_FLOAT_EQ(Players().alignment.pending, 0.09375f);
	// taken: 0.25 x 0.25 x 0.75
	ecs::effects::alignment::UpdateForResource(PlayerNames::PLAYER_ONE, entt::null, -3, 0.25f);
	EXPECT_FLOAT_EQ(Players().alignment.pending, 0.09375f + 0.046875f);
	// nothing is not above 0: the take multiplier; against the lean, x (1 + 0.25)
	ecs::effects::alignment::UpdateForResource(PlayerNames::PLAYER_ONE, entt::null, 0, -0.5f);
	EXPECT_FLOAT_EQ(Players().alignment.pending, 0.09375f + 0.046875f - 0.15625f);
	// the alignment itself waits for the turn
	EXPECT_EQ(Players().alignment.value, 0.5f);
}
