/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature mind system on a registry of its own: how a species' desires start from the game's tables, the first
// turn's 40 draws on the synced stream, the scans' order and what they pass over, what a creature knows of a villager,
// the night from the day / night clock, an action's mirroring, the creature a saved mind keeps, a script's name and
// what a script teaches over a mind file's, the decays a mind file leaves as they were drawn, the values the land's
// curse keeps of it, how much the animals catch a creature's eye, what it does about a miracle the reaction handler
// gives it, the hands pointing where the agenda says, and a forced plan or a gift stopping the reaction it follows

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureLearning.h"
#include "Creature/CreatureLocomotion.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreatureReactionRules.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureReaction.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/CreatureSwap.h"
#include "ECS/Components/CreatureTallies.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/PlacedAt.h"
#include "ECS/Components/ReactionRecords.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/CreatureNaming.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Placed.h"
#include "ECS/PlayerCreature.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "ECS/Systems/Implementations/PlayerSystem.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "GameClock.h"
#include "creature/CreatureSystemFakes.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/FireFakes.h"
#include "support/MagicFakes.h"
#include "support/RestoreService.h"
#include "support/TotemFakes.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureMindSystem;
namespace mind_detail = openblack::ecs::systems::mind_detail;
using creature_plan_actions::Target;

namespace
{
class CreatureMindSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// each desire's cap, and the range its decay is drawn from, a different one each
		for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
		{
			auto& initial = _world.Info().creatureInitialDesire.at(d);
			initial.field0x3c = 2.0f;
			initial.field0x40 = 0.9f;
			initial.field0x44 = 0.9f + (0.001f * static_cast<float>(d + 1));
		}
		// what a creature knows of a villager asks whether it burns
		Locator::fireSystem::emplace<test::FakeFires>();
	}

	static entt::entity MakeVillager(glm::vec3 position, float life = 1.0f)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		auto& villager = registry.Assign<Villager>(entity);
		villager.life = life;
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	// put back after the registry, which the entities may reach as they go
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	test::creature_world::World _world;
	const test::RestoreService<Locator::dayNightClock> _restoreClock;
	CreatureMindSystem _system;
};
} // namespace

TEST_F(CreatureMindSystemTest, HowFarItStraysFromHomeIsItsStagesFromTheTable)
{
	auto& phases = _world.Info().creatureDevelopmentPhaseEntry;
	for (size_t phase = 0; phase < phases.size(); ++phase)
	{
		phases.at(phase).goHomeDistance = 100.0f + static_cast<float>(phase);
	}
	EXPECT_FLOAT_EQ(mind_detail::GoHomeDistance(0), 100.0f);
	EXPECT_FLOAT_EQ(mind_detail::GoHomeDistance(5), 105.0f);
	// past the table's end, its last stage's
	EXPECT_FLOAT_EQ(mind_detail::GoHomeDistance(40), 100.0f + static_cast<float>(phases.size() - 1));
	// without the tables, none
	Locator::infoConstants::reset();
	EXPECT_FLOAT_EQ(mind_detail::GoHomeDistance(5), 0.0f);
}

TEST_F(CreatureMindSystemTest, TheDesiresStartFromTheSpeciesTables)
{
	const auto setup = mind_detail::SetupFor(CreatureType::GiantApe);
	for (size_t d = 0; d < setup.size(); ++d)
	{
		// the row's cap, then the range its decay is drawn from (docs/wiki/creature/mind.md#the-fear-desires-numbers)
		EXPECT_FLOAT_EQ(setup.at(d).max, 2.0f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMin, 0.9f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMax, 0.9f + (0.001f * static_cast<float>(d + 1)));
	}
	// without the tables, the defaults
	Locator::infoConstants::reset();
	EXPECT_FLOAT_EQ(mind_detail::SetupFor(CreatureType::GiantApe).front().max, 1.0f);
}

TEST_F(CreatureMindSystemTest, TheCreatureDrawsItsFortyDecaysInDesireOrderAsItIsMade)
{
	const game_random::testing::ScopedState state;
	std::vector<float> floats;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float x) {
		                                  floats.push_back(x);
		                                  return x * 0.5f;
	                                  });
	const auto local = game_random::Current().local;
	const auto crt = game_random::crt::Seed();

	const auto creature = test::creature_world::World::MakeCreature();

	// the strength and the fatness first, then a decay for each desire: the tables' growing times and source spreads
	// are 0 here, so they draw nothing in between
	ASSERT_GE(floats.size(), 2 + creature_desires::k_DesireCount);
	const auto& desires = test::creature_world::World::Registry().Get<CreatureMindState>(creature).desires;
	ASSERT_TRUE(desires.has_value());
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto range = (0.9f + (0.001f * static_cast<float>(d + 1))) - 0.9f;
		EXPECT_FLOAT_EQ(floats.at(2 + d), range);
		// low + the draw below high - low
		EXPECT_FLOAT_EQ(desires->desires.at(d).decay, 0.9f + (range * 0.5f));
	}
	EXPECT_EQ(game_random::Current().local, local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// its first turn draws them no more
	floats.clear();
	_system.ProcessTurn();
	EXPECT_TRUE(floats.empty());
}

TEST_F(CreatureMindSystemTest, ScansBreakTiesOnTheEntityAndPassOverWhatIsGoing)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto first = MakeVillager(glm::vec3(10.0f, 0.0f, 0.0f));
	const auto second = MakeVillager(glm::vec3(-10.0f, 0.0f, 0.0f));
	const auto nearest = MakeVillager(glm::vec3(0.0f, 0.0f, 5.0f));
	const auto going = MakeVillager(glm::vec3(0.0f, 0.0f, 1.0f));
	test::creature_world::World::Registry().Assign<Unavailable>(going);

	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto found = mind_detail::Gather(registry, Target::Villager, self, glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 3u);
	EXPECT_EQ(found.at(0).entity, nearest);
	// as near, the lower entity first
	EXPECT_EQ(found.at(1).entity,
	          std::min(first, second, [](auto a, auto b) { return entt::to_integral(a) < entt::to_integral(b); }));
	EXPECT_FALSE(mind_detail::Accepts(registry, Target::Villager, going, self));
	EXPECT_FALSE(mind_detail::Accepts(registry, Target::Villager, self, self));
	EXPECT_TRUE(mind_detail::Accepts(registry, Target::Living, nearest, self));
}

// Coming to find another creature nicer or nastier happens at once, on an attitude made when it has none, kept within
// -1 and 1 (docs/wiki/creature/fights.md, "A landed blow's opinion")
TEST_F(CreatureMindSystemTest, FindingAnotherCreatureNastierChangesItsAttitudeAtOnce)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(20.0f, 0.0f, 0.0f));
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	const auto howNice = [&]() {
		const auto& attitudes = mind.learnt->creatures;
		const auto found =
		    std::ranges::find(attitudes, entt::to_integral(other), &creature_learning::CreatureAttitude::creature);
		return found != attitudes.end() ? std::optional(found->howNice) : std::nullopt;
	};
	const auto before = howNice().value_or(0.0f);
	_system.ChangeHowNice(creature, other, -0.05f);
	ASSERT_TRUE(howNice().has_value());
	EXPECT_FLOAT_EQ(*howNice(), std::clamp(before - 0.05f, -1.0f, 1.0f));
	_system.ChangeHowNice(creature, other, -5.0f);
	EXPECT_FLOAT_EQ(*howNice(), -1.0f);
}

// Following to look is done only at a villager whose work shows a skill the creature does not know: the top state's skill
// in the game's table, none for a state that shows none or a skill it knows (docs/wiki/creature/plans-and-needs.md)
TEST_F(CreatureMindSystemTest, ItFollowsToLookOnlyAtAVillagerWhoseWorkShowsASkillItDoesNotKnow)
{
	constexpr size_t k_Build = 0;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	_world.Info().villagerStateTable.at(static_cast<size_t>(VillagerStates::Building)).field0xe0 = k_Build;
	const auto builder = MakeVillager(glm::vec3(105.0f, 0.0f, 100.0f));
	auto& action = registry.Assign<LivingAction>(builder, VillagerStates::MoveToPos, uint16_t {0});
	const auto accepted = [&]() {
		return mind_detail::Accepts(std::as_const(registry), Target::InterestingVillager, builder, creature);
	};
	// walking shows nothing
	EXPECT_FALSE(accepted());
	action.states[static_cast<size_t>(LivingAction::Index::Top)] = static_cast<uint8_t>(VillagerStates::Building);
	EXPECT_TRUE(accepted());
	// a skill it knows
	auto& known = mind.learnt->knowledge.skillsKnown;
	if (known.size() <= k_Build)
	{
		known.resize(k_Build + 1);
	}
	known.at(k_Build) = true;
	EXPECT_FALSE(accepted());
	// never the creature itself
	EXPECT_FALSE(mind_detail::Accepts(std::as_const(registry), Target::InterestingVillager, creature, creature));
}

TEST_F(CreatureMindSystemTest, AThingToPlayWithIsNeverADeadVillager)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto alive = MakeVillager(glm::vec3(5.0f, 0.0f, 0.0f));
	const auto dead = MakeVillager(glm::vec3(-5.0f, 0.0f, 0.0f));
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<LivingAction>(alive, VillagerStates::MoveToPos, uint16_t {0});
	registry.Assign<LivingAction>(dead, VillagerStates::MoveToPos, uint16_t {0});
	registry.Get<Villager>(dead).status |= Villager::k_StatusDead;

	const auto& lookup = std::as_const(registry);
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::Playable, alive, self));
	EXPECT_FALSE(mind_detail::Accepts(lookup, Target::Playable, dead, self));
	// whatever is not a villager or an animal is played with as before
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::Anything, dead, self));
}

TEST_F(CreatureMindSystemTest, AVillagerIsAliveByItsLife)
{
	const auto self = test::creature_world::World::MakeCreature();
	const auto alive = MakeVillager(glm::vec3(0.0f), 0.5f);
	const auto dead = MakeVillager(glm::vec3(0.0f), 0.0f);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto aliveBelief = mind_detail::BeliefOf(registry, alive, self);
	const auto deadBelief = mind_detail::BeliefOf(registry, dead, self);
	ASSERT_TRUE(aliveBelief.has_value());
	ASSERT_TRUE(deadBelief.has_value());
	EXPECT_EQ(aliveBelief->type, creature_tree::belief_types::k_Villager);
	EXPECT_EQ(aliveBelief->Value(creature_tree::Attribute::Life), 1u);
	EXPECT_EQ(deadBelief->Value(creature_tree::Attribute::Life), 0u);
}

TEST_F(CreatureMindSystemTest, AnotherCreatureIsHarderWhenDrawnTallerThanTheBelieversPlayersCreature)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	using creature_tree::Attribute;
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE, 0.0f, 1.0f);
	const auto taller = test::creature_world::World::MakeCreature(glm::vec3(5.0f), PlayerNames::PLAYER_TWO, 0.0f, 2.0f);
	const auto smaller = test::creature_world::World::MakeCreature(glm::vec3(9.0f), PlayerNames::PLAYER_THREE, 0.0f, 0.5f);
	const auto same = test::creature_world::World::MakeCreature(glm::vec3(-5.0f), PlayerNames::PLAYER_FOUR, 0.0f, 1.0f);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	// by the height drawn, not by strength: the taller one is the weaker
	test::creature_world::World::Registry().Get<Creature>(taller).strength = 0.0f;
	test::creature_world::World::Registry().Get<Creature>(smaller).strength = 1.0f;
	EXPECT_EQ(mind_detail::BeliefOf(registry, taller, self)->Value(Attribute::HarderThanMe), 1u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, smaller, self)->Value(Attribute::HarderThanMe), 0u);
	// as tall is not harder, nor is the believer itself
	EXPECT_EQ(mind_detail::BeliefOf(registry, same, self)->Value(Attribute::HarderThanMe), 0u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, self, self)->Value(Attribute::HarderThanMe), 0u);
	// a creature of no player has no player's creature to compare with
	const auto nobodys = test::creature_world::World::MakeCreature(glm::vec3(20.0f, 0.0f, 0.0f), PlayerNames::_COUNT);
	EXPECT_EQ(mind_detail::BeliefOf(registry, taller, nobodys)->Value(Attribute::HarderThanMe), 0u);
	// their heights, by the band of the size each is drawn at
	EXPECT_EQ(mind_detail::BeliefOf(registry, taller, self)->Value(Attribute::Height), 3u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, same, self)->Value(Attribute::Height), 2u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, smaller, self)->Value(Attribute::Height), 1u);
}

TEST_F(CreatureMindSystemTest, AMealEndsByWhatItWas)
{
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::creaturePhysiologySystem> restorePhysiology;
	auto& physiology = static_cast<test::creature_loop_fakes::FakePhysiology&>(
	    Locator::creaturePhysiologySystem::emplace<test::creature_loop_fakes::FakePhysiology>(log));
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.desires.emplace();
	mind.learnt = creature_mind_model::Fresh(*mind.desires, 0, {});
	auto& hunger = (*mind.desires)[creature_desires::Desire::Hunger];
	hunger.value = 0.9f;
	const auto animals = static_cast<uint32_t>(ObjectType::Animal);
	const auto mobile = static_cast<uint32_t>(ObjectType::MobileObject);

	// eaten: counted, and the meal takes the hunger down by what the body makes of it
	physiology.hungerAfterMeal = 0.4f;
	_system.EndMeal(creature, animals, 300.0f, creature_eating::MealEnd::Eaten);
	EXPECT_EQ(mind.learnt->objectTypes[animals].eaten, 1u);
	EXPECT_EQ(mind.learnt->objectTypes[animals].notFood, 0u);
	EXPECT_FLOAT_EQ(hunger.value, 0.4f);
	EXPECT_EQ(hunger.suppressedTurns, 0u);

	// not food: counted as such, the hunger held back for 20 seconds (10 turns a second), and it is sick
	_system.EndMeal(creature, mobile, 3.0f, creature_eating::MealEnd::NotFood);
	EXPECT_EQ(mind.learnt->objectTypes[mobile].notFood, 1u);
	EXPECT_EQ(mind.learnt->objectTypes[mobile].eaten, 0u);
	EXPECT_FLOAT_EQ(hunger.value, 0.4f);
	EXPECT_EQ(hunger.suppressedTurns, 200u);
	EXPECT_EQ(mind.idle.activity, creature_mind::Activity::Puke);

	// poisoned: sick and held back, but nothing learnt of the type
	hunger.suppressedTurns = 0;
	_system.EndMeal(creature, animals, 300.0f, creature_eating::MealEnd::Poisoned);
	EXPECT_EQ(mind.learnt->objectTypes[animals].notFood, 0u);
	EXPECT_EQ(mind.learnt->objectTypes[animals].eaten, 1u);
	EXPECT_EQ(hunger.suppressedTurns, 200u);

	// refused: sick and held back for 20 seconds, nothing learnt of the type
	hunger.suppressedTurns = 0;
	_system.EndMeal(creature, mobile, 3.0f, creature_eating::MealEnd::Refused);
	EXPECT_EQ(mind.learnt->objectTypes[mobile].notFood, 1u);
	EXPECT_EQ(mind.learnt->objectTypes[mobile].eaten, 0u);
	EXPECT_FLOAT_EQ(hunger.value, 0.4f);
	EXPECT_EQ(hunger.suppressedTurns, 200u);
	EXPECT_EQ(mind.idle.activity, creature_mind::Activity::Puke);

	// a type past the counts is only not counted
	_system.EndMeal(creature, 1000u, 300.0f, creature_eating::MealEnd::Eaten);
	EXPECT_FLOAT_EQ(hunger.value, 0.4f);
}

TEST_F(CreatureMindSystemTest, TheNightIsTheDayNightClocks)
{
	Locator::dayNightClock::reset();
	EXPECT_FALSE(mind_detail::IsNight());
	auto& clock = Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>().Clock();
	clock.Reset();
	EXPECT_FALSE(mind_detail::IsNight());
	clock.SetScriptTime(0.0f);
	EXPECT_TRUE(mind_detail::IsNight());
}

TEST_F(CreatureMindSystemTest, GivingUpItsPlanLeavesItNothingToCarryOut)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 12, .priority = 5.0f};
	_system.GiveUpPlan(creature);
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.idle.agenda.empty());
}

namespace
{
/// The game's stages of growing up bring no desire, but the first brings hunger and the second curiosity
void StagesBringHungerThenCuriosity(InfoConstants& info)
{
	constexpr auto k_NoDesire = static_cast<uint32_t>(creature_desires::k_DesireCount);
	for (auto& entry : info.creatureDevelopmentPhaseEntry)
	{
		for (auto* field : {&entry.field0x3c, &entry.field0x40, &entry.field0x44, &entry.field0x48, &entry.field0x4c,
		                    &entry.field0x50, &entry.field0x54, &entry.field0x58, &entry.field0x5c, &entry.field0x60,
		                    &entry.field0x64, &entry.field0x68, &entry.field0x6c, &entry.field0x70})
		{
			*field = k_NoDesire;
		}
	}
	info.creatureDevelopmentPhaseEntry.at(0).field0x3c = static_cast<uint32_t>(creature_desires::Desire::Hunger);
	info.creatureDevelopmentPhaseEntry.at(1).field0x3c = static_cast<uint32_t>(creature_desires::Desire::Curiosity);
}
} // namespace

TEST_F(CreatureMindSystemTest, MovedToAStageItsDesiresAreTheStagesAtOnce)
{
	using creature_desires::Desire;
	StagesBringHungerThenCuriosity(_world.Info());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	auto& desires = mind.desires.emplace();
	desires[Desire::Fear].suppressedTurns = 7;
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 12, .priority = 5.0f};
	_system.MoveToStage(creature, 0);
	EXPECT_EQ(mind.developmentPhase, 0u);
	EXPECT_EQ(mind.desiresPhase, std::optional<uint32_t>(0u));
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	EXPECT_FALSE(desires[Desire::Curiosity].activated);
	EXPECT_FALSE(desires[Desire::Fear].activated);
	EXPECT_EQ(desires[Desire::Fear].suppressedTurns, 0u);
	// and it gives up its plan
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.planner.current.has_value());
	// moved again to the stage it is at, nothing stays held back
	desires[Desire::Hunger].suppressedTurns = 3;
	_system.MoveToStage(creature, 0);
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, 0u);
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	// the next stage brings its own as well
	_system.MoveToStage(creature, 1);
	EXPECT_TRUE(desires[Desire::Hunger].activated);
	EXPECT_TRUE(desires[Desire::Curiosity].activated);
	EXPECT_EQ(mind.desiresPhase, std::optional<uint32_t>(1u));
}

TEST_F(CreatureMindSystemTest, AMindWithNoDesiresYetTakesTheStageWhenItFirstThinks)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	mind.desires.reset();
	mind.desiresPhase = 13;
	_system.MoveToStage(creature, 4);
	EXPECT_EQ(mind.developmentPhase, 4u);
	EXPECT_FALSE(mind.desiresPhase.has_value());
	EXPECT_FALSE(mind.desires.has_value());
	// a thing that is no creature is left alone
	_system.MoveToStage(test::creature_world::World::Registry().Create(), 4);
	_system.MoveToStage(entt::null, 4);
}

TEST_F(CreatureMindSystemTest, AStageGivenWhileItsMindFileWaitsIsPutOverTheFilesStage)
{
	using creature_desires::Desire;
	const game_random::testing::ScopedState state;
	StagesBringHungerThenCuriosity(_world.Info());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_system.ProcessTurn();
	auto file = _system.SaveMind(creature);
	ASSERT_TRUE(file.has_value());
	file->developmentPhase = 0;
	file->developmentTimer = 77;
	// the file waits for the mind's next turn; the script's stage comes after it
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	_system.MoveToStage(creature, 1);
	EXPECT_EQ(registry.Get<CreatureMindState>(creature).scriptStage, std::optional<uint32_t>(1u));
	_system.ProcessTurn();
	{
		const auto& mind = registry.Get<CreatureMindState>(creature);
		EXPECT_EQ(mind.developmentPhase, 1u);
		EXPECT_EQ(mind.desiresPhase, std::optional<uint32_t>(1u));
		EXPECT_FALSE(mind.scriptStage.has_value());
		ASSERT_TRUE(mind.desires.has_value());
		EXPECT_TRUE((*mind.desires)[Desire::Hunger].activated);
		EXPECT_TRUE((*mind.desires)[Desire::Curiosity].activated);
		EXPECT_FALSE((*mind.desires)[Desire::Fear].activated);
		// and the turns at its stage start again, as the file's are replaced
		ASSERT_TRUE(mind.learnt.has_value());
		EXPECT_EQ(mind.learnt->developmentTimer, 0u);
	}
	// with no file waiting it is at once, and the turns at the stage start again
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.learnt->developmentTimer = 5;
	_system.MoveToStage(creature, 0);
	EXPECT_FALSE(mind.scriptStage.has_value());
	EXPECT_EQ(mind.developmentPhase, 0u);
	EXPECT_EQ(mind.learnt->developmentTimer, 0u);
	const auto saved = _system.SaveMind(creature);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->developmentTimer, 0u);
	// a file given after the stage replaces the stage that waited for an earlier one
	file->developmentPhase = 0;
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	_system.MoveToStage(creature, 1);
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	EXPECT_FALSE(mind.scriptStage.has_value());
	_system.ProcessTurn();
	EXPECT_EQ(registry.Get<CreatureMindState>(creature).developmentPhase, 0u);
}

TEST_F(CreatureMindSystemTest, AStageGivenBeforeTheMindLooksAtItsResourceWaitsForIt)
{
	const game_random::testing::ScopedState state;
	StagesBringHungerThenCuriosity(_world.Info());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	// made from a mind resource, which its first turn looks at (the cache holds none here)
	ASSERT_NE(registry.Get<Creature>(creature).mind, entt::id_type {0});
	_system.MoveToStage(creature, 1);
	EXPECT_EQ(registry.Get<CreatureMindState>(creature).scriptStage, std::optional<uint32_t>(1u));
	_system.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(creature);
	EXPECT_FALSE(mind.scriptStage.has_value());
	EXPECT_EQ(mind.developmentPhase, 1u);
	EXPECT_EQ(mind.desiresPhase, std::optional<uint32_t>(1u));
	// one made from no mind resource takes the stage at once
	const auto plain = test::creature_world::World::MakeCreature();
	registry.Get<Creature>(plain).mind = entt::id_type {0};
	_system.MoveToStage(plain, 1);
	EXPECT_FALSE(registry.Get<CreatureMindState>(plain).scriptStage.has_value());
}

TEST_F(CreatureMindSystemTest, AScriptSetsHowPressingWhatItCarriesOutIs)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	_system.SetAgendaPriority(creature, 3.0f);
	EXPECT_FALSE(mind.planner.current.has_value());
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 12, .priority = 5.0f};
	_system.SetAgendaPriority(creature, 0.25f);
	EXPECT_EQ(mind.planner.current->priority, 0.25f);
}

TEST_F(CreatureMindSystemTest, AnActionToldItsMirroringDrawsNothing)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	std::vector<uint32_t> draws;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t n) {
		    draws.push_back(n);
		    return 1u;
	    },
	    [](float) { return 0.0f; });
	EXPECT_TRUE(_system.PlayAction(creature, 60, false));
	EXPECT_TRUE(draws.empty());
	EXPECT_FALSE(test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body.mirrored);
	// left to the mind, a coin is tossed on the synced stream
	test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body = {};
	EXPECT_TRUE(_system.PlayAction(creature, 60));
	ASSERT_EQ(draws.size(), 1u);
	EXPECT_EQ(draws.front(), 2u);
	EXPECT_TRUE(test::creature_world::World::Registry().Get<CreatureAnimation>(creature).body.mirrored);
}

TEST_F(CreatureMindSystemTest, ASavedMindKeepsTheCreatureAsItWasBeforeItsSpells)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& body = registry.Get<Creature>(creature);
	body.size = 0.2f;
	body.strength = 1.0f;
	body.alignment = -1.0f;
	// Nothing on it: as it is now
	auto saved = _system.SaveMind(creature);
	ASSERT_TRUE(saved.has_value());
	ASSERT_TRUE(saved->physique.size.has_value());
	ASSERT_TRUE(saved->alignment.has_value());
	EXPECT_FLOAT_EQ(*saved->physique.size, 0.2f);
	EXPECT_FLOAT_EQ(saved->physique.strength, 1.0f);
	EXPECT_FLOAT_EQ(*saved->alignment, -1.0f);
	// A grow and a nasty over it: the creature the spells found is saved, not what they made of it
	auto& spells = registry.AssignState<CreatureSpells>(creature).spells;
	spells[creature_spells::Spell::Big].phase = creature_spells::Phase::Holding;
	spells[creature_spells::Spell::Big].before = 1.1f;
	spells[creature_spells::Spell::Nasty].phase = creature_spells::Phase::Finishing;
	spells[creature_spells::Spell::Nasty].before = 0.5f;
	saved = _system.SaveMind(creature);
	ASSERT_TRUE(saved.has_value());
	EXPECT_FLOAT_EQ(*saved->physique.size, 1.1f);
	EXPECT_FLOAT_EQ(saved->physique.strength, 1.0f);
	EXPECT_FLOAT_EQ(*saved->alignment, 0.5f);
}

TEST_F(CreatureMindSystemTest, AStilledMindLearnsNothingByWatching)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	const auto& knowledge = mind.learnt->knowledge;
	ASSERT_FALSE(knowledge.skillsSeen.empty());
	ASSERT_GT(knowledge.miraclesSeen.size(), 1u);
	const auto point = registry.Get<Transform>(creature).position;
	// As by the freeze spell: a skill practised and a miracle cast before it leave it as it was
	mind.paused = true;
	_system.SeeSkill(point, 0);
	_system.SeeMiracle(point, 1);
	EXPECT_EQ(knowledge.skillsSeen[0].count, 0u);
	EXPECT_FALSE(knowledge.skillsSeen[0].turn.has_value());
	EXPECT_FALSE(knowledge.skillsKnown[0]);
	EXPECT_EQ(knowledge.miraclesSeen[1].count, 0u);
	EXPECT_FALSE(knowledge.miraclesSeen[1].turn.has_value());
	EXPECT_FALSE(knowledge.miraclesKnown[1]);
	// Thawed, the same sightings teach it
	mind.paused = false;
	_system.SeeSkill(point, 0);
	_system.SeeMiracle(point, 1);
	EXPECT_EQ(knowledge.skillsSeen[0].count, 1u);
	EXPECT_TRUE(knowledge.miraclesSeen[1].turn.has_value());
	EXPECT_TRUE(knowledge.miraclesKnown[1]);
}

// A skill watched by one creature, as the hand using a totem shows it: only that creature counts it, and a stilled one
// not at all
TEST_F(CreatureMindSystemTest, ASkillWatchedTeachesOnlyTheCreatureWatchingIt)
{
	const game_random::testing::ScopedState state;
	const auto watcher = test::creature_world::World::MakeCreature();
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 120.0f));
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(watcher);
	ASSERT_TRUE(mind.learnt.has_value());
	const auto& knowledge = mind.learnt->knowledge;
	const auto& others = registry.Get<CreatureMindState>(other);
	ASSERT_TRUE(others.learnt.has_value());
	constexpr size_t k_Skill = 0;
	ASSERT_FALSE(knowledge.skillsSeen.empty());
	ASSERT_FALSE(others.learnt->knowledge.skillsSeen.empty());
	mind.paused = true;
	_system.WatchSkill(watcher, k_Skill);
	EXPECT_EQ(knowledge.skillsSeen[k_Skill].count, 0u);
	mind.paused = false;
	_system.WatchSkill(watcher, k_Skill);
	EXPECT_EQ(knowledge.skillsSeen[k_Skill].count, 1u);
	EXPECT_TRUE(knowledge.skillsSeen[k_Skill].turn.has_value());
	EXPECT_EQ(others.learnt->knowledge.skillsSeen[k_Skill].count, 0u);
}

// A villager it sees again shows it the skill of what the villager does (the state's row of the game's table), or of what
// it goes on to while it waits for an animation: each sighting after the first counts until it knows the skill. A state
// that shows none, a creature too young, one busy with a plan for its hunger and a skill it knows show nothing
// (docs/wiki/creature/mind.md#who-reports-a-sighting)
TEST_F(CreatureMindSystemTest, AVillagerSeenAgainShowsItTheSkillOfWhatItDoes)
{
	const game_random::testing::ScopedState state;
	constexpr size_t k_Build = 0;
	// building takes a while to learn, so each sighting counts until then, as info.dat's six or seven seconds (the
	// tables are read once, as the creature is made)
	_world.Info().creatireActionKnownAboutEntry.at(k_Build).field0x48 = 7.0f;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	auto& knowledge = mind.learnt->knowledge;
	ASSERT_FALSE(knowledge.skillsSeen.empty());
	_world.Info().villagerStateTable.at(static_cast<size_t>(VillagerStates::Building)).field0xe0 = k_Build;
	const auto builder = MakeVillager(glm::vec3(105.0f, 0.0f, 100.0f));
	auto& action = registry.Assign<LivingAction>(builder, VillagerStates::MoveToPos, uint16_t {0});
	const auto see = [&]() {
		std::ignore = _system.NoticeCreatureToInspect(creature, builder, 0, 0);
		return knowledge.skillsSeen[k_Build].count;
	};
	const auto doing = [&](VillagerStates top, VillagerStates final = VillagerStates::InvalidState) {
		action.states[static_cast<size_t>(LivingAction::Index::Top)] = static_cast<uint8_t>(top);
		action.states[static_cast<size_t>(LivingAction::Index::Final)] = static_cast<uint8_t>(final);
		mind.planner.current.reset();
	};
	// the first sighting is of a thing new to it; walking shows nothing
	EXPECT_EQ(see(), 0u);
	EXPECT_EQ(see(), 0u);
	doing(VillagerStates::Building);
	EXPECT_EQ(see(), 1u);
	doing(VillagerStates::WaitForAnimation, VillagerStates::Building);
	EXPECT_EQ(see(), 2u);
	// too young
	const auto grown = mind.developmentPhase;
	mind.developmentPhase = creature_watching::k_MinVillagerWatchPhase - 1;
	doing(VillagerStates::Building);
	EXPECT_EQ(see(), 2u);
	mind.developmentPhase = grown;
	// busy eating
	doing(VillagerStates::Building);
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger};
	EXPECT_EQ(see(), 2u);
	// known already
	doing(VillagerStates::Building);
	knowledge.skillsKnown[k_Build] = true;
	EXPECT_EQ(see(), 2u);
}

TEST_F(CreatureMindSystemTest, AVillagerOfAnotherPlayersOrANeutralTownSeenAgainMakesItWishToImpress)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	ASSERT_GE(mind.developmentPhase, creature_watching::k_MinVillagerWatchPhase);
	const auto& wish = mind.desires->impressVillagers;
	const auto townOf = [&registry](PlayerNames owner) {
		const auto town = registry.Create();
		registry.Assign<Town>(town).owner = owner;
		return town;
	};
	const auto villagerOf = [&](entt::entity town) {
		const auto villager = MakeVillager(glm::vec3(105.0f, 0.0f, 100.0f));
		registry.Get<Villager>(villager).town = town;
		registry.Assign<LivingAction>(villager, VillagerStates::MoveToPos, uint16_t {0});
		return villager;
	};
	const auto see = [&](entt::entity villager) {
		mind.planner.current.reset();
		std::ignore = _system.NoticeCreatureToInspect(creature, villager, 0, 0);
	};
	const auto own = villagerOf(townOf(PlayerNames::PLAYER_ONE));
	const auto other = villagerOf(townOf(PlayerNames::PLAYER_TWO));
	const auto neutral = villagerOf(townOf(PlayerNames::NEUTRAL));
	const auto homeless = villagerOf(entt::null);
	// the first sighting is of a thing new to it, and its own town's villagers and those of no town never raise it
	for (const auto villager : {own, other, neutral, homeless})
	{
		see(villager);
	}
	see(own);
	see(homeless);
	EXPECT_EQ(wish, 0.0f);
	// seen again, another player's villager and a neutral one each add a half, to 1 at most
	see(other);
	EXPECT_EQ(wish, 0.5f);
	see(neutral);
	see(other);
	EXPECT_EQ(wish, 1.0f);
	// busy with a plan for its own needs, or too young, it adds nothing
	mind.desires->impressVillagers = 0.0f;
	see(other);
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger};
	std::ignore = _system.NoticeCreatureToInspect(creature, other, 0, 0);
	EXPECT_EQ(wish, 0.5f);
	const auto grown = mind.developmentPhase;
	mind.developmentPhase = creature_watching::k_MinVillagerWatchPhase - 1;
	see(other);
	EXPECT_EQ(wish, 0.5f);
	mind.developmentPhase = grown;
	// gone, they are seen no more
	for (const auto villager : {own, other, neutral, homeless})
	{
		registry.Destroy(villager);
	}
	// the turn's sources read it for the impress desire's source of its kind, then it fades by the game's 0.99
	_system.ProcessTurn();
	EXPECT_EQ(wish, 0.5f * creature_desires::k_ImpressVillagersFade);
}

namespace
{
/// Whether the creature's agenda has a move of that kind about the point
bool Moves(const CreatureMindState& mind, creature_mind::Movement::Kind kind, glm::vec2 point)
{
	return std::ranges::any_of(mind.idle.agenda, [kind, point](const creature_mind::Step& step) {
		return step.kind == creature_mind::Step::Kind::Move && step.movement.kind == kind && step.movement.point == point;
	});
}
} // namespace

namespace
{
/// The rows of the game's table of the actions to run away from a place and to go and look at one
constexpr size_t k_RunFromPlaceRow = 156;
constexpr size_t k_ExaminePlaceRow = 157;
} // namespace

TEST_F(CreatureMindSystemTest, ANastyMiracleFrightensItAndItRunsUnlessOnTheRopeOrUnafraid)
{
	const game_random::testing::ScopedState state;
	std::ranges::copy(std::string_view("RunAwayFromPos"), _world.Info().creatureAction.at(k_RunFromPlaceRow).name.begin());
	std::ranges::copy(std::string_view("ExaminePos"), _world.Info().creatureAction.at(k_ExaminePlaceRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	auto& fear = (*mind.desires)[creature_desires::Desire::Fear];
	// the fear of scary magic, from nothing
	std::erase_if(fear.sources, [](const creature_desires::Source& source) {
		return source.type == creature_desires::sources::k_FearFromScaryMagic;
	});
	fear.sources.push_back({.type = creature_desires::sources::k_FearFromScaryMagic});
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	auto& leash = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	const glm::vec3 point(130.0f, 0.0f, 100.0f);
	const glm::vec2 at(point.x, point.z);
	using Kind = creature_mind::Movement::Kind;
	const auto scare = [&]() {
		_system.ReactToNastyMagic(creature, point, std::nullopt);
		return creature_desires::SourceValue(fear, creature_desires::sources::k_FearFromScaryMagic).value_or(-1.0f);
	};

	// afraid and free, it runs from where the miracle struck; the fear of scary magic grows by a half. The plan is forced
	// on it for its fear, about itself, where the miracle struck kept on its mind
	fear.value = 0.5f;
	EXPECT_FLOAT_EQ(scare(), 0.5f);
	EXPECT_TRUE(Moves(mind, Kind::FleeFrom, at));
	EXPECT_FALSE(Moves(mind, Kind::ToPoint, at));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_RunFromPlaceRow);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Fear);
	EXPECT_EQ(mind.planner.current->object, entt::to_integral(creature));
	EXPECT_TRUE(mind.planner.current->forced);
	EXPECT_EQ(mind.miraclePlace, at);
	// on another leash, as free
	leash.leashed = true;
	leash.type = LeashType::Good;
	fear.value = 0.5f;
	EXPECT_FLOAT_EQ(scare(), 1.0f);
	EXPECT_TRUE(Moves(mind, Kind::FleeFrom, at));
	// on the learning leash it goes to look however afraid, the source held at its most
	leash.type = LeashType::Rope;
	fear.value = 0.5f;
	EXPECT_FLOAT_EQ(scare(), 1.0f);
	EXPECT_TRUE(Moves(mind, Kind::ToPoint, at));
	EXPECT_FALSE(Moves(mind, Kind::FleeFrom, at));
	// free and unafraid, it goes to look too
	leash.leashed = false;
	leash.type = LeashType::None;
	fear.value = 0.0f;
	scare();
	EXPECT_TRUE(Moves(mind, Kind::ToPoint, at));
	EXPECT_FALSE(Moves(mind, Kind::FleeFrom, at));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_ExaminePlaceRow);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
}

TEST_F(CreatureMindSystemTest, AMiracleItReactsToTeachesItOnlyWhenTheHandlerSaysSoAndItsMindIsNotStilled)
{
	const game_random::testing::ScopedState state;
	std::ranges::copy(std::string_view("RunAwayFromPos"), _world.Info().creatureAction.at(k_RunFromPlaceRow).name.begin());
	std::ranges::copy(std::string_view("ExaminePos"), _world.Info().creatureAction.at(k_ExaminePlaceRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	const auto& knowledge = mind.learnt->knowledge;
	ASSERT_GT(knowledge.miraclesSeen.size(), 1u);
	const glm::vec3 point(130.0f, 0.0f, 100.0f);
	const glm::vec2 at(point.x, point.z);
	// a nice one: it goes to look, and learns nothing unless told the miracle
	_system.ReactToNiceMagic(creature, point, std::nullopt);
	EXPECT_TRUE(Moves(mind, creature_mind::Movement::Kind::ToPoint, at));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_ExaminePlaceRow);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_FALSE(knowledge.miraclesSeen[1].turn.has_value());
	_system.ReactToNastyMagic(creature, point, std::nullopt);
	EXPECT_FALSE(knowledge.miraclesSeen[1].turn.has_value());
	// stilled, as by the freeze spell, it still goes to look but learns nothing
	mind.paused = true;
	mind.idle.agenda.clear();
	_system.ReactToNiceMagic(creature, point, 1);
	EXPECT_TRUE(Moves(mind, creature_mind::Movement::Kind::ToPoint, at));
	EXPECT_FALSE(knowledge.miraclesSeen[1].turn.has_value());
	mind.paused = false;
	_system.ReactToNiceMagic(creature, point, 1);
	EXPECT_TRUE(knowledge.miraclesSeen[1].turn.has_value());
}

TEST_F(CreatureMindSystemTest, APointAtOrderPointsTheHandsAtTheGroundUnderThePoint)
{
	const auto creature = test::creature_world::World::MakeCreature();
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	Locator::creatureObjectActionSystem::emplace<test::creature_loop_fakes::FakeObjectAction>(log);
	creature_mind::Commands commands;
	commands.object = creature_mind::ObjectOrder {.kind = creature_mind::ObjectOrder::Kind::PointAt, .point = {130.0f, 100.0f}};
	mind_detail::Order(std::as_const(test::creature_world::World::Registry()), creature, commands);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "objectAction.PointAt");
	EXPECT_EQ(log.front().args, (std::vector<float> {static_cast<float>(entt::to_integral(creature)), 130.0f, 0.0f, 100.0f}));
}

TEST_F(CreatureMindSystemTest, APointAtOrderAtAThingPointsAtItForItsSeconds)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = test::creature_world::World::MakeCreature(glm::vec3(140.0f, 0.0f, 120.0f));
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	Locator::creatureObjectActionSystem::emplace<test::creature_loop_fakes::FakeObjectAction>(log);
	creature_mind::Commands commands;
	// At a thing: wherever the thing is, for the order's seconds
	commands.object = creature_mind::ObjectOrder {.kind = creature_mind::ObjectOrder::Kind::PointAt,
	                                              .object = static_cast<uint32_t>(entt::to_integral(thing)),
	                                              .point = {0.0f, 0.0f},
	                                              .seconds = 1.0f};
	mind_detail::Order(std::as_const(test::creature_world::World::Registry()), creature, commands);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "objectAction.PointAtFor");
	EXPECT_EQ(log.front().args, (std::vector<float> {static_cast<float>(entt::to_integral(creature)), 140.0f, 0.0f, 120.0f,
	                                                 1.0f, static_cast<float>(entt::to_integral(thing))}));
	// At a point in the air, for the usual while
	log.clear();
	commands.object = creature_mind::ObjectOrder {
	    .kind = creature_mind::ObjectOrder::Kind::PointAt, .point = {130.0f, 100.0f}, .pointHeight = 20.0f};
	mind_detail::Order(std::as_const(test::creature_world::World::Registry()), creature, commands);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log.front().name, "objectAction.PointAtFor");
	EXPECT_EQ(log.front().args,
	          (std::vector<float> {static_cast<float>(entt::to_integral(creature)), 130.0f, 20.0f, 100.0f, 5.0f, -1.0f}));
}

TEST(CreatureMindBeliefs, ATownDesireFlagIsBelievedInAsAThingOfTheDefaultKind)
{
	// a town desire flag in the cells its eyes sweep: the game's default belief type for a thing on the map, Other
	ecs::Registry registry;
	const auto flag = registry.Create();
	registry.AssignState<ecs::components::TownDesireFlag>(flag);
	EXPECT_EQ(mind_detail::BeliefKindOf(registry, flag), std::optional<uint32_t>(creature_tree::belief_types::k_Other));
}

TEST(CreatureMindLook, EveryBirdAndTheVultureCatchTheEyeAsADove)
{
	// the kinds the original looks at as much as a dove; every other animal as an animal. Traced through the original's
	// animal factory: 0-4, 6 and 8-16, 20-26; the Puzzle ones (27-30) are of the animal class by their vtables. Not
	// traced, so these pin openblack's choice rather than the original's data (docs/wiki/creature/mind.md#pending): Goat (5),
	// Zebra (7) and the Vulture (17), taken by their class's name, and CitadelDove and CitadelBat (18, 19), whose class is
	// unknown
	constexpr std::array k_DoveKinds {AnimalInfo::Crow,    AnimalInfo::Dove,      AnimalInfo::Swallow,
	                                  AnimalInfo::Pigeon,  AnimalInfo::Seagull,   AnimalInfo::Bat,
	                                  AnimalInfo::Vulture, AnimalInfo::SpellDove, AnimalInfo::SpellBat};
	for (int i = 0; i < static_cast<int>(AnimalInfo::_COUNT); ++i)
	{
		const auto type = static_cast<AnimalInfo>(i);
		const auto expected = std::ranges::find(k_DoveKinds, type) != k_DoveKinds.end() ? creature_look::Interest::Dove
		                                                                                : creature_look::Interest::Animal;
		EXPECT_EQ(mind_detail::LookInterestOf(type), expected) << i;
	}
}

TEST_F(CreatureMindSystemTest, ACreatureLooksAtADoveOverACowAndACowOverAVillager)
{
	auto& world = test::creature_world::World::Registry();
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto makeAnimal = [&world](AnimalInfo type, glm::vec3 position) {
		const auto entity = world.Create();
		world.Assign<Animal>(entity, type, 0u);
		world.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	};
	// all three ahead of the creature and within half its range, where how far does not count
	const auto cow = makeAnimal(AnimalInfo::Cow, glm::vec3(-5.0f, 0.0f, -30.0f));
	const auto dove = makeAnimal(AnimalInfo::Dove, glm::vec3(5.0f, 0.0f, -30.0f));
	MakeVillager(glm::vec3(0.0f, 0.0f, -30.0f));

	const auto& registry = std::as_const(world);
	const auto& transform = registry.Get<Transform>(self);
	const creature_look::Viewer viewer {
	    .position = transform.position,
	    .ahead = -(transform.rotation * glm::vec3(0.0f, 0.0f, 1.0f)),
	    .size = 1.0f,
	};
	std::vector<creature_look::Candidate> others;
	std::ranges::copy_if(mind_detail::GatherCandidates(registry), std::back_inserter(others),
	                     [self](const creature_look::Candidate& candidate) { return candidate.id != entt::to_integral(self); });
	ASSERT_EQ(others.size(), 3u);
	const auto cowCandidate = std::ranges::find(others, entt::to_integral(cow), &creature_look::Candidate::id);
	ASSERT_NE(cowCandidate, others.end());
	EXPECT_EQ(cowCandidate->kind, creature_look::Interest::Animal);
	// an animal is looked at where it stands
	EXPECT_EQ(cowCandidate->point, glm::vec3(-5.0f, 0.0f, -30.0f));

	const auto first = creature_look::LookAbout({}, others, viewer, 10.0f);
	ASSERT_TRUE(first.id.has_value());
	EXPECT_EQ(*first.id, entt::to_integral(dove));
	EXPECT_EQ(first.kind, creature_look::Interest::Dove);

	// with the dove gone, the cow beats the villager
	std::erase_if(others,
	              [dove](const creature_look::Candidate& candidate) { return candidate.id == entt::to_integral(dove); });
	const auto second = creature_look::LookAbout({}, others, viewer, 10.0f);
	ASSERT_TRUE(second.id.has_value());
	EXPECT_EQ(*second.id, entt::to_integral(cow));
}

TEST_F(CreatureMindSystemTest, AScriptNameStaysOverTheMindFilesName)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto file = _system.SaveMind(creature);
	ASSERT_TRUE(file.has_value());
	file->name = u"Matey";
	// the file is taken up at the mind's next turn, after the script has named the creature
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	auto& registry = test::creature_world::World::Registry();
	ASSERT_TRUE(ecs::player_creature::SetName(registry, creature, u"Khalen"));
	// saved before then, it is saved with the script's name
	const auto waiting = _system.SaveMind(creature);
	ASSERT_TRUE(waiting.has_value());
	EXPECT_EQ(waiting->name, u"Khalen");
	_system.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	EXPECT_EQ(mind.learnt->name, u"Khalen");
	EXPECT_FALSE(mind.scriptName.has_value());
}

// A swap of minds moves each mind into the other body, while each body keeps its own name, size and alignment and grows
// into the other's; the owners swap with the minds
TEST_F(CreatureMindSystemTest, TheLeashesAMindKnowsGoWithItToTheOtherBodyOnce)
{
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	auto& leashes = Locator::leashSystem::emplace<ecs::systems::LeashSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	_system.ProcessTurn();
	// the first knows every leash, the second none
	leashes.SetKnown(first, LeashType::Rope, true);
	leashes.SetKnown(first, LeashType::Good, true);
	leashes.SetKnown(first, LeashType::Evil, true);
	leashes.SetKnown(second, LeashType::Rope, false);

	ASSERT_TRUE(_system.SwapMinds(first, second));
	// the second body now knows what the first mind knew, and the first none: the leashes moved once, not back again
	EXPECT_TRUE(leashes.Knows(second, LeashType::Rope));
	EXPECT_TRUE(leashes.Knows(second, LeashType::Good));
	EXPECT_TRUE(leashes.Knows(second, LeashType::Evil));
	EXPECT_FALSE(leashes.Knows(first, LeashType::Rope));
	EXPECT_EQ(registry.Get<CreatureLeash>(second).known.count(), 3u);
	EXPECT_EQ(registry.Get<CreatureLeash>(first).known.count(), 0u);
}

TEST_F(CreatureMindSystemTest, SwappedMindsCrossOverWhileEachBodyKeepsItsNameSizeAndAlignment)
{
	const game_random::testing::ScopedState state;
	auto& registry = test::creature_world::World::Registry();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	_system.ProcessTurn();
	ASSERT_TRUE(ecs::player_creature::SetName(registry, first, u"Khalen"));
	ASSERT_TRUE(ecs::player_creature::SetName(registry, second, u"Laetes"));
	registry.Get<CreatureMindState>(first).attitudeToPlayer = 0.75f;
	registry.Get<CreatureMindState>(second).attitudeToPlayer = -0.25f;
	auto& firstBody = registry.Get<Creature>(first);
	auto& secondBody = registry.Get<Creature>(second);
	firstBody.size = 1.5f;
	firstBody.alignment = -0.9f;
	secondBody.size = 0.6f;
	secondBody.alignment = 0.0f;
	// the second takes no notice of reactions, as a script's creature is made
	registry.AssignState<CreatureReactionsOff>(second);

	ASSERT_TRUE(_system.SwapMinds(first, second));
	// now the second takes notice of them and the first does not
	EXPECT_FALSE(registry.AllOf<CreatureReactionsOff>(second));
	EXPECT_TRUE(registry.AllOf<CreatureReactionsOff>(first));
	// the minds crossed over
	EXPECT_FLOAT_EQ(registry.Get<CreatureMindState>(first).attitudeToPlayer, -0.25f);
	EXPECT_FLOAT_EQ(registry.Get<CreatureMindState>(second).attitudeToPlayer, 0.75f);
	// each body kept its own name, size and alignment
	EXPECT_EQ(ecs::creature_naming::NameOf(registry, first), u"Khalen");
	EXPECT_EQ(ecs::creature_naming::NameOf(registry, second), u"Laetes");
	EXPECT_EQ(registry.Get<Creature>(first).size, 1.5f);
	EXPECT_EQ(registry.Get<Creature>(first).alignment, -0.9f);
	EXPECT_EQ(registry.Get<Creature>(second).size, 0.6f);
	EXPECT_EQ(registry.Get<Creature>(second).alignment, 0.0f);
	// and grows into the other's
	const auto& swapFirst = registry.Get<CreatureSwap>(first);
	EXPECT_EQ(swapFirst.other, second);
	EXPECT_EQ(swapFirst.target.size, 0.6f);
	EXPECT_EQ(swapFirst.target.alignment, 0.0f);
	const auto& swapSecond = registry.Get<CreatureSwap>(second);
	EXPECT_EQ(swapSecond.other, first);
	EXPECT_EQ(swapSecond.target.size, 1.5f);
	EXPECT_EQ(swapSecond.target.alignment, -0.9f);
	// the owners swapped
	EXPECT_EQ(registry.Get<Creature>(first).owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(registry.Get<Creature>(second).owner, PlayerNames::PLAYER_ONE);
	// a creature does not swap with itself
	EXPECT_FALSE(_system.SwapMinds(first, first));
}

// What the original's mind file keeps goes with the mind: its stage of growing up, its counts of kills, poos,
// mushrooms, fights fought and won, whether it is in its growing-up scripts, its fight health and the wounds and blood
// on its skin. How it leans in fights stays with the body. The player's creature is now the second, and the first is
// left to no one, taking no notice of reactions, to be offered again
TEST_F(CreatureMindSystemTest, SwappedMindsTakeTheirStageCountsAndFightHealthToTheOtherBody)
{
	const game_random::testing::ScopedState state;
	auto& registry = test::creature_world::World::Registry();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	_system.ProcessTurn();
	registry.Get<CreatureMindState>(first).developmentPhase = 13;
	registry.Get<CreatureMindState>(second).developmentPhase = 0;
	auto& tallies = registry.AssignOrReplaceState<CreatureTallies>(first);
	tallies.meals.peopleKilled = 4;
	tallies.meals.animalsKilled = 2;
	tallies.meals.creaturesKilled = 1;
	tallies.poos = 9;
	tallies.mushroomsEaten = 3;
	registry.AssignOrReplaceState<CreatureFightRecord>(first, CreatureFightRecord {.tendency = 0.5f, .fights = 6, .wins = 5});
	registry.AssignOrReplaceState<CreatureFightRecord>(second, CreatureFightRecord {.tendency = -0.25f, .fights = 1});
	registry.Get<Creature>(first).inDevScript = true;
	registry.AssignOrReplaceState<CreatureFightHealth>(first, CreatureFightHealth {.health = 0.4f});
	registry.AssignState<CreatureReactionsOff>(second);
	auto& marks = registry.AssignOrReplaceState<CreatureMarks>(first);
	marks.marks.wounds.push_back({.u = 10, .v = 20, .skin = 1, .age = 5, .type = 2, .column = 3});
	marks.marks.blood.push_back({.u = 30, .v = 40});
	marks.marks.counts = 250;
	registry.AssignOrReplaceState<CreatureMarks>(second).marks.counts = 70;

	ASSERT_TRUE(_system.SwapMinds(first, second));
	// the wounds and blood go with the mind, each body keeping how far it is towards its next step of healing
	const auto& marksSecond = registry.Get<CreatureMarks>(second);
	ASSERT_EQ(marksSecond.marks.wounds.size(), 1u);
	EXPECT_EQ(marksSecond.marks.wounds[0].column, 3u);
	EXPECT_EQ(marksSecond.marks.blood.size(), 1u);
	EXPECT_EQ(marksSecond.marks.counts, 70u);
	EXPECT_EQ(marksSecond.revision, 1u);
	const auto& marksFirst = registry.Get<CreatureMarks>(first);
	EXPECT_TRUE(marksFirst.marks.wounds.empty());
	EXPECT_TRUE(marksFirst.marks.blood.empty());
	EXPECT_EQ(marksFirst.marks.counts, 250u);
	EXPECT_EQ(registry.Get<CreatureMindState>(second).developmentPhase, 13u);
	EXPECT_EQ(registry.Get<CreatureMindState>(first).developmentPhase, 0u);
	const auto& moved = registry.Get<CreatureTallies>(second);
	EXPECT_EQ(moved.meals.peopleKilled, 4u);
	EXPECT_EQ(moved.meals.animalsKilled, 2u);
	EXPECT_EQ(moved.meals.creaturesKilled, 1u);
	EXPECT_EQ(moved.poos, 9u);
	EXPECT_EQ(moved.mushroomsEaten, 3u);
	const auto& left = registry.Get<CreatureTallies>(first);
	EXPECT_EQ(left.meals.peopleKilled, 0u);
	EXPECT_EQ(left.poos, 0u);
	const auto& recordSecond = registry.Get<CreatureFightRecord>(second);
	EXPECT_EQ(recordSecond.fights, 6u);
	EXPECT_EQ(recordSecond.wins, 5u);
	EXPECT_EQ(recordSecond.tendency, -0.25f);
	const auto& recordFirst = registry.Get<CreatureFightRecord>(first);
	EXPECT_EQ(recordFirst.fights, 1u);
	EXPECT_EQ(recordFirst.wins, 0u);
	EXPECT_EQ(recordFirst.tendency, 0.5f);
	EXPECT_TRUE(registry.Get<Creature>(second).inDevScript);
	EXPECT_FALSE(registry.Get<Creature>(first).inDevScript);
	ASSERT_TRUE(registry.AllOf<CreatureFightHealth>(second));
	EXPECT_EQ(registry.Get<CreatureFightHealth>(second).health, 0.4f);
	ASSERT_TRUE(registry.AllOf<CreatureFightHealth>(first));
	EXPECT_EQ(registry.Get<CreatureFightHealth>(first).health, CreatureFightHealth {}.health);
	// the second is the player's, the first nobody's and deaf to reactions, as an offered creature is
	EXPECT_EQ(registry.Get<Creature>(second).owner, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(registry.Get<Creature>(first).owner, PlayerNames::NEUTRAL);
	EXPECT_TRUE(registry.AllOf<CreatureReactionsOff>(first));
	EXPECT_FALSE(registry.AllOf<CreatureReactionsOff>(second));
}

TEST_F(CreatureMindSystemTest, TwoCreaturesOfOnePlayerSwappedLeaveThatPlayerWithTheFirst)
{
	const test::RestoreService<Locator::playerSystem> restorePlayers;
	Locator::playerSystem::emplace<ecs::systems::PlayerSystem>();
	const game_random::testing::ScopedState state;
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	_system.ProcessTurn();
	auto& players = Locator::playerSystem::value();
	players.SetPrimaryCreature(PlayerNames::PLAYER_TWO, second);
	ASSERT_TRUE(_system.SwapMinds(first, second));
	// the player is given the second, then the first
	EXPECT_EQ(players.GetPrimaryCreature(PlayerNames::PLAYER_TWO), std::optional(first));
	EXPECT_EQ(test::creature_world::World::Registry().Get<Creature>(first).owner, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(test::creature_world::World::Registry().Get<Creature>(second).owner, PlayerNames::PLAYER_TWO);
}

// As each player is given the other creature, the totems of that player's towns wear its icon: the first creature's
// player first, then the second's; a creature of no player gives nobody's totems anything
TEST_F(CreatureMindSystemTest, SwappedMindsSetEachPlayersTotemsAsThePlayerIsGivenTheOtherCreature)
{
	const test::RestoreService<Locator::villageTotemSystem> restoreTotems;
	auto& totems =
	    static_cast<test::RecordingVillageTotems&>(Locator::villageTotemSystem::emplace<test::RecordingVillageTotems>());
	const game_random::testing::ScopedState state;
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	const auto third = test::creature_world::World::MakeCreature(glm::vec3(140.0f, 0.0f, 100.0f), PlayerNames::PLAYER_THREE);
	const auto nobodys = test::creature_world::World::MakeCreature(glm::vec3(160.0f, 0.0f, 100.0f), PlayerNames::_COUNT);
	_system.ProcessTurn();
	totems.creaturesSet.clear();
	ASSERT_TRUE(_system.SwapMinds(first, second));
	EXPECT_EQ(totems.creaturesSet, (std::vector {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO}));
	// no swap, no totem
	totems.creaturesSet.clear();
	EXPECT_FALSE(_system.SwapMinds(first, first));
	EXPECT_TRUE(totems.creaturesSet.empty());
	// the player's creature for one of no player: only the player's totems
	ASSERT_TRUE(_system.SwapMinds(third, nobodys));
	EXPECT_EQ(totems.creaturesSet, std::vector {PlayerNames::PLAYER_THREE});
}

// As each body takes up the other's mind, the mind's saved sources draw twice each, as a mind file read: the first
// creature takes up the second's mind first, so the second's sources draw first, then the first's, each in its desires'
// order, the value's spread then the threshold's, before anything else the swap draws
TEST_F(CreatureMindSystemTest, SwappedMindsDrawForTheSecondsSavedSourcesThenTheFirsts)
{
	const game_random::testing::ScopedState state;
	auto& registry = test::creature_world::World::Registry();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	_system.ProcessTurn();
	// each mind holds only the sources given here, of types whose spreads tell them apart
	const auto give = [&registry](entt::entity creature, std::vector<std::pair<size_t, uint32_t>> sources) {
		auto& desires = registry.Get<CreatureMindState>(creature).desires;
		ASSERT_TRUE(desires.has_value());
		for (auto& desire : desires->desires)
		{
			desire.sources.clear();
		}
		for (const auto& [desire, type] : sources)
		{
			desires->desires.at(desire).sources.push_back({.type = type, .value = 0.5f, .threshold = 0.5f});
		}
	};
	give(first, {{0, 3}, {5, 7}});
	give(second, {{2, 9}, {2, 3}});
	auto& table = _world.Info().desireSourceTable;
	table.at(3).field0xc = 0.05f;
	table.at(3).field0x10 = 0.15f;
	table.at(7).field0xc = 0.25f;
	table.at(7).field0x10 = 0.35f;
	table.at(9).field0xc = 0.45f;
	table.at(9).field0x10 = 0.55f;
	std::vector<float> floats;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float x) {
		                                  floats.push_back(x);
		                                  return 0.0f;
	                                  });

	ASSERT_TRUE(_system.SwapMinds(first, second));
	const std::vector<float> expected {0.9f, 1.1f, 0.1f, 0.3f, 0.1f, 0.3f, 0.5f, 0.7f};
	ASSERT_GE(floats.size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_FLOAT_EQ(floats.at(i), expected.at(i)) << i;
	}
}

TEST_F(CreatureMindSystemTest, AMindFileKeepsTheDecaysDrawnForTheCreature)
{
	const game_random::testing::ScopedState state;
	// a quarter of each range, not its middle
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float x) { return x * 0.25f; });
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto file = _system.SaveMind(creature);
	ASSERT_TRUE(file.has_value());
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	_system.ProcessTurn();
	const auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto range = (0.9f + (0.001f * static_cast<float>(d + 1))) - 0.9f;
		EXPECT_FLOAT_EQ(mind.desires->desires.at(d).decay, 0.9f + (range * 0.25f)) << d;
	}
}

TEST_F(CreatureMindSystemTest, WhatAScriptTeachesStaysOverTheMindFiles)
{
	const game_random::testing::ScopedState state;
	// a miracle (by its magic type, its row) that must be seen ten times, before the species' multiplier
	_world.Info().creatureMagicActionKnownAboutEntry.at(10).field0x44 = 10;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	// before the mind's first turn it waits
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	EXPECT_EQ(registry.Get<CreatureMindState>(creature).scriptKnows.size(), 1u);
	_system.ProcessTurn();
	{
		const auto& mind = registry.Get<CreatureMindState>(creature);
		ASSERT_TRUE(mind.learnt.has_value());
		EXPECT_TRUE(mind.learnt->knowledge.skillsKnown.at(4));
		EXPECT_TRUE(mind.scriptKnows.empty());
	}
	auto file = _system.SaveMind(creature);
	ASSERT_TRUE(file.has_value());
	file->known.at(0).clear();
	file->known.at(1).clear();
	// the file is taken up at the mind's next turn, after the script has taught the creature
	_system.LoadMind(creature, std::make_shared<const creaturemind::MindFileData>(*file));
	ASSERT_TRUE(_system.SetKnowsAction(creature, 1, 10, true));
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, false));
	const auto taught = creature_watching::TaughtSightings(
	    10, creature_mind_tables::MiracleMultiplier(creature::InfoRow(CreatureType::GiantApe)));
	// saved before then, it is saved as taught
	const auto waiting = _system.SaveMind(creature);
	ASSERT_TRUE(waiting.has_value());
	ASSERT_EQ(waiting->known.at(1).size(), 1u);
	EXPECT_EQ(waiting->known.at(1).front().id, 10u);
	EXPECT_EQ(waiting->miraclesSeen.at(10).count, taught);
	EXPECT_TRUE(waiting->known.at(0).empty());
	_system.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	EXPECT_TRUE(mind.learnt->knowledge.miraclesKnown.at(10));
	EXPECT_EQ(mind.learnt->knowledge.miraclesSeen.at(10).count, taught);
	// in order: taught, then taken away
	EXPECT_FALSE(mind.learnt->knowledge.skillsKnown.at(4));
	EXPECT_TRUE(mind.scriptKnows.empty());
	// set up, with no file to take up, it is at once
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	EXPECT_TRUE(mind.learnt->knowledge.skillsKnown.at(4));
	EXPECT_TRUE(mind.scriptKnows.empty());
}

TEST_F(CreatureMindSystemTest, AScriptTeachingEverythingMakesEverySkillKnownAndEveryMiracleKnownAboutWithoutSightings)
{
	const game_random::testing::ScopedState state;
	// a miracle that must be seen ten times: taught by CREATURE_SET_KNOWS_ACTION it would count as seen that often
	_world.Info().creatureMagicActionKnownAboutEntry.at(10).field0x44 = 10;
	auto& registry = test::creature_world::World::Registry();
	// before the mind's first turn it waits, as one edit a skill or a miracle row
	const auto waiting = test::creature_world::World::MakeCreature();
	ASSERT_TRUE(_system.LearnEverything(waiting));
	EXPECT_EQ(registry.Get<CreatureMindState>(waiting).scriptKnows.size(), 6u + 42u);
	_system.ProcessTurn();
	EXPECT_TRUE(registry.Get<CreatureMindState>(waiting).scriptKnows.empty());
	// set up, with no file to take up, it is at once
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.learnt.has_value());
	std::vector<uint32_t> before;
	std::ranges::transform(mind.learnt->knowledge.miraclesSeen, std::back_inserter(before),
	                       [](const creature_watching::Sighting& seen) { return seen.count; });
	ASSERT_TRUE(_system.LearnEverything(creature));
	EXPECT_TRUE(mind.scriptKnows.empty());
	for (const auto taught : {waiting, creature})
	{
		const auto& knowledge = registry.Get<CreatureMindState>(taught).learnt->knowledge;
		ASSERT_FALSE(knowledge.skillsKnown.empty());
		ASSERT_FALSE(knowledge.miraclesKnown.empty());
		EXPECT_TRUE(std::ranges::all_of(knowledge.skillsKnown, [](bool known) { return known; }));
		EXPECT_TRUE(std::ranges::all_of(knowledge.miraclesKnown, [](bool known) { return known; }));
	}
	// known about only: no sighting counted
	std::vector<uint32_t> after;
	std::ranges::transform(mind.learnt->knowledge.miraclesSeen, std::back_inserter(after),
	                       [](const creature_watching::Sighting& seen) { return seen.count; });
	EXPECT_EQ(after, before);
	// anything but a creature is not taught
	EXPECT_FALSE(_system.LearnEverything(MakeVillager(glm::vec3(0.0f))));
	EXPECT_FALSE(_system.LearnEverything(entt::null));
}

TEST_F(CreatureMindSystemTest, OnlyACreatureIsTaught)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = MakeVillager(glm::vec3(0.0f));
	EXPECT_FALSE(_system.SetKnowsAction(villager, 0, 4, true));
	EXPECT_FALSE(_system.SetKnowsAction(entt::null, 0, 4, true));
	EXPECT_FALSE(registry.AllOf<CreatureMindState>(villager));
}

namespace
{
/// The advisor, keeping the messages raised
class RecordingHelp final: public ecs::systems::CreatureHelpSystemInterface
{
public:
	void Raise(entt::entity creature, const creature_help::Message& message) override
	{
		raised.emplace_back(creature, message);
	}
	void ProcessTurn() override {}
	void PromptKnown(entt::entity /*creature*/, creature_help::Prompt /*prompt*/) override {}
	void Fainting(entt::entity /*creature*/) override {}
	void SetHelpOn(bool /*on*/) override {}
	[[nodiscard]] bool IsHelpOn() const override { return true; }
	[[nodiscard]] const std::vector<Told>& GetTold() const override { return _told; }

	std::vector<std::pair<entt::entity, creature_help::Message>> raised;

private:
	std::vector<Told> _told;
};
} // namespace

// What a script teaches it that it did not know is a lesson the advisor tells of: a skill or a miracle by its number,
// at once when its mind is set up, else once its mind has it; what it knew already, what it is made to forget and what
// the tables don't have are not, nor the debug tools' teaching
TEST_F(CreatureMindSystemTest, EachCreatureAMindIsSwappedIntoTellsTheMessageOfThatMindsStage)
{
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<RecordingHelp&>(Locator::creatureHelpSystem::emplace<RecordingHelp>());
	const game_random::testing::ScopedState state;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::reset();
	Locator::leashSystem::emplace<ecs::systems::LeashSystem>();
	auto& registry = test::creature_world::World::Registry();
	const auto first = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto second = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::NEUTRAL);
	_system.ProcessTurn();
	registry.Get<CreatureMindState>(first).developmentPhase = 3;
	registry.Get<CreatureMindState>(second).developmentPhase = 1;
	help.raised.clear();
	ASSERT_TRUE(_system.SwapMinds(first, second));
	// the first creature now has the second's mind, at stage 1, and the second the first's, at stage 3
	std::vector<std::pair<entt::entity, uint32_t>> stages;
	for (const auto& [about, message] : help.raised)
	{
		if (creature_help::IsStageMessage(message.kind))
		{
			stages.emplace_back(about,
			                    static_cast<uint32_t>(message.kind) - static_cast<uint32_t>(creature_help::Kind::StageInitial));
		}
	}
	EXPECT_EQ(stages, (std::vector<std::pair<entt::entity, uint32_t>> {{first, 1}, {second, 3}}));
}

// A mind file the creature-file load gave a creature tells the player the stage it holds once the mind takes it up;
// a file taken up otherwise (the player's own creature loaded from its profile) tells nothing
TEST_F(CreatureMindSystemTest, AFileTheCreatureFileLoadGaveTellsTheStageItHolds)
{
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<RecordingHelp&>(Locator::creatureHelpSystem::emplace<RecordingHelp>());
	auto& registry = test::creature_world::World::Registry();
	const auto loaded = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE);
	const auto own = test::creature_world::World::MakeCreature(glm::vec3(120.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	auto file = std::make_shared<creaturemind::MindFileData>();
	file->developmentPhase = 4;
	for (const auto creature : {loaded, own})
	{
		registry.Get<CreatureMindState>(creature).pendingFile = file;
	}
	registry.Get<CreatureMindState>(loaded).fileTellsStage = true;
	_system.ProcessTurn();
	std::vector<std::pair<entt::entity, uint32_t>> stages;
	for (const auto& [about, message] : help.raised)
	{
		if (creature_help::IsStageMessage(message.kind))
		{
			stages.emplace_back(about,
			                    static_cast<uint32_t>(message.kind) - static_cast<uint32_t>(creature_help::Kind::StageInitial));
		}
	}
	EXPECT_EQ(stages, (std::vector<std::pair<entt::entity, uint32_t>> {{loaded, 4}}));
	EXPECT_FALSE(registry.Get<CreatureMindState>(loaded).fileTellsStage);
}

TEST(CreatureMindEyes, SleepyEyesDroopTheLidsAndSlowTheBlinkingAndLeaveTheEyesModeAlone)
{
	CreatureEyes eyes;
	eyes.mode = creature_eyes::Mode::Wide;
	mind_detail::ApplyEyes(&eyes, creature_mind::Eyes::Sleepy);
	// as the game's just-woken look: the lids at -0.7, a blink every 2.5 s, the mode as it was
	EXPECT_EQ(eyes.mode, creature_eyes::Mode::Wide);
	EXPECT_FLOAT_EQ(eyes.openness, -0.7f);
	EXPECT_EQ(eyes.blink.intervalMs, 2500);
	// normal eyes are calm again, the lids and the blinking as they start
	mind_detail::ApplyEyes(&eyes, creature_mind::Eyes::Normal);
	EXPECT_EQ(eyes.mode, creature_eyes::Mode::Calm);
	EXPECT_FLOAT_EQ(eyes.openness, 0.0f);
}

TEST_F(CreatureMindSystemTest, WhatAScriptTeachesItAnewTheAdvisorTellsOfAsALesson)
{
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<RecordingHelp&>(Locator::creatureHelpSystem::emplace<RecordingHelp>());
	const auto creature = test::creature_world::World::MakeCreature();
	// the lessons told so far, by kind and number
	const auto lessons = [&help, creature]() {
		std::vector<std::pair<creature_help::Kind, uint32_t>> told;
		for (const auto& [about, message] : help.raised)
		{
			if (message.kind == creature_help::Kind::LessonNormalAction ||
			    message.kind == creature_help::Kind::LessonMagicAction)
			{
				EXPECT_EQ(about, creature);
				EXPECT_FALSE(message.forced);
				told.emplace_back(message.kind, message.parameter);
			}
		}
		return told;
	};
	using Told = std::vector<std::pair<creature_help::Kind, uint32_t>>;
	constexpr auto k_Skill = creature_help::Kind::LessonNormalAction;
	constexpr auto k_Miracle = creature_help::Kind::LessonMagicAction;
	// before the mind's first turn it waits, and is told of as the mind takes it up
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	EXPECT_TRUE(lessons().empty());
	_system.ProcessTurn();
	EXPECT_EQ(lessons(), (Told {{k_Skill, 4}}));
	// set up, at once
	ASSERT_TRUE(_system.SetKnowsAction(creature, 1, 10, true));
	EXPECT_EQ(lessons(), (Told {{k_Skill, 4}, {k_Miracle, 10}}));
	// known already, forgotten, or of a kind the lists don't hold: nothing
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, false));
	ASSERT_TRUE(_system.SetKnowsAction(creature, 2, 1, true));
	EXPECT_EQ(lessons().size(), 2u);
	// a skill past the tables is learnt and told of, as the original's list takes any action number
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 6, true));
	EXPECT_EQ(lessons(), (Told {{k_Skill, 4}, {k_Miracle, 10}, {k_Skill, 6}}));
	// learnt again after it was forgotten, it is told of again
	ASSERT_TRUE(_system.SetKnowsAction(creature, 0, 4, true));
	EXPECT_EQ(lessons(), (Told {{k_Skill, 4}, {k_Miracle, 10}, {k_Skill, 6}, {k_Skill, 4}}));
	// the debug tools teach without a word
	_system.KnowMiracle(creature, 11);
	EXPECT_TRUE(
	    test::creature_world::World::Registry().Get<CreatureMindState>(creature).learnt->knowledge.miraclesKnown.at(11));
	EXPECT_EQ(lessons().size(), 4u);
}

// A script teaching it everything tells the player of none of it, waiting for the mind or not
TEST_F(CreatureMindSystemTest, AScriptTeachingEverythingTellsNoLesson)
{
	const test::RestoreService<Locator::creatureHelpSystem> restoreHelp;
	auto& help = static_cast<RecordingHelp&>(Locator::creatureHelpSystem::emplace<RecordingHelp>());
	const auto isLesson = [](const std::pair<entt::entity, creature_help::Message>& raised) {
		return raised.second.kind == creature_help::Kind::LessonNormalAction ||
		       raised.second.kind == creature_help::Kind::LessonMagicAction;
	};
	const auto waiting = test::creature_world::World::MakeCreature();
	ASSERT_TRUE(_system.LearnEverything(waiting));
	_system.ProcessTurn();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	ASSERT_TRUE(_system.LearnEverything(creature));
	auto& registry = test::creature_world::World::Registry();
	for (const auto taught : {waiting, creature})
	{
		const auto& knowledge = registry.Get<CreatureMindState>(taught).learnt->knowledge;
		EXPECT_TRUE(std::ranges::all_of(knowledge.skillsKnown, [](bool known) { return known; }));
		EXPECT_TRUE(std::ranges::all_of(knowledge.miraclesKnown, [](bool known) { return known; }));
	}
	EXPECT_TRUE(std::ranges::none_of(help.raised, isLesson));
}

TEST(CreatureMindCurse, NoCurseRunningKeepsNothing)
{
	const auto saved = mind_detail::CurseValuesToSave([](std::string_view) { return false; },
	                                                  [](std::string_view) { return std::optional<float>(1.0f); });
	EXPECT_FALSE(saved.has_value());
}

TEST(CreatureMindCurse, TheCurseKeepsTheCreatureItFound)
{
	const std::map<std::string, float, std::less<>> globals {
	    {"OriginalSizeOfMyCreature", 30.0f},
	    {"OriginalStrengthOfMyCreature", 0.75f},
	    {"OriginalAlignmentOfMyCreature", -0.5f},
	};
	std::vector<std::string> asked;
	const auto global = [&globals](std::string_view name) -> std::optional<float> {
		const auto found = globals.find(name);
		return found != globals.end() ? std::optional<float>(found->second) : std::nullopt;
	};
	const auto saved = mind_detail::CurseValuesToSave(
	    [&asked](std::string_view script) {
		    asked.emplace_back(script);
		    return script == "CreatureCurse";
	    },
	    global);
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(asked, std::vector<std::string> {"CreatureCurse"});
	// The size is kept as a height: twice the height of a creature of size one is size two
	EXPECT_FLOAT_EQ(saved->size, 2.0f);
	EXPECT_FLOAT_EQ(saved->strength, 0.75f);
	EXPECT_FLOAT_EQ(saved->alignment, -0.5f);
}

TEST(CreatureMindCurse, AGlobalTheCurseDoesNotHaveIsZero)
{
	const auto saved = mind_detail::CurseValuesToSave([](std::string_view) { return true; },
	                                                  [](std::string_view name) -> std::optional<float> {
		                                                  return name == "OriginalStrengthOfMyCreature"
		                                                             ? std::optional<float>(0.25f)
		                                                             : std::nullopt;
	                                                  });
	ASSERT_TRUE(saved.has_value());
	EXPECT_FLOAT_EQ(saved->size, 0.0f);
	EXPECT_FLOAT_EQ(saved->strength, 0.25f);
	EXPECT_FLOAT_EQ(saved->alignment, 0.0f);
}

TEST_F(CreatureMindSystemTest, ADesireMadeDominantIsCountedEachTurnUntilItsTimeIsUp)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	// with none, a turn leaves the count alone
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());
	_system.ProcessTurn();
	EXPECT_EQ(mind.dominantDesire.turns, 0u);

	// two seconds at ten turns a second: let go of again each turn, and gone on the turn the count reaches 30
	mind.dominantDesire =
	    creature_spell_mind::SetCheatDominant(*mind.desires, creature_desires::Desire::Anger, false, 10.0f, 0.0f, 2.0f);
	ASSERT_GT((*mind.desires)[creature_desires::Desire::Play].suppressedTurns, 0u);
	for (uint32_t turn = 1; turn < 30; ++turn)
	{
		_system.ProcessTurn();
		ASSERT_TRUE(mind.dominantDesire.desire.has_value()) << turn;
		EXPECT_EQ(mind.dominantDesire.turns, turn);
		EXPECT_EQ((*mind.desires)[creature_desires::Desire::Anger].suppressedTurns, 0u);
	}
	_system.ProcessTurn();
	EXPECT_FALSE(mind.dominantDesire.desire.has_value());
	EXPECT_EQ((*mind.desires)[creature_desires::Desire::Play].suppressedTurns, 0u);
}

TEST_F(CreatureMindSystemTest, ANextStepWaitsWhileTheHandsStillActOnAThing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_loop_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_loop_fakes::FakeObjectAction>(log));
	// told to turn to a point while its hands are still half way through an action on a thing
	_system.FocusByScript(creature, glm::vec3(30.0f, 0.0f, 40.0f));
	ASSERT_EQ(mind.idle.agenda.size(), 1u);
	ASSERT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::TurnToFacePoint);
	hands.progress = 0.5f;
	for (int turn = 0; turn < 3; ++turn)
	{
		_system.ProcessTurn();
		EXPECT_FALSE(mind.idle.stepStarted) << turn;
		EXPECT_EQ(mind.idle.step, 0u) << turn;
	}
	// the hands' animations over, the turn starts once nothing else holds the body
	hands.progress.reset();
	for (int turn = 0; turn < 100 && !mind.idle.stepStarted; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_TRUE(mind.idle.stepStarted);
}

TEST_F(CreatureMindSystemTest, AScriptsFocusControlsItTurnsItAndStillsItsMindUntilReleased)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	// a desire a free turn changes: activated, with nothing driving it, it decays to a half of itself each turn
	auto& watched = (*mind.desires)[creature_desires::Desire::Anger];
	watched.activated = true;
	watched.sources.clear();
	watched.decay = 0.5f;
	watched.value = 0.5f;
	_system.ProcessTurn();
	ASSERT_FLOAT_EQ(watched.value, 0.25f);
	const glm::vec3 point {30.0f, 0.0f, 40.0f};
	_system.FocusByScript(creature, point);
	// under the script's control, obeying the player by following them, about to turn to the point
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	ASSERT_TRUE(mind.scriptFocus.has_value());
	EXPECT_EQ(*mind.scriptFocus, point);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, creature_planner::k_FollowPlayerAction);
	ASSERT_EQ(mind.idle.agenda.size(), 1u);
	EXPECT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::TurnToFacePoint);
	// the turn is the plan's, carried out to its end as a plan
	EXPECT_TRUE(mind.planActive);
	EXPECT_EQ(mind.planSerial, mind.idle.serial);
	// its desires are left as they were while it is controlled
	watched.value = 0.5f;
	_system.ProcessTurn();
	EXPECT_FLOAT_EQ(watched.value, 0.5f);
	// released, its focus is off and what it was doing given up
	ecs::script_held::SetControlledByScript(creature, false);
	_system.ReleasedByScript(creature);
	EXPECT_FALSE(mind.scriptFocus.has_value());
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.idle.agenda.empty());
	_system.ProcessTurn();
	EXPECT_NE(watched.value, 0.5f);
}

TEST_F(CreatureMindSystemTest, AScriptsFocusOnAThingHoldsItsHeadWhereTheThingStandsUntilReleasedOrGone)
{
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto villager = MakeVillager({150.0f, 2.0f, 100.0f});
	_system.FocusOnThingByScript(creature, villager);
	// under the script's control, about to turn to the thing and hold still two seconds; the point focus is gone
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	EXPECT_FALSE(mind.scriptFocus.has_value());
	ASSERT_TRUE(mind.scriptFocusThing.has_value());
	EXPECT_EQ(*mind.scriptFocusThing, villager);
	// it comes to know the thing it is told to look at
	EXPECT_NE(mind.beliefs.Find(villager), nullptr);
	ASSERT_EQ(mind.idle.agenda.size(), 1u);
	EXPECT_EQ(mind.idle.agenda.front().movement.kind, creature_mind::Movement::Kind::TurnToFaceObject);
	EXPECT_FLOAT_EQ(mind.idle.agenda.front().seconds, creature_mind::k_FaceThingSeconds);
	// the head on where the thing stands, not its top
	_system.ProcessTurn();
	const auto& animation = registry.Get<CreatureAnimation>(creature);
	ASSERT_TRUE(animation.lookAt.has_value());
	EXPECT_EQ(*animation.lookAt, glm::vec3(150.0f, 2.0f, 100.0f));
	// a point focus takes the thing's place: one focus at a time
	_system.FocusByScript(creature, {30.0f, 0.0f, 40.0f});
	EXPECT_FALSE(mind.scriptFocusThing.has_value());
	_system.FocusOnThingByScript(creature, villager);
	EXPECT_FALSE(mind.scriptFocus.has_value());
	// released from the focus only: still under the script's control
	_system.ReleaseFocusByScript(creature);
	EXPECT_FALSE(mind.scriptFocusThing.has_value());
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	// the thing gone: the focus lets go by itself
	_system.FocusOnThingByScript(creature, villager);
	registry.Destroy(villager);
	_system.ProcessTurn();
	EXPECT_FALSE(mind.scriptFocusThing.has_value());
}

TEST_F(CreatureMindSystemTest, AScriptsFocusAndReleaseStartThePassOverItsDesiresAgain)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	// the desires a pass goes over, the first one last in the list
	std::vector<creature_desires::Desire> full;
	for (size_t d = creature_desires::k_DesireCount; d > 0; --d)
	{
		if (creature_planner::PassDesires(*mind.desires).at(d - 1))
		{
			full.push_back(static_cast<creature_desires::Desire>(d - 1));
		}
	}
	mind.planner.remaining.clear();
	_system.FocusByScript(creature, {10.0f, 0.0f, 10.0f});
	EXPECT_EQ(mind.planner.remaining, full);
	mind.planner.remaining.clear();
	ecs::script_held::SetControlledByScript(creature, false);
	_system.ReleasedByScript(creature);
	EXPECT_EQ(mind.planner.remaining, full);
}

TEST_F(CreatureMindSystemTest, AScriptsFocusIsCompletedAsAPlanOnceTheTurnIsOver)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// right on the point it has no turn to make: the step is done on the turn after it starts, and the plan with it
	_system.FocusByScript(creature, {100.0f, 0.0f, 100.0f});
	ASSERT_TRUE(mind.planActive);
	for (int turn = 0; turn < 5 && mind.planActive; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.idle.gaveUp);
	EXPECT_GE(mind.idle.step, mind.idle.agenda.size());
	// the head stays on the point until the release
	EXPECT_TRUE(mind.scriptFocus.has_value());
}

// A plan forced on it ends what it was doing as an action finished unsuccessfully: what the leash had it do once its walk
// was over goes too (docs/wiki/creature/leash.md, ForceActivityAndForceAction)
TEST_F(CreatureMindSystemTest, APlanForcedOnItGivesUpWhatTheLeashHadItDoAfterItsWalk)
{
	constexpr size_t k_ReflectionRow = 44;
	std::ranges::copy(std::string_view("LookAtReflection"), _world.Info().creatureAction.at(k_ReflectionRow).name.begin());
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	auto& leash = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	leash.recordOrderGivenUp = true;
	const auto creature = test::creature_world::World::MakeCreature();
	std::ignore = _system.ForceActivity(creature, {.desire = creature_desires::Desire::Water, .action = "LookAtReflection"});
	ASSERT_FALSE(log.empty());
	EXPECT_EQ(log.front().name, "leash.OrderGivenUp");
}

// Kept within an area, a plan that walks it out of the area can't be made: what it was doing ends, it does nothing, and
// it does not take that action again while it is kept within an area, asked as the planner asks; free of the area, it
// takes it (docs/wiki/creature/leash.md#tying-the-leash-in-openblack)
TEST_F(CreatureMindSystemTest, KeptWithinAnAreaAPlanThatWalksItOutCantBeMadeAndTheActionIsNotTakenAgain)
{
	constexpr size_t k_ReflectionRow = 44;
	std::ranges::copy(std::string_view("LookAtReflection"), _world.Info().creatureAction.at(k_ReflectionRow).name.begin());
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log);
	auto& registry = test::creature_world::World::Registry();
	// at (100, 100); with no map of the land the sea and the coast are the map's low corner, 141 away
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& kept = registry.AssignOrReplace<CreatureLeash>(creature);
	kept.confinementCentre = glm::vec3(100.0f, 0.0f, 100.0f);
	const CreatureMindSystem::ForcedActivity reflection {.desire = creature_desires::Desire::Water,
	                                                     .action = "LookAtReflection"};
	auto chosen = reflection;
	chosen.checkedAsChosen = true;

	// kept within 150, the walks stay inside
	kept.confinementRadius = 150.0f;
	ASSERT_TRUE(_system.ForceActivity(creature, reflection));
	EXPECT_FALSE(mind.idle.agenda.empty());
	EXPECT_TRUE(mind.unavailableWhenConfined.empty());

	// kept within 100, they leave it: no plan, nothing to do, and the action is given up while it is kept
	kept.confinementRadius = 100.0f;
	EXPECT_FALSE(_system.ForceActivity(creature, reflection));
	EXPECT_TRUE(mind.idle.agenda.empty());
	ASSERT_GT(mind.unavailableWhenConfined.size(), k_ReflectionRow);
	EXPECT_TRUE(mind.unavailableWhenConfined[k_ReflectionRow]);
	EXPECT_FALSE(_system.ForceActivity(creature, chosen));

	// kept within no area, it takes it again, asked as the planner asks
	kept.confinementRadius = 0.0f;
	EXPECT_TRUE(_system.ForceActivity(creature, chosen));
	EXPECT_FALSE(mind.idle.agenda.empty());
}

// A forced plan that does not end what the creature does leaves its body's action playing, and its first walk takes up
// the walk under way; one that does cuts the body's action short
TEST_F(CreatureMindSystemTest, AForcedPlanThatLeavesWhatItDoesGoingKeepsTheBodyAndTakesUpTheWalk)
{
	constexpr size_t k_ReflectionRow = 44;
	std::ranges::copy(std::string_view("LookAtReflection"), _world.Info().creatureAction.at(k_ReflectionRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	CreatureMindSystem::ForcedActivity reflection {.desire = creature_desires::Desire::Water, .action = "LookAtReflection"};
	reflection.endsWhatItDoes = false;
	animation.body = *creature_layers::PlaySequence({}, 10, 11, 12);
	ASSERT_TRUE(_system.ForceActivity(creature, reflection));
	EXPECT_TRUE(creature_layers::IsPlaying(animation.body));
	ASSERT_FALSE(mind.idle.agenda.empty());
	EXPECT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::Move);
	EXPECT_TRUE(mind.idle.agenda.front().startsWhileWalking);

	reflection.endsWhatItDoes = true;
	ASSERT_TRUE(_system.ForceActivity(creature, reflection));
	EXPECT_FALSE(creature_layers::IsPlaying(animation.body));
	EXPECT_FALSE(mind.idle.agenda.front().startsWhileWalking);
}

TEST_F(CreatureMindSystemTest, ACreatureToldToActInTheTurnItWasMadeHasItsMindSetUpFirst)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// made, it has not thought yet: what it has learnt is set up on its first turn
	ASSERT_FALSE(mind.learnt.has_value());
	EXPECT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer, .action = "SitDown"}));
	EXPECT_TRUE(mind.learnt.has_value());
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_SitDownRow);
	EXPECT_EQ(mind.idle.activity, creature_mind::Activity::Sit);
}

// Sent by its player with the leash to act on a thing its desires find nothing to do with, it is made curious about the
// thing: it looks it over, or holds it up when it can pick it up. A thing it was given to act on otherwise is left to
// its desires alone
TEST_F(CreatureMindSystemTest, SentByTheLeashToAThingItsDesiresFindNothingForItLooksItOverOrHoldsItUp)
{
	// the game's rows of the two actions
	constexpr size_t k_LookRow = 20;
	constexpr size_t k_HoldRow = 64;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("ExamineByLooking"), info.creatureAction.at(k_LookRow).name.begin());
	std::ranges::copy(std::string_view("HoldObject"), info.creatureAction.at(k_HoldRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto number = entt::to_integral(thing);
	auto& mind = registry.Get<CreatureMindState>(creature);
	// its first turn sets its mind up
	_system.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());
	// pulled away from what it did and told of the thing, by the leash or not, before its next plan
	const auto toldOf = [&](bool byLeash) {
		mind.planActive = false;
		mind.planner.current.reset();
		mind.idle.agenda.clear();
		mind.idle.step = 0;
		mind.plannedTurn = mind.turn + 1;
		mind.leash.actOn = {number};
		mind.leash.ordered = byLeash ? std::optional(number) : std::nullopt;
	};

	// none of its desires has anything to do: it looks the thing over, curious about it
	toldOf(true);
	_system.PlanTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(mind.planner.current->action, k_LookRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(number));
	EXPECT_TRUE(mind.planActive);
	EXPECT_TRUE(mind.leash.actOn.empty());
	EXPECT_FALSE(mind.leash.ordered.has_value());

	// a thing it can pick up, it holds up
	hands.canPickUp = true;
	toldOf(true);
	_system.PlanTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(mind.planner.current->action, k_HoldRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(number));

	// a thing it was not sent to by the leash is left to its desires, which have nothing to do with it
	toldOf(false);
	_system.PlanTurn();
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.leash.actOn.empty());
}

// For a thing it is sent to act on, it chooses only from the plans found for the thing: a plan another desire kept from
// an earlier pass is neither chosen nor changed
TEST_F(CreatureMindSystemTest, ForAThingItIsSentToOnlyThePlansFoundForTheThingAreChosenFrom)
{
	constexpr size_t k_LookRow = 20;
	std::ranges::copy(std::string_view("ExamineByLooking"), _world.Info().creatureAction.at(k_LookRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto food = registry.Create();
	registry.Assign<Transform>(food, glm::vec3(80.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto number = entt::to_integral(thing);
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());
	// a plan its hunger kept from an earlier pass, more pressing than anything
	const auto hunger = static_cast<size_t>(creature_desires::Desire::Hunger);
	const creature_planner::Plan kept {
	    .desire = creature_desires::Desire::Hunger, .action = 5, .object = entt::to_integral(food), .priority = 10.0f};
	mind.planner.best.at(hunger) = kept;

	mind.planActive = false;
	mind.planner.current.reset();
	mind.idle.agenda.clear();
	mind.idle.step = 0;
	mind.plannedTurn = mind.turn + 1;
	mind.leash.actOn = {number};
	mind.leash.ordered = number;
	_system.PlanTurn();
	// nothing found for the thing: it looks it over, the kept plan neither taken nor changed
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_LookRow);
	ASSERT_TRUE(mind.planner.best.at(hunger).has_value());
	EXPECT_EQ(mind.planner.best.at(hunger)->object, std::optional(entt::to_integral(food)));
	EXPECT_EQ(mind.planner.best.at(hunger)->priority, 10.0f);
}

// Under a script's control it plans nothing of its own accord, but a thing its player sent it to act on with the leash
// is planned all the same, as the game's order on a thing does not ask whether a script holds the creature
TEST_F(CreatureMindSystemTest, UnderAScriptsControlAThingTheLeashSentItToIsPlannedAllTheSame)
{
	constexpr size_t k_LookRow = 20;
	std::ranges::copy(std::string_view("ExamineByLooking"), _world.Info().creatureAction.at(k_LookRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto number = entt::to_integral(thing);
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());
	ecs::script_held::SetControlledByScript(creature, true);
	ASSERT_TRUE(ecs::script_held::IsControlledByScript(creature));
	const auto toldOf = [&](bool byLeash) {
		mind.planActive = false;
		mind.planner.current.reset();
		mind.idle.agenda.clear();
		mind.idle.step = 0;
		mind.plannedTurn = mind.turn + 1;
		mind.leash.actOn = {number};
		mind.leash.ordered = byLeash ? std::optional(number) : std::nullopt;
	};

	// told of the thing other than by the leash, it plans nothing, and the thing waits for its mind
	toldOf(false);
	_system.PlanTurn();
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_EQ(mind.leash.actOn, (std::vector<uint32_t> {number}));

	// sent to it by the leash, it plans for the thing, and with nothing to do with it looks it over
	toldOf(true);
	_system.PlanTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_LookRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(number));
	EXPECT_TRUE(mind.leash.actOn.empty());
	ecs::script_held::SetControlledByScript(creature, false);
}

// Carrying out what its player sent it to do with the leash, it chooses nothing for itself while the plan has steps
// left; once none is left, or what it does stops as a failure, the order is over and it plans again
TEST_F(CreatureMindSystemTest, CarryingOutItsPlayersOrderItChoosesNothingForItselfUntilThePlanIsOver)
{
	constexpr size_t k_LookRow = 20;
	std::ranges::copy(std::string_view("ExamineByLooking"), _world.Info().creatureAction.at(k_LookRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(120.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto number = entt::to_integral(thing);
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());
	EXPECT_FALSE(mind.commanded);

	// sent by the leash to the thing, it looks it over as its player's order
	mind.planActive = false;
	mind.planner.current.reset();
	mind.idle.agenda.clear();
	mind.idle.step = 0;
	mind.plannedTurn = mind.turn + 1;
	mind.leash.actOn = {number};
	mind.leash.ordered = number;
	_system.PlanTurn();
	ASSERT_TRUE(mind.planActive);
	ASSERT_LT(mind.idle.step, mind.idle.agenda.size());
	EXPECT_TRUE(mind.commanded);

	// while the plan has steps left it plans nothing
	mind.plannedTurn = mind.turn + 1;
	_system.PlanTurn();
	EXPECT_TRUE(mind.commanded);
	EXPECT_EQ(mind.plannedTurn, mind.turn + 1);

	// with no step left, the order is over and it plans
	mind.idle.step = mind.idle.agenda.size();
	_system.PlanTurn();
	EXPECT_FALSE(mind.commanded);
	EXPECT_EQ(mind.plannedTurn, mind.turn);

	// what it does stopping as a failure ends an order too
	mind.commanded = true;
	_system.AbandonAction(creature);
	EXPECT_FALSE(mind.commanded);
}

// Sick, fainted or doing what a stroke left it to do, it chooses nothing for itself until its agenda is done, as while
// carrying out its player's order; stopped as a failure, that is over
TEST_F(CreatureMindSystemTest, SickFaintedOrAfterAStrokeItChoosesNothingForItselfUntilItIsDone)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.learnt.has_value());

	ASSERT_TRUE(_system.Puke(creature));
	EXPECT_TRUE(mind.commanded);
	ASSERT_LT(mind.idle.step, mind.idle.agenda.size());
	mind.plannedTurn = mind.turn + 1;
	_system.PlanTurn();
	EXPECT_EQ(mind.plannedTurn, mind.turn + 1);
	mind.idle.step = mind.idle.agenda.size();
	_system.PlanTurn();
	EXPECT_FALSE(mind.commanded);

	ASSERT_TRUE(_system.Faint(creature));
	EXPECT_TRUE(mind.commanded);
	_system.AbandonAction(creature);
	EXPECT_FALSE(mind.commanded);

	// free to choose, what a stroke left it to do comes first, as an order
	mind.planActive = false;
	mind.idle.agenda.clear();
	mind.idle.step = 0;
	mind.afterFeedback = CreatureMindState::AfterFeedback::Happy;
	_system.ProcessTurn();
	EXPECT_FALSE(mind.afterFeedback.has_value());
	EXPECT_TRUE(mind.commanded);
}

// A new plan cuts short what the body plays, but not the end of an action the leash pulled it away from
TEST_F(CreatureMindSystemTest, ANewPlanLetsTheEndOfAnActionTheLeashPulledItAwayFromPlayOut)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto ending = [] {
		auto body = *creature_layers::PlaySequence({}, 10, 11, 12);
		body.phase = creature_layers::BodyAction::Phase::Loop;
		return creature_layers::EndLoop(body);
	};
	animation.body = ending();
	mind.leash.bodyEndPlaysOut = true;
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer, .action = "SitDown"}));
	EXPECT_TRUE(creature_layers::IsPlaying(animation.body));
	EXPECT_TRUE(animation.body.endWanted);
	EXPECT_FALSE(mind.leash.bodyEndPlaysOut);
	// without the pull-away it is cut short
	animation.body = ending();
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer, .action = "SitDown"}));
	EXPECT_FALSE(creature_layers::IsPlaying(animation.body));
}

// A plan forced on it, and a thing the hand gives it, first stop the reaction it follows, the reaction's record refreshed
// to the turn (docs/wiki/creature/mimicry.md#the-miracle-reactions-in-openblack)
TEST_F(CreatureMindSystemTest, AForcedPlanOrAGiftStopsTheReactionItFollowsAndRefreshesItsRecord)
{
	constexpr size_t k_SitDownRow = 81;
	constexpr size_t k_TakeRow = 142;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("SitDown"), info.creatureAction.at(k_SitDownRow).name.begin());
	std::ranges::copy(std::string_view("TakeObjectFromHand"), info.creatureAction.at(k_TakeRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	constexpr uint8_t k_Type = 3;
	const uint32_t before = game_clock::Turn() + 500;
	const auto follow = [&registry, creature, before]() {
		auto& state = registry.AssignOrReplace<CreatureReaction>(creature);
		state.reaction = 7;
		state.type = k_Type;
		state.object = creature;
		state.follower = true;
		auto& memory = registry.AssignOrReplace<ReactionRecords>(creature);
		memory.records[0] = {.type = k_Type, .turn = before};
		memory.count = 1;
	};
	const auto stopped = [&registry, creature]() {
		const auto& state = std::as_const(registry).Get<const CreatureReaction>(creature);
		const auto& memory = std::as_const(registry).Get<const ReactionRecords>(creature);
		return state.reaction == 0 && !state.follower && state.object == entt::null &&
		       memory.records[0].turn == game_clock::Turn();
	};
	follow();
	EXPECT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::ObeyPlayer, .action = "SitDown"}));
	EXPECT_TRUE(stopped());
	// the hand's giving
	follow();
	const auto gift = registry.Create();
	registry.Assign<Transform>(gift, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_TRUE(_system.TakeFromHand(creature, gift, false, glm::vec2(0.0f)));
	EXPECT_TRUE(stopped());
}

// A plan forced on it is remembered for the player's feedback as its player's order, but a reaction's plan is not
// (docs/wiki/creature/mind.md#the-players-feedback)
TEST_F(CreatureMindSystemTest, AReactionsPlanIsNotRememberedAsItsPlayersOrder)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value() && mind.learnt.has_value());
	// something says why it is curious, so what it does for curiosity is remembered
	(*mind.desires)[creature_desires::Desire::Curiosity].sources = {{.type = 1, .drive = 1.0f}};
	const auto lastOrdered = [&mind]() {
		return !mind.learnt->contexts.empty() && mind.learnt->contexts.back().action == k_SitDownRow
		           ? std::optional(mind.learnt->contexts.back().ordered)
		           : std::nullopt;
	};
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
	EXPECT_EQ(lastOrdered(), std::optional(true));
	mind.learnt->contexts.clear();
	_system.TakeUpReaction(creature, entt::null,
	                       creature_reaction_rules::Setup {.plan = creature_reaction_rules::Plan {
	                                                           .desire = creature_desires::Desire::Curiosity,
	                                                           .action = "SitDown",
	                                                           .onSelf = true,
	                                                       }});
	EXPECT_EQ(lastOrdered(), std::optional(false));
}

TEST_F(CreatureMindSystemTest, StrokedWhileHoldingItEatsWhatItHoldsAsAPlanNoReactionsPlayerForced)
{
	constexpr size_t k_SitDownRow = 81;
	constexpr size_t k_EatAliveRow = 11;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("SitDown"), info.creatureAction.at(k_SitDownRow).name.begin());
	std::ranges::copy(std::string_view("EatAlive"), info.creatureAction.at(k_EatAliveRow).name.begin());
	// sitting down lets it eat what it holds when stroked for it
	info.creatureAction.at(k_SitDownRow).field0xe0 = 1;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto food = registry.Create();
	registry.Assign<Transform>(food, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	hands.held = food;
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value() && mind.learnt.has_value());
	(*mind.desires)[creature_desires::Desire::Curiosity].sources = {{.type = 1, .drive = 1.0f}};
	(*mind.desires)[creature_desires::Desire::Hunger].sources = {{.type = 1, .drive = 1.0f}};
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));

	_system.ReceiveFeedback(creature, 0.5f);

	// it now eats it alive, and that plan is not remembered as one its player forced on it
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_EatAliveRow);
	ASSERT_FALSE(mind.learnt->contexts.empty());
	EXPECT_EQ(mind.learnt->contexts.back().action, k_EatAliveRow);
	EXPECT_FALSE(mind.learnt->contexts.back().ordered);
	// and once free to choose again, it is to do again what it learns most from then
	ASSERT_TRUE(mind.afterFeedback.has_value());
	EXPECT_EQ(*mind.afterFeedback, CreatureMindState::AfterFeedback::RepeatLastAction);
	// The food eaten, the meal it learns most from acted on a thing no longer there: it does nothing again
	registry.Destroy(food);
	hands.held.reset();
	_system.AbandonAction(creature);
	_system.ProcessTurn();
	EXPECT_FALSE(mind.afterFeedback.has_value());
	EXPECT_FALSE(mind.commanded);
}

TEST_F(CreatureMindSystemTest, AStrokeHoldsBackItsSadnessForAMinuteAndAHalfAndASlapDoesNot)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	for (const float feedback : {0.5f, -0.5f})
	{
		const auto creature = test::creature_world::World::MakeCreature();
		_system.ProcessTurn();
		auto& mind = registry.Get<CreatureMindState>(creature);
		ASSERT_TRUE(mind.desires.has_value() && mind.learnt.has_value());
		(*mind.desires)[creature_desires::Desire::Curiosity].sources = {{.type = 1, .drive = 1.0f}};
		// something it did, for the feedback to be about
		ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
		ASSERT_FALSE(mind.learnt->contexts.empty());
		// as if it had chosen it itself, so that it is worth crediting
		mind.learnt->contexts.back().learnable = true;
		_system.ReceiveFeedback(creature, feedback);
		// 90 seconds of turns for a stroke, none for a slap
		EXPECT_EQ((*mind.desires)[creature_desires::Desire::Sadness].suppressedTurns, feedback > 0.0f ? 900u : 0u);
	}
}

namespace
{
/// A creature stroked for sitting down, done for curiosity as if chosen itself: how fast its curiosity grows after
float CuriosityGrowthAfterAStroke(CreatureMindSystem& system, test::creature_world::World& world, bool learnable)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), world.Info().creatureAction.at(k_SitDownRow).name.begin());
	auto& curiosity = world.Info().creatureInitialDesire.at(static_cast<size_t>(creature_desires::Desire::Curiosity));
	curiosity.field0x28 = learnable ? 1u : 0u;
	curiosity.field0x54 = 2.0f;
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& desire = (*mind.desires)[creature_desires::Desire::Curiosity];
	desire.sources = {{.type = 1, .drive = 1.0f}};
	desire.increaseSeconds = 100.0f;
	EXPECT_TRUE(system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
	EXPECT_FALSE(mind.learnt->contexts.empty());
	mind.learnt->contexts.back().learnable = true;
	system.ReceiveFeedback(creature, 0.5f);
	return desire.increaseSeconds;
}
} // namespace

// Feedback changes a desire only when the game's tables mark it learnable: a stroke makes a learnable curiosity grow
// faster, and leaves one that is not as it was (docs/wiki/creature/mind.md#the-players-feedback)
TEST_F(CreatureMindSystemTest, FeedbackLeavesADesireTheTablesMarkNotLearnableAsItWas)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	EXPECT_FLOAT_EQ(CuriosityGrowthAfterAStroke(_system, _world, false), 100.0f);
}

TEST_F(CreatureMindSystemTest, FeedbackMakesADesireTheTablesMarkLearnableGrowFaster)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	EXPECT_LT(CuriosityGrowthAfterAStroke(_system, _world, true), 100.0f);
}

// A lesson from feedback is something to do with the player: the turns since it last was are counted from none again,
// whether or not the desire can be taught; how long the player has not looked at it is left as it was
// (docs/wiki/creature/mind.md#the-players-feedback)
TEST_F(CreatureMindSystemTest, FeedbackCountsAsSomethingToDoWithThePlayerAndLeavesItsUnseenTurns)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value() && mind.learnt.has_value());
	(*mind.desires)[creature_desires::Desire::Curiosity].sources = {{.type = 1, .drive = 1.0f}};
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
	ASSERT_FALSE(mind.learnt->contexts.empty());
	mind.learnt->contexts.back().learnable = true;
	mind.turnsSinceMadeToAct = 50;
	mind.turnsNotLookedAt = 40;
	_system.ReceiveFeedback(creature, 0.5f);
	EXPECT_EQ(mind.turnsSinceMadeToAct, 0u);
	EXPECT_EQ(mind.turnsNotLookedAt, 40u);
}

// An agenda with a fight in it marks the creature as set out to fight, until the action it does ends
TEST_F(CreatureMindSystemTest, SettingOutToFightIsMarkedUntilTheActionEnds)
{
	constexpr size_t k_FightRow = 21;
	std::ranges::copy(std::string_view("Fight"), _world.Info().creatureAction.at(k_FightRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto other = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	EXPECT_FALSE(mind.fightStarted);
	ASSERT_TRUE(
	    _system.ForceActivity(creature, {.desire = creature_desires::Desire::Anger, .action = "Fight", .actionObject = other}));
	EXPECT_TRUE(mind.fightStarted);
	_system.ReceiveFeedback(creature, -0.5f);
	EXPECT_FALSE(mind.fightStarted);
}

// A slap first stops whatever it was doing, its agenda and its plan, as the original's response to its player's
// feedback ends the action unsuccessfully before anything else, even for a creature that learns nothing from it
// (docs/wiki/creature/mind.md#the-players-feedback)
namespace
{
/// A creature too young to learn, stroked or slapped for sitting down with every draw of the game's stream giving `roll`:
/// what the feedback leaves it to do next
std::optional<CreatureMindState::AfterFeedback>
AfterYoungFeedback(CreatureMindSystem& system, test::creature_world::World& world, float feedback, uint32_t roll)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), world.Info().creatureAction.at(k_SitDownRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.developmentPhase = 1;
	std::ignore = system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"});
	if (mind.learnt.has_value() && !mind.learnt->contexts.empty())
	{
		mind.learnt->contexts.back().learnable = true;
	}
	const game_random::testing::ScopedState state;
	game_random::testing::SetGameRand([roll](uint32_t) { return roll; }, [](float) { return 0.0f; });
	system.ReceiveFeedback(creature, feedback);
	return mind.afterFeedback;
}
} // namespace

TEST_F(CreatureMindSystemTest, TooYoungToLearnOneTimeInTwoItIsHappyWhenStrokedAndFrightenedWhenSlapped)
{
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	// the draw of one in two at 0: stroked it shows it is happy, slapped it is frightened in place of the slap's sadness
	EXPECT_EQ(AfterYoungFeedback(_system, _world, 0.5f, 0), std::optional(CreatureMindState::AfterFeedback::Happy));
	EXPECT_EQ(AfterYoungFeedback(_system, _world, -0.5f, 0), std::optional(CreatureMindState::AfterFeedback::Frightened));
	// the other half: nothing after a stroke, the slap's own reaction after a slap
	EXPECT_FALSE(AfterYoungFeedback(_system, _world, 0.5f, 1).has_value());
	EXPECT_EQ(AfterYoungFeedback(_system, _world, -0.5f, 1), std::optional(CreatureMindState::AfterFeedback::ShowSlapFeeling));
}

TEST_F(CreatureMindSystemTest, TheWishToRunFromThePlayerIsTheOppositeOfItsAttitudeToThePlayerFromNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value());
	auto& runAway = (*mind.desires)[creature_desires::Desire::RunAwayFromPlayer];
	std::erase_if(runAway.sources, [](const creature_desires::Source& source) {
		return source.type == creature_desires::sources::k_RunAwayFromPlayer;
	});
	runAway.sources.push_back({.type = creature_desires::sources::k_RunAwayFromPlayer});
	const auto sourceAfterATurnAt = [&](float attitude) {
		mind.attitudeToPlayer = attitude;
		_system.ProcessTurn();
		return creature_desires::SourceValue(runAway, creature_desires::sources::k_RunAwayFromPlayer).value_or(-1.0f);
	};
	// Each turn the source reads how nasty the player has been to it, and nothing for a player who has been kind
	EXPECT_FLOAT_EQ(sourceAfterATurnAt(-0.4f), 0.4f);
	EXPECT_FLOAT_EQ(sourceAfterATurnAt(-1.0f), 1.0f);
	EXPECT_FLOAT_EQ(sourceAfterATurnAt(0.3f), 0.0f);
}

TEST_F(CreatureMindSystemTest, LeftHappyItShowsItsStateWithTheHappyClipAndLeftFrightenedItRunsFromThePlayerOneTimeInThree)
{
	constexpr size_t k_CommunicateStateRow = 23;
	constexpr size_t k_RunAwayFromPlayerRow = 70;
	constexpr size_t k_BeFrightenedOnTheSpotRow = 174;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("CommunicateState"), info.creatureAction.at(k_CommunicateStateRow).name.begin());
	std::ranges::copy(std::string_view("RunAwayFromPlayer"), info.creatureAction.at(k_RunAwayFromPlayerRow).name.begin());
	std::ranges::copy(std::string_view("BeFrightenedOnTheSpot"),
	                  info.creatureAction.at(k_BeFrightenedOnTheSpotRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	// the player's eye, which it runs from
	const test::RestoreService<Locator::camera> restoreCamera;
	Locator::camera::emplace(glm::vec3(150.0f, 20.0f, 100.0f));
	auto& registry = test::creature_world::World::Registry();
	const auto freeWith = [&](CreatureMindState::AfterFeedback after, uint32_t roll) -> CreatureMindState& {
		const auto creature = test::creature_world::World::MakeCreature();
		_system.ProcessTurn();
		auto& mind = registry.Get<CreatureMindState>(creature);
		_system.AbandonAction(creature);
		mind.afterFeedback = after;
		const game_random::testing::ScopedState state;
		game_random::testing::SetGameRand([roll](uint32_t) { return roll; }, [](float) { return 0.0f; });
		_system.ProcessTurn();
		return mind;
	};

	// happy: it shows its state, under the desire to, with the happy clip alone, as an order is carried out
	auto& happy = freeWith(CreatureMindState::AfterFeedback::Happy, 0);
	EXPECT_FALSE(happy.afterFeedback.has_value());
	ASSERT_TRUE(happy.planner.current.has_value());
	EXPECT_EQ(happy.planner.current->desire, creature_desires::Desire::ManifestState);
	EXPECT_EQ(happy.planner.current->action, k_CommunicateStateRow);
	ASSERT_EQ(happy.idle.agenda.size(), 1u);
	EXPECT_EQ(happy.idle.agenda[0].animation, creature_layers::animations::k_Happy);
	EXPECT_TRUE(happy.commanded);

	// frightened: the draw of one in three at 0 runs from the player, else it is frightened where it stands
	auto& runs = freeWith(CreatureMindState::AfterFeedback::Frightened, 0);
	ASSERT_TRUE(runs.planner.current.has_value());
	EXPECT_EQ(runs.planner.current->desire, creature_desires::Desire::Fear);
	EXPECT_EQ(runs.planner.current->action, k_RunAwayFromPlayerRow);
	EXPECT_TRUE(runs.commanded);
	auto& stays = freeWith(CreatureMindState::AfterFeedback::Frightened, 1);
	ASSERT_TRUE(stays.planner.current.has_value());
	EXPECT_EQ(stays.planner.current->action, k_BeFrightenedOnTheSpotRow);
}

TEST_F(CreatureMindSystemTest, ASlapLeavesItToShowHowItFeltOnceFreeToChoose)
{
	constexpr size_t k_SitDownRow = 81;
	constexpr size_t k_CommunicateStateRow = 23;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("SitDown"), info.creatureAction.at(k_SitDownRow).name.begin());
	std::ranges::copy(std::string_view("CommunicateState"), info.creatureAction.at(k_CommunicateStateRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.desires.has_value() && mind.learnt.has_value());
	// something it did, for the slap to be about
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
	mind.learnt->contexts.back().learnable = true;
	_system.ReceiveFeedback(creature, -0.5f);
	ASSERT_TRUE(mind.afterFeedback.has_value());
	EXPECT_EQ(*mind.afterFeedback, CreatureMindState::AfterFeedback::ShowSlapFeeling);
	// Free to choose, it shows it under the desire to show its state, as an order is carried out
	_system.ProcessTurn();
	EXPECT_FALSE(mind.afterFeedback.has_value());
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::ManifestState);
	EXPECT_EQ(mind.planner.current->action, k_CommunicateStateRow);
	EXPECT_TRUE(mind.commanded);
	EXPECT_EQ(mind.idle.agenda.size(), 1u);
}

TEST_F(CreatureMindSystemTest, ASitForcedWithNoClearPlaceIsTakenUpAndFailsAtOnce)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	// off the land, where no place is clear to sit in
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(9000.0f, 0.0f, 9000.0f));
	// a body as wide as a creature's, so that the place it looks for is a few cells wide
	auto* body = registry.TryGet<CreatureBodyMetrics>(creature);
	if (body == nullptr)
	{
		body = &registry.Assign<CreatureBodyMetrics>(creature);
	}
	body->restHeight = 10.0f;
	body->reach = 5.0f;
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	EXPECT_FALSE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Tiredness, .action = "SitDown"}));
	// it was the plan, and it stopped as a failure: nothing current, nothing under way, its agenda empty
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.lastAction, std::optional<uint32_t>(k_SitDownRow));
	EXPECT_FALSE(mind.planActive);
	EXPECT_TRUE(mind.idle.agenda.empty());
}

TEST_F(CreatureMindSystemTest, ASlapStopsWhatItIsDoing)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>();
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	registry.Assign<CreatureLearningOff>(creature);
	_system.ProcessTurn();
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Curiosity, .action = "SitDown"}));
	ASSERT_TRUE(mind.planActive);
	ASSERT_TRUE(mind.planner.current.has_value());
	_system.ReceiveFeedback(creature, -0.5f);
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.idle.agenda.empty());
	EXPECT_EQ(mind.idle.step, 0u);
}

TEST_F(CreatureMindSystemTest, APlanForcedByItsPlayersActionIsMarkedAndTheCommandedPlanCanBeAskedAbout)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	EXPECT_FALSE(_system.IsCommanded(creature));
	EXPECT_FALSE(_system.IsForcedByPlayerAction(creature));
	_system.SetForcedByPlayerAction(creature, true);
	EXPECT_TRUE(_system.IsForcedByPlayerAction(creature));
	registry.Get<CreatureMindState>(creature).commanded = true;
	EXPECT_TRUE(_system.IsCommanded(creature));
	_system.SetForcedByPlayerAction(creature, false);
	EXPECT_FALSE(_system.IsForcedByPlayerAction(creature));
	// with no mind, neither
	const auto thing = registry.Create();
	EXPECT_FALSE(_system.IsCommanded(thing));
	_system.SetForcedByPlayerAction(thing, true);
	EXPECT_FALSE(_system.IsForcedByPlayerAction(thing));
}

TEST_F(CreatureMindSystemTest, AGodsHandMakesADesireDominantWithOrWithoutTheNeedsAndEndsIt)
{
	using creature_desires::Desire;
	auto& registry = test::creature_world::World::Registry();
	// a desire the dominance holds down only when the needs are held down too
	std::optional<Desire> need;
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		const auto desire = static_cast<Desire>(d);
		if (desire != Desire::ObeyPlayer && creature_spell_mind::HeldDownByCheat(desire, true) &&
		    !creature_spell_mind::HeldDownByCheat(desire, false))
		{
			need = desire;
			break;
		}
	}
	ASSERT_TRUE(need.has_value());
	for (const bool needsToo : {false, true})
	{
		const auto creature = test::creature_world::World::MakeCreature();
		_system.ProcessTurn();
		_system.MakeDesireDominant(creature, Desire::ObeyPlayer, 120.0f, needsToo);
		auto& desires = *registry.Get<CreatureMindState>(creature).desires;
		EXPECT_GT(desires[Desire::ObeyPlayer].value, 0.0f);
		EXPECT_EQ(desires[*need].suppressedTurns > 0, needsToo);
		_system.EndDominantDesire(creature);
		EXPECT_EQ(desires[*need].suppressedTurns, 0u);
	}
}

TEST_F(CreatureMindSystemTest, AGodsOrderIsForcedAsObeyingAndWhatItTeachesIsKnownAtOnce)
{
	constexpr size_t k_SitDownRow = 81;
	std::ranges::copy(std::string_view("SitDown"), _world.Info().creatureAction.at(k_SitDownRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	const auto villager = MakeVillager({130.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	ASSERT_TRUE(_system.ObeyGodOrder(creature, k_SitDownRow, villager));
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, k_SitDownRow);
	EXPECT_TRUE(mind.planner.current->forced);
	EXPECT_TRUE(_system.IsCommanded(creature));
	EXPECT_TRUE(_system.IsForcedByPlayerAction(creature));
	// a row past the table's end is no order
	EXPECT_FALSE(_system.ObeyGodOrder(creature, 100000, villager));
	// taught an ordinary skill, it knows it at once
	constexpr auto k_Normal = static_cast<uint32_t>(CreatureActionLearningType::Normal);
	EXPECT_FALSE(_system.KnowsAction(creature, k_Normal, 1));
	_system.LearnAction(creature, k_Normal, 1);
	EXPECT_TRUE(_system.KnowsAction(creature, k_Normal, 1));
	// with no mind, nothing
	const auto thing = registry.Create();
	_system.LearnAction(thing, k_Normal, 1);
	EXPECT_FALSE(_system.KnowsAction(thing, k_Normal, 1));
}

// Setting off into its player's temple, it comes to believe in the temple's heart as the agenda is made
// (docs/wiki/project/progress/creature/object_actions.md, "Enter the citadel")
TEST_F(CreatureMindSystemTest, SettingOffIntoItsTempleItComesToBelieveInTheHeart)
{
	constexpr size_t k_EnterRow = 120;
	std::ranges::copy(std::string_view("EnterCitadel"), _world.Info().creatureAction.at(k_EnterRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto temple = registry.Create();
	registry.Assign<Transform>(temple, glm::vec3(200.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_EQ(mind.beliefs.Find(temple), nullptr);
	ASSERT_TRUE(_system.ForceActivity(creature, {.desire = creature_desires::Desire::Rest, .action = "EnterCitadel"}));
	EXPECT_NE(mind.beliefs.Find(temple), nullptr);
}

TEST_F(CreatureMindSystemTest, GivenSomethingItDecidesAboutWhatItHoldsOnceFreeToChooseTakenOrNot)
{
	// the game's rows of the take and of what it may do with a gift
	constexpr size_t k_TakeRow = 142;
	constexpr std::array<size_t, 3> k_UseRows {275, 276, 278};
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("TakeObjectFromHand"), info.creatureAction.at(k_TakeRow).name.begin());
	std::ranges::copy(std::string_view("ExamineObjectInHand"), info.creatureAction.at(k_UseRows[0]).name.begin());
	std::ranges::copy(std::string_view("EatObjectInHand"), info.creatureAction.at(k_UseRows[1]).name.begin());
	std::ranges::copy(std::string_view("ThrowObjectInHand"), info.creatureAction.at(k_UseRows[2]).name.begin());
	// and a plan done to a thing that a gift may carry on
	constexpr size_t k_CarriedRow = 300;
	std::ranges::copy(std::string_view("BeSad"), info.creatureAction.at(k_CarriedRow).name.begin());
	info.creatureAction.at(k_CarriedRow).field0x94 = 1;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto thing = [&registry](glm::vec3 position) {
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	};
	const auto gift = thing(glm::vec3(110.0f, 0.0f, 100.0f));
	auto& mind = registry.Get<CreatureMindState>(creature);

	// Given something, that is what it has to do next, in place of what a slap left it to do, and a magic mushroom
	// eaten meanwhile does not make it high
	mind.afterFeedback = CreatureMindState::AfterFeedback::Frightened;
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false, glm::vec2(0.0f)));
	EXPECT_TRUE(mind.useWhatItHolds);
	EXPECT_FALSE(mind.afterFeedback.has_value());
	_system.AteMagicMushroom(creature);
	EXPECT_FALSE(mind.highPending);

	// The take given up with nothing taken: free to choose, it decides about what it holds, here nothing, and plans
	// nothing else that turn
	_system.AbandonAction(creature);
	_system.ProcessTurn();
	EXPECT_FALSE(mind.useWhatItHolds);
	EXPECT_NE(mind.plannedTurn, mind.turn);
	EXPECT_FALSE(mind.planActive);

	// Given again, and holding another thing once free to choose (it took nothing): it decides about that one
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false, glm::vec2(0.0f)));
	_system.AbandonAction(creature);
	const auto other = thing(glm::vec3(90.0f, 0.0f, 100.0f));
	hands.held = other;
	_system.ProcessTurn();
	EXPECT_FALSE(mind.useWhatItHolds);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->object, std::optional(entt::to_integral(other)));
	EXPECT_NE(std::ranges::find(k_UseRows, mind.planner.current->action), k_UseRows.end());

	// Given something while carrying out a plan whose action is done to a thing, it carries that plan on with the gift
	// as what it is done to, once free to choose
	hands.held = gift;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = static_cast<uint32_t>(k_CarriedRow)};
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false, glm::vec2(0.0f)));
	_system.AbandonAction(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_CarriedRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(entt::to_integral(gift)));
}

// Given something, the take is a plan forced on it, of curiosity and the take's row, as the original's give handler
// forces it; so the gift is no lesson by itself (docs/wiki/hand/grab-hold-and-throw.md#giving-a-held-object)
TEST_F(CreatureMindSystemTest, GivenSomethingTheTakeIsAForcedPlanOfCuriosity)
{
	constexpr size_t k_TakeRow = 142;
	std::ranges::copy(std::string_view("TakeObjectFromHand"), _world.Info().creatureAction.at(k_TakeRow).name.begin());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	const auto gift = registry.Create();
	registry.Assign<Transform>(gift, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(_system.TakeFromHand(creature, gift, false, glm::vec2(0.0f)));
	const auto& mind = registry.Get<CreatureMindState>(creature);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.planner.current->forced);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Curiosity);
	EXPECT_EQ(mind.planner.current->action, k_TakeRow);
	EXPECT_TRUE(mind.planActive);
}

TEST_F(CreatureMindSystemTest, AScriptsPlayControlsItAndPlaysWhatItGaveAsThePlanOfObeyingItsPlayer)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// nothing given yet, the static action 0 no times: it only waits for its body to be free
	ASSERT_TRUE(_system.PlayForScript(creature));
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	ASSERT_EQ(mind.idle.agenda.size(), 1u);
	EXPECT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::WaitUntilFree);
	// "play 0 loop 1": the static action 0 has no start, so it never gets past its first play
	_system.SetScriptPlay(creature, 0, 1);
	ASSERT_TRUE(_system.PlayForScript(creature));
	ASSERT_EQ(mind.idle.agenda.size(), 2u);
	EXPECT_TRUE(mind.idle.agenda.front().neverStarts);
	EXPECT_FALSE(_system.HasPlayed(creature));
	// "play 63 loop 2"
	_system.SetScriptPlay(creature, 63, 2);
	ASSERT_TRUE(_system.PlayForScript(creature));
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, creature_planner::k_FollowPlayerAction);
	EXPECT_TRUE(mind.planActive);
	EXPECT_EQ(mind.idle.activity, creature_mind::Activity::Told);
	ASSERT_EQ(mind.idle.agenda.size(), 3u);
	EXPECT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::Action);
	EXPECT_EQ(mind.idle.agenda.front().animation, 63u);
	EXPECT_EQ(mind.idle.agenda.back().kind, creature_mind::Step::Kind::WaitUntilFree);
	// while it plays it has not played; once its agenda is over it has
	EXPECT_FALSE(_system.HasPlayed(creature));
	mind.idle.step = mind.idle.agenda.size();
	EXPECT_TRUE(_system.HasPlayed(creature));
	// a static action is its start, its loop for ten seconds and its end
	_system.SetScriptPlay(creature, 38, 1);
	ASSERT_TRUE(_system.PlayForScript(creature));
	ASSERT_EQ(mind.idle.agenda.size(), 2u);
	EXPECT_EQ(mind.idle.agenda.front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(mind.idle.agenda.front().sequence, (std::array<size_t, 3> {37, 38, 39}));
}

namespace
{
/// Counts the miracles let go of
class ReleasesCounted final: public test::InertMagicSystem
{
public:
	void ReleaseCreatureCast(entt::entity /*creature*/) override { ++releases; }
	uint32_t releases {0};
};
} // namespace

TEST_F(CreatureMindSystemTest, AScriptsStartLetsGoOfAMiracleItHoldsAndEndsASitThereAndThen)
{
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	auto& magic = static_cast<ReleasesCounted&>(Locator::magicSystem::emplace<ReleasesCounted>());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	animation.body = {};
	animation.body = *creature_layers::PlaySequence(animation.body, 37, 38, 39);
	ASSERT_FALSE(animation.body.endWanted);
	_system.SetScriptPlay(creature, 63, 1);
	ASSERT_TRUE(_system.PlayForScript(creature));
	// at once, not on its next turn: the miracle let go of, and the sit's end asked for
	EXPECT_EQ(magic.releases, 1u);
	EXPECT_TRUE(animation.body.endWanted);
}

TEST_F(CreatureMindSystemTest, AScriptsReleaseLetsGoOfAMiracleItHoldsAndEndsASitThereAndThen)
{
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	auto& magic = static_cast<ReleasesCounted&>(Locator::magicSystem::emplace<ReleasesCounted>());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.FocusByScript(creature, {30.0f, 0.0f, 40.0f});
	const auto releases = magic.releases;
	// sitting when the script lets it go
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = {};
	animation.body = *creature_layers::PlaySequence(animation.body, 37, 38, 39);
	ASSERT_FALSE(animation.body.endWanted);
	ecs::script_held::SetControlledByScript(creature, false);
	_system.ReleasedByScript(creature);
	// at once, not on its next turn: its focus off, the miracle let go of, and the sit's end asked for
	EXPECT_FALSE(mind.scriptFocus.has_value());
	EXPECT_EQ(magic.releases, releases + 1);
	EXPECT_TRUE(animation.body.endWanted);
}

TEST_F(CreatureMindSystemTest, AForcedFinishHurriesTheBodyAndGivesUpWhatItDidAsAFailure)
{
	const test::RestoreService<Locator::magicSystem> restoreMagic;
	auto& magic = static_cast<ReleasesCounted&>(Locator::magicSystem::emplace<ReleasesCounted>());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.body = *creature_layers::PlayOnce({}, 63, false);
	mind.planActive = true;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 12, .priority = 5.0f};
	const auto releases = magic.releases;
	_system.ForceFinish(creature);
	// the action its body plays ends at twice the speed, and what it was doing is given up, its cast let go of
	EXPECT_TRUE(animation.body.hurried);
	EXPECT_FALSE(mind.planActive);
	EXPECT_FALSE(mind.planner.current.has_value());
	EXPECT_TRUE(mind.idle.agenda.empty());
	EXPECT_EQ(magic.releases, releases + 1);
}

TEST_F(CreatureMindSystemTest, AScriptChangesTheDesiresOfAMindItSetsUpFirst)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// made, it has not thought yet: its mind is set up, with its stage's desires, before the change
	bool changed = false;
	_system.ChangeDesires(creature, [&changed](creature_desires::Desires& desires) {
		for (auto& state : desires.desires)
		{
			state.activated = false;
		}
		changed = true;
	});
	EXPECT_TRUE(changed);
	ASSERT_TRUE(mind.desires.has_value());
	EXPECT_EQ(mind.desiresPhase, mind.developmentPhase);
	for (const auto& state : mind.desires->desires)
	{
		EXPECT_FALSE(state.activated);
	}
	// a thing without a mind is not changed
	bool touched = false;
	_system.ChangeDesires(MakeVillager(glm::vec3(0.0f)),
	                      [&touched](creature_desires::Desires& /*desires*/) { touched = true; });
	EXPECT_FALSE(touched);
}

TEST_F(CreatureMindSystemTest, PlayedOfACreatureOnlyIdlingGivesUpWhatIsLeftOfItsAgenda)
{
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	creature_mind::Plan(mind.idle, creature_mind::Activity::BeIdle,
	                    {creature_mind::Step {.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f},
	                     creature_mind::Step {.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f}});
	mind.idle.stepStarted = true;
	EXPECT_TRUE(_system.HasPlayed(creature));
	// the steps it had not done are gone, and the agenda counts as given up, not carried out
	EXPECT_EQ(mind.idle.agenda.size(), 0u);
	EXPECT_EQ(mind.idle.step, 0u);
	EXPECT_FALSE(mind.idle.stepStarted);
	EXPECT_TRUE(mind.idle.gaveUp);
	// busy with something else, nothing is given up
	creature_mind::Plan(mind.idle, creature_mind::Activity::HangAround, {creature_mind::Step {}});
	EXPECT_FALSE(_system.HasPlayed(creature));
	EXPECT_EQ(mind.idle.agenda.size(), 1u);
}

TEST_F(CreatureMindSystemTest, AScriptSendsItWalkingInTwoLegsByItsHeightAsThePlanOfObeyingItsPlayer)
{
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	const auto height = ecs::object::GetHeight(creature);
	ASSERT_GT(height, 0.0f);
	// 60 away with no distance given: a sixth of the way, or its height if more, then its height
	const glm::vec3 point {160.0f, 3.0f, 100.0f};
	ASSERT_TRUE(_system.MoveForScript(creature, point, 0.0f));
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, creature_planner::k_FollowPlayerAction);
	EXPECT_TRUE(mind.planActive);
	ASSERT_EQ(mind.idle.agenda.size(), 2u);
	const auto first = height > 10.0f ? height : std::min(10.0f, (0.7f * height) + 22.0f);
	EXPECT_FLOAT_EQ(mind.idle.agenda[0].movement.maxDistance, first);
	EXPECT_FLOAT_EQ(mind.idle.agenda[1].movement.maxDistance, height);
	EXPECT_EQ(mind.idle.agenda[1].movement.point, glm::vec2(160.0f, 100.0f));
	// with a distance, both legs go within it
	ASSERT_TRUE(_system.MoveForScript(creature, point, 5.0f));
	EXPECT_FLOAT_EQ(mind.idle.agenda[0].movement.maxDistance, 5.0f);
	EXPECT_FLOAT_EQ(mind.idle.agenda[1].movement.maxDistance, 5.0f);
}

namespace
{
/// A creature's walking, as far as a script's move needs it: a walk asked for again to the point it is already walking
/// to carries on, one to another point turns the walk there, and the test says when it arrives
class WalkingMoves final: public ecs::systems::CreatureLocomotionSystemInterface
{
public:
	std::optional<glm::vec2> walkingTo;
	/// The walks begun or turned to a new point, and the stops asked for
	int walks {0};
	int stops {0};

	void Arrive() { walkingTo.reset(); }

	void ProcessTurn() override {}
	void Update(float) override {}
	void AnimationAt(entt::entity, float, CreatureAnimationInputs&) const override {}
	MoveResult MoveTo(entt::entity, glm::vec2 point, Pace, float, float) override
	{
		if (walkingTo != point)
		{
			walkingTo = point;
			++walks;
		}
		return MoveResult::Started;
	}
	MoveResult WalkBack(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override { return false; }
	void Stop(entt::entity) override
	{
		++stops;
		walkingTo.reset();
	}
	void Place(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return walkingTo.has_value(); }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return true; }
};
} // namespace

TEST_F(CreatureMindSystemTest, AScriptsMoveSentAgainEachTurnCarriesTheWalkOnAndANewPointTurnsIt)
{
	const test::RestoreService<Locator::creatureLocomotionSystem> restoreMoves;
	auto& moves = static_cast<WalkingMoves&>(Locator::creatureLocomotionSystem::emplace<WalkingMoves>());
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	// whatever its first turn had it do, it stands
	moves.walkingTo.reset();
	moves.walks = 0;
	moves.stops = 0;
	const glm::vec3 point {160.0f, 3.0f, 100.0f};
	ASSERT_TRUE(_system.MoveForScript(creature, point, 0.0f));
	_system.ProcessTurn();
	ASSERT_EQ(moves.walkingTo, glm::vec2(160.0f, 100.0f));
	EXPECT_EQ(moves.walks, 1);
	// sent again on the next turns while it walks: the walk is neither stopped nor begun afresh
	for (int turn = 0; turn < 3; ++turn)
	{
		ASSERT_TRUE(_system.MoveForScript(creature, point, 0.0f));
		_system.ProcessTurn();
	}
	EXPECT_EQ(moves.stops, 0);
	EXPECT_EQ(moves.walks, 1);
	EXPECT_EQ(moves.walkingTo, glm::vec2(160.0f, 100.0f));
	EXPECT_FALSE(_system.HasPlayed(creature));
	// it arrives within the first leg, then walks the second and arrives: the move is played
	moves.Arrive();
	_system.ProcessTurn();
	_system.ProcessTurn();
	EXPECT_EQ(moves.walks, 2);
	moves.Arrive();
	_system.ProcessTurn();
	EXPECT_TRUE(_system.HasPlayed(creature));
	EXPECT_EQ(moves.stops, 0);

	// sent walking again, then to a new point while it walks: the walk turns there without stopping
	ASSERT_TRUE(_system.MoveForScript(creature, point, 0.0f));
	_system.ProcessTurn();
	ASSERT_EQ(moves.walkingTo, glm::vec2(160.0f, 100.0f));
	const auto walks = moves.walks;
	const auto stops = moves.stops;
	const glm::vec3 elsewhere {100.0f, 3.0f, 40.0f};
	ASSERT_TRUE(_system.MoveForScript(creature, elsewhere, 0.0f));
	_system.ProcessTurn();
	EXPECT_EQ(moves.walkingTo, glm::vec2(100.0f, 40.0f));
	EXPECT_EQ(moves.walks, walks + 1);
	EXPECT_EQ(moves.stops, stops);
}

namespace
{
/// A creature's turning on the spot, as far as a watch of the hand needs it: whether it is busy moving, and the turns
/// asked of it
class TurningMoves final: public ecs::systems::CreatureLocomotionSystemInterface
{
public:
	bool busy {false};
	int turns {0};

	void ProcessTurn() override {}
	void Update(float) override {}
	void AnimationAt(entt::entity, float, CreatureAnimationInputs&) const override {}
	MoveResult MoveTo(entt::entity, glm::vec2, Pace, float, float) override { return MoveResult::Busy; }
	MoveResult WalkBack(entt::entity, glm::vec2, float, float) override { return MoveResult::Busy; }
	MoveResult MoveToObject(entt::entity, entt::entity, Pace, float) override { return MoveResult::Busy; }
	MoveResult Follow(entt::entity, entt::entity, float, Pace) override { return MoveResult::Busy; }
	MoveResult FleeFrom(entt::entity, glm::vec2) override { return MoveResult::Busy; }
	bool TurnToFace(entt::entity, glm::vec2) override
	{
		++turns;
		return true;
	}
	void Stop(entt::entity) override {}
	void Place(entt::entity, glm::vec3) override {}
	[[nodiscard]] bool IsMoving(entt::entity) const override { return busy; }
	[[nodiscard]] bool IsValidPosition(glm::vec2, float) const override { return true; }
};
} // namespace

// Watching its player's hand behind it, it asks every turn to turn to the hand, but a turn is started only while it is
// not already moving or turning, so the turn under way is not begun afresh each turn
TEST_F(CreatureMindSystemTest, WatchingTheHandBehindItATurnUnderWayIsNotBegunAfresh)
{
	const test::RestoreService<Locator::creatureLocomotionSystem> restoreMoves;
	auto& moves = static_cast<TurningMoves&>(Locator::creatureLocomotionSystem::emplace<TurningMoves>());
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).owner = PlayerNames::PLAYER_ONE;
	auto& mind = registry.Get<CreatureMindState>(creature);
	// the hand half a turn round from where it faces
	const float heading = registry.AllOf<CreatureLocomotion>(creature)
	                          ? std::as_const(registry).Get<const CreatureLocomotion>(creature).heading
	                          : 0.0f;
	const glm::vec2 ahead = creature_locomotion::DirectionOf(heading);
	mind.lastHandPoint = glm::vec3(100.0f - (ahead.x * 60.0f), 0.0f, 100.0f - (ahead.y * 60.0f));
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told, creature_mind::LookAtHand());
	moves.busy = true;
	moves.turns = 0;
	_system.ProcessTurn();
	_system.ProcessTurn();
	EXPECT_EQ(moves.turns, 0);
	moves.busy = false;
	_system.ProcessTurn();
	EXPECT_EQ(moves.turns, 1);
}

TEST_F(CreatureMindSystemTest, AStepWithSomethingToLookAtHasTheHeadOnIt)
{
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	// a point: the ground under it (with no land here, the point as given)
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
	                    {creature_mind::Step {.kind = creature_mind::Step::Kind::Wait,
	                                          .seconds = 5.0f,
	                                          .gaze = creature_mind::Gaze {.point = glm::vec3(130.0f, 7.0f, 140.0f)}}});
	_system.ProcessTurn();
	EXPECT_FALSE(mind.lookingAbout);
	const auto& animation = registry.Get<CreatureAnimation>(creature);
	ASSERT_TRUE(animation.lookAt.has_value());
	EXPECT_EQ(*animation.lookAt, glm::vec3(130.0f, 7.0f, 140.0f));
	// a thing: its top
	const auto villager = MakeVillager({150.0f, 2.0f, 100.0f});
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
	                    {creature_mind::Step {.kind = creature_mind::Step::Kind::Wait,
	                                          .seconds = 5.0f,
	                                          .gaze = creature_mind::Gaze {.object = entt::to_integral(villager)}}});
	_system.ProcessTurn();
	ASSERT_TRUE(animation.lookAt.has_value());
	EXPECT_EQ(*animation.lookAt, glm::vec3(150.0f, 2.0f + ecs::object::GetHeight(villager), 100.0f));
	// a gaze that names nothing, or a thing that is gone: it looks about, as with no look of its own (at what it
	// watches, or ahead)
	for (const auto& gaze : {creature_mind::Gaze {}, creature_mind::Gaze {.object = 99999u}})
	{
		creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
		                    {creature_mind::Step {.kind = creature_mind::Step::Kind::Action, .animation = 63, .gaze = gaze}});
		_system.ProcessTurn();
		EXPECT_TRUE(mind.lookingAbout);
		ASSERT_TRUE(animation.lookAt.has_value());
		if (mind.look.id.has_value())
		{
			EXPECT_EQ(*animation.lookAt, mind.look.point);
		}
	}
	// an action with no gaze at all does not look about
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
	                    {creature_mind::Step {.kind = creature_mind::Step::Kind::Action, .animation = 63}});
	_system.ProcessTurn();
	EXPECT_FALSE(mind.lookingAbout);
}

TEST_F(CreatureMindSystemTest, OnTheWayToAScriptedSitTheHeadTakesTheWalksLookWithTheSpeciesNumbers)
{
	auto& ape = _world.Info().creature.at(0);
	ape.lookSweepAngle = 1.5f;
	ape.lookSweepStep = 0.1f;
	ape.lookSweepFirstDistance = 15.0f;
	ape.lookSweepFarthest = 95.0f;
	ape.lookSweepDistanceStep = 10.0f;
	ape.lookAtPointWithin = 25.0f;
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto& animation = registry.Get<CreatureAnimation>(creature);
	// 60 from the point: its first look is at the point's ground (with no land here, as given), the sweep set up. The
	// place is a thing that stays, or the creature would give the sit up as it lost the thing
	const auto place = entt::to_integral(registry.Create());
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
	                    creature_mind::SitDownAt(place, {160.0f, 100.0f}, 3.0f, 10.0f));
	_system.ProcessTurn();
	ASSERT_TRUE(mind.walkLook.has_value());
	EXPECT_FLOAT_EQ(mind.walkLook->distance, 15.0f);
	EXPECT_FALSE(mind.lookingAbout);
	ASSERT_TRUE(animation.lookAt.has_value());
	EXPECT_EQ(*animation.lookAt, glm::vec3(160.0f, 0.0f, 100.0f));
	// within 8 of the point it has no look of its own, and looks about
	creature_mind::Plan(mind.idle, creature_mind::Activity::Told,
	                    creature_mind::SitDownAt(place, {105.0f, 100.0f}, 3.0f, 10.0f));
	_system.ProcessTurn();
	EXPECT_TRUE(mind.lookingAbout);
	EXPECT_TRUE(animation.lookAt.has_value());
}

TEST_F(CreatureMindSystemTest, AScriptsForcedActionControlsItAndCarriesItOutAsObeyingItsPlayerFromTheTurnItIsMade)
{
	constexpr size_t k_SitDownRow = 81;
	constexpr size_t k_LookButDontApproachRow = 188;
	constexpr size_t k_LookForeverRow = 189;
	auto& actions = _world.Info().creatureAction;
	std::ranges::copy(std::string_view("SitDown"), actions.at(k_SitDownRow).name.begin());
	std::ranges::copy(std::string_view("LookButDontApproach"), actions.at(k_LookButDontApproachRow).name.begin());
	std::ranges::copy(std::string_view("LookForever"), actions.at(k_LookForeverRow).name.begin());
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	const auto villager = MakeVillager({130.0f, 0.0f, 100.0f});
	auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(creature);
	// made this turn, with no mind turn yet
	ASSERT_TRUE(_system.ScriptDoAction(creature, k_LookForeverRow, villager, std::nullopt));
	EXPECT_TRUE(ecs::script_held::IsControlledByScript(creature));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::ObeyPlayer);
	EXPECT_EQ(mind.planner.current->action, k_LookForeverRow);
	EXPECT_TRUE(mind.planner.current->forced);
	ASSERT_TRUE(mind.planner.current->object.has_value());
	EXPECT_EQ(*mind.planner.current->object, entt::to_integral(villager));
	ASSERT_EQ(mind.idle.agenda.size(), 2u);
	EXPECT_FLOAT_EQ(mind.idle.agenda.back().seconds, 86400.0f);
	// a look that does not go up to the thing lasts 1.1 seconds and up to 1.2 more, in whole turns
	ASSERT_TRUE(_system.ScriptDoAction(creature, k_LookButDontApproachRow, villager, std::nullopt));
	ASSERT_EQ(mind.idle.agenda.size(), 2u);
	const auto seconds = mind.idle.agenda.back().seconds;
	EXPECT_GE(seconds, 1.1f - 0.001f);
	EXPECT_LE(seconds, 2.3f + 0.001f);
	const auto turns = seconds / 0.1f;
	EXPECT_NEAR(turns, std::round(turns), 0.001f);
	// under the script's control it sits at the thing: there, facing it, down the slope, ten seconds and up to five more
	ASSERT_TRUE(_system.ScriptDoAction(creature, k_SitDownRow, villager, std::nullopt));
	ASSERT_EQ(mind.idle.agenda.size(), 4u);
	EXPECT_EQ(mind.idle.agenda.front().movement.point, glm::vec2(130.0f, 100.0f));
	EXPECT_FLOAT_EQ(mind.idle.agenda.front().movement.maxDistance, ecs::object::Get2DRadius(creature));
	EXPECT_GE(mind.idle.agenda.back().seconds, 10.0f - 0.001f);
	EXPECT_LE(mind.idle.agenda.back().seconds, 15.0f + 0.001f);
	// an action openblack cannot carry out, or a row past the table's end
	EXPECT_FALSE(_system.ScriptDoAction(creature, 5, villager, std::nullopt));
	EXPECT_FALSE(_system.ScriptDoAction(creature, 100000, villager, std::nullopt));
}

TEST(CreatureMindCurrentAction, ThePlansActionOrTheRowItsIdleActivityCountsAs)
{
	creature_mind_tables::Tables tables;
	tables.actions.resize(320);
	tables.actions.at(313).name = "SleepOnTheSpot";
	tables.actions.at(81).name = "SitDown";
	ecs::components::CreatureMindState mind;
	// doing nothing, nothing counts
	EXPECT_FALSE(mind_detail::CurrentAction(mind, &tables).has_value());
	// asleep on the spot by its own choice: that row
	creature_mind::Plan(mind.idle, creature_mind::Activity::Sleep, {creature_mind::Step {}});
	EXPECT_EQ(mind_detail::CurrentAction(mind, &tables), 313u);
	EXPECT_FALSE(mind_detail::CurrentAction(mind, nullptr).has_value());
	// an activity that counts as no row
	creature_mind::Plan(mind.idle, creature_mind::Activity::Planned, {creature_mind::Step {}});
	EXPECT_FALSE(mind_detail::CurrentAction(mind, &tables).has_value());
	// carrying out a plan: the plan's action
	mind.planner.current = creature_planner::Plan {.action = 17};
	mind.planActive = true;
	EXPECT_EQ(mind_detail::CurrentAction(mind, &tables), 17u);
}

// The plan others see the creature hold: the planner's, else its idle activity as a plan of the row it counts as, for
// that row's desire and done to the step's thing; none once a plan has ended and before the next (the original zeroes
// its plan at every end, and every activity is a plan there)
TEST(CreatureMindCurrentAction, TheHeldPlanIsThePlannersOrTheIdleActivitysAndNoneBetween)
{
	creature_mind_tables::Tables tables;
	tables.actions.resize(320);
	tables.actions.at(313).name = "SleepOnTheSpot";
	tables.actions.at(313).desire = static_cast<uint32_t>(creature_desires::Desire::Tiredness);
	ecs::components::CreatureMindState mind;
	EXPECT_FALSE(mind_detail::HeldPlan(mind, &tables).has_value());
	// asleep on the spot: that row, for its desire, done to itself
	creature_mind::Plan(mind.idle, creature_mind::Activity::Sleep, {creature_mind::Step {}});
	const auto idle = mind_detail::HeldPlan(mind, &tables);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->action, 313u);
	EXPECT_EQ(idle->desire, creature_desires::Desire::Tiredness);
	EXPECT_FALSE(idle->object.has_value());
	// a plan of the planner's
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 15, .object = 9};
	mind.planActive = true;
	const auto planned = mind_detail::HeldPlan(mind, &tables);
	ASSERT_TRUE(planned.has_value());
	EXPECT_EQ(planned->action, 15u);
	EXPECT_EQ(planned->object, std::optional<uint32_t>(9));
	// the plan over, its agenda not yet run out: nothing, whatever the last plan was
	mind.planner.current.reset();
	EXPECT_FALSE(mind_detail::HeldPlan(mind, &tables).has_value());
}

namespace
{
/// Records the fights it is asked to start, and answers each as it is told to
class StartsFights final: public ecs::systems::CreatureFightSystemInterface
{
public:
	void ProcessTurn() override {}
	void Update(float /*turnFraction*/, float /*frameMs*/) override {}
	void AnimationAt(entt::entity /*creature*/, float /*turnFraction*/, CreatureAnimationInputs& /*inputs*/) const override {}
	StartResult StartFight(entt::entity creature, entt::entity opponent) override
	{
		started.emplace_back(creature, opponent);
		return answer;
	}
	void AbortFight(entt::entity /*creature*/) override {}
	void EndOwnFight(entt::entity creature) override { endedOwn.push_back(creature); }
	[[nodiscard]] bool IsFighting(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<entt::entity> OpponentOf(entt::entity /*creature*/) const override { return std::nullopt; }
	[[nodiscard]] std::vector<ArenaView> GetArenas() const override { return {}; }
	[[nodiscard]] std::optional<ArenaView> ArenaOf(entt::entity /*fighter*/) const override { return std::nullopt; }
	bool QueueMove(entt::entity /*creature*/, const creature_fight::Move& /*move*/, bool /*replace*/) override { return false; }
	void ReleaseCharge(entt::entity /*creature*/, float /*heldMs*/) override {}
	void SetAutoFighting(entt::entity /*creature*/, bool /*autoFight*/) override {}
	[[nodiscard]] bool IsAutoFighting(entt::entity /*creature*/) const override { return false; }
	void KnockOut(entt::entity creature) override { knockedOut.push_back(creature); }
	void KillPermanently(entt::entity /*creature*/) override {}
	void Resurrect(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsKnockedOut(entt::entity /*creature*/) const override { return false; }
	[[nodiscard]] std::optional<creature_fight_hud::Values> GetPanel() const override { return std::nullopt; }
	void SetAngerStartsFights(bool /*enabled*/) override {}
	[[nodiscard]] bool GetAngerStartsFights() const override { return false; }
	void SetCameraWatches(bool /*enabled*/) override {}
	[[nodiscard]] bool GetCameraWatches() const override { return false; }
	[[nodiscard]] bool IsCameraOnFight() const override { return false; }

	StartResult answer {StartResult::Started};
	std::vector<std::pair<entt::entity, entt::entity>> started;
	/// The creatures whose own side of a fight was ended
	std::vector<entt::entity> endedOwn;
	/// The creatures knocked out, as a faint knocks them out
	std::vector<entt::entity> knockedOut;
};

/// The row of the game's table of the fight a script forces (CREATURE_FIGHT)
constexpr size_t k_FightRow = 21;

/// Two creatures under a script's control, the first told to fight the second, thinking turn by turn until a fight is
/// asked for, or for 50 turns
struct ForcedFight
{
	entt::entity guide;
	entt::entity other;
	bool told;
};
ForcedFight ForceAFight(CreatureMindSystem& system, test::creature_world::World& world, StartsFights& fights)
{
	std::ranges::copy(std::string_view("Fight"), world.Info().creatureAction.at(k_FightRow).name.begin());
	const auto guide = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_TWO);
	const auto other = test::creature_world::World::MakeCreature({130.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE);
	// a script forcing an action takes control of the creature first, as CREATURE_DO_ACTION does
	ecs::script_held::SetControlledByScript(guide, true);
	ecs::script_held::SetControlledByScript(other, true);
	system.ProcessTurn();
	const bool told = system.ScriptDoAction(guide, k_FightRow, other, std::nullopt);
	for (int turn = 0; turn < 50 && fights.started.empty(); ++turn)
	{
		system.ProcessTurn();
	}
	return {.guide = guide, .other = other, .told = told};
}
} // namespace

// A creature that started a duel a saved game kept takes up its plan of the fight once its mind is set up, with its
// opponent, by the desire the save kept, as the game's load keeps the starter's plan (docs/wiki/engine/saved-games.md)
TEST_F(CreatureMindSystemTest, ALoadedDuelsStarterTakesUpItsPlanOfTheFight)
{
	auto& registry = test::creature_world::World::Registry();
	const auto starter = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE);
	const auto opponent = test::creature_world::World::MakeCreature({140.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_TWO);
	registry.Assign<CreatureFighting>(
	    starter,
	    CreatureFighting {.stage = CreatureFighting::Stage::Respond, .opponent = opponent, .madeArena = true, .ended = true});
	registry.Get<CreatureMindState>(starter).loadedFightDesire = creature_desires::Desire::Anger;
	_system.ProcessTurn();
	const auto& mind = registry.Get<CreatureMindState>(starter);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, 21u);
	EXPECT_EQ(mind.planner.current->desire, creature_desires::Desire::Anger);
	EXPECT_EQ(mind.planner.current->object, std::optional<uint32_t>(entt::to_integral(opponent)));
	EXPECT_FALSE(mind.loadedFightDesire.has_value());
}

// A creature walking where the leash sent it, or with its mind paused, faints all the same once its life, energy or
// rest gives out, as the game's creature turn takes the faint whatever the leash does; before, it stood there unfainted
TEST_F(CreatureMindSystemTest, OnTheLeashOrWithItsMindPausedItFaintsAllTheSame)
{
	const test::RestoreService<Locator::creatureFightSystem> restoreFights;
	auto& fights = static_cast<StartsFights&>(Locator::creatureFightSystem::emplace<StartsFights>());
	auto& registry = test::creature_world::World::Registry();
	const auto led = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE);
	const auto paused = test::creature_world::World::MakeCreature({200.0f, 0.0f, 100.0f}, PlayerNames::PLAYER_ONE);
	for (int turn = 0; turn < 3; ++turn)
	{
		_system.ProcessTurn();
	}
	registry.Get<CreatureMindState>(led).leash.obeying = true;
	registry.Get<CreatureMindState>(paused).paused = true;
	for (const auto creature : {led, paused})
	{
		registry.Get<CreatureNeeds>(creature).faint = creature_physiology::Faint::OutOfLife;
	}
	_system.ProcessTurn();
	ASSERT_EQ(fights.knockedOut.size(), 2u);
	EXPECT_TRUE(std::ranges::find(fights.knockedOut, led) != fights.knockedOut.end());
	EXPECT_TRUE(std::ranges::find(fights.knockedOut, paused) != fights.knockedOut.end());
}

TEST_F(CreatureMindSystemTest, AFightAScriptForcesAsksTheOtherFirstThenStartsTheDuel)
{
	const test::RestoreService<Locator::creatureFightSystem> restoreFights;
	auto& fights = static_cast<StartsFights&>(Locator::creatureFightSystem::emplace<StartsFights>());
	const auto [guide, other, told] = ForceAFight(_system, _world, fights);
	ASSERT_TRUE(told);
	const auto& mind = test::creature_world::World::Registry().Get<CreatureMindState>(guide);
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_FightRow);
	// both under a script's control, of a size, the other agrees, and the fight between the two is started once
	ASSERT_EQ(fights.started.size(), 1u);
	EXPECT_EQ(fights.started.front(), std::pair(guide, other));
	EXPECT_FALSE(mind.idle.gaveUp);
}

TEST_F(CreatureMindSystemTest, AForcedFightThatCannotStartGivesTheRestUp)
{
	const test::RestoreService<Locator::creatureFightSystem> restoreFights;
	auto& fights = static_cast<StartsFights&>(Locator::creatureFightSystem::emplace<StartsFights>());
	fights.answer = StartsFights::StartResult::Busy;
	const auto [guide, other, told] = ForceAFight(_system, _world, fights);
	ASSERT_TRUE(told);
	ASSERT_EQ(fights.started.size(), 1u);
	EXPECT_TRUE(test::creature_world::World::Registry().Get<CreatureMindState>(guide).idle.gaveUp);
}

TEST_F(CreatureMindSystemTest, AScriptsPlayAndReleaseEndADanceAndItsSideOfAFightThereAndThen)
{
	const test::RestoreService<Locator::creatureFightSystem> restoreFights;
	auto& fights = static_cast<StartsFights&>(Locator::creatureFightSystem::emplace<StartsFights>());
	const auto creature = test::creature_world::World::MakeCreature();
	_system.ProcessTurn();
	auto& animation = test::creature_world::World::Registry().Get<CreatureAnimation>(creature);
	// dancing its moves when the script has it play: at once, its side of a fight is ended and the dance's finish plays
	animation.body = *creature_layers::PlayDance({}, 500.0f);
	animation.body.phase = creature_layers::BodyAction::Phase::Loop;
	_system.SetScriptPlay(creature, 63, 1);
	ASSERT_TRUE(_system.PlayForScript(creature));
	EXPECT_EQ(fights.endedOwn, std::vector {creature});
	EXPECT_EQ(animation.body.kind, creature_layers::BodyAction::Kind::Dance);
	EXPECT_EQ(animation.body.phase, creature_layers::BodyAction::Phase::End);
	EXPECT_FALSE(creature_layers::IsDancing(animation.body));
	// still starting its dance when the script lets it go: the finish is asked for after the start, and its side of a
	// fight is ended again
	animation.body = *creature_layers::PlayDance({}, 500.0f);
	ASSERT_FALSE(animation.body.endWanted);
	_system.ReleasedByScript(creature);
	EXPECT_EQ(fights.endedOwn, (std::vector {creature, creature}));
	EXPECT_EQ(animation.body.phase, creature_layers::BodyAction::Phase::Start);
	EXPECT_TRUE(animation.body.endWanted);
}

// The kind of belief each thing is, as the original's classes give it (an evidence table in the creature mind page of
// the wiki): a big forest, a lantern, a shield, a script's highlight and the singing stone's base are of no kind of their
// own (21), a spell icon its own (17); a static of the rock class a rock (12), any other static that can be moved a
// mobile object (19); a loose thing of the mobile objects' class by its row's kind: magic food (14), poo (13), any other
// a mobile object. The rows' kinds here are the test's own, not the game's
TEST_F(CreatureMindSystemTest, EachThingIsBelievedInByTheKindItsClassGivesIt)
{
	auto& registry = test::creature_world::World::Registry();
	auto& info = _world.Info();
	info.mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock)).mobileType = MobileStaticInfo::Rock;
	info.mobileStatic.at(static_cast<size_t>(MobileStaticInfo::SingingStoneBase)).mobileType = MobileStaticInfo::Rock;
	info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::MagicFood)).mobileType = MobileObjectInfo::MagicFood;
	info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).mobileType = MobileObjectInfo::LumpOfPoo;
	info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::EgyptBarrel)).mobileType = MobileObjectInfo::LumpOfPoo;
	info.mobileObject.at(static_cast<size_t>(MobileObjectInfo::OneOffSpellSeed)).mobileType = MobileObjectInfo::OneOffSpellSeed;
	info.ball.mobileType = MobileObjectInfo::Ball;
	const auto make = [&registry]<typename Component>(Component component) {
		const auto entity = registry.Create();
		registry.Assign<Component>(entity, std::move(component));
		return entity;
	};
	const auto kindOf = [&registry](entt::entity thing) { return mind_detail::BeliefKindOf(std::as_const(registry), thing); };
	constexpr uint32_t k_Other = 21;
	EXPECT_EQ(kindOf(make(Forest {.type = 0})), 1u);
	EXPECT_EQ(kindOf(make(BigForest {.type = 0})), k_Other);
	EXPECT_EQ(kindOf(make(StreetLantern {})), k_Other);
	EXPECT_EQ(kindOf(make(MagicShield {})), k_Other);
	EXPECT_EQ(kindOf(make(ScriptHighlight {})), k_Other);
	EXPECT_EQ(kindOf(make(SpellIcon {})), 17u);
	EXPECT_EQ(kindOf(make(MobileStatic {.type = MobileStaticInfo::Rock})), 12u);
	EXPECT_EQ(kindOf(make(MobileStatic {.type = MobileStaticInfo::Bonfire})), 12u);
	EXPECT_EQ(kindOf(make(Fragment {})), 12u);
	EXPECT_EQ(kindOf(make(MobileStatic {.type = MobileStaticInfo::SingingStoneBase})), k_Other);
	EXPECT_EQ(kindOf(make(MobileStatic {.type = MobileStaticInfo::DeadTree})), 19u);
	EXPECT_EQ(kindOf(make(MobileObject {.type = MobileObjectInfo::MagicFood})), 14u);
	EXPECT_EQ(kindOf(make(MobileObject {.type = MobileObjectInfo::LumpOfPoo})), 13u);
	// by the row's kind, not the row
	EXPECT_EQ(kindOf(make(MobileObject {.type = MobileObjectInfo::EgyptBarrel})), 13u);
	EXPECT_EQ(kindOf(make(MobileObject {.type = MobileObjectInfo::Cart})), 19u);
	EXPECT_EQ(kindOf(make(Ball {})), 19u);
	EXPECT_EQ(kindOf(make(OneOffSpellSeed {})), 19u);
	EXPECT_EQ(kindOf(make(Reward {})), 19u);
}

// A thing is put where it is made, and moves (as the creature's eyes ask it) once it is not there, on the map's grid,
// x and z only, until it is put again where it is (a script's SET_POSITION, the end of its flight)
TEST_F(CreatureMindSystemTest, AThingMovesOnlyAwayFromWhereItWasLastPut)
{
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<MobileStatic>(rock, MobileStatic {.type = MobileStaticInfo::Rock});
	auto& transform = registry.Assign<Transform>(rock, glm::vec3(10.0f, 0.0f, 20.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ASSERT_TRUE(registry.AllOf<PlacedAt>(rock));
	EXPECT_EQ(registry.Get<PlacedAt>(rock).position, glm::vec3(10.0f, 0.0f, 20.0f));
	EXPECT_FALSE(ecs::MovedSincePlaced(std::as_const(registry), rock));
	// up and down, or less than a unit of the map's grid, is no move
	transform.position.y = 5.0f;
	transform.position.x = 10.00001f;
	EXPECT_FALSE(ecs::MovedSincePlaced(std::as_const(registry), rock));
	transform.position.x = 11.0f;
	EXPECT_TRUE(ecs::MovedSincePlaced(std::as_const(registry), rock));
	ecs::PlaceWhereItIs(registry, rock);
	EXPECT_FALSE(ecs::MovedSincePlaced(std::as_const(registry), rock));
	transform.position.z = 19.0f;
	EXPECT_TRUE(ecs::MovedSincePlaced(std::as_const(registry), rock));
	EXPECT_TRUE(ecs::placed::MovedFrom(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 0.001f)));
	EXPECT_FALSE(ecs::placed::MovedFrom(glm::vec3(0.0f), glm::vec3(0.0f, 7.0f, 0.0f)));
}

TEST_F(CreatureMindSystemTest, WaitingForAFriendToStandStillWaitsThroughItsMoveButNotATurnToFace)
{
	const auto creature = test::creature_world::World::MakeCreature({100.0f, 0.0f, 100.0f});
	const auto other = test::creature_world::World::MakeCreature({140.0f, 0.0f, 100.0f});
	_system.ProcessTurn();
	auto& registry = test::creature_world::World::Registry();
	auto& mind = registry.Get<CreatureMindState>(creature);
	auto& walk = registry.AllOf<CreatureLocomotion>(other) ? registry.Get<CreatureLocomotion>(other)
	                                                       : registry.Assign<CreatureLocomotion>(other);
	const auto waitFor = [&](CreatureLocomotion::Motion motion, bool facingOnly) {
		walk.motion = motion;
		walk.facingOnly = facingOnly;
		creature_mind::Plan(
		    mind.idle, creature_mind::Activity::Told,
		    {creature_mind::Step {.kind = creature_mind::Step::Kind::WaitForStill, .object = entt::to_integral(other)},
		     creature_mind::Step {.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f}});
		_system.ProcessTurn();
		return mind.idle.step;
	};
	// on its way, or turning as part of a move, it is waited for
	EXPECT_EQ(waitFor(CreatureLocomotion::Motion::Walking, false), 0u);
	EXPECT_EQ(waitFor(CreatureLocomotion::Motion::Turning, false), 0u);
	// a turn on the spot to face a point is no move, as standing
	EXPECT_EQ(waitFor(CreatureLocomotion::Motion::Turning, true), 1u);
	EXPECT_EQ(waitFor(CreatureLocomotion::Motion::Standing, false), 1u);
}

// A walk that finds no way leaves the creature keeping in mind that it found no way to the thing its step was about,
// when it has a belief about that thing; a step about nothing marks nothing
TEST_F(CreatureMindSystemTest, AWalkThatFindsNoWayMarksTheThingItsStepWasAbout)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto thing = registry.Create();
	mind.beliefs.Add(thing, 1);
	openblack::game_clock::SetTurn(700);
	mind.idle.agenda = {
	    {.kind = creature_mind::Step::Kind::Move,
	     .movement = {.kind = creature_mind::Movement::Kind::GoNearObject, .object = entt::to_integral(thing)}}};
	mind.idle.step = 0;
	_system.FoundNoWay(creature);
	ASSERT_NE(mind.beliefs.Find(thing), nullptr);
	EXPECT_EQ(mind.beliefs.Find(thing)->blockedTurn, std::optional<uint32_t>(700u));
	mind.beliefs.Find(thing)->blockedTurn.reset();
	mind.idle.agenda = {{.kind = creature_mind::Step::Kind::Wait}};
	_system.FoundNoWay(creature);
	EXPECT_FALSE(mind.beliefs.Find(thing)->blockedTurn.has_value());
	openblack::game_clock::SetTurn(0);
}

// The first search of a walk finding it shut in keeps where the walk was going. With nothing met it is not shut in;
// with things met it is, whether or not it could be given anything to clear; once shut in, a second give-up chooses
// nothing more; and it is no longer shut in once it stops what it was doing
TEST_F(CreatureMindSystemTest, AShutInWalkKeepsItsPointAndStoppingEndsIt)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	_system.ShutIn(creature, {}, {120.0f, 340.0f});
	EXPECT_EQ(mind.keptWalkPoint, std::optional(glm::vec2(120.0f, 340.0f)));
	EXPECT_FALSE(mind.trapped);
	const std::array met {registry.Create()};
	_system.ShutIn(creature, met, {125.0f, 345.0f});
	EXPECT_EQ(mind.keptWalkPoint, std::optional(glm::vec2(125.0f, 345.0f)));
	EXPECT_TRUE(mind.trapped);
	_system.ShutIn(creature, met, {130.0f, 350.0f});
	EXPECT_EQ(mind.keptWalkPoint, std::optional(glm::vec2(130.0f, 350.0f)));
	EXPECT_TRUE(mind.trapped);
	_system.AbandonAction(creature);
	EXPECT_FALSE(mind.trapped);
}

// A stroke's or a slap's action plays on the body alone, as the game's hand forces it: the mind keeps its plan and its
// agenda, which wait for the body, so that its next plan does not cut the action short
TEST_F(CreatureMindSystemTest, AStrokesActionPlaysOnTheBodyAndTheMindKeepsItsPlan)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Play, .action = 7};
	mind.idle.agenda = {{.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f},
	                    {.kind = creature_mind::Step::Kind::Wait, .seconds = 5.0f}};
	mind.idle.step = 0;
	ASSERT_TRUE(_system.ForceAction(creature, 157, false, std::nullopt, 0.8));
	EXPECT_TRUE(creature_layers::IsPlaying(registry.Get<CreatureAnimation>(creature).body));
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, 7u);
	EXPECT_EQ(mind.idle.agenda.size(), 2u);
}

// A grown creature given a one-shot globe of a miracle for creatures, of anger, casts it at the nearest other creature
// it knows of, and the cast is a lesson in the miracle
TEST_F(CreatureMindSystemTest, AGrownCreatureGivenAGlobeOfACreatureMiracleCastsItAtTheNearestCreature)
{
	constexpr size_t k_TakeRow = 142;
	constexpr size_t k_CastRow = 228;
	constexpr auto k_Miracle = MagicType::CreatureSpellFreeze;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("TakeObjectFromHand"), info.creatureAction.at(k_TakeRow).name.begin());
	std::ranges::copy(std::string_view("CastOneOffSpellInHandAggressive"), info.creatureAction.at(k_CastRow).name.begin());
	info.creatureMagicActionKnownAboutEntry.at(static_cast<size_t>(k_Miracle)).field0x40 = 2;
	info.spellSeed.at(static_cast<size_t>(SpellSeedType::Storm)).magicTypes[0] = k_Miracle;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto other = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.developmentPhase = 8;
	mind.beliefs.Add(other, creature_tree::belief_types::k_Creature);
	const auto globe = registry.Create();
	registry.Assign<Transform>(globe, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<OneOffSpellSeed>(globe).seedType = SpellSeedType::Storm;
	hands.held = globe;
	ASSERT_TRUE(_system.TakeFromHand(creature, globe, false, glm::vec2(0.0f)));
	_system.AbandonAction(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_CastRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(entt::to_integral(other)));
}

// A grown creature given a globe of another miracle casts it from the miracle's desire; when the casting action is done
// to nothing it casts it at itself
TEST_F(CreatureMindSystemTest, AGrownCreatureGivenAGlobeOfAMiracleDoneToNothingCastsItAtItself)
{
	constexpr size_t k_TakeRow = 142;
	constexpr size_t k_CastRow = 229;
	constexpr auto k_Miracle = MagicType::Heal;
	auto& info = _world.Info();
	std::ranges::copy(std::string_view("TakeObjectFromHand"), info.creatureAction.at(k_TakeRow).name.begin());
	std::ranges::copy(std::string_view("CastOneOffSpellInHandCompassiona"), info.creatureAction.at(k_CastRow).name.begin());
	info.creatureAction.at(k_CastRow).field0x94 = 0;
	info.creatureMagicActionKnownAboutEntry.at(static_cast<size_t>(k_Miracle)).field0x40 = 1;
	info.spellSeed.at(static_cast<size_t>(SpellSeedType::Storm)).magicTypes[0] = k_Miracle;
	const test::RestoreService<Locator::creatureObjectActionSystem> restoreHands;
	auto& hands = static_cast<test::creature_fakes::FakeObjectAction&>(
	    Locator::creatureObjectActionSystem::emplace<test::creature_fakes::FakeObjectAction>());
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.developmentPhase = 8;
	const auto globe = registry.Create();
	registry.Assign<Transform>(globe, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<OneOffSpellSeed>(globe).seedType = SpellSeedType::Storm;
	hands.held = globe;
	ASSERT_TRUE(_system.TakeFromHand(creature, globe, false, glm::vec2(0.0f)));
	_system.AbandonAction(creature);
	_system.ProcessTurn();
	ASSERT_TRUE(mind.planner.current.has_value());
	EXPECT_EQ(mind.planner.current->action, k_CastRow);
	EXPECT_EQ(mind.planner.current->object, std::optional(entt::to_integral(creature)));
}

// An agenda a saved game kept is taken up as an order its mind waits on, as the game's load leaves every kept agenda:
// a script's play loaded starts again, the creature commanded while it plays
TEST_F(CreatureMindSystemTest, AKeptAgendaTakenUpFromASaveIsAnOrderItsMindWaitsOn)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	mind.loadedScriptPlay = creature_script_play::Request {.animation = 63, .plays = 2};
	_system.ProcessTurn();
	EXPECT_FALSE(mind.loadedScriptPlay.has_value());
	EXPECT_FALSE(mind.idle.agenda.empty());
	EXPECT_TRUE(mind.commanded);
}

// Taking up a new plan while in the fight its plan set out on ends the fight as the game ends it: the creature gains a
// fight's strength, an opponent dueling it stops, its own side of the duel ends; a fight still being set up is left by
// both. A fight its plan did not set out on, or no fight, is left alone
TEST_F(CreatureMindSystemTest, ANewPlanEndsTheFightItsPlanSetOutOn)
{
	using openblack::test::creature_loop_fakes::CallLog;
	using openblack::test::creature_loop_fakes::FakeFight;
	constexpr size_t k_ReflectionRow = 44;
	std::ranges::copy(std::string_view("LookAtReflection"), _world.Info().creatureAction.at(k_ReflectionRow).name.begin());
	const test::RestoreService<Locator::creatureFightSystem> restoreFights;
	CallLog log;
	Locator::creatureFightSystem::emplace<FakeFight>(log);
	auto& registry = test::creature_world::World::Registry();
	const auto creature = test::creature_world::World::MakeCreature();
	auto& mind = registry.Get<CreatureMindState>(creature);
	const auto plan = [&]() {
		const CreatureMindSystem::ForcedActivity wait {
		    .desire = creature_desires::Desire::Curiosity,
		    .action = "LookAtReflection",
		    .agenda = std::vector<creature_mind::Step> {{.kind = creature_mind::Step::Kind::Wait, .seconds = 1.0f}}};
		log.clear();
		std::ignore = _system.ForceActivity(creature, wait);
		std::vector<std::string> out;
		for (const auto& call : log)
		{
			if (call.name.starts_with("fight.") && call.name != "fight.ProcessTurn")
			{
				out.push_back(call.name);
			}
		}
		return out;
	};

	EXPECT_TRUE(plan().empty());
	auto& fighting = registry.Assign<CreatureFighting>(creature);
	fighting.stage = CreatureFighting::Stage::Approach;
	// a fight it did not set out on itself, as a script's
	mind.fightStarted = false;
	EXPECT_TRUE(plan().empty());
	mind.fightStarted = true;
	EXPECT_EQ(plan(), (std::vector<std::string> {"fight.FailsWhileDueling", "fight.PartnerStops", "fight.EndOwnFight",
	                                             "fight.CallOff"}));
	fighting.stage = CreatureFighting::Stage::Duel;
	mind.fightStarted = true;
	EXPECT_EQ(plan(), (std::vector<std::string> {"fight.FailsWhileDueling", "fight.PartnerStops", "fight.EndOwnFight"}));
	registry.Remove<CreatureFighting>(creature);
}
