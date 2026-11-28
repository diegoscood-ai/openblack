/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The dead list: an animal marked by ecs::ToBeDeleted with the deferral on. animal_ai::Forget takes it off its town
// and its flock; the walk tags and DownedVillager go at mark time (inferred: marking takes it off the living list);
// the AI's readers (detail::Available, ecs::IsAvailable) no longer take it; ProcessDeadList(drain) frees it.
// The villager side: villager::IsAvailable (not unavailable, and the final state is not DYING) is what a predator
// asks of its prey.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <memory>

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "ECS/AnimalAI.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerCreatureDance.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// info.dat's villager state table: 14 DYING and 17 DOWNED are final,
// 239 PAUSE is not
constexpr auto k_Dying = VillagerStates::Dying;
constexpr auto k_Downed = VillagerStates::Downed;
constexpr auto k_Pause = static_cast<VillagerStates>(239);
} // namespace

class VillagerDeadListTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		for (const auto state : {k_Dying, k_Downed})
		{
			info->villagerStateTable.at(static_cast<size_t>(state)).isFinalState = 1;
		}
		info->villagerStateTable.at(static_cast<size_t>(k_Pause)).isFinalState = 0;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}
	void TearDown() override
	{
		ecs::ProcessDeadList(true);
		ecs::SetDeferredDeletion(false);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// a villager in the map with TOP / FINAL states
	static entt::entity MakeVillager(VillagerStates top, VillagerStates final)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Villager>(e);
		registry.Assign<Transform>(e);
		auto& action = registry.Assign<LivingAction>(e, top, static_cast<uint16_t>(0));
		action.states.at(static_cast<size_t>(LivingAction::Index::Final)) = static_cast<uint8_t>(final);
		return e;
	}

	/// a lion hunting the target
	static entt::entity MakeLion(entt::entity target)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Animal>(e).type = AnimalInfo::Lion;
		registry.Assign<Transform>(e);
		registry.Assign<AnimalBrain>(e).target = target;
		return e;
	}

	/// an animal of a town, walking round an obstacle, with a DownedVillager (in the game that one is on the downed
	/// villager; here it only checks that Forget takes it)
	static entt::entity MakeAnimal(entt::entity town)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		auto& animal = registry.Assign<Animal>(e);
		animal.town = town;
		registry.Assign<Transform>(e);
		registry.Assign<DownedVillager>(e);
		registry.Assign<MoveStateLinearTag>(e);
		registry.Assign<WallHugObjectReference>(e, town);
		return e;
	}
};

TEST_F(VillagerDeadListTest, ForgetClearsTownWalkAndDowned)
{
	const auto town = Reg().Create();
	const auto e = MakeAnimal(town);
	ecs::animal_ai::Forget(e);
	EXPECT_EQ(Reg().Get<Animal>(e).town, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<Animal>(e).flock, entt::entity(entt::null));
	EXPECT_FALSE((Reg().AnyOf<DownedVillager, MoveStateLinearTag, WallHugObjectReference>(e)));
	EXPECT_TRUE(Reg().AllOf<Transform>(e));
}

// Forgetting a deleted animal also stops the reaction it follows, as the original's living deletion does: no reaction
// and no object of it left
TEST_F(VillagerDeadListTest, ForgetStopsTheReactionTheAnimalFollows)
{
	const auto town = Reg().Create();
	const auto e = MakeAnimal(town);
	auto& brain = Reg().AllOf<AnimalBrain>(e) ? Reg().Get<AnimalBrain>(e) : Reg().Assign<AnimalBrain>(e);
	brain.reaction = 7; // a reaction no longer in the list: nothing of its record to refresh
	brain.predator = town;
	ecs::animal_ai::Forget(e);
	EXPECT_EQ(Reg().Get<AnimalBrain>(e).reaction, 0u);
	EXPECT_EQ(Reg().Get<AnimalBrain>(e).predator, entt::entity(entt::null));
}

TEST_F(VillagerDeadListTest, ZombieIsNotAvailable)
{
	const auto town = Reg().Create();
	const auto e = MakeAnimal(town);
	EXPECT_TRUE(ecs::animal_ai::detail::Available(e));
	ecs::SetDeferredDeletion(true);
	ecs::ToBeDeleted(e);
	// marked: still in the registry (1-2 passes), unlinked, and no reader takes it
	ASSERT_TRUE(Reg().Valid(e));
	EXPECT_TRUE(Reg().AllOf<Unavailable>(e));
	EXPECT_FALSE(ecs::IsAvailable(e));
	EXPECT_FALSE(ecs::animal_ai::detail::Available(e));
	EXPECT_EQ(Reg().Get<Animal>(e).town, entt::entity(entt::null));
	EXPECT_FALSE((Reg().AnyOf<DownedVillager, MoveStateLinearTag, WallHugObjectReference>(e)));
	// a second ToBeDeleted is nothing; the drain frees it
	ecs::ToBeDeleted(e);
	ecs::ProcessDeadList(true);
	EXPECT_FALSE(Reg().Valid(e));
}

TEST_F(VillagerDeadListTest, VillagerZombieIsNotAvailable)
{
	const auto e = MakeVillager(k_Pause, VillagerStates::InvalidState);
	EXPECT_TRUE(ecs::villager::IsAvailable(e));
	EXPECT_TRUE(ecs::animal_ai::detail::Available(e));
	// marked by ToBeDeleted: villager::IsAvailable delegates to ecs::IsAvailable
	Reg().Assign<Unavailable>(e);
	EXPECT_FALSE(ecs::villager::IsAvailable(e));
	EXPECT_FALSE(ecs::animal_ai::detail::Available(e));
}

TEST_F(VillagerDeadListTest, LionTargetsByTheOriginalsAvailability)
{
	// a predator asks villager::IsAvailable: the final state (TOP if final, else FINAL) != DYING
	const auto downed = MakeVillager(k_Downed, k_Dying); // DOWNED is final: still prey
	const auto dying = MakeVillager(k_Dying, VillagerStates::InvalidState);
	const auto pausedDying = MakeVillager(k_Pause, k_Dying); // a passing TOP over FINAL DYING: no prey
	const auto noAction = Reg().Create();
	Reg().Assign<Villager>(noAction);
	Reg().Assign<Transform>(noAction);
	auto available = [](entt::entity villager) {
		return ecs::animal_ai::detail::Available(Reg().Get<AnimalBrain>(MakeLion(villager)).target);
	};
	EXPECT_TRUE(available(downed));
	EXPECT_FALSE(available(dying));
	EXPECT_FALSE(available(pausedDying));
	EXPECT_FALSE(available(noAction)); // no LivingAction: not available (no FINAL state to read)
}

TEST_F(VillagerDeadListTest, MarkedThingsAreFreedByTheSecondPass)
{
	// the game's dead list keeps ecs::DeadList's rules: a pass notes what was marked before it, the next frees it
	ecs::SetDeferredDeletion(true);
	const auto first = Reg().Create();
	const auto second = Reg().Create();
	Reg().Assign<Transform>(first);
	Reg().Assign<Transform>(second);
	ecs::ToBeDeleted(first);
	ecs::ToBeDeleted(second);
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(first));
	EXPECT_TRUE(Reg().Valid(second));
	// marked after the first pass: noted by the second, which frees the other two
	const auto late = Reg().Create();
	Reg().Assign<Transform>(late);
	ecs::ToBeDeleted(late);
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(first));
	EXPECT_FALSE(Reg().Valid(second));
	ASSERT_TRUE(Reg().Valid(late));
	EXPECT_TRUE(Reg().AllOf<Unavailable>(late));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(late));
}

TEST_F(VillagerDeadListTest, AnArtefactsObjectWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a town's artefact is a class whose deletion already waits, as the original's every thing's does: marked
	// unavailable, kept until the second pass after it, while a plain thing still goes at once
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto artefact = Reg().Create();
	const auto plain = Reg().Create();
	Reg().Assign<Transform>(artefact);
	Reg().Assign<Transform>(plain);
	Reg().Assign<TownArtefact>(artefact);
	ecs::ToBeDeleted(artefact);
	ecs::ToBeDeleted(plain);
	EXPECT_FALSE(Reg().Valid(plain));
	ASSERT_TRUE(Reg().Valid(artefact));
	EXPECT_TRUE(Reg().AllOf<Unavailable>(artefact));
	EXPECT_FALSE(ecs::IsAvailable(artefact));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(artefact));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(artefact));
	// deleted at once when asked so, as every thing
	const auto now = Reg().Create();
	Reg().Assign<Transform>(now);
	Reg().Assign<TownArtefact>(now);
	ecs::ToBeDeleted(now, true);
	EXPECT_FALSE(Reg().Valid(now));
}

TEST_F(VillagerDeadListTest, AnAnimatedStaticWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// an animated static is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto gate = Reg().Create();
	Reg().Assign<Transform>(gate);
	Reg().Assign<AnimatedStatic>(gate);
	ecs::ToBeDeleted(gate);
	ASSERT_TRUE(Reg().Valid(gate));
	EXPECT_FALSE(ecs::IsAvailable(gate));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(gate));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(gate));
}

TEST_F(VillagerDeadListTest, AFeatureWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a feature is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto monument = Reg().Create();
	Reg().Assign<Transform>(monument);
	Reg().Assign<Feature>(monument);
	ecs::ToBeDeleted(monument);
	ASSERT_TRUE(Reg().Valid(monument));
	EXPECT_FALSE(ecs::IsAvailable(monument));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(monument));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(monument));
}

TEST_F(VillagerDeadListTest, ARewardChestWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a reward chest is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto chest = Reg().Create();
	Reg().Assign<Transform>(chest);
	Reg().Assign<Reward>(chest);
	ecs::ToBeDeleted(chest);
	ASSERT_TRUE(Reg().Valid(chest));
	EXPECT_FALSE(ecs::IsAvailable(chest));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(chest));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(chest));
}

TEST_F(VillagerDeadListTest, AFootballWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a football is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto ball = Reg().Create();
	Reg().Assign<Transform>(ball);
	Reg().Assign<Ball>(ball);
	ecs::ToBeDeleted(ball);
	ASSERT_TRUE(Reg().Valid(ball));
	EXPECT_FALSE(ecs::IsAvailable(ball));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(ball));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(ball));
}

TEST_F(VillagerDeadListTest, AMobileStaticWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a mobile static is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto rock = Reg().Create();
	Reg().Assign<Transform>(rock);
	Reg().Assign<MobileStatic>(rock);
	ecs::ToBeDeleted(rock);
	ASSERT_TRUE(Reg().Valid(rock));
	EXPECT_FALSE(ecs::IsAvailable(rock));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(rock));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(rock));
}

TEST_F(VillagerDeadListTest, AnInfluenceRingWaitsOnTheDeadListOutOfTheRingsList)
{
	// an influence ring is a class whose deletion already waits: out of the game's rings at its mark, kept until the
	// second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto ring = influence::CreateRing(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE, 50.0f, 0);
	ASSERT_EQ(std::ranges::count(influence::Rings(), ring), 1);
	influence::DeleteRing(ring);
	ASSERT_TRUE(Reg().Valid(ring));
	EXPECT_FALSE(ecs::IsAvailable(ring));
	EXPECT_EQ(std::ranges::count(influence::Rings(), ring), 0);
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(ring));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(ring));
}

TEST_F(VillagerDeadListTest, AScriptHighlightWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a script's highlight is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto highlight = Reg().Create();
	Reg().Assign<Transform>(highlight);
	Reg().Assign<ScriptHighlight>(highlight);
	ecs::ToBeDeleted(highlight);
	ASSERT_TRUE(Reg().Valid(highlight));
	EXPECT_FALSE(ecs::IsAvailable(highlight));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(highlight));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(highlight));
}

TEST_F(VillagerDeadListTest, AFragmentWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a building's fragment is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto piece = Reg().Create();
	Reg().Assign<Transform>(piece);
	Reg().Assign<Fragment>(piece);
	ecs::ToBeDeleted(piece);
	ASSERT_TRUE(Reg().Valid(piece));
	EXPECT_FALSE(ecs::IsAvailable(piece));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(piece));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(piece));
}

TEST_F(VillagerDeadListTest, APotWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a pot or pile is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto pot = Reg().Create();
	Reg().Assign<Transform>(pot);
	Reg().Assign<Pot>(pot);
	ecs::ToBeDeleted(pot);
	ASSERT_TRUE(Reg().Valid(pot));
	EXPECT_FALSE(ecs::IsAvailable(pot));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(pot));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(pot));
}

TEST_F(VillagerDeadListTest, ADeadTreeWaitsOnTheDeadListWhileTheDeferralIsOffForTheRest)
{
	// a dead tree is a class whose deletion already waits: marked unavailable, kept until the second pass
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto stump = Reg().Create();
	Reg().Assign<Transform>(stump);
	Reg().Assign<DeadTree>(stump);
	ecs::ToBeDeleted(stump);
	ASSERT_TRUE(Reg().Valid(stump));
	EXPECT_FALSE(ecs::IsAvailable(stump));
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(stump));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(stump));
}

// A tree waits too: at its mark it leaves its forest (and so the game's tree list) and is unavailable, and the second
// pass destroys it; deleted at once, it goes at once
TEST_F(VillagerDeadListTest, ATreeLeavesItsForestAtItsMarkAndWaitsOnTheDeadList)
{
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto tree = Reg().Create();
	Reg().Assign<Transform>(tree);
	Reg().Assign<Tree>(tree).forestId = 3;
	ecs::ToBeDeleted(tree);
	ASSERT_TRUE(Reg().Valid(tree));
	EXPECT_FALSE(ecs::IsAvailable(tree));
	EXPECT_EQ(Reg().Get<Tree>(tree).forestId, 0u);
	ecs::ProcessDeadList(false);
	EXPECT_TRUE(Reg().Valid(tree));
	ecs::ProcessDeadList(false);
	EXPECT_FALSE(Reg().Valid(tree));

	const auto felled = Reg().Create();
	Reg().Assign<Transform>(felled);
	Reg().Assign<Tree>(felled).forestId = 3;
	ecs::ToBeDeleted(felled, true);
	EXPECT_FALSE(Reg().Valid(felled));
}

TEST_F(VillagerDeadListTest, WithTheDeferralOffThePassesFindNothing)
{
	// the game never turns the deferral on: a deletion destroys at once, nothing waits, and the turn's pass, the map's
	// clear and the close's drain leave every entity as it was
	EXPECT_FALSE(ecs::DeferredDeletion());
	const auto gone = Reg().Create();
	const auto kept = Reg().Create();
	Reg().Assign<Transform>(gone);
	Reg().Assign<Transform>(kept);
	ecs::ToBeDeleted(gone);
	EXPECT_FALSE(Reg().Valid(gone));
	const auto alive = Reg().Size<Transform>();
	ecs::ProcessDeadList(false);
	ecs::ProcessDeadList(false);
	ecs::ProcessDeadList(true);
	EXPECT_TRUE(Reg().Valid(kept));
	EXPECT_FALSE(Reg().AllOf<Unavailable>(kept));
	EXPECT_EQ(Reg().Size<Transform>(), alive);
}

// A creature that goes lets go of the villagers it called that are in the hand, worshipping it or under its control;
// the others go on as they are
TEST(CreatureDeletion, ItsCalledVillagersInItsHandItsDanceOrItsControlAreLetGo)
{
	using ecs::villager_creature_dance::ReleasedAsItsCreatureGoes;
	EXPECT_TRUE(ReleasedAsItsCreatureGoes(VillagerStates::InHand));
	EXPECT_TRUE(ReleasedAsItsCreatureGoes(VillagerStates::WorshippingCreature));
	EXPECT_TRUE(ReleasedAsItsCreatureGoes(VillagerStates::ControlledByCreature));
	EXPECT_FALSE(ReleasedAsItsCreatureGoes(VillagerStates::DecideWhatToDo));
	EXPECT_FALSE(ReleasedAsItsCreatureGoes(VillagerStates::Flying));
}

// A creature's own things go with it: its casting beams, its swap beam and its leash's mark are let go (the visuals
// deleted at once), and so is its held miracle
TEST_F(VillagerDeadListTest, ACreaturesBeamsSwapBeamAndLeashMarkGoWithIt)
{
	const auto creature = Reg().Create();
	auto& casting = Reg().Assign<CreatureCasting>(creature);
	casting.beams = {Reg().Create(), Reg().Create()};
	casting.gestureChain = 5;
	Reg().Assign<CreatureSwap>(creature).beam = Reg().Create();
	Reg().Assign<CreatureLeash>(creature).targetMarker = Reg().Create();
	ecs::DeleteCreaturesOwnThings(creature);
	EXPECT_EQ(Reg().Get<CreatureCasting>(creature).held, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<CreatureCasting>(creature).beams[0], entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<CreatureCasting>(creature).beams[1], entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<CreatureSwap>(creature).beam, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<CreatureLeash>(creature).targetMarker, entt::entity(entt::null));
	EXPECT_EQ(Reg().Get<CreatureCasting>(creature).gestureChain, 0u);
}
