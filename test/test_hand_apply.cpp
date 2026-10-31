/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A held object given to the object under the hand on the action press (ecs::held_apply): the decision by held class
// and target, then the giving on a fake registry: a storage pit with its six piles, loose piles and plain objects,
// with fake info rows. No game data and no hand: the hand's own part (leaving the hand) is a callback here.

#define LOCATOR_IMPLEMENTATIONS

#include <array>
#include <memory>
#include <span>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/HeldApply.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandToolTipRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
namespace held_apply = openblack::ecs::held_apply;
using held_apply::ApplyAction;
using held_apply::HeldKind;
using held_apply::TargetFacts;

TEST(HeldClassApply, ATreeIsGivenOnlyToAStoreOfWood)
{
	for (const auto kind : {HeldKind::Tree, HeldKind::DeadTree})
	{
		EXPECT_EQ(held_apply::HeldClassApply(kind, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
		EXPECT_EQ(held_apply::HeldClassApply(kind, TargetFacts {}), ApplyAction::None);
	}
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Other, TargetFacts {.storesHeldType = true}), ApplyAction::None);
}

TEST(HeldClassApply, APotIsGivenToAStoreOfItsTypeElseMergedIntoAPotOfItsType)
{
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
	// a pile of a store is both: the store first
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.storesHeldType = true, .sameTypePot = true}),
	          ApplyAction::GiveToStore);
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {.sameTypePot = true}), ApplyAction::MergeIntoPot);
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Pot, TargetFacts {}), ApplyAction::None);
}

TEST(HeldClassApply, AThrownPotGoesIntoWhatItHitsOnlyWhenBothAreAvailable)
{
	EXPECT_TRUE(held_apply::PotImpactTakes(true, true, TargetFacts {.storesHeldType = true}));
	EXPECT_TRUE(held_apply::PotImpactTakes(true, true, TargetFacts {.sameTypePot = true}));
	EXPECT_FALSE(held_apply::PotImpactTakes(true, true, TargetFacts {}));
	// a thrown tree goes into any store of wood it hits, not only a storage pit
	EXPECT_TRUE(held_apply::TreeImpactTakes(true, true));
	EXPECT_FALSE(held_apply::TreeImpactTakes(true, false));
	EXPECT_FALSE(held_apply::TreeImpactTakes(false, true));
	EXPECT_FALSE(held_apply::PotImpactTakes(false, true, TargetFacts {.storesHeldType = true}));
	EXPECT_FALSE(held_apply::PotImpactTakes(true, false, TargetFacts {.storesHeldType = true}));
}

TEST(HeldClassApply, AnAnimalIsGivenOnlyToAStoragePit)
{
	const TargetFacts pit {.storesHeldType = true, .storagePit = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Animal, pit));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, pit), ApplyAction::GiveToStore);
	// a worship site stores food, but the press gives an animal only to a pit
	const TargetFacts site {.storesHeldType = true};
	EXPECT_FALSE(held_apply::HeldClassValid(HeldKind::Animal, site));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, site), ApplyAction::None);
	// an indestructible animal over a pit: the press is valid, the apply does nothing
	const TargetFacts guarded {.storesHeldType = true, .storagePit = true, .heldIndestructible = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Animal, guarded));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Animal, guarded), ApplyAction::None);
}

TEST(HeldClassApply, AThrownAnimalGoesIntoAFoodStoreOnlyWhenBothAreAvailable)
{
	EXPECT_TRUE(held_apply::AnimalImpactTakes(true, true, true));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(true, true, false));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(false, true, true));
	EXPECT_FALSE(held_apply::AnimalImpactTakes(true, false, true));
}

TEST(HeldClassApply, AFenceIsGivenToAStoreOfWoodUnlessIndestructible)
{
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Fence, TargetFacts {.storesHeldType = true}));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Fence, TargetFacts {.storesHeldType = true}), ApplyAction::GiveToStore);
	EXPECT_FALSE(held_apply::HeldClassValid(HeldKind::Fence, TargetFacts {}));
	const TargetFacts guarded {.storesHeldType = true, .heldIndestructible = true};
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::Fence, guarded));
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Fence, guarded), ApplyAction::None);
}

TEST(HeldClassApply, AThrownFenceGoesIntoAWoodStoreUnlessIndestructible)
{
	EXPECT_TRUE(held_apply::FenceImpactTakes(true, false, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(false, false, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(true, true, true));
	EXPECT_FALSE(held_apply::FenceImpactTakes(true, false, false));
}

TEST(HeldClassApply, AGateStoneIsLaidOnlyInTheGateStonePlinth)
{
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::GateStone, TargetFacts {.gateStonePlinth = true}), ApplyAction::LayInPlinth);
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::GateStone, TargetFacts {}), ApplyAction::None);
	// a store of any kind doesn't take it
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::GateStone, TargetFacts {.storesHeldType = true}), ApplyAction::None);
	EXPECT_TRUE(held_apply::HeldClassValid(HeldKind::GateStone, TargetFacts {.gateStonePlinth = true}));
	EXPECT_FALSE(held_apply::HeldClassValid(HeldKind::GateStone, TargetFacts {}));
	// nothing else is laid in the plinth
	EXPECT_EQ(held_apply::HeldClassApply(HeldKind::Fence, TargetFacts {.gateStonePlinth = true}), ApplyAction::None);
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::LayInPlinth, true), held_apply::k_ApplyConsumed);
}

TEST(HeldClassApply, OnlyAnActionDoneConsumesTheObject)
{
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::GiveToStore, true), held_apply::k_ApplyConsumed);
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::GiveToStore, false), 0);
	EXPECT_EQ(held_apply::ApplyResult(ApplyAction::None, true), 0);
}

TEST(HeldClassApply, AnApplyToASacrificeAltarCountsSacrificeElseSupply)
{
	EXPECT_EQ(held_apply::ApplyHelpEvent(true), help_profile::Event::Sacrifice);
	EXPECT_EQ(held_apply::ApplyHelpEvent(false), help_profile::Event::Supply);
	EXPECT_EQ(static_cast<int>(held_apply::ApplyHelpEvent(true)), 7);
	EXPECT_EQ(static_cast<int>(held_apply::ApplyHelpEvent(false)), 6);
}

TEST(HeldClassApply, APointIsWithinTheBuildingSiteOfTheLastBuildingGoingUpWhoseShapeMeetsIt)
{
	using held_apply::SiteCandidate;
	// a building built and repaired is no site; one not built, or built and not repaired, is
	const SiteCandidate standing {.multiCell = true, .built = true, .repaired = true, .hasShape = true, .shapeTouches = true};
	const SiteCandidate goingUp {.multiCell = true, .built = false, .repaired = true, .hasShape = true, .shapeTouches = true};
	const SiteCandidate damaged {.multiCell = true, .built = true, .repaired = false, .hasShape = true, .shapeTouches = true};
	const SiteCandidate away {.multiCell = true, .built = false, .repaired = true, .hasShape = true, .shapeTouches = false};
	const SiteCandidate noShape {.multiCell = true, .built = false, .repaired = false, .hasShape = false};
	const SiteCandidate single {.multiCell = false, .built = false, .repaired = false, .hasShape = true, .shapeTouches = true};
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {standing}), std::nullopt);
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {goingUp}), 0u);
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {damaged}), 0u);
	// its shape must meet the point's circle, unless it has none
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {away}), std::nullopt);
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {noShape}), 0u);
	// only a building-like object counts
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {single}), std::nullopt);
	// the whole list is walked: the last one found wins
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::array {goingUp, standing, damaged, away}), 2u);
	EXPECT_EQ(held_apply::BuildingSiteAmong(std::span<const SiteCandidate> {}), std::nullopt);
	EXPECT_FLOAT_EQ(held_apply::k_BuildingSitePointRadius, 0.3f);
}

TEST(HeldClassApply, APotLetGoGoesToTheSiteUnderItOnlyWhenTheSiteStoresItsResource)
{
	EXPECT_TRUE(held_apply::ReleasedGoesToSite(true, true));
	EXPECT_FALSE(held_apply::ReleasedGoesToSite(true, false));
	EXPECT_FALSE(held_apply::ReleasedGoesToSite(false, true));
	EXPECT_FALSE(held_apply::ReleasedGoesToSite(false, false));
}

TEST(HeldClassApply, ATreeADeadTreeAPotAndAFenceLetGoLookForABuildingSiteAndNothingElseDoes)
{
	EXPECT_TRUE(held_apply::ReleaseLooksForBuildingSite(HeldKind::Tree));
	EXPECT_TRUE(held_apply::ReleaseLooksForBuildingSite(HeldKind::DeadTree));
	EXPECT_TRUE(held_apply::ReleaseLooksForBuildingSite(HeldKind::Pot));
	EXPECT_TRUE(held_apply::ReleaseLooksForBuildingSite(HeldKind::Fence));
	// an animal is only put down or thrown, and so is a gate stone or any other static object
	EXPECT_FALSE(held_apply::ReleaseLooksForBuildingSite(HeldKind::Animal));
	EXPECT_FALSE(held_apply::ReleaseLooksForBuildingSite(HeldKind::GateStone));
	EXPECT_FALSE(held_apply::ReleaseLooksForBuildingSite(HeldKind::Other));
}

namespace
{
constexpr auto k_TreeType = static_cast<TreeInfo>(3);
constexpr uint32_t k_TreeWood = 400;
constexpr auto k_CowType = static_cast<AnimalInfo>(1);
constexpr auto k_TortoiseType = static_cast<AnimalInfo>(2);

class HeldApplyTest: public ::testing::Test
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
		test::EmplaceWorldSystems();
		ecs::effects::reactions::Clear();
		auto info = std::make_unique<InfoConstants>();
		for (auto& row : info->pot)
		{
			row.resourceType = ResourceType::None;
			row.nextPotForResource = static_cast<PotInfo>(19); // no cap
		}
		const auto set = [&info](PotInfo pot, ResourceType type, PotType potType) {
			auto& row = info->pot.at(static_cast<size_t>(pot));
			row.resourceType = type;
			row.potType = potType;
		};
		set(PotInfo::StoragePitFoodPile, ResourceType::Food, PotType::PileFood);
		for (int i = 0; i < 5; ++i)
		{
			set(static_cast<PotInfo>(static_cast<int>(PotInfo::WoodPile_1) + i), ResourceType::Wood, PotType::PileWood);
		}
		set(PotInfo::MagicWood, ResourceType::Wood, PotType::PileWood);
		set(PotInfo::HandWood, ResourceType::Wood, PotType::Pot);
		set(PotInfo::HandFood, ResourceType::Food, PotType::Pot);
		set(PotInfo::MagicFood, ResourceType::Food, PotType::PileFood);
		info->tree.at(static_cast<size_t>(k_TreeType)).woodValue = k_TreeWood;
		auto& cow = info->animal.at(static_cast<size_t>(k_CowType));
		cow.foodValue = 1200.0f;
		cow.foodType = FoodType::Meat;
		auto& tortoise = info->animal.at(static_cast<size_t>(k_TortoiseType));
		tortoise.foodValue = 300.0f;
		tortoise.foodType = FoodType::Graze;
		auto& fence = info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::CeltFenceShort));
		fence.meshId = MeshId::BuildingCelticFenceShort;
		fence.woodValue = 25;
		info->mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).meshId = static_cast<MeshId>(0);
		Locator::infoConstants::reset(info.release());
	}
	void TearDown() override
	{
		ecs::effects::reactions::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakePot(PotInfo type, uint16_t amount = 0)
	{
		const auto pot = Reg().Create();
		auto& component = Reg().Assign<ecs::components::Pot>(pot);
		component.type = type;
		component.amount = amount;
		Reg().Assign<ecs::components::Transform>(pot, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return pot;
	}

	/// A storage pit of no town with its five wood piles and its food pile
	static entt::entity MakePit()
	{
		const auto pit = Reg().Create();
		Reg().Assign<ecs::components::Abode>(pit);
		Reg().Assign<ecs::components::Transform>(pit, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		auto& store = Reg().Assign<ecs::components::StoragePit>(pit);
		for (int i = 0; i < 5; ++i)
		{
			store.woodPiles.at(static_cast<size_t>(i)) =
			    MakePot(static_cast<PotInfo>(static_cast<int>(PotInfo::WoodPile_1) + i));
		}
		store.foodPile = MakePot(PotInfo::StoragePitFoodPile);
		return pit;
	}

	/// A dead tree of the fake row, at scale 1: worth k_TreeWood
	static entt::entity MakeDeadTree()
	{
		const auto tree = Reg().Create();
		Reg().Assign<ecs::components::DeadTree>(tree, ecs::components::DeadTree {k_TreeType});
		Reg().Assign<ecs::components::Transform>(tree, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return tree;
	}

	static entt::entity MakeAnimal(AnimalInfo type)
	{
		const auto animal = Reg().Create();
		Reg().Assign<ecs::components::Animal>(animal).type = type;
		Reg().Assign<ecs::components::Transform>(animal, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return animal;
	}

	static entt::entity MakeStatic(MobileStaticInfo type, float scale = 1.0f)
	{
		const auto object = Reg().Create();
		Reg().Assign<ecs::components::MobileStatic>(object).type = type;
		Reg().Assign<ecs::components::Transform>(object, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(scale));
		return object;
	}

	static ecs::components::StoragePit& Pit(entt::entity pit) { return Reg().Get<ecs::components::StoragePit>(pit); }

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(HeldApplyTest, ATreeOverAPitGoesIntoItsWood)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	EXPECT_EQ(held_apply::KindOfHeld(tree), HeldKind::DeadTree);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(tree, pit));
	int left = 0;
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pit, hand, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(Pit(pit).woodPiles.at(0)).amount, k_TreeWood); // pile 1 first
	EXPECT_FALSE(Reg().Valid(tree));
	// the pit's reaction to the hand putting something in it
	EXPECT_NE(ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit), 0u);
}

TEST_F(HeldApplyTest, TheHandOverAStoresWoodPileShowsTheStoresWholeWood)
{
	// The original's "Wood Amount" over a pile reads the pile's resource, and a store's pile answers its store's whole
	// total, not what the pile itself holds (docs/wiki/interface/tooltips.md)
	const auto pit = MakePit();
	const auto pot = MakePot(PotInfo::HandWood, 7000);
	ASSERT_EQ(held_apply::ApplyThisToObject(pot, pit, {}, []() {}), held_apply::k_ApplyConsumed);
	ASSERT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 7000u);
	for (const auto pile : Pit(pit).woodPiles)
	{
		const auto tip =
		    ecs::systems::hand_tooltips::PileToolTip(false, ecs::object_resources::GetResource(pile, ResourceType::Wood));
		EXPECT_EQ(tip.text, 0xEFEu);
		ASSERT_EQ(tip.numbers.count, 1u);
		EXPECT_EQ(tip.numbers.values[0], 7000.0);
	}
	// a loose pile shows its own
	const auto loose = MakePot(PotInfo::MagicWood, 300);
	EXPECT_EQ(ecs::systems::hand_tooltips::PileToolTip(false, ecs::object_resources::GetResource(loose, ResourceType::Wood))
	              .numbers.values[0],
	          300.0);
}

TEST_F(HeldApplyTest, ATreeOverAPitsWoodPileGoesIntoThePit)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	const auto pile = Pit(pit).woodPiles.at(3);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(tree, pile));
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pile, {}, {}), held_apply::k_ApplyConsumed);
	// the pile hands it to its pit, which fills from pile 1
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(Pit(pit).woodPiles.at(0)).amount, k_TreeWood);
	EXPECT_FALSE(Reg().Valid(tree));
}

TEST_F(HeldApplyTest, ATreeOverAFoodPileALoosePileOrNothingIsNotGiven)
{
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	const auto loose = MakePot(PotInfo::MagicWood, 50);
	const auto plain = Reg().Create();
	for (const auto target : {Pit(pit).foodPile, loose, plain, entt::entity {entt::null}})
	{
		EXPECT_FALSE(held_apply::ValidToApplyThisToObject(tree, target));
		int left = 0;
		EXPECT_EQ(held_apply::ApplyThisToObject(tree, target, {}, [&left]() { ++left; }), 0);
		EXPECT_EQ(left, 0); // still held
	}
	EXPECT_TRUE(Reg().Valid(tree));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 0u);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(loose).amount, 50u);
}

TEST_F(HeldApplyTest, AHandPotOfWoodOverAPitGoesWholeIntoItsTotal)
{
	const auto pit = MakePit();
	const auto pot = MakePot(PotInfo::HandWood, 7000);
	EXPECT_EQ(held_apply::KindOfHeld(pot), HeldKind::Pot);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(pot, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(pot, pit, {}, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	// the whole amount joins the pit's single total; no pile is made beside it
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 7000u);
	EXPECT_FALSE(Reg().Valid(pot));
	size_t pots = 0;
	Reg().Each<const ecs::components::Pot>([&pots](entt::entity, const ecs::components::Pot&) { ++pots; });
	EXPECT_EQ(pots, 6u); // the pit's six piles only
}

TEST_F(HeldApplyTest, AHandPotOfFoodOverAPitsWoodPileIsNotGiven)
{
	const auto pit = MakePit();
	const auto pot = MakePot(PotInfo::HandFood, 300);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(pot, Pit(pit).woodPiles.at(0)));
	// over its food pile it is
	EXPECT_TRUE(held_apply::ValidToApplyThisToObject(pot, Pit(pit).foodPile));
}

TEST_F(HeldApplyTest, AHandPotOfWoodOverALooseWoodPileMergesIntoIt)
{
	const auto pile = MakePot(PotInfo::MagicWood, 100);
	ecs::map_cells::InsertMapObject(pile);
	const auto pot = MakePot(PotInfo::HandWood, 250); // at the pile's own point
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(pot, pile));
	EXPECT_EQ(held_apply::ApplyThisToObject(pot, pile, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(Reg().Get<ecs::components::Pot>(pile).amount, 350u);
	EXPECT_FALSE(Reg().Valid(pot));
	// a loose pile of food is not a pot of its type
	const auto food = MakePot(PotInfo::MagicFood, 10);
	const auto other = MakePot(PotInfo::HandWood, 5);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(other, food));
}

TEST_F(HeldApplyTest, AnAnimalThrownIntoAPitIsItsFoodWithTheThrowersPlayer)
{
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	const ecs::pot_resource::Dropper thrower {true, PlayerNames::PLAYER_TWO, false};
	EXPECT_TRUE(ecs::resource_stores::IsResourceStore(pit, ResourceType::Food));
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, cow, thrower));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
	EXPECT_FALSE(Reg().Valid(cow));
	const auto id = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(ecs::effects::reactions::Find(id)->player, PlayerNames::PLAYER_TWO);
}

TEST_F(HeldApplyTest, AnAnimalThrownWithNoInterfaceHasNoPlayer)
{
	// no interface: the pit's reaction has no player (the original's null, not the neutral player)
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, cow, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
	const auto id = ecs::effects::reactions::GetReactionOfTypeInitiatedBy(pit, Reaction::ReactToHandPuttingStuffInStoragePit);
	ASSERT_NE(id, 0u);
	EXPECT_EQ(ecs::effects::reactions::Find(id)->player, std::nullopt);
}

TEST_F(HeldApplyTest, AnAnimalWorthNoFoodIsStillTakenAndGoes)
{
	const auto pit = MakePit();
	const auto tortoise = MakeAnimal(k_TortoiseType); // a grazer only: no food value
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, tortoise, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 0u);
	EXPECT_FALSE(Reg().Valid(tortoise));
}

TEST_F(HeldApplyTest, AHeldAnimalIsGivenOnlyToAPitAndNotWhenIndestructible)
{
	const auto pit = MakePit();
	const auto cow = MakeAnimal(k_CowType);
	EXPECT_EQ(held_apply::KindOfHeld(cow), HeldKind::Animal);
	// not to the pit's food pile: only to the pit itself
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(cow, Pit(pit).foodPile));
	Reg().Assign<ecs::components::Indestructible>(cow);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(cow, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(cow, pit, {}, [&left]() { ++left; }), 0);
	EXPECT_EQ(left, 0);
	EXPECT_TRUE(Reg().Valid(cow)); // still held
	Reg().Remove<ecs::components::Indestructible>(cow);
	EXPECT_EQ(held_apply::ApplyThisToObject(cow, pit, {}, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 1200u);
}

TEST_F(HeldApplyTest, ACelticFenceOverAPitIsItsWood)
{
	const auto pit = MakePit();
	const auto fence = MakeStatic(MobileStaticInfo::CeltFenceShort);
	EXPECT_EQ(held_apply::KindOfHeld(fence), HeldKind::Fence);
	ASSERT_TRUE(held_apply::ValidToApplyThisToObject(fence, pit));
	EXPECT_EQ(held_apply::ApplyThisToObject(fence, pit, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 25u);
	EXPECT_FALSE(Reg().Valid(fence));
	// scaled 0.9: 25 x 0.9^3 = 18.2
	const auto small = MakeStatic(MobileStaticInfo::CeltFenceShort, 0.9f);
	EXPECT_TRUE(ecs::resource_stores::DeleteObjectAndTakeResource(pit, small, {}));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 43u);
}

TEST_F(HeldApplyTest, AnotherStaticObjectOrAnIndestructibleFenceIsNotGiven)
{
	const auto pit = MakePit();
	const auto rock = MakeStatic(MobileStaticInfo::Rock);
	EXPECT_EQ(held_apply::KindOfHeld(rock), HeldKind::Other);
	EXPECT_FALSE(held_apply::ValidToApplyThisToObject(rock, pit));
	const auto fence = MakeStatic(MobileStaticInfo::CeltFenceShort);
	Reg().Assign<ecs::components::Indestructible>(fence);
	EXPECT_TRUE(held_apply::ValidToApplyThisToObject(fence, pit));
	int left = 0;
	EXPECT_EQ(held_apply::ApplyThisToObject(fence, pit, {}, [&left]() { ++left; }), 0);
	EXPECT_EQ(left, 0);
	EXPECT_TRUE(Reg().Valid(fence));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 0u);
}

TEST_F(HeldApplyTest, ATakenObjectLeavesAGhostForHalfASecond)
{
	ecs::object_ghosts::Clear();
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	Reg().Assign<ecs::components::Mesh>(tree, entt::id_type {77}, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pit, {}, {}), held_apply::k_ApplyConsumed);
	ASSERT_EQ(ecs::object_ghosts::Count(), 1u);
	const auto draws = ecs::object_ghosts::Instances();
	ASSERT_EQ(draws.size(), 2u);
	// the first draw only the depth through the mask at the moving offset at t = 500, every pixel passing (its
	// ALPHAREF 0); the second the model's own materials at no offset, cut off at 10, where that depth was written; both
	// at the tree's place
	EXPECT_EQ(draws[0].meshId, 77u);
	EXPECT_EQ(draws[0].uv, graphics::frame_anim::GhostFrame(500.0f).uv);
	EXPECT_EQ(draws[0].mode, graphics::render_modes::Mode::ChromaDepthOnly);
	EXPECT_EQ(draws[0].texture, ecs::object_ghosts::MaskTexture());
	EXPECT_EQ(draws[0].forcedAlphaRef, 0);
	EXPECT_FALSE(draws[0].depthEqual);
	EXPECT_EQ(draws[1].uv, glm::vec2(0.0f));
	EXPECT_FALSE(draws[1].mode.has_value());
	EXPECT_FALSE(draws[1].texture.has_value());
	EXPECT_EQ(draws[1].forcedAlphaRef, 10);
	EXPECT_TRUE(draws[1].depthEqual);
	EXPECT_EQ(glm::vec3(draws[0].model[3]), glm::vec3(10.0f, 5.0f, 10.0f));
	// it melts with the frames and is gone after 500 ms
	ecs::object_ghosts::Update(250.0f);
	EXPECT_EQ(ecs::object_ghosts::Count(), 1u);
	EXPECT_EQ(ecs::object_ghosts::Instances()[0].uv, graphics::frame_anim::GhostFrame(250.0f).uv);
	// half its time gone, the mask lets half its range through: 255 - truncated(255 x 250 / 500)
	EXPECT_EQ(ecs::object_ghosts::Instances()[0].forcedAlphaRef, 128);
	ecs::object_ghosts::Update(250.0f);
	EXPECT_EQ(ecs::object_ghosts::Count(), 0u);
	// an object without a mesh leaves none
	const auto bare = MakeDeadTree();
	EXPECT_EQ(held_apply::ApplyThisToObject(bare, pit, {}, {}), held_apply::k_ApplyConsumed);
	EXPECT_EQ(ecs::object_ghosts::Count(), 0u);
}

TEST_F(HeldApplyTest, ATakenObjectsGhostIsWhereTheHandDrewIt)
{
	ecs::object_ghosts::Clear();
	const auto pit = MakePit();
	const auto tree = MakeDeadTree();
	Reg().Assign<ecs::components::Mesh>(tree, entt::id_type {77}, static_cast<int8_t>(0), static_cast<int8_t>(-1));
	// the hand draws it below itself; letting go (before the store takes it) drops that pose
	Reg().Assign<ecs::components::HandDrawPose>(tree, glm::vec3(3.0f, 20.0f, 4.0f));
	const auto leaveHand = [tree]() { Reg().Remove<ecs::components::HandDrawPose>(tree); };
	EXPECT_EQ(held_apply::ApplyThisToObject(tree, pit, {}, leaveHand), held_apply::k_ApplyConsumed);
	ASSERT_EQ(ecs::object_ghosts::Count(), 1u);
	EXPECT_EQ(glm::vec3(ecs::object_ghosts::Instances()[0].model[3]), glm::vec3(3.0f, 20.0f, 4.0f));
	ecs::object_ghosts::Clear();
}

TEST_F(HeldApplyTest, OnlyAWorshipTotemIsASacrificeAltar)
{
	const auto totem = Reg().Create();
	Reg().Assign<ecs::components::WorshipTotem>(totem);
	EXPECT_TRUE(held_apply::IsSacrificeAltar(totem));
	// a store or anything else is not
	EXPECT_FALSE(held_apply::IsSacrificeAltar(MakePit()));
	EXPECT_FALSE(held_apply::IsSacrificeAltar(Reg().Create()));
}

TEST_F(HeldApplyTest, AHandPotLetGoOverAPitGoingUpGoesIntoItAndNotOverABuiltOne)
{
	// a pit still going up, in the map cells (no mesh, so no collide data: any point of its cell is within it)
	const auto pit = MakePit();
	auto& abode = Reg().Get<ecs::components::Abode>(pit);
	abode.buildFlags = ecs::components::Abode::k_UnderConstruction;
	abode.percentBuilt = 0.5f;
	ecs::map_cells::InsertMapObject(pit);
	ASSERT_EQ(held_apply::BuildingSiteAt(glm::vec3(10.0f, 0.0f, 10.0f)), pit);
	const auto pot = MakePot(PotInfo::HandWood, 700);
	int left = 0;
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	EXPECT_EQ(held_apply::BuildingSiteTakingReleased(pot), pit);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(pot, hand, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 700u);
	EXPECT_FALSE(Reg().Valid(pot));

	// built (and repaired), it is no building site: the pot is left to be put down or thrown
	abode.buildFlags = ecs::components::Abode::k_Built;
	abode.percentBuilt = 1.0f;
	EXPECT_EQ(held_apply::BuildingSiteAt(glm::vec3(10.0f, 0.0f, 10.0f)), entt::entity {entt::null});
	const auto other = MakePot(PotInfo::HandWood, 300);
	EXPECT_TRUE(held_apply::BuildingSiteTakingReleased(other) == entt::null);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(other, hand, [&left]() { ++left; }), 0);
	EXPECT_EQ(left, 1);
	EXPECT_TRUE(Reg().Valid(other));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), 700u);
	// nor is a dead tree over it taken
	const auto tree = MakeDeadTree();
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(tree, hand, {}), 0);
	EXPECT_TRUE(Reg().Valid(tree));
}

TEST_F(HeldApplyTest, ATreeADeadTreeOrAFenceLetGoOverAPitGoingUpGoesIntoItAsWood)
{
	const auto pit = MakePit();
	auto& abode = Reg().Get<ecs::components::Abode>(pit);
	abode.buildFlags = ecs::components::Abode::k_UnderConstruction;
	abode.percentBuilt = 0.5f;
	ecs::map_cells::InsertMapObject(pit);
	const ecs::pot_resource::Dropper hand {true, PlayerNames::PLAYER_ONE, true};
	int left = 0;

	// a dead tree: its wood (k_TreeWood at scale 1), out of the hand first
	const auto deadTree = MakeDeadTree();
	EXPECT_EQ(held_apply::BuildingSiteTakingReleased(deadTree), pit);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(deadTree, hand, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 1);
	EXPECT_FALSE(Reg().Valid(deadTree));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood);

	// a living tree over it is taken by it too (its wood is the press's, tested there)
	const auto tree = Reg().Create();
	Reg().Assign<ecs::components::Tree>(tree, ecs::components::Tree {.type = k_TreeType, .maxSize = 1.0f});
	Reg().Assign<ecs::components::Transform>(tree, glm::vec3(10.0f, 5.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_EQ(held_apply::KindOfHeld(tree), HeldKind::Tree);
	EXPECT_EQ(held_apply::BuildingSiteTakingReleased(tree), pit);

	// a fence: its wood (25 at scale 1), an indestructible one too, as the release asks nothing about it
	const auto fence = MakeStatic(MobileStaticInfo::CeltFenceShort);
	Reg().Assign<ecs::components::Indestructible>(fence);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(fence, hand, [&left]() { ++left; }), held_apply::k_ApplyConsumed);
	EXPECT_EQ(left, 2);
	EXPECT_FALSE(Reg().Valid(fence));
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Wood), k_TreeWood + 25u);

	// another static object, or an animal though the pit stores food, is left to be put down or thrown
	const auto rock = MakeStatic(MobileStaticInfo::Rock);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(rock, hand, [&left]() { ++left; }), 0);
	EXPECT_TRUE(Reg().Valid(rock));
	const auto cow = MakeAnimal(k_CowType);
	EXPECT_TRUE(held_apply::BuildingSiteTakingReleased(cow) == entt::null);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(cow, hand, [&left]() { ++left; }), 0);
	EXPECT_TRUE(Reg().Valid(cow));
	EXPECT_EQ(left, 2);
	EXPECT_EQ(ecs::StoragePitStore::GetResource(pit, ResourceType::Food), 0u);

	// the site is looked for under the object's own place: a dead tree away from the pit's cell stays
	const auto away = MakeDeadTree();
	Reg().Get<ecs::components::Transform>(away).position = glm::vec3(200.0f, 5.0f, 200.0f);
	EXPECT_EQ(held_apply::ApplyHeldToMapCoord(away, hand, [&left]() { ++left; }), 0);
	EXPECT_TRUE(Reg().Valid(away));
	EXPECT_EQ(left, 2);
}
