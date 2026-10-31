/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's fires over a small world of their own: the fire list and its turn, the spread through the cells round a
// fire, the blazes and their heads, a fire that starts moving, the starts from a burn, the script's fire natives and the
// storm that watches its rain put a fire out. The objects burn with the frozen data's values (trees: combustion 110,
// heat capacity 1000, burn defence 0.01, a bush's capacity 100; the short Celtic fence: 300, 150 and 0.2), and every
// expected temperature is worked out with the fire model's own formulas, in the order the original's turn takes them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/FireSystem.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/SpellSystemInterface.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "Fire/FireModel.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Script/CHLFire.h"
#include "Magic/Spells/SpellStormAndTornado.h"
#include "support/FireFakes.h"
#include "support/LandFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack::ecs;
using openblack::Locator;
using openblack::PlayerNames;
namespace model = openblack::fire;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
// The frozen data's rows (info.dat): every tree's combustion, burn defence and heat capacity, the bushes' capacity,
// and the short Celtic fence's
constexpr float k_TreeCombustion = 110.0f;
constexpr float k_TreeCapacity = 1000.0f;
constexpr float k_BushCapacity = 100.0f;
constexpr float k_TreeDefence = 0.01f;
constexpr float k_FenceCombustion = 300.0f;
constexpr float k_FenceCapacity = 150.0f;
constexpr float k_FenceDefence = 0.2f;

constexpr float k_Air = model::k_AmbientTemperature;
/// A tree at twice its combustion temperature: as hot as burning makes it, so it stays so turn after turn
constexpr float k_Burning = 2.0f * k_TreeCombustion;

// The meshes: a tree 2 m across and 8 m tall (radius 1), a fence 1 m across (radius 0.5)
constexpr entt::id_type k_TreeMesh = 101;
constexpr entt::id_type k_FenceMesh = 102;
constexpr float k_TreeRadius = 1.0f;
constexpr float k_TreeHeight = 8.0f;
/// The fence's place in the cells: a circle of this radius round its position, reached this far
constexpr float k_FenceShapeRadius = 0.5f;
constexpr float k_FenceShapeReach = 1.0f;

/// The objects stand on the land: the fake land's height everywhere
constexpr float k_Ground = openblack::test::WaterCellIsland::k_HeightMarker;

std::optional<openblack::AxisAlignedBoundingBox> Boxes(entt::id_type id)
{
	switch (id)
	{
	case k_TreeMesh:
		return openblack::AxisAlignedBoundingBox {{-1.0f, 0.0f, -1.0f}, {1.0f, 8.0f, 1.0f}};
	case k_FenceMesh:
		return openblack::AxisAlignedBoundingBox {{-0.5f, 0.0f, -0.5f}, {0.5f, 1.0f, 0.5f}};
	default:
		return std::nullopt;
	}
}

/// A tree of that heat capacity as the fire model sees it
model::Material TreeMaterial(float capacity = k_TreeCapacity)
{
	return {.combustion = k_TreeCombustion,
	        .capacity = capacity,
	        .defence = k_TreeDefence,
	        .rainCooling = model::k_RainCooling,
	        .radius = k_TreeRadius,
	        .height = k_TreeHeight};
}

/// What a tree burning at twice its combustion temperature makes a target of that capacity at that temperature
float HeatedByABurningTree(float target, float capacity)
{
	return model::HeatTransfer(k_Burning, TreeMaterial(), target, {.capacity = capacity}).targetTemperature;
}

/// The temperature of a cold tree beside a tree burning at twice its combustion temperature, at the end of each turn
/// until it burns: the burning tree heats it in the turn it is made, and from the next turn on it takes its own turn
/// first, as the newest fire of the list, before the burning tree heats it again
std::vector<float> CatchingNeighbour(const model::Material& material, int turns)
{
	std::vector<float> temperatures;
	model::State state {.temperature = k_Air, .previous = k_Air};
	for (int turn = 0; turn < turns; ++turn)
	{
		if (turn > 0)
		{
			static_cast<void>(model::Step(state, material, {}));
			state.previous = state.temperature;
		}
		state.temperature = model::HeatTransfer(k_Burning, TreeMaterial(), state.temperature, material).targetTemperature;
		temperatures.push_back(state.temperature);
		if (model::IsOnFire(state.temperature, material))
		{
			break;
		}
	}
	return temperatures;
}

/// The game's fires, real, over a world of their own: a dry land of 64 x 64 cells, the map cells, the reactions and the
/// object rows of the frozen data. One turn is the fire system's turn
class FireWorld: public ::testing::Test
{
protected:
	void SetUp() override
	{
		openblack::test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<Registry>();
		openblack::test::EmplaceWorldSystems();
		fires = &Locator::fireSystem::emplace<systems::FireSystem>();
		// the weather a fire asks at its place reads the clock
		Locator::dayNightClock::emplace<systems::DayNightClockSystem>();
		map_cells::Clear();
		object_index::OnLoadMap();
		openblack::test::FakeMeshBoxProvider meshBoxes;
		meshBoxes.meshBox = &Boxes;
		Locator::meshBoxProvider::emplace<openblack::test::FakeMeshBoxProvider>(meshBoxes);
		// a fence is in every cell its circle touches
		openblack::test::FakeMapShapeProvider shapes;
		shapes.meshShape = [](entt::entity object, map_collide::Shape& shape, float& reach) {
			const auto& position = Locator::entitiesRegistry::value().Get<const components::Transform>(object).position;
			shape = {{position.x, position.z}, k_FenceShapeRadius, {}, 0.0f, "fence"};
			reach = k_FenceShapeReach;
			return true;
		};
		Locator::mapShapeProvider::emplace<openblack::test::FakeMapShapeProvider>(shapes);
		Locator::terrainSystem::emplace<openblack::test::WaterCellIsland>(uint16_t {4});
		auto info = std::make_unique<openblack::InfoConstants>();
		for (const auto type : {openblack::TreeInfo::Beech, openblack::TreeInfo::Bush})
		{
			auto& row = info->tree.at(static_cast<size_t>(type));
			row.type = openblack::ObjectType::ForestTree;
			row.combustionTemperature = k_TreeCombustion;
			row.heatCapacity = type == openblack::TreeInfo::Bush ? k_BushCapacity : k_TreeCapacity;
			row.defenceMultiplierBurn = k_TreeDefence;
		}
		auto& fence = info->mobileStatic.at(static_cast<size_t>(openblack::MobileStaticInfo::CeltFenceShort));
		fence.type = openblack::ObjectType::MobileStatic;
		fence.combustionTemperature = k_FenceCombustion;
		fence.heatCapacity = k_FenceCapacity;
		fence.defenceMultiplierBurn = k_FenceDefence;
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		// the fires go as at a new land, with their crackle
		fires->Reset();
		Locator::dayNightClock::reset();
		map_cells::Clear();
		Locator::infoConstants::reset();
		Locator::terrainSystem::reset();
		// the registry first, as the game's shutdown does
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
	}

	static Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity Place(float x, float z)
	{
		const auto object = Reg().Create();
		object_index::Assign(object);
		Reg().Assign<components::Transform>(object, glm::vec3(x, k_Ground, z), glm::mat3(1.0f), glm::vec3(1.0f));
		return object;
	}
	/// A tree on the map
	static entt::entity TreeAt(float x, float z, openblack::TreeInfo type = openblack::TreeInfo::Beech)
	{
		const auto tree = Place(x, z);
		Reg().Assign<components::Tree>(tree, type, 1.0f);
		Reg().Assign<components::Mesh>(tree, k_TreeMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		map_cells::InsertMapObject(tree);
		return tree;
	}
	/// A short Celtic fence on the map, in every cell its circle touches
	static entt::entity FenceAt(float x, float z)
	{
		const auto fence = Place(x, z);
		Reg().Assign<components::MobileStatic>(fence, openblack::MobileStaticInfo::CeltFenceShort);
		Reg().Assign<components::Mesh>(fence, k_FenceMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		map_cells::InsertMapObject(fence);
		return fence;
	}

	/// The object is made this hot (a new fire for it), with what heated it
	fire::FireEffect* Heat(entt::entity object, float temperature, entt::entity source = entt::null)
	{
		fires->SetTemperature(object, temperature, source);
		return fires->FindByObject(object);
	}

	void Turn() { fires->ProcessTurn(); }

	systems::FireSystemInterface* fires {nullptr};
};
} // namespace

TEST_F(FireWorld, TheListIsNewestFirstAndANewFireWaitsForTheNextTurn)
{
	// One list, the newest fire first
	auto* first = Heat(TreeAt(500.5f, 500.5f), 60.0f);
	auto* second = Heat(TreeAt(500.5f, 550.5f), 60.0f);
	ASSERT_NE(first, nullptr);
	ASSERT_NE(second, nullptr);
	EXPECT_EQ(fires->List(), (std::vector<fire::FireEffect*> {second, first}));

	// A fire made while the list is walked is not walked that turn: the burning tree's heat is all the new one has
	const auto burning = TreeAt(100.5f, 100.5f);
	const auto neighbour = TreeAt(101.5f, 100.5f);
	ASSERT_NE(Heat(burning, k_Burning), nullptr);
	Turn();
	auto* caught = fires->FindByObject(neighbour);
	ASSERT_NE(caught, nullptr);
	const float firstTurn = HeatedByABurningTree(k_Air, k_TreeCapacity);
	EXPECT_FLOAT_EQ(caught->temperature, firstTurn);
	EXPECT_FLOAT_EQ(caught->previous, k_Air);
	EXPECT_EQ(fires->List().front(), caught);

	// The next turn it takes its own turn first, then the burning tree heats it again
	Turn();
	model::State state {.temperature = firstTurn, .previous = k_Air};
	static_cast<void>(model::Step(state, TreeMaterial(), {}));
	EXPECT_FLOAT_EQ(caught->previous, state.temperature);
	EXPECT_FLOAT_EQ(caught->temperature, HeatedByABurningTree(state.temperature, k_TreeCapacity));
}

TEST_F(FireWorld, AFireHeatsWhatIsInTheCellsOfItsSpiralOncePerCellUntilACellIsOutOfReach)
{
	// A tree burning at twice its combustion temperature reaches 1.25 m (its radius by 1.25), so its cells are searched
	// while they are within 11.25 m: its own cell, then the next one along -x ("west"), then the one at -x -z, 14.1 m
	// away, ends the search. The cell along -z ("south") is never searched, though the tree there is as near as the one
	// to the west
	const auto burning = TreeAt(300.5f, 300.5f);
	const auto own = TreeAt(301.5f, 301.5f);
	const auto west = TreeAt(299.0f, 300.5f);
	const auto south = TreeAt(300.5f, 299.0f);
	const auto distant = TreeAt(306.0f, 306.0f);
	// a fence across the burning tree's cell and the one west of it: there is no record of what was heated, so it is
	// heated once in each
	const auto fence = FenceAt(300.0f, 302.0f);
	ASSERT_EQ(map_cells::CellsOf(fence).size(), 2u);
	ASSERT_NE(Heat(burning, k_Burning), nullptr);
	Turn();

	const float once = HeatedByABurningTree(k_Air, k_TreeCapacity);
	for (const auto tree : {own, west})
	{
		const auto* fire = fires->FindByObject(tree);
		ASSERT_NE(fire, nullptr);
		EXPECT_FLOAT_EQ(fire->temperature, once);
		// it joins the burning tree's blaze
		EXPECT_EQ(fire->root, fires->FindByObject(burning));
	}
	EXPECT_EQ(fires->FindByObject(south), nullptr);
	// in its cell but beyond its reach
	EXPECT_EQ(fires->FindByObject(distant), nullptr);
	const auto* fenceFire = fires->FindByObject(fence);
	ASSERT_NE(fenceFire, nullptr);
	EXPECT_FLOAT_EQ(fenceFire->temperature,
	                HeatedByABurningTree(HeatedByABurningTree(k_Air, k_FenceCapacity), k_FenceCapacity));
}

TEST_F(FireWorld, AFireNeverHeatsWhatHeatedIt)
{
	const auto burning = TreeAt(200.5f, 200.5f);
	const auto heater = TreeAt(201.5f, 200.5f);
	const auto other = TreeAt(200.5f, 201.5f);
	ASSERT_NE(Heat(burning, k_Burning, heater), nullptr);
	Turn();
	EXPECT_EQ(fires->FindByObject(heater), nullptr);
	EXPECT_FLOAT_EQ(fires->GetTemperature(heater), k_Air);
	const auto* heated = fires->FindByObject(other);
	ASSERT_NE(heated, nullptr);
	EXPECT_FLOAT_EQ(heated->temperature, HeatedByABurningTree(k_Air, k_TreeCapacity));
}

TEST(FireGroups, FiresThatTouchJoinOneBlazeRightBehindTheOneThatLitIt)
{
	std::array<fire::FireEffect, 5> f;
	auto& [a, b, c, d, e] = f;
	const auto villager = [](uint32_t n) { return static_cast<entt::entity>(n); };
	fire::AddToFireGroup(a, b);
	EXPECT_EQ(a.next, &b);
	EXPECT_EQ(b.root, &a);
	EXPECT_EQ(b.next, nullptr);
	// the next one goes right behind the one that lit it, before the older ones
	fire::AddToFireGroup(a, c);
	EXPECT_EQ(a.next, &c);
	EXPECT_EQ(c.next, &b);
	EXPECT_EQ(c.root, &a);
	// joined twice, nothing changes
	fire::AddToFireGroup(a, b);
	EXPECT_EQ(a.next, &c);
	EXPECT_EQ(c.next, &b);
	// a member of another blaze brings its whole blaze, head first; the head's firemen go to the end of the new head's
	a.firemen = {villager(1)};
	fire::AddToFireGroup(d, e);
	d.firemen = {villager(2), villager(3)};
	fire::AddToFireGroup(a, e);
	EXPECT_EQ(a.next, &d);
	EXPECT_EQ(d.next, &e);
	EXPECT_EQ(e.next, &c);
	EXPECT_EQ(b.next, nullptr);
	for (const auto* member : {&a, &b, &c, &d, &e})
	{
		EXPECT_EQ(member->root, &a);
	}
	EXPECT_EQ(a.firemen, (std::vector {villager(1), villager(2), villager(3)}));
	EXPECT_TRUE(d.firemen.empty());
}

TEST_F(FireWorld, AColdHeadHandsTheBlazeToItsFirstHotMember)
{
	// The head has cooled below the reaction temperature; a member far from it still burns
	auto* head = Heat(TreeAt(50.5f, 50.5f), 30.0f);
	auto* member = Heat(TreeAt(450.5f, 450.5f), 200.0f);
	ASSERT_NE(head, nullptr);
	ASSERT_NE(member, nullptr);
	fire::AddToFireGroup(*head, *member);
	const auto first = Reg().Create();
	const auto second = Reg().Create();
	head->firemen = {first};
	member->firemen = {second};
	Turn();
	EXPECT_EQ(member->root, member);
	EXPECT_EQ(member->next, head);
	EXPECT_EQ(head->root, member);
	EXPECT_EQ(head->next, nullptr);
	// the new head's own firemen first, the old head's after them
	EXPECT_EQ(member->firemen, (std::vector {second, first}));
	EXPECT_TRUE(head->firemen.empty());
}

TEST_F(FireWorld, ABurningTreeSetsTheNextOneAlightAndABushSooner)
{
	constexpr int k_Turns = 1000;
	const auto expectedTree = CatchingNeighbour(TreeMaterial(), k_Turns);
	const auto expectedBush = CatchingNeighbour(TreeMaterial(k_BushCapacity), k_Turns);
	ASSERT_TRUE(model::IsOnFire(expectedTree.back(), TreeMaterial()));
	ASSERT_TRUE(model::IsOnFire(expectedBush.back(), TreeMaterial(k_BushCapacity)));
	// a bush holds a tenth of a tree's heat, so it catches much sooner
	EXPECT_LT(expectedBush.size() * 5, expectedTree.size());

	const auto treeFire = TreeAt(150.5f, 150.5f);
	const auto tree = TreeAt(151.5f, 150.5f);
	const auto bushFire = TreeAt(450.5f, 150.5f);
	const auto bush = TreeAt(451.5f, 150.5f, openblack::TreeInfo::Bush);
	ASSERT_NE(Heat(treeFire, k_Burning), nullptr);
	ASSERT_NE(Heat(bushFire, k_Burning), nullptr);
	for (size_t turn = 0; turn < expectedTree.size(); ++turn)
	{
		Turn();
		const auto* treeHeat = fires->FindByObject(tree);
		ASSERT_NE(treeHeat, nullptr) << "turn " << turn + 1;
		EXPECT_FLOAT_EQ(treeHeat->temperature, expectedTree.at(turn)) << "turn " << turn + 1;
		EXPECT_EQ(fires->IsOnFire(tree), turn + 1 == expectedTree.size()) << "turn " << turn + 1;
		if (turn < expectedBush.size())
		{
			const auto* bushHeat = fires->FindByObject(bush);
			ASSERT_NE(bushHeat, nullptr) << "turn " << turn + 1;
			EXPECT_FLOAT_EQ(bushHeat->temperature, expectedBush.at(turn)) << "turn " << turn + 1;
			EXPECT_EQ(fires->IsOnFire(bush), turn + 1 == expectedBush.size()) << "turn " << turn + 1;
		}
	}
}

TEST_F(FireWorld, ABurnLightsWhatHasNoFireAndACoolingNever)
{
	const auto tree = TreeAt(250.5f, 250.5f);
	const auto wet = TreeAt(350.5f, 250.5f);
	const auto caster = Reg().Create();
	effects::EffectValues values;
	auto& burn = values.numbers.at(static_cast<size_t>(effects::EffectValues::Number::Burn));
	values.player = PlayerNames::PLAYER_TWO;
	values.appliedBy = caster;
	// a lightning's burn: a fire with the effect's player and what applied it, pulled towards the air plus the burn
	burn = 10000.0f;
	fire::ApplyEffectToFireEffectIfNecessary(tree, values);
	const auto* lit = fires->FindByObject(tree);
	ASSERT_NE(lit, nullptr);
	EXPECT_EQ(lit->player.value_or(PlayerNames::NEUTRAL), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(lit->source, caster);
	const float hot = model::ApplyBurn(k_Air, TreeMaterial(), 10000.0f);
	EXPECT_FLOAT_EQ(lit->temperature, hot);
	// the water's -4000 cools a fire, and makes none where there is none
	burn = -4000.0f;
	fire::ApplyEffectToFireEffectIfNecessary(tree, values);
	EXPECT_FLOAT_EQ(lit->temperature, model::ApplyBurn(hot, TreeMaterial(), -4000.0f));
	fire::ApplyEffectToFireEffectIfNecessary(wet, values);
	EXPECT_EQ(fires->FindByObject(wet), nullptr);
	// no burn at all does nothing
	burn = 0.0f;
	fire::ApplyEffectToFireEffectIfNecessary(wet, values);
	EXPECT_EQ(fires->FindByObject(wet), nullptr);
}

// A thing in the hand is off the map: each turn the hand's turn has it catch from the fires in the map cell of its fire
// centre, from none in the cells round it (docs/wiki/world/fire.md#spreading)
TEST_F(FireWorld, AThingHeldOverAFireCatchesFromTheFiresInItsCellAlone)
{
	const auto burning = TreeAt(250.5f, 250.5f);
	ASSERT_NE(Heat(burning, k_Burning), nullptr);
	const auto held = Place(250.2f, 250.8f);
	Reg().Assign<components::Tree>(held, openblack::TreeInfo::Beech, 1.0f);
	Reg().Assign<components::Mesh>(held, k_TreeMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	const auto beside = Place(261.5f, 250.5f);
	Reg().Assign<components::Tree>(beside, openblack::TreeInfo::Beech, 1.0f);
	Reg().Assign<components::Mesh>(beside, k_TreeMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	fires->HeatHeldObject(held);
	fires->HeatHeldObject(beside);
	const auto* caught = fires->FindByObject(held);
	ASSERT_NE(caught, nullptr);
	EXPECT_FLOAT_EQ(caught->temperature, HeatedByABurningTree(k_Air, k_TreeCapacity));
	EXPECT_EQ(fires->FindByObject(beside), nullptr);
}

// A burning tree that falls burns on as its dead tree: the dead tree is the same entity, so its fire, heat and blaze
// stay with it and there is nothing to move (docs/wiki/world/fire.md#passing-a-fire-to-another-object)
TEST_F(FireWorld, ABurningTreeThatFallsBurnsOnAsItsDeadTree)
{
	const auto tree = TreeAt(150.5f, 150.5f);
	auto* fire = Heat(tree, k_Burning);
	ASSERT_NE(fire, nullptr);
	openblack::ecs::TreeBecomesDeadTree(tree, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(Reg().AllOf<components::DeadTree>(tree));
	EXPECT_EQ(fires->FindByObject(tree), fire);
	EXPECT_EQ(fire->object, tree);
	EXPECT_FLOAT_EQ(fire->temperature, k_Burning);
}

TEST_F(FireWorld, AFireThatStartsMovingLeavesItsBlazeAndItsReaction)
{
	const auto standing = TreeAt(400.5f, 400.5f);
	const auto picked = TreeAt(401.5f, 400.5f);
	auto* head = Heat(standing, k_Burning);
	auto* moving = Heat(picked, k_Burning);
	ASSERT_NE(head, nullptr);
	ASSERT_NE(moving, nullptr);
	fire::AddToFireGroup(*head, *moving);
	moving->reaction = reactions::CreateReaction(picked, openblack::Reaction::ReactToFire, std::nullopt, true);
	ASSERT_NE(moving->reaction, 0u);
	// thrown: out of the blaze, without its reaction to fire, and still burning
	fires->StartedMoving(picked, false);
	EXPECT_EQ(moving->root, moving);
	EXPECT_EQ(moving->next, nullptr);
	EXPECT_EQ(head->next, nullptr);
	EXPECT_EQ(moving->reaction, 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(picked, openblack::Reaction::ReactToFire), 0u);
	EXPECT_EQ(fires->FindByObject(picked), moving);
	EXPECT_FLOAT_EQ(moving->temperature, k_Burning);
	// picked up: the living react to a burning thing in the hand instead
	fires->StartedMoving(picked, true);
	EXPECT_NE(moving->reaction, 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(picked, openblack::Reaction::ReactToBurningObjectInHand),
	          moving->reaction);
}

TEST_F(FireWorld, AScriptPutsOutAFireAtTheAirsTemperatureAndSetsWhatFireDoes)
{
	const auto tree = TreeAt(250.5f, 450.5f);
	auto* fire = Heat(tree, k_Burning);
	ASSERT_NE(fire, nullptr);
	fires->PutOut(tree);
	EXPECT_FLOAT_EQ(fire->temperature, k_Air);
	// an existing fire takes a lower temperature too
	fires->SetTemperature(tree, 20.0f, entt::null);
	EXPECT_FLOAT_EQ(fire->temperature, 20.0f);
	// set alight at a speed: its combustion temperature and that much more of twice it
	fires->SetOnFire(tree, 0.5f);
	EXPECT_FLOAT_EQ(fire->temperature, model::SetOnFireTemperature(TreeMaterial(), 0.5f));
	// a script's "not hurt" and "not set alight" are the opposite of what it asks for
	fires->SetHurtByFire(tree, false);
	EXPECT_TRUE(fire::traits::NotHurtByFire(tree));
	fires->SetHurtByFire(tree, true);
	EXPECT_FALSE(fire::traits::NotHurtByFire(tree));
	fires->SetCanBeSetOnFire(tree, false);
	EXPECT_TRUE(fire::traits::CannotBeSetOnFire(tree));
	fires->SetCanBeSetOnFire(tree, true);
	EXPECT_FALSE(fire::traits::CannotBeSetOnFire(tree));
}

TEST_F(FireWorld, RainOnAFireGivesTheStormThatRainedOneWaterReaction)
{
	// Two storm spells of 40 m, the second one cast last (the list's head)
	const auto storm = [](float x, PlayerNames player) {
		const auto spell = Reg().Create();
		auto& component = Reg().Assign<components::Spell>(spell);
		component.position = glm::vec3(x, 0.0f, 700.0f);
		component.magnitude = 40.0f;
		component.player = player;
		component.hasPlayer = true;
		Reg().Assign<openblack::magic::SpellStormData>(spell);
		auto& spells = Locator::magicSystem::value().SpellStore().StormSpells();
		spells.insert(spells.begin(), spell);
		return spell;
	};
	const auto west = storm(500.0f, PlayerNames::PLAYER_TWO);
	const auto east = storm(600.0f, PlayerNames::PLAYER_ONE);
	const auto waterReaction = [](entt::entity spell) {
		return Reg().Get<const openblack::magic::SpellStormData>(spell).waterReaction;
	};
	// 30 m from the west one and 70 m from the east one: the west one's
	openblack::magic::spell_storm::ReactToRainOnFire(glm::vec3(530.0f, 0.0f, 700.0f));
	const auto reaction = waterReaction(west);
	ASSERT_NE(reaction, 0u);
	EXPECT_EQ(waterReaction(east), 0u);
	const auto* made = reactions::Find(reaction);
	ASSERT_NE(made, nullptr);
	EXPECT_EQ(made->type, openblack::Reaction::ReactToMagicWaterPuttingOutFire);
	EXPECT_EQ(made->initiator, west);
	EXPECT_EQ(made->player.value_or(PlayerNames::NEUTRAL), PlayerNames::PLAYER_TWO);
	// once: more rain there makes no second one
	openblack::magic::spell_storm::ReactToRainOnFire(glm::vec3(530.0f, 0.0f, 700.0f));
	EXPECT_EQ(waterReaction(west), reaction);
	EXPECT_EQ(waterReaction(east), 0u);
	// 50 m from the east one is outside its 40 m
	openblack::magic::spell_storm::ReactToRainOnFire(glm::vec3(550.0f, 0.0f, 700.0f));
	EXPECT_EQ(waterReaction(east), 0u);
	// within it, the east one has its own
	openblack::magic::spell_storm::ReactToRainOnFire(glm::vec3(590.0f, 0.0f, 700.0f));
	ASSERT_NE(waterReaction(east), 0u);
	EXPECT_EQ(reactions::Find(waterReaction(east))->player.value_or(PlayerNames::NEUTRAL), PlayerNames::PLAYER_ONE);
}

namespace
{
/// The script's fire natives as a script calls them, over fake fires that keep what they were asked
class FireNatives: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<Registry>();
		// entity 0 is "no thing" for a script id
		registry.Create();
		object = registry.Create();
		registry.Assign<components::Transform>(object, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Locator::vm::emplace<openblack::lhvm::LHVM>();
		fires = &static_cast<openblack::test::FakeFires&>(Locator::fireSystem::emplace<openblack::test::FakeFires>());
	}

	static openblack::lhvm::LHVM& Vm() { return Locator::vm::value(); }
	[[nodiscard]] uint32_t Id() const { return entt::to_integral(object); }

	openblack::test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	openblack::test::RestoreService<Locator::vm> _restoreVm;
	openblack::test::RestoreService<Locator::fireSystem> _restoreFires;
	openblack::test::FakeFires* fires {nullptr};
	entt::entity object {entt::null};
};
} // namespace

TEST_F(FireNatives, TheNativesPopTheirArgumentsInTheOriginalsOrder)
{
	namespace script = openblack::magic::script;
	// SET_ON_FIRE takes the switch, the object and the speed: on sets it alight at the speed, off puts it out
	Vm().Pushb(true);
	Vm().Pusho(Id());
	Vm().Pushf(0.5f);
	script::SetOnFire();
	ASSERT_EQ(fires->setOnFire.size(), 1u);
	EXPECT_EQ(fires->setOnFire.front().first, object);
	EXPECT_FLOAT_EQ(fires->setOnFire.front().second, 0.5f);
	Vm().Pushb(false);
	Vm().Pusho(Id());
	Vm().Pushf(0.5f);
	script::SetOnFire();
	EXPECT_EQ(fires->putOut, (std::vector {object}));
	EXPECT_EQ(fires->setOnFire.size(), 1u);

	// SET_TEMPERATURE takes the object and the temperature, with no source
	Vm().Pusho(Id());
	Vm().Pushf(300.0f);
	script::SetTemperature();
	ASSERT_EQ(fires->temperaturesSet.size(), 1u);
	EXPECT_EQ(fires->temperaturesSet.front().first, object);
	EXPECT_FLOAT_EQ(fires->temperaturesSet.front().second, 300.0f);
	EXPECT_TRUE(fires->temperatureSources.front() == entt::null);

	// IS_FIRE_NEAR takes the point's x, y and z, then the radius
	fires->fireNear = true;
	Vm().Pushf(1.0f);
	Vm().Pushf(2.0f);
	Vm().Pushf(3.0f);
	Vm().Pushf(4.0f);
	script::IsFireNear();
	EXPECT_EQ(Vm().Pop().intVal, 1);
	ASSERT_EQ(fires->fireNearAsked.size(), 1u);
	EXPECT_EQ(fires->fireNearAsked.front().first, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(fires->fireNearAsked.front().second, 4.0f);

	// SET_HURT_BY_FIRE and SET_SET_ON_FIRE take the switch, then the object
	Vm().Pushb(false);
	Vm().Pusho(Id());
	script::SetHurtByFire();
	ASSERT_EQ(fires->hurtByFire.size(), 1u);
	EXPECT_EQ(fires->hurtByFire.front().first, object);
	EXPECT_FALSE(fires->hurtByFire.front().second);
	Vm().Pushb(true);
	Vm().Pusho(Id());
	script::SetSetOnFire();
	ASSERT_EQ(fires->canBeSetOnFire.size(), 1u);
	EXPECT_EQ(fires->canBeSetOnFire.front().first, object);
	EXPECT_TRUE(fires->canBeSetOnFire.front().second);

	// IS_ON_FIRE: no thing is never on fire; an object answers as its fire does
	Vm().Pusho(0);
	script::IsOnFire();
	EXPECT_EQ(Vm().Pop().intVal, 0);
	fires->Burn(object, 200.0f);
	Vm().Pusho(Id());
	script::IsOnFire();
	EXPECT_EQ(Vm().Pop().intVal, 1);
}
