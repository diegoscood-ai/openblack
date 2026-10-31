/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// DeleteObjectAndTakeResource of the storage pit and the worship site: the return values, the
// storage pit's REACTION 22 and the object deleted. No game data: no entities map, so
// CreateReaction does not spread. Then the deed a storage pit reports to its creature after taking a
// resource, with the random draw of food that belonged to another player, and the owner the hand records at a pick-up
// (a pile's player, a fish farm's town's owner or none), and the interface a put-down gives the pit. And the help
// profile's Supply count when what the store takes was thrown by the hand. Then what each kind of object answers when
// asked for a resource (its own resource by its kind, a worship site's food pot, a building site's pile) and what it
// gives when one is taken from it. Then what is put down where nothing takes it, and a land script's pot. And
// GET_RESOURCE as a script calls it.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Common/GameRandomTesting.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/FishFarms.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/DynamicsState.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/TakeResource.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownBelief.h"
#include "Enums.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "InfoConstants.h"
#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"
#include "support/LandFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
namespace reactions = openblack::ecs::effects::reactions;

namespace
{
class TakeResourceTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		reactions::Clear();
		game_clock::SetTurn(1234);
	}
	void TearDown() override
	{
		reactions::Clear();
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(TakeResourceTest, StoragePitReactsWithThePlayerOfTheInterface)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_TWO, true};
	// returns true
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, hand));
	// CreateReaction(the pit, 0x16, the interface's player, stamped)
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	const auto* reaction = reactions::Find(id);
	ASSERT_NE(reaction, nullptr);
	EXPECT_EQ(static_cast<int>(reaction->type), 0x16);
	EXPECT_EQ(reaction->player, std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(reaction->turnCreated, 1234u); // the last argument 1: stamped
	EXPECT_EQ(reactions::All().size(), 1u);
	// the object goes
	EXPECT_FALSE(Reg().Valid(object));
}

TEST_F(TakeResourceTest, StoragePitWithoutInterfaceHasNoPlayer)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	// no interface -> the reaction has no player (the original's null, not the neutral player)
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, {}));
	const auto id = reactions::GetReactionOfTypeInitiatedBy(store, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(reactions::Find(id)->player, std::nullopt);
}

TEST_F(TakeResourceTest, WorshipSiteTakesWithoutReaction)
{
	const auto site = Reg().Create();
	const auto object = Reg().Create();
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	// returns true, and no CreateReaction
	EXPECT_TRUE(worship::site::DeleteObjectAndTakeResource(site, object, hand));
	EXPECT_TRUE(reactions::All().empty());
	EXPECT_FALSE(Reg().Valid(object));
}

namespace
{
/// The object listed in the physics, flying or resting, thrown by a player or not
void InPhysics(entt::entity object, bool flying, std::optional<PlayerNames> throwerPlayer)
{
	auto entry = std::make_unique<ecs::PhysicsEntry>();
	entry->entity = object;
	entry->body.resting = !flying;
	entry->throwerPlayer = throwerPlayer;
	Locator::dynamicsSystem::value().GetState().objects.push_back(std::move(entry));
}

uint32_t Supplies()
{
	return help_profile::Get(help_profile::Event::Supply).Total();
}
} // namespace

TEST_F(TakeResourceTest, AnObjectTheHandThrewIntoAStoreCountsOneSupply)
{
	const auto store = Reg().Create();
	const auto object = Reg().Create();
	InPhysics(object, true, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, object, {}));
	EXPECT_EQ(Supplies(), 1u);
}

TEST_F(TakeResourceTest, AnObjectNotFlyingOrNotThrownByTheHandCountsNoSupply)
{
	const auto store = Reg().Create();
	// not in the physics: put down by the hand, or dropped
	const auto placed = Reg().Create();
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, placed, {true, PlayerNames::PLAYER_ONE, true}));
	// resting in the physics
	const auto resting = Reg().Create();
	InPhysics(resting, false, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, resting, {}));
	// flying, thrown by something else
	const auto other = Reg().Create();
	InPhysics(other, true, std::nullopt);
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, other, {}));
	// flying, thrown by another player: not the local interface
	const auto rivals = Reg().Create();
	InPhysics(rivals, true, PlayerNames::PLAYER_TWO);
	EXPECT_TRUE(ecs::take_resource::StoragePit(store, rivals, {}));
	EXPECT_EQ(Supplies(), 0u);
}

TEST_F(TakeResourceTest, AnObjectTheHandThrewIntoAWorshipSiteCountsOneSupply)
{
	const auto site = Reg().Create();
	const auto object = Reg().Create();
	InPhysics(object, true, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(worship::site::DeleteObjectAndTakeResource(site, object, {}));
	EXPECT_EQ(Supplies(), 1u);
}

namespace
{
using ecs::StoragePitDeed;
using ecs::StoragePitStore;

/// Counts the GameRand draws (each must be GameRand(2)) and answers them in turn with `answers`, then 0
struct CountedDraws
{
	game_random::testing::ScopedState state;
	uint32_t draws {0};
	std::vector<uint32_t> answers;

	explicit CountedDraws(std::vector<uint32_t> values = {})
	    : answers(std::move(values))
	{
		game_random::testing::SetGameRand(
		    [this](uint32_t n) -> uint32_t {
			    EXPECT_EQ(n, 2u);
			    const uint32_t value = draws < answers.size() ? answers[draws] : 0;
			    ++draws;
			    return value;
		    },
		    nullptr);
	}
};

/// An interface of `player` putting down what belonged to `owner`
ecs::pot_resource::Dropper DropperOf(PlayerNames player, PlayerNames owner)
{
	ecs::pot_resource::Dropper dropper {true, player, false};
	dropper.sourceOwner = owner;
	return dropper;
}

entt::entity MakePit()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::StoragePit>(pit);
	return pit;
}
} // namespace

TEST_F(TakeResourceTest, FoodOfAnotherPlayerDrawsOnceForTheStealDeed)
{
	CountedDraws rand({1, 0});
	const auto pit = MakePit();
	const auto dropper = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// one GameRand(2) per deposit: not 0 -> stealing from a storage pit, 0 -> from a farm
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, dropper),
	          StoragePitDeed::StealFoodFromStoragePit);
	EXPECT_EQ(rand.draws, 1u);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, dropper),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 2u);
}

TEST_F(TakeResourceTest, OwnFoodAndAnyWoodDrawNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	// the player's own food
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::PutFoodInStoragePit);
	// wood, the player's own or another's
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE)),
	          StoragePitDeed::PutWoodInStoragePit);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::StealWoodFromStoragePit);
	EXPECT_EQ(rand.draws, 0u);
}

TEST_F(TakeResourceTest, WoodOnAnUnbuiltPitIsBuildingSiteWood)
{
	CountedDraws rand;
	const auto pit = MakePit();
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::Abode>(pit).buildingSite = site;
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::PutWoodInBuildingSite);
	EXPECT_EQ(rand.draws, 0u);
	// food on the same pit still goes the pit's way, with its draw
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, NoInterfaceOrNoPitDrawsNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	ecs::pot_resource::Dropper none;
	none.sourceOwner = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, none), std::nullopt);
	const auto other = Reg().Create(); // a worship site or a workshop: not a storage pit
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(other, ResourceType::Food,
	                                                              DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)),
	          std::nullopt);
	EXPECT_EQ(rand.draws, 0u);
}

TEST_F(TakeResourceTest, ADepositThatTakesNothingDrawsNothing)
{
	CountedDraws rand;
	const auto pit = MakePit();
	const auto object = Reg().Create(); // no resource: the pit takes nothing, so the creature is not told
	EXPECT_TRUE(ecs::take_resource::StoragePit(pit, object, DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO)));
	EXPECT_EQ(rand.draws, 0u);
}

namespace
{
/// A town of `id` owned by `owner`, listed in the registry's towns
entt::entity MakeTown(uint32_t id, PlayerNames owner)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<ecs::components::Town>(town, id).owner = owner;
	registry.Context().towns[id] = town;
	return town;
}

/// A storage pit of the town `townId` (none: Abode::k_NoTown) with a food pile and no wood piles; returns {pit, pile}
std::pair<entt::entity, entt::entity> MakePitWithPile(uint32_t townId)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::Abode>(pit).townId = townId;
	const auto pile = registry.Create();
	registry.Assign<ecs::components::Pot>(pile).type = PotInfo::StoragePitFoodPile;
	registry.Assign<ecs::components::Transform>(pile, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& store = registry.Assign<ecs::components::StoragePit>(pit);
	store.woodPiles.fill(entt::null);
	store.foodPile = pile;
	return {pit, pile};
}

/// A loose pile (not part of any store), its own player `owner` when given (a magic pile's maker)
entt::entity MakeLoosePile(std::optional<PlayerNames> owner = std::nullopt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pile = registry.Create();
	auto& pot = registry.Assign<ecs::components::Pot>(pile);
	if (owner)
	{
		pot.owner = *owner;
	}
	return pile;
}

/// A fish farm of `town` (entt::null: none, as the fish puzzle's farms)
entt::entity MakeFishFarm(entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto farm = registry.Create();
	registry.Assign<ecs::components::FishFarm>(farm).town = town;
	return farm;
}
} // namespace

TEST_F(TakeResourceTest, PickUpFromAPileRecordsThePilesPlayer)
{
	MakeTown(4, PlayerNames::PLAYER_THREE);
	// a pile of a storage pit: the pit's town's owner
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakePitWithPile(4).second), PlayerNames::PLAYER_THREE);
	// a pit without a town: the neutral player
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakePitWithPile(ecs::components::Abode::k_NoTown).second),
	          PlayerNames::NEUTRAL);
	// a magic pile: its maker; any other loose pile: the neutral player
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakeLoosePile(PlayerNames::PLAYER_TWO)), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(MakeLoosePile()), PlayerNames::NEUTRAL);
	// not a pot
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(Reg().Create()), PlayerNames::NEUTRAL);
}

TEST_F(TakeResourceTest, AWorshipSitesFoodPotAnswersWithTheSitesPlayer)
{
	// the pot's own player is not the one that answers while it is the site's
	const auto pot = MakeLoosePile(PlayerNames::PLAYER_FOUR);
	const auto site = Reg().Create();
	auto& worship = Reg().Assign<ecs::components::WorshipSite>(site);
	worship.player = PlayerNames::PLAYER_TWO;
	worship.foodPot = pot;
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(pot), PlayerNames::PLAYER_TWO);
	// no longer the site's pot: its own player again
	worship.foodPot = entt::null;
	EXPECT_EQ(ecs::object_resources::PlayerOfPile(pot), PlayerNames::PLAYER_FOUR);
}

TEST_F(TakeResourceTest, PickUpFromAFishFarmRecordsItsTownsOwnerOrNone)
{
	const auto town = MakeTown(5, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::fish_farms::PlayerOf(MakeFishFarm(town)), PlayerNames::PLAYER_TWO);
	// no town: no owner at all, not the neutral player
	EXPECT_EQ(ecs::fish_farms::PlayerOf(MakeFishFarm(entt::null)), std::nullopt);
}

TEST_F(TakeResourceTest, TheDrawFollowsTheRecordedOwner)
{
	CountedDraws rand;
	MakeTown(6, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(6);
	// food scooped from the player's own pit and put back: the player's own, no draw
	ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	hand.sourceOwner = ecs::object_resources::PlayerOfPile(pile);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::PutFoodInStoragePit);
	EXPECT_EQ(rand.draws, 0u);
	// food from a loose pile (the neutral player's): not the player's own, one draw
	hand.sourceOwner = ecs::object_resources::PlayerOfPile(MakeLoosePile());
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 1u);
	// fish from a farm with no town: no owner, one draw
	hand.sourceOwner = ecs::fish_farms::PlayerOf(MakeFishFarm(entt::null));
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Food, hand),
	          StoragePitDeed::StealFoodFromFarm);
	EXPECT_EQ(rand.draws, 2u);
	// an interface that has picked nothing up yet records no owner: the same
	const ecs::pot_resource::Dropper fresh {true, PlayerNames::PLAYER_ONE, true};
	EXPECT_EQ(fresh.sourceOwner, std::nullopt);
	EXPECT_EQ(StoragePitStore::DoCreatureMimicAfterAddingResource(pit, ResourceType::Wood, fresh),
	          StoragePitDeed::StealWoodFromStoragePit);
	EXPECT_EQ(rand.draws, 2u); // wood never draws
}

namespace
{
/// What a deposit into a pit of a town reaches from an interface, set up as the game has them: the info (the pit's food
/// pile with no cap, every multiplier 0, so no belief is drawn) and the day / night clock (the town's desire). Both are
/// put back when it goes
class InterfaceDepositServices
{
public:
	InterfaceDepositServices()
	{
		auto info = std::make_unique<InfoConstants>();
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = static_cast<PotInfo>(19);
		Locator::infoConstants::reset(info.release());
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
	test::RestoreService<Locator::dayNightClock> _clock;
};
} // namespace

TEST_F(TakeResourceTest, APutDownIntoAPitGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(7, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(7);
	// food the hand took from another player's store, put down on the pit of the dropper's own town
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 50, hand), 50u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 50u);
	// the pit's AddResource had the interface: the deed at the end of its town's side draws, then the put-down's own
	EXPECT_EQ(rand.draws, 2u);
}

TEST_F(TakeResourceTest, APutDownOnAPitsPileGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(8, PlayerNames::PLAYER_ONE);
	const auto pile = MakePitWithPile(8).second;
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// the pile hands it to its pit with the interface: the pit's town side and its deed; a pile tells the creature
	// nothing itself
	EXPECT_EQ(ecs::pot_resource::PotStructureAddResource(pile, ResourceType::Food, 30, false, hand), 30u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 30u);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, APutDownIntoAPitWithoutATownOnlyTellsItsOwnDeed)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// no town: no town side and no deed of its own, the put-down's deed only
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 20, hand), 20u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 20u);
	EXPECT_EQ(rand.draws, 1u);
	// no interface: nothing is told
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 5, {}), 5u);
	EXPECT_EQ(rand.draws, 1u);
}

TEST_F(TakeResourceTest, APoisonedPutDownPoisonsThePit)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	// unpoisoned food leaves the pit clean
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 10, {}), 10u);
	EXPECT_FALSE(ecs::object_resources::IsPoisoned(pit));
	// poisoned food put down on the pit poisons it
	EXPECT_EQ(ecs::pot_resource::StoragePitTakesPutDownResource(pit, ResourceType::Food, 10, {}, true), 10u);
	EXPECT_TRUE(Reg().Get<ecs::components::Pot>(pile).poisoned);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
}

// What a delivery or a taker asks of a thing's poison is the thing's own answer, as the original's: a toadstool always,
// another loose object never, a living thing its poisoned bit
TEST_F(TakeResourceTest, AToadstoolAndAPoisonedLivingThingAnswerPoisoned)
{
	auto& registry = Reg();
	const auto toadstool = registry.Create();
	registry.Assign<ecs::components::MobileObject>(toadstool, ecs::components::MobileObject {MobileObjectInfo::Toadstool});
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(toadstool));
	const auto barrel = registry.Create();
	registry.Assign<ecs::components::MobileObject>(barrel, ecs::components::MobileObject {MobileObjectInfo::EgyptBarrel});
	EXPECT_FALSE(ecs::object_resources::IsPoisoned(barrel));
	const auto lion = registry.Create();
	registry.Assign<ecs::components::Animal>(lion, AnimalInfo::Lion);
	EXPECT_FALSE(ecs::object_resources::IsPoisoned(lion));
	ecs::life::SetPoisoned(lion, true);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(lion));
}

TEST_F(TakeResourceTest, APoisonedPutDownOnAPitsPilePoisonsThePit)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	// the pile hands the poison on to its pit with the resource
	EXPECT_EQ(ecs::pot_resource::PotStructureAddResource(pile, ResourceType::Food, 25, true), 25u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 25u);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
}

TEST_F(TakeResourceTest, AResourceGivenToAPitsPileGivesThePitTheInterface)
{
	const InterfaceDepositServices services;
	CountedDraws rand;
	MakeTown(9, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(9);
	const auto hand = DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO);
	// a pile asked through the objects' AddResource passes the interface on: the pit's town side and its deed draw once
	EXPECT_EQ(ecs::object_resources::AddResource(pile, ResourceType::Food, 40, hand, true), 40u);
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Food), 40u);
	EXPECT_EQ(rand.draws, 1u);
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(pit));
	// without an interface the pit's town is told nothing
	EXPECT_EQ(ecs::object_resources::AddResource(pile, ResourceType::Food, 5), 5u);
	EXPECT_EQ(rand.draws, 1u);
}

namespace
{
/// The rows the objects' own resources read, set as the game's tables have them where it matters: a rock row and the
/// singing stone base's (both mobileType Rock) and the bonfire's (not), each worth 200 wood; the mushroom's food
/// value; a lion's food; a beech's wood; the storage pit food pile's kind (`foodPile`: a plain pot unless asked). Put
/// back when it goes
class ObjectResourceRows
{
public:
	explicit ObjectResourceRows(PotType foodPile = PotType::Pot)
	{
		auto info = std::make_unique<InfoConstants>();
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).potType = foodPile;
		for (const auto row : {MobileStaticInfo::Rock, MobileStaticInfo::SingingStoneBase, MobileStaticInfo::Bonfire})
		{
			auto& statics = info->mobileStatic.at(static_cast<size_t>(row));
			statics.mobileType = row == MobileStaticInfo::Bonfire ? MobileStaticInfo::CeltFenceShort : MobileStaticInfo::Rock;
			statics.woodValue = 200;
		}
		auto& champi = info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Champi));
		champi.mobileType = MobileObjectInfo::Champi;
		champi.foodValue = 99.9f;
		auto& ball = info->mobileObject.at(static_cast<size_t>(MobileObjectInfo::Ball));
		ball.mobileType = MobileObjectInfo::Ball;
		ball.foodValue = 50.0f;
		auto& lion = info->animal.at(static_cast<size_t>(AnimalInfo::Lion));
		lion.foodValue = 900.0f;
		lion.foodType = FoodType::Meat;
		info->tree.at(static_cast<size_t>(TreeInfo::Beech)).woodValue = 350;
		info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).resourceType = ResourceType::Wood;
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};

/// A thing of `scale`
entt::entity Sized(float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(scale));
	return thing;
}

entt::entity MakeMobileStatic(MobileStaticInfo row, float scale)
{
	const auto thing = Sized(scale);
	Locator::entitiesRegistry::value().Assign<ecs::components::MobileStatic>(thing, row);
	return thing;
}

/// A pot of the row `type` holding `amount`
entt::entity MakePot(PotInfo type, uint16_t amount)
{
	const auto pot = Sized(1.0f);
	auto& component = Locator::entitiesRegistry::value().Assign<ecs::components::Pot>(pot);
	component.type = type;
	component.amount = amount;
	return pot;
}
} // namespace

TEST_F(TakeResourceTest, RocksHoldWoodByTheirScale)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	// a rock: its scale times its row's wood, truncated (the original gives 158 for a rock of scale 0.7935, 274 for
	// 1.37); nothing of any other type
	const auto rock = MakeMobileStatic(MobileStaticInfo::Rock, 0.7935f);
	EXPECT_EQ(resources::GetResourceType(rock), ResourceType::Wood);
	EXPECT_EQ(resources::GetResource(rock, ResourceType::Wood), 158u);
	EXPECT_EQ(resources::GetResource(rock, ResourceType::Food), 0u);
	// the bonfire is of the rock class whatever its row's kind
	EXPECT_EQ(resources::GetResource(MakeMobileStatic(MobileStaticInfo::Bonfire, 1.37f), ResourceType::Wood), 274u);
	// a piece knocked off a building has the rock row
	const auto fragment = Sized(0.5f);
	Reg().Assign<ecs::components::Fragment>(fragment);
	EXPECT_EQ(resources::GetResource(fragment, ResourceType::Wood), 100u);
	// the singing stone base's row is made a plain base, not a rock: it holds nothing
	const auto base = MakeMobileStatic(MobileStaticInfo::SingingStoneBase, 1.0f);
	EXPECT_EQ(resources::GetResourceType(base), ResourceType::None);
	EXPECT_EQ(resources::GetResource(base, ResourceType::Wood), 0u);
}

TEST_F(TakeResourceTest, ARockTheBonfireAndAFragmentAnswerTheyAreRocksButOnlyARockHasARocksOwnRules)
{
	const ObjectResourceRows rows;
	namespace object_physics = ecs::object_physics;
	const auto rock = MakeMobileStatic(MobileStaticInfo::Rock, 1.0f);
	const auto bonfire = MakeMobileStatic(MobileStaticInfo::Bonfire, 1.0f);
	const auto fragment = Sized(1.0f);
	Reg().Assign<ecs::components::Fragment>(fragment);
	const auto deadTree = Sized(1.0f);
	Reg().Assign<ecs::components::DeadTree>(deadTree);
	const auto felledTree = Sized(1.0f);
	Reg().Assign<ecs::components::DeadTree>(felledTree);
	Reg().Assign<ecs::components::FelledTree>(felledTree);
	const auto fence = MakeMobileStatic(MobileStaticInfo::CeltFenceShort, 1.0f);
	// a rock, the bonfire and a piece knocked off a building answer that they are rocks; a dead or felled tree, a
	// fence and the plain base made of the singing stone base's row do not
	EXPECT_TRUE(object_physics::IsRock(rock));
	EXPECT_TRUE(object_physics::IsRock(bonfire));
	EXPECT_TRUE(object_physics::IsRock(fragment));
	EXPECT_FALSE(object_physics::IsRock(deadTree));
	EXPECT_FALSE(object_physics::IsRock(felledTree));
	EXPECT_FALSE(object_physics::IsRock(fence));
	EXPECT_FALSE(object_physics::IsRock(MakeMobileStatic(MobileStaticInfo::SingingStoneBase, 1.0f)));
	// only the rock itself is a rock object, with its own rules for the hand, the tap and the knocks
	EXPECT_TRUE(object_physics::IsRockObject(rock));
	EXPECT_FALSE(object_physics::IsRockObject(bonfire));
	EXPECT_FALSE(object_physics::IsRockObject(fragment));
	EXPECT_FALSE(object_physics::IsRockObject(deadTree));
	EXPECT_FALSE(object_physics::IsRockObject(felledTree));
	EXPECT_FALSE(object_physics::IsRockObject(fence));
	// the bonfire is the bonfire's row only
	EXPECT_TRUE(object_physics::IsBonfire(bonfire));
	EXPECT_FALSE(object_physics::IsBonfire(rock));
	EXPECT_FALSE(object_physics::IsBonfire(fragment));
}

TEST_F(TakeResourceTest, ASplitThingsHalvesAreMadeFromItsRowWithItsPlayer)
{
	const ObjectResourceRows rows;
	namespace object_physics = ecs::object_physics;
	// a rock's halves are rocks of its row, and keep its player
	const auto rock = MakeMobileStatic(MobileStaticInfo::Rock, 1.0f);
	Reg().Get<ecs::components::MobileStatic>(rock).owner = PlayerNames::PLAYER_TWO;
	const auto ofRock = object_physics::HalvesOf(rock);
	ASSERT_TRUE(ofRock.has_value());
	EXPECT_EQ(ofRock->row, MobileStaticInfo::Rock);
	EXPECT_EQ(ofRock->owner, std::optional {PlayerNames::PLAYER_TWO});
	// the bonfire's are bonfires, made as the bonfire's row is, with no player of their own
	const auto ofBonfire = object_physics::HalvesOf(MakeMobileStatic(MobileStaticInfo::Bonfire, 1.0f));
	ASSERT_TRUE(ofBonfire.has_value());
	EXPECT_EQ(ofBonfire->row, MobileStaticInfo::Bonfire);
	EXPECT_FALSE(ofBonfire->owner.has_value());
	// a piece knocked off a building gives two rocks of the rock's row, the neutral player's
	const auto fragment = Sized(1.0f);
	Reg().Assign<ecs::components::Fragment>(fragment);
	const auto ofFragment = object_physics::HalvesOf(fragment);
	ASSERT_TRUE(ofFragment.has_value());
	EXPECT_EQ(ofFragment->row, MobileStaticInfo::Rock);
	EXPECT_EQ(ofFragment->owner, std::optional {PlayerNames::NEUTRAL});
	// nothing else splits: a dead tree, a fence
	const auto deadTree = Sized(1.0f);
	Reg().Assign<ecs::components::DeadTree>(deadTree);
	EXPECT_FALSE(object_physics::HalvesOf(deadTree).has_value());
	EXPECT_FALSE(object_physics::HalvesOf(MakeMobileStatic(MobileStaticInfo::CeltFenceShort, 1.0f)).has_value());
}

TEST_F(TakeResourceTest, LivingAndGrowingThingsAnswerTheirOwnResource)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	// a mushroom is food worth its row's food value, truncated; another mobile object holds nothing
	const auto mushroom = Sized(1.0f);
	Reg().Assign<ecs::components::MobileObject>(mushroom, MobileObjectInfo::Champi);
	EXPECT_EQ(resources::GetResourceType(mushroom), ResourceType::Food);
	EXPECT_EQ(resources::GetResource(mushroom, ResourceType::Food), 99u);
	EXPECT_EQ(resources::GetResource(mushroom, ResourceType::Wood), 0u);
	const auto ball = Sized(1.0f);
	Reg().Assign<ecs::components::MobileObject>(ball, MobileObjectInfo::Ball);
	EXPECT_EQ(resources::GetResourceType(ball), ResourceType::None);
	EXPECT_EQ(resources::GetResource(ball, ResourceType::Food), 0u);
	// an animal its food; no wood
	const auto lion = Sized(2.0f);
	Reg().Assign<ecs::components::Animal>(lion, AnimalInfo::Lion);
	EXPECT_EQ(resources::GetResourceType(lion), ResourceType::Food);
	EXPECT_EQ(resources::GetResource(lion, ResourceType::Food), 900u);
	EXPECT_EQ(resources::GetResource(lion, ResourceType::Wood), 0u);
	// a dead tree its wood, by its scale; no food
	const auto log = Sized(0.5f);
	Reg().Assign<ecs::components::DeadTree>(log, TreeInfo::Beech, 1.0f);
	EXPECT_EQ(resources::GetResourceType(log), ResourceType::Wood);
	EXPECT_EQ(resources::GetResource(log, ResourceType::Wood), 175u);
	EXPECT_EQ(resources::GetResource(log, ResourceType::Food), 0u);
}

TEST_F(TakeResourceTest, AFishFarmIsFoodButAnswersNone)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	// a fish farm's type is food, but asked through the objects it holds nothing (its fish are taken its own way)
	const auto farm = MakeFishFarm(entt::null);
	EXPECT_EQ(resources::GetResourceType(farm), ResourceType::Food);
	EXPECT_EQ(resources::GetResource(farm, ResourceType::Food), 0u);
	// a thing of no kind: no type, and nothing even asked for no type
	const auto thing = Reg().Create();
	EXPECT_EQ(resources::GetResourceType(thing), ResourceType::None);
	EXPECT_EQ(resources::GetResource(thing, ResourceType::None), 0u);
}

TEST_F(TakeResourceTest, AWorshipSiteGivesTheFoodInItsPot)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto pot = MakePot(PotInfo::StoragePitFoodPile, 40);
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::WorshipSite>(site).foodPot = pot;
	// it holds the food in its pot and nothing else
	EXPECT_EQ(resources::GetResource(site, ResourceType::Food), 40u);
	EXPECT_EQ(resources::GetResource(site, ResourceType::Wood), 0u);
	// food comes out of the pot; without a building site wood (or any) comes from nowhere
	EXPECT_EQ(resources::RemoveResource(site, ResourceType::Food, 15), 15u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pot).amount, 25u);
	EXPECT_EQ(resources::RemoveResource(site, ResourceType::Wood, 5), 0u);
	EXPECT_EQ(resources::RemoveResource(site, ResourceType::Any, 5), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pot).amount, 25u);
	// no pot: nothing, and none is made
	Reg().Get<ecs::components::WorshipSite>(site).foodPot = entt::null;
	EXPECT_EQ(resources::GetResource(site, ResourceType::Food), 0u);
	EXPECT_EQ(resources::RemoveResource(site, ResourceType::Food, 5), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::WorshipSite>(site).foodPot, entt::entity {entt::null});
}

TEST_F(TakeResourceTest, ABuildingSiteAndTheStructuresOnItGiveItsPilesWood)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto pile = MakePot(PotInfo::WoodPile_1, 30);
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::BuildingSite>(site).woodPile = pile;
	// the site holds its pile's wood
	EXPECT_EQ(resources::GetResource(site, ResourceType::Wood), 30u);
	EXPECT_EQ(resources::GetResource(site, ResourceType::Food), 0u);
	EXPECT_EQ(resources::RemoveResource(site, ResourceType::Wood, 10), 10u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 20u);
	// a part of a citadel on the site passes whatever is asked on to it (a standard site gives only wood); the part
	// holds nothing itself
	const auto part = Reg().Create();
	Reg().Assign<ecs::components::CitadelPartBuild>(part).buildingSite = site;
	EXPECT_EQ(resources::RemoveResource(part, ResourceType::Wood, 5), 5u);
	EXPECT_EQ(resources::RemoveResource(part, ResourceType::Food, 5), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 15u);
	EXPECT_EQ(resources::GetResource(part, ResourceType::Wood), 0u);
	// off its site, nothing
	Reg().Get<ecs::components::CitadelPartBuild>(part).buildingSite = entt::null;
	EXPECT_EQ(resources::RemoveResource(part, ResourceType::Wood, 5), 0u);
}

TEST_F(TakeResourceTest, ADeadTreeGivesItsWoodAndGoesWhenAskedForAnythingElse)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto log = Sized(0.5f);
	Reg().Assign<ecs::components::DeadTree>(log, TreeInfo::Beech, 1.0f);
	// 175 wood: 75 taken, it shrinks to what is left
	EXPECT_EQ(resources::RemoveResource(log, ResourceType::Wood, 75), 75u);
	EXPECT_EQ(resources::GetResource(log, ResourceType::Wood), 100u);
	// asked for food it holds none, so it goes and gives nothing
	EXPECT_EQ(resources::RemoveResource(log, ResourceType::Food, 1), 0u);
	EXPECT_FALSE(Reg().Valid(log));
}

TEST_F(TakeResourceTest, ABigForestAndAFishFarmGiveOnlyTheirOwnResource)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto forest = Sized(1.0f);
	auto& big = Reg().Assign<ecs::components::BigForest>(forest);
	big.wood = 500.0f;
	big.woodValue = 500.0f;
	EXPECT_EQ(resources::RemoveResource(forest, ResourceType::Food, 10), 0u);
	EXPECT_EQ(resources::RemoveResource(forest, ResourceType::Wood, 100), 100u);
	EXPECT_FLOAT_EQ(Reg().Get<ecs::components::BigForest>(forest).wood, 400.0f);
	// a big forest's wood is not read through the objects: it answers nothing
	EXPECT_EQ(resources::GetResource(forest, ResourceType::Wood), 0u);

	const auto farm = MakeFishFarm(entt::null);
	Reg().Get<ecs::components::FishFarm>(farm).food = 30.0f;
	EXPECT_EQ(resources::RemoveResource(farm, ResourceType::Wood, 10), 0u);
	EXPECT_EQ(resources::RemoveResource(farm, ResourceType::Food, 10), 10u);
	// more than it has: all that is left, truncated
	Reg().Get<ecs::components::FishFarm>(farm).food = 20.5f;
	EXPECT_EQ(resources::RemoveResource(farm, ResourceType::Food, 50), 20u);
	EXPECT_EQ(Reg().Get<ecs::components::FishFarm>(farm).food, 0.0f);
}

TEST_F(TakeResourceTest, ARockShrinksAsItsWoodIsTakenAndGoesWhenAllOfItIs)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	// the original's numbers: a rock of scale 0.7935 at 0.75 above the ground gives 37 of its 158 wood and keeps scale
	// 0.605, its height shrinking with it
	const auto rock = MakeMobileStatic(MobileStaticInfo::Rock, 0.7935f);
	Reg().Get<ecs::components::Transform>(rock).position.y = 0.75f;
	EXPECT_EQ(resources::RemoveResource(rock, ResourceType::Wood, 37), 37u);
	const auto& transform = Reg().Get<const ecs::components::Transform>(rock);
	EXPECT_EQ(std::bit_cast<uint32_t>(transform.scale.x), 0x3F1AE148u);
	EXPECT_EQ(std::bit_cast<uint32_t>(transform.position.y), 0x3F1263B1u);
	// one short of all of it: a tiny rock
	const auto boulder = MakeMobileStatic(MobileStaticInfo::Rock, 1.37f);
	Reg().Get<ecs::components::Transform>(boulder).position.y = 1.5f;
	EXPECT_EQ(resources::RemoveResource(boulder, ResourceType::Wood, 273), 273u);
	EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<const ecs::components::Transform>(boulder).scale.x), 0x3BA3D70Au);
	EXPECT_EQ(std::bit_cast<uint32_t>(Reg().Get<const ecs::components::Transform>(boulder).position.y), 0x3BB36309u);
	// all of it: the rock goes, giving what it had
	const auto pebble = MakeMobileStatic(MobileStaticInfo::Rock, 0.5f);
	EXPECT_EQ(resources::RemoveResource(pebble, ResourceType::Wood, 100), 100u);
	EXPECT_FALSE(Reg().Valid(pebble));
	// asked for food it holds none, so it goes and gives nothing
	const auto stone = MakeMobileStatic(MobileStaticInfo::Rock, 1.0f);
	EXPECT_EQ(resources::RemoveResource(stone, ResourceType::Food, 5), 0u);
	EXPECT_FALSE(Reg().Valid(stone));
}

TEST_F(TakeResourceTest, AWorshipSitesFoodPotIsPartOfTheSiteAndStaysWhenEmptied)
{
	// the pot is a pile of food, which goes once emptied unless it is part of a structure
	const ObjectResourceRows rows(PotType::PileFood);
	namespace resources = ecs::object_resources;
	const auto pot = MakePot(PotInfo::StoragePitFoodPile, 30);
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::WorshipSite>(site).foodPot = pot;
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 20), 20u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pot).amount, 10u);
	// more than it has: all of it, and the empty pot stays the site's
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 25), 10u);
	ASSERT_TRUE(Reg().Valid(pot));
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pot).amount, 0u);
	EXPECT_EQ(Reg().Get<ecs::components::WorshipSite>(site).foodPot, pot);
	// a loose pile of the same kind goes once emptied
	const auto loose = MakePot(PotInfo::StoragePitFoodPile, 5);
	EXPECT_EQ(resources::RemoveResource(loose, ResourceType::Food, 9), 5u);
	EXPECT_FALSE(Reg().Valid(loose));
}

TEST_F(TakeResourceTest, AWorshipSitesFoodPotGivesWhatItHasThenTheSiteTheRest)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto pot = MakePot(PotInfo::StoragePitFoodPile, 5);
	const auto pile = MakePot(PotInfo::WoodPile_1, 50);
	const auto buildingSite = Reg().Create();
	Reg().Assign<ecs::components::BuildingSite>(buildingSite).woodPile = pile;
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::WorshipSite>(site).foodPot = pot;
	Reg().Assign<ecs::components::CitadelPartBuild>(site).buildingSite = buildingSite;
	// asked for wood the pot gives its 5 all the same (its take does not look at the type), then the site's building
	// site the 7 left
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Wood, 12), 12u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pot).amount, 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 43u);
	// asked for food, an empty pot gives nothing and neither does the site
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 3), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 43u);
}

TEST_F(TakeResourceTest, APoisonedPileStaysPoisonedUntilItIsEmptied)
{
	// the worship site's food pot is a pile of food that stays its site's when emptied
	const ObjectResourceRows rows(PotType::PileFood);
	namespace resources = ecs::object_resources;
	const auto pot = MakePot(PotInfo::StoragePitFoodPile, 30);
	Reg().Get<ecs::components::Pot>(pot).poisoned = true;
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::WorshipSite>(site).foodPot = pot;
	// some of it taken: the rest is as poisoned as before
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 20), 20u);
	EXPECT_TRUE(resources::IsPoisoned(pot));
	// all of it taken: the empty pot is clean
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 25), 10u);
	ASSERT_TRUE(Reg().Valid(pot));
	EXPECT_FALSE(resources::IsPoisoned(pot));
	// a store's poisoned food pile: the store stays poisoned while any of it is left, and is clean once it is emptied
	const auto [pit, pile] = MakePitWithPile(ecs::components::Abode::k_NoTown);
	auto& food = Reg().Get<ecs::components::Pot>(pile);
	food.amount = 40;
	food.poisoned = true;
	EXPECT_EQ(resources::RemoveResource(pit, ResourceType::Food, 15), 15u);
	EXPECT_TRUE(resources::IsPoisoned(pit));
	EXPECT_EQ(resources::RemoveResource(pit, ResourceType::Food, 25), 25u);
	ASSERT_TRUE(Reg().Valid(pile));
	EXPECT_FALSE(resources::IsPoisoned(pile));
	EXPECT_FALSE(resources::IsPoisoned(pit));
}

TEST_F(TakeResourceTest, AHomeKeepsNoPoisonFromTheFoodItIsGiven)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	const auto home = Reg().Create();
	Reg().Assign<ecs::components::Abode>(home, AbodeNumber::A, ecs::components::Abode::k_NoTown, 0u, 0u);
	// poisoned food goes in whole, and the home is never poisoned by it
	EXPECT_EQ(resources::AddResource(home, ResourceType::Food, 60, {}, true), 60u);
	EXPECT_EQ(Reg().Get<ecs::components::Abode>(home).foodAmount, 60u);
	EXPECT_FALSE(resources::IsPoisoned(home));
}

namespace
{
/// A hand's gift to a town reads the town's want of food before and after, as info.dat has it: the villagers want five
/// times their food for dinner; the belief is a quarter of how much the want drops (a player giving to another's town
/// counts `nonOwnerMultiplier` times, once as info.dat has it), the giver's alignment half of it, and a type taken
/// by the same player within the last 1000 turns counts less. The day / night clock is the town desire's. Both are
/// put back when it goes
class GiftServices
{
public:
	explicit GiftServices(float nonOwnerMultiplier = 1.0f)
	{
		auto info = std::make_unique<InfoConstants>();
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).nextPotForResource = static_cast<PotInfo>(19);
		info->town.foodWantedMultiplier = 5.0f;
		info->town.multiplierForAddingResourceToTown = 0.25f;
		info->town.multiplierForNonOwnerAddingResource = nonOwnerMultiplier;
		info->town.giveResourceAligmnetChangeMultiplier = 0.5f;
		info->town.maxGameturnsForBeliefAfterRemovingFromStoragePit = 1000;
		info->townDesire.at(static_cast<size_t>(TownDesireInfo::ForFood)).tribeMultiplier.fill(1.0f);
		Locator::infoConstants::reset(info.release());
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
	test::RestoreService<Locator::dayNightClock> _clock;
};

/// A town owned by the first player whose villagers eat 850 food at dinner (so it wants 4250), and its storage pit
struct HungryTown
{
	entt::entity town;
	entt::entity pit;
	entt::entity pile;
};

/// The town of `id`, its storage pit holding `food`
HungryTown MakeHungryTown(uint32_t id, uint16_t food)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = MakeTown(id, PlayerNames::PLAYER_ONE);
	const auto [pit, pile] = MakePitWithPile(id);
	auto& t = registry.Get<ecs::components::Town>(town);
	t.storagePit = pit;
	t.stats.foodForDinner = 850.0f;
	registry.Get<ecs::components::Pot>(pile).amount = food;
	return {town, pit, pile};
}

/// The first player's hand, giving back what it took from its own town
ecs::pot_resource::Dropper OwnHand()
{
	return DropperOf(PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE);
}

float PendingBelief(entt::entity town, PlayerNames player)
{
	return Locator::entitiesRegistry::value().Get<ecs::components::Town>(town).belief.pending.at(static_cast<size_t>(player));
}

float PendingAlignment(PlayerNames player)
{
	return ecs::effects::alignment::Of(player).pending;
}

// 1000 food of the 4250 wanted: a want of 1 - 1000 / 4250 times the same for the food no one is fetching, 0.584775;
// with 2000 it is 0.280277, so 1000 more lowers it by 0.304498
constexpr float k_WantDrop = 0.304498f;
} // namespace

TEST_F(TakeResourceTest, AGiftOfFoodToATownThatWantsItAddsBeliefByHowMuchTheWantDrops)
{
	const GiftServices services;
	CountedDraws rand;
	const auto hungry = MakeHungryTown(11, 1000);
	EXPECT_EQ(ecs::object_resources::AddResource(hungry.pit, ResourceType::Food, 1000, OwnHand()), 1000u);
	// a quarter of the drop to the giver's belief in the town, half of it to the giver's alignment
	EXPECT_NEAR(PendingBelief(hungry.town, PlayerNames::PLAYER_ONE), k_WantDrop * 0.25f, 1e-6f);
	EXPECT_NEAR(PendingAlignment(PlayerNames::PLAYER_ONE), k_WantDrop * 0.5f, 1e-6f);
	// the belief is shown over the store, in ten thousandths
	ASSERT_EQ(ecs::town_belief::BeliefSpriteCount(), 1u);
	EXPECT_EQ(ecs::town_belief::PopBeliefSprite()->amount, 761);
	// food given back 500 turns after the giver took food from the town counts an eighth: half of the 1000 turns, cubed
	const auto taken = MakeHungryTown(12, 1000);
	Reg()
	    .Get<ecs::components::Town>(taken.town)
	    .resourceLastRemovedTurn.at(static_cast<size_t>(PlayerNames::PLAYER_ONE))
	    .at(static_cast<size_t>(ResourceType::Food)) = game_clock::Turn() - 500;
	const float alignment = PendingAlignment(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::object_resources::AddResource(taken.pit, ResourceType::Food, 1000, OwnHand()), 1000u);
	EXPECT_NEAR(PendingBelief(taken.town, PlayerNames::PLAYER_ONE), k_WantDrop * 0.125f * 0.25f, 1e-6f);
	EXPECT_NEAR(PendingAlignment(PlayerNames::PLAYER_ONE) - alignment, k_WantDrop * 0.125f * 0.5f, 1e-6f);
	// food given with no hand (a villager's, a script's) leaves belief and alignment alone
	const auto quiet = MakeHungryTown(13, 1000);
	const float before = PendingAlignment(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::object_resources::AddResource(quiet.pit, ResourceType::Food, 1000), 1000u);
	EXPECT_EQ(PendingBelief(quiet.town, PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_EQ(PendingAlignment(PlayerNames::PLAYER_ONE), before);
}

TEST_F(TakeResourceTest, APoisonedGiftOfFoodCountsAsAnOrdinaryGift)
{
	const GiftServices services;
	CountedDraws rand;
	// the same gift to two towns alike, clean food to one and poisoned food (a toadstool's) to the other
	const auto clean = MakeHungryTown(14, 1000);
	const auto poisoned = MakeHungryTown(15, 1000);
	EXPECT_EQ(ecs::object_resources::AddResource(clean.pit, ResourceType::Food, 1000, OwnHand()), 1000u);
	const float cleanAlignment = PendingAlignment(PlayerNames::PLAYER_ONE);
	EXPECT_EQ(ecs::object_resources::AddResource(poisoned.pit, ResourceType::Food, 1000, OwnHand(), true), 1000u);
	const float poisonedAlignment = PendingAlignment(PlayerNames::PLAYER_ONE) - cleanAlignment;
	// the store keeps the poison, and the giver gets the usual belief and alignment for it
	EXPECT_TRUE(ecs::object_resources::IsPoisoned(poisoned.pit));
	EXPECT_FALSE(ecs::object_resources::IsPoisoned(clean.pit));
	EXPECT_NEAR(PendingBelief(poisoned.town, PlayerNames::PLAYER_ONE), k_WantDrop * 0.25f, 1e-6f);
	EXPECT_EQ(PendingBelief(poisoned.town, PlayerNames::PLAYER_ONE), PendingBelief(clean.town, PlayerNames::PLAYER_ONE));
	EXPECT_EQ(poisonedAlignment, cleanAlignment);
}

TEST_F(TakeResourceTest, AForestMiraclesTreeGivesWoodByItsCastersTribalPower)
{
	const ObjectResourceRows rows;
	namespace resources = ecs::object_resources;
	// a forest miracle's tree is worth a quarter of its caster's tribal power times an ordinary tree of its kind
	constexpr float k_TribalPower = 6.0f;
	const auto plain = Sized(1.0f);
	Reg().Assign<ecs::components::Tree>(plain).type = TreeInfo::Beech;
	EXPECT_EQ(resources::GetResource(plain, ResourceType::Wood), 350u);
	const auto grown = Sized(1.0f);
	auto& tree = Reg().Assign<ecs::components::Tree>(grown);
	tree.type = TreeInfo::Beech;
	tree.woodValueMultiplier = 0.25f * k_TribalPower;
	EXPECT_EQ(resources::GetResource(grown, ResourceType::Wood), 525u);
	// felled, the dead tree keeps what its tree was worth: by its scale, truncated
	const auto log = Sized(0.5f);
	Reg().Assign<ecs::components::DeadTree>(log, TreeInfo::Beech, 0.25f * k_TribalPower);
	EXPECT_EQ(resources::GetResource(log, ResourceType::Wood), 262u);
}

namespace
{
/// The storage pit piles' rows: a wood pile holds 100 drawn full and the food pile 300. Put back when it goes
class StorePileRows
{
public:
	static constexpr uint32_t k_WoodPileMost = 100;
	static constexpr uint32_t k_FoodPileMost = 300;

	StorePileRows()
	{
		auto info = std::make_unique<InfoConstants>();
		info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1)).maxAmountInPot = k_WoodPileMost;
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).maxAmountInPot = k_FoodPileMost;
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};

uint32_t AmountOf(entt::entity pot)
{
	return Locator::entitiesRegistry::value().Get<const ecs::components::Pot>(pot).amount;
}
} // namespace

TEST_F(TakeResourceTest, WhatAStoreHoldsOverItsMaximumIsTakenFirst)
{
	const StorePileRows rows;
	namespace resources = ecs::object_resources;
	// a store of no town: four full wood piles and a last one, which has no cap, holding 250; its food pile 400
	const auto pit = Reg().Create();
	auto& store = Reg().Assign<ecs::components::StoragePit>(pit);
	constexpr std::array<uint16_t, 5> k_Wood {100, 100, 100, 100, 250};
	for (size_t i = 0; i < k_Wood.size(); ++i)
	{
		const auto type = static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + static_cast<int32_t>(i));
		store.woodPiles.at(i) = MakePot(type, k_Wood.at(i));
	}
	store.foodPile = MakePot(PotInfo::StoragePitFoodPile, 400);
	const auto piles = Reg().Get<const ecs::components::StoragePit>(pit).woodPiles;
	// over its maximum: the wood over five full piles, the food over one
	EXPECT_EQ(StoragePitStore::AmountOverMaximum(pit, ResourceType::Wood), 150);
	EXPECT_EQ(StoragePitStore::AmountOverMaximum(pit, ResourceType::Food), 100);
	// 200 asked of the first pile: it gives only the 50 not over the maximum, and the 150 over comes from the store, last
	// pile first
	EXPECT_EQ(resources::RemoveResource(piles.at(0), ResourceType::Wood, 200), 200u);
	EXPECT_EQ(AmountOf(piles.at(0)), 50u);
	EXPECT_EQ(AmountOf(piles.at(4)), 100u);
	for (size_t i = 1; i < 4; ++i)
	{
		EXPECT_EQ(AmountOf(piles.at(i)), 100u) << i;
	}
	// below its maximum the pile asked gives it all itself
	EXPECT_EQ(StoragePitStore::AmountOverMaximum(pit, ResourceType::Wood), -50);
	EXPECT_EQ(resources::RemoveResource(piles.at(1), ResourceType::Wood, 30), 30u);
	EXPECT_EQ(AmountOf(piles.at(1)), 70u);
	EXPECT_EQ(AmountOf(piles.at(4)), 100u);
}

TEST_F(TakeResourceTest, APotOverItsMaximumIsBroughtBackToItAndAnswersTheWrappedRoom)
{
	// a clipped kind: the amount asked, or the room left when it does not fit, 32-bit unsigned and unchecked
	const StorePileRows rows;
	namespace pots = ecs::pot_resource;
	const auto pile = MakePot(PotInfo::WoodPile_1, 60);
	// it fits: all of it
	EXPECT_EQ(pots::AddToPotDirect(pile, ResourceType::Wood, 30), 30u);
	EXPECT_EQ(AmountOf(pile), 90u);
	// it does not: the room left
	EXPECT_EQ(pots::AddToPotDirect(pile, ResourceType::Wood, 30), 10u);
	EXPECT_EQ(AmountOf(pile), 100u);
	// already over the maximum: back to it, and the room 100 - 250 wraps round
	Reg().Get<ecs::components::Pot>(pile).amount = 250;
	EXPECT_EQ(pots::AddToPotDirect(pile, ResourceType::Wood, 10), 4294967146u);
	EXPECT_EQ(AmountOf(pile), 100u);
}

namespace
{
/// The wood piles' rows for a split: the store's five wood piles and the magic wood hold wood, a store wood pile 100
/// drawn full. Put back when it goes
class SplitPileRows
{
public:
	SplitPileRows()
	{
		auto info = std::make_unique<InfoConstants>();
		for (int32_t i = 0; i < 5; ++i)
		{
			auto& pile = info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1) + static_cast<size_t>(i));
			pile.resourceType = ResourceType::Wood;
			pile.maxAmountInPot = 100;
		}
		info->pot.at(static_cast<size_t>(PotInfo::MagicWood)).resourceType = ResourceType::Wood;
		info->pot.at(static_cast<size_t>(PotInfo::HandWood)).resourceType = ResourceType::Wood;
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(TakeResourceTest, APileSplitOffAStoresPileComesFromTheWholeStoreAsAPileOfTheHandsKind)
{
	const SplitPileRows rows;
	namespace resources = ecs::object_resources;
	// a store of no town whose five wood piles hold 100, 100, 100, 100 and 50
	const auto pit = Reg().Create();
	auto& store = Reg().Assign<ecs::components::StoragePit>(pit);
	constexpr std::array<uint16_t, 5> k_Wood {100, 100, 100, 100, 50};
	for (size_t i = 0; i < k_Wood.size(); ++i)
	{
		const auto type = static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + static_cast<int32_t>(i));
		store.woodPiles.at(i) = MakePot(type, k_Wood.at(i));
	}
	const auto piles = Reg().Get<const ecs::components::StoragePit>(pit).woodPiles;
	// 300 split off the first pile: the store gives it, last pile first, and the pile asked keeps its own
	const auto piece = resources::SplitOffPile(piles.at(0), 300);
	ASSERT_NE(piece, entt::entity {entt::null});
	EXPECT_EQ(Reg().Get<const ecs::components::Pot>(piece).type, PotInfo::HandWood);
	EXPECT_EQ(AmountOf(piece), 300u);
	EXPECT_EQ(AmountOf(piles.at(0)), 100u);
	EXPECT_EQ(AmountOf(piles.at(1)), 50u);
	EXPECT_EQ(AmountOf(piles.at(2)), 0u);
	EXPECT_EQ(AmountOf(piles.at(3)), 0u);
	EXPECT_EQ(AmountOf(piles.at(4)), 0u);
	// at most what the whole store holds, then nothing
	EXPECT_EQ(AmountOf(resources::SplitOffPile(piles.at(4), 1000)), 150u);
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Wood), 0u);
	EXPECT_EQ(resources::SplitOffPile(piles.at(0), 1000), entt::entity {entt::null});
	// a pile of no store gives from itself
	const auto loose = MakePot(PotInfo::MagicWood, 80);
	EXPECT_EQ(AmountOf(resources::SplitOffPile(loose, 50)), 50u);
	EXPECT_EQ(AmountOf(loose), 30u);
}

namespace
{
/// The pot rows' kinds as the game's data has them: the store's food pile and the magic food are food piles, the
/// store's first wood pile and the magic wood wood piles, the food pot a plain pot; each a thing of the pot type. Put
/// back when it goes
class PileKindRows
{
public:
	PileKindRows()
	{
		auto info = std::make_unique<InfoConstants>();
		const auto row = [&info](PotInfo type, PotType kind) {
			auto& pot = info->pot.at(static_cast<size_t>(type));
			pot.type = ObjectType::Pot;
			pot.potType = kind;
			pot.maxAmountInPot = 1000;
		};
		row(PotInfo::StoragePitFoodPile, PotType::PileFood);
		row(PotInfo::MagicFood, PotType::PileFood);
		row(PotInfo::WoodPile_1, PotType::PileWood);
		row(PotInfo::MagicWood, PotType::PileWood);
		row(PotInfo::FoodPot, PotType::Pot);
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(TakeResourceTest, OnlyFoodPilesFollowTheLandsShape)
{
	const PileKindRows rows;
	const auto follows = [](PotInfo type) {
		const auto pot = ecs::archetypes::PotArchetype::Create(glm::vec3(50.0f, 0.0f, 50.0f), 0.0f, type, 10);
		EXPECT_NE(pot, entt::entity {entt::null});
		return Reg().AllOf<ecs::components::MorphWithTerrain>(pot);
	};
	// a food pile is drawn melted onto the land under it; a wood pile and a plain pot keep their own shape
	EXPECT_TRUE(follows(PotInfo::StoragePitFoodPile));
	EXPECT_TRUE(follows(PotInfo::MagicFood));
	EXPECT_FALSE(follows(PotInfo::WoodPile_1));
	EXPECT_FALSE(follows(PotInfo::MagicWood));
	EXPECT_FALSE(follows(PotInfo::FoodPot));
}

namespace
{
/// The town rows' alignment multipliers for giving to and taking from a town: giving's is the game's data's 0.5,
/// taking's (0.5 there too) is made 0.25 here to tell them apart. Put back when it goes
class ResourceAlignmentRows
{
public:
	static constexpr float k_Give = 0.5f;
	static constexpr float k_Take = 0.25f;

	ResourceAlignmentRows()
	{
		auto info = std::make_unique<InfoConstants>();
		info->town.giveResourceAligmnetChangeMultiplier = k_Give;
		info->town.takeResourceAligmnetChangeMultiplier = k_Take;
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(TakeResourceTest, GivingATownWhatItWantsIsAGoodDeedAndTakingItAnEvilOne)
{
	const ResourceAlignmentRows rows;
	const test::RestoreService<Locator::playerSystem> restore;
	auto& players = Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	namespace alignment = ecs::effects::alignment;
	// players of neither alignment, so the change is not scaled. Given, the town's desire falls by 0.5: giving's share
	// of it, good
	alignment::UpdateForResource(PlayerNames::PLAYER_ONE, entt::null, 100, 0.5f);
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_ONE).pending, 0.25f);
	// taken, the town's desire rises by 0.5: taking's share of it, evil
	alignment::UpdateForResource(PlayerNames::PLAYER_TWO, entt::null, -100, -0.5f);
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_TWO).pending, -0.125f);
	// nothing given counts as taking
	alignment::UpdateForResource(PlayerNames::PLAYER_THREE, entt::null, 0, 0.5f);
	EXPECT_EQ(players.Alignment(PlayerNames::PLAYER_THREE).pending, 0.125f);
}

namespace
{
/// The rows of the magic food and wood piles and of a land script's food pile, which starts the food reaction (the
/// reactions are this test's own), on a land of 32 x 32 dry cells with the island in the locator. Put back when it
/// goes
class PutDownLand
{
public:
	PutDownLand()
	{
		auto info = std::make_unique<InfoConstants>();
		const auto row = [&info](PotInfo type, PotType kind, ResourceType resource, Reaction reaction) {
			auto& pot = info->pot.at(static_cast<size_t>(type));
			pot.type = ObjectType::Pot;
			pot.potType = kind;
			pot.resourceType = resource;
			pot.maxAmountInPot = 1000;
			pot.associatedReaction = reaction;
		};
		row(PotInfo::MagicFood, PotType::PileFood, ResourceType::Food, Reaction::None);
		row(PotInfo::MagicWood, PotType::PileWood, ResourceType::Wood, Reaction::None);
		row(PotInfo::FoodPile, PotType::PileFood, ResourceType::Food, Reaction::ReactToFood);
		Locator::infoConstants::reset(info.release());
		island = &static_cast<test::WaterCellIsland&>(Locator::terrainSystem::emplace<test::WaterCellIsland>(uint16_t {2}));
	}

	test::WaterCellIsland* island {nullptr};

private:
	test::RestoreService<Locator::infoConstants> _info;
	test::RestoreService<Locator::terrainSystem> _terrain;
};

/// Every pot and pile there is
std::vector<entt::entity> Pots()
{
	std::vector<entt::entity> pots;
	Locator::entitiesRegistry::value().Each<const ecs::components::Pot>(
	    [&pots](entt::entity entity, const ecs::components::Pot&) { pots.push_back(entity); });
	return pots;
}
} // namespace

TEST_F(TakeResourceTest, WhatNothingTakesMakesANewPileOnLandButNotInTheWater)
{
	// The original's put-down: what no store or pile in reach takes, on land, is a new magic food pile for food and a
	// new magic wood pile for wood, with the player of the interface that put it down (none without one) and the
	// poison; in the water, off the map or of any other resource it is lost
	// (docs/wiki/world/objects.md#pots-and-piles, docs/wiki/magic/food-wood-water-heal.md#food-and-wood)
	const PutDownLand land;
	land.island->Cell(glm::u16vec2(25, 25)) = test::MakeWaterCell(0, true, false);
	namespace pot_resource = ecs::pot_resource;
	using ecs::components::Pot;

	entt::entity food {entt::null};
	pot_resource::AddResourceToPos({55.0f, 0.0f, 55.0f}, {}, ResourceType::Food, 150, true, false, &food);
	ASSERT_NE(food, entt::entity {entt::null});
	const auto& foodPile = Reg().Get<const Pot>(food);
	EXPECT_EQ(foodPile.type, PotInfo::MagicFood);
	EXPECT_EQ(foodPile.amount, 150u);
	EXPECT_TRUE(foodPile.poisoned);
	EXPECT_EQ(foodPile.owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(Reg().Get<const ecs::components::Transform>(food).position.x, 55.0f);
	EXPECT_EQ(Reg().Get<const ecs::components::Transform>(food).position.z, 55.0f);

	const pot_resource::Dropper secondPlayer {.hasInterface = true, .player = PlayerNames::PLAYER_TWO};
	entt::entity wood {entt::null};
	pot_resource::AddResourceToPos({155.0f, 0.0f, 155.0f}, secondPlayer, ResourceType::Wood, 300, false, false, &wood);
	ASSERT_NE(wood, entt::entity {entt::null});
	const auto& woodPile = Reg().Get<const Pot>(wood);
	EXPECT_EQ(woodPile.type, PotInfo::MagicWood);
	EXPECT_EQ(woodPile.amount, 300u);
	EXPECT_FALSE(woodPile.poisoned);
	EXPECT_EQ(woodPile.owner, PlayerNames::PLAYER_TWO);
	ASSERT_EQ(Pots().size(), 2u);

	// in the water, off the map and of no resource nothing is made
	entt::entity lost {entt::entity {0}};
	pot_resource::AddResourceToPos({255.0f, 0.0f, 255.0f}, {}, ResourceType::Food, 150, false, false, &lost);
	EXPECT_EQ(lost, entt::entity {entt::null});
	pot_resource::AddResourceToPos({255.0f, 0.0f, 255.0f}, {}, ResourceType::Wood, 150, false, false, &lost);
	EXPECT_EQ(lost, entt::entity {entt::null});
	pot_resource::AddResourceToPos({400.0f, 0.0f, 55.0f}, {}, ResourceType::Food, 150, false, false, &lost);
	EXPECT_EQ(lost, entt::entity {entt::null});
	pot_resource::AddResourceToPos({105.0f, 0.0f, 205.0f}, {}, ResourceType::None, 150, false, false, &lost);
	EXPECT_EQ(lost, entt::entity {entt::null});
	EXPECT_EQ(Pots().size(), 2u);
}

TEST_F(TakeResourceTest, ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater)
{
	// CREATE_POT: the place check first (a 0.5 m circle at the point against the fixed objects of its cell, which
	// blocks only on land), then nothing for an amount of 0 or less; else the pot, which starts its row's reaction as it
	// is made and is the last thing created. A refused one writes nothing
	// (docs/wiki/scripts/map-script-objects.md#isoktocreateatpos and #pots-features-and-flowers)
	const PutDownLand land;
	land.island->Cell(glm::u16vec2(10, 10)) = test::MakeWaterCell(0, true, false);
	// a fixed object with a 3 m circle at its position, in its own cell only
	test::FakeMapShapeProvider shapes;
	shapes.meshShape = [](entt::entity object, ecs::map_collide::Shape& shape, float& reach) {
		const auto& at = Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(object).position;
		shape = {{at.x, at.z}, 3.0f, {}, 0.0f, "test"};
		reach = 4.0f;
		return true;
	};
	Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>(shapes);
	const auto fixedAt = [](glm::vec3 at) {
		const auto object = Reg().Create();
		ecs::object_index::Assign(object);
		Reg().Assign<ecs::components::Transform>(object, at, glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<ecs::components::MobileStatic>(object);
		ecs::map_cells::InsertMapObject(object);
	};
	fixedAt({55.0f, 0.0f, 55.0f});
	fixedAt({105.0f, 0.0f, 105.0f});
	using lhscriptx::FeatureScriptCommands;
	using lhscriptx::Script;
	Script::SetLastCreated(entt::null);

	// on the object, on land: refused
	FeatureScriptCommands::CreatePot({56.0f, 0.0f, 55.0f}, PotInfo::FoodPile, 0, 100);
	EXPECT_TRUE(Pots().empty());
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});
	// no amount: nothing
	FeatureScriptCommands::CreatePot({155.0f, 0.0f, 155.0f}, PotInfo::FoodPile, 0, 0);
	FeatureScriptCommands::CreatePot({155.0f, 0.0f, 155.0f}, PotInfo::FoodPile, 0, -5);
	EXPECT_TRUE(Pots().empty());
	EXPECT_EQ(Script::LastCreated(), entt::entity {entt::null});

	// on the object, in the water: made
	FeatureScriptCommands::CreatePot({106.0f, 0.0f, 105.0f}, PotInfo::FoodPile, 0, 100);
	ASSERT_EQ(Pots().size(), 1u);
	// on open land: made, with its amount, at its point, its reaction started and the last thing created
	FeatureScriptCommands::CreatePot({155.0f, 0.0f, 155.0f}, PotInfo::FoodPile, 0, 100);
	ASSERT_EQ(Pots().size(), 2u);
	const auto pot = Script::LastCreated();
	ASSERT_NE(pot, entt::entity {entt::null});
	const auto& made = Reg().Get<const ecs::components::Pot>(pot);
	EXPECT_EQ(made.type, PotInfo::FoodPile);
	EXPECT_EQ(made.amount, 100u);
	EXPECT_EQ(Reg().Get<const ecs::components::Transform>(pot).position.x, 155.0f);
	EXPECT_EQ(Reg().Get<const ecs::components::Transform>(pot).position.z, 155.0f);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(pot, Reaction::ReactToFood), 0u);
	Script::SetLastCreated(entt::null);
}

namespace
{
/// The pot rows as the game's data has them (FoodWood.realInfoDatMushroomsAndPiles): the food and wood pots plain pots,
/// the magic food and wood piles, each starting the food or the wood reaction by its resource. The land script's food
/// pile is given no reaction here, for the row that starts none. Put back when it goes
class PileReactionRows
{
public:
	PileReactionRows()
	{
		auto info = std::make_unique<InfoConstants>();
		const auto row = [&info](PotInfo type, PotType kind, ResourceType resource, Reaction reaction) {
			auto& pot = info->pot.at(static_cast<size_t>(type));
			pot.type = ObjectType::Pot;
			pot.potType = kind;
			pot.resourceType = resource;
			pot.maxAmountInPot = 1000;
			pot.associatedReaction = reaction;
		};
		row(PotInfo::FoodPot, PotType::Pot, ResourceType::Food, Reaction::ReactToFood);
		row(PotInfo::WoodPot, PotType::Pot, ResourceType::Wood, Reaction::ReactToWood);
		row(PotInfo::MagicFood, PotType::PileFood, ResourceType::Food, Reaction::ReactToFood);
		row(PotInfo::MagicWood, PotType::PileWood, ResourceType::Wood, Reaction::ReactToWood);
		row(PotInfo::FoodPile, PotType::PileFood, ResourceType::Food, Reaction::None);
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(TakeResourceTest, APileStartsItsRowsReactionOnceWithItsOwnerUntilTheReactionIsRemoved)
{
	// The original's pot reaction set-up: once (its flag), the type its row says, from the pile's player; none for a
	// row that starts none; taken away whole, then it may start again
	// (docs/wiki/beings/animal-life.md, docs/wiki/world/objects.md#pots-and-piles)
	const PileReactionRows rows;
	const auto wood = MakePot(PotInfo::MagicWood, 300);
	Reg().Get<ecs::components::Pot>(wood).owner = PlayerNames::PLAYER_TWO;
	ecs::animal_ai::SetupPotReaction(wood);
	const auto id = reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood);
	ASSERT_NE(id, 0u);
	ASSERT_NE(reactions::Find(id), nullptr);
	EXPECT_EQ(reactions::Find(id)->player, std::optional(PlayerNames::PLAYER_TWO));
	EXPECT_EQ(reactions::All().size(), 1u);
	// a second offer starts nothing more
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_EQ(reactions::All().size(), 1u);
	// a food pile starts the food reaction
	const auto food = MakePot(PotInfo::MagicFood, 150);
	ecs::animal_ai::SetupPotReaction(food);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(food, Reaction::ReactToFood), 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(food, Reaction::ReactToWood), 0u);
	EXPECT_EQ(reactions::All().size(), 2u);
	// a row that starts none
	const auto plain = MakePot(PotInfo::FoodPile, 100);
	ecs::animal_ai::SetupPotReaction(plain);
	EXPECT_EQ(reactions::All().size(), 2u);
	// removed (picked up, emptied), it may be offered again
	ecs::animal_ai::RemovePotReaction(wood);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood), 0u);
	EXPECT_EQ(reactions::All().size(), 1u);
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood), 0u);
	EXPECT_EQ(reactions::All().size(), 2u);
}

TEST_F(TakeResourceTest, APotsReactionFlagKeepsItOnceAndGuardsTheTakingAwayOfItsReactions)
{
	// The original's pot keeps its own flag: set as its reaction is offered, kept when that reaction ends some other
	// way, and cleared only when its reactions are taken away; a pot that never offered one keeps the others it
	// started (docs/wiki/world/objects.md#pots-and-piles)
	const PileReactionRows rows;
	using ecs::components::Pot;
	// a pile of a row that starts none keeps its flight's reaction when it is picked up
	const auto plain = MakePot(PotInfo::FoodPile, 100);
	ecs::animal_ai::SetupPotReaction(plain);
	EXPECT_FALSE(Reg().Get<const Pot>(plain).reactionStarted);
	ASSERT_NE(reactions::CreateReaction(plain, Reaction::ReactToFlyingObject, std::nullopt, false), 0u);
	ecs::animal_ai::RemovePotReaction(plain);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(plain, Reaction::ReactToFlyingObject), 0u);
	// a wood pile that offered its reaction loses every one it started
	const auto wood = MakePot(PotInfo::MagicWood, 300);
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_TRUE(Reg().Get<const Pot>(wood).reactionStarted);
	ASSERT_NE(reactions::CreateReaction(wood, Reaction::ReactToFlyingObject, std::nullopt, false), 0u);
	ecs::animal_ai::RemovePotReaction(wood);
	EXPECT_FALSE(Reg().Get<const Pot>(wood).reactionStarted);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood), 0u);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToFlyingObject), 0u);
	// its reaction ended some other way: the flag holds, so it is not offered again until it is taken away
	ecs::animal_ai::SetupPotReaction(wood);
	reactions::RemoveAllReactionsOfTypeInitiatedBy(wood, Reaction::ReactToWood);
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood), 0u);
	ecs::animal_ai::RemovePotReaction(wood);
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(wood, Reaction::ReactToWood), 0u);
}

TEST_F(TakeResourceTest, AnEmptiedLoosePileGoesButAnEmptiedPlainPotOrABuildingSitesPileStays)
{
	// The original's take from a pot: all of it takes its reaction and poison away; then a pile of no structure goes,
	// a plain pot and a structure's pile stay (docs/wiki/world/objects.md#pots-and-piles)
	const PileReactionRows rows;
	namespace resources = ecs::object_resources;
	using ecs::components::Pot;
	// a plain food pot: part of it leaves it as it was, all of it leaves it empty and clean, without its reaction
	const auto pot = MakePot(PotInfo::FoodPot, 40);
	Reg().Get<Pot>(pot).poisoned = true;
	ecs::animal_ai::SetupPotReaction(pot);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(pot, Reaction::ReactToFood), 0u);
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 15), 15u);
	EXPECT_EQ(Reg().Get<const Pot>(pot).amount, 25u);
	EXPECT_TRUE(Reg().Get<const Pot>(pot).poisoned);
	EXPECT_NE(reactions::GetReactionOfTypeInitiatedBy(pot, Reaction::ReactToFood), 0u);
	EXPECT_EQ(resources::RemoveResource(pot, ResourceType::Food, 60), 25u);
	ASSERT_TRUE(Reg().Valid(pot));
	EXPECT_EQ(Reg().Get<const Pot>(pot).amount, 0u);
	EXPECT_FALSE(Reg().Get<const Pot>(pot).poisoned);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(pot, Reaction::ReactToFood), 0u);
	// a loose magic wood pile: part of it stays, the rest goes with the pile
	const auto wood = MakePot(PotInfo::MagicWood, 300);
	ecs::animal_ai::SetupPotReaction(wood);
	EXPECT_EQ(resources::RemoveResource(wood, ResourceType::Wood, 100), 100u);
	ASSERT_TRUE(Reg().Valid(wood));
	EXPECT_EQ(Reg().Get<const Pot>(wood).amount, 200u);
	EXPECT_EQ(resources::RemoveResource(wood, ResourceType::Wood, 200), 200u);
	EXPECT_FALSE(Reg().Valid(wood));
	EXPECT_TRUE(reactions::All().empty());
	// a loose magic food pile goes the same way
	const auto food = MakePot(PotInfo::MagicFood, 50);
	EXPECT_EQ(resources::RemoveResource(food, ResourceType::Food, 80), 50u);
	EXPECT_FALSE(Reg().Valid(food));
	// a building site's wood pile is part of its site and stays
	const auto pile = MakePot(PotInfo::MagicWood, 30);
	const auto site = Reg().Create();
	Reg().Assign<ecs::components::BuildingSite>(site).woodPile = pile;
	EXPECT_EQ(resources::RemoveResource(pile, ResourceType::Wood, 30), 30u);
	ASSERT_TRUE(Reg().Valid(pile));
	EXPECT_EQ(Reg().Get<const Pot>(pile).amount, 0u);
	EXPECT_EQ(Reg().Get<const ecs::components::BuildingSite>(site).woodPile, pile);
}

TEST_F(TakeResourceTest, AGiftToATownOfAnotherPlayerTimesItsBeliefByTheNonOwnerMultiplier)
{
	// The town's belief from a giver who is not its owner is the want's drop x the non-owner multiplier x the town
	// multiplier; the giver's alignment is not multiplied (docs/wiki/world/buildings.md#resources-held-by-objects,
	// docs/wiki/world/objects.md#store-storagepit). info.dat's non-owner multiplier is 1: 2 here, so that it shows
	const GiftServices services(2.0f);
	CountedDraws rand;
	// two towns of the first player, alike, each wanting 4250 food and holding 1000
	const auto theirs = MakeHungryTown(16, 1000);
	const auto own = MakeHungryTown(17, 1000);
	// the second player gives back food it took itself: its own deed, so no draw
	const auto hand = DropperOf(PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_TWO);
	const float alignment = PendingAlignment(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(ecs::object_resources::AddResource(theirs.pit, ResourceType::Food, 1000, hand), 1000u);
	EXPECT_NEAR(PendingBelief(theirs.town, PlayerNames::PLAYER_TWO), k_WantDrop * 2.0f * 0.25f, 1e-6f);
	EXPECT_EQ(PendingBelief(theirs.town, PlayerNames::PLAYER_ONE), 0.0f);
	EXPECT_NEAR(PendingAlignment(PlayerNames::PLAYER_TWO) - alignment, k_WantDrop * 0.5f, 1e-6f);
	// the owner's own gift of the same is not multiplied
	EXPECT_EQ(ecs::object_resources::AddResource(own.pit, ResourceType::Food, 1000, OwnHand()), 1000u);
	EXPECT_NEAR(PendingBelief(own.town, PlayerNames::PLAYER_ONE), k_WantDrop * 0.25f, 1e-6f);
	EXPECT_EQ(rand.draws, 0u);
}

namespace
{
/// The storage pit piles' rows as the wiki gives them: a wood pile holds 5000, the last one with no cap, and the food
/// pile 15000. Put back when it goes
class StoreFillRows
{
public:
	static constexpr uint32_t k_WoodPileMost = 5000;
	static constexpr uint32_t k_FoodPileMost = 15000;

	StoreFillRows()
	{
		auto info = std::make_unique<InfoConstants>();
		for (size_t i = 0; i < 5; ++i)
		{
			info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1) + i).maxAmountInPot = k_WoodPileMost;
		}
		// a row whose next pot is 19 or more is not capped
		info->pot.at(static_cast<size_t>(PotInfo::WoodPile_1) + 4).nextPotForResource = static_cast<PotInfo>(19);
		info->pot.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).maxAmountInPot = k_FoodPileMost;
		Locator::infoConstants::reset(info.release());
	}

private:
	test::RestoreService<Locator::infoConstants> _info;
};

/// A storage pit of no town with its five empty wood piles and its empty food pile
entt::entity MakeEmptyPit()
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto pit = registry.Create();
	registry.Assign<ecs::components::Abode>(pit).townId = ecs::components::Abode::k_NoTown;
	std::array<entt::entity, 5> woodPiles {};
	for (size_t i = 0; i < woodPiles.size(); ++i)
	{
		const auto type = static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + static_cast<int32_t>(i));
		woodPiles.at(i) = MakePot(type, 0);
	}
	const auto foodPile = MakePot(PotInfo::StoragePitFoodPile, 0);
	registry.Assign<ecs::components::StoragePit>(pit, woodPiles, foodPile);
	return pit;
}

std::array<uint32_t, 5> WoodPileAmounts(entt::entity pit)
{
	const auto& piles = Locator::entitiesRegistry::value().Get<const ecs::components::StoragePit>(pit).woodPiles;
	std::array<uint32_t, 5> amounts {};
	std::ranges::transform(piles, amounts.begin(), AmountOf);
	return amounts;
}
} // namespace

TEST_F(TakeResourceTest, WoodGoesIntoAPitsPilesFromTheFirstAndComesOutFromTheLast)
{
	// Wood fills the piles 1 to 5, each up to its cap and the last with none, and is taken from 5 down to 1
	// (docs/wiki/world/objects.md#store-storagepit)
	const StoreFillRows rows;
	const auto pit = MakeEmptyPit();
	using Amounts = std::array<uint32_t, 5>;
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Wood, 12000), 12000u);
	EXPECT_EQ(WoodPileAmounts(pit), (Amounts {5000, 5000, 2000, 0, 0}));
	// the next wood fills the third pile, then the fourth, and the last takes all the rest
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Wood, 20000), 20000u);
	EXPECT_EQ(WoodPileAmounts(pit), (Amounts {5000, 5000, 5000, 5000, 12000}));
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Wood), 32000u);
	EXPECT_EQ(Reg().Get<const ecs::components::Abode>(pit).woodAmount, 32000u);
	// taking empties the last pile first, then the one before it
	EXPECT_EQ(StoragePitStore::RemoveResource(pit, ResourceType::Wood, 13000), 13000u);
	EXPECT_EQ(WoodPileAmounts(pit), (Amounts {5000, 5000, 5000, 4000, 0}));
	EXPECT_EQ(StoragePitStore::RemoveResource(pit, ResourceType::Wood, 2000), 2000u);
	EXPECT_EQ(WoodPileAmounts(pit), (Amounts {5000, 5000, 5000, 2000, 0}));
	// wood added again goes to the first pile with room
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Wood, 1000), 1000u);
	EXPECT_EQ(WoodPileAmounts(pit), (Amounts {5000, 5000, 5000, 3000, 0}));
	EXPECT_EQ(Reg().Get<const ecs::components::Abode>(pit).woodAmount, 18000u);
	// none of it is food
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Food), 0u);
}

TEST_F(TakeResourceTest, FoodPutInAPitGoesOnlyToItsFoodPile)
{
	// A storage pit's food goes to its food pile alone, and comes out of it alone; the wood piles are not touched
	// (docs/wiki/world/objects.md#store-storagepit)
	const StoreFillRows rows;
	const auto pit = MakeEmptyPit();
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Wood, 6000), 6000u);
	const auto wood = WoodPileAmounts(pit);
	const auto food = Reg().Get<const ecs::components::StoragePit>(pit).foodPile;
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Food, 700), 700u);
	EXPECT_EQ(AmountOf(food), 700u);
	EXPECT_EQ(WoodPileAmounts(pit), wood);
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Food), 700u);
	EXPECT_EQ(StoragePitStore::GetResource(pit, ResourceType::Wood), 6000u);
	EXPECT_EQ(Reg().Get<const ecs::components::Abode>(pit).foodAmount, 700u);
	// a full food pile takes no more: nothing overflows into the wood piles
	EXPECT_EQ(StoragePitStore::AddResource(pit, ResourceType::Food, 20000), StoreFillRows::k_FoodPileMost - 700);
	EXPECT_EQ(AmountOf(food), StoreFillRows::k_FoodPileMost);
	EXPECT_EQ(WoodPileAmounts(pit), wood);
	// food taken comes from the food pile only
	EXPECT_EQ(StoragePitStore::RemoveResource(pit, ResourceType::Food, 500), 500u);
	EXPECT_EQ(AmountOf(food), StoreFillRows::k_FoodPileMost - 500);
	EXPECT_EQ(WoodPileAmounts(pit), wood);
}

namespace
{
/// GET_RESOURCE as a script calls it, on top of a value of the script's own: the resource type is pushed first and the
/// thing last (the thing is popped first). The value it gives, which must be the only one it leaves, and its type
std::pair<lhvm::VMValue, lhvm::DataType> GetResourceNative(chlapi::CHLApi& api, uint32_t thing, ResourceType type)
{
	const auto& table = api.GetFunctionsTable();
	const auto isGetResource = [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "GET_RESOURCE"; };
	const auto native = std::ranges::find_if(table, isGetResource);
	EXPECT_NE(native, table.end());
	if (native == table.end())
	{
		return {};
	}
	constexpr int32_t k_ScriptsOwn = 7777;
	auto& vm = Locator::vm::value();
	vm.Pushi(k_ScriptsOwn);
	vm.Pushi(static_cast<int32_t>(type));
	vm.Pusho(thing);
	native->impl();
	lhvm::DataType given {};
	const auto value = vm.Pop(given);
	EXPECT_EQ(vm.Pop().intVal, k_ScriptsOwn) << "GET_RESOURCE left more than one value";
	return {value, given};
}
} // namespace

TEST_F(TakeResourceTest, GetResourceGivesTheObjectsOwnAmountOfTheKindAndZeroForAThingThatIsNoObject)
{
	// The thing, then the resource type; no thing or a thing that is not an object gives 0, else the object's own
	// amount of the kind as a float (docs/wiki/world/objects.md#own-resource and GET_RESOURCE on that page)
	const ObjectResourceRows rows;
	if (spdlog::get("scripting") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	const test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto amount = [&api](entt::entity thing, ResourceType type) {
		return GetResourceNative(api, static_cast<uint32_t>(thing), type);
	};
	// a script's object 0 is no thing, so the first entity is not one the test names
	static_cast<void>(Reg().Create());
	// a rock of scale 0.7935 holds 158 wood (the original's own answer) and no food
	const auto rock = MakeMobileStatic(MobileStaticInfo::Rock, 0.7935f);
	const auto [wood, type] = amount(rock, ResourceType::Wood);
	EXPECT_EQ(type, lhvm::DataType::Float);
	EXPECT_EQ(wood.floatVal, 158.0f);
	EXPECT_EQ(amount(rock, ResourceType::Food).first.floatVal, 0.0f);
	// a mushroom its row's food value, truncated, and no wood
	const auto mushroom = Sized(1.0f);
	Reg().Assign<ecs::components::MobileObject>(mushroom, MobileObjectInfo::Champi);
	EXPECT_EQ(amount(mushroom, ResourceType::Food).first.floatVal, 99.0f);
	EXPECT_EQ(amount(mushroom, ResourceType::Wood).first.floatVal, 0.0f);
	// a fish farm answers nothing of any kind
	EXPECT_EQ(amount(MakeFishFarm(entt::null), ResourceType::Food).first.floatVal, 0.0f);
	// a town and a script marker are not objects, and there is no thing 0
	const auto town = Reg().Create();
	Reg().Assign<ecs::components::Town>(town, 9u);
	const auto marker = Reg().Create();
	Reg().Assign<ecs::components::ScriptMarker>(marker);
	for (const auto thing : {town, marker})
	{
		const auto [none, noneType] = amount(thing, ResourceType::Food);
		EXPECT_EQ(noneType, lhvm::DataType::Float);
		EXPECT_EQ(none.floatVal, 0.0f);
	}
	EXPECT_EQ(GetResourceNative(api, 0, ResourceType::Wood).first.floatVal, 0.0f);
}

TEST_F(TakeResourceTest, APotThatGoesTakesTheReactionsItStartedWithItAtOnce)
{
	// As the original's pot leaves: before it goes, it takes away the reactions it started, so the villagers drawn by
	// them stop at once rather than at the next turn's sweep of reactions whose initiator went. Destroyed by an effect
	// (the blast, a fire) or deleted (docs/wiki/world/objects.md#pots-and-piles)
	const PileReactionRows rows;
	const auto byEffect = MakePot(PotInfo::MagicWood, 300);
	ecs::animal_ai::SetupPotReaction(byEffect);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(byEffect, Reaction::ReactToWood), 0u);
	ecs::fire::traits::DestroyedByEffect(byEffect);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(byEffect, Reaction::ReactToWood), 0u);
	EXPECT_TRUE(reactions::All().empty());

	const auto deleted = MakePot(PotInfo::MagicWood, 300);
	ecs::animal_ai::SetupPotReaction(deleted);
	ASSERT_NE(reactions::GetReactionOfTypeInitiatedBy(deleted, Reaction::ReactToWood), 0u);
	ecs::ToBeDeleted(deleted);
	EXPECT_EQ(reactions::GetReactionOfTypeInitiatedBy(deleted, Reaction::ReactToWood), 0u);
	EXPECT_TRUE(reactions::All().empty());
}
