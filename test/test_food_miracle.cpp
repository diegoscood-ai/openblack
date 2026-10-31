/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The food miracle, pinned to the original's rules (docs/wiki/magic/food-wood-water-heal.md#food-and-wood): where a
// grain becomes food and how much, how the pour holds, tips and lets go of the hand, the pile's rise out of the ground
// and its grain, and the pile's shadows. The numbers are the original's: the FOOD row of info.dat (200 food for the
// first grain, then 18, 7 prayer a unit) and SF_Food's raise (ClampHand 1, TotalTime 4 s, HeightToRaise 10,
// AngleToRaise 1.07257).

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/ShadowReceiver.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/MeshBoxProviderInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Chants.h"
#include "Magic/Core/Players.h"
#include "Magic/Core/Spell.h"
#include "Magic/HandHoldPose.h"
#include "Magic/HandMotion.h"
#include "Particles/SpellLink.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;
using openblack::ecs::components::SpellCreator;
using EventType = openblack::particles::SpellEventInfo::Type;

namespace
{
/// SF_Food's raise of the hand: the hand is held where the pour began, rises and tips over 4 s, and starts over
constexpr PourSettings k_FoodPour {
    .totalTime = 4.0f, .heightToRaise = 10.0f, .angleToRaise = 1.07257f, .loops = true, .clampHand = true};
/// SF_Water's: the same raise, but the hand is not held (ClampHand 0)
constexpr PourSettings k_WaterPour {
    .totalTime = 4.0f, .heightToRaise = 10.0f, .angleToRaise = 1.07257f, .loops = true, .clampHand = false};
/// A game turn
constexpr float k_TurnSeconds = 0.1f;

/// The food miracle over the tests' fake world: a registry, tables zeroed but for the FOOD row, the animals' shared
/// state the spell classes register with, and the inert miracles' service, whose world is a FakeMagicWorld, each put
/// back as it was when the test ends
class FoodMiracleSpell: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		// the spell classes register on their first use, and the flock classes register the flock animals' dying
		Locator::animalSystem::emplace<ecs::systems::AnimalSystem>();
		_magic = &static_cast<test::InertMagicSystem&>(Locator::magicSystem::emplace<test::InertMagicSystem>());
		auto info = std::make_unique<InfoConstants>();
		// the FOOD row: 200 food for the first grain, 18 for each later one, 7 prayer a unit
		auto& food = info->magicFood.at(0);
		food.resourceType = ResourceType::Food;
		food.resourceAmountFirstEvent = 200;
		food.resourceAmountPerEvent = 18;
		food.costPerUnit = 7;
		_info = info.get();
		Locator::infoConstants::reset(info.release());
	}

	[[nodiscard]] test::FakeMagicWorld& World() const { return _magic->FakeWorld(); }
	[[nodiscard]] static ecs::Registry& Registry() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] GMagicEffectInfo& Effect(MagicType type) const { return _info->magicEffect.at(static_cast<size_t>(type)); }

	/// A food miracle the first player cast at (100, 0, 100), with the prayer power in its horn
	[[nodiscard]] static entt::entity MakeFood(float prayerPower)
	{
		const auto spell = Registry().Create();
		auto& component = Registry().Assign<Spell>(spell);
		component.magicType = MagicType::Food;
		component.spellClass = ClassOf(MagicType::Food);
		component.creator = {.kind = SpellCreator::Kind::Player, .player = PlayerNames::PLAYER_ONE, .entity = entt::null};
		component.player = PlayerNames::PLAYER_ONE;
		component.hasPlayer = true;
		chants::SetChants(component, prayerPower);
		component.position = glm::vec3(100.0f, 0.0f, 100.0f);
		component.castPos = component.position;
		return spell;
	}

	/// A grain of the miracle lands at a point
	static void Land(entt::entity spell, glm::vec3 point)
	{
		const particles::SpellEventInfo event {.type = EventType::Landed,
		                                       .position = point,
		                                       .velocity = glm::vec3(0.0f, -1.0f, 0.0f),
		                                       .strength = 1.0f,
		                                       .checkShields = false,
		                                       .target = entt::null};
		OpsOf(SpellClass::Resource).spellEvent(spell, event);
	}

	[[nodiscard]] static float Chants(entt::entity spell) { return Registry().Get<const Spell>(spell).chants; }

private:
	// put back in reverse order: the tables, the miracles' service with its stores, the registry, then the animals'
	// shared state
	test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::magicSystem> _restoreMagic;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	InfoConstants* _info {nullptr};
	test::InertMagicSystem* _magic {nullptr};
};

/// Every mesh is a box 2 m tall standing on its origin, as wide as asked (2 m across unless told)
class FakeMeshBoxes final: public ecs::systems::MeshBoxProviderInterface
{
public:
	explicit FakeMeshBoxes(float halfWidth = 1.0f)
	    : _halfWidth(halfWidth)
	{
	}
	[[nodiscard]] std::optional<AxisAlignedBoundingBox> MeshBox(entt::id_type /*meshId*/) const override
	{
		return AxisAlignedBoundingBox {.minima = glm::vec3(-_halfWidth, 0.0f, -_halfWidth),
		                               .maxima = glm::vec3(_halfWidth, 2.0f, _halfWidth)};
	}

private:
	float _halfWidth;
};

/// The miracle's piles on flat land at height 0: a registry, the magic food and magic wood rows of the pots' table and
/// the meshes' boxes, each put back as it was when the test ends
class FoodMiraclePile: public ::testing::Test
{
protected:
	static constexpr uint16_t k_MaxInPile = 1000;

	void SetUp() override
	{
		Locator::meshBoxProvider::emplace<FakeMeshBoxes>();
		auto info = std::make_unique<InfoConstants>();
		auto& food = info->pot.at(static_cast<size_t>(PotInfo::MagicFood));
		food.potType = PotType::PileFood;
		food.resourceType = ResourceType::Food;
		food.maxAmountInPot = k_MaxInPile;
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.potType = PotType::PileWood;
		wood.resourceType = ResourceType::Wood;
		wood.maxAmountInPot = k_MaxInPile;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	/// A pile as the miracle makes it, at its creation scale (magic food 0.3, magic wood 0.7), its size not yet set
	[[nodiscard]] static entt::entity MakePile(PotInfo type, uint16_t amount)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto pile = registry.Create();
		const float scale = type == PotInfo::MagicFood ? 0.3f : 0.7f;
		registry.Assign<ecs::components::Transform>(pile, glm::vec3(50.0f, 0.0f, 50.0f), glm::mat3(1.0f), glm::vec3(scale));
		registry.Assign<ecs::components::Pot>(pile, amount, k_MaxInPile, type);
		registry.Assign<ecs::components::Mesh>(pile, entt::id_type {1}, int8_t {0}, int8_t {1});
		return pile;
	}

	[[nodiscard]] static float HeightOf(entt::entity pile)
	{
		return Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(pile).position.y;
	}

private:
	// the registry goes first, while the tables and the boxes it was made with are still there
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

/// Food put down on the land among food piles of the map cells: a registry, the food piles' rows of the pots' table,
/// the meshes' boxes and the map cells, each put back as it was when the test ends. There is no land, so what no pile
/// takes makes no new pile (the sea)
class FoodMiracleDrop: public ::testing::Test
{
protected:
	static constexpr uint16_t k_Most = 1000;
	/// No next pot: what such a pile takes is not capped at its most
	static constexpr auto k_NoNextPot = static_cast<PotInfo>(19);

	void SetUp() override
	{
		Locator::meshBoxProvider::emplace<FakeMeshBoxes>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		auto info = std::make_unique<InfoConstants>();
		// pots and piles are things of type 21, filed at the tail of their cell's fixed list
		const auto foodPile = [&info](PotInfo type, PotInfo next) {
			auto& row = info->pot.at(static_cast<size_t>(type));
			row.type = ObjectType::Pot;
			row.potType = PotType::PileFood;
			row.resourceType = ResourceType::Food;
			row.maxAmountInPot = k_Most;
			row.nextPotForResource = next;
		};
		// the miracle's own pile and the pile a store or a worship site keeps its food in: not capped
		foodPile(PotInfo::MagicFood, k_NoNextPot);
		foodPile(PotInfo::StoragePitFoodPile, k_NoNextPot);
		// a pile with a next pot: capped at its most
		foodPile(PotInfo::FoodPile, PotInfo::FoodPot);
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
	}

	/// A food pile of a type and amount at a point, at scale 1, sized and filed in its map cell as a new pile is
	static entt::entity MakePile(PotInfo type, uint16_t amount, glm::vec3 at)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto pile = registry.Create();
		registry.Assign<ecs::components::Transform>(pile, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::Pot>(pile, amount, k_Most, type);
		registry.Assign<ecs::components::Mesh>(pile, entt::id_type {1}, int8_t {0}, int8_t {1});
		ecs::archetypes::PotArchetype::SetSize(pile, false);
		ecs::map_cells::InsertMapObject(pile);
		return pile;
	}

	/// Food put down at a point by no interface; what the piles there took
	static uint32_t Drop(glm::vec3 at, uint32_t amount)
	{
		return ecs::pot_resource::AddResourceToPos(at, {}, ResourceType::Food, amount, false, false);
	}

	[[nodiscard]] static uint32_t AmountOf(entt::entity pile)
	{
		return Locator::entitiesRegistry::value().Get<const ecs::components::Pot>(pile).amount;
	}

private:
	// the registry goes first, then the map cells, while the tables and the boxes are still there
	test::RestoreService<Locator::meshBoxProvider> _restoreBoxes;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};

/// The food pile's proportion raised, as the wiki gives it: x = amount / most, p = 0.05 + 0.95 x, then 1 - (1 - p)^2
float FoodPileRaised(float amount, float most)
{
	const float p = 0.05f + 0.95f * (amount / most);
	return 1.0f - (1.0f - p) * (1.0f - p);
}
} // namespace

// ---- Casting and paying ----

TEST_F(FoodMiracleSpell, AGrainIsPaidForWhereverItLandsButIsFoodOnlyOnDryLandWhileTheMiracleHasStrength)
{
	World().wetBelowX = 50.0f;
	const auto spell = MakeFood(7000.0f);
	// over water: the first grain's 200 food are paid for (1400) and lost
	Land(spell, {10.0f, 0.0f, 100.0f});
	EXPECT_TRUE(World().resources.empty());
	EXPECT_FLOAT_EQ(Chants(spell), 7000.0f - 1400.0f);
	// off the map: the next grain's 18 are paid for (126) and lost; the first grain was the one over the water
	Land(spell, {-5.0f, 0.0f, 100.0f});
	EXPECT_TRUE(World().resources.empty());
	EXPECT_FLOAT_EQ(Chants(spell), 7000.0f - 1400.0f - 126.0f);
	// on dry land it is food: 18 more, where it landed
	Land(spell, {100.0f, 0.0f, 100.0f});
	ASSERT_EQ(World().resources.size(), 1u);
	EXPECT_EQ(World().resources[0].type, ResourceType::Food);
	EXPECT_EQ(World().resources[0].amount, 18u);
	EXPECT_EQ(World().resources[0].point, glm::vec3(100.0f, 0.0f, 100.0f));
	EXPECT_FLOAT_EQ(Chants(spell), 7000.0f - 1400.0f - 2.0f * 126.0f);

	// the strength is read after the grain is paid for: a grain that spends the last of the horn's prayer finds the
	// miracle with none and is lost, on dry land too
	const auto spent = MakeFood(1400.0f);
	Land(spent, {100.0f, 0.0f, 100.0f});
	EXPECT_EQ(World().resources.size(), 1u);
	EXPECT_FLOAT_EQ(Chants(spent), 0.0f);

	// a miracle that has closed down neither pays for its grains nor puts them down
	const auto closed = MakeFood(7000.0f);
	Registry().Get<Spell>(closed).closedDown = true;
	Land(closed, {100.0f, 0.0f, 100.0f});
	EXPECT_EQ(World().resources.size(), 1u);
	EXPECT_FLOAT_EQ(Chants(closed), 7000.0f);
}

TEST_F(FoodMiracleSpell, TheFoodAGrainBringsIsItsAmountTimesTheTribalPowerCutDownToAWholeUnit)
{
	// the caster's first tribe has a tribal power of 1.25, and the FOOD miracle uses that tribe's
	Effect(MagicType::Food).useTribalPowerMultiplier.at(0) = 1;
	players::MagicOf(PlayerNames::PLAYER_ONE).tribalPower.at(0) = 1.25f;
	const auto spell = MakeFood(7000.0f);
	Land(spell, {100.0f, 0.0f, 100.0f});
	Land(spell, {101.0f, 0.0f, 100.0f});
	ASSERT_EQ(World().resources.size(), 2u);
	// the first grain's 200 x 1.25
	EXPECT_EQ(World().resources[0].amount, 250u);
	// a later grain's 18 x 1.25 = 22.5, cut down, not rounded
	EXPECT_EQ(World().resources[1].amount, 22u);
}

// ---- The hand while pouring ----

TEST(FoodMiracle, TheTipRollsTheHandBackAboutTheLineToTheCameraByThePoursTilt)
{
	// two seconds into the pour, the middle of its rise and fall: the tilt peaks at about 1.73 rad (the wiki's)
	const glm::vec3 hand(100.0f, 20.0f, 100.0f);
	PourState pour;
	StartPour(pour, k_FoodPour, hand);
	for (int turn = 0; turn < 20; ++turn)
	{
		StepPour(pour, k_TurnSeconds);
	}
	const float tilt = PourPoseAt(pour, 1.0f).tilt;
	EXPECT_NEAR(tilt, 1.73f, 0.005f);
	// the camera due east of the hand, level with it: the line from the hand to the camera is +x. The hand's up is
	// straight up turned by minus the tilt about that line, with nothing in between to smooth it
	const glm::vec3 camera(150.0f, 20.0f, 100.0f);
	const auto up = hand_hold::HeldUp(camera, hand, tilt, 0.0f);
	EXPECT_NEAR(up.x, 0.0f, 1e-5f);
	EXPECT_NEAR(up.y, std::cos(tilt), 1e-5f);
	EXPECT_NEAR(up.z, -std::sin(tilt), 1e-5f);
}

TEST(FoodMiracle, TheHandIsPinnedWhereThePourBeganForTheWholePour)
{
	const glm::vec3 start(100.0f, 30.0f, 200.0f);
	PourState pour;
	StartPour(pour, k_FoodPour, start);
	ASSERT_TRUE(PinnedHand(pour).has_value());
	EXPECT_EQ(*PinnedHand(pour), start);
	// through a whole rise and fall and on into the next one, the place is not taken again
	for (int turn = 0; turn < 45; ++turn)
	{
		StepPour(pour, k_TurnSeconds);
		const auto pose = PourPoseAt(pour, 0.5f);
		ASSERT_TRUE(pose.pinned.has_value());
		EXPECT_EQ(*pose.pinned, start);
	}
	EXPECT_TRUE(pour.active);
	// the pour stops: the hand is let go
	StopPour(pour);
	EXPECT_FALSE(PinnedHand(pour).has_value());
	// water's pour does not hold the hand: it follows the mouse
	PourState water;
	StartPour(water, k_WaterPour, start);
	StepPour(water, k_TurnSeconds);
	EXPECT_FALSE(PinnedHand(water).has_value());
}

TEST(FoodMiracle, StoppingEasesTheHandBackToRestOverOneGameTurn)
{
	PourState pour;
	StartPour(pour, k_FoodPour, glm::vec3(0.0f));
	for (int turn = 0; turn < 20; ++turn)
	{
		StepPour(pour, k_TurnSeconds);
	}
	// what the last turn drew, the start of the pose between this turn and the next
	const auto last = PourPoseAt(pour, 0.0f);
	ASSERT_GT(last.raise, 10.0f);
	ASSERT_GT(last.tilt, 1.0f);
	// the miracle ends: the pose drawn goes from the last turn's to rest by the fraction of the turn drawn
	StopPour(pour);
	for (const float fraction : {0.0f, 0.25f, 0.5f, 1.0f})
	{
		const auto pose = PourPoseAt(pour, fraction);
		EXPECT_FLOAT_EQ(pose.raise, last.raise + (0.0f - last.raise) * fraction);
		EXPECT_FLOAT_EQ(pose.tilt, last.tilt + (0.0f - last.tilt) * fraction);
	}
	// from the next game turn the hand is at rest, whatever the fraction
	StepPour(pour, k_TurnSeconds);
	for (const float fraction : {0.0f, 0.5f, 1.0f})
	{
		EXPECT_EQ(PourPoseAt(pour, fraction).raise, 0.0f);
		EXPECT_EQ(PourPoseAt(pour, fraction).tilt, 0.0f);
	}
	EXPECT_FALSE(pour.active);
}

TEST_F(FoodMiracleSpell, ThePourStopsWhenAMiracleThisComputerCastClosesDown)
{
	using namespace ecs::systems;
	hand_grain::Reset();
	const auto spell = MakeFood(7000.0f);
	Registry().Get<Spell>(spell).isMyInterfaceCasting = true;
	hand_grain::Start(true, 4.0f, 10.0f, 1.07257f, true);
	hand_grain::GameTurnUpdate(k_TurnSeconds);
	ASSERT_TRUE(hand_grain::Active());
	// its close-down stops the hand's raise and lets go of the hand
	OpsOf(SpellClass::Resource).closeDown(spell);
	EXPECT_TRUE(Registry().Get<const Spell>(spell).closedDown);
	EXPECT_FALSE(hand_grain::Active());
	EXPECT_FALSE(hand_grain::ClampedPosition().has_value());
	// only its first close-down does: a pour started after it goes on
	hand_grain::Start(true, 4.0f, 10.0f, 1.07257f, true);
	OpsOf(SpellClass::Resource).closeDown(spell);
	EXPECT_TRUE(hand_grain::Active());
	// a miracle no interface of this computer cast leaves the hand alone
	const auto other = MakeFood(7000.0f);
	OpsOf(SpellClass::Resource).closeDown(other);
	EXPECT_TRUE(hand_grain::Active());
	hand_grain::Reset();
}

// ---- Landing and piles ----

TEST_F(FoodMiracleDrop, AGrainLooksForATakerInItsOwnCellThenTheEightAroundItAndNoFurther)
{
	// piles that reach 46 m: 30 m half across at scale 1, half full (a radius of 23 m, a pot reaching twice that)
	Locator::meshBoxProvider::emplace<FakeMeshBoxes>(30.0f);
	const glm::vec3 grain(15.0f, 0.0f, 15.0f); // in the 10 m cell (1, 1)
	// two cells to the east: within reach, but not among the nine cells looked in
	const auto twoCellsAway = MakePile(PotInfo::MagicFood, 500, {35.0f, 0.0f, 15.0f});
	EXPECT_EQ(Drop(grain, 18), 0u);
	EXPECT_EQ(AmountOf(twoCellsAway), 500u);
	// the next cell to the east is one of them
	const auto nextCell = MakePile(PotInfo::MagicFood, 500, {25.0f, 0.0f, 15.0f});
	EXPECT_EQ(Drop(grain, 18), 18u);
	EXPECT_EQ(AmountOf(nextCell), 518u);
	EXPECT_EQ(AmountOf(twoCellsAway), 500u);
	// the grain's own cell is looked in first, whichever pile was made first
	const auto ownCell = MakePile(PotInfo::MagicFood, 500, {12.0f, 0.0f, 15.0f});
	EXPECT_EQ(Drop(grain, 18), 18u);
	EXPECT_EQ(AmountOf(ownCell), 518u);
	EXPECT_EQ(AmountOf(nextCell), 518u);
}

TEST_F(FoodMiracleDrop, AWorshipSitesFoodPotTakesFoodWithinTwiceItsRadius)
{
	// the pile a worship site keeps its food in, half full: 1 m half across at scale 1, a radius of 0.77 m
	const glm::vec3 at(55.0f, 0.0f, 55.0f);
	const auto pot = MakePile(PotInfo::StoragePitFoodPile, 500, at);
	const float radius = FoodPileRaised(500.0f, 1000.0f);
	// further than 1.2 times its radius but within twice it: a pot takes it
	EXPECT_EQ(Drop(at + glm::vec3(1.6f * radius, 0.0f, 0.0f), 18), 18u);
	EXPECT_EQ(AmountOf(pot), 518u);
	// beyond twice its radius it does not
	EXPECT_EQ(Drop(at + glm::vec3(2.2f * radius, 0.0f, 0.0f), 18), 0u);
	EXPECT_EQ(AmountOf(pot), 518u);
}

TEST_F(FoodMiracleDrop, PilesInReachTakeItInTurnAndOnlyAMagicPileTakesMoreThanItsMost)
{
	// two piles where the grain lands, the one with a next pot made first: it is offered the food first and takes what
	// it can, up to its most; the rest goes on to the magic pile
	const glm::vec3 at(55.0f, 0.0f, 55.0f);
	const auto capped = MakePile(PotInfo::FoodPile, 990, at);
	const auto magicPile = MakePile(PotInfo::MagicFood, 200, at);
	EXPECT_EQ(Drop(at, 200), 200u);
	EXPECT_EQ(AmountOf(capped), 1000u);
	EXPECT_EQ(AmountOf(magicPile), 390u);
	// full, it takes nothing more; the magic pile takes it all, past its most
	EXPECT_EQ(Drop(at, 1000), 1000u);
	EXPECT_EQ(AmountOf(capped), 1000u);
	EXPECT_EQ(AmountOf(magicPile), 1390u);
	// the first that qualifies, not the nearest: a pile made first half a metre away takes a grain before one under it
	const glm::vec3 there(155.0f, 0.0f, 155.0f);
	const auto first = MakePile(PotInfo::MagicFood, 500, there + glm::vec3(0.5f, 0.0f, 0.0f));
	const auto nearest = MakePile(PotInfo::MagicFood, 500, there);
	EXPECT_EQ(Drop(there, 18), 18u);
	EXPECT_EQ(AmountOf(first), 518u);
	EXPECT_EQ(AmountOf(nearest), 500u);
}

TEST_F(FoodMiraclePile, ANewPileRisesOutOfTheGroundOverOneSecond)
{
	using ecs::archetypes::PotArchetype;
	const auto pile = MakePile(PotInfo::MagicFood, 200);
	PotArchetype::SetSize(pile, true);
	// its height: the mesh's 2 m at the magic food's 0.3
	const float height = 0.6f;
	const auto& sink = Locator::entitiesRegistry::value().Get<const ecs::components::PileSink>(pile);
	EXPECT_FLOAT_EQ(sink.height, height);
	// it starts buried, its top at the ground
	EXPECT_FLOAT_EQ(HeightOf(pile), -height);
	// it rises to (the proportion raised - 1) x its height
	const float target = (FoodPileRaised(200.0f, 1000.0f) - 1.0f) * height;
	// half way through the second, the zoomer's ease from rest to rest has covered 0.6875 of the way
	PotArchetype::UpdateSizes(0.5f);
	EXPECT_NEAR(HeightOf(pile), -height + 0.6875f * (target + height), 1e-4f);
	EXPECT_TRUE(sink.offset.IsMoving());
	// at the second's end it is there, and stays
	PotArchetype::UpdateSizes(0.5f);
	EXPECT_NEAR(HeightOf(pile), target, 1e-6f);
	EXPECT_FALSE(sink.offset.IsMoving());
	PotArchetype::UpdateSizes(0.5f);
	EXPECT_NEAR(HeightOf(pile), target, 1e-6f);
}

TEST_F(FoodMiraclePile, TheGrainTextureFlowsWhileTheFoodPileStillSinks)
{
	using ecs::archetypes::PotArchetype;
	const auto pile = MakePile(PotInfo::MagicFood, 200);
	PotArchetype::SetSize(pile, true);
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_NE(registry.TryGet<const ecs::components::UvScroll>(pile), nullptr);
	const auto v = [&registry, pile] { return registry.Get<const ecs::components::UvScroll>(pile).v; };
	// the texture's v offset is 0.25 (1 - clamp(offset / height + 1, 0, 1)): a quarter while buried...
	EXPECT_FLOAT_EQ(v(), 0.25f);
	// ...moving with the pile while it rises...
	PotArchetype::UpdateSizes(0.5f);
	const float raised = FoodPileRaised(200.0f, 1000.0f);
	const float halfWay = -1.0f + 0.6875f * raised; // the offset over the height, half way through the second
	EXPECT_NEAR(v(), 0.25f * (1.0f - (halfWay + 1.0f)), 1e-5f);
	// ...and still once it is up
	PotArchetype::UpdateSizes(0.5f);
	EXPECT_NEAR(v(), 0.25f * (1.0f - raised), 1e-5f);
	PotArchetype::UpdateSizes(0.5f);
	EXPECT_NEAR(v(), 0.25f * (1.0f - raised), 1e-5f);
	// the magic wood pile's logs do not flow
	const auto wood = MakePile(PotInfo::MagicWood, 500);
	PotArchetype::SetSize(wood, true);
	EXPECT_EQ(registry.TryGet<const ecs::components::UvScroll>(wood), nullptr);
}

TEST(FoodMiracle, AMagicFoodPileReceivesNoProjectedShadowButAMagicWoodPileDoes)
{
	using ecs::shadow_receiver::Receives;
	EXPECT_FALSE(Receives({.pot = PotInfo::MagicFood}));
	EXPECT_TRUE(Receives({.pot = PotInfo::MagicWood}));
}
