/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the land's animals do about their needs, pinned to the original game: where an animal may turn to, where a
// grazer looks for grass, how a hungry grazer eats from a food pile, how a hunter finds its prey and what it takes, a
// body killed again and the tortoise's row. The kinds' numbers are info.dat's (the frozen game); the rules are on
// docs/wiki/beings/animals.md and docs/wiki/beings/animal-life.md. A fake flat land, the game's map cells and the
// animals' own state; no game data but in the one integration test, which skips itself without it.

#define LOCATOR_IMPLEMENTATIONS

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/EventManager.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Events/CreatureMimicEvents.h"
#include "ECS/LastDropped.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Resources/Resources.h"
#include "support/DynamicsFakes.h"
#include "support/LandFakes.h"
#include "support/LivingActionFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace ai = openblack::ecs::animal_ai::detail;
using openblack::ecs::animal_ai::AnimalState;

namespace
{
constexpr uint32_t k_Now = 100000;
constexpr int32_t k_TurnsPerYear = 1500;

SpeedGroup Speeds(uint32_t normal, uint32_t fleeing, uint32_t second, uint32_t third, uint32_t fourth)
{
	return {
	    .speedDefault = static_cast<SpeedState>(normal),
	    .speedFleeing = static_cast<SpeedState>(fleeing),
	    .speed2 = static_cast<SpeedState>(second),
	    .speed3 = static_cast<SpeedState>(third),
	    .speed4 = static_cast<SpeedState>(fourth),
	    .speed5 = SpeedState::MetresSec_0,
	};
}

/// The map a reaction's spread asks to sync its cells outside the turn: the tests file the cells themselves
class CellsFiledByTheTest final: public ecs::MapInterface
{
public:
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetFixedInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const CellId& /*cellId*/) const override { return {}; }
	[[nodiscard]] std::span<const entt::entity> GetMobileInGridCell(const glm::vec3& /*pos*/) const override { return {}; }
	[[nodiscard]] std::vector<entt::entity> GetAllInCell(glm::ivec2 /*cell*/) const override { return {}; }
	void Sync() override {}
	void Refile(entt::entity /*entity*/) override {}
};

/// The parts of info.dat's animal rows these tests read, as the frozen game has them
std::unique_ptr<InfoConstants> GameAnimalRows()
{
	auto info = std::make_unique<InfoConstants>();
	// every animal row is of the animal type, so an animal goes in its map cell's mobile list
	for (auto& animal : info->animal)
	{
		animal.type = ObjectType::Animal;
	}
	const auto row = [&info](AnimalInfo type) -> GAnimalInfo& { return info->animal.at(static_cast<size_t>(type)); };
	// the lion: hungry after 12000 turns, sleepy after 18000; it eats meat, looks 64 cells round and chases within 100 m
	auto& lion = row(AnimalInfo::Lion);
	lion.speedGroup = Speeds(983, 5243, 819, 6554, 5243);
	lion.moveState = static_cast<LivingStates>(1);
	lion.grownUpAge = 13;
	lion.hunger = 12000;
	lion.sleep = 18000;
	lion.needToBreed = 3000;
	lion.turnAngle = static_cast<TurnangleStates>(64);
	lion.viewAngle = TurnangleStates::Degrees_180;
	lion.needsFoodTypes = FoodType::Meat;
	lion.foodType = FoodType::Meat;
	lion.foodValue = 900.0f;
	lion.farSightDistance = 64;
	lion.huntingDistance = 100.0f;
	lion.stalkingDistance = 100.0f;
	lion.attackDistance = 50.0f;
	lion.chaseTime = 200;
	// the wolf: the lion's numbers (only the pack's own rules are under test)
	auto& wolf = row(AnimalInfo::Wolf);
	wolf = lion;
	// the sheep: 0.75 m/s, a turn of 64, meat worth 800
	auto& sheep = row(AnimalInfo::Sheep);
	sheep.speedGroup = Speeds(492, 2621, 492, 492, 492);
	sheep.moveState = static_cast<LivingStates>(1);
	sheep.grownUpAge = 13;
	sheep.hunger = 50;
	sheep.sleep = 1000;
	sheep.needToBreed = 3000;
	sheep.turnAngle = static_cast<TurnangleStates>(64);
	sheep.viewAngle = TurnangleStates::Degrees_180;
	sheep.needsFoodTypes = FoodType::VegetableGraze;
	sheep.foodType = FoodType::Meat;
	sheep.foodValue = 800.0f;
	// the cow: 0.75 m/s and a turn of 34 (6 degrees)
	auto& cow = row(AnimalInfo::Cow);
	cow.speedGroup = Speeds(492, 2621, 492, 492, 492);
	cow.moveState = static_cast<LivingStates>(1);
	cow.grownUpAge = 13;
	cow.hunger = 50;
	cow.sleep = 1000;
	cow.needToBreed = 3000;
	cow.turnAngle = static_cast<TurnangleStates>(34);
	cow.viewAngle = TurnangleStates::Degrees_180;
	cow.needsFoodTypes = FoodType::VegetableGraze;
	cow.foodType = FoodType::Meat;
	cow.foodValue = 1200.0f;
	// the tortoise: 0.25 m/s, and no meat on it
	auto& tortoise = row(AnimalInfo::Tortoise);
	tortoise.speedGroup = Speeds(164, 164, 164, 164, 164);
	tortoise.moveState = static_cast<LivingStates>(1);
	tortoise.grownUpAge = 13;
	tortoise.turnAngle = static_cast<TurnangleStates>(64);
	tortoise.viewAngle = TurnangleStates::Degrees_180;
	tortoise.foodType = FoodType::None;
	tortoise.foodValue = 50.0f;
	// a pot of food
	auto& pot = info->pot.at(static_cast<size_t>(PotInfo::FoodPot));
	pot.potType = PotType::Pot;
	pot.resourceType = ResourceType::Food;
	pot.scaleEvery = 10;
	return info;
}

class AnimalBehaviourTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::infoConstants::reset(GameAnimalRows().release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// a flat land at height 0, 512 cells a side
		Locator::terrainSystem::emplace<test::HeightFieldIsland>([](glm::vec2) { return 0.0f; });
		game_clock::SetTurn(k_Now);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static const GAnimalInfo& Info(AnimalInfo type)
	{
		return Locator::infoConstants::value().animal.at(static_cast<size_t>(type));
	}

	/// A grown-up animal standing at `at` on the land, in the map's cells, in `flock` when it is given one
	static entt::entity MakeAnimal(AnimalInfo type, glm::vec2 at, entt::entity flock = entt::null)
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		ecs::object_index::Assign(entity);
		registry.Assign<Animal>(entity, type, 13u, flock, true);
		registry.Assign<AnimalBrain>(entity).birthTurn = static_cast<int32_t>(k_Now) - 13 * k_TurnsPerYear;
		registry.Assign<Transform>(entity, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		if (flock != entt::null)
		{
			registry.Get<Flock>(flock).members.push_back(entity);
		}
		ecs::map_cells::InsertMapObject(entity);
		return entity;
	}

	/// A flock whose home is at `at`, with the default domain of 80 m
	static entt::entity MakeFlock(glm::vec2 at)
	{
		auto& registry = Reg();
		const auto flock = registry.Create();
		registry.Assign<Flock>(flock).domainCentre = glm::vec3(at.x, 0.0f, at.y);
		return flock;
	}

	/// A hunter of `type` in a flock of its own at `at`
	static entt::entity MakeHunter(AnimalInfo type, glm::vec2 at) { return MakeAnimal(type, at, MakeFlock(at)); }

	/// A pot of food with `amount` in it at `at`, in the map's cells
	static entt::entity MakeFoodPot(glm::vec2 at, uint16_t amount)
	{
		auto& registry = Reg();
		const auto pot = registry.Create();
		ecs::object_index::Assign(pot);
		registry.Assign<Transform>(pot, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Pot>(pot, Pot {.amount = amount, .maxAmount = 200, .type = PotInfo::FoodPot});
		ecs::map_cells::InsertMapObject(pot);
		return pot;
	}

	/// A villager standing at `at`, out in the open, or inside its home
	static entt::entity MakeVillager(glm::vec2 at, bool atHome)
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		registry.Assign<Villager>(entity).life = 1.0f;
		registry.Assign<LivingAction>(entity, VillagerStates::MoveToPos, static_cast<uint16_t>(0));
		registry.Assign<Transform>(entity, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		if (atHome)
		{
			registry.Assign<AtHome>(entity);
		}
		return entity;
	}

	/// A hungry lion chasing `target` from this turn, walking straight at it
	static void Chase(entt::entity lion, entt::entity target)
	{
		auto& brain = Brain(lion);
		brain.hunger = 12000;
		brain.target = target;
		brain.chaseStart = k_Now;
		brain.speed = static_cast<uint16_t>(SpeedState::MetresSec_8);
		brain.topState = static_cast<uint8_t>(AnimalState::HuntingMoveToPos);
		brain.finalState = static_cast<uint8_t>(AnimalState::HuntingMoveToPos);
		brain.moveState = ai::k_MoveStepThrough;
	}

	/// The animal's context, taken afresh
	static ai::Context Ctx(entt::entity entity)
	{
		auto& registry = Reg();
		auto& animal = registry.Get<Animal>(entity);
		return {entity, animal, registry.Get<AnimalBrain>(entity), registry.Get<Transform>(entity), Info(animal.type)};
	}

	static AnimalBrain& Brain(entt::entity entity) { return Reg().Get<AnimalBrain>(entity); }

	const test::RestoreService<Locator::infoConstants> _restoreInfo;
	const test::RestoreService<Locator::terrainSystem> _restoreLand;
};
} // namespace

// ---- stepping -------------------------------------------------------------------------------------------------------

TEST_F(AnimalBehaviourTest, AnAnimalStepsOffTheLandIntoNoMapCellAndBackIntoItsCell)
{
	// An animal's step has no test of the land's edge. It counts as a new cell (7, true) when either whole part of its
	// map coordinates changes, else as the same cell (6, false); a new cell takes it out of the old one, sets the
	// position and puts it in the new one, which is none off the map (docs/wiki/beings/animals.md#turn)
	const auto cow = MakeAnimal(AnimalInfo::Cow, {0.5f, 500.5f});
	const glm::ivec2 cell {0, 50};
	ASSERT_EQ(ecs::map_cells::FirstMobile(cell), cow);
	// one metre west, in map coordinates (6553.6 a metre)
	const glm::ivec2 west {-6554, 0};
	auto ctx = Ctx(cow);

	EXPECT_TRUE(ai::MoveBy(ctx, west));
	EXPECT_LT(Reg().Get<Transform>(cow).position.x, 0.0f);
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(cow));
	EXPECT_EQ(ecs::map_cells::FirstMobile(cell), entt::entity {entt::null});

	// further off, and back to half a metre off: the same cell off the map
	EXPECT_FALSE(ai::MoveBy(ctx, west));
	EXPECT_FALSE(ai::MoveBy(ctx, -west));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(cow));

	// back on to the land: in its cell again
	EXPECT_TRUE(ai::MoveBy(ctx, -west));
	EXPECT_GT(Reg().Get<Transform>(cow).position.x, 0.0f);
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(cow));
	EXPECT_EQ(ecs::map_cells::FirstMobile(cell), cow);

	// the final step to a goal off the land is taken too
	Brain(cow).goal = {-20.0f, 500.5f};
	Brain(cow).moveState = ai::k_MoveFinalStep;
	EXPECT_EQ(ai::MoveTo(ctx), 0xA);
	EXPECT_FLOAT_EQ(Reg().Get<Transform>(cow).position.x, -20.0f);
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(cow));
}

// ---- turning and looking --------------------------------------------------------------------------------------------

TEST_F(AnimalBehaviourTest, AnAnimalOnlyGoesWhereItsTurningCirclesLeaveRoom)
{
	// The two circles it turns on lie a quarter turn either side of its heading, of radius 2 x speed / turn angle (in
	// radians): a cow at 0.75 m/s turning 34 (6 degrees) a turn has circles of 1.44 m, a sheep turning 64 of 0.76 m. A
	// point inside either cannot be reached; ahead, behind and well to the side can
	// (docs/wiki/beings/animal-life.md, IsPosValidForTurnAngle; docs/wiki/engine/coordinates-and-angles.md)
	const auto cow = MakeAnimal(AnimalInfo::Cow, {500.0f, 500.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {500.0f, 500.0f});
	// heading 0 faces +x, so the circles lie along z
	Brain(cow).speed = 492;
	Brain(sheep).speed = 492;
	const auto reaches = [](entt::entity animal, glm::vec2 p) { return ai::IsPosValidForTurnAngle(Ctx(animal), p); };

	EXPECT_FALSE(reaches(cow, {500.0f, 501.0f}));
	EXPECT_FALSE(reaches(cow, {500.0f, 499.0f}));
	EXPECT_FALSE(reaches(cow, {500.0f, 501.8f}));
	EXPECT_TRUE(reaches(cow, {510.0f, 500.0f}));
	EXPECT_TRUE(reaches(cow, {499.0f, 500.0f}));
	EXPECT_TRUE(reaches(cow, {500.0f, 503.5f}));
	EXPECT_TRUE(reaches(cow, {500.0f, 496.5f}));
	// the sheep's tighter turn reaches the point the cow cannot
	EXPECT_TRUE(reaches(sheep, {500.0f, 501.8f}));
	EXPECT_FALSE(reaches(sheep, {500.0f, 501.0f}));

	// The turn angle is not tested: with none the radius comes out negative and every point passes
	GAnimalInfo noTurn = Info(AnimalInfo::Cow);
	noTurn.turnAngle = static_cast<TurnangleStates>(0);
	auto& registry = Reg();
	const ai::Context ctx {cow, registry.Get<Animal>(cow), Brain(cow), registry.Get<Transform>(cow), noTurn};
	EXPECT_TRUE(ai::IsPosValidForTurnAngle(ctx, {500.0f, 501.0f}));
}

TEST_F(AnimalBehaviourTest, AGrazersFlockPullsItsNewStepTowardsTheOthersAndItsNearestMember)
{
	// Pinned to the original run by the oracle on fake flocks (the wander's flock steering, docs/wiki/beings/animals.md
	// WANDER): within the speed's budget a fifth of the speed towards the others' centre, then that again with, per axis,
	// 8/25 of the speed towards the nearest member when it is farther than the flock distance (raw map coordinates) or
	// two fifths away when it is nearer, then three fifths of the nearest member's own step. True when the budget is used
	// up; nothing without another member
	struct Mate
	{
		glm::vec2 at;
		glm::ivec2 step;
	};
	struct Case
	{
		const char* name;
		glm::vec2 me;
		uint16_t speed;
		uint16_t distance;
		std::vector<Mate> others; // the flock's list from its newest member, without the steered one
		glm::ivec2 out0;
		size_t place; // the steered one's place in that list
		bool full;
		glm::ivec2 out;
	};
	const std::vector<Mate> herd {{{510.0f, 504.0f}, {300, -200}}, {{495.0f, 520.0f}, {-100, 400}}, {{530.0f, 470.0f}, {0, 0}}};
	const std::vector<Case> cases {
	    {"a herd", {500.0f, 500.0f}, 492, 30, herd, {0, 0}, 1, false, {209, 26}},
	    {"already pulled", {500.0f, 500.0f}, 492, 30, herd, {300, -100}, 3, true, {405, -52}},
	    {"no budget left", {500.0f, 500.0f}, 492, 30, {{{510.0f, 504.0f}, {300, -200}}}, {492, 0}, 0, true, {492, 0}},
	    {"near on x",
	     {500.0f, 500.0f},
	     492,
	     30,
	     {{{500.0f, 508.0f}, {250, 250}}, {{480.0f, 480.0f}, {0, 0}}},
	     {0, 0},
	     2,
	     false,
	     {-125, 97}},
	    {"a wide flock distance",
	     {500.0f, 500.0f},
	     492,
	     0xFFFF,
	     {{{503.0f, 498.0f}, {-400, 100}}, {{520.0f, 520.0f}, {0, 0}}},
	     {0, 0},
	     0,
	     false,
	     {-167, 167}},
	    {"alone", {500.0f, 500.0f}, 492, 30, {}, {10, 20}, 0, false, {10, 20}},
	    {"a horse",
	     {1200.5f, 800.25f},
	     983,
	     30,
	     {{{1190.0f, 812.0f}, {900, 0}}, {{1215.5f, 790.0f}, {0, -900}}, {{1201.0f, 801.0f}, {50, 50}}},
	     {0, 0},
	     1,
	     false,
	     {275, 209}},
	    {"a tortoise", {200.0f, 300.0f}, 164, 30, {{{199.0f, 305.0f}, {100, 50}}}, {-60, 40}, 1, false, {-68, 95}},
	    {"on the same spot", {500.0f, 500.0f}, 492, 0xFFFF, {{{500.0f, 500.0f}, {120, -80}}}, {0, 0}, 0, false, {-12, -84}},
	};
	for (const auto& c : cases)
	{
		const auto flock = MakeFlock(c.me);
		Reg().Get<Flock>(flock).flockDistance = c.distance;
		const auto me = MakeAnimal(AnimalInfo::Sheep, c.me, flock);
		Brain(me).speed = c.speed;
		std::vector<entt::entity> fromNewest;
		for (const auto& mate : c.others)
		{
			const auto other = MakeAnimal(AnimalInfo::Sheep, mate.at, flock);
			Brain(other).step = mate.step;
			fromNewest.push_back(other);
		}
		fromNewest.insert(fromNewest.begin() + static_cast<std::ptrdiff_t>(c.place), me);
		// the members are kept from the leader, the reverse of the list from the newest
		Reg().Get<Flock>(flock).members.assign(fromNewest.rbegin(), fromNewest.rend());
		glm::ivec2 out = c.out0;
		EXPECT_EQ(ai::FlockSteer(Ctx(me), out), c.full) << c.name;
		EXPECT_EQ(out, c.out) << c.name;
	}
}

TEST_F(AnimalBehaviourTest, AnAnimalTurnsAtMostItsTurnAngleAndHarderInsideItsTurningCircle)
{
	// A turn is at most the kind's turn angle (the cow's 34 of 2048); with the goal inside its turning circle (R = 2 x
	// speed / turn angle in radians: 1.44 m for a cow at 0.75 m/s) it turns max(turn angle, |diff|) - turn angle x d / R
	// instead, truncated and at most |diff|, so it does not keep circling its goal. A turn within the turn angle is
	// taken whole, on the shorter side, and half a circle turns the positive way (docs/wiki/beings/animals.md,
	// SetTowardsAngle)
	const auto cow = MakeAnimal(AnimalInfo::Cow, {500.0f, 500.0f});
	Brain(cow).speed = 492;
	const auto turnTo = [cow](uint16_t from, uint16_t target, float distance) {
		Brain(cow).angle = from;
		auto ctx = Ctx(cow);
		ai::SetTowardsAngle(ctx, target, distance);
		return Brain(cow).angle;
	};

	// far from the goal: 34 a turn, either way
	EXPECT_EQ(turnTo(0, 200, 10.0f), 34);
	EXPECT_EQ(turnTo(0, 2048 - 200, 10.0f), 2048 - 34);
	EXPECT_EQ(turnTo(1000, 1200, 10.0f), 1034);
	// a turn within 34 is taken whole
	EXPECT_EQ(turnTo(0, 20, 10.0f), 20);
	EXPECT_EQ(turnTo(100, 66, 10.0f), 66);
	// half a circle turns the positive way
	EXPECT_EQ(turnTo(0, 1024, 10.0f), 34);
	// inside the 1.44 m circle: 200 - 34 x 0.5 / 1.4394 = 188.19 and 200 - 34 x 1 / 1.4394 = 176.38, truncated
	EXPECT_EQ(turnTo(0, 200, 0.5f), 188);
	EXPECT_EQ(turnTo(0, 200, 1.0f), 176);
	EXPECT_EQ(turnTo(0, 2048 - 200, 1.0f), 2048 - 176);
	// at the goal, the whole difference
	EXPECT_EQ(turnTo(0, 200, 0.0f), 200);
	// just outside the circle, the turn angle again
	EXPECT_EQ(turnTo(0, 200, 1.5f), 34);
}

TEST_F(AnimalBehaviourTest, AGrazerLooksForGrassWithinHalfItsViewAngle)
{
	// A hungry grazer walks the spiral of cells round its own, (-1, 0) first, and takes the first one within half its
	// view angle (180 degrees: 90 either side, the edge included) of its heading: facing +x, the cells behind are passed
	// over and the one square to its side is taken; facing -x, the first cell is ahead
	// (docs/wiki/beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig)
	const glm::vec2 home {105.0f, 105.0f};
	const auto cow = MakeAnimal(AnimalInfo::Cow, home, MakeFlock(home));
	// a calf, so that breeding does not come first
	Brain(cow).birthTurn = static_cast<int32_t>(k_Now);
	Brain(cow).hunger = 50;

	auto ctx = Ctx(cow);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Started);
	EXPECT_EQ(Brain(cow).finalState, static_cast<uint8_t>(AnimalState::StartToEat));
	EXPECT_NEAR(Brain(cow).goal.x, 105.0f, 1e-3f);
	EXPECT_NEAR(Brain(cow).goal.y, 95.0f, 1e-3f);

	auto& brain = Brain(cow);
	brain.topState = static_cast<uint8_t>(AnimalState::DecideWhatToDo);
	brain.finalState = static_cast<uint8_t>(AnimalState::DecideWhatToDo);
	brain.angle = 1024;
	auto turned = Ctx(cow);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(turned), ai::k_Started);
	EXPECT_NEAR(Brain(cow).goal.x, 95.0f, 1e-3f);
	EXPECT_NEAR(Brain(cow).goal.y, 105.0f, 1e-3f);
}

TEST_F(AnimalBehaviourTest, AGrazerPassesOverACellUnderAManyCelledFixedThing)
{
	// After the land's collide test the search refuses a cell whose fixed list starts with a building or another
	// many-celled fixed thing, here a rock: facing +x, the cell square to its side is passed over and the next one of
	// the spiral, ahead and to that side, is taken (docs/wiki/beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig)
	const test::RestoreService<Locator::mapShapeProvider> restoreShapes;
	auto shapes = std::make_unique<test::FakeMapShapeProvider>();
	// no mesh: the rock is in the cell of its position only
	shapes->meshShape = [](entt::entity, ecs::map_collide::Shape&, float&) { return false; };
	Locator::mapShapeProvider::reset(shapes.release());
	const glm::vec2 home {105.0f, 105.0f};
	const auto cow = MakeAnimal(AnimalInfo::Cow, home, MakeFlock(home));
	// a calf, so that breeding does not come first
	Brain(cow).birthTurn = static_cast<int32_t>(k_Now);
	Brain(cow).hunger = 50;
	auto& registry = Reg();
	const auto rock = registry.Create();
	ecs::object_index::Assign(rock);
	registry.Assign<Transform>(rock, glm::vec3(105.0f, 0.0f, 95.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MobileStatic>(rock);
	ecs::map_cells::InsertMapObject(rock);
	ASSERT_TRUE(ecs::map_cells::IsFixed({10, 9}));

	auto ctx = Ctx(cow);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Started);
	EXPECT_EQ(Brain(cow).finalState, static_cast<uint8_t>(AnimalState::StartToEat));
	EXPECT_NEAR(Brain(cow).goal.x, 115.0f, 1e-3f);
	EXPECT_NEAR(Brain(cow).goal.y, 95.0f, 1e-3f);
}

// ---- eating -----------------------------------------------------------------------------------------------------------

TEST_F(AnimalBehaviourTest, AGrazerAtAFoodPileEatsFiftyOfItAndIsNoLongerHungry)
{
	// At the pile it takes 50 of its food, or what is left, and its hunger goes back to 0; the pile it empties goes
	// (docs/wiki/beings/animal-life.md#reactions)
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {100.0f, 100.0f});
	const auto pot = MakeFoodPot({102.0f, 100.0f}, 120);
	const auto arrive = [sheep, pot] {
		auto& brain = Brain(sheep);
		brain.hunger = 50;
		brain.topState = static_cast<uint8_t>(AnimalState::ArrivesAtFoodReaction);
		brain.predator = pot;
		auto ctx = Ctx(sheep);
		ai::ArrivesAtFoodReaction(ctx);
	};

	arrive();
	EXPECT_EQ(Brain(sheep).hunger, 0);
	EXPECT_EQ(Reg().Get<Pot>(pot).amount, 70);
	arrive();
	EXPECT_EQ(Brain(sheep).hunger, 0);
	EXPECT_EQ(Reg().Get<Pot>(pot).amount, 20);
	arrive();
	EXPECT_EQ(Brain(sheep).hunger, 0);
	EXPECT_FALSE(Reg().Valid(pot));
}

TEST_F(AnimalBehaviourTest, AFoodPileDrawsOnlyTheHungryOnesWhoseFoodKindsTakeIt)
{
	// A food pot (info.dat's foodType VEGETABLE) starts its food reaction as it is put down: it draws a hungry animal
	// that reacts to food and whose needsFoodTypes take vegetables (the sheep's VEGETABLE | GRAZE), and passes over one
	// not yet hungry, one that does not react to food and a lion whose food is meat (docs/wiki/beings/animal-life.md,
	// Food and the food priority). The reaction row's numbers are the test's: within its distances
	namespace reactions = openblack::ecs::effects::reactions;
	auto info = GameAnimalRows();
	info->pot.at(static_cast<size_t>(PotInfo::FoodPot)).foodType = FoodType::Vegetable;
	info->pot.at(static_cast<size_t>(PotInfo::FoodPot)).associatedReaction = Reaction::ReactToFood;
	auto& food = info->reaction.at(static_cast<size_t>(Reaction::ReactToFood));
	food.priority = 50;
	food.maxReactionDistance = 35.0f;
	food.maxDistanceToRunAwayFromObject = 35.0f;
	info->animal.at(static_cast<size_t>(AnimalInfo::Sheep)).isReacting.isReactingToFood = 1;
	info->animal.at(static_cast<size_t>(AnimalInfo::Lion)).isReacting.isReactingToFood = 1;
	Locator::infoConstants::reset(info.release());
	reactions::Clear();
	// the spread walks the map's cells only with a map to sync them
	const test::RestoreService<Locator::entitiesMap> keepMap;
	Locator::entitiesMap::emplace<CellsFiledByTheTest>();

	const auto hungry = MakeAnimal(AnimalInfo::Sheep, {105.0f, 100.0f});
	const auto notYet = MakeAnimal(AnimalInfo::Sheep, {100.0f, 105.0f});
	const auto noReaction = MakeAnimal(AnimalInfo::Cow, {95.0f, 100.0f});
	const auto lion = MakeHunter(AnimalInfo::Lion, {100.0f, 95.0f});
	Brain(hungry).hunger = 50;
	Brain(notYet).hunger = 49;
	Brain(noReaction).hunger = 50;
	Brain(lion).hunger = 12000;
	const auto pot = MakeFoodPot({100.0f, 100.0f}, 120);

	ecs::animal_ai::SetupPotReaction(pot);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(pot, Reaction::ReactToFood), 0u);
	EXPECT_EQ(Brain(hungry).topState, static_cast<uint8_t>(AnimalState::GotoFoodReaction));
	EXPECT_EQ(Brain(hungry).predator, pot);
	for (const auto passedOver : {notYet, noReaction, lion})
	{
		EXPECT_NE(Brain(passedOver).topState, static_cast<uint8_t>(AnimalState::GotoFoodReaction));
		EXPECT_EQ(Brain(passedOver).reaction, 0u);
	}
	reactions::Clear();
}

TEST_F(AnimalBehaviourTest, AFireIsRunFromByTheAnimalsThatReactToItTheBurningOneFromItsOwn)
{
	// A burning sheep starts the fire's reaction: every animal in reach whose kind reacts to a fire takes it up at the
	// row's priority with no other test and runs from the burning thing as from any frightening thing, the burning one
	// from itself; one whose kind does not react to it passes it over (docs/wiki/beings/animal-life.md#reactions). The
	// reaction row's numbers are the test's
	namespace reactions = openblack::ecs::effects::reactions;
	auto info = GameAnimalRows();
	auto& fire = info->reaction.at(static_cast<size_t>(Reaction::ReactToFire));
	fire.priority = 210;
	fire.maxReactionDistance = 35.0f;
	fire.maxDistanceToRunAwayFromObject = 35.0f;
	fire.numGameTurnsForNormalThingsToReact = 100;
	info->animal.at(static_cast<size_t>(AnimalInfo::Sheep)).isReacting.isReactingToAbodeBurning = 1;
	Locator::infoConstants::reset(info.release());
	reactions::Clear();
	// the spread walks the map's cells only with a map to sync them
	const test::RestoreService<Locator::entitiesMap> keepMap;
	Locator::entitiesMap::emplace<CellsFiledByTheTest>();

	const auto burning = MakeAnimal(AnimalInfo::Sheep, {100.0f, 100.0f});
	const auto near = MakeAnimal(AnimalInfo::Sheep, {110.0f, 100.0f});
	const auto noReaction = MakeAnimal(AnimalInfo::Cow, {95.0f, 100.0f});
	const auto id = reactions::CreateReaction(burning, Reaction::ReactToFire, std::nullopt, true);
	ASSERT_NE(id, 0u);
	for (const auto runs : {burning, near})
	{
		EXPECT_EQ(Brain(runs).reaction, id);
		EXPECT_EQ(Brain(runs).topState, static_cast<uint8_t>(AnimalState::FleeingFromObjectReaction));
		EXPECT_EQ(Brain(runs).predator, burning);
	}
	EXPECT_EQ(Brain(noReaction).reaction, 0u);
	reactions::Clear();
}

// ---- hunting ------------------------------------------------------------------------------------------------------------

TEST_F(AnimalBehaviourTest, AHunterHuntsOnceItsHungerReachesItsKindsMostAndChasesThePreyItFound)
{
	// The lion grows hungry by one a turn and hunts at its kind's 12000: the turn it gets there it finds its prey, and
	// from the next it chases that one at its fourth speed (8 m/s), even with another one nearer by then
	// (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto prey = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f});
	Brain(lion).hunger = 11999;

	auto notHungry = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(notHungry), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});

	auto needs = Ctx(lion);
	ai::ProcessNeeds(needs);
	EXPECT_EQ(Brain(lion).hunger, 12000);
	auto hungry = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(hungry), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, prey);
	EXPECT_EQ(Brain(lion).preyCell, glm::vec2(115.0f, 105.0f));

	const auto nearer = MakeAnimal(AnimalInfo::Sheep, {106.0f, 105.0f});
	auto chase = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(chase), ai::k_Started);
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::HuntingMoveToPos));
	EXPECT_EQ(Brain(lion).target, prey);
	EXPECT_NE(Brain(lion).target, nearer);
	EXPECT_EQ(Brain(lion).goal, glm::vec2(115.0f, 105.0f));
	EXPECT_EQ(Brain(lion).speed, static_cast<uint16_t>(SpeedState::MetresSec_8));
}

TEST_F(AnimalBehaviourTest, AHunterWhoseFlockAScriptHasCalmedTakesNoPrey)
{
	// A script's calm on the hunter's own flock (CHANGE_INNER_OUTER_PROPERTIES): while it is not 0 the prey search passes
	// over every animal and villager; at 0 the same lion finds the sheep beside it
	// (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {95.0f, 105.0f});
	auto& flock = Reg().Get<Flock>(Reg().Get<Animal>(lion).flock);
	flock.calm = 1;
	Brain(lion).hunger = 12000;

	auto calmed = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(calmed), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(lion).preyCell, glm::vec2(0.0f));

	flock.calm = 0;
	auto hunting = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(hunting), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, sheep);
}

TEST_F(AnimalBehaviourTest, ACalmedWolfPackNeitherHuntsNorLeavesItsLairAndKeepsIt)
{
	// A wolf pack its script has calmed: hungry, it does not hunt; lying in its lair after 23:00, it does not go out; and
	// its lair is not looked for again. Uncalmed, the same wolf hunts the sheep, goes out of its lair, and takes its lair
	// where it is, with no forest or tree about (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	Locator::animalSystem::value().SetVisualTime(23.5f);
	const auto wolf = MakeAnimal(AnimalInfo::Wolf, {105.0f, 105.0f}, MakeFlock({300.0f, 300.0f}));
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {95.0f, 105.0f});
	auto& flock = Reg().Get<Flock>(Reg().Get<Animal>(wolf).flock);
	flock.calm = 1;
	Brain(wolf).hunger = 12000;
	const auto hide = [wolf] {
		auto& brain = Brain(wolf);
		brain.topState = static_cast<uint8_t>(AnimalState::HideInLair);
		brain.finalState = static_cast<uint8_t>(AnimalState::HideInLair);
		auto ctx = Ctx(wolf);
		ai::HideInLair(ctx);
		return brain.topState;
	};
	const auto lair = [wolf, &flock] {
		auto ctx = Ctx(wolf);
		ai::CalculateLairPosition(ctx);
		return glm::vec2(flock.domainCentre.x, flock.domainCentre.z);
	};

	auto calmed = Ctx(wolf);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(calmed), ai::k_Nothing);
	EXPECT_EQ(Brain(wolf).target, entt::entity {entt::null});
	EXPECT_EQ(hide(), static_cast<uint8_t>(AnimalState::HideInLair));
	EXPECT_EQ(lair(), glm::vec2(300.0f, 300.0f));

	flock.calm = 0;
	auto hunting = Ctx(wolf);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(hunting), ai::k_Nothing);
	EXPECT_EQ(Brain(wolf).target, sheep);
	Brain(wolf).target = entt::null;
	Brain(wolf).preyCell = glm::vec2(0.0f);
	EXPECT_NE(hide(), static_cast<uint8_t>(AnimalState::HideInLair));
	EXPECT_EQ(Brain(wolf).finalState, static_cast<uint8_t>(AnimalState::StartWander));
	EXPECT_EQ(lair(), glm::vec2(105.0f, 105.0f));
}

TEST_F(AnimalBehaviourTest, AHunterLooksForPreyRoundTheSpiralOfCellsFromItsOwn)
{
	// The spiral goes (-1, 0) first from its own cell: of two sheep a cell away on either side, the one at -x is found
	// (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f});
	const auto first = MakeAnimal(AnimalInfo::Sheep, {95.0f, 105.0f});
	Brain(lion).hunger = 12000;

	auto ctx = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, first);
	EXPECT_EQ(Brain(lion).preyCell, glm::vec2(95.0f, 105.0f));
}

TEST_F(AnimalBehaviourTest, AHunterTakesOnlyAnAnimalWithMeatOnIt)
{
	// Its own cell holds a pot of food and the next one a tortoise (no meat in info.dat): it passes over both for the
	// sheep further round (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	MakeFoodPot({107.0f, 107.0f}, 100);
	MakeAnimal(AnimalInfo::Tortoise, {95.0f, 105.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f});
	Brain(lion).hunger = 12000;

	auto ctx = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, sheep);
}

TEST_F(AnimalBehaviourTest, AHunterNeverHuntsWhatCannotBeEaten)
{
	// What cannot be eaten is never prey: the first lion passes over the sheep that cannot be eaten for the next one;
	// the second, with only such a sheep about, finds none (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf,
	// docs/wiki/beings/animal-life.md#scripts-and-flags)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f});
	const auto uneaten = MakeAnimal(AnimalInfo::Sheep, {95.0f, 105.0f});
	const auto alone = MakeHunter(AnimalInfo::Lion, {405.0f, 405.0f});
	const auto onlyUneaten = MakeAnimal(AnimalInfo::Sheep, {395.0f, 405.0f});
	ecs::script_held::SetCannotBeEaten(uneaten);
	ecs::script_held::SetCannotBeEaten(onlyUneaten);
	Brain(lion).hunger = 12000;
	Brain(alone).hunger = 12000;

	auto ctx = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, sheep);
	auto other = Ctx(alone);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(other), ai::k_Nothing);
	EXPECT_EQ(Brain(alone).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(alone).preyCell, glm::vec2(0.0f));
}

TEST_F(AnimalBehaviourTest, AHunterGivesUpAVillagerWhoWentIntoAHouse)
{
	// The chase and the check of the target seen last turn both ask whether the villager can still be reached: one gone
	// into its home cannot, so the lion gives it up at once, where one still out in the open is chased on (60 m away:
	// at the stalking speed). docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto inside = MakeVillager({165.0f, 105.0f}, true);
	Chase(lion, inside);
	auto chase = Ctx(lion);
	ai::HuntingMoveToPos(chase);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::StartWander));
	EXPECT_EQ(Brain(lion).speed, static_cast<uint16_t>(Info(AnimalInfo::Lion).speedGroup.speedDefault));

	// the one seen last turn is not chased again from its home either; with no other prey about there is no hunt
	Chase(lion, inside);
	Brain(lion).topState = static_cast<uint8_t>(AnimalState::DecideWhatToDo);
	Brain(lion).finalState = static_cast<uint8_t>(AnimalState::DecideWhatToDo);
	auto needs = Ctx(lion);
	EXPECT_EQ(ai::PredatorReactToAnimalNeeds(needs), ai::k_Nothing);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::DecideWhatToDo));

	const auto outside = MakeVillager({165.0f, 105.0f}, false);
	Chase(lion, outside);
	auto onward = Ctx(lion);
	ai::HuntingMoveToPos(onward);
	EXPECT_EQ(Brain(lion).target, outside);
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::HuntingMoveToPos));
	EXPECT_EQ(Brain(lion).speed, static_cast<uint16_t>(Info(AnimalInfo::Lion).speedGroup.speed2));
}

TEST_F(AnimalBehaviourTest, AHunterTakesItsStepAtThePreyBeforeItsChaseTimeIsTested)
{
	// The chase steps at the prey first and only then asks whether its chase time (200 turns) has run out: the lion
	// whose time is up still takes this turn's step, 0.8 m at 8 m/s, then gives up
	// (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {165.0f, 105.0f});
	Chase(lion, sheep);
	Brain(lion).chaseStart = k_Now - Info(AnimalInfo::Lion).chaseTime;

	auto ctx = Ctx(lion);
	ai::HuntingMoveToPos(ctx);
	EXPECT_NEAR(Reg().Get<Transform>(lion).position.x, 105.8f, 0.01f);
	EXPECT_NEAR(Reg().Get<Transform>(lion).position.z, 105.0f, 0.01f);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(lion).preyCell, glm::vec2(0.0f));
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::StartWander));
}

TEST_F(AnimalBehaviourTest, AHunterThatGivesUpAPreyItIsNotFacingStillSprintsThatTurn)
{
	// Within its pounce reach (none here: no clips are loaded, so only the prey's very spot) but facing away from it
	// (beyond +-22.5 degrees), the lion gives the prey up and goes to wander; the chase then still sets this turn's speed
	// by the distance, the sprint (10 m/s) inside the attack distance, where the default speed (1.5 m/s) was left before
	// (docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf)
	const test::RestoreService<Locator::resources> keepResources;
	Locator::resources::emplace<resources::Resources>();
	const auto lion = MakeHunter(AnimalInfo::Lion, {105.0f, 105.0f});
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {105.0f, 105.0f});
	Chase(lion, sheep);
	Brain(lion).moveState = ai::k_MoveArrived;
	// facing -x; the prey's spot gives the angle 0 (+x)
	Brain(lion).angle = 1024;

	auto ctx = Ctx(lion);
	ai::HuntingMoveToPos(ctx);
	EXPECT_EQ(Brain(lion).target, entt::entity {entt::null});
	EXPECT_EQ(Brain(lion).topState, static_cast<uint8_t>(AnimalState::StartWander));
	EXPECT_EQ(Brain(lion).speed, static_cast<uint16_t>(Info(AnimalInfo::Lion).speedGroup.speed3));
}

// ---- death ------------------------------------------------------------------------------------------------------------

TEST_F(AnimalBehaviourTest, ABodyKilledAgainLiesItsFullTimeAfresh)
{
	// The first death sets DYING and the body's 600 turns; killed again it only starts its 600 turns again, still dead
	// (docs/wiki/beings/animal-life.md, Living::SetDying)
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {100.0f, 100.0f});

	ecs::animal_ai::DestroyedByEffect(sheep);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dying));
	EXPECT_EQ(Brain(sheep).counter, 600);
	EXPECT_EQ(Brain(sheep).status & 1, 1);

	Brain(sheep).topState = static_cast<uint8_t>(AnimalState::Dead);
	Brain(sheep).counter = 37;
	ecs::animal_ai::DestroyedByEffect(sheep);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dead));
	EXPECT_EQ(Brain(sheep).counter, 600);
}

// The common dying does nothing while the animal is in the physics, a storm's carry too; once out of it, it dies
TEST_F(AnimalBehaviourTest, AnAnimalInThePhysicsDoesNotStartDying)
{
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {100.0f, 100.0f});
	Reg().AssignOrReplaceState<InPhysics>(sheep);
	ecs::animal_ai::DestroyedByEffect(sheep);
	EXPECT_NE(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dying));
	EXPECT_EQ(Brain(sheep).status & 1, 0);
	Reg().RemoveState<InPhysics>(sheep);
	ecs::animal_ai::DestroyedByEffect(sheep);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dying));
}

// An animal whose life ran out in the air dies as it lands through its own SetDying: the common one sets DYING with
// the body's 600 turns, a corpse thrown again lies dead again, and a species with its own
// SetDying (a spell's dove or wolf fades) takes that in place of the common one
TEST_F(AnimalBehaviourTest, AnAnimalThatLandsDeadDiesThroughItsOwnSetDying)
{
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {100.0f, 100.0f});
	Reg().AssignOrReplace<Life>(sheep).value = -5.0f;
	ecs::animal_ai::EndPhysics(sheep, 1);
	EXPECT_EQ(Reg().Get<Life>(sheep).value, 0.0f);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dying));
	EXPECT_EQ(Brain(sheep).counter, 600);
	EXPECT_EQ(Brain(sheep).status & 0x31, 0x11);

	// thrown again, the corpse lies dead again, its turns started afresh
	Brain(sheep).counter = 37;
	ecs::animal_ai::EndPhysics(sheep, 2);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dead));
	EXPECT_EQ(Brain(sheep).counter, 600);
	EXPECT_EQ(Brain(sheep).status & 0x30, 0x20);

	// a species' own SetDying runs instead of the common one
	const auto cow = MakeAnimal(AnimalInfo::Cow, {120.0f, 100.0f});
	Reg().AssignOrReplace<Life>(cow).value = 0.0f;
	int fades = 0;
	ecs::animal_ai::SetSpeciesDying(AnimalInfo::Cow, [&fades](entt::entity) { ++fades; });
	ecs::animal_ai::EndPhysics(cow, 1);
	ecs::animal_ai::SetSpeciesDying(AnimalInfo::Cow, {});
	EXPECT_EQ(fades, 1);
	EXPECT_NE(Brain(cow).topState, static_cast<uint8_t>(AnimalState::Dying));
	EXPECT_EQ(Brain(cow).status & 1, 0);
}

// An animal that sinks while it can be reached tells the creature of the player whose hand last let go of it that it
// was thrown in the sea, takes its species' own dying once, and is deleted; one that cannot be reached has not sunk
// (docs/wiki/beings/animal-life.md#hand-flight-and-death)
TEST_F(AnimalBehaviourTest, ASunkAnimalIsReportedDiesThroughItsOwnSetDyingAndIsDeleted)
{
	// the sinking writes to the game's log
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	std::vector<ecs::events::PlayerDeedForMimic> deeds;
	Locator::events::value().AddHandler<ecs::events::PlayerDeedForMimic>(
	    [&deeds](const ecs::events::PlayerDeedForMimic& event) { deeds.push_back(event); });
	// marked unavailable rather than destroyed, so what happened to it can still be seen
	ecs::SetDeferredDeletion(true);
	Reg().Assign<Player>(Reg().Create(), PlayerNames::PLAYER_ONE);
	int fades = 0;
	ecs::animal_ai::SetSpeciesDying(AnimalInfo::Cow, [&fades](entt::entity) { ++fades; });

	const auto cow = MakeAnimal(AnimalInfo::Cow, {100.0f, 100.0f});
	Reg().AssignOrReplaceState<InPhysics>(cow);
	ecs::last_dropped::RecordDrop(Reg(), PlayerNames::PLAYER_ONE, cow);
	EXPECT_TRUE(ecs::HasSunk(cow));
	EXPECT_EQ(fades, 1);
	ASSERT_EQ(deeds.size(), 1u);
	EXPECT_EQ(deeds.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(deeds.front().deed, creature_watching::Deed::ThrowInTheSea);
	EXPECT_EQ(deeds.front().object, cow);
	EXPECT_EQ(Brain(cow).topState, static_cast<uint8_t>(AnimalState::Dead));
	EXPECT_FALSE(ecs::IsAvailable(cow));

	// already unavailable: not sunk, nothing done to it
	EXPECT_FALSE(ecs::HasSunk(cow));
	EXPECT_EQ(fades, 1);
	EXPECT_EQ(deeds.size(), 1u);

	// the common dying does nothing in the physics; no hand let it go, so nothing is reported
	const auto sheep = MakeAnimal(AnimalInfo::Sheep, {120.0f, 100.0f});
	Reg().AssignOrReplaceState<InPhysics>(sheep);
	EXPECT_TRUE(ecs::HasSunk(sheep));
	EXPECT_EQ(Brain(sheep).status & 1, 0);
	EXPECT_EQ(Brain(sheep).topState, static_cast<uint8_t>(AnimalState::Dead));
	EXPECT_FALSE(ecs::IsAvailable(sheep));
	EXPECT_EQ(deeds.size(), 1u);
	ecs::animal_ai::SetSpeciesDying(AnimalInfo::Cow, {});
}

// ---- birds ------------------------------------------------------------------------------------------------------------

namespace
{
/// The class hooks of a test that only wants the velocity a start in the world is given: it is written down and the
/// start refused, so no body is made
class RecordWorldStartHooks final: public ecs::systems::PhysicsClassHooks
{
public:
	explicit RecordWorldStartHooks(std::optional<glm::vec3>& velocity)
	    : _velocity(velocity)
	{
	}
	bool StartPhysicsInWorld([[maybe_unused]] ecs::systems::DynamicsSystemInterface& dynamics,
	                         [[maybe_unused]] entt::entity object, glm::vec3 velocity, [[maybe_unused]] glm::vec3 spin) override
	{
		_velocity = velocity;
		return false;
	}

private:
	std::optional<glm::vec3>& _velocity;
};
} // namespace

TEST_F(AnimalBehaviourTest, ABirdKilledFallsWithItsFlightSpeedAlongItsHeading)
{
	// A dying bird goes into the physics at its speed in metres a second (its speed / 6553.6 x 10) along its heading:
	// with s its heading as the physics' angle, (sin(s) v, 0, -cos(s) v). The bits are the original's own, run on the
	// exe for these headings and speeds (docs/wiki/beings/animals.md, the dove's Dying)
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	std::optional<glm::vec3> velocity;
	auto& dynamics = Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(
	    std::make_unique<test::FakeDynamicsWorld>(Reg(), &Locator::infoConstants::value()));
	dynamics.SetClassHooks(std::make_unique<RecordWorldStartHooks>(velocity));
	const auto dove = MakeAnimal(AnimalInfo::Dove, {500.0f, 500.0f});
	struct Case
	{
		uint16_t speed;
		uint16_t angle;
		uint32_t x;
		uint32_t z;
	};
	constexpr std::array<Case, 6> k_Cases {{
	    {.speed = 6554, .angle = 256, .x = 0x40E249B9, .z = 0x40E249B9},
	    {.speed = 1311, .angle = 1000, .x = 0xBFFF5C64, .z = 0x3E16B11C},
	    {.speed = 4096, .angle = 0, .x = 0x40C80000, .z = 0x3492ABCC},
	    {.speed = 2000, .angle = 1700, .x = 0x3FBC5A64, .z = 0xC02B1B80},
	    {.speed = 492, .angle = 512, .x = 0xB38CF116, .z = 0x3F403000},
	    {.speed = 6554, .angle = 1024, .x = 0xC1200280, .z = 0xB4000CDD},
	}};
	for (const auto& c : k_Cases)
	{
		velocity.reset();
		Brain(dove).speed = c.speed;
		Brain(dove).angle = c.angle;
		auto ctx = Ctx(dove);
		ai::BirdDying(ctx);
		ASSERT_TRUE(velocity.has_value()) << c.angle;
		EXPECT_EQ(std::bit_cast<uint32_t>(velocity->x), c.x) << c.speed << " " << c.angle;
		EXPECT_EQ(std::bit_cast<uint32_t>(velocity->y), 0u) << c.speed << " " << c.angle;
		EXPECT_EQ(std::bit_cast<uint32_t>(velocity->z), c.z) << c.speed << " " << c.angle;
	}
}

// The flock's pull on a wanderer's step against the original's own steering, run on the same animals (the oracle, the
// frozen exe's flock steering with the flock distance stubbed): positions on a 0.625 m grid (4096 map units), so that
// the map coordinates are exact; the members oldest first, the wanderer the newest (docs/wiki/beings/animals.md)
TEST_F(AnimalBehaviourTest, TheFlocksPullOnAWanderersStepIsTheOriginals)
{
	struct Member
	{
		glm::ivec2 at;
		glm::ivec2 step;
	};
	struct Case
	{
		glm::ivec2 me;
		uint16_t speed;
		glm::ivec2 out;
		std::vector<Member> members;
		uint16_t flockDistance;
		bool usedUp;
		glm::ivec2 expected;
	};
	constexpr int32_t k_Grid = 4096;
	const auto at = [](int32_t x, int32_t z) { return glm::ivec2(x * k_Grid, z * k_Grid); };
	const std::vector<Case> cases {
	    {at(100, 200), 491, {0, 0}, {{at(110, 200), {300, -40}}}, 1, false, {211, -108}},
	    // a flock distance past the larger axis: 1000000 as the original keeps it, 16960
	    {at(100, 200), 491, {0, 0}, {{at(110, 200), {300, -40}}}, 16960, false, {211, -108}},
	    {at(100, 200), 491, {0, 0}, {{at(100, 205), {0, 0}}, {at(96, 203), {-120, 77}}}, 1, false, {-123, 132}},
	    {at(100, 200),
	     983,
	     {0, 0},
	     {{at(100, 205), {0, 0}}, {at(96, 203), {-120, 77}}, {at(130, 180), {500, 500}}},
	     12288,
	     false,
	     {-19, 40}},
	    {at(100, 200), 98, {0, 0}, {{at(90, 190), {60, -60}}}, 1, false, {-7, -31}},
	    {at(100, 200), 491, {400, 0}, {{at(110, 200), {300, -40}}}, 1, true, {417, 0}},
	    {at(100, 200), 491, {491, 0}, {{at(110, 200), {300, -40}}}, 1, true, {491, 0}},
	    {at(100, 200), 491, {-200, 150}, {{at(100, 200), {250, 250}}}, 0, false, {-119, 195}},
	    {at(50, 60), 737, {0, 0}, {{at(52, 61), {-737, 0}}, {at(49, 59), {0, 737}}}, 30000, false, {290, 440}},
	    {at(50, 60), 737, {0, 0}, {}, 5, false, {0, 0}},
	};
	const auto metres = [](glm::ivec2 coords) { return glm::vec2(coords) / 6553.6f; };
	for (size_t i = 0; i < cases.size(); ++i)
	{
		const auto& c = cases[i];
		const auto flock = MakeFlock(metres(c.me));
		Reg().Get<Flock>(flock).flockDistance = c.flockDistance;
		for (const auto& member : c.members)
		{
			const auto other = MakeAnimal(AnimalInfo::Sheep, metres(member.at), flock);
			Brain(other).step = member.step;
		}
		const auto me = MakeAnimal(AnimalInfo::Sheep, metres(c.me), flock);
		Brain(me).speed = c.speed;
		glm::ivec2 out = c.out;
		EXPECT_EQ(ai::FlockSteer(Ctx(me), out), c.usedUp) << "case " << i;
		EXPECT_EQ(out, c.expected) << "case " << i;
	}
}

// ---- wandering ------------------------------------------------------------------------------------------------------

/// A wanderer that steps into a new map cell looks round twice its flock's domain for another flock of its kind to
/// merge with, only when its kind's flocks can merge; a step within its cell looks for none. With the game's rows no
/// kind's flocks can merge, so a wanderer never merges (docs/wiki/beings/animal-life.md#flocks-age-and-smoke)
TEST_F(AnimalBehaviourTest, AWandererInANewCellMergesWithANearbyFlockOnlyWhenItsKindsFlocksCanMerge)
{
	// the sheep's row as the game has it for these: flocks of up to 50 that cannot merge
	GAnimalInfo game = Info(AnimalInfo::Sheep);
	game.maxFlockSize = 50;
	game.flocksCanMerge = 0;
	GAnimalInfo merging = game;
	merging.flocksCanMerge = 1;
	const auto wanderBy = [](entt::entity animal, const GAnimalInfo& info, glm::ivec2 step) {
		auto& registry = Reg();
		Brain(animal).step = step;
		ai::Context ctx {animal, registry.Get<Animal>(animal), Brain(animal), registry.Get<Transform>(animal), info};
		ai::Wander(ctx);
	};
	const auto flockOf = [](entt::entity animal) { return Reg().Get<const Animal>(animal).flock; };
	const auto x = [](entt::entity animal) { return Reg().Get<const Transform>(animal).position.x; };
	// a metre is 6553.6 map units, a cell 10 m
	const glm::ivec2 metreEast {6554, 0};
	const glm::ivec2 twoMetresWest {-13107, 0};

	// the wanderer at 109.5 m, its flock's domain round it; another sheep's flock in the next cell east
	const auto wanderer = MakeAnimal(AnimalInfo::Sheep, {109.5f, 105.0f}, MakeFlock({109.5f, 105.0f}));
	const auto other = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f}, MakeFlock({115.0f, 105.0f}));
	ASSERT_NE(flockOf(wanderer), flockOf(other));

	// into the next cell with the game's row: no merge
	wanderBy(wanderer, game, metreEast);
	EXPECT_NEAR(x(wanderer), 110.5f, 0.01f);
	EXPECT_NE(flockOf(wanderer), flockOf(other));

	// within that cell with a row whose flocks can merge: no search, no merge
	wanderBy(wanderer, merging, metreEast);
	EXPECT_NEAR(x(wanderer), 111.5f, 0.01f);
	EXPECT_NE(flockOf(wanderer), flockOf(other));

	// back into the first cell with that row: the other flock is found and merged; of two the same size, the
	// wanderer's keeps them both
	wanderBy(wanderer, merging, twoMetresWest);
	EXPECT_NEAR(x(wanderer), 109.5f, 0.01f);
	EXPECT_NE(flockOf(wanderer), entt::entity {entt::null});
	EXPECT_EQ(flockOf(other), flockOf(wanderer));
	EXPECT_EQ(Reg().Get<const Flock>(flockOf(wanderer)).members.size(), 2u);
}

/// With OPENBLACK_GAME_PATH set to the install: no animal kind's flocks can merge, so the wander's and the grazers'
/// flock searches never merge in play (docs/wiki/beings/animal-life.md#flocks-age-and-smoke)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// A flock never merges with one of another player, as the original's search compares the two flocks' players (the
// neutral player for an animal's own flock and a CREATE_FLOCK's)
TEST_F(AnimalBehaviourTest, AWandererNeverMergesWithAFlockOfAnotherPlayer)
{
	GAnimalInfo merging = Info(AnimalInfo::Sheep);
	merging.maxFlockSize = 50;
	merging.flocksCanMerge = 1;
	const auto wanderBy = [&merging](entt::entity animal, glm::ivec2 step) {
		auto& registry = Reg();
		Brain(animal).step = step;
		ai::Context ctx {animal, registry.Get<Animal>(animal), Brain(animal), registry.Get<Transform>(animal), merging};
		ai::Wander(ctx);
	};
	const auto flockOf = [](entt::entity animal) { return Reg().Get<const Animal>(animal).flock; };
	const glm::ivec2 metreEast {6554, 0};
	const glm::ivec2 twoMetresWest {-13107, 0};
	// the other sheep's flock is player two's; the wanderer's has none
	const auto wanderer = MakeAnimal(AnimalInfo::Sheep, {109.5f, 105.0f}, MakeFlock({109.5f, 105.0f}));
	const auto other = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f}, MakeFlock({115.0f, 105.0f}));
	Reg().Get<Flock>(flockOf(other)).player = PlayerNames::PLAYER_TWO;
	wanderBy(wanderer, metreEast);
	wanderBy(wanderer, twoMetresWest);
	EXPECT_NE(flockOf(wanderer), flockOf(other));
	// two other flocks, both player two's, further north: they merge
	const auto same = MakeAnimal(AnimalInfo::Sheep, {109.5f, 305.0f}, MakeFlock({109.5f, 305.0f}));
	const auto near = MakeAnimal(AnimalInfo::Sheep, {115.0f, 305.0f}, MakeFlock({115.0f, 305.0f}));
	Reg().Get<Flock>(flockOf(same)).player = PlayerNames::PLAYER_TWO;
	Reg().Get<Flock>(flockOf(near)).player = PlayerNames::PLAYER_TWO;
	wanderBy(same, metreEast);
	wanderBy(same, twoMetresWest);
	EXPECT_EQ(flockOf(same), flockOf(near));
}

TEST(AnimalInfoRows, realInfoDatNoKindsFlocksCanMerge)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	for (size_t type = 0; type < info->animal.size(); ++type)
	{
		EXPECT_EQ(info->animal.at(type).flocksCanMerge, 0u) << type;
		EXPECT_EQ(info->animal.at(type).domainRadius, 80u) << type;
	}
}

// ---- the tortoise ------------------------------------------------------------------------------------------------------

/// The reactions a row takes: none for a row with none set, only its own for a row with one
TEST(AnimalInfoRows, ARowWithNoReactionsTakesNone)
{
	IsReacting none {};
	IsReacting food {};
	food.isReactingToFood = 1;
	for (size_t type = 0; type < sizeof(IsReacting) / sizeof(uint32_t); ++type)
	{
		EXPECT_FALSE(ReactsTo(none, type)) << type;
		EXPECT_EQ(ReactsTo(food, type), type == 7) << type;
	}
	// the tortoise is a grazer of the shared class, as the sheep, cow, horse and pig are
	for (const auto type : {AnimalInfo::Sheep, AnimalInfo::Tortoise, AnimalInfo::Cow, AnimalInfo::Horse, AnimalInfo::Pig})
	{
		EXPECT_TRUE(ai::IsGrazer(type)) << static_cast<int>(type);
	}
	EXPECT_FALSE(ai::IsGrazer(AnimalInfo::Lion));
	EXPECT_FALSE(ai::IsGrazer(AnimalInfo::Dove));
}

/// With OPENBLACK_GAME_PATH set to the install: the tortoise's row is the slowest of the grazers' (0.25 m/s) and takes
/// no reaction, where the sheep's takes food and predators (docs/wiki/beings/animal-life.md#reactions)
// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(AnimalInfoRows, realInfoDatTheTortoiseIsTheSlowestGrazerAndReactsToNothing)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	std::ifstream file(std::filesystem::path(game) / "Scripts" / "info.dat", std::ios::binary);
	ASSERT_TRUE(file.is_open());
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	ASSERT_EQ(data.size(), 0x2C + sizeof(InfoConstants));
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + 0x2C, sizeof(InfoConstants));
	const auto& tortoise = info->animal.at(static_cast<size_t>(AnimalInfo::Tortoise));
	EXPECT_EQ(tortoise.speedGroup.speedDefault, SpeedState::MetresSec_0_25);
	for (const auto type : {AnimalInfo::Sheep, AnimalInfo::Cow, AnimalInfo::Horse, AnimalInfo::Pig})
	{
		EXPECT_GT(static_cast<uint32_t>(info->animal.at(static_cast<size_t>(type)).speedGroup.speedDefault),
		          static_cast<uint32_t>(tortoise.speedGroup.speedDefault))
		    << static_cast<int>(type);
	}
	const auto& sheep = info->animal.at(static_cast<size_t>(AnimalInfo::Sheep));
	for (size_t type = 0; type < sizeof(IsReacting) / sizeof(uint32_t); ++type)
	{
		EXPECT_FALSE(ReactsTo(tortoise.isReacting, type)) << type;
	}
	EXPECT_TRUE(ReactsTo(sheep.isReacting, 7));
	EXPECT_TRUE(ReactsTo(sheep.isReacting, 28));
	EXPECT_EQ(tortoise.foodType, FoodType::None);
}

// A flock member keeps with its flock as the original's: it stays while it is within its domain and within the flock
// distance of the flock's position, the edge included; otherwise it walks to the point drawn near the flock when that
// point is within its domain, or when the flock's position itself is not, and else stays where it is
// (docs/wiki/beings/animals.md)
TEST(AnimalFlockRules, AMemberFarFromItsFlockWalksBackToAPointByItUnlessThatPointIsOutsideItsDomain)
{
	EXPECT_TRUE(ai::IsWithinFlockArea(true, 0.0f, 10.0f));
	EXPECT_TRUE(ai::IsWithinFlockArea(true, 10.0f, 10.0f));
	EXPECT_FALSE(ai::IsWithinFlockArea(true, 10.5f, 10.0f));
	// out of its domain it goes back whatever the distance
	EXPECT_FALSE(ai::IsWithinFlockArea(false, 0.0f, 10.0f));
	EXPECT_TRUE(ai::MovesBackToFlock(true, true));
	EXPECT_TRUE(ai::MovesBackToFlock(true, false));
	EXPECT_TRUE(ai::MovesBackToFlock(false, false));
	EXPECT_FALSE(ai::MovesBackToFlock(false, true));
}

// A wolf's prey search: no candidate stored takes any prey; else twice the prey's distance in map units against my cell
// indices' distance to the stored candidate in map units, so a new prey wins unless it is several times farther than
// the candidate's own position from the land's corner
TEST(AnimalPredatorRules, ANewPreyReplacesTheStoredOneByTheOriginalsMixedUnits)
{
	const glm::vec2 me(100.0f, 100.0f);
	// none stored
	EXPECT_TRUE(ai::PreyNearerThanStored(me, {900.0f, 900.0f}, glm::vec2(0.0f)));
	// a candidate stored at (500, 500): a prey 10 m away wins, one 300 m away does not
	EXPECT_TRUE(ai::PreyNearerThanStored(me, {110.0f, 100.0f}, {500.0f, 500.0f}));
	EXPECT_FALSE(ai::PreyNearerThanStored(me, {400.0f, 100.0f}, {500.0f, 500.0f}));
	// the mixed units: a candidate stored 1 m away still loses to a prey 40 m away
	EXPECT_TRUE(ai::PreyNearerThanStored(me, {140.0f, 100.0f}, {101.0f, 100.0f}));
}

// An animal's first clip is set as it is made, as the original's creation sets it: a dove tosses its coin then (the
// first of its two move clips, 8, on 0, the second, 9, on anything else) and keeps the clip's time
TEST_F(AnimalBehaviourTest, ADoveTakesItsFirstClipAsItIsMadeWithItsCoin)
{
	for (const auto [draw, clip] : {std::pair {0u, 8}, std::pair {1u, 9}})
	{
		game_random::testing::SetGameRand([draw](uint32_t) { return draw; }, [](float) { return 0.0f; });
		const auto dove = MakeAnimal(AnimalInfo::Dove, {500.0f, 500.0f});
		ecs::SetFirstAnimalClip(dove);
		const auto* animation = Reg().TryGet<const SkeletalAnimation>(dove);
		ASSERT_NE(animation, nullptr);
		EXPECT_TRUE(animation->hasClip);
		EXPECT_EQ(animation->clipIndex, clip) << draw;
	}
	game_random::testing::SetGameRand({}, {});
}

// ---- a bird's banking ---------------------------------------------------------------------------------------------

// Every bird kind leans 0.5 rad into a turn over 2 s, however fast it flies and however hard it turns; the spell's
// doves and bats lean over 0.5 s; what does not fly does not lean
// (docs/wiki/beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat,
// docs/wiki/magic/flocks.md#spelldove-spellbat-and-spellwolf-what-concerns-the-spell; every kind's two values are on
// the first page, under Banking)
TEST(AnimalBirdBanking, aBirdLeansHalfARadianOverTwoSecondsAndTheSpellsBirdsOverHalfASecond)
{
	for (const auto bird :
	     {AnimalInfo::Crow, AnimalInfo::Dove, AnimalInfo::Swallow, AnimalInfo::Pigeon, AnimalInfo::Seagull, AnimalInfo::Bat})
	{
		EXPECT_EQ(ai::TimeToBank(bird), 2.0f) << static_cast<int>(bird);
		for (const auto [speed, turn] : {std::pair {0.0f, 0.0f}, std::pair {100.0f, 3.0f}, std::pair {9830.0f, 512.0f}})
		{
			EXPECT_EQ(ai::BankAngle(bird, speed, turn), 0.5f) << static_cast<int>(bird) << " " << speed << " " << turn;
		}
	}
	for (const auto spellBird : {AnimalInfo::SpellDove, AnimalInfo::SpellBat})
	{
		EXPECT_EQ(ai::TimeToBank(spellBird), 0.5f) << static_cast<int>(spellBird);
		EXPECT_EQ(ai::BankAngle(spellBird, 9830.0f, 512.0f), 0.5f) << static_cast<int>(spellBird);
	}
	for (const auto walker : {AnimalInfo::Sheep, AnimalInfo::Cow, AnimalInfo::Tiger, AnimalInfo::Tortoise})
	{
		EXPECT_EQ(ai::TimeToBank(walker), 0.0f) << static_cast<int>(walker);
		EXPECT_EQ(ai::BankAngle(walker, 9830.0f, 512.0f), 0.0f) << static_cast<int>(walker);
	}
}

// ---- a villager brought down ----------------------------------------------------------------------------------------

namespace
{
/// The villagers' state table, reduced to the top state: a new state is written into the villager's own action
class TopStateOnly final: public test::LivingActionFakeBase
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, LivingAction::Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction& action, LivingAction::Index index, VillagerStates state,
	                      bool /*skipTransition*/) const override
	{
		action.states.at(static_cast<size_t>(index)) = static_cast<uint8_t>(state);
	}
	uint32_t VillagerCallState(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction& /*action*/, VillagerStates /*row*/, VillagerStates /*final*/,
	                                VillagerStates /*next*/) const override
	{
		return 1;
	}
	uint32_t VillagerCallExitState(LivingAction& /*action*/, VillagerStates /*row*/, VillagerStates /*next*/) const override
	{
		return 1;
	}
	int VillagerCallOutOfAnimation(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return -1; }
	bool VillagerCallValidate(LivingAction& /*action*/, LivingAction::Index /*index*/) const override { return false; }
};

VillagerStates TopOf(entt::entity villager)
{
	const auto& action = Locator::entitiesRegistry::value().Get<LivingAction>(villager);
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
}

void SetTopOf(entt::entity villager, VillagerStates state)
{
	auto& action = Locator::entitiesRegistry::value().Get<LivingAction>(villager);
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(state);
}
} // namespace

// A villager a predator brought down lies DOWNED until its clip has played (none here: no clips are loaded), then is
// eaten for 300 turns; one that cannot be eaten gets up (LANDED) when they end, alive and no longer caught; one taken out
// of those states meanwhile (picked up, thrown) is no longer caught either
// (docs/wiki/beings/villager-reactions.md#hunted-villagers, docs/wiki/beings/animals.md#predators-lion-tiger-leopard-wolf).
// The eaten villager's death at the end of the 300 turns is not run here: it goes through the villagers' whole death
// (their town, statistics, drops and the dynamics), more world than these fakes give
TEST_F(AnimalBehaviourTest, ADownedVillagerIsEatenForThreeHundredTurnsAndOneThatCannotBeEatenGetsUp)
{
	// the survival writes to the game's log
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::RestoreService<Locator::livingActionSystem> restoreStates;
	Locator::livingActionSystem::emplace<TopStateOnly>();
	const auto survivor = MakeVillager({100.0f, 100.0f}, false);
	ecs::script_held::SetCannotBeEaten(survivor);
	const auto pickedUp = MakeVillager({120.0f, 100.0f}, false);
	for (const auto villager : {survivor, pickedUp})
	{
		SetTopOf(villager, VillagerStates::Downed);
		Reg().Assign<DownedVillager>(villager);
	}

	// its clip has played: being eaten, the 300 turns counted from now
	ai::ProcessDownedVillagers();
	for (const auto villager : {survivor, pickedUp})
	{
		EXPECT_EQ(TopOf(villager), VillagerStates::BeingEaten);
		ASSERT_TRUE(Reg().AllOf<DownedVillager>(villager));
		EXPECT_EQ(Reg().Get<DownedVillager>(villager).counter, 300);
	}

	// the hand takes one up: it is no longer caught, and its state is left as the hand set it
	SetTopOf(pickedUp, VillagerStates::MoveToPos);
	ai::ProcessDownedVillagers();
	EXPECT_FALSE(Reg().AllOf<DownedVillager>(pickedUp));
	EXPECT_EQ(TopOf(pickedUp), VillagerStates::MoveToPos);

	// 299 turns of being eaten in all (the one above among them), then the 300th ends it
	for (int turn = 2; turn < 300; ++turn)
	{
		ai::ProcessDownedVillagers();
	}
	EXPECT_EQ(TopOf(survivor), VillagerStates::BeingEaten);
	ASSERT_TRUE(Reg().AllOf<DownedVillager>(survivor));
	EXPECT_EQ(Reg().Get<DownedVillager>(survivor).counter, 1);
	ai::ProcessDownedVillagers();
	EXPECT_EQ(TopOf(survivor), VillagerStates::Landed);
	EXPECT_FALSE(Reg().AllOf<DownedVillager>(survivor));
	EXPECT_EQ(Reg().Get<Villager>(survivor).life, 1.0f);
}

// ---- the land script's flocks -------------------------------------------------------------------------------------

namespace
{
/// The land script's version for one test, put back as it was afterwards
class ScriptVersion
{
public:
	explicit ScriptVersion(float version)
	    : _saved(Locator::mapScriptSystem::value().Globals().version)
	{
		Set(version);
	}
	~ScriptVersion() { Set(_saved); }
	ScriptVersion(const ScriptVersion&) = delete;
	ScriptVersion& operator=(const ScriptVersion&) = delete;
	ScriptVersion(ScriptVersion&&) = delete;
	ScriptVersion& operator=(ScriptVersion&&) = delete;
	static void Set(float version) { Locator::mapScriptSystem::value().Globals().version = version; }

private:
	float _saved;
};

/// A town of the land script's number `id`, with nothing in it
entt::entity MakeScriptTown(uint32_t id)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town, id).owner = PlayerNames::PLAYER_ONE;
	return town;
}

/// The flock the land script's id finds, or entt::null
entt::entity ScriptFlock(int32_t id)
{
	const auto& flocks = Locator::entitiesRegistry::value().Context().flocks;
	const auto found = flocks.find(id);
	return found != flocks.end() ? found->second : entt::entity {entt::null};
}
} // namespace

// CREATE_FLOCK makes a flock with the script's id at its first position, its domain round the second, its domain radius
// the line's (0 giving 80); from VERSION 2.1 on the flock distance is the fifth number and the town the sixth, before
// that the town is the fifth and the distance stays 30. With its town the flock goes on the town's list; the script's
// id finds the newest flock of that id, and the flock is the last thing created
// (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
TEST_F(AnimalBehaviourTest, TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion)
{
	using lhscriptx::FeatureScriptCommands;
	const ScriptVersion version(2.3f);
	const auto town = MakeScriptTown(5);

	FeatureScriptCommands::CreateFlock(7, {100.0f, 0.0f, 200.0f}, {150.0f, 0.0f, 250.0f}, 0, 40, 5);
	const auto first = ScriptFlock(7);
	ASSERT_NE(first, entt::entity {entt::null});
	EXPECT_EQ(lhscriptx::Script::LastCreated(), first);
	const auto& made = Reg().Get<Flock>(first);
	EXPECT_EQ(made.id, 7);
	EXPECT_EQ(made.savedDomainCentre, glm::vec3(100.0f, 0.0f, 200.0f));
	EXPECT_EQ(made.domainCentre, glm::vec3(150.0f, 0.0f, 250.0f));
	EXPECT_EQ(made.domainRadius, 80);
	EXPECT_EQ(made.flockDistance, 40);
	EXPECT_EQ(made.town, town);
	EXPECT_EQ(Reg().Get<Town>(town).flocks, std::vector {first});

	// a radius of its own is kept; VERSION 2.1 itself reads the new layout
	ScriptVersion::Set(2.1f);
	FeatureScriptCommands::CreateFlock(8, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, 120, 12, 5);
	ASSERT_NE(ScriptFlock(8), entt::entity {entt::null});
	const auto& wide = Reg().Get<Flock>(ScriptFlock(8));
	EXPECT_EQ(wide.domainRadius, 120);
	EXPECT_EQ(wide.flockDistance, 12);
	EXPECT_EQ(wide.town, town);

	// before 2.1: the fifth number is the town, the sixth is not read and the distance stays 30
	ScriptVersion::Set(2.0f);
	FeatureScriptCommands::CreateFlock(9, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, 60, 5, 12);
	ASSERT_NE(ScriptFlock(9), entt::entity {entt::null});
	const auto& old = Reg().Get<Flock>(ScriptFlock(9));
	EXPECT_EQ(old.domainRadius, 60);
	EXPECT_EQ(old.flockDistance, 30);
	EXPECT_EQ(old.town, town);
	// a number no town has: no town, on no list
	FeatureScriptCommands::CreateFlock(10, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 10.0f}, 0, 99, 5);
	ASSERT_NE(ScriptFlock(10), entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<Flock>(ScriptFlock(10)).town, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<Town>(town).flocks, (std::vector {first, ScriptFlock(8), ScriptFlock(9)}));

	// a second flock of id 7: the id now finds it
	FeatureScriptCommands::CreateFlock(7, {300.0f, 0.0f, 300.0f}, {300.0f, 0.0f, 300.0f}, 0, 99, 0);
	EXPECT_NE(ScriptFlock(7), first);
	EXPECT_EQ(Reg().Get<Flock>(ScriptFlock(7)).savedDomainCentre, glm::vec3(300.0f, 0.0f, 300.0f));
	lhscriptx::Script::SetLastCreated(entt::null);
}

// CREATE_FLOCK's flock belongs to the neutral player, as the original, the same player as an animal's own flock: a
// wanderer in a flock of the neutral player merges with a land script's flock of its kind, and one of player two's
// never does (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
TEST_F(AnimalBehaviourTest, TheLandScriptsFlockIsTheNeutralPlayersAndMergesWithAnAnimalsOwn)
{
	using lhscriptx::FeatureScriptCommands;
	const ScriptVersion version(2.3f);
	GAnimalInfo merging = Info(AnimalInfo::Sheep);
	merging.maxFlockSize = 50;
	merging.flocksCanMerge = 1;
	const auto wanderBy = [&merging](entt::entity animal, glm::ivec2 step) {
		auto& registry = Reg();
		Brain(animal).step = step;
		ai::Context ctx {animal, registry.Get<Animal>(animal), Brain(animal), registry.Get<Transform>(animal), merging};
		ai::Wander(ctx);
	};
	const auto flockOf = [](entt::entity animal) { return Reg().Get<const Animal>(animal).flock; };
	const glm::ivec2 metreEast {6554, 0};
	const glm::ivec2 twoMetresWest {-13107, 0};

	// two land script flocks with no town, of the neutral player
	FeatureScriptCommands::CreateFlock(7, {115.0f, 0.0f, 105.0f}, {115.0f, 0.0f, 105.0f}, 0, 30, 99);
	FeatureScriptCommands::CreateFlock(8, {115.0f, 0.0f, 305.0f}, {115.0f, 0.0f, 305.0f}, 0, 30, 99);
	const auto herd = ScriptFlock(7);
	const auto farHerd = ScriptFlock(8);
	ASSERT_NE(herd, entt::entity {entt::null});
	ASSERT_NE(farHerd, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const Flock>(herd).player, std::optional {PlayerNames::NEUTRAL});
	EXPECT_EQ(Reg().Get<const Flock>(farHerd).player, std::optional {PlayerNames::NEUTRAL});

	// a sheep of the first, and a wanderer beside it in a flock of its own, the neutral player's: they merge
	const auto other = MakeAnimal(AnimalInfo::Sheep, {115.0f, 105.0f}, herd);
	const auto wanderer = MakeAnimal(AnimalInfo::Sheep, {109.5f, 105.0f}, MakeFlock({109.5f, 105.0f}));
	Reg().Get<Flock>(flockOf(wanderer)).player = PlayerNames::NEUTRAL;
	wanderBy(wanderer, metreEast);
	wanderBy(wanderer, twoMetresWest);
	EXPECT_EQ(flockOf(wanderer), flockOf(other));

	// a sheep of the second, and a wanderer beside it in a flock of player two's: they stay apart
	const auto farOther = MakeAnimal(AnimalInfo::Sheep, {115.0f, 305.0f}, farHerd);
	const auto stranger = MakeAnimal(AnimalInfo::Sheep, {109.5f, 305.0f}, MakeFlock({109.5f, 305.0f}));
	Reg().Get<Flock>(flockOf(stranger)).player = PlayerNames::PLAYER_TWO;
	wanderBy(stranger, metreEast);
	wanderBy(stranger, twoMetresWest);
	EXPECT_NE(flockOf(stranger), flockOf(farOther));
	lhscriptx::Script::SetLastCreated(entt::null);
}

// CREATE_NEW_ANIMAL joins the animal to the flock of the line's id, a grazer keeping the town and an animal that cannot
// be herded taking the flock off its town's list; with no flock of that id the animal gets a flock of its own at its
// position, with no script id, its kind's domain radius and flock distance and no town. The animal is the last thing
// created (docs/wiki/scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal)
TEST_F(AnimalBehaviourTest, TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn)
{
	using lhscriptx::FeatureScriptCommands;
	auto info = GameAnimalRows();
	for (const auto type : {AnimalInfo::Sheep, AnimalInfo::Lion})
	{
		auto& row = info->animal.at(static_cast<size_t>(type));
		row.animalInfo = type;
		row.domainRadius = 70;
		row.flockDistance = 25.0f;
	}
	Locator::infoConstants::reset(info.release());
	const ScriptVersion version(2.3f);
	const auto town = MakeScriptTown(5);
	FeatureScriptCommands::CreateFlock(7, {100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}, 0, 30, 5);
	const auto herd = ScriptFlock(7);
	ASSERT_NE(herd, entt::entity {entt::null});

	FeatureScriptCommands::CreateNewAnimal({100.0f, 0.0f, 100.0f}, static_cast<int32_t>(AnimalInfo::Sheep), 7, 5, 13);
	const auto sheep = lhscriptx::Script::LastCreated();
	ASSERT_NE(sheep, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<Animal>(sheep).flock, herd);
	EXPECT_EQ(Reg().Get<Animal>(sheep).age, 13u);
	EXPECT_EQ(Reg().Get<Animal>(sheep).town, town);
	EXPECT_EQ(Reg().Get<Flock>(herd).members, std::vector {sheep});
	EXPECT_EQ(Reg().Get<Town>(town).flocks, std::vector {herd});

	// a lion cannot be herded: the flock leaves its town, and the lion has none
	FeatureScriptCommands::CreateNewAnimal({105.0f, 0.0f, 100.0f}, static_cast<int32_t>(AnimalInfo::Lion), 7, 5, 13);
	const auto lion = lhscriptx::Script::LastCreated();
	ASSERT_NE(lion, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<Animal>(lion).flock, herd);
	EXPECT_EQ(Reg().Get<Animal>(lion).town, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<Flock>(herd).members.size(), 2u);
	EXPECT_EQ(Reg().Get<Flock>(herd).town, entt::entity {entt::null});
	EXPECT_TRUE(Reg().Get<Town>(town).flocks.empty());

	// no flock of id 42: one of its own
	FeatureScriptCommands::CreateNewAnimal({300.0f, 0.0f, 200.0f}, static_cast<int32_t>(AnimalInfo::Sheep), 42, 5, 13);
	const auto alone = lhscriptx::Script::LastCreated();
	ASSERT_NE(alone, entt::entity {entt::null});
	const auto own = Reg().Get<Animal>(alone).flock;
	ASSERT_NE(own, entt::entity {entt::null});
	EXPECT_NE(own, herd);
	const auto& ownFlock = Reg().Get<Flock>(own);
	EXPECT_EQ(ownFlock.id, -1);
	EXPECT_EQ(ownFlock.members, std::vector {alone});
	EXPECT_EQ(ownFlock.domainRadius, 70);
	EXPECT_EQ(ownFlock.flockDistance, 25);
	EXPECT_EQ(ownFlock.town, entt::entity {entt::null});
	EXPECT_EQ(ownFlock.player, std::optional {PlayerNames::NEUTRAL});
	EXPECT_EQ(ownFlock.domainCentre.x, 300.0f);
	EXPECT_EQ(ownFlock.domainCentre.z, 200.0f);
	EXPECT_FALSE(Reg().Context().flocks.contains(-1));
	lhscriptx::Script::SetLastCreated(entt::null);
}

// ---- a script's move ------------------------------------------------------------------------------------------------

// MOVE_GAME_THING on an animal: nothing while it is out of the map's cells (here as the hand or the physics take it
// out); in them, it is already there only when its whole map units are less than its speed from the point, strictly,
// so a point 492 units off in whole units is a walk for a speed of 492 even when the exact distance is 491.2
// (docs/wiki/scripts/script-commands.md#how-a-living-sets-off-and-arrives)
TEST_F(AnimalBehaviourTest, AScriptsMoveTakesAnAnimalInTheMapAndMeasuresItsArrivalInWholeUnits)
{
	// 6553.6 map units a metre: the cow at 655360.9 units (whole 655360), the points at 655852.1 (whole 655852, 492
	// off) and 655851.1 (whole 655851, 491 off)
	const auto cow = MakeAnimal(AnimalInfo::Cow, {655360.9f / 6553.6f, 100.0f});
	const glm::vec2 walk {655852.1f / 6553.6f, 100.0f};
	const glm::vec2 there {655851.1f / 6553.6f, 100.0f};
	Brain(cow).speed = 492;
	const auto top = Brain(cow).topState;

	// out of the map's cells: nothing, though nothing holds it
	ecs::map_cells::RemoveMapObject(cow);
	ecs::animal_ai::ScriptMoveTo(cow, walk);
	EXPECT_EQ(Brain(cow).topState, top);
	EXPECT_EQ(Brain(cow).finalState, top);

	// back in them: 492 whole units is not less than the speed, so it walks there to stand in the script
	ecs::map_cells::InsertMapObject(cow);
	ecs::animal_ai::ScriptMoveTo(cow, walk);
	EXPECT_EQ(Brain(cow).topState, static_cast<uint8_t>(Info(AnimalInfo::Cow).moveState));
	EXPECT_EQ(Brain(cow).finalState, static_cast<uint8_t>(AnimalState::InScript));
	EXPECT_EQ(Brain(cow).goal, walk);

	// 491 whole units: there already, so it only takes the script's state
	ecs::animal_ai::ScriptMoveTo(cow, there);
	EXPECT_EQ(Brain(cow).topState, static_cast<uint8_t>(AnimalState::InScript));
	EXPECT_EQ(Brain(cow).goal, walk);
}
