/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pots and piles of a saved game: a loaded pile takes back what its record keeps (docs/wiki/engine/saved-games.md,
// what a load does)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include <SavedRecords.h>
#include <TownRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "Common/Zoomer.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/MapFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"

using namespace openblack;
namespace components = openblack::ecs::components;
namespace pot_flags = openblack::gamesave::pot_flags;

namespace
{
class PileLoad: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::objectCreationIndexSystem::emplace<ecs::systems::ObjectCreationIndexSystem>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		// the store's food pile is a food pile of food
		auto info = std::make_unique<InfoConstants>();
		auto& row = info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile));
		row.type = ObjectType::Pot;
		row.potType = PotType::PileFood;
		row.resourceType = ResourceType::Food;
		row.maxAmountInPot = 1000;
		// the magic wood is a wood pile of wood, whose size its amount does not set as a pot's
		auto& wood = info->pot.at(static_cast<size_t>(PotInfo::MagicWood));
		wood.type = ObjectType::Pot;
		wood.potType = PotType::PileWood;
		wood.resourceType = ResourceType::Wood;
		wood.maxAmountInPot = 1000;
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		// the services the entities reach as they go, after the registry
		Locator::entitiesRegistry::reset();
		Locator::mapCellsSystem::reset();
		Locator::objectCreationIndexSystem::reset();
		test::ResetMapAndVillagerDefaults();
	}

	ecs::Registry& Reg() { return *_registry; }

private:
	test::RestoreService<Locator::infoConstants> _restoreInfo;
	ecs::Registry* _registry {nullptr};
};
} // namespace

// A loaded pile takes back its poison, its speed-up and whether its reaction was started, and what only a saved game
// reads: the mark of a building site's pile and of one made to start its reaction
TEST_F(PileLoad, ALoadedPileTakesBackWhatItsRecordKeeps)
{
	gamesave::FoodPileRecord record;
	record.pile.resourceAmount = 400;
	record.pile.flags =
	    pot_flags::k_Poisoned | pot_flags::k_ReactionStarted | pot_flags::k_FromBuildingSite | pot_flags::k_SpeedUp;
	record.pile.buildingByte = 1;
	const auto pile = ecs::saved_world_load::MakePileEntity(
	    {.number = 3, .type = PotInfo::StoragePitFoodPile, .base = record.base, .pile = record.pile, .record = record});
	ASSERT_NE(pile, entt::entity {entt::null});

	const auto& pot = Reg().Get<const components::Pot>(pile);
	EXPECT_EQ(pot.amount, 400u);
	EXPECT_TRUE(pot.poisoned);
	EXPECT_TRUE(pot.speedUp);
	EXPECT_TRUE(pot.reactionStarted);
	EXPECT_TRUE(pot.fromBuildingSite);
	EXPECT_TRUE(pot.reactsWhenMade);

	// none of them: a pile as plain as it was made
	gamesave::FoodPileRecord plain;
	plain.pile.resourceAmount = 10;
	const auto other = ecs::saved_world_load::MakePileEntity(
	    {.number = 4, .type = PotInfo::StoragePitFoodPile, .base = plain.base, .pile = plain.pile, .record = plain});
	ASSERT_NE(other, entt::entity {entt::null});
	const auto& made = Reg().Get<const components::Pot>(other);
	EXPECT_FALSE(made.poisoned || made.speedUp || made.reactionStarted || made.fromBuildingSite || made.reactsWhenMade);
}

// A loaded pile starts buried again at the depth of its height and rises to the height of its amount over 1 s, as a new
// pile does, whatever sink it was saved with (here half risen, still rising): the saved sink is not used
TEST_F(PileLoad, ALoadedPileRisesAgainFromBuried)
{
	const test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	// every model 4 high at size 1
	boxes.meshBox = [](entt::id_type) {
		return std::optional<AxisAlignedBoundingBox> {
		    AxisAlignedBoundingBox {.minima = {-1.0f, 0.0f, -1.0f}, .maxima = {1.0f, 4.0f, 1.0f}}};
	};
	gamesave::FoodPileRecord record;
	record.pile.resourceAmount = 400;
	record.base.object.scale = 1.0f;
	Zoomer saved;
	saved.SetPosition(-3.0f);
	saved.SetDestinationWithSpeedAndTime(-1.0f, 0.0f, 1.0f);
	saved.Update(0.25f);
	record.pile.altitudeZoomer = ecs::saved_zoomer::BytesOf(saved);
	const auto pile = ecs::saved_world_load::MakePileEntity(
	    {.number = 3, .type = PotInfo::StoragePitFoodPile, .base = record.base, .pile = record.pile, .record = record});
	ASSERT_NE(pile, entt::entity {entt::null});

	const float height = ecs::object::GetHeight(pile);
	ASSERT_EQ(height, 4.0f);
	const float target = (ecs::object::GetProportionRaised(pile) - 1.0f) * height;
	const auto& sink = Reg().Get<const components::PileSink>(pile);
	EXPECT_NE(ecs::saved_zoomer::BytesOf(sink.offset), record.pile.altitudeZoomer);
	EXPECT_EQ(sink.offset.value, -height);
	EXPECT_EQ(sink.offset.startValue, -height);
	EXPECT_EQ(sink.offset.startSpeed, 0.0f);
	EXPECT_EQ(sink.offset.destination, target);
	EXPECT_EQ(sink.offset.duration, 1.0f);
	EXPECT_EQ(sink.offset.time, 0.0f);
	EXPECT_EQ(sink.height, height);
	EXPECT_EQ(Reg().Get<const components::Transform>(pile).position.y, sink.baseY - height);

	// the same as a pile the game makes there at that amount
	const auto made = ecs::archetypes::PotArchetype::Create(glm::vec3(0.0f), 0.0f, PotInfo::StoragePitFoodPile, 400, true);
	ASSERT_NE(made, entt::entity {entt::null});
	EXPECT_EQ(ecs::saved_zoomer::BytesOf(Reg().Get<const components::PileSink>(made).offset),
	          ecs::saved_zoomer::BytesOf(sink.offset));
}

// A loaded magic wood is a wood pile of its row with its amount, and the player its record names
TEST_F(PileLoad, ALoadedMagicWoodTakesBackItsPlayer)
{
	gamesave::MagicWoodRecord record;
	record.pile.resourceAmount = 40;
	const auto pile = ecs::saved_world_load::MakePileEntity({.number = 5,
	                                                         .type = PotInfo::MagicWood,
	                                                         .base = record.base,
	                                                         .pile = record.pile,
	                                                         .record = record,
	                                                         .player = PlayerNames::PLAYER_TWO});
	ASSERT_NE(pile, entt::entity {entt::null});
	const auto& pot = Reg().Get<const components::Pot>(pile);
	EXPECT_EQ(pot.amount, 40u);
	EXPECT_TRUE(pot.owner == PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(Reg().AllOf<components::LoadedRecord<gamesave::MagicWoodRecord>>(pile));
}

// A loaded food or wood pile answers with the player its record names, as the load's resolve sets it, and the next
// save writes it; one that names none answers with the neutral player; one part of a structure (a worship site's food
// pot here) answers with its structure's player at the load, whatever it was saved with; a magic wood with its own
TEST_F(PileLoad, ALoadedPileAnswersWithThePlayerTheLoadsResolveGivesIt)
{
	namespace load = ecs::saved_world_load;
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	constexpr gamesave::ObjectNumber k_Two = 1;
	constexpr gamesave::ObjectNumber k_Named = 2;
	constexpr gamesave::ObjectNumber k_None = 3;
	constexpr gamesave::ObjectNumber k_SitePot = 4;
	constexpr gamesave::ObjectNumber k_Wood = 5;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::PlayerRecord {.playerNumber = static_cast<uint8_t>(PlayerNames::PLAYER_TWO)});
	const auto food = [](gamesave::ObjectNumber player) {
		gamesave::FoodPileRecord record;
		record.base.object.info = load::PotInfoRowOf(PotInfo::StoragePitFoodPile);
		record.base.object.scale = 1.0f;
		record.pile.resourceAmount = 10;
		record.pile.player.number = player;
		return record;
	};
	world.objects.emplace_back(food(k_Two));
	world.objects.emplace_back(food(0));
	world.objects.emplace_back(food(k_Two));
	gamesave::MagicWoodRecord wood;
	wood.base.object.info = load::PotInfoRowOf(PotInfo::MagicWood);
	wood.base.object.scale = 1.0f;
	wood.pile.resourceAmount = 10;
	wood.player.number = k_Two;
	world.objects.emplace_back(wood);
	world.loadOrder = {k_Two, k_Named, k_None, k_SitePot, k_Wood};
	std::map<gamesave::ObjectNumber, entt::entity> made;
	const load::Makers makers {.pile = [&made, this](const load::PlannedPile& pile) {
		const auto entity = load::MakePileEntity(pile);
		made[pile.number] = entity;
		if (pile.number == k_SitePot)
		{
			auto& site = Reg().Assign<components::WorshipSite>(Reg().Create());
			site.player = PlayerNames::PLAYER_THREE;
			site.foodPot = entity;
		}
		return entity;
	}};
	std::ignore = load::Apply(load::PlanOf(world), makers);
	ASSERT_EQ(made.size(), 4u);

	const auto& named = Reg().Get<const components::Pot>(made.at(k_Named));
	EXPECT_TRUE(named.playerWhenMade == PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(named.owner == PlayerNames::PLAYER_TWO);
	const auto& none = Reg().Get<const components::Pot>(made.at(k_None));
	EXPECT_TRUE(none.playerWhenMade == PlayerNames::NEUTRAL);
	EXPECT_TRUE(none.owner == PlayerNames::NEUTRAL);
	const auto& sitePot = Reg().Get<const components::Pot>(made.at(k_SitePot));
	EXPECT_TRUE(sitePot.playerWhenMade == PlayerNames::PLAYER_THREE);
	EXPECT_TRUE(sitePot.owner == PlayerNames::NEUTRAL);
	const auto& magic = Reg().Get<const components::Pot>(made.at(k_Wood));
	EXPECT_TRUE(magic.owner == PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(magic.playerWhenMade == PlayerNames::PLAYER_TWO);
}

// A pile in play saved made to start its reaction with none started starts it again once the load has made the
// reactions the save kept, as the game's load does as it resolves the pile, in the order their reads ended; one whose
// reaction was started, one not made to start one and one out of play do not
TEST_F(PileLoad, ALoadedPileMadeToReactWithNoReactionStartsItAgain)
{
	namespace load = ecs::saved_world_load;
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	const auto food = [](uint8_t buildingByte, uint8_t flags, uint8_t thingFlags) {
		gamesave::FoodPileRecord record;
		record.thing.flags = thingFlags;
		record.base.object.info = load::PotInfoRowOf(PotInfo::StoragePitFoodPile);
		record.base.object.scale = 1.0f;
		record.pile.resourceAmount = 10;
		record.pile.buildingByte = buildingByte;
		record.pile.flags = flags;
		return record;
	};
	gamesave::WorldRead world;
	world.objects.emplace_back(food(1, 0, 0));                            // 1: starts it again
	world.objects.emplace_back(food(1, pot_flags::k_ReactionStarted, 0)); // 2: its reaction started
	world.objects.emplace_back(food(0, 0, 0));                            // 3: not made to start one
	world.objects.emplace_back(food(1, 0, 1));                            // 4: out of play
	gamesave::MagicWoodRecord wood;
	wood.base.object.info = load::PotInfoRowOf(PotInfo::MagicWood);
	wood.base.object.scale = 1.0f;
	wood.pile.resourceAmount = 10;
	wood.pile.buildingByte = 1;
	world.objects.emplace_back(wood); // 5: starts it again, read before the first
	world.loadOrder = {5, 2, 3, 4, 1};
	const auto plan = load::PlanOf(world);
	EXPECT_EQ(plan.pileReactions, (std::vector<gamesave::ObjectNumber> {5, 1}));

	std::map<gamesave::ObjectNumber, entt::entity> made;
	std::vector<entt::entity> started;
	const load::Makers makers {.pile =
	                               [&made](const load::PlannedPile& pile) {
		                               const auto entity = load::MakePileEntity(pile);
		                               made[pile.number] = entity;
		                               return entity;
	                               },
	                           .pileReaction = [&started](entt::entity pile) { started.push_back(pile); }};
	std::ignore = load::Apply(plan, makers);
	ASSERT_TRUE(made.contains(1) && made.contains(5));
	EXPECT_EQ(started, (std::vector {made.at(5), made.at(1)}));
	// the game's load starts it as the game starts a pile's reaction
	EXPECT_TRUE(static_cast<bool>(load::GameMakers().pileReaction));
}

// A loaded pile takes back its size as saved, not the size its row is made at (a magic wood's 0.7), as the object's
// load reads it, so a save writes it again; a pot's size still follows its amount
TEST_F(PileLoad, ALoadedPileTakesBackItsSavedSize)
{
	gamesave::MagicWoodRecord wood;
	wood.pile.resourceAmount = 40;
	wood.base.object.scale = 1.25f;
	wood.base.object.heading = 0.5f;
	const auto pile = ecs::saved_world_load::MakePileEntity(
	    {.number = 5, .type = PotInfo::MagicWood, .base = wood.base, .pile = wood.pile, .record = wood});
	ASSERT_NE(pile, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const components::Transform>(pile).scale, glm::vec3(1.25f));

	gamesave::FoodPileRecord food;
	food.pile.resourceAmount = 30;
	food.base.object.scale = 0.875f;
	const auto foodPile = ecs::saved_world_load::MakePileEntity(
	    {.number = 6, .type = PotInfo::StoragePitFoodPile, .base = food.base, .pile = food.pile, .record = food});
	ASSERT_NE(foodPile, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const components::Transform>(foodPile).scale, glm::vec3(0.875f));
}

namespace
{
/// The spot visuals started, each a new entity
class SparkleParticles: public test::InertParticleSystem
{
public:
	entt::entity StartSpotVisual(SpotVisualType type, glm::vec3 /*position*/, std::optional<int> turns, entt::entity owner,
	                             float /*magnitude*/) override
	{
		started.push_back({type, turns, owner});
		return Locator::entitiesRegistry::value().Create();
	}
	struct Started
	{
		SpotVisualType type;
		std::optional<int> turns;
		entt::entity owner;
	};
	std::vector<Started> started;
};
} // namespace

// A food pile saved sped up keeps its speed-up and starts no sparkle of its own: its sparkle is the container the save
// kept, which the load makes with the game's containers and gives the pile; one whose record names no sparkle is the same
TEST_F(PileLoad, ALoadedSpedUpFoodPileKeepsItsSpeedUpAndStartsNoSparkle)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = static_cast<SparkleParticles&>(Locator::particleSystem::emplace<SparkleParticles>());
	gamesave::FoodPileRecord record;
	record.pile.resourceAmount = 50;
	record.pile.flags = pot_flags::k_SpeedUp;
	record.store.number = 12;
	const auto pile = ecs::saved_world_load::MakePileEntity(
	    {.number = 3, .type = PotInfo::StoragePitFoodPile, .base = record.base, .pile = record.pile, .record = record});
	ASSERT_NE(pile, entt::entity {entt::null});
	EXPECT_TRUE(particles.started.empty());
	const auto& pot = Reg().Get<const components::Pot>(pile);
	EXPECT_TRUE(pot.speedUp);
	EXPECT_EQ(pot.speedUpVisual, entt::entity {entt::null});

	record.store.number = 0;
	const auto bare = ecs::saved_world_load::MakePileEntity(
	    {.number = 4, .type = PotInfo::StoragePitFoodPile, .base = record.base, .pile = record.pile, .record = record});
	EXPECT_TRUE(particles.started.empty());
	EXPECT_TRUE(Reg().Get<const components::Pot>(bare).speedUp);
	EXPECT_EQ(Reg().Get<const components::Pot>(bare).speedUpVisual, entt::entity {entt::null});
}

// A zoomer's 48 bytes are the game's: each value at its place, the second time 0, and back to the same zoomer
TEST(SavedZoomer, AZoomersBytesAreTheGamesAndComeBackTheSame)
{
	Zoomer zoomer;
	zoomer.SetPosition(2.0f);
	zoomer.SetDestinationWithSpeedAndTime(5.0f, 1.0f, 2.0f);
	zoomer.Update(0.5f);
	const auto bytes = ecs::saved_zoomer::BytesOf(zoomer);
	const auto at = [&bytes](size_t offset) {
		float value = 0.0f;
		std::memcpy(&value, bytes.data() + offset, sizeof(value));
		return value;
	};
	EXPECT_EQ(at(0x00), zoomer.value);
	EXPECT_EQ(at(0x04), 5.0f);
	EXPECT_EQ(at(0x08), 1.0f);
	EXPECT_EQ(at(0x0C), zoomer.speed);
	EXPECT_EQ(at(0x10), 0.0f);
	EXPECT_EQ(at(0x14), 0.5f);
	EXPECT_EQ(at(0x18), 2.0f);
	EXPECT_EQ(at(0x1C), 2.0f);
	EXPECT_EQ(at(0x2C), zoomer.c4);
	EXPECT_EQ(ecs::saved_zoomer::BytesOf(ecs::saved_zoomer::ZoomerOf(bytes)), bytes);
}
