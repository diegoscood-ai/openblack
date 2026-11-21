/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The flocks and the animals and birds of a frozen saved game made as the game's load makes them, each animal with
// what its record says, and the file written back: every flock and animal by its writer, as its record keeps it but for its
// members' line, which the load puts back as the original's load does (docs/wiki/engine/saved-games.md#what-a-load-does)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <AnimalRecords.h>
#include <CreatureRecords.h>
#include <SavedRecords.h>
#include <VillageRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/Zoomer.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptMemory.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/SavedZoomer.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/WallHugBookkeeping.h"
#include "ECS/WallHugRules.h"
#include "GameClock.h"
#include "Locator.h"
#include "Magic/Spells/SpellFlock.h"
#include "support/RestoreService.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace load = openblack::ecs::saved_world_load;

namespace
{
class FlockLoadWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!spdlog::get("game"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
		RegisterFlockWriters(_writers);
		RegisterAnimalWriters(_writers);
		RegisterCreatureWriters(_writers);
		// the animals' and birds' models, which their places are drawn from
		for (const auto& row : Info().animal)
		{
			LoadMesh(row.std, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
			LoadMesh(row.high, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
		}
	}

	ClassWriters _writers;

private:
	test::RestoreService<Locator::forestSystem> _restoreForests;
};

std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	std::ifstream file(std::filesystem::path(game) / "Profiles" / relative, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), {});
}

/// The index of the first field two records differ in, in their FieldsOf order; -1 when they agree
template <typename Record>
int FirstDifferentField(const Record& written, const Record& saved)
{
	const auto ours = Record::FieldsOf(written);
	const auto theirs = Record::FieldsOf(saved);
	return [&]<size_t... I>(std::index_sequence<I...>) {
		int found = -1;
		static_cast<void>(((std::get<I>(ours) == std::get<I>(theirs) ? false : (found = static_cast<int>(I), true)) || ...));
		return found;
	}(std::make_index_sequence<std::tuple_size_v<decltype(ours)>> {});
}

/// The record with every object number it names taken from the other record: a write numbers the objects in the order
/// of their first references, which the flocks' reversed lines change, so only the values are compared
template <typename Record>
Record WithNumbersOf(Record record, const Record& numbers)
{
	record.object.fire = numbers.object.fire;
	if constexpr (requires { record.living.flock; })
	{
		auto& living = record.living;
		const auto& from = numbers.living;
		living.footpath = from.footpath;
		living.footpathNode = from.footpathNode;
		living.reaction = from.reaction;
		living.dataPath = from.dataPath;
		living.scriptReminder = from.scriptReminder;
		living.flock = from.flock;
		living.otherObject = from.otherObject;
		living.footpathAgain = from.footpathAgain;
		living.danceGroup = from.danceGroup;
		record.move.footpath = numbers.move.footpath;
		record.move.target = numbers.move.target;
		if (record.move.circle.has_value() && numbers.move.circle.has_value())
		{
			record.move.circle->object = numbers.move.circle->object;
		}
	}
	if constexpr (requires { record.animal.secondObject; })
	{
		record.animal.object = numbers.animal.object;
		record.animal.secondObject = numbers.animal.secondObject;
	}
	if constexpr (std::is_same_v<Record, gamesave::CreatureRecord>)
	{
		auto& creature = record.creature;
		const auto& from = numbers.creature;
		for (size_t i = 0; i < creature.spells.size(); ++i)
		{
			creature.spells[i].miracle = from.spells[i].miracle;
		}
		creature.queued = from.queued;
		creature.spellVisuals = from.spellVisuals;
		creature.handParticles = from.handParticles;
		creature.targetParticles = from.targetParticles;
		creature.targetObject = from.targetObject;
		creature.arena = from.arena;
		creature.dancePartners = from.dancePartners;
		creature.dance = from.dance;
		creature.plan = from.plan;
	}
	return record;
}

/// A member of a flock by its creation number, which a load and a save keep (its object number may change)
uint32_t CreationOf(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	uint32_t id = 0;
	if (number != 0 && number <= world.objects.size() && world.objects[number - 1].has_value())
	{
		std::visit(
		    [&id](const auto& record) {
			    if constexpr (requires { record.object.creationIndex; })
			    {
				    id = record.object.creationIndex;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*world.objects[number - 1]));
	}
	return id;
}

/// A flock's members by their creation numbers, in its record's order
std::vector<uint32_t> MembersByCreation(const gamesave::WorldRead& world, const gamesave::FlockRecord& flock)
{
	std::vector<uint32_t> members;
	for (const auto member : flock.members.numbers)
	{
		members.push_back(CreationOf(world, member));
	}
	return members;
}

/// A member's place in its flock's line, as its living part keeps it
uint8_t PlaceOf(const gamesave::WorldRead& world, gamesave::ObjectNumber number)
{
	uint8_t place = 0;
	if (number != 0 && number <= world.objects.size() && world.objects[number - 1].has_value())
	{
		std::visit(
		    [&place](const auto& record) {
			    if constexpr (requires { record.living.secondByte; })
			    {
				    place = record.living.secondByte;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*world.objects[number - 1]));
	}
	return place;
}

/// The flock of a world by its kept centre and its script id, which a load and a save keep
const gamesave::FlockRecord* FlockAt(const gamesave::WorldRead& world, const gamesave::MapCoords& centre, uint32_t id)
{
	for (const auto& object : world.objects)
	{
		const auto* flock = object.has_value() ? std::get_if<gamesave::FlockRecord>(&*object) : nullptr;
		if (flock != nullptr && flock->savedDomainCentre == centre && flock->lastWord == id)
		{
			return flock;
		}
	}
	return nullptr;
}

/// What a write back counted: of each class, how many records its writer wrote, and how many the load made
struct WriteBack
{
	std::map<uint32_t, size_t> byWriter;
	load::Census census;
};

/// A frozen save with its flocks, animals and birds made as the game's load makes them, every other object keeping its
/// record (the villagers are not made, so a flock with a villager in it is not made either), written back by the flocks',
/// the players' and the game object's writers; each animal made checked against its record
WriteBack WriteBackWithFlocks(ClassWriters& writers, const std::vector<uint8_t>& bytes)
{
	const auto world = gamesave::ReadWorld(bytes);
	EXPECT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	if (!state.has_value())
	{
		ADD_FAILURE() << "no game state";
		return {};
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto keeping = [&registry]<typename Record>(const Record& record) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::LoadedRecord<Record>>(entity, record);
		return entity;
	};
	const auto keepingOne = [&keeping](const auto& variant) {
		return std::visit([&keeping](const auto& record) { return keeping(record); }, variant);
	};
	const load::Makers makers {
	    .footpath = [&](const load::PlannedFootpath& footpath) { return keeping(footpath.record); },
	    .footpathLink = [&](const load::PlannedFootpathLink& link,
	                        const std::vector<entt::entity>& /*footpaths*/) { return keeping(link.record); },
	    .town = [&](const load::PlannedTown& town) { return keeping(town.record); },
	    .tree = [&](const load::PlannedTree& tree, uint32_t /*forest*/) { return keeping(tree.record); },
	    .bigForest = [&](const load::PlannedBigForest& big, uint32_t /*forest*/) { return keeping(big.record); },
	    .pile = [&](const load::PlannedPile& pile) { return keepingOne(pile.record); },
	    .storagePit = [&](const load::PlannedStoragePit& pit, uint32_t /*town*/,
	                      const ecs::archetypes::AbodeArchetype::StoragePitPiles& /*piles*/) { return keeping(pit.record); },
	    .citadelHeart =
	        [&](const load::PlannedCitadelHeart& heart, entt::entity citadel) {
		        const auto entity = keeping(heart.record);
		        // a heart, for the flock of its doves or bats to be given to, as the game's heart maker makes one
		        registry.Assign<ecs::components::CitadelHeart>(entity);
		        if (citadel == entt::null)
		        {
			        registry.Assign<ecs::components::LoadedRecord<gamesave::CitadelRecord>>(entity, heart.citadel);
		        }
		        return entity;
	        },
	    .abode = [&](const load::PlannedAbode& abode, uint32_t /*town*/) { return keepingOne(abode.record); },
	    .desireFlag = [&](const load::PlannedDesireFlag& flag, entt::entity /*town*/) { return keeping(flag.record); },
	    .mobileStatic = [&](const load::PlannedMobileStatic& object) { return keepingOne(object.record); },
	    .climates = [](const std::vector<load::PlannedClimate>& climates) { return climates.size(); },
	    .flock = load::MakeFlockEntity,
	    .animal = load::MakeAnimalEntity,
	    // the creatures stay as the save has them: making one needs the game's services, which this test does not set up
	};
	LoadedObjects loaded;
	WriteBack result;
	// the resolves that draw (the animals' clips) are the game's after its seeds are back: they write nothing
	std::vector<entt::entity> resolves;
	result.census = load::Apply(load::PlanOf(world), makers, &loaded, &resolves);
	ecs::object_index::Restore(state->createdObjects);
	// a load clears every object's draw-list mark and the next drawn frame lists the objects in view again; this test
	// draws no frame, so the animals the save had listed are listed as that frame would
	for (const auto& [number, object] : loaded)
	{
		if (!registry.AllOf<ecs::components::Animal>(object.entity))
		{
			continue;
		}
		std::visit(
		    [&registry, &object](const auto& record) {
			    if constexpr (requires { record.animal; })
			    {
				    if ((record.position.flags & position_flags::k_DrawListed) != 0)
				    {
					    registry.AssignOrReplace<ecs::components::DrawListed>(object.entity);
				    }
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*world.objects[number - 1]));
	}

	// each animal made holds what its record says
	for (const auto& [number, object] : loaded)
	{
		const auto* animal = registry.TryGet<const ecs::components::Animal>(object.entity);
		if (animal == nullptr || number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			continue;
		}
		std::visit(
		    [&](const auto& record) {
			    if constexpr (requires { record.animal; })
			    {
				    const auto& brain = registry.Get<const ecs::components::AnimalBrain>(object.entity);
				    EXPECT_EQ(brain.topState, record.living.action[0]) << "animal " << number;
				    EXPECT_EQ(brain.finalState, record.living.action[1]) << "animal " << number;
				    EXPECT_EQ(brain.counter, record.move.turnsToStateChange) << "animal " << number;
				    EXPECT_EQ(brain.speed, record.move.speed) << "animal " << number;
				    EXPECT_EQ(brain.angle, record.move.angle) << "animal " << number;
				    EXPECT_EQ(brain.hunger, record.animal.hunger) << "animal " << number;
				    EXPECT_EQ(brain.birthTurn, std::bit_cast<int32_t>(record.living.birthTurn)) << "animal " << number;
				    EXPECT_EQ(animal->flockOrder, record.living.secondByte) << "animal " << number;
				    EXPECT_EQ(registry.Get<const ecs::components::ObjectCreationIndex>(object.entity).value,
				              record.object.creationIndex)
				        << "animal " << number;
				    const auto flock = loaded.find(record.living.flock.number);
				    EXPECT_EQ(animal->flock, flock != loaded.end() ? flock->second.entity : entt::entity {entt::null})
				        << "animal " << number;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*world.objects[number - 1]));
	}

	// the players, kept as the players' source by their numbers, written with the values their records hold
	std::map<PlayerNames, const gamesave::PlayerRecord*> players;
	for (const auto& [number, record] : ecs::saved_players::ListPlayers(world))
	{
		loaded.insert_or_assign(
		    number, LoadedObject {.entity = entt::null, .type = gamesave::k_PlayerType, .id = record->playerNumber});
		players.emplace(static_cast<PlayerNames>(record->playerNumber), record);
	}
	std::map<gamesave::ObjectNumber, entt::entity> entityOf;
	for (const auto& [number, object] : loaded)
	{
		entityOf.emplace(number, object.entity);
	}
	RegisterPlayerWriters(writers,
	                      {.values = [&players](PlayerNames player) -> std::optional<ecs::saved_players::PlayerValues> {
		                       return ecs::saved_players::ValuesOf(*players.at(player));
	                       },
	                       .objects =
	                           [&players, entityOf](const ecs::Registry& /*registry*/, PlayerNames player) {
		                           const auto at = [&entityOf](gamesave::ObjectNumber number) {
			                           const auto found = entityOf.find(number);
			                           return found != entityOf.end() ? found->second : entt::entity {entt::null};
		                           };
		                           ecs::saved_players::PlayerObjects objects {.citadel = at(players.at(player)->citadel)};
		                           for (const auto town : players.at(player)->towns)
		                           {
			                           objects.towns.push_back(at(town));
		                           }
		                           return objects;
	                           },
	                       .newRecord = {}});
	RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	const auto written =
	    WriteWorld(writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	EXPECT_TRUE(written.Complete()) << written.failure;
	for (const auto& record : written.records)
	{
		if (record.source == RecordSource::Writer)
		{
			++result.byWriter[record.type];
		}
	}
	// every flock written back as its record keeps it, but its members in the line the load put them in: the
	// original's load and save does the same (OriginalsResaveReversesTheFlocksTies)
	EXPECT_EQ(written.bytes.size(), bytes.size());
	const auto again = gamesave::ReadWorld(written.bytes);
	EXPECT_EQ(again.stop, gamesave::k_ReadToTheEnd);
	size_t flocksCompared = 0;
	for (const auto& [number, object] : loaded)
	{
		// the flocks the load made; the others are written as they were loaded
		if (!registry.AllOf<ecs::components::Flock>(object.entity) || number == 0 || number > world.objects.size() ||
		    !world.objects[number - 1].has_value())
		{
			continue;
		}
		const auto* kept = std::get_if<gamesave::FlockRecord>(&*world.objects[number - 1]);
		if (kept == nullptr)
		{
			continue;
		}
		const auto* made = FlockAt(again, kept->savedDomainCentre, kept->lastWord);
		if (made == nullptr)
		{
			continue;
		}
		++flocksCompared;
		// the running sum in its thing part is the file writer's
		auto expected = *kept;
		expected.thing = made->thing;
		expected.members = made->members;
		EXPECT_EQ(FirstDifferentField(*made, expected), -1) << "a flock's values, flock " << number;
		const auto keptMembers = MembersByCreation(world, *kept);
		std::vector<uint8_t> orders;
		for (const auto member : kept->members.numbers)
		{
			orders.push_back(PlaceOf(world, member));
		}
		std::vector<uint32_t> line;
		for (const auto index : load::LoadedFlockLine(orders))
		{
			line.push_back(keptMembers[index]);
		}
		EXPECT_EQ(MembersByCreation(again, *made), line) << "a flock's members";
	}
	EXPECT_GT(flocksCompared, 0u);
	// every animal the load made written by its class's writer as its record keeps it, found by its creation number
	std::map<std::pair<uint32_t, uint32_t>, gamesave::ObjectNumber> writtenByCreation;
	for (size_t n = 0; n < again.objects.size(); ++n)
	{
		if (again.objects[n].has_value())
		{
			writtenByCreation.emplace(std::pair {gamesave::TypeOf(*again.objects[n]), CreationOf(again, n + 1)}, n + 1);
		}
	}
	size_t animalsCompared = 0;
	for (const auto& [number, object] : loaded)
	{
		if (!registry.AnyOf<ecs::components::Animal, ecs::components::Creature>(object.entity))
		{
			continue;
		}
		const auto& kept = *world.objects[number - 1];
		const auto found = writtenByCreation.find({gamesave::TypeOf(kept), CreationOf(world, number)});
		if (found == writtenByCreation.end())
		{
			ADD_FAILURE() << "animal or creature " << number << " is not in the file written";
			continue;
		}
		std::visit(
		    [&](const auto& saved) {
			    using Record = std::decay_t<decltype(saved)>;
			    if constexpr (requires { saved.animal; } || std::is_same_v<Record, gamesave::CreatureRecord>)
			    {
				    const auto& writtenRecord = std::get<Record>(*again.objects[found->second - 1]);
				    // an animal's tilt is cleared as its load makes it, as the original's resolve clears it
				    auto expected = WithNumbersOf(saved, writtenRecord);
				    if constexpr (requires { expected.animal; })
				    {
					    expected.animal.angleZoomer = {};
				    }
				    EXPECT_EQ(FirstDifferentField(writtenRecord, expected), -1) << "animal " << number;
				    ++animalsCompared;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(kept));
	}
	EXPECT_GT(animalsCompared, 0u);
	EXPECT_EQ(animalsCompared, static_cast<size_t>(std::ranges::count_if(written.records, [](const WrittenRecord& record) {
		          return record.source == RecordSource::Writer && record.entity != entt::null &&
		                 Locator::entitiesRegistry::value().AnyOf<ecs::components::Animal, ecs::components::Creature>(
		                     record.entity);
	          })));
	return result;
}

uint32_t MadeOf(const load::Census& census, uint32_t type)
{
	const auto line = std::ranges::find(census, type, &load::CensusLine::type);
	return line != census.end() ? line->made : 0;
}
} // namespace

// An animal made in play is written in its kind's class, from its brain and components, and as its constructor leaves
// what openblack does not hold: the word, the food place, the thing it pounced on and the three places 0
TEST_P(FlockLoadWorld, AnAnimalMadeInPlayIsWrittenInItsKindsClassAsItsConstructorLeavesTheRest)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, glm::vec3(500.0f, 0.0f, 600.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Animal>(entity, AnimalInfo::Sheep, 3u, entt::entity {entt::null}, true);
	auto& brain = registry.Assign<ecs::components::AnimalBrain>(entity);
	brain.hunger = 12;
	brain.sleep = 34;
	brain.breed = 56;
	brain.sleepCell = {7, 9};
	brain.birthTurn = -4500;
	brain.bank.value = 0.25f;
	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto made = context.RecordOf(context.Reference(entity));
	ASSERT_NE(made.record, nullptr) << context.Failure();
	const auto* record = std::get_if<gamesave::SheepRecord>(made.record.get());
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->object.info, 2548u - static_cast<uint32_t>(AnimalInfo::Sheep));
	EXPECT_EQ(record->animal.hunger, 12);
	EXPECT_EQ(record->animal.tiredness, 34);
	EXPECT_EQ(record->animal.breedingUrge, 56);
	EXPECT_EQ(record->animal.sleepPosition, 7u | (9u << 16U));
	EXPECT_EQ(record->animal.half, 0);
	EXPECT_EQ(record->animal.foodPosition, (std::array<int32_t, 2> {}));
	EXPECT_EQ(record->animal.secondObject.number, 0u);
	EXPECT_EQ(record->animal.physicalObjectPosition, 0u);
	EXPECT_EQ(record->animal.environmentPosition, 0u);
	EXPECT_EQ(record->living.birthTurn, std::bit_cast<uint32_t>(int32_t {-4500}));
	EXPECT_NE(record->animal.angleZoomer, (std::array<uint8_t, 48> {}));
}

// A spell's dove is written in its own class with its fade; a spell's wolf with its final destination, its corridor as
// the game's plane (its normal across the run, 0 for its height, its offset), the corridor's half width, its fade and
// its player
TEST_P(FlockLoadWorld, ASpellsDoveAndWolfAreWrittenWithTheirFadesTheWolfWithItsRunAndPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto made = [&registry](AnimalInfo type) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Transform>(entity, glm::vec3(500.0f, 0.0f, 600.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::Animal>(entity, type, 0u, entt::entity {entt::null}, true);
		registry.Assign<ecs::components::AnimalBrain>(entity);
		auto& spell = registry.Assign<magic::SpellFlockAnimal>(entity);
		spell.fade.value = 128.0f;
		spell.fade.destination = 0.0f;
		return entity;
	};
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_ONE;
	const auto dove = made(AnimalInfo::SpellDove);
	const auto wolf = made(AnimalInfo::SpellWolf);
	registry.Get<ecs::components::Animal>(wolf).player = 0;
	auto& run = registry.Get<magic::SpellFlockAnimal>(wolf);
	run.normal = {0.6f, -0.8f};
	run.offset = 12.5f;
	run.halfWidth = 45.0f;
	run.finalDestination = {.x = 65536, .z = 131072, .altitude = 0.0f};
	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto bird = context.RecordOf(context.Reference(dove));
	ASSERT_NE(bird.record, nullptr) << context.Failure();
	const auto* doveRecord = std::get_if<gamesave::SpellDoveRecord>(bird.record.get());
	ASSERT_NE(doveRecord, nullptr);
	EXPECT_EQ(ecs::saved_zoomer::ZoomerOf(doveRecord->spell).value, 128.0f);
	const auto runner = context.RecordOf(context.Reference(wolf));
	ASSERT_NE(runner.record, nullptr) << context.Failure();
	const auto* wolfRecord = std::get_if<gamesave::SpellWolfRecord>(runner.record.get());
	ASSERT_NE(wolfRecord, nullptr);
	EXPECT_EQ(wolfRecord->finalDestination, (gamesave::MapCoords {.x = 65536, .z = 131072, .altitude = 0.0f}));
	EXPECT_EQ((std::bit_cast<std::array<float, 4>>(wolfRecord->corridor)), (std::array<float, 4> {0.6f, 0.0f, -0.8f, 12.5f}));
	EXPECT_EQ(wolfRecord->halfWidth, 45.0f);
	EXPECT_EQ(ecs::saved_zoomer::ZoomerOf(wolfRecord->fade).value, 128.0f);
	EXPECT_EQ(wolfRecord->player.number, PlayerReference(context, PlayerNames::PLAYER_ONE));
	EXPECT_NE(wolfRecord->player.number, 0u);
}

// A spell's wolf of a save is made again with its fade as saved, its final destination, its corridor, its half width
// and its player, its brain's final destination the same; written back, its spell's parts are its record's
TEST_P(FlockLoadWorld, ASpellsWolfIsMadeAgainWithItsFadeItsRunAndItsPlayerAndWrittenBackTheSame)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player).name = PlayerNames::PLAYER_TWO;
	gamesave::SpellWolfRecord record;
	record.position.position = {.x = map_coords::ToFixed(500.0f), .z = map_coords::ToFixed(600.0f), .altitude = 0.0f};
	record.object.info = 2548u - static_cast<uint32_t>(AnimalInfo::SpellWolf);
	record.object.coords = record.position.position;
	const auto wander = static_cast<uint8_t>(ecs::animal_ai::AnimalState::Wander);
	record.living.action = {wander, wander, wander, 0, 0, 0};
	record.finalDestination = {.x = map_coords::ToFixed(520.0f), .z = map_coords::ToFixed(640.0f), .altitude = 0.0f};
	record.corridor = std::bit_cast<std::array<uint8_t, 16>>(std::array<float, 4> {0.6f, 0.0f, -0.8f, 12.5f});
	record.halfWidth = 45.0f;
	Zoomer fade;
	fade.SetPosition(255.0f);
	fade.value = 200.0f;
	record.fade = ecs::saved_zoomer::BytesOf(fade);
	record.player = {9};
	const load::PlannedAnimal planned {.number = 1,
	                                   .recordType = gamesave::SpellWolfRecord::k_Type,
	                                   .type = AnimalInfo::SpellWolf,
	                                   .age = 0,
	                                   .thing = record.thing,
	                                   .position = record.position,
	                                   .object = record.object,
	                                   .mobile = record.mobile,
	                                   .move = record.move,
	                                   .living = record.living,
	                                   .animal = record.animal,
	                                   .record = record};
	const auto entity = load::MakeAnimalEntity(
	    planned, [player](gamesave::ObjectNumber number) { return number == 9 ? player : entt::entity {entt::null}; });
	ASSERT_TRUE(entity != entt::null);
	const auto& data = registry.Get<const magic::SpellFlockAnimal>(entity);
	EXPECT_TRUE(data.wolf);
	EXPECT_EQ(data.fade.value, 200.0f);
	EXPECT_EQ(data.finalDestination.x, record.finalDestination.x);
	EXPECT_EQ(data.destination,
	          glm::vec2(map_coords::ToMetres(record.finalDestination.x), map_coords::ToMetres(record.finalDestination.z)));
	EXPECT_EQ(data.normal, glm::vec2(0.6f, -0.8f));
	EXPECT_EQ(data.offset, 12.5f);
	EXPECT_EQ(data.halfWidth, 45.0f);
	EXPECT_EQ(registry.Get<const ecs::components::AnimalBrain>(entity).finalDestination, data.destination);
	EXPECT_EQ(registry.Get<const ecs::components::Animal>(entity).player, static_cast<int32_t>(PlayerNames::PLAYER_TWO));

	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto again = context.RecordOf(context.Reference(entity));
	ASSERT_NE(again.record, nullptr) << context.Failure();
	const auto* written = std::get_if<gamesave::SpellWolfRecord>(again.record.get());
	ASSERT_NE(written, nullptr);
	EXPECT_EQ(written->finalDestination, record.finalDestination);
	EXPECT_EQ(written->corridor, record.corridor);
	EXPECT_EQ(written->halfWidth, record.halfWidth);
	EXPECT_EQ(written->fade, record.fade);
	EXPECT_EQ(written->player.number, PlayerReference(context, PlayerNames::PLAYER_TWO));
}

// An animal that has had no turn yet (a save as the game starts, before the first turn) is written with the values it was
// made with, as the original's holds them from its making: its needs 0, the birth turn of its age, DECIDE_WHAT_TO_DO,
// its row's speed and life; the write makes it no brain
TEST_P(FlockLoadWorld, AnAnimalThatHasHadNoTurnYetIsWrittenWithTheValuesItWasMadeWith)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<ecs::components::Transform>(entity, glm::vec3(500.0f, 0.0f, 600.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<ecs::components::Animal>(entity, AnimalInfo::Sheep, 3u, entt::entity {entt::null}, true);
	const auto made = ecs::animal_ai::BrainAsMade(entity);
	EXPECT_EQ(made.topState, static_cast<uint8_t>(ecs::animal_ai::AnimalState::DecideWhatToDo));
	EXPECT_EQ(made.hunger, 0);
	EXPECT_EQ(made.birthTurn, static_cast<int32_t>(game_clock::Turn()) - 3 * 1500);
	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(_writers, source);
	const auto written = context.RecordOf(context.Reference(entity));
	ASSERT_NE(written.record, nullptr) << context.Failure();
	const auto* record = std::get_if<gamesave::SheepRecord>(written.record.get());
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->animal.hunger, 0);
	EXPECT_EQ(record->animal.tiredness, 0);
	EXPECT_EQ(record->animal.breedingUrge, 0);
	EXPECT_EQ(record->living.action[0], static_cast<uint8_t>(ecs::animal_ai::AnimalState::DecideWhatToDo));
	EXPECT_EQ(record->living.birthTurn, std::bit_cast<uint32_t>(made.birthTurn));
	EXPECT_EQ(record->move.speed, made.speed);
	EXPECT_EQ(record->object.life, Info().animal.at(static_cast<size_t>(AnimalInfo::Sheep)).life);
	EXPECT_FALSE(registry.AllOf<ecs::components::AnimalBrain>(entity));
}

// A predator made without a flock heads for its lair from its making, as the original's making sets its destination to
// its new flock's centre: a save before its first turn writes that goal; one in a flock of others is given none
TEST_P(FlockLoadWorld, ALonePredatorIsMadeHeadingForItsLair)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto lion = registry.Create();
	registry.Assign<ecs::components::Transform>(lion, glm::vec3(500.0f, 0.0f, 600.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto flock = registry.Create();
	auto& own = registry.Assign<ecs::components::Flock>(flock);
	own.domainCentre = glm::vec3(520.0f, 0.0f, 640.0f);
	own.members = {lion};
	registry.Assign<ecs::components::Animal>(lion, AnimalInfo::Lion, 3u, entt::entity {entt::null}, true).flock = flock;
	EXPECT_EQ(ecs::animal_ai::BrainAsMade(lion).goal, glm::vec2(520.0f, 640.0f));
	// a sheep in the same kind of flock is a grazer: no goal from its making
	const auto sheep = registry.Create();
	registry.Assign<ecs::components::Transform>(sheep, glm::vec3(500.0f, 0.0f, 600.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto herd = registry.Create();
	auto& its = registry.Assign<ecs::components::Flock>(herd);
	its.domainCentre = glm::vec3(520.0f, 0.0f, 640.0f);
	its.members = {sheep};
	registry.Assign<ecs::components::Animal>(sheep, AnimalInfo::Sheep, 3u, entt::entity {entt::null}, true).flock = herd;
	EXPECT_EQ(ecs::animal_ai::BrainAsMade(sheep).goal, glm::vec2(0.0f));
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave with its flocks, animals and birds made: every flock written by its writer as its record keeps it, its
// members in the line the load put them in, and the file as long as the original's
TEST_P(FlockLoadWorld, TheFrozenLandOneAutosaveWithItsFlocksAndAnimalsMadeWritesBackByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 1 autosave is the game's data";
	}
	const auto result = WriteBackWithFlocks(_writers, *bytes);
	const auto flocks = MadeOf(result.census, gamesave::FlockRecord::k_Type);
	EXPECT_GT(flocks, 0u);
	EXPECT_EQ(result.byWriter.contains(gamesave::FlockRecord::k_Type) ? result.byWriter.at(gamesave::FlockRecord::k_Type) : 0,
	          flocks);
	EXPECT_GT(MadeOf(result.census, gamesave::DoveRecord::k_Type), 0u);
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 2 autosave, the same way
TEST_P(FlockLoadWorld, TheFrozenLandTwoAutosaveWithItsFlocksAndAnimalsMadeWritesBackByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen Land 2 autosave is the game's data";
	}
	const auto result = WriteBackWithFlocks(_writers, *bytes);
	const auto flocks = MadeOf(result.census, gamesave::FlockRecord::k_Type);
	EXPECT_GT(flocks, 0u);
	EXPECT_EQ(result.byWriter.contains(gamesave::FlockRecord::k_Type) ? result.byWriter.at(gamesave::FlockRecord::k_Type) : 0,
	          flocks);
	EXPECT_GT(MadeOf(result.census, gamesave::SheepRecord::k_Type), 0u);
}

namespace
{
/// The move states of the walk round a circle that a save keeps: the straight look ahead and the orbit clockwise
constexpr uint8_t k_LinearState = 0x0C;
constexpr uint8_t k_OrbitState = 0x10;

/// A sheep of a save in a walk as its record keeps it, with the reminder a script left it (none for 0), naming no other
/// object
load::PlannedAnimal SavedSheep(uint8_t moveState, const std::optional<gamesave::CircleHug>& circle,
                               gamesave::ObjectNumber reminder = 0)
{
	gamesave::SheepRecord record;
	record.position.position = {.x = map_coords::ToFixed(500.0f), .z = map_coords::ToFixed(600.0f), .altitude = 0.0f};
	record.object.info = 2548u - static_cast<uint32_t>(AnimalInfo::Sheep);
	record.object.coords = record.position.position;
	record.move.moveState = moveState;
	record.move.circle = circle;
	const auto wander = static_cast<uint8_t>(ecs::animal_ai::AnimalState::Wander);
	record.living.action = {wander, wander, wander, 0, 0, 0};
	record.living.scriptReminder.number = reminder;
	return load::PlannedAnimal {.number = 1,
	                            .recordType = gamesave::SheepRecord::k_Type,
	                            .type = AnimalInfo::Sheep,
	                            .age = 3,
	                            .thing = record.thing,
	                            .position = record.position,
	                            .object = record.object,
	                            .mobile = record.mobile,
	                            .move = record.move,
	                            .living = record.living,
	                            .animal = record.animal,
	                            .record = record};
}

/// A sheep of a save walking round a circle as its record keeps it, naming no other object
load::PlannedAnimal CirclingSheep(uint8_t moveState, const gamesave::CircleHug& circle)
{
	return SavedSheep(moveState, circle);
}

entt::entity NoObject(gamesave::ObjectNumber /*number*/)
{
	return entt::null;
}
} // namespace

// A loaded animal walking round a circle is made in that walk, as the original's load reads it: its turns to the circle
// and its entry distance, and the circle it holds, none, or water or the land's edge in the middle of the cell its
// saved centre is in; written back, its walk is its record's
TEST_P(FlockLoadWorld, AnAnimalWalkingRoundACircleIsMadeInItsWalkAndWrittenBackAsItsRecordKeepsIt)
{
	const std::array cases {
	    // holding no circle: no object, the index 0 and no place, with its own turns and entry distance
	    std::pair {k_LinearState, gamesave::CircleHug {.turnsToObject = 0xFF, .entryDistance = 894}},
	    // water or the land's edge: the index 1 and the middle of its cell
	    std::pair {k_OrbitState, gamesave::CircleHug {.turnsToObject = 19,
	                                                  .entryDistance = 1322,
	                                                  .index = 1,
	                                                  .coords = {.x = 0xCB8000, .z = 0x1108000, .altitude = 0.0f}}},
	};
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto& [state, circle] : cases)
	{
		const auto planned = CirclingSheep(state, circle);
		const auto entity = load::MakeAnimalEntity(planned, NoObject);
		ASSERT_TRUE(entity != entt::null) << "move state " << static_cast<int>(state);
		const auto& brain = registry.Get<const ecs::components::AnimalBrain>(entity);
		EXPECT_EQ(brain.moveState, state);
		EXPECT_EQ(brain.turnsToObj, circle.turnsToObject);
		EXPECT_EQ(brain.hugGoalDistance, circle.entryDistance);
		const bool water = circle.index == 1;
		EXPECT_EQ(brain.hugCircle.set, water);
		if (water)
		{
			EXPECT_EQ(brain.hugCircle.owner, entt::entity {entt::null});
			EXPECT_EQ(brain.hugCircle.centre, ecs::wall_hug::LandscapeCircleCentre({0xCB, 0x110}));
			EXPECT_EQ(brain.hugCircle.radius, ecs::wall_hug::k_LandscapeBlockerRadius);
		}
		const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
		WriteContext context(_writers, source);
		const auto written = context.RecordOf(context.Reference(entity));
		ASSERT_NE(written.record, nullptr) << context.Failure();
		const auto* record = std::get_if<gamesave::SheepRecord>(written.record.get());
		ASSERT_NE(record, nullptr);
		EXPECT_EQ(record->move, planned.move) << "move state " << static_cast<int>(state);
	}
}

// A loaded animal going round a thing the load has not made is not made: its walk would name a thing that is not there
TEST_P(FlockLoadWorld, AnAnimalWalkingRoundAThingNotMadeIsNotMade)
{
	const auto planned =
	    CirclingSheep(k_LinearState, gamesave::CircleHug {.turnsToObject = 24, .entryDistance = 0, .object = {7}, .index = 7});
	EXPECT_EQ(load::MakeAnimalEntity(planned, NoObject), entt::entity {entt::null});
}

// Integration test: needs the original game's data; skipped without it. The frozen save of _J_a_h_o_v_i_a's slot 17
// (its third), whose 16 animals walking round water, the land's edge or no circle the load refused before: each now
// made in its walk, written back as its record keeps it
TEST_P(FlockLoadWorld, TheFrozenSavesAnimalsWalkingRoundWaterOrNoCircleAreMadeInTheirWalk)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "17" / "3" / "17.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen save is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	auto& registry = Locator::entitiesRegistry::value();
	size_t made = 0;
	for (const auto& planned : load::PlanOf(world).animals)
	{
		if (!planned.move.circle.has_value() || planned.move.circle->object.number != 0)
		{
			continue;
		}
		const auto entity = load::MakeAnimalEntity(planned, NoObject);
		if (entity == entt::null)
		{
			continue;
		}
		++made;
		const auto& brain = registry.Get<const ecs::components::AnimalBrain>(entity);
		EXPECT_EQ(brain.turnsToObj, planned.move.circle->turnsToObject) << "animal " << planned.number;
		EXPECT_EQ(brain.hugGoalDistance, planned.move.circle->entryDistance) << "animal " << planned.number;
		EXPECT_EQ(brain.hugCircle.set, planned.move.circle->index == 1) << "animal " << planned.number;
		const WorldSource source {.registry = registry, .landscape = world.framing.landscape};
		WriteContext context(_writers, source);
		const auto written = context.RecordOf(context.Reference(entity));
		ASSERT_NE(written.record, nullptr) << context.Failure();
		std::visit(
		    [&planned](const auto& record) {
			    if constexpr (requires { record.animal; })
			    {
				    // the things it names are numbered by the write; only its walk's values are compared
				    auto move = record.move;
				    move.footpath = planned.move.footpath;
				    move.target = planned.move.target;
				    EXPECT_EQ(move, planned.move) << "animal " << planned.number;
			    }
		    },
		    static_cast<const gamesave::SavedRecordVariant&>(*written.record));
	}
	EXPECT_GE(made, 16u);
}

// A loaded animal a script left a reminder on is made with it, kept as the original keeps it (nothing but a save reads
// an animal's): written again, its living part names the reminder and the reminder holds what its record held
TEST_P(FlockLoadWorld, AnAnimalsScriptReminderIsKeptAndWrittenAgain)
{
	gamesave::ScriptReminderRecord reminder;
	const auto put = [](auto& bytes, size_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, sizeof(value)); };
	put(reminder.firstValue, 0, std::bit_cast<uint32_t>(map_coords::ToFixed(510.0f)));
	put(reminder.firstValue, sizeof(uint32_t), std::bit_cast<uint32_t>(map_coords::ToFixed(620.0f)));
	put(reminder.firstValue, 2 * sizeof(uint32_t), std::bit_cast<uint32_t>(2.5f));
	// no script's place, number or pair (an animal's are never kept), the clip its state last put on and the state to
	// take up, IN_SCRIPT
	reminder.words = {0, 0, 0, 17, static_cast<uint32_t>(ecs::animal_ai::AnimalState::InScript)};
	constexpr gamesave::ObjectNumber k_Reminder = 2;
	load::WorldPlan plan;
	plan.animals.push_back(SavedSheep(0, std::nullopt, k_Reminder));
	plan.scriptReminders.emplace(k_Reminder, reminder);
	LoadedObjects loaded;
	static_cast<void>(load::Apply(plan, load::Makers {.animal = load::MakeAnimalEntity}, &loaded));
	ASSERT_TRUE(loaded.contains(1));
	const auto entity = loaded.at(1).entity;
	auto& registry = Locator::entitiesRegistry::value();
	ASSERT_TRUE(registry.AllOf<ecs::components::ScriptMemory>(entity));
	ClassWriters writers;
	RegisterAnimalWriters(writers);
	RegisterVillagerWriters(writers);
	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(writers, source);
	const auto written = context.RecordOf(context.Reference(entity));
	ASSERT_NE(written.record, nullptr) << context.Failure();
	const auto* sheep = std::get_if<gamesave::SheepRecord>(written.record.get());
	ASSERT_NE(sheep, nullptr);
	ASSERT_NE(sheep->living.scriptReminder.number, 0u);
	const auto again = context.RecordOf(sheep->living.scriptReminder.number);
	ASSERT_NE(again.record, nullptr) << context.Failure();
	const auto* writtenReminder = std::get_if<gamesave::ScriptReminderRecord>(again.record.get());
	ASSERT_NE(writtenReminder, nullptr);
	EXPECT_EQ(gamesave::ScriptReminderRecord::FieldsOf(*writtenReminder), gamesave::ScriptReminderRecord::FieldsOf(reminder));
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 1 save of _s_i_s_i's slot 20,
// whose doves and sheep a script left a reminder on: each such animal is made, its reminder planned
TEST_P(FlockLoadWorld, TheFrozenSavesAnimalsWithAScriptsReminderAreMade)
{
	const auto bytes = FrozenSave(std::filesystem::path("_s_i_s_i") / "Saved Games" / "20" / "20.sav");
	if (!GameData() || !bytes.has_value())
	{
		GTEST_SKIP() << "the frozen save is the game's data";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto plan = load::PlanOf(world);
	size_t made = 0;
	for (const auto& planned : plan.animals)
	{
		const auto number = planned.living.scriptReminder.number;
		if (number == 0)
		{
			continue;
		}
		EXPECT_TRUE(plan.scriptReminders.contains(number)) << "animal " << planned.number;
		made += load::MakeAnimalEntity(planned, NoObject) != entt::null ? 1 : 0;
	}
	EXPECT_GT(made, 0u);
}

// A loaded animal the physics moved at the save (thrown or falling) is made and goes back into the physics with the
// body its record keeps, flying, as the original's load puts it back; written again, its place is in the physics and its
// body is its record's. One a particle system carries keeps no body and is still not made
TEST_P(FlockLoadWorld, AnAnimalInThePhysicsIsMadeInItWithItsBodyAndWrittenBackWithIt)
{
	auto sheep =
	    std::get<gamesave::SheepRecord>(static_cast<const gamesave::SavedRecordVariant&>(SavedSheep(0, std::nullopt).record));
	const auto flying = static_cast<uint8_t>(ecs::animal_ai::AnimalState::Flying);
	sheep.living.action = {flying, flying, flying, 0, 0, 0};
	sheep.position.flags = gamesave::k_PositionFlagInPhysics;
	sheep.object.physics = gamesave::PhysicsPart {
	    .matrix = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 500.0f, 30.0f, 600.0f},
	    .velocity = {1.0f, -2.0f, 3.0f},
	    .spin = {0.0f, 0.5f, 0.0f},
	};
	gamesave::WorldRead world;
	world.objects.resize(2);
	world.objects[0] = gamesave::SavedRecord(sheep);
	auto carried = sheep;
	carried.thing.flags = gamesave::k_ThingFlagCarriedByParticles;
	carried.object.physics.reset();
	world.objects[1] = gamesave::SavedRecord(carried);
	world.loadOrder = {1, 2};
	const auto plan = load::PlanOf(world);
	ASSERT_EQ(plan.animals.size(), 2u);
	ASSERT_TRUE(plan.inPhysics.contains(1));
	EXPECT_FALSE(plan.inPhysics.contains(2));
	EXPECT_EQ(load::MakeAnimalEntity(plan.animals[1], NoObject), entt::entity {entt::null});

	LoadedObjects loaded;
	static_cast<void>(load::Apply(plan, load::Makers {.animal = load::MakeAnimalEntity}, &loaded));
	ASSERT_TRUE(loaded.contains(1));
	EXPECT_FALSE(loaded.contains(2));
	const auto entity = loaded.at(1).entity;
	const auto* entry = Dynamics().Find(entity);
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->body.velocity, glm::vec3(1.0f, -2.0f, 3.0f));
	EXPECT_EQ(entry->body.angularMomentum, glm::vec3(0.0f, 0.5f, 0.0f));
	auto& registry = Locator::entitiesRegistry::value();
	EXPECT_EQ(registry.Get<const ecs::components::AnimalBrain>(entity).topState, flying);

	ClassWriters writers;
	RegisterAnimalWriters(writers);
	RegisterVillagerWriters(writers);
	const WorldSource source {.registry = registry, .landscape = "Land1.lnd"};
	WriteContext context(writers, source);
	const auto written = context.RecordOf(context.Reference(entity));
	ASSERT_NE(written.record, nullptr) << context.Failure();
	const auto* again = std::get_if<gamesave::SheepRecord>(written.record.get());
	ASSERT_NE(again, nullptr);
	EXPECT_TRUE((again->position.flags & gamesave::k_PositionFlagInPhysics) != 0);
	ASSERT_TRUE(again->object.physics.has_value());
	EXPECT_EQ(*again->object.physics, *sheep.object.physics);
}

// Integration test: needs the original game's data; skipped without it. The frozen Land 3 saves of _J_a_h_o_v_i_a's
// slots 0 and 15, a pigeon on fire and a sheep in the physics: each is planned back into the physics with its body and
// made, and its flock is made with its members
TEST_P(FlockLoadWorld, TheFrozenSavesAnimalsInThePhysicsAreMadeAndTheirFlocksWithThem)
{
	const std::array cases {
	    std::tuple {std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "0" / "0.sav", gamesave::ObjectNumber {7071},
	                gamesave::ObjectNumber {7068}},
	    std::tuple {std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "15" / "3" / "15.sav",
	                gamesave::ObjectNumber {6220}, gamesave::ObjectNumber {6212}},
	};
	for (const auto& [relative, animalNumber, flockNumber] : cases)
	{
		const auto bytes = FrozenSave(relative);
		if (!GameData() || !bytes.has_value())
		{
			GTEST_SKIP() << "the frozen save is the game's data";
		}
		const auto world = gamesave::ReadWorld(*bytes);
		ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
		const auto plan = load::PlanOf(world);
		EXPECT_TRUE(plan.inPhysics.contains(animalNumber)) << "animal " << animalNumber;
		const auto flock = std::ranges::find(plan.flocks, flockNumber, &load::PlannedFlock::number);
		ASSERT_NE(flock, plan.flocks.end()) << "flock " << flockNumber;
		// its members made as the load makes them; a member walking round a thing (sheep 6216 round object 6133) needs
		// that thing made, which only the whole load does (AnAnimalWalkingRoundACircleIsMadeInItsWalk... covers it), so
		// it stands in made, as what else the flock names does
		auto& registry = Locator::entitiesRegistry::value();
		std::map<gamesave::ObjectNumber, entt::entity> made;
		for (const auto& planned : plan.animals)
		{
			if (planned.living.flock.number != flockNumber)
			{
				continue;
			}
			if (planned.move.circle.has_value() && planned.move.circle->object.number != 0)
			{
				made.emplace(planned.number, registry.Create());
				continue;
			}
			const auto entity = load::MakeAnimalEntity(planned, NoObject);
			EXPECT_TRUE(entity != entt::null) << "animal " << planned.number;
			made.emplace(planned.number, entity);
		}
		ASSERT_TRUE(made.contains(animalNumber));
		EXPECT_TRUE(registry.AllOf<ecs::components::Animal>(made.at(animalNumber))) << "animal " << animalNumber;
		const auto objectOf = [&made, &registry](gamesave::ObjectNumber number) {
			const auto found = made.find(number);
			return found != made.end() ? found->second : registry.Create();
		};
		EXPECT_TRUE(load::MakeFlockEntity(*flock, objectOf) != entt::null) << "flock " << flockNumber;
	}
}

INSTANTIATE_TEST_SUITE_P(Data, FlockLoadWorld, ::testing::Values(test::world::WorldData::Game), test::world::DataName);

// Integration test: needs the original game's data and a save the original made (local reference data in
// original_saves beside the game's folder); skipped without them. The original loaded the frozen Land 1
// autosave and saved it again (land1_autosave_resumed_120s): each of its flocks of two or more members has them in
// the other order, as a flock's load puts members of the same place (every saved member's is 0) the other way round
TEST(FlockLoadOriginal, TheOriginalsResaveReversesTheFlocksTies)
{
	const auto frozen = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                               "Saved Games" / "20" / "20.sav");
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!frozen.has_value() || game == nullptr)
	{
		GTEST_SKIP() << "the frozen Land 1 autosave and the original's resave of it are local data";
	}
	// the original's own saves sit beside the frozen copy of the game
	const auto folder = std::filesystem::path(game).parent_path() / "original_saves";
	std::ifstream file(folder / "land1_autosave_resumed_120s" /
	                       "profile__e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" / "Saved Games" / "20" /
	                       "20.sav",
	                   std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no resave of the original in " << folder;
	}
	const std::vector<uint8_t> resaved((std::istreambuf_iterator<char>(file)), {});
	const auto before = gamesave::ReadWorld(*frozen);
	const auto after = gamesave::ReadWorld(resaved);
	size_t flocks = 0;
	for (const auto& object : before.objects)
	{
		const auto* kept = object.has_value() ? std::get_if<gamesave::FlockRecord>(&*object) : nullptr;
		if (kept == nullptr || kept->members.numbers.size() < 2)
		{
			continue;
		}
		const auto* again = FlockAt(after, kept->savedDomainCentre, kept->lastWord);
		ASSERT_NE(again, nullptr);
		auto reversed = MembersByCreation(before, *kept);
		std::ranges::reverse(reversed);
		EXPECT_EQ(MembersByCreation(after, *again), reversed);
		++flocks;
	}
	EXPECT_EQ(flocks, 16u);
}
