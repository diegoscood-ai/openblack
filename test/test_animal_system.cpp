/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The questions the animal service answers about one animal or flock, from the animal's components. The system is
// made here and asked directly; the locator only holds the fake registry and info constants it reads. And an animal the
// hand takes from its flock, which gets one of its own whose home moves where it lands.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <initializer_list>
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
#include "ECS/Components/Forest.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AnimalSystem.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "creature/CreatureSystemFakes.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::AnimalSystem;

namespace
{
class AnimalSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		auto info = std::make_unique<InfoConstants>();
		info->animal.at(static_cast<size_t>(AnimalInfo::Sheep)).playerCanPickUp = 1;
		info->animal.at(static_cast<size_t>(AnimalInfo::Lion)).playerCanPickUp = 0;
		// flags the game's own state table gives the states the exit tests use: final (IN_SCRIPT, IN_DANCE,
		// MOVE_IN_FLOCK, GOTO_FOOD_REACTION, EAT), script states (IN_SCRIPT, IN_DANCE, MOVE_IN_FLOCK) and two of the
		// states a script may interrupt (IN_SCRIPT, SET_DYING; the game's table marks IN_HAND too, left out here so
		// that the hand is let in on its own)
		for (const size_t state : {4u, 5u, 27u, 19u, 33u})
		{
			info->animalStateTable.at(state).field0xc = 1;
		}
		for (const size_t state : {4u, 5u, 27u})
		{
			info->animalStateTable.at(state).field0x14 = 1;
		}
		for (const size_t state : {4u, 13u})
		{
			info->animalStateTable.at(state).field0x18 = 1;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
	}

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity MakeAnimal(AnimalInfo type)
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		registry.Assign<Animal>(entity, type, 0u);
		registry.Assign<Transform>(entity, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	AnimalSystem _system;
	const test::RestoreService<Locator::infoConstants> _restoreInfo;
};
} // namespace

TEST_F(AnimalSystemTest, BatsVulturesBigCatsAndWolvesFrightenCreatures)
{
	// the species that frighten a creature, as the original's classes answer; every other one does not, the puzzle lion
	// and wolf among them
	constexpr std::array k_Frightening {AnimalInfo::Lion, AnimalInfo::Tiger,   AnimalInfo::Wolf,     AnimalInfo::Leopard,
	                                    AnimalInfo::Bat,  AnimalInfo::Vulture, AnimalInfo::SpellBat, AnimalInfo::SpellWolf};
	for (int t = 0; t < static_cast<int>(AnimalInfo::_COUNT); ++t)
	{
		const auto type = static_cast<AnimalInfo>(t);
		EXPECT_EQ(_system.IsFrighteningToCreature(MakeAnimal(type)),
		          std::ranges::find(k_Frightening, type) != k_Frightening.end())
		    << t;
	}
	// Anything that is not an animal frightens no creature here
	EXPECT_FALSE(_system.IsFrighteningToCreature(Reg().Create()));
}

TEST_F(AnimalSystemTest, TheHandPicksUpWhatTheSpeciesAllows)
{
	EXPECT_TRUE(_system.CanPlayerPickUp(MakeAnimal(AnimalInfo::Sheep)));
	EXPECT_FALSE(_system.CanPlayerPickUp(MakeAnimal(AnimalInfo::Lion)));
	// Not an animal: no
	EXPECT_FALSE(_system.CanPlayerPickUp(Reg().Create()));
}

TEST_F(AnimalSystemTest, TheLeaderIsTheFirstMemberStillThere)
{
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto leader = MakeAnimal(AnimalInfo::Sheep);
	const auto follower = MakeAnimal(AnimalInfo::Sheep);
	registry.Assign<Flock>(flock).members = {leader, follower};

	EXPECT_EQ(_system.LeaderOf(flock), leader);
	EXPECT_EQ(_system.MembersOf(flock), (std::vector<entt::entity> {leader, follower}));

	// An empty flock and a thing that is no flock have neither
	const auto empty = registry.Create();
	registry.Assign<Flock>(empty);
	EXPECT_EQ(_system.LeaderOf(empty), entt::entity {entt::null});
	EXPECT_TRUE(_system.MembersOf(empty).empty());
	EXPECT_EQ(_system.LeaderOf(leader), entt::entity {entt::null});
	EXPECT_TRUE(_system.MembersOf(leader).empty());
}

TEST_F(AnimalSystemTest, TheGoalIsTheBrainsMoveGoal)
{
	const auto animal = MakeAnimal(AnimalInfo::Dove);
	// No brain yet: nothing, and asking makes none
	EXPECT_EQ(_system.GoalOf(animal), glm::vec2(0.0f));
	EXPECT_EQ(_system.GoalHeightOf(animal), 0.0f);
	EXPECT_FALSE(Reg().AllOf<AnimalBrain>(animal));

	auto& brain = Reg().Assign<AnimalBrain>(animal);
	brain.goal = {12.5f, -3.0f};
	brain.goalAltitude = 20.0f;
	EXPECT_EQ(_system.GoalOf(animal), glm::vec2(12.5f, -3.0f));
	EXPECT_EQ(_system.GoalHeightOf(animal), 20.0f);
}

TEST_F(AnimalSystemTest, ScaleAndRadius)
{
	const auto animal = MakeAnimal(AnimalInfo::Sheep);
	_system.SetScale(animal, 0.75f);
	EXPECT_EQ(Reg().Get<Transform>(animal).scale, glm::vec3(0.75f));
	// Without a loaded model there is no footprint
	EXPECT_EQ(_system.RadiusOf(animal), 0.0f);
}

namespace
{
using openblack::ecs::animal_ai::AnimalState;

/// An animal whose top state is `top` and final state `final`, going round a circle, with a target
AnimalBrain& RoundACircle(entt::entity animal, AnimalState top, AnimalState final)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& brain = registry.Assign<AnimalBrain>(animal);
	brain.topState = static_cast<uint8_t>(top);
	brain.finalState = static_cast<uint8_t>(final);
	brain.hugCircle = {.centre = {3.0f, 4.0f}, .radius = 2.0f, .owner = registry.Create(), .set = true};
	brain.turnsToObj = 7;
	brain.hugGoalDistance = 0x200;
	brain.target = registry.Create();
	return brain;
}

/// Whether it let go of the circle: none held, no turns to one and no goal distance from an orbit
bool LetGo(const AnimalBrain& brain)
{
	return !brain.hugCircle.set && brain.hugCircle.owner == entt::entity {entt::null} && brain.turnsToObj == 0xFF &&
	       brain.hugGoalDistance == 0;
}
} // namespace

TEST_F(AnimalSystemTest, LeavingAScriptLetsGoOfTheCircle)
{
	namespace detail = openblack::ecs::animal_ai::detail;
	const auto stateOf = [](const AnimalBrain& brain) { return static_cast<AnimalState>(brain.topState); };
	// Into a state that is not the script's (DECIDE_WHAT_TO_DO): refused, and the circle let go of all the same; the
	// target is the walks' and stays
	const auto refused = MakeAnimal(AnimalInfo::Sheep);
	auto& held = RoundACircle(refused, AnimalState::InScript, AnimalState::InScript);
	detail::SetTopState(refused, held, AnimalState::DecideWhatToDo);
	EXPECT_EQ(stateOf(held), AnimalState::InScript);
	EXPECT_TRUE(LetGo(held));
	EXPECT_NE(held.target, entt::entity {entt::null});
	// Into a script state (IN_DANCE), a state a script may interrupt (SET_DYING) or the hand: let go of, and let in
	for (const auto next : {AnimalState::InDance, AnimalState::SetDying, AnimalState::InHand})
	{
		const auto animal = MakeAnimal(AnimalInfo::Sheep);
		auto& brain = RoundACircle(animal, AnimalState::InScript, AnimalState::InScript);
		detail::SetTopState(animal, brain, next);
		EXPECT_EQ(stateOf(brain), next);
		EXPECT_TRUE(LetGo(brain));
	}
	// On its way to the script's place (a walk whose final state is IN_SCRIPT) it leaves the same way
	const auto walking = MakeAnimal(AnimalInfo::Sheep);
	auto& walk = RoundACircle(walking, AnimalState::MoveToPos, AnimalState::InScript);
	detail::SetTopState(walking, walk, AnimalState::Wander);
	EXPECT_EQ(stateOf(walk), AnimalState::MoveToPos);
	EXPECT_TRUE(LetGo(walk));
}

TEST_F(AnimalSystemTest, LeavingAWalkOrAReactionLetsGoOfTheCircle)
{
	namespace detail = openblack::ecs::animal_ai::detail;
	// MOVE_IN_FLOCK, a move state: the circle let go of and the target dropped
	const auto flocking = MakeAnimal(AnimalInfo::Sheep);
	auto& flock = RoundACircle(flocking, AnimalState::MoveInFlock, AnimalState::MoveInFlock);
	detail::SetTopState(flocking, flock, AnimalState::Wander);
	EXPECT_EQ(flock.topState, static_cast<uint8_t>(AnimalState::Wander));
	EXPECT_TRUE(LetGo(flock));
	EXPECT_EQ(flock.target, entt::entity {entt::null});
	// GOTO_FOOD_REACTION, a reaction: the circle let go of even into the next reaction state
	const auto feeding = MakeAnimal(AnimalInfo::Sheep);
	auto& food = RoundACircle(feeding, AnimalState::GotoFoodReaction, AnimalState::GotoFoodReaction);
	detail::SetTopState(feeding, food, AnimalState::ArrivesAtFoodReaction);
	EXPECT_EQ(food.topState, static_cast<uint8_t>(AnimalState::ArrivesAtFoodReaction));
	EXPECT_TRUE(LetGo(food));
	// EAT has no exit: the circle, its turns and its distance are kept, and the target too
	const auto eating = MakeAnimal(AnimalInfo::Sheep);
	auto& eat = RoundACircle(eating, AnimalState::Eat, AnimalState::Eat);
	detail::SetTopState(eating, eat, AnimalState::Wander);
	EXPECT_EQ(eat.topState, static_cast<uint8_t>(AnimalState::Wander));
	EXPECT_TRUE(eat.hugCircle.set);
	EXPECT_EQ(eat.turnsToObj, 7);
	EXPECT_EQ(eat.hugGoalDistance, 0x200u);
	EXPECT_NE(eat.target, entt::entity {entt::null});
}

namespace
{
/// A forest at a centre with one full grown tree 5 m along x from it; returns the tree
entt::entity ForestWithATree(const glm::vec3& centre)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto id = ecs::CreateForest(0, centre);
	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, centre + glm::vec3(5.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f, .forestId = id});
	return tree;
}

/// The lair a predator leading a flock of its own at a point picks: the flock's domain centre once it is worked out
glm::vec3 LairOf(entt::entity animal, const glm::vec3& at)
{
	namespace detail = openblack::ecs::animal_ai::detail;
	auto& registry = Locator::entitiesRegistry::value();
	const auto flock = registry.Create();
	registry.Assign<Flock>(flock).members = {animal};
	auto& brain = registry.Assign<AnimalBrain>(animal);
	auto& component = registry.Get<Animal>(animal);
	component.flock = flock;
	auto& transform = registry.Get<Transform>(animal);
	transform.position = at;
	detail::Context ctx {animal, component, brain, transform, detail::InfoOf(component)};
	detail::CalculateLairPosition(ctx);
	return registry.Get<const Flock>(flock).domainCentre;
}
} // namespace

TEST_F(AnimalSystemTest, PredatorsLairsFollowTheForestRule)
{
	const glm::vec3 at {1000.0f, 0.0f, 1000.0f};
	// no forest: a tiger stays where it is
	EXPECT_EQ(LairOf(MakeAnimal(AnimalInfo::Tiger), at), at);
	// one forest, however far: its tree nearest its centre
	const auto far = ForestWithATree(glm::vec3(1500.0f, 0.0f, 1000.0f));
	EXPECT_EQ(LairOf(MakeAnimal(AnimalInfo::Tiger), at), Reg().Get<const Transform>(far).position);
	// the forests are walked newest first and the first is taken unscored. Each later one is scored on its distance in
	// map units over 1000, which puts every forest more than a few centimetres away at the bottom of the curve: the
	// second newest scores more than the unscored first and takes it, and no older one scores more than it. So neither
	// the nearest forest, the newest, nor the oldest is the one
	const auto secondNewest = ForestWithATree(glm::vec3(1300.0f, 0.0f, 1000.0f));
	ForestWithATree(glm::vec3(1050.0f, 0.0f, 1000.0f));
	EXPECT_EQ(LairOf(MakeAnimal(AnimalInfo::Tiger), at), Reg().Get<const Transform>(secondNewest).position);
	// a lion stays where it is
	EXPECT_EQ(LairOf(MakeAnimal(AnimalInfo::Lion), at), at);
	// a wolf goes to the nearest big forest before any forest
	const auto bigForest = Reg().Create();
	const glm::vec3 bigForestAt {900.0f, 0.0f, 800.0f};
	Reg().Assign<Transform>(bigForest, bigForestAt, glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<BigForest>(bigForest);
	EXPECT_EQ(LairOf(MakeAnimal(AnimalInfo::Wolf), at), bigForestAt);
}

TEST_F(AnimalSystemTest, ALonePredatorsLairIsWorkedOutAsItIsMade)
{
	namespace animal_ai = openblack::ecs::animal_ai;
	const glm::vec3 at {1000.0f, 0.0f, 1000.0f};
	// an animal made alone: a flock of its own centred where it stands, and no brain until its first turn
	const auto alone = [&at](AnimalInfo type) {
		const auto animal = MakeAnimal(type);
		Reg().Get<Transform>(animal).position = at;
		const auto flock = Reg().Create();
		auto& data = Reg().Assign<Flock>(flock);
		data.members = {animal};
		data.domainCentre = at;
		Reg().Get<Animal>(animal).flock = flock;
		return flock;
	};
	const auto tree = ForestWithATree(glm::vec3(1500.0f, 0.0f, 1000.0f));
	// a tiger takes the forest there is as it is made, without a brain
	const auto tigers = alone(AnimalInfo::Tiger);
	const auto tiger = Reg().Get<const Flock>(tigers).members.front();
	animal_ai::CalculateLairOnCreation(tiger);
	EXPECT_EQ(Reg().Get<const Flock>(tigers).domainCentre, Reg().Get<const Transform>(tree).position);
	EXPECT_FALSE(Reg().AllOf<AnimalBrain>(tiger));
	// a sheep keeps the centre where it stands
	const auto sheep = alone(AnimalInfo::Sheep);
	animal_ai::CalculateLairOnCreation(Reg().Get<const Flock>(sheep).members.front());
	EXPECT_EQ(Reg().Get<const Flock>(sheep).domainCentre, at);
}

TEST_F(AnimalSystemTest, AnAnimalTakenFromItsFlockGetsItsOwnWhoseHomeMovesWhereItLands)
{
	namespace animal_ai = openblack::ecs::animal_ai;
	namespace flocks = openblack::ecs::flocks;
	// a cow the hand takes from a herd of three leaves it for a flock of its own, with the herd's domain radius and
	// flock distance and no script id; the herd keeps the other two
	const auto herd = flocks::Create(glm::vec3(50.0f, 0.0f, 50.0f), -1);
	Reg().Get<Flock>(herd).domainRadius = 40;
	Reg().Get<Flock>(herd).flockDistance = 12;
	std::array<entt::entity, 3> cows {};
	for (auto& cow : cows)
	{
		cow = MakeAnimal(AnimalInfo::Cow);
		Reg().Assign<AnimalBrain>(cow);
		flocks::AddMember(herd, cow);
	}
	animal_ai::PlaceInHand(cows[0]);
	const auto own = flocks::FlockOf(cows[0]);
	ASSERT_TRUE(Reg().Valid(own));
	ASSERT_NE(own, herd);
	EXPECT_EQ(Reg().Get<const Flock>(own).domainRadius, 40);
	EXPECT_EQ(Reg().Get<const Flock>(own).flockDistance, 12);
	EXPECT_EQ(Reg().Get<const Flock>(own).id, -1);
	EXPECT_EQ(Reg().Get<const Flock>(own).members, std::vector<entt::entity> {cows[0]});
	EXPECT_EQ(Reg().Get<const Flock>(herd).members.size(), 2u);
	EXPECT_EQ(Reg().Get<const AnimalBrain>(cows[0]).topState, static_cast<uint8_t>(AnimalState::InHand));

	// landed where the hand threw it, its next turn moves its flock's home there, and it plays its landing before it
	// decides what to do
	const glm::vec3 fell {120.0f, 3.0f, 80.0f};
	Reg().Get<Transform>(cows[0]).position = fell;
	Reg().Get<AnimalBrain>(cows[0]).topState = static_cast<uint8_t>(AnimalState::Landed);
	animal_ai::ProcessAnimal(cows[0]);
	EXPECT_EQ(Reg().Get<const Flock>(own).domainCentre, fell);
	const auto& landed = Reg().Get<const AnimalBrain>(cows[0]);
	EXPECT_EQ(landed.topState, static_cast<uint8_t>(AnimalState::WaitForAnimation));
	EXPECT_EQ(landed.finalState, static_cast<uint8_t>(AnimalState::InteractDecideWhatToDo));

	// the herd goes once the hand has taken all its cows (no script holds it)
	animal_ai::PlaceInHand(cows[1]);
	EXPECT_TRUE(openblack::ecs::IsAvailable(herd));
	animal_ai::PlaceInHand(cows[2]);
	EXPECT_FALSE(openblack::ecs::IsAvailable(herd));
}

TEST_F(AnimalSystemTest, SwitchingReactionsKeepsTheStateFromBeforeTheFirstAndRecordsTheNewType)
{
	namespace detail = openblack::ecs::animal_ai::detail;
	namespace reactions = openblack::ecs::effects::reactions;
	const auto turnBefore = game_clock::Turn();
	game_clock::SetTurn(1234);
	// A sheep that wandered (its state kept when it first reacted) and now runs from a thing thrown at it
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	auto& brain = Reg().Assign<AnimalBrain>(sheep);
	brain.topState = static_cast<uint8_t>(AnimalState::FleeingAndLookingAtObjectReaction);
	brain.finalState = static_cast<uint8_t>(AnimalState::FleeingAndLookingAtObjectReaction);
	brain.previousState = static_cast<uint8_t>(AnimalState::Wander);
	brain.reaction = 4;
	brain.predator = Reg().Create();
	// It switches to a creature's reaction
	const auto creature = Reg().Create();
	const reactions::Reaction creatureReaction {.id = 5, .initiator = creature, .type = openblack::Reaction::ReactToCreature};
	detail::SwitchReaction(sheep, brain, creatureReaction);
	// It runs from the creature, and still goes back to wandering once it is over, not to the reaction it left
	EXPECT_EQ(brain.topState, static_cast<uint8_t>(AnimalState::FleeingFromObjectReaction));
	EXPECT_EQ(brain.previousState, static_cast<uint8_t>(AnimalState::Wander));
	EXPECT_EQ(brain.reaction, 5u);
	EXPECT_EQ(brain.predator, creature);
	// The new type's record has the turn of the switch, which its reaction's length counts from
	EXPECT_EQ(reactions::RecordTurn(sheep, 6), 1234u);
	game_clock::SetTurn(turnBefore);
}

TEST_F(AnimalSystemTest, AnAnimalsWayIsItsLastStepEvenWhenItStandsStill)
{
	namespace animal_ai = openblack::ecs::animal_ai;
	const auto sheep = MakeAnimal(AnimalInfo::Sheep);
	auto& brain = Reg().Assign<AnimalBrain>(sheep);
	brain.step = {300, -700};
	// Standing still this turn, its way is still its last step, in whole map units, level
	brain.movedLastTurn = 0.0f;
	EXPECT_EQ(animal_ai::MovementDirection(sheep), glm::vec3(300.0f, 0.0f, -700.0f));
	// The same while it walks
	brain.movedLastTurn = 0.5f;
	EXPECT_EQ(animal_ai::MovementDirection(sheep), glm::vec3(300.0f, 0.0f, -700.0f));
}

namespace
{
/// An animal in no state of its own (its turn runs nothing), with a brain
AnimalBrain& Stateless(entt::entity animal)
{
	auto& brain = Locator::entitiesRegistry::value().Assign<AnimalBrain>(animal);
	brain.topState = static_cast<uint8_t>(AnimalState::Invalid);
	brain.finalState = static_cast<uint8_t>(AnimalState::Invalid);
	return brain;
}
} // namespace

TEST_F(AnimalSystemTest, TheTurnsInAStateAreCountedAfterTheReaction)
{
	namespace animal_ai = openblack::ecs::animal_ai;
	namespace reactions = openblack::ecs::effects::reactions;
	reactions::Clear();
	// no reaction: one more turn in the state
	const auto calm = MakeAnimal(AnimalInfo::Sheep);
	Stateless(calm).turnsSinceStateChange = 5;
	animal_ai::ProcessAnimal(calm);
	EXPECT_EQ(Reg().Get<const AnimalBrain>(calm).turnsSinceStateChange, 6);
	// a reaction whose initiator has gone ends into the stored state's return state (here the invalid state 0), and
	// that new state has already counted this turn
	const auto initiator = Reg().Create();
	Reg().Assign<Transform>(initiator, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto id = reactions::CreateReaction(initiator, openblack::Reaction::FleeFromObject, PlayerNames::NEUTRAL, false);
	ASSERT_NE(id, 0u);
	const auto fleeing = MakeAnimal(AnimalInfo::Sheep);
	auto& brain = Stateless(fleeing);
	brain.turnsSinceStateChange = 5;
	brain.reaction = id;
	animal_ai::ProcessAnimal(fleeing);
	const auto& after = Reg().Get<const AnimalBrain>(fleeing);
	EXPECT_EQ(after.reaction, 0u);
	EXPECT_EQ(after.turnsSinceStateChange, 1);
	reactions::Clear();
}

TEST_F(AnimalSystemTest, AnAnimalLetsGoOfFoodThePlayersHandHolds)
{
	namespace animal_ai = openblack::ecs::animal_ai;
	const test::RestoreService<Locator::handSystem> restoreHand;
	auto& hand = static_cast<test::creature_fakes::FakeHand&>(Locator::handSystem::emplace<test::creature_fakes::FakeHand>());
	const auto food = Reg().Create();
	Reg().Assign<Transform>(food, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// held by nothing: it keeps eating it
	const auto eating = MakeAnimal(AnimalInfo::Sheep);
	auto& brain = Stateless(eating);
	brain.foodTarget = food;
	brain.counter = 7;
	animal_ai::ProcessAnimal(eating);
	EXPECT_EQ(Reg().Get<const AnimalBrain>(eating).foodTarget, food);
	EXPECT_EQ(Reg().Get<const AnimalBrain>(eating).counter, 7);
	// in the player's hand, still available: dropped, its counter 0, and it decides what to do
	hand.held = food;
	animal_ai::ProcessAnimal(eating);
	const auto& after = Reg().Get<const AnimalBrain>(eating);
	EXPECT_EQ(after.foodTarget, entt::entity {entt::null});
	EXPECT_EQ(after.counter, 0);
	EXPECT_EQ(after.topState, static_cast<uint8_t>(AnimalState::DecideWhatToDo));
}

TEST_F(AnimalSystemTest, AFlockDeletedLetsGoOfItsTownItsMembersAndItsCitadelHeart)
{
	namespace flocks = openblack::ecs::flocks;
	// a town's herd of two, which is also a citadel heart's flock: deleted, it leaves the town's flocks, each cow
	// leaves it (the last one's leaving deletes it), and the heart lets go of it; the cows stay
	auto& registry = Reg();
	const auto town = registry.Create();
	registry.Assign<openblack::ecs::components::Town>(town);
	const auto herd = flocks::Create(glm::vec3(50.0f, 0.0f, 50.0f), -1);
	registry.Get<openblack::ecs::components::Town>(town).flocks.push_back(herd);
	registry.Get<Flock>(herd).town = town;
	const auto heart = registry.Create();
	registry.Assign<openblack::ecs::components::CitadelHeart>(heart).flock = herd;
	std::array<entt::entity, 2> cows {};
	for (auto& cow : cows)
	{
		cow = MakeAnimal(AnimalInfo::Cow);
		registry.Assign<AnimalBrain>(cow);
		flocks::AddMember(herd, cow);
	}
	openblack::ecs::ToBeDeleted(herd);
	EXPECT_FALSE(openblack::ecs::IsAvailable(herd));
	EXPECT_TRUE(registry.Get<const openblack::ecs::components::Town>(town).flocks.empty());
	EXPECT_EQ(registry.Get<const openblack::ecs::components::CitadelHeart>(heart).flock, entt::entity {entt::null});
	for (const auto cow : cows)
	{
		EXPECT_TRUE(openblack::ecs::IsAvailable(cow));
		EXPECT_EQ(flocks::FlockOf(cow), entt::entity {entt::null});
	}
}
