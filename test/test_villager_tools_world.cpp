/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The tools villagers hold at their work, in a small world of the game's own systems and data with villagers made as
// the game makes them: a forester holds an axe from heading for the forest to chopping and puts it away when done, a
// fisherman a fishing rod on the way and while fishing, a farmer a spade on the way to the field and digging up the
// crop but nothing to sow. On synthetic data the states' rows carry the original's tools, set by hand; on the game's
// data they are info.dat's own. Every expected value is the original's
// (docs/wiki/project/progress/villager/tools_and_carried_items.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/VillagerAnimation.h"
#include "Enums.h"
#include "support/VillagerWorld.h"

using namespace openblack;
using ecs::villager::Index;
using enum VillagerStates;

namespace
{
const glm::vec2 k_At {1005.0f, 1205.0f};

class VillagerToolsWorld: public test::world::VillagerWorld
{
protected:
	void SetUp() override
	{
		VillagerWorld::SetUp();
		if (IsSkipped() || HasFatalFailure() || GameData())
		{
			return;
		}
		for (const auto type : {VillagerInfo::CelticForesterMale, VillagerInfo::CelticFishermanMale,
		                        VillagerInfo::CelticFarmerMale, VillagerInfo::CelticHousewifeFemale})
		{
			auto& row = Info().villager.at(static_cast<size_t>(type));
			row.life = 1.0f;
			row.lifeWhenCrawlsWounded = 0.15f;
			row.grownUpAge = 13;
		}
		// the states' rows: the tool each gives, and the places a walk ends at, which are final states
		auto& table = Info().villagerStateTable;
		const auto carries = [&table](VillagerStates state, CarriedObject tool) {
			table.at(static_cast<size_t>(state)).field0xdc = static_cast<int32_t>(tool);
		};
		for (const auto state : {ForesterMoveToForest, ForesterGotoForest, ForesterArrivesAtForest, ForesterChopsTree,
		                         ForesterChopsTreeForBuilding, ArrivesAtBigForest})
		{
			carries(state, CarriedObject::Axe);
		}
		carries(FishermanArrivesAtFishing, CarriedObject::FishingRod);
		carries(Fishing, CarriedObject::FishingRod);
		carries(FarmerArrivesAtFarm, CarriedObject::Spade);
		carries(FarmerDigsUpCrop, CarriedObject::Spade);
		for (const auto state : {FishermanArrivesAtFishing, FarmerArrivesAtFarm})
		{
			table.at(static_cast<size_t>(state)).isFinalState = 1;
		}
		carries(PutOutFireWithWater, CarriedObject::Bag);
		// the rows that hide the tool and the load
		for (const auto state : k_HoldNothing)
		{
			carries(state, CarriedObject::None);
		}
		for (const auto state : k_AtWorship)
		{
			carries(state, CarriedObject::None);
		}
		carries(GetWaterToPutOutFire, CarriedObject::None);
		carries(PutOutFireByBeating, CarriedObject::None);
		for (const auto type : {VillagerInfo::CelticForesterMale, VillagerInfo::CelticFishermanMale,
		                        VillagerInfo::CelticFarmerMale, VillagerInfo::CelticHousewifeFemale})
		{
			auto& row = Info().villager.at(static_cast<size_t>(type));
			row.minWoodToShowGraphic = 50;
			row.minFoodToShowGraphic = 100;
		}
		// the log a villager drops is a dead pine: its row's wood value, weight and tree type
		auto& pine = Info().tree.at(static_cast<size_t>(TreeInfo::Pine));
		pine.normal = MeshId::ObjectRockChalk;
		pine.woodValue = 100;
		pine.weight = 50.0f;
		pine.carriedType = CarriedTreeType::Evergreen;
		// taking wood holds nothing; at home the villager is hidden, and the love, birth and dinner states hold nothing
		carries(TakeWoodFromTree, CarriedObject::None);
		carries(TakeWoodFromPot, CarriedObject::None);
		for (const auto state : k_HiddenAtHome)
		{
			table.at(static_cast<size_t>(state)).animation = static_cast<uint32_t>(ecs::villager_animation::k_HiddenClip);
		}
		for (const auto state : k_EmptyHanded)
		{
			carries(state, CarriedObject::None);
		}
		// the big rock a log may fall on
		auto& rock = Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::RockChalk));
		rock.mobileType = static_cast<MobileStaticInfo>(2);
		rock.meshId = MeshId::ObjectRockChalk;
		rock.weight = 300.0f;
	}

	/// The four logs a villager can carry, by what it carries: plain wood and the three trees' branches
	static constexpr std::array k_Logs {std::pair {CarriedObject::Wood, MeshId::ObjectWoodInHand},
	                                    std::pair {CarriedObject::Tree_1, MeshId::ObjectBranchEvergreenInHand},
	                                    std::pair {CarriedObject::Tree_2, MeshId::ObjectBranchFruitInHand},
	                                    std::pair {CarriedObject::Tree_3, MeshId::ObjectBranchHardWoodInHand}};

	/// The carried logs' models into the cache: on synthetic data each a beam 3 m long
	void LoadLogs() const
	{
		for (const auto& [carried, model] : k_Logs)
		{
			LoadMesh(model, glm::vec3(-1.5f, -0.25f, -0.25f), glm::vec3(1.5f, 0.25f, 0.25f));
		}
	}

	/// The villager carrying this much wood, shown as that object
	static void CarriesWood(entt::entity villager, int16_t wood, CarriedObject shown)
	{
		Carries(villager, wood, 0);
		Registry().Get<ecs::components::SkeletalAnimation>(villager).carriedObject = static_cast<int32_t>(shown);
	}

	/// The villager lets its wood fall at that velocity, as the hand lets go of it to fly: the log it dropped
	[[nodiscard]] static entt::entity DropsWood(entt::entity villager, glm::vec3 velocity)
	{
		std::vector<entt::entity> before;
		Registry().Each<const ecs::components::DeadTree>(
		    [&before](entt::entity log, const ecs::components::DeadTree& /*dead*/) { before.push_back(log); });
		ecs::villager::CreateDroppedResource(villager, velocity, std::nullopt, glm::vec3(0.0f));
		entt::entity dropped = entt::null;
		Registry().Each<const ecs::components::DeadTree>([&](entt::entity log, const ecs::components::DeadTree& /*dead*/) {
			if (std::ranges::find(before, log) == before.end())
			{
				dropped = log;
			}
		});
		return dropped;
	}

	/// The states in which a villager holds nothing, whatever it carries
	static constexpr std::array k_HoldNothing {
	    DecideWhatToDo, VillagerGossips,  MournDeadPerson, InspectObject, PointAtDeadPerson,
	    ScaredStiff,    ConfusedReaction, ShowPoisoned,    OnFire,        FaintingReaction};
	/// The worship and the dances
	static constexpr std::array k_AtWorship {GotoWorshipSiteForWorship, WorshippingAtWorshipSite, DanceButNotWorship,
	                                         ArtifactDance};

	/// At home: sleeping, cooking, housework and eating
	static constexpr std::array k_HiddenAtHome {SleepingAtHome, HousewifeMakeDinner, HousewifeDoesHousework, EatFoodAtHome};
	/// Sleeping out, eating dinner, giving birth and making love
	static constexpr std::array k_EmptyHanded {SleepInTent, SitsDownToDinner, HousewifeGivingBirth, HavingSex, HavingSexAtHome};

	/// The villager carries this much wood and food
	static void Carries(entt::entity villager, int16_t wood, int16_t food)
	{
		auto& held = Registry().Get<ecs::components::Villager>(villager).resourceHeld;
		held.at(0) = food;
		held.at(1) = wood;
	}

	/// What the villager holds in that state (and, walking, on its way to that final state), with no load
	[[nodiscard]] static CarriedObject HeldIn(entt::entity villager, VillagerStates top, VillagerStates final = InvalidState)
	{
		ecs::villager::SetState(villager, Index::Top, top);
		ecs::villager::SetState(villager, Index::Final, final);
		return static_cast<CarriedObject>(
		    ecs::villager::SetStateCarriedObject(villager, static_cast<int32_t>(CarriedObject::None)));
	}
};
} // namespace

// A forester holds an axe from the moment it heads for the forest, while it looks for a tree and chops, and at a big
// forest; finishing puts it away
TEST_P(VillagerToolsWorld, AForesterHoldsAnAxeFromHeadingForTheForestToChoppingAndPutsItAwayWhenDone)
{
	const auto forester = Villager(VillagerInfo::CelticForesterMale, k_At);
	for (const auto state : {ForesterMoveToForest, ForesterGotoForest, ForesterArrivesAtForest, ForesterChopsTree,
	                         ForesterChopsTreeForBuilding, ArrivesAtBigForest})
	{
		EXPECT_EQ(HeldIn(forester, state), CarriedObject::Axe) << static_cast<int>(state);
	}
	EXPECT_EQ(HeldIn(forester, ForesterFinishedForestering), CarriedObject::None);
	EXPECT_EQ(HeldIn(forester, DecideWhatToDo), CarriedObject::None);
}

// A fisherman holds a fishing rod on the way to its fishing spot and while it fishes
TEST_P(VillagerToolsWorld, AFishermanHoldsAFishingRodOnTheWayAndWhileFishing)
{
	const auto fisherman = Villager(VillagerInfo::CelticFishermanMale, k_At);
	EXPECT_EQ(HeldIn(fisherman, MoveToPos, FishermanArrivesAtFishing), CarriedObject::FishingRod);
	EXPECT_EQ(HeldIn(fisherman, Fishing), CarriedObject::FishingRod);
	EXPECT_EQ(HeldIn(fisherman, DecideWhatToDo), CarriedObject::None);
}

// A farmer holds a spade on the way to the field and while it digs up the crop, but sows empty-handed
TEST_P(VillagerToolsWorld, AFarmerHoldsASpadeOnTheWayAndDiggingButSowsEmptyHanded)
{
	const auto farmer = Villager(VillagerInfo::CelticFarmerMale, k_At);
	EXPECT_EQ(HeldIn(farmer, MoveToPos, FarmerArrivesAtFarm), CarriedObject::Spade);
	EXPECT_EQ(HeldIn(farmer, FarmerDigsUpCrop), CarriedObject::Spade);
	EXPECT_EQ(HeldIn(farmer, FarmerPlantsCrop), CarriedObject::None);
}

// Fighting a fire with water a villager holds a bag while it throws the water; it fetches the water and beats the
// flames empty-handed
TEST_P(VillagerToolsWorld, AFireFighterHoldsABagOnlyToThrowTheWater)
{
	const auto fighter = Villager(VillagerInfo::CelticFarmerMale, k_At);
	EXPECT_EQ(HeldIn(fighter, PutOutFireWithWater), CarriedObject::Bag);
	EXPECT_EQ(HeldIn(fighter, GetWaterToPutOutFire), CarriedObject::None);
	EXPECT_EQ(HeldIn(fighter, PutOutFireByBeating), CarriedObject::None);
}

// No state of the table gives the scythe
TEST_P(VillagerToolsWorld, TheScytheIsNeverHeld)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the shipped state table's rows: no synthetic data stands for them";
	}
	for (size_t state = 0; state < Info().villagerStateTable.size(); ++state)
	{
		EXPECT_NE(Info().villagerStateTable.at(state).field0xdc, static_cast<int32_t>(CarriedObject::Scythe)) << state;
	}
}

// Walking home a villager shows its load: the wood when it has more than 50, even with more food, else the food's bag
// when it has more than 100
TEST_P(VillagerToolsWorld, MoreThanFiftyWoodShowsEvenWithMoreFood)
{
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	Carries(villager, 51, 200);
	EXPECT_EQ(HeldIn(villager, GoHome), CarriedObject::Wood);
	Carries(villager, 50, 200);
	EXPECT_EQ(HeldIn(villager, GoHome), CarriedObject::Bag);
	Carries(villager, 50, 100);
	EXPECT_EQ(HeldIn(villager, GoHome), CarriedObject::None);
}

// The tool and the load go while the villager decides what to do, gossips, mourns, inspects or points at something,
// is scared stiff, confused, poisoned, on fire or fainting
TEST_P(VillagerToolsWorld, TheToolAndTheLoadGoWhileItDecidesGossipsMournsOrIsUnwell)
{
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	Carries(villager, 200, 200);
	for (const auto state : k_HoldNothing)
	{
		EXPECT_EQ(HeldIn(villager, state), CarriedObject::None) << static_cast<int>(state);
	}
}

// At worship and at a dance the villager holds nothing, whatever it carries
TEST_P(VillagerToolsWorld, AtWorshipAndDancingItHoldsNothing)
{
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	Carries(villager, 200, 200);
	for (const auto state : k_AtWorship)
	{
		EXPECT_EQ(HeldIn(villager, state), CarriedObject::None) << static_cast<int>(state);
	}
}

// Walking home or panicking it keeps showing its load
TEST_P(VillagerToolsWorld, WalkingHomeOrPanickingItKeepsShowingItsLoad)
{
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	Carries(villager, 200, 0);
	EXPECT_EQ(HeldIn(villager, GoHome), CarriedObject::Wood);
	EXPECT_EQ(HeldIn(villager, PanicReaction), CarriedObject::Wood);
}

// Let go by the hand to fly, a villager lets its wood fall as a log that flies on beside it at the villager's speed, a
// dead pine worth the wood, and the villager has none left
TEST_P(VillagerToolsWorld, AFlyingVillagersWoodFallsAsALogFlyingOnBesideIt)
{
	LoadLogs();
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	CarriesWood(villager, 200, CarriedObject::Tree_3);
	const glm::vec3 velocity(4.0f, 6.0f, 1.0f);
	const auto log = DropsWood(villager, velocity);
	ASSERT_NE(log, entt::entity {entt::null});
	const auto* po = Dynamics().Find(log);
	ASSERT_NE(po, nullptr);
	EXPECT_EQ(po->body.velocity, velocity);
	const auto& dead = Registry().Get<const ecs::components::DeadTree>(log);
	EXPECT_EQ(dead.type, TreeInfo::Pine);
	EXPECT_EQ(dead.woodValueMultiplier,
	          200.0f / static_cast<float>(Info().tree.at(static_cast<size_t>(TreeInfo::Pine)).woodValue));
	EXPECT_EQ(Registry().Get<const ecs::components::Villager>(villager).resourceHeld.at(1), 0);
}

// The log flies without hitting other objects, so it hurts nothing and knocks nothing over
TEST_P(VillagerToolsWorld, TheDroppedLogHitsNoOtherObject)
{
	LoadLogs();
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	CarriesWood(villager, 200, CarriedObject::Tree_1);
	const auto log = DropsWood(villager, glm::vec3(4.0f, 6.0f, 1.0f));
	ASSERT_NE(log, entt::entity {entt::null});
	const auto* po = Dynamics().Find(log);
	ASSERT_NE(po, nullptr);
	EXPECT_NE(po->flags & ecs::PhysicsEntry::k_NoObjectCollision, 0u);
}

// The log is put on the ground where the villager is, and raised clear of anything it overlaps before it flies
TEST_P(VillagerToolsWorld, TheDroppedLogIsRaisedClearOfWhatItOverlaps)
{
	LoadLogs();
	const auto dropAt = [this](glm::vec2 at, bool rock) {
		if (rock)
		{
			LoadMesh(Info().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::RockChalk)).meshId,
			         glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.5f, 1.0f));
			static_cast<void>(ecs::archetypes::MobileStaticArchetype::CreateFromInfo(
			    glm::vec3(at.x, k_Ground, at.y), MobileStaticInfo::RockChalk, 0.0f, 0.0f, 3.0f));
		}
		const auto villager = Villager(VillagerInfo::CelticForesterMale, at);
		CarriesWood(villager, 200, CarriedObject::Tree_1);
		const auto log = DropsWood(villager, glm::vec3(0.0f, 1.0f, 0.0f));
		const auto* po = log != entt::null ? Dynamics().Find(log) : nullptr;
		return po != nullptr ? std::optional(po->body.Centre().y) : std::nullopt;
	};
	const auto clear = dropAt(k_At, false);
	const auto raised = dropAt(k_At + glm::vec2(100.0f, 0.0f), true);
	ASSERT_TRUE(clear.has_value());
	ASSERT_TRUE(raised.has_value());
	EXPECT_GT(*raised, *clear + 1.0f);
}

// A log picked up again gives back the tree type it was carried as: evergreen, fruit or hardwood; plain wood none
TEST_P(VillagerToolsWorld, ADroppedLogGivesBackItsTreeType)
{
	LoadLogs();
	constexpr std::array k_Types {CarriedTreeType::None, CarriedTreeType::Evergreen, CarriedTreeType::Fruit,
	                              CarriedTreeType::Hardwood};
	for (size_t i = 0; i < k_Logs.size(); ++i)
	{
		const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At + glm::vec2(20.0f * static_cast<float>(i), 0.0f));
		CarriesWood(villager, 200, k_Logs.at(i).first);
		const auto log = DropsWood(villager, glm::vec3(0.0f, 1.0f, 0.0f));
		ASSERT_NE(log, entt::entity {entt::null});
		EXPECT_EQ(ecs::TreeCarriedType(log), k_Types.at(i)) << i;
	}
}

// The trees' records give their logs: conifers, pines, cypresses and copses evergreen; beech, birch, cedar, oak,
// olive, palms, bushes, hedges and the burnt tree hardwood; no tree gives a fruit tree's log
TEST_P(VillagerToolsWorld, TheTreesGiveEvergreenOrHardwoodLogsAndNoneAFruitTreesLog)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the shipped tree records: no synthetic data stands for them";
	}
	using enum TreeInfo;
	const auto type = [this](TreeInfo tree) { return Info().tree.at(static_cast<size_t>(tree)).carriedType; };
	for (const auto tree : {Conifer, ConiferA, Pine, Cypress, CypressA, Copse, CopseA})
	{
		EXPECT_EQ(type(tree), CarriedTreeType::Evergreen) << static_cast<int>(tree);
	}
	for (const auto tree :
	     {Beech, Birch, Cedar, Oak, OakA, Olive, Palm, PalmA, PalmB, PalmC, Bush, BushA, BushB, Hedge, HedgeA, Burnt})
	{
		EXPECT_EQ(type(tree), CarriedTreeType::Hardwood) << static_cast<int>(tree);
	}
}

// Taking wood from a tree or a pot, a villager plays the chopping clip with nothing in its hands
TEST_P(VillagerToolsWorld, TakingWoodItChopsEmptyHanded)
{
	const auto villager = Villager(VillagerInfo::CelticForesterMale, k_At);
	for (const auto state : {TakeWoodFromTree, TakeWoodFromPot})
	{
		EXPECT_EQ(ecs::villager_animation::ClipChoiceOf(state), ecs::villager_animation::ClipChoice::Forestering)
		    << static_cast<int>(state);
		EXPECT_EQ(HeldIn(villager, state), CarriedObject::None) << static_cast<int>(state);
	}
}

// At home the villager is not drawn at all, sleeping, cooking, doing the housework or eating, so neither is what it
// holds; sleeping out, eating dinner, giving birth and making love it holds nothing
TEST_P(VillagerToolsWorld, AtHomeItIsHiddenAndAtLoveBirthOrDinnerItHoldsNothing)
{
	for (const auto state : k_HiddenAtHome)
	{
		EXPECT_TRUE(ecs::villager_animate::IsHiddenByState(state)) << static_cast<int>(state);
	}
	const auto villager = Villager(VillagerInfo::CelticHousewifeFemale, k_At);
	Carries(villager, 200, 200);
	for (const auto state : k_EmptyHanded)
	{
		EXPECT_EQ(HeldIn(villager, state), CarriedObject::None) << static_cast<int>(state);
	}
}

INSTANTIATE_TEST_SUITE_P(Data, VillagerToolsWorld,
                         ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
