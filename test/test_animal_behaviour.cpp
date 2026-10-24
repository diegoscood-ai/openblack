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

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LandFakes.h"
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

/// The parts of info.dat's animal rows these tests read, as the frozen game has them
std::unique_ptr<InfoConstants> GameAnimalRows()
{
	auto info = std::make_unique<InfoConstants>();
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
