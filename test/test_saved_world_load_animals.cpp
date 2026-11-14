/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flocks, the animals and birds, and the creatures of a saved game's world file, as a load plans them: in the
// order their reads ended, each with the type its row names, and how many of each the load made
// (docs/wiki/engine/saved-games.md#what-a-load-does)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include <AnimalRecords.h>
#include <CreatureRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
namespace load = openblack::ecs::saved_world_load;

namespace
{
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / relative;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	std::ifstream stream(path, std::ios::binary);
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream), {});
}

const std::filesystem::path k_LandOneAutosave =
    std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") / "Saved Games" / "20" / "20.sav";
const std::filesystem::path k_LandTwoAutosave = std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "20" / "20.sav";

/// The game's log, which the makers write their lines to
void GameLog()
{
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
}

uint32_t MadeOf(const load::Census& census, uint32_t type)
{
	const auto line = std::ranges::find(census, type, &load::CensusLine::type);
	return line != census.end() ? line->made : 0;
}
} // namespace

TEST(SavedWorldLoadAnimals, TheAnimalRowsRunFromTheLionsDown)
{
	EXPECT_EQ(load::AnimalTypeOfInfo(2548), AnimalInfo::Lion);
	EXPECT_EQ(load::AnimalTypeOfInfo(2548 - 12), AnimalInfo::Dove);
	EXPECT_EQ(load::AnimalTypeOfInfo(2518), AnimalInfo::PuzzlePig);
	EXPECT_EQ(load::AnimalTypeOfInfo(2549), std::nullopt);
	EXPECT_EQ(load::AnimalTypeOfInfo(2517), std::nullopt);
}

TEST(SavedWorldLoadAnimals, TheCreatureRowsRunFromTheGiantApesDown)
{
	EXPECT_EQ(load::CreatureSpeciesOfInfo(2517), CreatureType::GiantApe);
	EXPECT_EQ(load::CreatureSpeciesOfInfo(2516), CreatureType::Cow);
	EXPECT_EQ(load::CreatureSpeciesOfInfo(2501), CreatureType::Gorilla);
	EXPECT_EQ(load::CreatureSpeciesOfInfo(2518), std::nullopt);
	EXPECT_EQ(load::CreatureSpeciesOfInfo(2500), std::nullopt);
}

// The plan takes each flock, animal and creature in the order the reads ended; Apply makes the animals, then the flocks
// (which name them), then the creatures, and counts what its makers made
TEST(SavedWorldLoadAnimals, TheMakersAreCalledInTheReadsOrderAndCounted)
{
	gamesave::WorldRead world;
	world.objects.resize(4);
	gamesave::SheepRecord sheep;
	sheep.object.info = 2548 - static_cast<uint32_t>(AnimalInfo::Sheep);
	gamesave::DoveRecord dove;
	dove.object.info = 2548 - static_cast<uint32_t>(AnimalInfo::Dove);
	gamesave::CreatureRecord creature;
	creature.object.info = 2517 - static_cast<uint32_t>(CreatureType::Lion);
	world.objects[0] = gamesave::SavedRecord(dove);
	world.objects[1] = gamesave::SavedRecord(gamesave::FlockRecord {});
	world.objects[2] = gamesave::SavedRecord(sheep);
	world.objects[3] = gamesave::SavedRecord(creature);
	world.loadOrder = {3, 1, 2, 4};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.flocks.size(), 1u);
	EXPECT_EQ(plan.flocks[0].number, 2u);
	ASSERT_EQ(plan.animals.size(), 2u);
	EXPECT_EQ(plan.animals[0].number, 3u);
	EXPECT_EQ(plan.animals[0].type, AnimalInfo::Sheep);
	EXPECT_EQ(plan.animals[0].recordType, gamesave::SheepRecord::k_Type);
	EXPECT_EQ(plan.animals[1].number, 1u);
	EXPECT_EQ(plan.animals[1].type, AnimalInfo::Dove);
	ASSERT_EQ(plan.creatures.size(), 1u);
	EXPECT_EQ(plan.creatures[0].species, CreatureType::Lion);

	auto withCensus = plan;
	withCensus.census = {{gamesave::FlockRecord::k_Type, 1},
	                     {gamesave::SheepRecord::k_Type, 1},
	                     {gamesave::DoveRecord::k_Type, 1},
	                     {gamesave::CreatureRecord::k_Type, 1}};
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	std::vector<gamesave::ObjectNumber> order;
	load::Makers makers;
	makers.flock = [&](const load::PlannedFlock& flock, const auto& objectOf) {
		order.push_back(flock.number);
		EXPECT_NE(objectOf(3), entt::entity {entt::null}) << "the animals are made before the flocks";
		EXPECT_EQ(objectOf(1), entt::entity {entt::null}) << "the dove is not made";
		return registry.Create();
	};
	// the dove is not made
	makers.animal = [&](const load::PlannedAnimal& animal, const auto&) {
		order.push_back(animal.number);
		return animal.type == AnimalInfo::Dove ? entt::entity {entt::null} : registry.Create();
	};
	makers.creature = [&](const load::PlannedCreature& planned, const auto&) {
		order.push_back(planned.number);
		return registry.Create();
	};
	const auto census = load::Apply(withCensus, makers);
	EXPECT_EQ(order, (std::vector<gamesave::ObjectNumber> {3, 1, 2, 4}));
	EXPECT_EQ(MadeOf(census, gamesave::FlockRecord::k_Type), 1u);
	EXPECT_EQ(MadeOf(census, gamesave::SheepRecord::k_Type), 1u);
	EXPECT_EQ(MadeOf(census, gamesave::DoveRecord::k_Type), 0u);
	EXPECT_EQ(MadeOf(census, gamesave::CreatureRecord::k_Type), 1u);
}

// An animal of a flock that is not made is taken away again; the animals made are given for the resolves that draw,
// in the order their reads ended, and not resolved by Apply itself
TEST(SavedWorldLoadAnimals, TheAnimalsOfAFlockNotMadeGoAndTheOthersWaitForTheirResolve)
{
	GameLog();
	gamesave::WorldRead world;
	world.objects.resize(3);
	gamesave::SheepRecord herded;
	herded.object.info = 2548 - static_cast<uint32_t>(AnimalInfo::Sheep);
	herded.living.flock.number = 2;
	gamesave::CowRecord alone;
	alone.object.info = 2548 - static_cast<uint32_t>(AnimalInfo::Cow);
	gamesave::FlockRecord flock;
	flock.members.numbers = {1};
	world.objects[0] = gamesave::SavedRecord(herded);
	world.objects[1] = gamesave::SavedRecord(flock);
	world.objects[2] = gamesave::SavedRecord(alone);
	world.loadOrder = {2, 1, 3};
	auto plan = load::PlanOf(world);
	plan.census = {{gamesave::SheepRecord::k_Type, 1}, {gamesave::CowRecord::k_Type, 1}, {gamesave::FlockRecord::k_Type, 1}};

	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	std::vector<entt::entity> made;
	load::Makers makers;
	makers.animal = [&](const load::PlannedAnimal& animal, const auto&) {
		const auto entity = registry.Create();
		registry.Assign<components::Animal>(entity, animal.type, 0u, entt::entity {entt::null}, true);
		made.push_back(entity);
		return entity;
	};
	makers.flock = [](const load::PlannedFlock&, const auto&) { return entt::entity {entt::null}; };
	std::vector<entt::entity> resolves;
	const auto census = load::Apply(plan, makers, nullptr, &resolves);
	ASSERT_EQ(made.size(), 2u);
	EXPECT_FALSE(registry.Valid(made[0])) << "the sheep of the flock not made";
	EXPECT_TRUE(registry.Valid(made[1]));
	EXPECT_EQ(MadeOf(census, gamesave::SheepRecord::k_Type), 0u);
	EXPECT_EQ(MadeOf(census, gamesave::CowRecord::k_Type), 1u);
	EXPECT_EQ(resolves, (std::vector<entt::entity> {made[1]}));
}

// Two creatures in their duel are made with their fight over, as the game's load leaves it with no fight of their
// bodies: each responds to it, fighting the other, the starter with the arena its record names, each with its saved
// fight health and who fights for it; one whose opponent is not made fights no more
TEST(SavedWorldLoadAnimals, TwoCreaturesInTheirDuelAreMadeRespondingToItsEnd)
{
	GameLog();
	load::WorldPlan plan;
	load::PlannedCreature starter;
	starter.number = 11;
	starter.species = CreatureType::Lion;
	starter.record.creature.keepsPlan = 1;
	starter.record.creature.arena = {.number = 30};
	starter.record.creature.fightHealth = 0.75f;
	starter.record.creature.autoFight = 1;
	starter.record.creature.plan = {
	    .action = 21, .current = 14, .count = 18, .steps = {{.action = 0x1C, .object = {.number = 12}}}};
	auto opponent = starter;
	opponent.number = 12;
	opponent.record.creature.arena = {};
	opponent.record.creature.fightHealth = 0.5f;
	opponent.record.creature.autoFight = 2;
	opponent.record.creature.plan = {
	    .desire = 26, .action = 124, .count = 2, .steps = {{.action = 0x1C, .object = {.number = 11}}, {.action = 0x4E}}};
	auto lonely = opponent;
	lonely.number = 13;
	lonely.record.creature.plan.steps.front().object = {.number = 14};
	plan.creatures = {starter, opponent, lonely};
	gamesave::ArenaRecord arena;
	arena.position.position = {.x = 6553600, .z = 13107200, .altitude = 0.0f};
	arena.words[1] = std::bit_cast<uint32_t>(40.0f);
	plan.arenas.push_back({.number = 30, .record = arena});

	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<Registry>();
	std::map<gamesave::ObjectNumber, entt::entity> made;
	load::Makers makers;
	makers.creature = [&](const load::PlannedCreature& planned, const auto&) {
		const auto entity = registry.Create();
		made[planned.number] = entity;
		return entity;
	};
	std::vector<entt::entity> resolves;
	std::ignore = load::Apply(plan, makers, nullptr, &resolves);
	ASSERT_TRUE(registry.AllOf<components::CreatureFighting>(made.at(11)));
	ASSERT_TRUE(registry.AllOf<components::CreatureFighting>(made.at(12)));
	EXPECT_FALSE(registry.AllOf<components::CreatureFighting>(made.at(13)));
	const auto& first = registry.Get<components::CreatureFighting>(made.at(11));
	const auto& second = registry.Get<components::CreatureFighting>(made.at(12));
	EXPECT_EQ(first.stage, components::CreatureFighting::Stage::Respond);
	EXPECT_EQ(second.stage, components::CreatureFighting::Stage::Respond);
	EXPECT_EQ(first.opponent, made.at(12));
	EXPECT_EQ(second.opponent, made.at(11));
	EXPECT_TRUE(first.madeArena);
	EXPECT_FALSE(second.madeArena);
	EXPECT_TRUE(first.ended);
	EXPECT_FALSE(first.played);
	EXPECT_EQ(first.arena.centre, glm::vec2(1000.0f, 2000.0f));
	EXPECT_EQ(first.arena.radius, 40.0f);
	EXPECT_EQ(first.fighter.health, 0.75f);
	EXPECT_EQ(second.fighter.health, 0.5f);
	EXPECT_EQ(first.fighter.control, creature_fight::Control::Player);
	EXPECT_EQ(second.fighter.control, creature_fight::Control::Computer);
}

// A flock's load puts each member, in its record's order, before the first member from the head whose place is not
// lower: members of the same place come back the other way round
TEST(SavedWorldLoadAnimals, AFlocksLoadLinesItsMembersUpByTheirPlaces)
{
	EXPECT_TRUE(load::LoadedFlockLine({}).empty());
	EXPECT_EQ(load::LoadedFlockLine(std::vector<uint8_t> {0, 0, 0}), (std::vector<size_t> {2, 1, 0}));
	EXPECT_EQ(load::LoadedFlockLine(std::vector<uint8_t> {5, 1, 3}), (std::vector<size_t> {1, 2, 0}));
	EXPECT_EQ(load::LoadedFlockLine(std::vector<uint8_t> {2, 0, 2, 1}), (std::vector<size_t> {1, 3, 2, 0}));
}

// A member's place in its flock's line is its saved one, but 0 for a member whose own read was still going on while
// its flock was read (it began before the flock's and ended after it): its place is read only after its flock
TEST(SavedWorldLoadAnimals, AMemberStillBeingReadTakesPlaceZeroInItsFlocksLine)
{
	gamesave::WorldRead world;
	world.objects.resize(4);
	gamesave::SheepRecord first;
	first.living.secondByte = 7;
	first.living.flock.number = 3;
	gamesave::SheepRecord second;
	second.living.secondByte = 9;
	second.living.flock.number = 3;
	gamesave::FlockRecord flock;
	flock.members.numbers = {2, 4};
	world.objects[1] = gamesave::SavedRecord(first);
	world.objects[2] = gamesave::SavedRecord(flock);
	world.objects[3] = gamesave::SavedRecord(second);
	// the first sheep's read pulls the flock in, which reads the second sheep whole
	world.loadOrder = {4, 3, 2};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.flocks.size(), 1u);
	EXPECT_EQ(plan.flocks[0].lineOrders, (std::vector<uint8_t> {0, 9}));
}

// A flock that names an object the load did not make is not made
TEST(SavedWorldLoadAnimals, AFlockWithAMemberNotMadeIsNotMade)
{
	GameLog();
	load::PlannedFlock planned;
	planned.record.members.numbers = {5};
	EXPECT_EQ(load::MakeFlockEntity(planned, [](gamesave::ObjectNumber) { return entt::entity {entt::null}; }),
	          entt::entity {entt::null});
	planned.record.members.numbers.clear();
	planned.record.town.number = 6;
	EXPECT_EQ(load::MakeFlockEntity(planned, [](gamesave::ObjectNumber) { return entt::entity {entt::null}; }),
	          entt::entity {entt::null});
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's 116 animals and birds and 16 flocks, no creature; every animal's row names an animal type
TEST(SavedWorldLoadAnimals, TheFrozenLandOneAutosavesAnimalsAndFlocksArePlanned)
{
	const auto bytes = FrozenSave(k_LandOneAutosave);
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto plan = load::PlanOf(world);
	EXPECT_EQ(plan.animals.size(), 116u);
	EXPECT_EQ(plan.flocks.size(), 16u);
	EXPECT_TRUE(plan.creatures.empty());
	for (const auto& animal : plan.animals)
	{
		EXPECT_NE(animal.type, AnimalInfo::None) << "animal " << animal.number << " row " << animal.object.info;
	}
	const auto doves = std::ranges::count(plan.animals, gamesave::DoveRecord::k_Type, &load::PlannedAnimal::recordType);
	EXPECT_EQ(doves, 40);
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 2 autosave's three creatures,
// each of a species, its 57 flocks, and its animals and birds, each of the type its class makes
TEST(SavedWorldLoadAnimals, TheFrozenLandTwoAutosavesCreaturesAnimalsAndFlocksArePlanned)
{
	const auto bytes = FrozenSave(k_LandTwoAutosave);
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 2 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.creatures.size(), 3u);
	for (const auto& creature : plan.creatures)
	{
		EXPECT_TRUE(creature.species.has_value()) << "creature " << creature.number << " row " << creature.record.object.info;
	}
	EXPECT_EQ(plan.flocks.size(), 57u);
	for (const auto& animal : plan.animals)
	{
		ASSERT_NE(animal.type, AnimalInfo::None) << "animal " << animal.number << " row " << animal.object.info;
		if (animal.recordType == gamesave::SheepRecord::k_Type)
		{
			EXPECT_EQ(animal.type, AnimalInfo::Sheep);
		}
		else if (animal.recordType == gamesave::WolfRecord::k_Type)
		{
			EXPECT_EQ(animal.type, AnimalInfo::Wolf);
		}
		else if (animal.recordType == gamesave::CrowRecord::k_Type)
		{
			EXPECT_EQ(animal.type, AnimalInfo::Crow);
		}
	}
	EXPECT_EQ(std::ranges::count(plan.animals, gamesave::SheepRecord::k_Type, &load::PlannedAnimal::recordType), 113);
}

// The creatures the load does not make yet: one of no species row, held, fighting, carrying out a kept plan other than
// a script's play or the plan a finished one leaves, holding a miracle or with a spell waiting; any other is made
TEST(SavedWorldLoadAnimals, TheCreaturesTheLoadDoesNotMakeYetSayWhy)
{
	load::PlannedCreature planned;
	planned.species = CreatureType::Lion;
	EXPECT_EQ(load::WhyCreatureNotMade(planned), nullptr);
	auto none = planned;
	none.species.reset();
	EXPECT_NE(load::WhyCreatureNotMade(none), nullptr);
	auto held = planned;
	held.record.thing.flags = 1;
	EXPECT_NE(load::WhyCreatureNotMade(held), nullptr);
	auto fighting = planned;
	fighting.record.creature.arena.number = 9;
	EXPECT_NE(load::WhyCreatureNotMade(fighting), nullptr);
	// one in its duel is made, the creature that started it and its opponent
	auto duelling = fighting;
	duelling.record.creature.keepsPlan = 1;
	duelling.record.creature.plan = {
	    .action = 21, .current = 14, .count = 18, .steps = {{.action = 0x1C, .object = {.number = 12}}}};
	EXPECT_EQ(load::WhyCreatureNotMade(duelling), nullptr);
	auto opponent = planned;
	opponent.record.creature.keepsPlan = 1;
	opponent.record.creature.plan = {
	    .desire = 26, .action = 124, .count = 2, .steps = {{.action = 0x1C, .object = {.number = 11}}, {.action = 0x4E}}};
	EXPECT_EQ(load::WhyCreatureNotMade(opponent), nullptr);
	auto planning = planned;
	planning.record.creature.keepsPlan = 1;
	EXPECT_NE(load::WhyCreatureNotMade(planning), nullptr);
	auto cleared = planning;
	cleared.record.creature.plan = {.desire = 40};
	EXPECT_EQ(load::WhyCreatureNotMade(cleared), nullptr);
	auto playing = planning;
	playing.record.creature.plan = {.desire = 24, .action = 22, .steps = {{.action = 10, .integer = 63}, {.action = 0x85}}};
	EXPECT_EQ(load::WhyCreatureNotMade(playing), nullptr);
	// and one made to look at the camera
	auto looking = planning;
	looking.record.creature.plan = {.desire = 24, .action = 82, .count = 1, .steps = {{.action = 0x42, .integer = 75}}};
	EXPECT_EQ(load::WhyCreatureNotMade(looking), nullptr);
	// and one a script sends to a point
	auto sent = planning;
	sent.record.creature.plan = {.desire = 24, .action = 22, .count = 2, .steps = {{.action = 8}, {.action = 8}}};
	EXPECT_EQ(load::WhyCreatureNotMade(sent), nullptr);
	// one that keeps villagers it called to dance with it, or the dance it made, is made
	auto dancing = planned;
	dancing.record.creature.dancePartners.numbers = {12, 13};
	dancing.record.creature.dance.number = 14;
	EXPECT_EQ(load::WhyCreatureNotMade(dancing), nullptr);
	// one with the beams of a cast from above at its hands is made, its beams given back; one holding a miracle is not
	auto beaming = planned;
	beaming.record.creature.handParticles[1].number = 4;
	EXPECT_EQ(load::WhyCreatureNotMade(beaming), nullptr);
	auto casting = planned;
	casting.record.creature.spellVisuals[0].number = 4;
	EXPECT_NE(load::WhyCreatureNotMade(casting), nullptr);
	auto waiting = planned;
	waiting.record.creature.queued.push_back({});
	EXPECT_NE(load::WhyCreatureNotMade(waiting), nullptr);
}

// A script's play kept in a save is taken up again as the play it was: the animation and the plays left, individual
// numbers by sub-action 10, static ones by 4; a plan of any other step, or of two animations, is none
TEST(SavedWorldLoadAnimals, AKeptScriptsPlayIsTheAnimationAndThePlaysLeft)
{
	const gamesave::KeptCreaturePlan twice {
	    .desire = 24, .action = 22, .steps = {{.action = 10, .integer = 63}, {.action = 10, .integer = 63}, {.action = 0x85}}};
	const auto request = load::ScriptPlayOf(twice);
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->animation, 63u);
	EXPECT_EQ(request->plays, 2u);
	const gamesave::KeptCreaturePlan still {
	    .desire = 24, .action = 22, .steps = {{.action = 4, .integer = 20}, {.action = 0x85}}};
	EXPECT_EQ(load::ScriptPlayOf(still)->animation, 20u);
	const gamesave::KeptCreaturePlan waiting {.desire = 24, .action = 22, .steps = {{.action = 0x85}}};
	EXPECT_EQ(load::ScriptPlayOf(waiting)->plays, 0u);
	auto mixed = twice;
	mixed.steps[1].integer = 64;
	EXPECT_FALSE(load::ScriptPlayOf(mixed).has_value());
	auto walk = twice;
	walk.steps[0].action = 8;
	EXPECT_FALSE(load::ScriptPlayOf(walk).has_value());
	EXPECT_TRUE(load::IsClearedPlan({.desire = 40}));
	EXPECT_FALSE(load::IsClearedPlan(twice));
}

// A script's move kept in a save is taken up again as the legs left of it, each a point at its height and the distance
// it walks to within of it; a plan with any other step, or of no step, is none
TEST(SavedWorldLoadAnimals, AKeptScriptsMoveIsItsLegsLeft)
{
	const gamesave::KeptCreaturePlan sent {.desire = 24,
	                                       .action = 22,
	                                       .current = 1,
	                                       .count = 2,
	                                       .steps = {{.action = 8, .point = {2500.0f, 30.0f, 2600.0f}, .real = 12.5f}}};
	const auto legs = load::ScriptMoveOf(sent);
	ASSERT_TRUE(legs.has_value());
	ASSERT_EQ(legs->size(), 1u);
	EXPECT_EQ(legs->front().first, glm::vec3(2500.0f, 30.0f, 2600.0f));
	EXPECT_EQ(legs->front().second, 12.5f);
	auto playing = sent;
	playing.steps.push_back({.action = 0x85});
	EXPECT_FALSE(load::ScriptMoveOf(playing).has_value());
	auto none = sent;
	none.steps.clear();
	EXPECT_FALSE(load::ScriptMoveOf(none).has_value());
	auto forced = sent;
	forced.action = 82;
	EXPECT_FALSE(load::ScriptMoveOf(forced).has_value());
}

// A duel kept in a save: the creature that started it at the fight step of its fight's agenda, its opponent at the
// fight its order gave it, each naming the other; before the fight step or with any other step it is none. Who fights
// for it: 1 the player, 2 the computer, else nobody
TEST(SavedWorldLoadAnimals, AKeptDuelIsTheStarterOrTheOpponentAndTheOther)
{
	const gamesave::KeptCreaturePlan starter {
	    .action = 21, .current = 14, .count = 18, .steps = {{.action = 0x1C, .object = {.number = 12}}, {.action = 0x4A}}};
	EXPECT_EQ(load::DuelOf(starter), (load::KeptDuel {.startedIt = true, .other = 12}));
	const gamesave::KeptCreaturePlan opponent {
	    .desire = 26, .action = 124, .count = 2, .steps = {{.action = 0x1C, .object = {.number = 11}}, {.action = 0x4E}}};
	EXPECT_EQ(load::DuelOf(opponent), (load::KeptDuel {.startedIt = false, .other = 11}));
	auto before = starter;
	before.current = 13;
	EXPECT_FALSE(load::DuelOf(before).has_value());
	auto responding = starter;
	responding.steps.front().action = 0x48;
	EXPECT_FALSE(load::DuelOf(responding).has_value());
	auto alone = opponent;
	alone.steps.front().object = {};
	EXPECT_FALSE(load::DuelOf(alone).has_value());
	EXPECT_EQ(load::AutoFightControlOf(1), creature_fight::Control::Player);
	EXPECT_EQ(load::AutoFightControlOf(2), creature_fight::Control::Computer);
	EXPECT_EQ(load::AutoFightControlOf(0), creature_fight::Control::None);
}

// A plan forced on a creature kept in a save is taken up again as the forced plan it was: a look at the camera at its
// step with the turns it looks for, lying dead for good from its first step; any other forced plan is none
TEST(SavedWorldLoadAnimals, AKeptForcedLookOrDeathIsTheActionAndItsStep)
{
	const gamesave::KeptCreaturePlan looking {.desire = 24,
	                                          .action = 82,
	                                          .objects = {gamesave::Ref {}, gamesave::Ref {.number = 7}, gamesave::Ref {}},
	                                          .state = 2,
	                                          .count = 1,
	                                          .steps = {{.action = 0x42, .integer = 75}}};
	EXPECT_EQ(load::ForcedPlanOf(looking),
	          (load::KeptForcedPlan {.action = 82, .objects = {0, 7, 0}, .step = 0, .count = 1, .turns = 75}));
	const gamesave::KeptCreaturePlan dead {
	    .desire = 24, .action = 261, .count = 2, .steps = {{.action = 0x0E, .integer = 3}, {.action = 0x18, .integer = 900}}};
	EXPECT_EQ(load::ForcedPlanOf(dead), (load::KeptForcedPlan {.action = 261, .count = 2}));
	// sleeping at home, at its sleep, with no yawn first (four steps) or with one (five)
	const gamesave::KeptCreaturePlan sleeping {
	    .desire = 24, .action = 17, .current = 2, .count = 4, .steps = {{.action = 0x84}, {.action = 0x0A, .integer = 7}}};
	EXPECT_EQ(load::ForcedPlanOf(sleeping), (load::KeptForcedPlan {.action = 17, .step = 2, .count = 4}));
	auto yawned = sleeping;
	yawned.current = 3;
	yawned.count = 5;
	EXPECT_EQ(load::ForcedPlanOf(yawned), (load::KeptForcedPlan {.action = 17, .step = 3, .count = 5}));
	auto walking = sleeping;
	walking.current = 0;
	walking.steps = {{.action = 8}, {.action = 0x44}, {.action = 0x84}, {.action = 0x0A}};
	EXPECT_EQ(load::ForcedPlanOf(walking), (load::KeptForcedPlan {.action = 17, .step = 0, .count = 4}));
	// sitting down at a thing, at the sit, held for the seconds the save keeps
	const gamesave::KeptCreaturePlan sitting {
	    .desire = 24, .action = 81, .current = 3, .count = 4, .steps = {{.action = 4, .integer = 9, .real = 12.5f}}};
	EXPECT_EQ(load::ForcedPlanOf(sitting), (load::KeptForcedPlan {.action = 81, .step = 3, .count = 4, .seconds = 12.5f}));
	auto standing = sitting;
	standing.steps.front().action = 0x44;
	EXPECT_FALSE(load::ForcedPlanOf(standing).has_value());
	auto oddSleep = sleeping;
	oddSleep.steps.front().action = 0x18;
	EXPECT_FALSE(load::ForcedPlanOf(oddSleep).has_value());
	auto other = looking;
	other.action = 81;
	EXPECT_FALSE(load::ForcedPlanOf(other).has_value());
	auto notObeying = looking;
	notObeying.desire = 22;
	EXPECT_FALSE(load::ForcedPlanOf(notObeying).has_value());
	auto shortDeath = dead;
	shortDeath.count = 1;
	shortDeath.steps.pop_back();
	EXPECT_FALSE(load::ForcedPlanOf(shortDeath).has_value());
}

// Integration test: needs the original game's data; skipped without it. Of the frozen saves' creatures, those the load
// makes: every creature of the Land 1 and Land 2 autosaves that hold creatures has a species, and at least one is made
TEST(SavedWorldLoadAnimals, TheFrozenAutosavesCreaturesHaveASpeciesAndSomeAreMade)
{
	size_t creatures = 0;
	size_t made = 0;
	for (const auto& save : {std::filesystem::path("_s_i_s_i") / "Saved Games" / "20" / "20.sav", k_LandTwoAutosave})
	{
		const auto bytes = FrozenSave(save);
		if (!bytes.has_value())
		{
			GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen saves with creatures";
		}
		const auto plan = load::PlanOf(gamesave::ReadWorld(*bytes));
		for (const auto& creature : plan.creatures)
		{
			++creatures;
			EXPECT_TRUE(creature.species.has_value()) << save << " creature " << creature.number;
			made += load::WhyCreatureNotMade(creature) == nullptr ? 1 : 0;
		}
	}
	EXPECT_EQ(creatures, 6u);
	EXPECT_GT(made, 0u);
	RecordProperty("made", static_cast<int>(made));
}

// A loaded creature's mind file: the profile's for the local player's creature, else the SavedCreature file of its place
// in the list of creatures, in its slot's folder; none for a creature of no place
TEST(SavedWorldLoadAnimals, ALoadedCreatureReadsTheProfilesMindOrItsSlotsSavedCreatureFile)
{
	const std::filesystem::path slot = std::filesystem::path("Profiles") / "p" / "Saved Games" / "16" / "2";
	const std::filesystem::path profile = std::filesystem::path("Scripts") / "CreatureMind" / "mine.erc";
	gamesave::CreatureFields mine;
	mine.owner = 0;
	mine.index = 1;
	EXPECT_EQ(load::CreatureMindFileOf(mine, slot, profile), profile);
	EXPECT_EQ(load::CreatureMindFileOf(mine, slot, std::nullopt), std::nullopt);
	gamesave::CreatureFields rival;
	rival.owner = 1;
	rival.index = 2;
	EXPECT_EQ(load::CreatureMindFileOf(rival, slot, profile), slot / "SavedCreature2");
	gamesave::CreatureFields stray;
	stray.owner = 0xFF;
	stray.index = 0xFF;
	EXPECT_EQ(load::CreatureMindFileOf(stray, slot, profile), std::nullopt);
}

// Integration test: needs the original game's data; skipped without it. Each creature of the frozen Land 2 autosave
// that is not the local player's names a SavedCreature file of its slot that is there and not empty
TEST(SavedWorldLoadAnimals, TheFrozenLandTwoAutosavesOtherCreaturesNameTheirSlotsMindFiles)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	const auto bytes = FrozenSave(k_LandTwoAutosave);
	if (game == nullptr || !bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 2 autosave";
	}
	const auto slot = (std::filesystem::path(game) / "Profiles" / k_LandTwoAutosave).parent_path();
	const auto plan = load::PlanOf(gamesave::ReadWorld(*bytes));
	size_t others = 0;
	for (const auto& creature : plan.creatures)
	{
		const auto& fields = creature.record.creature;
		if (fields.owner == 0)
		{
			continue;
		}
		const auto path = load::CreatureMindFileOf(fields, slot, std::nullopt);
		ASSERT_TRUE(path.has_value()) << "creature " << creature.number;
		std::error_code error;
		EXPECT_GT(std::filesystem::file_size(*path, error), 0u) << *path;
		++others;
	}
	EXPECT_GT(others, 0u);
}
