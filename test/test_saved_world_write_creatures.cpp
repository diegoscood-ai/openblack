/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature's own values written from what openblack holds: its place in the game's list of creatures, its player,
// its spells and those waiting, its mind's stillness, its sizes, its fizz (none while its invisible spell is off, as
// the save also sets it), its freeze, the leash's target, its home, its fight health, its body, its alignment (through
// the curse's script global for the local player's creature), its scale and its creed powers; the values read back
// as written; and a creature whose values have no source yet stops the write (docs/wiki/engine/saved-games.md#openblack)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <CreatureRecords.h>
#include <LHVMTypes.h>
#include <ObjectStream.h>
#include <ParticleRecords.h>
#include <SavedRecords.h>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureHandGlow.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureMiracleReactions.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureScriptPlay.h"
#include "Creature/CreatureSpells.h"
#include "Creature/CreatureTownHelp.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureAutoscale.h"
#include "ECS/Components/CreatureCasting.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureFizz.h"
#include "ECS/Components/CreatureHandGlows.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSizeLimits.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/DrawPosition.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureLoop.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteCreatures.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleManagerState.h"
#include "Particles/ParticleSpellFile.h"
#include "support/FireFakes.h"
#include "support/ParticleFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
class CreatureWrite: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::fireSystem::emplace<test::FakeFires>();
		Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
		Locator::time::emplace<ecs::systems::TimeSystem>();
		Locator::vm::reset();
		Locator::playerSystem::reset();
	}

	ecs::Registry& Reg() { return *_registry; }
	uint32_t _creaturesMade {0};

	/// A creature as the creature archetype makes it, at the head of the game's list of creatures
	entt::entity Creature(PlayerNames owner)
	{
		const auto entity = Reg().Create();
		Reg().Assign<components::Creature>(entity, components::Creature {.owner = owner});
		// the list of creatures runs from the newest made
		Reg().Assign<components::ObjectCreationIndex>(entity, ++_creaturesMade);
		return entity;
	}

	/// The creature's own values the writer makes, and the failure when it makes none
	std::optional<gamesave::CreatureFields> Written(entt::entity creature, std::string* failure = nullptr)
	{
		_context.reset();
		_source.reset();
		_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
		_context.emplace(_writers, *_source);
		auto fields = CreatureFieldsOf(*_context, creature, nullptr);
		if (failure != nullptr)
		{
			*failure = _context->Failure();
		}
		return fields;
	}

	ClassWriters _writers;
	std::optional<WorldSource> _source;
	std::optional<WriteContext> _context;

private:
	// the map cells the place's flags ask, reset after the registry
	test::RestoreService<Locator::mapCellsSystem> _restoreCells;
	// the fires an object's writer asks of its burning, reset after the registry
	test::RestoreService<Locator::fireSystem> _restoreFires;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::time> _restoreTime;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::playerSystem> _restorePlayers;
	ecs::Registry* _registry {nullptr};
};

/// The record a creature's own values make, read back
gamesave::CreatureFields ReadBack(const gamesave::CreatureFields& fields)
{
	constexpr gamesave::StreamStart k_After100 {.objects = 100, .checksum = 0x27000};
	gamesave::CreatureRecord record;
	record.creature = fields;
	gamesave::StreamWriter writer(k_After100);
	gamesave::Write(writer, gamesave::SavedRecord {record}, nullptr);
	const auto bytes = writer.Take();
	gamesave::StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	gamesave::SavedRecord read;
	EXPECT_TRUE(gamesave::ReadRecord(reader, type, extra, read, nullptr));
	const auto* creature = std::get_if<gamesave::CreatureRecord>(&read);
	return creature != nullptr ? creature->creature : gamesave::CreatureFields {};
}
} // namespace

// Every value a creature made in play holds is written in its place, and the values read back as written
// A loaded creature's spell slots name the miracles the load made again from the numbers they were saved naming, none
// for one not made; the kept numbers go once linked (docs/wiki/engine/saved-games.md)
TEST_F(CreatureWrite, ALoadedCreaturesSpellSlotsNameTheMiraclesMadeAgain)
{
	const auto creature = Reg().Create();
	auto& spells = Reg().Assign<components::CreatureSpells>(creature);
	components::LoadedSpellSlots saved;
	saved.miracles.at(0) = 41;
	saved.miracles.at(2) = 42;
	Reg().Assign<components::LoadedSpellSlots>(creature, saved);
	const auto miracle = Reg().Create();
	ecs::saved_world_load::LinkSpellSlots(Reg(), {{41u, miracle}});
	EXPECT_EQ(spells.spells.slots.at(0).miracle, miracle);
	EXPECT_EQ(spells.spells.slots.at(1).miracle, entt::entity {entt::null});
	EXPECT_EQ(spells.spells.slots.at(2).miracle, entt::entity {entt::null});
	EXPECT_FALSE(Reg().AllOf<components::LoadedSpellSlots>(creature));
}

TEST_F(CreatureWrite, ACreatureMadeInPlayWritesWhatItHoldsAndReadsBackTheSame)
{
	const auto older = Creature(PlayerNames::PLAYER_TWO);
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	auto& body = Reg().Get<components::Creature>(creature);
	body.strength = 0.7f;
	body.size = 1.3f;
	body.alignment = -0.2f;
	body.canDie = false;
	body.mayGoThroughVortex = true;
	const auto miracle = Reg().Create();
	auto& spells = Reg().Assign<components::CreatureSpells>(creature);
	auto& freeze = spells.spells[creature_spells::Spell::Freeze];
	freeze = {.phase = creature_spells::Phase::Holding, .turnsLeft = 5, .holdTurns = 30, .miracle = miracle, .before = 0.5f};
	spells.freeze = 0.25f;
	Reg().Assign<components::CreatureSizeLimits>(
	    creature, components::CreatureSizeLimits {.limits = {.smallest = 0.3f, .largest = 2.0f}});
	Reg().Assign<components::CreatureFizz>(creature,
	                                       components::CreatureFizz {.fizz = {.now = 0.4f, .target = 1.0f, .perSecond = 0.5f}});
	Reg().Assign<components::CreatureMindState>(creature).paused = true;
	auto& glows = Reg().Assign<components::CreatureHandGlows>(creature);
	glows.glows.at(1) = creature_hand_glow::Glow {.power = 0.3f, .scale = 0.8f};
	Reg().Assign<components::CreatureAutoscale>(creature, components::CreatureAutoscale {.enabled = true, .factor = 0.6f});
	Reg().Assign<components::CreatureFightHealth>(creature, components::CreatureFightHealth {.health = 0.6f});
	const auto rock = Reg().Create();
	auto& leash = Reg().Assign<components::CreatureLeash>(creature);
	leash.targetPoint = glm::vec3(1.0f, 2.0f, 3.0f);
	leash.targetThing = rock;
	leash.hasTarget = true;
	leash.home = glm::vec3(100.0f, 0.0f, 200.0f);
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);

	const auto fields = Written(creature);
	ASSERT_TRUE(fields.has_value()) << _context->Failure();
	EXPECT_EQ(fields->version, 33u);
	EXPECT_EQ(fields->savedInTemple, 1u);
	EXPECT_EQ(fields->index, 0u);
	EXPECT_EQ(fields->owner, 0u);
	EXPECT_EQ(fields->spells.at(0).spell, 16u);
	EXPECT_EQ(fields->spells.at(0).phase, 3u);
	EXPECT_EQ(fields->spells.at(0).turnsLeft, 5);
	EXPECT_EQ(fields->spells.at(0).found, 0.5f);
	EXPECT_EQ(fields->spells.at(0).holdTurns, 30);
	EXPECT_EQ(fields->spells.at(0).miracle.number, _context->Reference(miracle));
	EXPECT_EQ(fields->spells.at(1).phase, 0u);
	EXPECT_TRUE(fields->queued.empty());
	EXPECT_EQ(fields->mindProcessing, 0u);
	EXPECT_EQ(fields->largestSize, 2.0f);
	EXPECT_EQ(fields->smallestSize, 0.3f);
	EXPECT_EQ(fields->mayGoThroughVortex, 1u);
	// its invisible spell is off: no fizz
	EXPECT_EQ(fields->fizz, 0.0f);
	EXPECT_EQ(fields->fizzTarget, 0.0f);
	EXPECT_EQ(fields->fizzPerSecond, 0.0f);
	EXPECT_EQ(fields->freeze, 0.25f);
	EXPECT_EQ(fields->animationPace, 0.75f);
	EXPECT_EQ(fields->targetPoint, (gamesave::Point {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(fields->targetObject.number, _context->Reference(rock));
	EXPECT_EQ(fields->hasTarget, 1u);
	// no temple: the home the leash keeps it at
	EXPECT_EQ(fields->home.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(fields->home.z, map_coords::ToFixed(200.0f));
	EXPECT_EQ(fields->fightHealth, 0.6f);
	EXPECT_EQ(fields->canDie, 0u);
	EXPECT_EQ(fields->strength, 0.7f);
	EXPECT_EQ(fields->size, 1.3f);
	EXPECT_EQ(fields->alignment, -0.2f);
	EXPECT_EQ(fields->autoScale, 1u);
	EXPECT_EQ(fields->autoScaleShare, 0.6f);
	EXPECT_EQ(fields->keepsPlan, 0u);
	EXPECT_EQ(fields->creedPowers.at(0).power, 0.0f);
	EXPECT_EQ(fields->creedPowers.at(1).scale, 0.8f);
	EXPECT_EQ(fields->creedPowers.at(1).power, 0.3f);
	EXPECT_EQ(ReadBack(*fields), *fields);

	// the older creature is the second in the list
	const auto second = Written(older);
	ASSERT_TRUE(second.has_value()) << _context->Failure();
	EXPECT_EQ(second->index, 1u);
	EXPECT_EQ(second->owner, 1u);
	game_clock::SetSequenceMode(0);
}

// A save leaves the creature's fizz as the original's does: zeroed while its invisible spell is off, as it was while
// the spell is on; the record writes the fizz the same way
TEST_F(CreatureWrite, ASaveZeroesTheFizzOnlyWhileTheInvisibleSpellIsOff)
{
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	Reg().Assign<components::CreatureFizz>(creature,
	                                       components::CreatureFizz {.fizz = {.now = 0.4f, .target = 1.0f, .perSecond = 0.5f}});
	auto& spells = Reg().Assign<components::CreatureSpells>(creature);
	spells.spells[creature_spells::Spell::Invisible].phase = creature_spells::Phase::Holding;

	ZeroFizzForSave(Reg(), creature);
	const auto& kept = Reg().Get<components::CreatureFizz>(creature).fizz;
	EXPECT_EQ(kept.now, 0.4f);
	EXPECT_EQ(kept.target, 1.0f);
	EXPECT_EQ(kept.perSecond, 0.5f);
	const auto on = Written(creature);
	ASSERT_TRUE(on.has_value()) << _context->Failure();
	EXPECT_EQ(on->fizz, 0.4f);
	EXPECT_EQ(on->fizzTarget, 1.0f);
	EXPECT_EQ(on->fizzPerSecond, 0.5f);

	Reg().Get<components::CreatureSpells>(creature).spells[creature_spells::Spell::Invisible].phase =
	    creature_spells::Phase::Off;
	ZeroFizzForSave(Reg(), creature);
	const auto& zeroed = Reg().Get<components::CreatureFizz>(creature).fizz;
	EXPECT_EQ(zeroed.now, 0.0f);
	EXPECT_EQ(zeroed.target, 0.0f);
	EXPECT_EQ(zeroed.perSecond, 0.0f);
}

// The local player's creature writes, while the curse's script runs, the alignment the script's global keeps; the
// script library finds a task by its script's name letter for letter and reads the first global of the name as a
// float, 0 without one
TEST(CreatureWriteCurse, WhileTheCursesScriptRunsTheLocalCreatureWritesItsAlignmentFromBeforeTheCurse)
{
	std::map<uint32_t, lhvm::VMTask> tasks;
	tasks.emplace(3, lhvm::VMTask({}, 1, 3, 0, 0, {}, "CreatureCurse", "challenge.chl", lhvm::ScriptType::Script));
	std::vector<lhvm::VMVar> globals;
	globals.emplace_back(lhvm::DataType::Float, lhvm::VMValue(0.75f), "OriginalAlignmentOfMyCreature");
	globals.emplace_back(lhvm::DataType::Float, lhvm::VMValue(-1.0f), "OriginalAlignmentOfMyCreature");

	EXPECT_TRUE(ScriptIsRunning(tasks, "CreatureCurse"));
	EXPECT_FALSE(ScriptIsRunning(tasks, "creaturecurse"));
	EXPECT_EQ(GlobalAsFloat(globals, "OriginalAlignmentOfMyCreature"), 0.75f);
	EXPECT_EQ(GlobalAsFloat(globals, "NoSuchGlobal"), 0.0f);
	EXPECT_EQ(SavedAlignment(-0.5f, true, &tasks, globals), 0.75f);
	EXPECT_EQ(SavedAlignment(-0.5f, false, &tasks, globals), -0.5f);
	EXPECT_EQ(SavedAlignment(-0.5f, true, nullptr, globals), -0.5f);
	const std::map<uint32_t, lhvm::VMTask> none;
	EXPECT_EQ(SavedAlignment(-0.5f, true, &none, globals), -0.5f);
}

// A creature casting, fighting, dancing, controlled by a script, showing the leash's mark or with spells waiting stops the
// write, naming it: those values are not written from openblack yet
TEST_F(CreatureWrite, ACreatureWhoseValuesHaveNoSourceYetStopsTheWrite)
{
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	Reg().Assign<components::CreatureFighting>(creature);
	std::string failure;
	EXPECT_FALSE(Written(creature, &failure).has_value());
	EXPECT_NE(failure.find("is fighting"), std::string::npos) << failure;

	const auto waiting = Creature(PlayerNames::PLAYER_TWO);
	Reg().Assign<components::CreatureSpells>(waiting).spells.waiting.push_back(
	    {.spell = creature_spells::Spell::Thin, .holdTurns = 40, .miracle = entt::null});
	failure.clear();
	EXPECT_FALSE(Written(waiting, &failure).has_value());
	EXPECT_NE(failure.find("spells waiting"), std::string::npos) << failure;
}

// A creature that called villagers to dance with it writes them, the last called first, and the dance it made last
// while there is one; it keeps its plan meanwhile (with no plan and nothing to do, the plan a finished one leaves)
TEST_F(CreatureWrite, ACreatureThatCalledDancersWritesThemAndKeepsItsPlan)
{
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	const auto first = Reg().Create();
	const auto second = Reg().Create();
	const auto dance = Reg().Create();
	Reg().Assign<components::CreatureMindState>(creature);
	Reg().Assign<components::CreatureDance>(creature, components::CreatureDance {.dance = dance, .dancers = {second, first}});
	std::string failure;
	const auto fields = Written(creature, &failure);
	ASSERT_TRUE(fields.has_value()) << failure;
	ASSERT_EQ(fields->dancePartners.numbers.size(), 2u);
	EXPECT_EQ(fields->dancePartners.numbers[0], _context->Reference(second));
	EXPECT_EQ(fields->dancePartners.numbers[1], _context->Reference(first));
	EXPECT_EQ(fields->dance.number, _context->Reference(dance));
	EXPECT_EQ(fields->keepsPlan, 1u);
	EXPECT_EQ(fields->plan.desire, 40u);
}

// The miracle a creature holds as it casts, the beams at its hands (the mirror hand's first) and the leash's mark are
// written while their spot visuals are there, each by its container's key; a spot visual that has gone is none
TEST_F(CreatureWrite, TheHeldMiracleTheBeamsAndTheLeashsMarkAreTheirSpotVisualsContainers)
{
	const test::RestoreService<Locator::particleSystem> restoreParticles;
	auto& particles = Locator::particleSystem::emplace<test::InertParticleSystem>();
	auto& state = particles.GetState();
	const auto spotVisual = [&state, this](uint32_t effect) {
		const auto object = Reg().Create();
		auto file = std::make_shared<particles::File>();
		state.effects[effect].effect =
		    std::make_unique<particles::Effect>(file, glm::vec3(0.0f), 1.0f, game_random::particles::NetGameType::Synced);
		state.containers.push_front({.effect = effect, .object = object});
		return object;
	};
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	auto& casting = Reg().Assign<components::CreatureCasting>(creature);
	casting.held = Reg().Create();
	casting.beams = {spotVisual(3), spotVisual(4)};
	Reg().Assign<components::CreatureLeash>(creature).targetMarker = spotVisual(5);

	const auto fields = Written(creature);
	ASSERT_TRUE(fields.has_value()) << _context->Failure();
	EXPECT_EQ(fields->spellVisuals.at(0).number, _context->Reference(casting.held));
	EXPECT_EQ(fields->spellVisuals.at(1).number, 0u);
	EXPECT_EQ(fields->spellVisuals.at(2).number, 0u);
	EXPECT_EQ(fields->handParticles.at(0).number,
	          _context->Reference(SourceKey {.type = gamesave::ParticleContainerRecord::k_Type, .id = 3}));
	EXPECT_EQ(fields->handParticles.at(1).number,
	          _context->Reference(SourceKey {.type = gamesave::ParticleContainerRecord::k_Type, .id = 4}));
	EXPECT_EQ(fields->targetParticles.number,
	          _context->Reference(SourceKey {.type = gamesave::ParticleContainerRecord::k_Type, .id = 5}));

	// the mark's spot visual gone: none
	state.effects.erase(5);
	const auto gone = Written(creature);
	ASSERT_TRUE(gone.has_value()) << _context->Failure();
	EXPECT_EQ(gone->targetParticles.number, 0u);
}

// A creature made in play is written whole by its class's writer: its object names its species' row of the creature
// table (2517 less the row), its object's scale is its species' start size, its object's angle 0, its object's place
// where it stood as its last turn began, and its thing's flags none
TEST_F(CreatureWrite, TheClassWriterWritesACreatureMadeInPlayWhole)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	info->creature.at(static_cast<size_t>(CreatureType::Cow)).startScale = 0.3f;
	Locator::infoConstants::emplace(*info);
	RegisterCreatureWriters(_writers);

	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	Reg().Get<components::Creature>(creature).species = CreatureType::Cow;
	Reg().Assign<components::Transform>(creature, glm::vec3(100.0f, 0.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	// where it stood as its last turn began
	Reg().Assign<components::DrawPosition>(
	    creature, components::DrawPosition {.turnStart = glm::vec3(90.0f, 0.0f, 195.0f), .started = true});

	_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
	_context.emplace(_writers, *_source);
	const auto made = _context->RecordOf(_context->Reference(creature));
	ASSERT_NE(made.record, nullptr) << _context->Failure();
	const auto* record = std::get_if<gamesave::CreatureRecord>(made.record.get());
	ASSERT_NE(record, nullptr);
	EXPECT_EQ(record->object.info, 2516u);
	EXPECT_EQ(record->object.scale, creature_morph::ClampScale(0.3f));
	EXPECT_EQ(record->object.heading, 0.0f);
	EXPECT_EQ(record->object.coords.x, map_coords::ToFixed(90.0f));
	EXPECT_EQ(record->object.coords.z, map_coords::ToFixed(195.0f));
	EXPECT_EQ(record->position.position.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(record->position.position.z, map_coords::ToFixed(200.0f));
	EXPECT_EQ(record->thing.flags, 0);
	EXPECT_EQ(record->creature.owner, static_cast<uint8_t>(PlayerNames::PLAYER_ONE));
}

// A loaded creature's kept flock is none once that flock is gone, as a deleted flock takes itself off its members; a
// flock still there keeps its number
TEST_F(CreatureWrite, ALoadedCreatureNamesNoFlockGoneSinceTheLoad)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	Locator::infoConstants::emplace(*info);
	RegisterCreatureWriters(_writers);
	for (const bool gone : {true, false})
	{
		const auto creature = Creature(PlayerNames::PLAYER_ONE);
		Reg().Get<components::Creature>(creature).species = CreatureType::Cow;
		Reg().Assign<components::Transform>(creature, glm::vec3(100.0f, 0.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		gamesave::CreatureRecord kept;
		kept.living.flock = {5};
		Reg().Assign<components::LoadedRecord<gamesave::CreatureRecord>>(creature, kept);
		const auto flock = Reg().Create();
		ASSERT_TRUE(flock != entt::null);
		const LoadedObjects loaded {{5, {.entity = flock, .type = gamesave::FlockRecord::k_Type}}};
		if (gone)
		{
			Reg().Destroy(flock);
		}
		_context.reset();
		_source.reset();
		_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd", .loaded = loaded});
		_context.emplace(_writers, *_source);
		const auto written = (*_writers.WriterOf(gamesave::CreatureRecord::k_Type))(*_context, creature);
		ASSERT_TRUE(written.has_value()) << _context->Failure();
		EXPECT_EQ(std::get<gamesave::CreatureRecord>(*written).living.flock.number, gone ? 0u : 5u) << gone;
	}
}

// Without the game's creature table a creature has no row to name, and its write stops naming it
TEST_F(CreatureWrite, ACreatureWithNoRowOfItsSpeciesStopsTheWrite)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset();
	RegisterCreatureWriters(_writers);
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
	_context.emplace(_writers, *_source);
	const auto made = _context->RecordOf(_context->Reference(creature));
	EXPECT_EQ(made.record, nullptr);
	EXPECT_NE(_context->Failure().find("has no row of its species"), std::string::npos) << _context->Failure();
}

namespace
{
/// A thing named by its number, as the write names it
gamesave::Ref NumberRef(uint32_t thing)
{
	return {.number = thing + 100};
}

/// A mind a script's play of an animation holds: its plan of obeying its player, told to play, at its first step
components::CreatureMindState PlayingForScript(uint32_t animation, uint32_t plays)
{
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
	                                               .action = creature_planner::k_FollowPlayerAction};
	mind.idle.activity = creature_mind::Activity::Told;
	mind.idle.agenda = creature_script_play::Agenda({.animation = animation, .plays = plays});
	return mind;
}
} // namespace

// A creature a script focuses on a point or a thing keeps the same plan with its one turn: to face the point, at the
// height the script gave it, or the thing, held two seconds; the load reads the same turn back
TEST(CreatureWriteScriptPlan, AScriptsFocusKeepsItsPlanAndItsTurn)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
	                                               .action = creature_planner::k_FollowPlayerAction};
	mind.idle.activity = creature_mind::Activity::Told;
	mind.scriptFocus = glm::vec3(600.0f, 33.25f, 700.0f);
	mind.idle.agenda = creature_mind::TurnToFacePoint({600.0f, 700.0f});
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->action, 22u);
	ASSERT_EQ(plan->steps.size(), 1u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 7, .point = {600.0f, 33.25f, 700.0f}}));
	EXPECT_EQ(ecs::saved_world_load::ScriptTurnOf(*plan),
	          (ecs::saved_world_load::KeptScriptTurn {.point = glm::vec3(600.0f, 33.25f, 700.0f)}));
	mind.scriptFocus.reset();
	mind.idle.agenda = creature_mind::FaceThingForScript(5);
	const auto thing = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(thing.has_value()) << why;
	ASSERT_EQ(thing->steps.size(), 1u);
	EXPECT_EQ(thing->steps[0].action, 6u);
	EXPECT_EQ(thing->steps[0].object, NumberRef(5));
	EXPECT_EQ(thing->steps[0].real, 2.0f);
	EXPECT_EQ(ecs::saved_world_load::ScriptTurnOf(*thing),
	          (ecs::saved_world_load::KeptScriptTurn {.thing = NumberRef(5).number, .seconds = 2.0f}));
}

// A creature a script holds that sleeps on the spot by its own plan, with no clear place, keeps that plan as the game's
// plain sleep: the walk home (its point on the land), the face down the slope, the sleep (the main step) and the wake
TEST(CreatureWriteScriptPlan, AHeldCreaturesOwnPlainSleepKeepsItsPlanAndItsAgenda)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Tiredness, .action = 313};
	mind.idle.agenda =
	    creature_mind::SleepOnTheSpot(std::nullopt, 2.0f, glm::vec2(600.0f, 700.0f), 3.0f, [](uint32_t) { return 1u; });
	ASSERT_EQ(mind.idle.agenda.size(), 4u);
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::Tiredness));
	EXPECT_EQ(plan->action, 313u);
	EXPECT_EQ(plan->count, 4u);
	EXPECT_EQ(plan->words[1], 2u);
	ASSERT_EQ(plan->steps.size(), 4u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 8, .point = {600.0f, 0.0f, 700.0f}, .real = 3.0f}));
	EXPECT_EQ(plan->steps[1].action, 0x44u);
	EXPECT_EQ(plan->steps[2].action, 0x84u);
	EXPECT_EQ(plan->steps[3].action, 10u);
	EXPECT_EQ(plan->steps[3].integer, 63u);
}

// A creature a script holds that fishes and eats the catch by its own plan keeps that plan as the game's: the walk to
// the shoal with its height, the food made from the sea, its pick-up and the eat of it, the main step; once the fishing
// step has started, the game's pick-up is under way
TEST(CreatureWriteScriptPlan, AHeldCreaturesOwnFishingKeepsItsPlanAndItsAgenda)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 155};
	mind.idle.agenda = creature_mind::FishAndEat({600.0f, 700.0f}, 15.0f, 2.5f);
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::Hunger));
	EXPECT_EQ(plan->action, 155u);
	EXPECT_EQ(plan->count, 4u);
	EXPECT_EQ(plan->words[1], 3u);
	ASSERT_EQ(plan->steps.size(), 4u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 8, .point = {600.0f, 2.5f, 700.0f}, .real = 15.0f}));
	EXPECT_EQ(plan->steps[1].action, 0x5Cu);
	EXPECT_EQ(plan->steps[2].action, 0x37u);
	EXPECT_EQ(plan->steps[3].action, 0x80u);
	mind.idle.step = 1;
	mind.idle.stepStarted = true;
	const auto picking = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(picking.has_value()) << why;
	EXPECT_EQ(picking->current, 2u);
	EXPECT_EQ(picking->state, 1u);
	ASSERT_EQ(picking->steps.size(), 2u);
	EXPECT_EQ(picking->steps[0].action, 0x37u);
}

// A creature a script holds that looks over a miracle's place by its own plan keeps that plan as the game's: pointing
// first, the turn, the point a second, the walk to within five heights, the turn and the puzzled wait, the main step;
// else the walk, the turn and the point two seconds, the main step; each point the place on the land
TEST(CreatureWriteScriptPlan, AHeldCreaturesOwnLookOverAPlaceKeepsItsPlanAndItsAgenda)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::Curiosity, .action = 157, .forced = true};
	mind.idle.agenda = creature_mind::ExamineMiracle({600.0f, 700.0f}, 3.0f, [](uint32_t) { return 0u; });
	ASSERT_EQ(mind.idle.agenda.size(), 5u);
	mind.idle.step = 4;
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->action, 157u);
	EXPECT_EQ(plan->count, 5u);
	EXPECT_EQ(plan->current, 4u);
	EXPECT_EQ(plan->words[1], 4u);
	ASSERT_EQ(plan->steps.size(), 1u);
	EXPECT_EQ(plan->steps[0].action, 0x18u);
	mind.idle.agenda = creature_mind::ExamineMiracle({600.0f, 700.0f}, 3.0f, [](uint32_t) { return 1u; });
	mind.idle.step = 0;
	const auto pointing = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(pointing.has_value()) << why;
	EXPECT_EQ(pointing->count, 3u);
	EXPECT_EQ(pointing->words[1], 2u);
	ASSERT_EQ(pointing->steps.size(), 3u);
	EXPECT_EQ(pointing->steps[0], (gamesave::AgendaStep {.action = 8, .point = {600.0f, 0.0f, 700.0f}, .real = 15.0f}));
	EXPECT_EQ(pointing->steps[1].action, 7u);
	EXPECT_EQ(pointing->steps[2], (gamesave::AgendaStep {.action = 0x50, .point = {600.0f, 0.0f, 700.0f}, .real = 2.0f}));
}

// A creature a script sends somewhere keeps the same plan with its two walks to the point, the script's point with its
// height, the first within a sixth of the way and the second within its height; the second leg once past the first
TEST(CreatureWriteScriptPlan, AScriptsMoveKeepsItsPlanAndItsTwoWalks)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer,
	                                               .action = creature_planner::k_FollowPlayerAction};
	mind.idle.activity = creature_mind::Activity::Told;
	mind.idle.agenda = creature_mind::MoveForScript({0.0f, 0.0f}, {600.0f, 33.25f, 0.0f}, 15.0f, 0.0f);
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->action, 22u);
	ASSERT_EQ(plan->steps.size(), 2u);
	EXPECT_EQ(plan->steps[0],
	          (gamesave::AgendaStep {
	              .action = 8, .point = {600.0f, 33.25f, 0.0f}, .real = mind.idle.agenda[0].movement.maxDistance}));
	EXPECT_EQ(plan->steps[1], (gamesave::AgendaStep {.action = 8, .point = {600.0f, 33.25f, 0.0f}, .real = 15.0f}));
	mind.idle.step = 1;
	mind.idle.stepStarted = true;
	const auto second = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(second.has_value()) << why;
	EXPECT_EQ(second->current, 1u);
	EXPECT_EQ(second->state, 1u);
	EXPECT_EQ(second->steps.size(), 1u);
}

// A creature a script has play an individual animation twice keeps its plan of obeying its player, about itself, and
// its agenda as the game builds it: the animation twice, then the wait until its body is free, the first still to start
TEST(CreatureWriteScriptPlan, AScriptsPlayKeepsItsPlanAndItsAgendaFromTheStepItIsAt)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	auto mind = PlayingForScript(63, 2);
	std::string why;
	auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, 24u);
	EXPECT_EQ(plan->action, 22u);
	EXPECT_EQ(plan->objects, (std::array<gamesave::Ref, 3> {gamesave::Ref {}, k_Self, gamesave::Ref {}}));
	EXPECT_EQ(plan->firstWord, 1u);
	EXPECT_EQ(plan->current, 0u);
	EXPECT_EQ(plan->state, 0u);
	EXPECT_EQ(plan->count, 3u);
	EXPECT_EQ(plan->words, (std::array<uint32_t, 3> {0, 0xFFFFFFFF, 0}));
	ASSERT_EQ(plan->steps.size(), 3u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 63}));
	EXPECT_EQ(plan->steps[1], (gamesave::AgendaStep {.action = 10, .integer = 63}));
	EXPECT_EQ(plan->steps[2], (gamesave::AgendaStep {.action = 0x85}));
	EXPECT_EQ(plan->creature, k_Self);
	// written with the creature, it reads back as written
	gamesave::CreatureFields fields {.keepsPlan = 1, .plan = *plan};
	EXPECT_EQ(ReadBack(fields).plan, *plan);
	// under way at its second step: from there, past its start
	mind.idle.step = 1;
	mind.idle.stepStarted = true;
	plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value());
	EXPECT_EQ(plan->current, 1u);
	EXPECT_EQ(plan->state, 1u);
	EXPECT_EQ(plan->firstWord, 0u);
	EXPECT_EQ(plan->steps.size(), 2u);
}

// A sit-down a script forces at a thing keeps its plan of obeying its player with the action's row, what it is done to,
// and the game's four steps, the sit the main step: from the step it is at, turning to face the thing, facing down the
// slope and the sit for its drawn seconds, and the walk there with its point's height; a sit-down of other steps,
// or a walk whose height is not held, is not written yet
TEST(CreatureWriteScriptPlan, AForcedSitDownKeepsItsPlanAndItsAgendaFromTheStepItIsAt)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr uint32_t k_Place = 5;
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {
	    .desire = creature_desires::Desire::ObeyPlayer, .action = 81, .object = k_Place, .forced = true};
	mind.idle.activity = creature_mind::Activity::Sit;
	mind.idle.agenda = creature_mind::SitDownAt(k_Place, {10.0f, 20.0f}, 4.0f, 12.5f);
	mind.idle.step = 1;
	std::string why;
	auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, 24u);
	EXPECT_EQ(plan->action, 81u);
	EXPECT_EQ(plan->objects, (std::array<gamesave::Ref, 3> {gamesave::Ref {}, NumberRef(k_Place), gamesave::Ref {}}));
	EXPECT_EQ(plan->count, 4u);
	EXPECT_EQ(plan->current, 1u);
	EXPECT_EQ(plan->state, 0u);
	EXPECT_EQ(plan->firstWord, 1u);
	EXPECT_EQ(plan->words, (std::array<uint32_t, 3> {0, 3, 0}));
	ASSERT_EQ(plan->steps.size(), 3u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 6, .object = NumberRef(k_Place), .real = 0.1f}));
	EXPECT_EQ(plan->steps[1], (gamesave::AgendaStep {.action = 0x44}));
	EXPECT_EQ(plan->steps[2], (gamesave::AgendaStep {.action = 4, .integer = 38, .real = 12.5f}));
	EXPECT_EQ(plan->creature, k_Self);
	// sitting, past its start
	mind.idle.step = 3;
	mind.idle.stepStarted = true;
	plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->state, 1u);
	EXPECT_EQ(plan->firstWord, 0u);
	EXPECT_EQ(plan->steps.size(), 1u);
	// still walking to the thing: the walk to its point, its height the thing's, within the creature's radius
	mind.idle.agenda = creature_mind::SitDownAt(k_Place, {10.0f, 20.0f}, 4.0f, 12.5f, 6.0f);
	mind.idle.step = 0;
	mind.idle.stepStarted = false;
	plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	ASSERT_EQ(plan->steps.size(), 4u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 8, .point = {10.0f, 6.0f, 20.0f}, .real = 4.0f}));
	// a walk whose height is not held, or a sit of one step: not yet
	mind.idle.agenda = creature_mind::SitDownAt(k_Place, {10.0f, 20.0f}, 4.0f, 12.5f);
	EXPECT_FALSE(ScriptHeldPlanOf(mind, k_Self, NumberRef, why).has_value());
	EXPECT_NE(why.find("walking"), std::string::npos) << why;
	mind.idle.agenda.resize(1);
	EXPECT_FALSE(ScriptHeldPlanOf(mind, k_Self, NumberRef, why).has_value());
}

namespace
{
/// A mind a script has forced an action of a row on, done to a thing, with the agenda openblack builds for it
components::CreatureMindState Forced(uint32_t action, std::vector<creature_mind::Step> agenda)
{
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer, .action = action, .object = 5, .forced = true};
	mind.idle.activity = creature_mind::Activity::Told;
	mind.idle.agenda = std::move(agenda);
	return mind;
}
} // namespace

// The other actions the scripts of the first two lands force are written with the game's sub-actions, their arguments
// and their main step: looking at a thing for ever or for a while (turning to face it for two seconds, then the wait in
// whole turns), pointing at a thing for five seconds, looking at the camera for five seconds of turns, pointing at the
// camera (turning to it, then pointing at its place for a second); waving at the player and being idle; a row not written yet
// stops the write naming it
TEST(CreatureWriteScriptPlan, TheFirstLandsForcedActionsKeepTheGamesSubActions)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	const auto planOf = [&](uint32_t action, std::vector<creature_mind::Step> agenda) {
		return ScriptHeldPlanOf(Forced(action, std::move(agenda)), k_Self, NumberRef, why);
	};
	const auto forever = planOf(189, creature_mind::LookForever(5));
	ASSERT_TRUE(forever.has_value()) << why;
	EXPECT_EQ(forever->words[1], 1u);
	ASSERT_EQ(forever->steps.size(), 2u);
	EXPECT_EQ(forever->steps[0], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 2.0f}));
	EXPECT_EQ(forever->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 864000}));
	const auto glance = planOf(188, creature_mind::LookButDontApproach(5, 1.5f));
	ASSERT_TRUE(glance.has_value()) << why;
	EXPECT_EQ(glance->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 15}));
	const auto point = planOf(163, creature_mind::PointAtThing(5));
	ASSERT_TRUE(point.has_value()) << why;
	EXPECT_EQ(point->words[1], 0u);
	ASSERT_EQ(point->steps.size(), 1u);
	EXPECT_EQ(point->steps[0], (gamesave::AgendaStep {.action = 0x51, .object = NumberRef(5), .real = 5.0f}));
	const auto look = planOf(82, creature_mind::LookAtCamera());
	ASSERT_TRUE(look.has_value()) << why;
	EXPECT_EQ(look->steps[0], (gamesave::AgendaStep {.action = 0x42, .integer = 50}));
	const auto camera = planOf(168, creature_mind::PointAtCamera(true, {1.0f, 2.0f, 3.0f}));
	ASSERT_TRUE(camera.has_value()) << why;
	EXPECT_EQ(camera->words[1], 1u);
	ASSERT_EQ(camera->steps.size(), 2u);
	EXPECT_EQ(camera->steps[0], (gamesave::AgendaStep {.action = 0x1F}));
	EXPECT_EQ(camera->steps[1], (gamesave::AgendaStep {.action = 0x50, .point = {1.0f, 2.0f, 3.0f}, .real = 1.0f}));
	// waving at the player: the turn to the camera, then the main wave
	const auto wave = planOf(90, creature_mind::FaceCameraIndividual({1.0f, 3.0f}, 72));
	ASSERT_TRUE(wave.has_value()) << why;
	EXPECT_EQ(wave->words[1], 1u);
	ASSERT_EQ(wave->steps.size(), 2u);
	EXPECT_EQ(wave->steps[0], (gamesave::AgendaStep {.action = 0x1F}));
	EXPECT_EQ(wave->steps[1], (gamesave::AgendaStep {.action = 10, .integer = 72}));
	// being idle: twice a wait of its drawn seconds in turns and a yawn, the last yawn the main step
	const auto idle = planOf(161, creature_mind::BeIdle([](uint32_t) { return 0u; }));
	ASSERT_TRUE(idle.has_value()) << why;
	EXPECT_EQ(idle->words[1], 3u);
	ASSERT_EQ(idle->steps.size(), 4u);
	EXPECT_EQ(idle->steps[0].action, 0x18u);
	EXPECT_EQ(idle->steps[1], (gamesave::AgendaStep {.action = 10, .integer = 57}));
	EXPECT_EQ(idle->steps[3], (gamesave::AgendaStep {.action = 10, .integer = 57}));
	EXPECT_FALSE(planOf(13, creature_mind::LookAtCamera()).has_value());
	EXPECT_NE(why.find("row 13"), std::string::npos) << why;
}

// The creature guide's forced actions, which run on every land: being frightened on the spot, waving at a thing,
// smiling at a friend, holding a thing and looking a thing over, each with the game's sub-actions and its main step, the step
// openblack marks
TEST(CreatureWriteScriptPlan, TheGuidesForcedActionsKeepTheGamesSubActions)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	const creature_mind::Random none = [](uint32_t) { return 0u; };
	std::string why;
	const auto planOf = [&](uint32_t action, std::vector<creature_mind::Step> agenda) {
		return ScriptHeldPlanOf(Forced(action, std::move(agenda)), k_Self, NumberRef, why);
	};
	const auto frightened = planOf(174, creature_mind::BeFrightenedOnTheSpot());
	ASSERT_TRUE(frightened.has_value()) << why;
	EXPECT_EQ(frightened->words[1], 2u);
	ASSERT_EQ(frightened->steps.size(), 3u);
	EXPECT_EQ(frightened->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 61}));
	EXPECT_EQ(frightened->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 30}));
	EXPECT_EQ(frightened->steps[2], (gamesave::AgendaStep {.action = 10, .integer = 61}));
	const auto wave = planOf(91, creature_mind::WaveAt(5, 10.0f));
	ASSERT_TRUE(wave.has_value()) << why;
	EXPECT_EQ(wave->words[1], 2u);
	ASSERT_EQ(wave->steps.size(), 3u);
	EXPECT_EQ(wave->steps[0], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 35.0f}));
	EXPECT_EQ(wave->steps[1], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 1.4f}));
	EXPECT_EQ(wave->steps[2], (gamesave::AgendaStep {.action = 10, .integer = 72}));
	const auto smile = planOf(60, creature_mind::SmileAtFriend(5, 10.0f, none));
	ASSERT_TRUE(smile.has_value()) << why;
	EXPECT_EQ(smile->words[1], 1u);
	ASSERT_EQ(smile->steps.size(), 3u);
	EXPECT_EQ(smile->steps[0], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 30.0f}));
	EXPECT_EQ(smile->steps[1], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 4.0f}));
	EXPECT_EQ(smile->steps[2], (gamesave::AgendaStep {.action = 10, .integer = 55}));
	const auto hold =
	    planOf(64, creature_mind::HoldObject(
	                   {.thing = 5, .camera = glm::vec2 {1.0f, 3.0f}, .playsWithIt = true, .turnsPerSecond = 10.0f}, none));
	ASSERT_TRUE(hold.has_value()) << why;
	EXPECT_EQ(hold->words[1], 1u);
	ASSERT_EQ(hold->steps.size(), 4u);
	EXPECT_EQ(hold->steps[0], (gamesave::AgendaStep {.action = 0, .object = NumberRef(5)}));
	EXPECT_EQ(hold->steps[1], (gamesave::AgendaStep {.action = 0x1F}));
	EXPECT_EQ(hold->steps[2], (gamesave::AgendaStep {.action = 3, .integer = 100}));
	EXPECT_EQ(hold->steps[3], (gamesave::AgendaStep {.action = 0x18, .integer = 400}));
	// looking a thing over: going near it, facing it, watching its foot, and the main take-in of what it is, by the kind
	// of the creature's belief about it
	auto examining = Forced(20, creature_mind::ExamineByLooking(5, 10.0f, 4.0f));
	EXPECT_FALSE(ScriptHeldPlanOf(examining, k_Self, NumberRef, why).has_value());
	examining.beliefs.Add(entt::entity {5}, 9);
	const auto examine = ScriptHeldPlanOf(examining, k_Self, NumberRef, why);
	ASSERT_TRUE(examine.has_value()) << why;
	EXPECT_EQ(examine->words[1], 3u);
	ASSERT_EQ(examine->steps.size(), 4u);
	EXPECT_EQ(examine->steps[0], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 25.0f}));
	EXPECT_EQ(examine->steps[1], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 2.0f}));
	EXPECT_EQ(examine->steps[2], (gamesave::AgendaStep {.action = 0x18, .integer = 11}));
	EXPECT_EQ(examine->steps[3], (gamesave::AgendaStep {.action = 0x3E, .integer = 9}));
	// sleeping at home, past its walk there: facing down the slope, the main sleep and the dazed wake; walking home with
	// no height of its home held it is not written yet
	auto sleeping = Forced(17, creature_mind::SleepAtHome({50.0f, 60.0f}, 10.0f, none));
	EXPECT_FALSE(ScriptHeldPlanOf(sleeping, k_Self, NumberRef, why).has_value());
	sleeping.idle.step = 2;
	const auto sleep = ScriptHeldPlanOf(sleeping, k_Self, NumberRef, why);
	ASSERT_TRUE(sleep.has_value()) << why;
	EXPECT_EQ(sleep->count, 5u);
	EXPECT_EQ(sleep->words[1], 3u);
	ASSERT_EQ(sleep->steps.size(), 3u);
	EXPECT_EQ(sleep->steps[0], (gamesave::AgendaStep {.action = 0x44}));
	EXPECT_EQ(sleep->steps[1], (gamesave::AgendaStep {.action = 0x84}));
	EXPECT_EQ(sleep->steps[2], (gamesave::AgendaStep {.action = 10, .integer = 63}));
	// with the home's height held, the walk home too: a yawn first at this draw, then the walk within five metres
	auto walkingHome = Forced(17, creature_mind::SleepAtHome({50.0f, 60.0f}, 10.0f, none, 7.0f));
	const auto home = ScriptHeldPlanOf(walkingHome, k_Self, NumberRef, why);
	ASSERT_TRUE(home.has_value()) << why;
	ASSERT_EQ(home->steps.size(), 5u);
	EXPECT_EQ(home->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 57}));
	EXPECT_EQ(home->steps[1], (gamesave::AgendaStep {.action = 8, .point = {50.0f, 7.0f, 60.0f}, .real = 5.0f}));
	// with no camera to face, openblack keeps no main step: not written
	EXPECT_FALSE(
	    planOf(64, creature_mind::HoldObject({.thing = 5, .handFull = true, .turnsPerSecond = 10.0f}, none)).has_value());
}

// Of the later lands' forced actions: hurling a thing (angry at random, picking it up, going where it can throw, the
// main throw) and dying for good (the main faint, then the long wait); one dead for ever is written from its first step
TEST(CreatureWriteScriptPlan, TheLaterLandsForcedActionsKeepTheGamesSubActions)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	const creature_mind::Random none = [](uint32_t) { return 0u; };
	std::string why;
	const auto hurl =
	    ScriptHeldPlanOf(Forced(15, creature_mind::HurlAt(6, 5, {1.0f, 2.0f}, 10.0f, false, none)), k_Self, NumberRef, why);
	ASSERT_TRUE(hurl.has_value()) << why;
	EXPECT_EQ(hurl->words[1], 3u);
	ASSERT_EQ(hurl->steps.size(), 4u);
	EXPECT_EQ(hurl->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 53}));
	EXPECT_EQ(hurl->steps[1], (gamesave::AgendaStep {.action = 0, .object = NumberRef(6)}));
	EXPECT_EQ(hurl->steps[2], (gamesave::AgendaStep {.action = 0x10, .object = NumberRef(5), .real = 10.0f}));
	EXPECT_EQ(hurl->steps[3], (gamesave::AgendaStep {.action = 5, .object = NumberRef(5)}));
	const auto die = ScriptHeldPlanOf(Forced(217, creature_mind::DiePermanently()), k_Self, NumberRef, why);
	ASSERT_TRUE(die.has_value()) << why;
	EXPECT_EQ(die->words[1], 0u);
	ASSERT_EQ(die->steps.size(), 2u);
	EXPECT_EQ(die->steps[0], (gamesave::AgendaStep {.action = 0xE, .integer = 106}));
	EXPECT_EQ(die->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 9999990}));
	auto deadForever = Forced(261, creature_mind::DeadForever());
	deadForever.idle.step = 1;
	const auto dead = ScriptHeldPlanOf(deadForever, k_Self, NumberRef, why);
	ASSERT_TRUE(dead.has_value()) << why;
	EXPECT_EQ(dead->current, 0u);
	ASSERT_EQ(dead->steps.size(), 2u);
	EXPECT_EQ(dead->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 8640000}));
	// looking at the hand, and setting down what it holds, a plan with no main step
	const auto look = ScriptHeldPlanOf(Forced(8, creature_mind::LookAtHand()), k_Self, NumberRef, why);
	ASSERT_TRUE(look.has_value()) << why;
	EXPECT_EQ(look->steps[0], (gamesave::AgendaStep {.action = 0x12}));
	const auto down = ScriptHeldPlanOf(Forced(67, creature_mind::PutDownHeld()), k_Self, NumberRef, why);
	ASSERT_TRUE(down.has_value()) << why;
	EXPECT_EQ(down->words[1], 0xFFFFFFFFu);
	EXPECT_EQ(down->steps[0], (gamesave::AgendaStep {.action = 1, .integer = 0x61}));
	// sleeping by a thing, past its walk to the clear place: a yawn at this draw, face down the slope, the main sleep
	auto byThing = Forced(
	    78, creature_mind::SleepByObject(glm::vec2 {3.0f, 4.0f}, 2.0f, std::nullopt, 10.0f, [](uint32_t) { return 0u; }));
	byThing.idle.step = 1;
	const auto sleep = ScriptHeldPlanOf(byThing, k_Self, NumberRef, why);
	ASSERT_TRUE(sleep.has_value()) << why;
	EXPECT_EQ(sleep->words[1], 3u);
	ASSERT_EQ(sleep->steps.size(), 4u);
	EXPECT_EQ(sleep->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 57}));
	EXPECT_EQ(sleep->steps[2], (gamesave::AgendaStep {.action = 0x84}));
	// a poo by a thing, and a fart, the same steps: the need-a-poo clip at this draw, right up to the thing, its back
	// turned on it, the main squat of the poo's loop for four seconds, the poo itself
	for (const uint32_t action : {19u, 177u})
	{
		const auto poo = ScriptHeldPlanOf(Forced(action, creature_mind::PooBy(5, none)), k_Self, NumberRef, why);
		ASSERT_TRUE(poo.has_value()) << why;
		EXPECT_EQ(poo->action, action);
		EXPECT_EQ(poo->words[1], 3u);
		ASSERT_EQ(poo->steps.size(), 5u);
		EXPECT_EQ(poo->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 66}));
		EXPECT_EQ(poo->steps[1], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 0.0f}));
		EXPECT_EQ(poo->steps[2], (gamesave::AgendaStep {.action = 0x16, .object = NumberRef(5)}));
		EXPECT_EQ(poo->steps[3], (gamesave::AgendaStep {.action = 4, .integer = 32, .real = 4.0f}));
		EXPECT_EQ(poo->steps[4], (gamesave::AgendaStep {.action = 0xF}));
	}
	// eating from a food pile: the walk to the pile's point at its height within the distance, face it a tenth of a
	// second, make a portion of wheat from it, the main eat of what it made; with no height held it is not written yet
	const auto pile =
	    ScriptHeldPlanOf(Forced(158, creature_mind::EatPortion(5, {30.0f, 40.0f}, 6.0f, 2.5f)), k_Self, NumberRef, why);
	ASSERT_TRUE(pile.has_value()) << why;
	EXPECT_EQ(pile->words[1], 3u);
	ASSERT_EQ(pile->steps.size(), 4u);
	EXPECT_EQ(pile->steps[0], (gamesave::AgendaStep {.action = 8, .point = {30.0f, 2.5f, 40.0f}, .real = 6.0f}));
	EXPECT_EQ(pile->steps[1], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 0.1f}));
	EXPECT_EQ(pile->steps[2], (gamesave::AgendaStep {.action = 0x8A, .object = NumberRef(5), .integer = 17}));
	EXPECT_EQ(pile->steps[3], (gamesave::AgendaStep {.action = 0x80}));
	EXPECT_FALSE(
	    ScriptHeldPlanOf(Forced(158, creature_mind::EatPortion(5, {30.0f, 40.0f}, 6.0f)), k_Self, NumberRef, why).has_value());
	// eating a thing alive: hungry at this draw, the pick-up, the main eat of the thing; holding a thing it can eat, the
	// eat alone
	const auto alive = ScriptHeldPlanOf(Forced(11, creature_mind::EatAlive(5, false, none)), k_Self, NumberRef, why);
	ASSERT_TRUE(alive.has_value()) << why;
	EXPECT_EQ(alive->words[1], 2u);
	ASSERT_EQ(alive->steps.size(), 3u);
	EXPECT_EQ(alive->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 54}));
	EXPECT_EQ(alive->steps[1], (gamesave::AgendaStep {.action = 0, .object = NumberRef(5)}));
	EXPECT_EQ(alive->steps[2], (gamesave::AgendaStep {.action = 2, .object = NumberRef(5)}));
	const auto holding = ScriptHeldPlanOf(Forced(11, creature_mind::EatAlive(5, true, none)), k_Self, NumberRef, why);
	ASSERT_TRUE(holding.has_value()) << why;
	EXPECT_EQ(holding->words[1], 0u);
	ASSERT_EQ(holding->steps.size(), 1u);
	// stomping on a thing and eating it: the stomp, the pick-up, the main eat
	const auto stomp = ScriptHeldPlanOf(Forced(13, creature_mind::StompAndEat(5, false)), k_Self, NumberRef, why);
	ASSERT_TRUE(stomp.has_value()) << why;
	EXPECT_EQ(stomp->words[1], 2u);
	ASSERT_EQ(stomp->steps.size(), 3u);
	EXPECT_EQ(stomp->steps[0], (gamesave::AgendaStep {.action = 0x11, .object = NumberRef(5)}));
	EXPECT_EQ(stomp->steps[1], (gamesave::AgendaStep {.action = 0, .object = NumberRef(5)}));
	EXPECT_EQ(stomp->steps[2], (gamesave::AgendaStep {.action = 2, .object = NumberRef(5)}));
	// raising a totem pole: near it within twelve metres, facing it a tenth of a second, the summoning clip, the main
	// raise of the pole
	const auto totem = ScriptHeldPlanOf(Forced(56, creature_town_help::MoveTotem(5, true)), k_Self, NumberRef, why);
	ASSERT_TRUE(totem.has_value()) << why;
	EXPECT_EQ(totem->words[1], 3u);
	ASSERT_EQ(totem->steps.size(), 4u);
	EXPECT_EQ(totem->steps[0], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 12.0f}));
	EXPECT_EQ(totem->steps[1], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 0.1f}));
	EXPECT_EQ(totem->steps[2], (gamesave::AgendaStep {.action = 10, .integer = 52}));
	EXPECT_EQ(totem->steps[3], (gamesave::AgendaStep {.action = 0x31, .object = NumberRef(5)}));
	// a lightning bolt's power-up: angry at this draw, near the thing within fifty metres, backed off its height and ten,
	// facing it a tenth of a second, the miracle's gesture, then the main cast from the pose at it for three seconds
	const auto bolt = ScriptHeldPlanOf(
	    Forced(99, creature_mind::CastAt(creature_mind::CastStyle::Lightning, 5, 7, 5, 10.0f, none)), k_Self, NumberRef, why);
	ASSERT_TRUE(bolt.has_value()) << why;
	ASSERT_EQ(bolt->steps.size(), 6u);
	EXPECT_EQ(bolt->words[1], 5u);
	EXPECT_EQ(bolt->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 53}));
	EXPECT_EQ(bolt->steps[1], (gamesave::AgendaStep {.action = 0x3D, .object = NumberRef(5), .real = 50.0f}));
	EXPECT_EQ(bolt->steps[2], (gamesave::AgendaStep {.action = 0x87, .object = NumberRef(5), .real = 20.0f}));
	EXPECT_EQ(bolt->steps[3], (gamesave::AgendaStep {.action = 6, .object = NumberRef(5), .real = 0.1f}));
	EXPECT_EQ(bolt->steps[4], (gamesave::AgendaStep {.action = 0x58, .integer = 7}));
	EXPECT_EQ(bolt->steps[5],
	          (gamesave::AgendaStep {.action = 0x27, .object = NumberRef(5), .integer = 41, .real = 3.0f, .spell = 5}));
	// a fireball's: written once past its walk to the clear place, the main cast still its last step
	auto fireball = Forced(96, creature_mind::CastFireballAt(9, 0, 5, true, {1.0f, 2.0f}, 10.0f, none));
	EXPECT_FALSE(ScriptHeldPlanOf(fireball, k_Self, NumberRef, why).has_value());
	fireball.idle.step = 1;
	const auto fire = ScriptHeldPlanOf(fireball, k_Self, NumberRef, why);
	ASSERT_TRUE(fire.has_value()) << why;
	EXPECT_EQ(fire->words[1], 4u);
	EXPECT_EQ(fire->steps.back(),
	          (gamesave::AgendaStep {.action = 0x27, .object = NumberRef(5), .integer = 41, .real = 3.0f, .spell = 9}));
}

// A static animation is played for its ten seconds; with no plan and nothing left to do the plan a finished one leaves
// is kept; any other plan is not written yet
TEST(CreatureWriteScriptPlan, AStaticPlayAnIdleCreatureAndAnyOtherPlan)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	const auto still = ScriptHeldPlanOf(PlayingForScript(20, 1), k_Self, NumberRef, why);
	ASSERT_TRUE(still.has_value());
	EXPECT_EQ(still->steps[0], (gamesave::AgendaStep {.action = 4, .integer = 20, .real = 10.0f}));
	const auto idle = ScriptHeldPlanOf(components::CreatureMindState {}, k_Self, NumberRef, why);
	ASSERT_TRUE(idle.has_value());
	EXPECT_EQ(idle->desire, 40u);
	EXPECT_EQ(idle->action, 0u);
	EXPECT_EQ(idle->count, 0u);
	EXPECT_TRUE(idle->steps.empty());
	auto walking = PlayingForScript(63, 1);
	walking.idle.activity = creature_mind::Activity::HangAround;
	EXPECT_FALSE(ScriptHeldPlanOf(walking, k_Self, NumberRef, why).has_value());
}

// A knocked-out creature a script holds keeps the Die action every faint is: the main faint, the lie's wait, the carry
// home for a player's creature, three seconds' wait, the wait for the spells and the rest it decided on; from the step
// and stage it is at as it falls and lies, and not yet once it is carried home
TEST(CreatureWriteScriptPlan, AKnockedOutCreatureKeepsTheDieActionsSteps)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	components::CreatureKnockedOut knockedOut {.lieTurns = 140, .rests = true};
	// falling: the faint started
	const auto falling = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(falling.has_value()) << why;
	EXPECT_EQ(falling->desire, 24u);
	EXPECT_EQ(falling->action, 9u);
	EXPECT_EQ(falling->objects[1], k_Self);
	EXPECT_EQ(falling->current, 0u);
	EXPECT_EQ(falling->state, 1u);
	EXPECT_EQ(falling->firstWord, 0u);
	EXPECT_EQ(falling->count, 6u);
	EXPECT_EQ(falling->words[1], 0u);
	ASSERT_EQ(falling->steps.size(), 6u);
	EXPECT_EQ(falling->steps[0], (gamesave::AgendaStep {.action = 0xE, .integer = 0x6A}));
	EXPECT_EQ(falling->steps[1], (gamesave::AgendaStep {.action = 0x18, .integer = 140}));
	EXPECT_EQ(falling->steps[2], (gamesave::AgendaStep {.action = 0x70}));
	EXPECT_EQ(falling->steps[3], (gamesave::AgendaStep {.action = 0x18, .integer = 30}));
	EXPECT_EQ(falling->steps[4], (gamesave::AgendaStep {.action = 0x8B}));
	EXPECT_EQ(falling->steps[5], (gamesave::AgendaStep {.action = 0x82}));
	// seen lying: the faint's last stage
	knockedOut.seenLying = true;
	EXPECT_EQ(KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why)->state, 2u);
	// counting down: the wait, with the turns left; a neutral one has no carry home, and one not worn out no rest
	knockedOut.lieCounting = true;
	knockedOut.lieTurns = 90;
	knockedOut.rests = false;
	const auto lying = KnockedOutPlanOf(knockedOut, k_Self, k_Self, false, why);
	ASSERT_TRUE(lying.has_value()) << why;
	EXPECT_EQ(lying->current, 1u);
	EXPECT_EQ(lying->state, 1u);
	EXPECT_EQ(lying->count, 4u);
	ASSERT_EQ(lying->steps.size(), 3u);
	EXPECT_EQ(lying->steps[0], (gamesave::AgendaStep {.action = 0x18, .integer = 90}));
	// its turns out: the carry home next, not yet started
	knockedOut.lieTurns = 0;
	const auto due = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(due.has_value()) << why;
	EXPECT_EQ(due->current, 2u);
	EXPECT_EQ(due->state, 0u);
	EXPECT_EQ(due->firstWord, 1u);
	EXPECT_EQ(due->steps[0], (gamesave::AgendaStep {.action = 0x70}));
	// carried home: not written yet
	knockedOut.stage = components::CreatureKnockedOut::Stage::FadingOut;
	EXPECT_FALSE(KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why).has_value());
}

// A creature's dance with villagers is written as the game keeps the dance's fourteen steps, from the step it is at: the
// villager it goes near and faces, the summoning clip, the dance's place at the land's height there and its row, the
// fewest dancers, the walk to the place and the point at it, and the turns to dance; the load reads the same dance back
// from the steps left. An agenda of other steps is not a dance
TEST(CreatureWriteScriptPlan, ADanceWithVillagersIsWrittenAsTheGamesFourteenStepsAndReadBack)
{
	const auto agenda = creature_dance::Agenda(
	    {.villager = 42, .place = {1200.0f, 1300.0f}, .type = 10, .fewestDancers = 4, .beats = 6, .height = 8.0f});
	const auto groundAt = [](glm::vec2) { return 25.0f; };
	const auto refOf = [](uint32_t thing) { return gamesave::Ref {.number = thing + 1000}; };
	const auto all = DanceStepsOf(agenda, 0, groundAt, refOf);
	ASSERT_TRUE(all.has_value());
	ASSERT_EQ(all->size(), 14u);
	EXPECT_EQ(all->at(0).action, 0x1Du);
	EXPECT_EQ(all->at(1), (gamesave::AgendaStep {.action = 0x3D, .object = {.number = 1042}, .real = 40.0f}));
	EXPECT_EQ(all->at(2).action, 0x06u);
	EXPECT_EQ(all->at(2).object.number, 1042u);
	EXPECT_EQ(all->at(3), (gamesave::AgendaStep {.action = 0x0A, .integer = 0x34}));
	EXPECT_EQ(all->at(4), (gamesave::AgendaStep {.action = 0x7E, .integer = 10, .point = {1200.0f, 25.0f, 1300.0f}}));
	EXPECT_EQ(all->at(5), (gamesave::AgendaStep {.action = 0x0C, .object = {.number = 1042}, .integer = 4}));
	EXPECT_EQ(all->at(7), (gamesave::AgendaStep {.action = 0x08, .point = {1200.0f, 25.0f, 1300.0f}}));
	EXPECT_EQ(all->at(8), (gamesave::AgendaStep {.action = 0x50, .point = {1200.0f, 25.0f, 1300.0f}, .real = 1.0f}));
	EXPECT_EQ(all->at(13), (gamesave::AgendaStep {.action = 0x17, .integer = 30}));
	gamesave::KeptCreaturePlan plan {.desire = 0, .action = 294, .count = 14, .steps = *all};
	const auto read = ecs::saved_world_load::DanceOf(plan);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->place, glm::vec2(1200.0f, 1300.0f));
	EXPECT_EQ(read->type, 10u);
	EXPECT_EQ(read->fewestDancers, 4u);
	EXPECT_EQ(read->danceTurns, 30u);
	EXPECT_EQ(read->nearDistance, 40.0f);
	const auto left = DanceStepsOf(agenda, 10, groundAt, refOf);
	ASSERT_TRUE(left.has_value());
	ASSERT_EQ(left->size(), 4u);
	EXPECT_EQ(left->front().action, 0x19u);
	plan.current = 10;
	plan.steps = *left;
	EXPECT_EQ(ecs::saved_world_load::DanceOf(plan),
	          (ecs::saved_world_load::KeptDance {.action = 294, .step = 10, .danceTurns = 30}));
	auto other = agenda;
	other[4].dance.kind = creature_mind::DanceOrder::Kind::Summon;
	EXPECT_FALSE(DanceStepsOf(other, 0, groundAt, refOf).has_value());
	plan.steps.front().action = 0x18;
	EXPECT_FALSE(ecs::saved_world_load::DanceOf(plan).has_value());
}

// A creature in its duel keeps its fight's plan: the one that started it (it made the arena) at the main step of the
// fight's eighteen, the fight, then the order of the response after it to its opponent, its own response and the
// opponent let go; the opponent obeying it, its fight and then a wait for its next order. Who fights for each, as the
// game counts it
TEST(CreatureWriteScriptPlan, ACreatureInItsDuelKeepsItsFightsPlan)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr gamesave::Ref k_Opponent {.number = 9};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 21, .object = 3};
	std::string why;
	const auto starter = DuelPlanOf(&mind, true, k_Self, k_Opponent, why);
	ASSERT_TRUE(starter.has_value()) << why;
	EXPECT_EQ(starter->desire, static_cast<uint32_t>(creature_desires::Desire::Anger));
	EXPECT_EQ(starter->action, 21u);
	EXPECT_EQ(starter->objects[1], k_Opponent);
	EXPECT_EQ(starter->count, 18u);
	EXPECT_EQ(starter->current, 14u);
	EXPECT_EQ(starter->state, 1u);
	EXPECT_EQ(starter->words[1], 14u);
	EXPECT_EQ(starter->other, k_Opponent);
	ASSERT_EQ(starter->steps.size(), 4u);
	EXPECT_EQ(starter->steps[0], (gamesave::AgendaStep {.action = 0x1C, .object = k_Opponent}));
	EXPECT_EQ(starter->steps[1],
	          (gamesave::AgendaStep {.action = 0x4A, .object = k_Self, .other = k_Opponent, .orderKind = 0x48}));
	EXPECT_EQ(starter->steps[2], (gamesave::AgendaStep {.action = 0x48, .object = k_Opponent}));
	EXPECT_EQ(starter->steps[3], (gamesave::AgendaStep {.action = 0x4D, .object = k_Opponent}));
	const auto obeying = DuelPlanOf(nullptr, false, k_Opponent, k_Self, why);
	ASSERT_TRUE(obeying.has_value()) << why;
	EXPECT_EQ(obeying->desire, 26u);
	EXPECT_EQ(obeying->action, 124u);
	EXPECT_EQ(obeying->objects[0], k_Self);
	EXPECT_EQ(obeying->count, 2u);
	EXPECT_EQ(obeying->words[1], 0xFFFFFFFFu);
	ASSERT_EQ(obeying->steps.size(), 2u);
	EXPECT_EQ(obeying->steps[0], (gamesave::AgendaStep {.action = 0x1C, .object = k_Self}));
	EXPECT_EQ(obeying->steps[1], (gamesave::AgendaStep {.action = 0x4E, .object = k_Self}));
	// a starter whose mind holds no fight is not written yet
	EXPECT_FALSE(DuelPlanOf(nullptr, true, k_Self, k_Opponent, why).has_value());
	// one that told its opponent to go away, the fight's agenda its plan's, keeps that plan with the fight's steps
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 256, .object = 3};
	const auto goAway = DuelPlanOf(&mind, true, k_Self, k_Opponent, why);
	ASSERT_TRUE(goAway.has_value()) << why;
	EXPECT_EQ(goAway->action, 256u);
	EXPECT_EQ(goAway->count, 18u);
	EXPECT_EQ(goAway->steps, starter->steps);
	// any other plan is not one a fight is made under
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 11, .object = 3};
	EXPECT_FALSE(DuelPlanOf(&mind, true, k_Self, k_Opponent, why).has_value());
	// who fights: the computer, the player with the computer waiting, nobody
	creature_fight::Fighter fighter;
	EXPECT_EQ(AutoFightOf(fighter), 2u);
	fighter.control = creature_fight::Control::Player;
	EXPECT_EQ(AutoFightOf(fighter), 1u);
	fighter.control = creature_fight::Control::None;
	EXPECT_EQ(AutoFightOf(fighter), 0u);
	fighter.autoFight = true;
	EXPECT_EQ(AutoFightOf(fighter), 2u);
}
