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
// gives it, and the hands pointing where the agenda says

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "3D/DayNightClock.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureLook.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreatureSpellMind.h"
#include "Creature/CreatureWatching.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Systems/Implementations/CreatureMindSystem.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

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

TEST_F(CreatureMindSystemTest, TheDesiresStartFromTheSpeciesTables)
{
	const auto setup = mind_detail::SetupFor(CreatureType::GiantApe);
	for (size_t d = 0; d < setup.size(); ++d)
	{
		// the row's cap, then the range its decay is drawn from (docs/bw1-notes/creature.md)
		EXPECT_FLOAT_EQ(setup.at(d).max, 2.0f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMin, 0.9f);
		EXPECT_FLOAT_EQ(setup.at(d).decayMax, 0.9f + (0.001f * static_cast<float>(d + 1)));
	}
	// without the tables, the defaults
	Locator::infoConstants::reset();
	EXPECT_FLOAT_EQ(mind_detail::SetupFor(CreatureType::GiantApe).front().max, 1.0f);
}

TEST_F(CreatureMindSystemTest, TheFirstTurnDrawsFortyFloatsInDesireOrder)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	std::vector<float> floats;
	game_random::testing::SetGameRand([](uint32_t) { return 0u; },
	                                  [&floats](float x) {
		                                  floats.push_back(x);
		                                  return x * 0.5f;
	                                  });
	const auto local = game_random::Current().local;
	const auto crt = game_random::crt::Seed();

	_system.ProcessTurn();

	ASSERT_EQ(floats.size(), creature_desires::k_DesireCount);
	const auto& desires = test::creature_world::World::Registry().Get<CreatureMindState>(creature).desires;
	ASSERT_TRUE(desires.has_value());
	for (size_t d = 0; d < floats.size(); ++d)
	{
		const auto range = (0.9f + (0.001f * static_cast<float>(d + 1))) - 0.9f;
		EXPECT_FLOAT_EQ(floats.at(d), range);
		// low + the draw below high - low
		EXPECT_FLOAT_EQ(desires->desires.at(d).decay, 0.9f + (range * 0.5f));
	}
	EXPECT_EQ(game_random::Current().local, local);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// the second turn draws them no more
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

TEST_F(CreatureMindSystemTest, ANastyMiracleFrightensItAndItRunsUnlessOnTheRopeOrUnafraid)
{
	const game_random::testing::ScopedState state;
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

	// afraid and free, it runs from where the miracle struck; the fear of scary magic grows by a half
	fear.value = 0.5f;
	EXPECT_FLOAT_EQ(scare(), 0.5f);
	EXPECT_TRUE(Moves(mind, Kind::FleeFrom, at));
	EXPECT_FALSE(Moves(mind, Kind::ToPoint, at));
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
}

TEST_F(CreatureMindSystemTest, AMiracleItReactsToTeachesItOnlyWhenTheHandlerSaysSoAndItsMindIsNotStilled)
{
	const game_random::testing::ScopedState state;
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

TEST(CreatureMindLook, EveryBirdAndTheVultureCatchTheEyeAsADove)
{
	// the kinds the original looks at as much as a dove; every other animal as an animal. Traced through the original's
	// animal factory: 0-4, 6 and 8-16, 20-26; the Puzzle ones (27-30) are of the animal class by their vtables. Not
	// traced, so these pin openblack's choice rather than the original's data (creature.md Pending): Goat (5), Zebra (7)
	// and the Vulture (17), taken by their class's name, and CitadelDove and CitadelBat (18, 19), whose class is unknown
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

TEST_F(CreatureMindSystemTest, AMindFileKeepsTheDecaysDrawnForTheCreature)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	// a quarter of each range, not its middle
	game_random::testing::SetGameRand([](uint32_t) { return 0u; }, [](float x) { return x * 0.25f; });
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

TEST_F(CreatureMindSystemTest, OnlyACreatureIsTaught)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = MakeVillager(glm::vec3(0.0f));
	EXPECT_FALSE(_system.SetKnowsAction(villager, 0, 4, true));
	EXPECT_FALSE(_system.SetKnowsAction(entt::null, 0, 4, true));
	EXPECT_FALSE(registry.AllOf<CreatureMindState>(villager));
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
