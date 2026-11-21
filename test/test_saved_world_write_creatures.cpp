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

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
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
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

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
#include "Creature/CreatureRestAndHome.h"
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
#include "ECS/Components/Physics.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureLoop.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectFlags.h"
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

/// The record a creature's own values make, read back, in a stream after the objects `start` counts already read
gamesave::CreatureFields ReadBack(const gamesave::CreatureFields& fields,
                                  gamesave::StreamStart start = {.objects = 100, .checksum = 0x27000})
{
	gamesave::CreatureRecord record;
	record.creature = fields;
	gamesave::StreamWriter writer(start);
	gamesave::Write(writer, gamesave::SavedRecord {record}, nullptr);
	const auto bytes = writer.Take();
	gamesave::StreamReader reader(bytes, start);
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

// A loaded creature that held a miracle while it cast holds again the miracle the load made from the number it was
// saved naming, and none when that miracle was not made (docs/wiki/engine/saved-games.md)
TEST_F(CreatureWrite, ALoadedCreatureHoldsAgainTheMiracleItHeldWhileItCast)
{
	const auto creature = Reg().Create();
	Reg().Assign<components::CreatureSpells>(creature);
	components::LoadedSpellSlots saved;
	saved.held = 43;
	Reg().Assign<components::LoadedSpellSlots>(creature, saved);
	const auto miracle = Reg().Create();
	ecs::saved_world_load::LinkSpellSlots(Reg(), {{43u, miracle}});
	ASSERT_TRUE(Reg().AllOf<components::CreatureCasting>(creature));
	EXPECT_EQ(Reg().Get<components::CreatureCasting>(creature).held, miracle);
	EXPECT_FALSE(Reg().AllOf<components::LoadedSpellSlots>(creature));
	const auto other = Reg().Create();
	Reg().Assign<components::CreatureSpells>(other);
	components::LoadedSpellSlots lost;
	lost.held = 44;
	Reg().Assign<components::LoadedSpellSlots>(other, lost);
	ecs::saved_world_load::LinkSpellSlots(Reg(), {{43u, miracle}});
	EXPECT_FALSE(Reg().AllOf<components::CreatureCasting>(other));
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

// A creature in a fight with no opponent stops the write, naming it: its fight's plan has nobody to fight
TEST_F(CreatureWrite, ACreatureWhoseValuesHaveNoSourceYetStopsTheWrite)
{
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	Reg().Assign<components::CreatureFighting>(creature);
	std::string failure;
	EXPECT_FALSE(Written(creature, &failure).has_value());
	EXPECT_NE(failure.find("no opponent"), std::string::npos) << failure;
}

// A creature with spells waiting for another of their kind to finish writes them in the order they came: each spell, the
// seconds it was cast for and its miracle (none once the miracle has gone); the turns a cast holds for those seconds are
// the turns it waits with, as the load gives them back
TEST_F(CreatureWrite, ACreaturesSpellsWaitingAreWrittenWithTheirSecondsAndMiracles)
{
	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	const auto miracle = Reg().Create();
	const auto gone = Reg().Create();
	auto& spells = Reg().Assign<components::CreatureSpells>(creature).spells;
	spells.waiting.push_back({.spell = creature_spells::Spell::Thin, .holdTurns = 40, .miracle = miracle, .seconds = 4.0f});
	spells.waiting.push_back({.spell = creature_spells::Spell::Strong, .holdTurns = 25, .miracle = gone, .seconds = 2.5f});
	Reg().Destroy(gone);
	std::string failure;
	const auto fields = Written(creature, &failure);
	ASSERT_TRUE(fields.has_value()) << failure;
	ASSERT_EQ(fields->queued.size(), 2u);
	EXPECT_EQ(fields->queued[0], (gamesave::QueuedCreatureSpell {.spell = static_cast<uint32_t>(creature_spells::Spell::Thin),
	                                                             .seconds = 4.0f,
	                                                             .miracle = {.number = _context->Reference(miracle)}}));
	EXPECT_EQ(fields->queued[1], (gamesave::QueuedCreatureSpell {.spell = static_cast<uint32_t>(creature_spells::Spell::Strong),
	                                                             .seconds = 2.5f}));
	EXPECT_EQ(creature_spells::TurnsOf(4.0f, 1.0f / game_clock::k_TurnSeconds), 40);
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

// A creature the physics moves is written as any object there: its place marked in the physics and its object's body
// (one standing still while the physics holds none for it), and it reads back so, where the write used to stop on it
TEST_F(CreatureWrite, ACreatureInThePhysicsIsWrittenWithItsPlaceThere)
{
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	Locator::infoConstants::emplace(*info);
	RegisterCreatureWriters(_writers);

	const auto creature = Creature(PlayerNames::PLAYER_ONE);
	Reg().Get<components::Creature>(creature).species = CreatureType::Cow;
	Reg().Assign<components::Transform>(creature, glm::vec3(100.0f, 30.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	Reg().Assign<components::InPhysics>(creature);

	_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
	_context.emplace(_writers, *_source);
	const auto made = _context->RecordOf(_context->Reference(creature));
	ASSERT_NE(made.record, nullptr) << _context->Failure();
	const auto* record = std::get_if<gamesave::CreatureRecord>(made.record.get());
	ASSERT_NE(record, nullptr);
	EXPECT_NE(record->position.flags & gamesave::k_PositionFlagInPhysics, 0);
	EXPECT_EQ(record->position.position.x, map_coords::ToFixed(100.0f));
	EXPECT_EQ(record->position.position.z, map_coords::ToFixed(200.0f));

	// read back: still in the physics, with a body standing still
	constexpr gamesave::StreamStart k_After100 {.objects = 100, .checksum = 0x27000};
	gamesave::StreamWriter writer(k_After100);
	gamesave::Write(writer, gamesave::SavedRecord {*record}, nullptr);
	const auto bytes = writer.Take();
	gamesave::StreamReader reader(bytes, k_After100);
	uint32_t type = 0;
	uint32_t extra = 0;
	reader.Get(type);
	reader.Get(extra);
	gamesave::SavedRecord read;
	ASSERT_TRUE(gamesave::ReadRecord(reader, type, extra, read, nullptr));
	const auto* again = std::get_if<gamesave::CreatureRecord>(&read);
	ASSERT_NE(again, nullptr);
	EXPECT_NE(again->position.flags & gamesave::k_PositionFlagInPhysics, 0);
	ASSERT_TRUE(again->object.physics.has_value());
	EXPECT_EQ(again->object.physics->velocity, (gamesave::Point {0.0f, 0.0f, 0.0f}));
}

// A loaded creature the physics moved at the save is made, and its resolve leaves it as the game's does. No creature
// can have a body in the physics: once its mind file is read it is marked in the physics with none, out of the map,
// its previous state none; an immovable one stays on the map, out of the physics; one whose mind file is not there
// stays in the physics as its record says. Written again, it is in the physics with a body standing still, whatever
// body its record held. One held or flown by the particles is still not made
TEST_F(CreatureWrite, ALoadedCreatureInThePhysicsIsLeftThereWithNoBodyAndWrittenSo)
{
	namespace load = ecs::saved_world_load;
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	auto info = std::make_unique<InfoConstants>();
	Locator::infoConstants::emplace(*info);
	RegisterCreatureWriters(_writers);

	// of no player and no place in the list of creatures, so it names no mind file
	gamesave::CreatureRecord saved;
	saved.position.flags = gamesave::k_PositionFlagInPhysics;
	saved.object.physics = gamesave::PhysicsPart {
	    .matrix = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 500.0f, 30.0f, 600.0f},
	    .velocity = {1.0f, -2.0f, 3.0f},
	    .spin = {0.0f, 0.5f, 0.0f},
	};
	saved.living.action = {0, 0, 7, 0, 0, 0};

	// made by the load; one flown by the particles, or held, is not
	const load::PlannedCreature planned {.species = CreatureType::Cow, .record = saved};
	EXPECT_EQ(load::WhyCreatureNotMade(planned), nullptr);
	auto flown = planned;
	flown.record.thing.flags = gamesave::k_ThingFlagCarriedByParticles;
	flown.record.object.physics.reset();
	EXPECT_NE(load::WhyCreatureNotMade(flown), nullptr);
	auto held = planned;
	// a place that may not change: held
	held.record.position.flags = static_cast<uint16_t>(held.record.position.flags | 0x04U);
	EXPECT_NE(load::WhyCreatureNotMade(held), nullptr);

	const auto loaded = [this, &saved](bool immovable) {
		const auto creature = Creature(PlayerNames::PLAYER_ONE);
		Reg().Get<components::Creature>(creature).species = CreatureType::Cow;
		Reg().Assign<components::Transform>(creature, glm::vec3(100.0f, 30.0f, 200.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<components::LoadedRecord<gamesave::CreatureRecord>>(creature, saved);
		if (immovable)
		{
			Reg().Assign<components::Immovable>(creature);
		}
		ecs::map_cells::InsertMapObject(creature);
		return creature;
	};
	const auto keptStates = [this](entt::entity creature) {
		return Reg().Get<const components::LoadedRecord<gamesave::CreatureRecord>>(creature).record.living.action;
	};

	// its mind file read: in the physics with no body, out of the map, its previous state none
	const auto read = loaded(false);
	load::ResolveCreatureInThePhysics(read, true);
	EXPECT_TRUE(Reg().AllOf<components::InPhysics>(read));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(read));
	EXPECT_EQ(keptStates(read), (std::array<uint8_t, 6> {0, 0, 0, 0, 0, 0}));

	// immovable: its previous state none, but on the map as it was and out of the physics
	const auto fixed = loaded(true);
	const bool filed = ecs::map_cells::IsObjectInMap(fixed);
	load::ResolveCreatureInThePhysics(fixed, true);
	EXPECT_FALSE(Reg().AllOf<components::InPhysics>(fixed));
	EXPECT_EQ(ecs::map_cells::IsObjectInMap(fixed), filed);
	EXPECT_EQ(keptStates(fixed), (std::array<uint8_t, 6> {0, 0, 0, 0, 0, 0}));

	// its mind file not there (the resolve itself, which logs it): in the physics as its record says, out of the map,
	// its states kept
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const auto unread = loaded(false);
	const std::array resolves {unread};
	load::Resolve(resolves, {});
	EXPECT_TRUE(Reg().AllOf<components::InPhysics>(unread));
	EXPECT_FALSE(ecs::map_cells::IsObjectInMap(unread));
	EXPECT_EQ(keptStates(unread), saved.living.action);

	// written again: in the physics, with a body standing still, and its states as the resolve left them
	_source.emplace(WorldSource {.registry = Reg(), .landscape = "Land1.lnd"});
	_context.emplace(_writers, *_source);
	const auto written = (*_writers.WriterOf(gamesave::CreatureRecord::k_Type))(*_context, read);
	ASSERT_TRUE(written.has_value()) << _context->Failure();
	const auto& record = std::get<gamesave::CreatureRecord>(*written);
	EXPECT_NE(record.position.flags & gamesave::k_PositionFlagInPhysics, 0);
	EXPECT_EQ(record.object.physics, std::optional {gamesave::PhysicsPart {}});
	EXPECT_EQ(record.living.action, keptStates(read));
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

// The load reads a creature's own plain sleep back, with the yawn or not and from the step it was at, and the agenda it
// makes again is written as the same plan; a sleep forced on it is not its own
TEST(CreatureWriteScriptPlan, ACreaturesOwnPlainSleepIsReadBackAndWrittenTheSame)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr glm::vec2 k_Home {600.0f, 700.0f};
	for (const uint32_t draw : {0u, 1u})
	{
		components::CreatureMindState mind;
		mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Tiredness, .action = 313};
		mind.idle.agenda = creature_mind::SleepOnTheSpot(std::nullopt, 2.0f, k_Home, 3.0f, [draw](uint32_t) { return draw; });
		std::string why;
		const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(plan.has_value()) << why;
		const auto read = ecs::saved_world_load::OwnSleepOf(*plan);
		ASSERT_TRUE(read.has_value());
		EXPECT_EQ(read->desire, static_cast<uint32_t>(creature_desires::Desire::Tiredness));
		EXPECT_EQ(read->count, mind.idle.agenda.size());
		EXPECT_EQ(read->home, glm::vec3(600.0f, 0.0f, 700.0f));
		EXPECT_EQ(read->within, 3.0f);
		components::CreatureMindState loaded;
		loaded.planner.current = mind.planner.current;
		loaded.idle.agenda = ecs::saved_world_load::OwnSleepAgendaOf(*read, {0.0f, 0.0f});
		EXPECT_EQ(ScriptHeldPlanOf(loaded, k_Self, NumberRef, why), plan) << why;

		// once at the sleep, the walk done, to the home it keeps
		mind.idle.step = mind.idle.agenda.size() - 2;
		mind.idle.stepStarted = true;
		const auto asleep = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(asleep.has_value()) << why;
		const auto readAsleep = ecs::saved_world_load::OwnSleepOf(*asleep);
		ASSERT_TRUE(readAsleep.has_value());
		EXPECT_EQ(readAsleep->step, mind.idle.step);
		EXPECT_FALSE(readAsleep->home.has_value());
		loaded.idle.agenda = ecs::saved_world_load::OwnSleepAgendaOf(*readAsleep, k_Home);
		loaded.idle.step = readAsleep->step;
		loaded.idle.stepStarted = true;
		EXPECT_EQ(ScriptHeldPlanOf(loaded, k_Self, NumberRef, why), asleep) << why;
	}
	const gamesave::KeptCreaturePlan forced {.desire = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer),
	                                         .action = 313,
	                                         .current = 0,
	                                         .count = 4,
	                                         .words = {0, 2, 0},
	                                         .steps = {{.action = 8}, {.action = 0x44}, {.action = 0x84}, {.action = 10}}};
	EXPECT_FALSE(ecs::saved_world_load::OwnSleepOf(forced).has_value());
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

// The load reads a creature's own fishing back at the walk to the shoal or the making still to come, and the agenda it
// makes again is written as the same plan; once the food is made, or forced on it, it is not read
TEST(CreatureWriteScriptPlan, ACreaturesOwnFishingIsReadBackAndWrittenTheSameUntilTheFoodIsMade)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 155};
	mind.idle.agenda = creature_mind::FishAndEat({600.0f, 700.0f}, 15.0f, 2.5f);
	std::string why;
	for (const size_t step : {size_t {0}, size_t {1}})
	{
		mind.idle.step = step;
		const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(plan.has_value()) << why;
		const auto read = ecs::saved_world_load::FishAndEatOf(*plan);
		ASSERT_TRUE(read.has_value()) << "step " << step;
		EXPECT_EQ(read->desire, static_cast<uint32_t>(creature_desires::Desire::Hunger));
		EXPECT_EQ(read->step, step);
		EXPECT_EQ(read->shoal.has_value(), step == 0);
		const auto shoal = read->shoal.value_or(glm::vec3(0.0f));
		components::CreatureMindState loaded;
		loaded.planner.current = mind.planner.current;
		loaded.idle.agenda = creature_mind::FishAndEat({shoal.x, shoal.z}, read->within, shoal.y);
		loaded.idle.step = read->step;
		EXPECT_EQ(ScriptHeldPlanOf(loaded, k_Self, NumberRef, why), plan) << "step " << step;
	}
	// the pick-up of the food made, and the eat of it
	mind.idle.stepStarted = true;
	EXPECT_FALSE(ecs::saved_world_load::FishAndEatOf(*ScriptHeldPlanOf(mind, k_Self, NumberRef, why)).has_value());
	mind.idle.step = 2;
	mind.idle.stepStarted = false;
	EXPECT_FALSE(ecs::saved_world_load::FishAndEatOf(*ScriptHeldPlanOf(mind, k_Self, NumberRef, why)).has_value());
	mind.idle.step = 0;
	auto forced = *ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	forced.desire = static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer);
	EXPECT_FALSE(ecs::saved_world_load::FishAndEatOf(forced).has_value());
}

// A creature a script holds while it prays at its temple keeps that plan as the game's: the walk home to within its
// height, its point on the land, the main step, then the prayer's clip; the load reads the same plan back
TEST(CreatureWriteScriptPlan, AHeldCreaturePrayingAtItsTempleKeepsItsPlanAndTheLoadReadsItBack)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = static_cast<creature_desires::Desire>(17), .action = 319, .forced = true};
	mind.idle.agenda = creature_mind::PrayAtTemple({600.0f, 700.0f}, 12.0f);
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, 17u);
	EXPECT_EQ(plan->action, 319u);
	EXPECT_EQ(plan->count, 2u);
	EXPECT_EQ(plan->words[1], 0u);
	ASSERT_EQ(plan->steps.size(), 2u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 8, .point = {600.0f, 0.0f, 700.0f}, .real = 12.0f}));
	EXPECT_EQ(plan->steps[1], (gamesave::AgendaStep {.action = 10, .integer = 0xDD}));
	EXPECT_EQ(
	    ecs::saved_world_load::PrayAtTempleOf(*plan),
	    (ecs::saved_world_load::KeptPrayAtTemple {.desire = 17, .home = glm::vec3(600.0f, 0.0f, 700.0f), .within = 12.0f}));
	// at the prayer, once home
	mind.idle.step = 1;
	const auto praying = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(praying.has_value()) << why;
	ASSERT_EQ(praying->steps.size(), 1u);
	const auto read = ecs::saved_world_load::PrayAtTempleOf(*praying);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->step, 1u);
}

// A creature a script holds that eats a thing alive keeps that plan as the game's: the hungry clip or not, the pick-up of
// the thing and the eat of it, the main step, or the eat alone when it held something to eat; the load reads the same
// plan back, from the step it was at
TEST(CreatureWriteScriptPlan, AHeldCreatureEatingAThingAliveKeepsItsPlanAndTheLoadReadsItBack)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr uint32_t k_Food = 5;
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::Hunger, .action = 11, .object = k_Food, .forced = true};
	mind.idle.agenda = creature_mind::EatAlive(k_Food, false, [](uint32_t) { return 0u; });
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::Hunger));
	EXPECT_EQ(plan->action, 11u);
	EXPECT_EQ(plan->count, 3u);
	EXPECT_EQ(plan->words[1], 2u);
	ASSERT_EQ(plan->steps.size(), 3u);
	EXPECT_EQ(plan->steps[0], (gamesave::AgendaStep {.action = 10, .integer = 0x36}));
	EXPECT_EQ(plan->steps[1], (gamesave::AgendaStep {.action = 0, .object = NumberRef(k_Food)}));
	EXPECT_EQ(plan->steps[2], (gamesave::AgendaStep {.action = 2, .object = NumberRef(k_Food)}));
	EXPECT_EQ(ecs::saved_world_load::EatAliveOf(*plan),
	          (ecs::saved_world_load::KeptEatAlive {.desire = static_cast<uint32_t>(creature_desires::Desire::Hunger),
	                                                .objects = {0, NumberRef(k_Food).number, 0},
	                                                .count = 3}));
	// with no hungry clip, at the eat once it has picked the thing up
	mind.idle.agenda = creature_mind::EatAlive(k_Food, false, [](uint32_t) { return 1u; });
	mind.idle.step = 1;
	mind.idle.stepStarted = true;
	const auto eating = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(eating.has_value()) << why;
	EXPECT_EQ(eating->count, 2u);
	EXPECT_EQ(eating->current, 1u);
	EXPECT_EQ(eating->state, 1u);
	ASSERT_EQ(eating->steps.size(), 1u);
	EXPECT_EQ(eating->steps[0].action, 2u);
	const auto read = ecs::saved_world_load::EatAliveOf(*eating);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->step, 1u);
	EXPECT_EQ(read->count, 2u);
	// holding something to eat, the eat alone
	mind.idle.agenda = creature_mind::EatAlive(k_Food, true, [](uint32_t) { return 0u; });
	mind.idle.step = 0;
	mind.idle.stepStarted = false;
	const auto holding = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(holding.has_value()) << why;
	EXPECT_EQ(holding->count, 1u);
	EXPECT_EQ(holding->words[1], 0u);
	EXPECT_TRUE(ecs::saved_world_load::EatAliveOf(*holding).has_value());
	// another plan's steps are not read as it
	auto other = *plan;
	other.action = 12;
	EXPECT_FALSE(ecs::saved_world_load::EatAliveOf(other).has_value());
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

// The load reads a creature's look over a miracle's place back, pointing first or not and from every step it can be at,
// and the agenda it makes again is written as the same plan; another row's plan is not one
TEST(CreatureWriteScriptPlan, ALookOverAPlaceIsReadBackAndWrittenTheSame)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	for (const uint32_t draw : {0u, 1u})
	{
		components::CreatureMindState mind;
		mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Curiosity, .action = 157};
		mind.idle.agenda = creature_mind::ExamineMiracle({600.0f, 700.0f}, 3.0f, [draw](uint32_t) { return draw; });
		for (size_t step = 0; step < mind.idle.agenda.size(); ++step)
		{
			mind.idle.step = step;
			std::string why;
			const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(plan.has_value()) << why;
			const auto read = ecs::saved_world_load::ExaminePlaceOf(*plan);
			ASSERT_TRUE(read.has_value()) << "draw " << draw << ", step " << step;
			EXPECT_EQ(read->step, step);
			EXPECT_EQ(read->count, mind.idle.agenda.size());
			components::CreatureMindState loaded;
			loaded.planner.current = mind.planner.current;
			loaded.idle.agenda = ecs::saved_world_load::ExaminePlaceAgendaOf(*read);
			loaded.idle.step = read->step;
			EXPECT_EQ(ScriptHeldPlanOf(loaded, k_Self, NumberRef, why), plan) << "draw " << draw << ", step " << step;
		}
		std::string why;
		mind.idle.step = 0;
		auto other = *ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		other.action = 156;
		EXPECT_FALSE(ecs::saved_world_load::ExaminePlaceOf(other).has_value());
	}
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

namespace
{
/// A kept plan written in a creature's record and read back by the codec, after the objects its things are numbered
/// among (NumberRef's), so they are read as references to them
gamesave::KeptCreaturePlan PlanReadBack(const gamesave::KeptCreaturePlan& plan)
{
	gamesave::CreatureFields fields;
	fields.keepsPlan = 1;
	fields.plan = plan;
	return ReadBack(fields, {.objects = 200, .checksum = 0x27000}).plan;
}
} // namespace

// Being idle, as a script holds a creature on it: written at its second wait with the waits it drew, read back by the
// codec, taken up by the load as the same forced plan at the same step with each wait left as long as the save keeps it
// in turns, and written back as read; with other steps it is not taken up
TEST(CreatureWriteScriptPlan, BeingIdleIsLoadedWithItsWaitsAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	auto agenda = creature_mind::BeIdle([](uint32_t) { return 0u; });
	agenda[0].seconds = 1.37f;
	agenda[2].seconds = 1.84f;
	auto mind = Forced(161, std::move(agenda));
	mind.idle.step = 2;
	const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(written.has_value()) << why;
	ASSERT_EQ(written->steps.size(), 2u);
	EXPECT_EQ(written->steps[0], (gamesave::AgendaStep {.action = 0x18, .integer = 18}));

	const auto read = PlanReadBack(*written);
	EXPECT_EQ(read, *written);
	EXPECT_EQ(ecs::saved_world_load::ForcedPlanOf(read),
	          (ecs::saved_world_load::KeptForcedPlan {.action = 161, .objects = {0, 105, 0}, .step = 2, .count = 4}));
	const auto loaded = ecs::saved_world_load::IdleAgendaOf(read);
	ASSERT_TRUE(loaded.has_value());
	ASSERT_EQ(loaded->size(), 4u);
	EXPECT_EQ(game_clock::TicksForSeconds((*loaded)[2].seconds), 18);
	EXPECT_EQ((*loaded)[3].animation, mind.idle.agenda[3].animation);

	auto again = Forced(161, *loaded);
	again.idle.step = 2;
	const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
	ASSERT_TRUE(rewritten.has_value()) << why;
	EXPECT_EQ(*rewritten, read);

	// every whole number of turns a wait may be saved with comes back as those turns
	for (uint32_t turns = 0; turns < 400; ++turns)
	{
		EXPECT_EQ(game_clock::TicksForSeconds(ecs::saved_world_load::SecondsOfTurns(turns)), static_cast<int32_t>(turns));
	}
	auto odd = read;
	odd.steps[1].action = 0x18;
	EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(odd).has_value());
	EXPECT_FALSE(ecs::saved_world_load::IdleAgendaOf(odd).has_value());
}

// Pointing at a thing, as a script holds a creature on it: written, read back by the codec, taken up by the load as the
// same forced plan done to the same thing, its agenda the one the action makes for that thing, and written back as
// read; a point at another thing than the plan's is not taken up
TEST(CreatureWriteScriptPlan, PointingAtAThingIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	const auto written = ScriptHeldPlanOf(Forced(163, creature_mind::PointAtThing(5)), k_Self, NumberRef, why);
	ASSERT_TRUE(written.has_value()) << why;
	const auto read = PlanReadBack(*written);
	EXPECT_EQ(read, *written);
	const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
	EXPECT_EQ(loaded, (ecs::saved_world_load::KeptForcedPlan {.action = 163, .objects = {0, 105, 0}, .step = 0, .count = 1}));
	ASSERT_TRUE(loaded.has_value());
	const auto rewritten = ScriptHeldPlanOf(Forced(163, creature_mind::PointAtThing(5)), k_Self, NumberRef, why);
	ASSERT_TRUE(rewritten.has_value()) << why;
	EXPECT_EQ(*rewritten, read);
	auto elsewhere = read;
	elsewhere.steps.front().object = NumberRef(6);
	EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(elsewhere).has_value());
}

// Looking at a thing for ever, as a script holds a creature on it: written at its turn to the thing and at its long wait,
// read back by the codec, taken up by the load as the same forced plan at the same step, its agenda the one the action
// makes for that thing, and written back as read; a turn to another thing than the plan's is not taken up
TEST(CreatureWriteScriptPlan, LookingAtAThingForEverIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const size_t step : {size_t {0}, size_t {1}})
	{
		auto mind = Forced(189, creature_mind::LookForever(5));
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		EXPECT_EQ(loaded,
		          (ecs::saved_world_load::KeptForcedPlan {.action = 189, .objects = {0, 105, 0}, .step = step, .count = 2}));
		ASSERT_TRUE(loaded.has_value());
		auto again = Forced(189, creature_mind::LookForever(5));
		again.idle.step = loaded->step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read);
		if (step == 0)
		{
			auto elsewhere = read;
			elsewhere.steps.front().object = NumberRef(6);
			EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(elsewhere).has_value());
		}
	}
}

// Being frightened on the spot, as a script holds a creature on it: written at each of its three steps, read back by the
// codec, taken up by the load as the same forced plan at the same step, its agenda the one the action makes, and
// written back as read; with other steps it is not taken up
namespace
{
/// A plan of a row forced on a creature, the agenda the action makes for the thing 5, written at each of its steps, read
/// back by the codec, taken up by the load as the same forced plan at the same step, and written back as read from the
/// agenda the action makes again
void ExpectForcedRowRoundTrip(uint32_t action, const std::function<std::vector<creature_mind::Step>()>& make,
                              const std::function<void(components::CreatureMindState&)>& setUp = {})
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	const auto count = make().size();
	for (size_t step = 0; step < count; ++step)
	{
		auto mind = Forced(action, make());
		if (setUp)
		{
			setUp(mind);
		}
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		ASSERT_TRUE(loaded.has_value()) << "row " << action << ", step " << step;
		EXPECT_EQ(loaded->action, action);
		EXPECT_EQ(loaded->objects, (std::array<gamesave::ObjectNumber, 3> {0, 105, 0}));
		EXPECT_EQ(loaded->step, step);
		EXPECT_EQ(loaded->count, count);
		auto again = Forced(action, make());
		if (setUp)
		{
			setUp(again);
		}
		again.idle.step = loaded->step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read);
		// another row with the same steps is not this one
		auto other = read;
		other.action = 0;
		EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(other).has_value());
	}
}
} // namespace

// Looking at the hand, as a script holds a creature on it: written at each of its steps, read back, taken up by the load as
// the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, LookingAtTheHandIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(8, [] { return creature_mind::LookAtHand(); });
}

// Setting down what it holds, as a script holds a creature on it: written at each of its steps, read back, taken up by the load
// as the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, SettingDownWhatItHoldsIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(67, [] { return creature_mind::PutDownHeld(); });
}

// Dying for good, as a script forces it on a creature: written at each of its steps, read back, taken up by the load as
// the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, DyingForGoodIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(217, [] { return creature_mind::DiePermanently(); });
}

// Waving at a thing, as a script holds a creature on it: written at each of its steps, read back, taken up by the load as
// the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, WavingAtAThingIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(91, [] { return creature_mind::WaveAt(5, 10.0f); });
}

// Looking at a thing a while, as a script holds a creature on it: written at its turn to the thing and at its wait,
// read back, taken up by the load as the same forced plan at the same step, its agenda made again with the wait the save
// keeps, and written back as read; a turn to another thing is not taken up
TEST(CreatureWriteScriptPlan, LookingAtAThingAWhileIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const size_t step : {size_t {0}, size_t {1}})
	{
		auto mind = Forced(188, creature_mind::LookButDontApproach(5, 1.73f));
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		EXPECT_EQ(loaded,
		          (ecs::saved_world_load::KeptForcedPlan {.action = 188, .objects = {0, 105, 0}, .step = step, .count = 2}));
		const auto agenda = ecs::saved_world_load::GlanceAgendaOf(read, 5);
		ASSERT_TRUE(agenda.has_value());
		auto again = Forced(188, *agenda);
		again.idle.step = step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read);
		EXPECT_FALSE(ecs::saved_world_load::GlanceAgendaOf(read, std::nullopt).has_value());
		if (step == 0)
		{
			auto elsewhere = read;
			elsewhere.steps.front().object = NumberRef(6);
			EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(elsewhere).has_value());
		}
	}
}

// Looking a thing over, as a script holds a creature on it (its belief about the thing of kind 9): written at each of
// its steps, read back, taken up by the load as the same forced plan at the same step, its agenda made again for the
// thing with the walk's distance the save keeps, and written back as read
TEST(CreatureWriteScriptPlan, LookingAThingOverIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (size_t step = 0; step < 4; ++step)
	{
		auto mind = Forced(20, creature_mind::ExamineByLooking(5, 10.0f, 4.0f));
		mind.beliefs.Add(entt::entity {5}, 9);
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		EXPECT_EQ(loaded,
		          (ecs::saved_world_load::KeptForcedPlan {.action = 20, .objects = {0, 105, 0}, .step = step, .count = 4}));
		const auto agenda = ecs::saved_world_load::ExamineAgendaOf(read, 5);
		ASSERT_TRUE(agenda.has_value());
		auto again = Forced(20, *agenda);
		again.beliefs.Add(entt::entity {5}, 9);
		again.idle.step = step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read) << "step " << step;
	}
}

// Smiling at a friend, as a script holds a creature on it, with the happy clip drawn and without: written at each of its
// steps, read back, taken up by the load as the same forced plan at the same step, its agenda made again for the friend
// with the walk, the turn's seconds and the clip the save keeps, and written back as read
TEST(CreatureWriteScriptPlan, SmilingAtAFriendIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const uint32_t happy : {0u, 1u})
	{
		const auto make = [happy] {
			return creature_mind::SmileAtFriend(5, 10.0f, [happy](uint32_t range) { return range == 2 ? happy : 1234u; });
		};
		const auto count = make().size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(60, make());
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "happy " << happy << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(60, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "happy " << happy << ", step " << step;
		}
	}
}

// Raising a totem pole, as a script holds a creature on it: written at each of its steps, read back, taken up by the load as
// the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, RaisingATotemPoleIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(56, [] { return creature_town_help::MoveTotem(5, true); });
}

// Stomping on a thing and eating it, as a script holds a creature on it, from the stomp or holding a thing it can eat:
// written at each of its steps, read back, taken up by the load as the same forced plan at the same step, its agenda
// made again for the thing as the saved count of steps has it (holding the thing once past the pick-up), and written
// back as read
TEST(CreatureWriteScriptPlan, StompingOnAThingAndEatingItIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const bool holding : {false, true})
	{
		const auto count = creature_mind::StompAndEat(5, holding).size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(13, creature_mind::StompAndEat(5, holding));
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "holding " << holding << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(13, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "holding " << holding << ", step " << step;
		}
	}
}

// Eating from a food pile, as a script holds a creature on it: written at its walk, its turn and the making of the
// portion, read back, taken up by the load as the same forced plan at the same step, its agenda made again for the pile
// with the walk the save keeps, and written back as read; at the eat of the portion it made it is not taken up
TEST(CreatureWriteScriptPlan, EatingFromAFoodPileIsLoadedAndWrittenBackAsReadUntilThePortionIsMade)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	const auto make = [] { return creature_mind::EatPortion(5, {30.0f, 40.0f}, 6.0f, 2.5f); };
	for (size_t step = 0; step < 4; ++step)
	{
		auto mind = Forced(158, make());
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		if (step == 3)
		{
			EXPECT_FALSE(loaded.has_value());
			continue;
		}
		ASSERT_TRUE(loaded.has_value()) << "step " << step;
		EXPECT_EQ(loaded->count, 4u);
		const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
		ASSERT_TRUE(agenda.has_value());
		auto again = Forced(158, *agenda);
		again.idle.step = step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read) << "step " << step;
	}
}

// Sleeping by a thing, as a script holds a creature on it, at a clear place by it with the yawn and without: written at
// each step past its walk (a walk to a clear place has no height to write yet), read back, taken up by the load as the
// same forced plan at the same step, its agenda made again as the saved steps have it, and written back as read
TEST(CreatureWriteScriptPlan, SleepingByAThingIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const uint32_t yawn : {0u, 1u})
	{
		const auto make = [yawn] {
			return creature_mind::SleepByObject(glm::vec2 {3.0f, 4.0f}, 2.0f, std::nullopt, 10.0f,
			                                    [yawn](uint32_t) { return yawn; });
		};
		const auto count = make().size();
		for (size_t step = 1; step < count; ++step)
		{
			auto mind = Forced(78, make());
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "yawn " << yawn << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(78, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "yawn " << yawn << ", step " << step;
		}
	}
}

// A poo by a thing, as a script holds a creature on it, with the need-a-poo clip drawn and without: written at each of
// its steps, read back, taken up by the load as the same forced plan at the same step, its agenda made again for the
// thing with the clip as the saved count of steps has it, and written back as read
namespace
{
void ExpectPooRoundTrip(uint32_t action)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const uint32_t clip : {0u, 1u})
	{
		const auto make = [clip] { return creature_mind::PooBy(5, [clip](uint32_t) { return clip; }); };
		const auto count = make().size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(action, make());
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "row " << action << ", clip " << clip << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(action, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "row " << action << ", clip " << clip << ", step " << step;
		}
	}
}
} // namespace

TEST(CreatureWriteScriptPlan, APooByAThingIsLoadedAndWrittenBackAsRead)
{
	ExpectPooRoundTrip(19);
}

// A fart, as a script holds a creature on it: the same steps as a poo by a thing, loaded and written back as read
TEST(CreatureWriteScriptPlan, AFartIsLoadedAndWrittenBackAsRead)
{
	ExpectPooRoundTrip(177);
}

// Hurling a thing at a thing, as a script holds a creature on it, angry first, not, and with its hand full: written at
// each of its steps, read back, taken up by the load as the same forced plan at the same step, its agenda made again
// for the two things with the clip and the pick-up as the saved count of steps has them and the walk's height the save
// keeps, and written back as read; a pick-up of another thing than the one it throws is not taken up
TEST(CreatureWriteScriptPlan, HurlingAThingIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const auto& [angry, handFull] : {std::pair(0u, false), std::pair(1u, false), std::pair(1u, true)})
	{
		const auto make = [angry, handFull] {
			return creature_mind::HurlAt(6, 5, {1.0f, 2.0f}, 10.0f, handFull, [angry](uint32_t) { return angry; });
		};
		const auto count = make().size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(15, make());
			mind.planner.current->instrument = 6;
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "count " << count << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5, 6, {1.0f, 2.0f});
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(15, *agenda);
			again.planner.current->instrument = 6;
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "count " << count << ", step " << step;
			if (step == 0 && !handFull)
			{
				auto other = read;
				other.steps[count == 4 ? 1 : 0].object = NumberRef(8);
				EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(other).has_value());
			}
		}
	}
}

// Waving at the player, as a script holds a creature on it: written at each of its steps, read back, taken up by the load as
// the same forced plan at the same step, its agenda the one the action makes, and written back as read
TEST(CreatureWriteScriptPlan, WavingAtThePlayerIsLoadedAndWrittenBackAsRead)
{
	ExpectForcedRowRoundTrip(90, [] { return creature_mind::FaceCameraIndividual({1.0f, 3.0f}, 72); });
}

// Pointing at the camera, as a script holds a creature on it: written at its turn to the camera and at its point, read
// back, taken up by the load as the same forced plan at the same step, its agenda made again pointing at the camera's
// place the save keeps, and written back as read; with no turn (a creature with no player) it is not written
TEST(CreatureWriteScriptPlan, PointingAtTheCameraIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	EXPECT_FALSE(ScriptHeldPlanOf(Forced(168, creature_mind::PointAtCamera(false, {1.0f, 2.0f, 3.0f})), k_Self, NumberRef, why)
	                 .has_value());
	for (const bool hasPlayer : {true})
	{
		const auto make = [hasPlayer] { return creature_mind::PointAtCamera(hasPlayer, {1.0f, 2.0f, 3.0f}); };
		const auto count = make().size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(168, make());
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "count " << count << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(168, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "count " << count << ", step " << step;
		}
	}
}

// Holding a thing, as a script holds a creature on it, picking it up or with its hand full, playing with it or not:
// written at each of its steps, read back, taken up by the load as the same forced plan at the same step, its agenda
// made again for the thing as the saved steps lay it with the play and the wait the save keeps, and written back as
// read
TEST(CreatureWriteScriptPlan, HoldingAThingIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const auto& [handFull, plays] :
	     {std::pair(false, true), std::pair(false, false), std::pair(true, true), std::pair(true, false)})
	{
		const auto make = [handFull, plays] {
			return creature_mind::HoldObject({.thing = 5,
			                                  .handFull = handFull,
			                                  .camera = glm::vec2 {1.0f, 3.0f},
			                                  .playsWithIt = plays,
			                                  .turnsPerSecond = 10.0f},
			                                 [](uint32_t range) { return range == 4 ? 2u : 777u; });
		};
		const auto count = make().size();
		for (size_t step = 0; step < count; ++step)
		{
			auto mind = Forced(64, make());
			mind.idle.step = step;
			const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
			ASSERT_TRUE(written.has_value()) << why;
			const auto read = PlanReadBack(*written);
			EXPECT_EQ(read, *written);
			const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
			ASSERT_TRUE(loaded.has_value()) << "count " << count << ", step " << step;
			EXPECT_EQ(loaded->count, count);
			const auto agenda = ecs::saved_world_load::ForcedAgendaOf(read, 5);
			ASSERT_TRUE(agenda.has_value());
			auto again = Forced(64, *agenda);
			again.idle.step = step;
			const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
			ASSERT_TRUE(rewritten.has_value()) << why;
			EXPECT_EQ(*rewritten, read) << "count " << count << ", step " << step;
		}
	}
}

TEST(CreatureWriteScriptPlan, BeingFrightenedOnTheSpotIsLoadedAndWrittenBackAsRead)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	std::string why;
	for (const size_t step : {size_t {0}, size_t {1}, size_t {2}})
	{
		auto mind = Forced(174, creature_mind::BeFrightenedOnTheSpot());
		mind.idle.step = step;
		const auto written = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(written.has_value()) << why;
		const auto read = PlanReadBack(*written);
		EXPECT_EQ(read, *written);
		const auto loaded = ecs::saved_world_load::ForcedPlanOf(read);
		EXPECT_EQ(loaded,
		          (ecs::saved_world_load::KeptForcedPlan {.action = 174, .objects = {0, 105, 0}, .step = step, .count = 3}));
		ASSERT_TRUE(loaded.has_value());
		auto again = Forced(174, creature_mind::BeFrightenedOnTheSpot());
		again.idle.step = loaded->step;
		const auto rewritten = ScriptHeldPlanOf(again, k_Self, NumberRef, why);
		ASSERT_TRUE(rewritten.has_value()) << why;
		EXPECT_EQ(*rewritten, read);
		auto odd = read;
		odd.steps.back().action = 0x18;
		EXPECT_FALSE(ecs::saved_world_load::ForcedPlanOf(odd).has_value());
	}
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
	// a fireball's: its walk to the middle of the clear place it found, a point on the land there (none here: at 0),
	// within twice its height; read back, the walk the load starts again goes to that point
	const test::RestoreService<Locator::terrainSystem> restoreTerrain;
	Locator::terrainSystem::reset();
	auto fireball = Forced(96, creature_mind::CastFireballAt(9, 0, 5, true, {1.0f, 2.0f}, 10.0f, none));
	const auto walking = ScriptHeldPlanOf(fireball, k_Self, NumberRef, why);
	ASSERT_TRUE(walking.has_value()) << why;
	EXPECT_EQ(walking->current, 0u);
	EXPECT_EQ(walking->steps.front(), (gamesave::AgendaStep {.action = 8, .point = {1.0f, 0.0f, 2.0f}, .real = 20.0f}));
	const auto read = ecs::saved_world_load::ForcedPlanOf(*walking);
	ASSERT_TRUE(read.has_value());
	EXPECT_EQ(read->action, 96u);
	EXPECT_EQ(read->step, 0u);
	EXPECT_EQ(read->point, std::optional(glm::vec2(1.0f, 2.0f)));
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
// and stage it is at as it falls and lies, as it is carried home, waits, rests and gets up
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
	// carried home: fading out at the carry's running stage, faded in at home at its last
	using Stage = components::CreatureKnockedOut::Stage;
	knockedOut.rests = true;
	knockedOut.stage = Stage::FadingOut;
	const auto fading = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(fading.has_value()) << why;
	EXPECT_EQ(fading->current, 2u);
	EXPECT_EQ(fading->state, 1u);
	EXPECT_EQ(fading->firstWord, 0u);
	ASSERT_EQ(fading->steps.size(), 4u);
	EXPECT_EQ(fading->steps[0], (gamesave::AgendaStep {.action = 0x70}));
	knockedOut.stage = Stage::FadingIn;
	EXPECT_EQ(KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why)->state, 2u);
	// a creature with no player is never carried
	EXPECT_FALSE(KnockedOutPlanOf(knockedOut, k_Self, k_Self, false, why).has_value());
	// at home, the three seconds' wait with its turns left, counted from the turn after the carry ends; then the wait for
	// the spells
	knockedOut.stage = Stage::Waiting;
	knockedOut.homeWaitTurns = 25;
	const auto waiting = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(waiting.has_value()) << why;
	EXPECT_EQ(waiting->current, 3u);
	EXPECT_EQ(waiting->state, 1u);
	EXPECT_EQ(waiting->steps[0], (gamesave::AgendaStep {.action = 0x18, .integer = 25}));
	knockedOut.homeWaitTurns = 0;
	knockedOut.homeWaited = true;
	const auto spells = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(spells.has_value()) << why;
	EXPECT_EQ(spells->current, 4u);
	EXPECT_EQ(spells->steps[0], (gamesave::AgendaStep {.action = 0x8B}));
	// not carried home, its wait is the same, with no carry before it
	knockedOut.homeWaitTurns = 24;
	knockedOut.homeWaited = false;
	const auto still = KnockedOutPlanOf(knockedOut, k_Self, k_Self, false, why);
	ASSERT_TRUE(still.has_value()) << why;
	EXPECT_EQ(still->current, 2u);
	EXPECT_EQ(still->steps[0], (gamesave::AgendaStep {.action = 0x18, .integer = 24}));
	// resting: lying down not yet started, then healing in the pose, then getting up as the rest's end
	knockedOut.stage = Stage::Resting;
	const auto lyingDown = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(lyingDown.has_value()) << why;
	EXPECT_EQ(lyingDown->current, 5u);
	EXPECT_EQ(lyingDown->state, 0u);
	EXPECT_EQ(lyingDown->firstWord, 1u);
	ASSERT_EQ(lyingDown->steps.size(), 1u);
	EXPECT_EQ(lyingDown->steps[0], (gamesave::AgendaStep {.action = 0x82}));
	knockedOut.restTurns = 3;
	knockedOut.restPosed = true;
	EXPECT_EQ(KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why)->state, 1u);
	knockedOut.stage = Stage::GettingUp;
	EXPECT_EQ(KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why)->state, 2u);
	// getting up with no rest: the Die action is over in the game, and its plan is a finished one's, cleared, which the
	// load takes as no plan
	knockedOut.rests = false;
	const auto over = KnockedOutPlanOf(knockedOut, k_Self, k_Self, true, why);
	ASSERT_TRUE(over.has_value()) << why;
	EXPECT_EQ(over->desire, 40u);
	EXPECT_EQ(over->firstWord, 1u);
	EXPECT_EQ(over->count, 0u);
	EXPECT_TRUE(over->steps.empty());
	EXPECT_EQ(over->words[1], 0xFFFFFFFFu);
	EXPECT_TRUE(ecs::saved_world_load::IsClearedPlan(*over));
	EXPECT_FALSE(ecs::saved_world_load::KnockedOutOf(*over, true).has_value());
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

namespace
{
using Stage = components::CreatureFighting::Stage;
using Phase = creature_turn_to_face::Turn::Phase;

/// A fighter at a stage, the turns of its face-off or turn to face, its turn's phase and whether it was told what to do
components::CreatureFighting FighterAt(bool starter, Stage stage, uint32_t turns = 0, Phase phase = Phase::Idle,
                                       bool played = false, bool rude = false)
{
	components::CreatureFighting fighting {.stage = stage, .madeArena = starter, .awaitsArena = stage < Stage::Point};
	fighting.faceOffTurns = turns;
	fighting.faceTurn.phase = phase;
	fighting.played = played;
	fighting.rude = rude;
	return fighting;
}
} // namespace

// Where each fighter is in its fight's agenda, from its stage: the first creature's step of the eighteen and whether it
// is under way, the other's step its last order gave it, or its wait for the next. The other before its first order,
// a fight won and one called off with no response are not written
TEST(CreatureWriteScriptPlan, AFightersStepIsItsStageInTheFightsAgenda)
{
	using ecs::saved_world_write::FightPlace;
	std::string why;
	const auto starterAt = [&why](const components::CreatureFighting& self) {
		return FightPlaceOf(self, FighterAt(false, Stage::Ready), why);
	};
	EXPECT_EQ(starterAt(FighterAt(true, Stage::FaceOff, 1)), (FightPlace {.current = 1, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::FaceOff, 3)), (FightPlace {.current = 3, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::FaceOff, 5, Phase::Check)), (FightPlace {.current = 3, .state = 1}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::FaceOff, 7, Phase::Done)), (FightPlace {.current = 4, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Taunt)), (FightPlace {.current = 5, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Taunt, 0, Phase::Idle, true)), (FightPlace {.current = 5, .state = 1}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Point, 0, Phase::Idle, true)), (FightPlace {.current = 7, .state = 1}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Approach)), (FightPlace {.current = 9, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Ready, 0, Phase::Idle, true)), (FightPlace {.current = 10, .state = 1}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Square)), (FightPlace {.current = 11, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Square, 1)), (FightPlace {.current = 12, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Square, 3, Phase::Hold)), (FightPlace {.current = 12, .state = 1}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Square, 5, Phase::Done)), (FightPlace {.current = 13, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Square, 6, Phase::Done, true)), (FightPlace {.current = 14, .state = 0}));
	EXPECT_EQ(starterAt(FighterAt(true, Stage::Duel)), (FightPlace {.current = 14, .state = 1}));
	auto responding = FighterAt(true, Stage::Respond, 0, Phase::Idle, true);
	responding.responded = true;
	EXPECT_EQ(starterAt(responding), (FightPlace {.current = 16, .state = 1}));

	const auto otherAt = [&why](const components::CreatureFighting& self, const components::CreatureFighting& starter) {
		return FightPlaceOf(self, starter, why);
	};
	const auto turning = FighterAt(true, Stage::FaceOff, 4, Phase::Check);
	EXPECT_EQ(otherAt(FighterAt(false, Stage::FaceOff, 0, Phase::Start), turning),
	          (FightPlace {.current = 0, .state = 0, .ordered = 0x06}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::FaceOff, 0, Phase::Hold), turning),
	          (FightPlace {.current = 0, .state = 1, .ordered = 0x06}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::FaceOff, 0, Phase::Done), turning), (FightPlace {.current = 1, .state = 1}));
	const auto pointing = FighterAt(true, Stage::Point);
	EXPECT_EQ(otherAt(FighterAt(false, Stage::Taunt, 0, Phase::Idle, true), pointing),
	          (FightPlace {.current = 0, .state = 1, .ordered = 0x0A}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::Ready), pointing), (FightPlace {.current = 1, .state = 1}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::Approach), FighterAt(true, Stage::Approach)),
	          (FightPlace {.current = 0, .state = 0, .ordered = 0x63}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::Square, 0, Phase::Done, true), FighterAt(true, Stage::Square)),
	          (FightPlace {.current = 0, .state = 0, .ordered = 0x1C}));
	EXPECT_EQ(otherAt(FighterAt(false, Stage::Duel), FighterAt(true, Stage::Duel)),
	          (FightPlace {.current = 0, .state = 1, .ordered = 0x1C}));

	// asked, and not yet ordered to turn: in the game it still carries out its own plan
	EXPECT_FALSE(otherAt(FighterAt(false, Stage::FaceOff), FighterAt(true, Stage::FaceOff, 2)).has_value());
	EXPECT_NE(why.find("not yet ordered"), std::string::npos) << why;
	EXPECT_FALSE(starterAt(FighterAt(true, Stage::Celebrate)).has_value());
	// called off: it stands with no response
	auto calledOff = FighterAt(true, Stage::Respond, 0, Phase::Idle, true);
	EXPECT_FALSE(starterAt(calledOff).has_value());
	// which the creature's writer takes as out of a fight, as the game's fight plan failed; a fighter still finishing
	// its last move of the duel, or showing its response, or in its duel, is in its fight
	EXPECT_TRUE(FightCalledOff(calledOff));
	EXPECT_FALSE(FightCalledOff(FighterAt(true, Stage::Duel)));
	EXPECT_FALSE(FightCalledOff(responding));
	auto finishing = FighterAt(true, Stage::Respond);
	finishing.fighter.state = creature_fight::State::Finish;
	EXPECT_FALSE(FightCalledOff(finishing));
}

// The first creature's plan at a step before the duel keeps the fight's steps from there on as the agenda lays them,
// each order with the ordered step's own arguments and the taunts drawn for the two; a step not started has the
// agenda's first word set. The other's plan is the step its last order gave it and the wait for the next, or the wait
// alone
TEST(CreatureWriteScriptPlan, AFighterBeforeItsDuelKeepsItsFightsStepsFromWhereItIs)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr gamesave::Ref k_Opponent {.number = 9};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 21, .object = 3};
	std::string why;
	const auto other = FighterAt(false, Stage::FaceOff, 0, Phase::Done, false, true);
	const auto starter = FightPlanOf(&mind, FighterAt(true, Stage::FaceOff, 3), other, k_Self, k_Opponent, why);
	ASSERT_TRUE(starter.has_value()) << why;
	EXPECT_EQ(starter->current, 3u);
	EXPECT_EQ(starter->state, 0u);
	EXPECT_EQ(starter->firstWord, 1u);
	EXPECT_EQ(starter->count, 18u);
	EXPECT_EQ(starter->words[1], 14u);
	EXPECT_EQ(starter->other, k_Opponent);
	ASSERT_EQ(starter->steps.size(), 15u);
	EXPECT_EQ(starter->steps[0], (gamesave::AgendaStep {.action = 0x06, .object = k_Opponent, .real = 0.1f}));
	EXPECT_EQ(starter->steps[1],
	          (gamesave::AgendaStep {.action = 0x4A, .integer = 0xDE, .other = k_Opponent, .orderKind = 0x0A}));
	EXPECT_EQ(starter->steps[2], (gamesave::AgendaStep {.action = 0x0A, .integer = 0x46}));
	EXPECT_EQ(starter->steps[3], (gamesave::AgendaStep {.action = 0x62}));
	EXPECT_EQ(starter->steps[4], (gamesave::AgendaStep {.action = 0x81}));
	EXPECT_EQ(starter->steps[5], (gamesave::AgendaStep {.action = 0x4A, .other = k_Opponent, .orderKind = 0x63}));
	EXPECT_EQ(starter->steps[7], (gamesave::AgendaStep {.action = 0x46, .object = k_Opponent}));
	EXPECT_EQ(starter->steps[8],
	          (gamesave::AgendaStep {.action = 0x4A, .object = k_Self, .real = 0.1f, .other = k_Opponent, .orderKind = 0x06}));
	EXPECT_EQ(starter->steps[10],
	          (gamesave::AgendaStep {.action = 0x4A, .object = k_Self, .other = k_Opponent, .orderKind = 0x1C}));
	// the duel's own plan is the same agenda at its main step
	const auto duel = DuelPlanOf(&mind, true, k_Self, k_Opponent, why);
	ASSERT_TRUE(duel.has_value()) << why;
	EXPECT_TRUE(std::ranges::equal(duel->steps, std::span(starter->steps).subspan(11)));

	const auto taunting = FightPlanOf(nullptr, FighterAt(false, Stage::Taunt, 0, Phase::Idle, false, true),
	                                  FighterAt(true, Stage::Taunt), k_Opponent, k_Self, why);
	ASSERT_TRUE(taunting.has_value()) << why;
	EXPECT_EQ(taunting->desire, 26u);
	EXPECT_EQ(taunting->action, 124u);
	EXPECT_EQ(taunting->current, 0u);
	EXPECT_EQ(taunting->count, 2u);
	ASSERT_EQ(taunting->steps.size(), 2u);
	EXPECT_EQ(taunting->steps[0], (gamesave::AgendaStep {.action = 0x0A, .integer = 0xDE}));
	EXPECT_EQ(taunting->steps[1], (gamesave::AgendaStep {.action = 0x4E, .object = k_Self}));
	const auto waiting =
	    FightPlanOf(nullptr, FighterAt(false, Stage::Ready), FighterAt(true, Stage::Point), k_Opponent, k_Self, why);
	ASSERT_TRUE(waiting.has_value()) << why;
	EXPECT_EQ(waiting->current, 1u);
	ASSERT_EQ(waiting->steps.size(), 1u);
	EXPECT_EQ(waiting->steps[0].action, 0x4Eu);
	// a starter whose mind holds no fight is not written
	EXPECT_FALSE(FightPlanOf(nullptr, FighterAt(true, Stage::Taunt), other, k_Self, k_Opponent, why).has_value());
}

// A fight saved at any step the save writes is read back as it was written, and the load takes up the stage each
// fighter's step starts again at: the face-off from its order or its own turn, the taunt with the drawn gesture, the
// point, the walk, the wait and the turn to face in the arena from their starts; from the duel on, the duel's load
TEST(CreatureWriteScriptPlan, AFightSavedBeforeItsDuelLoadsAtTheStageItsStepStartsAgain)
{
	namespace load = openblack::ecs::saved_world_load;
	constexpr gamesave::Ref k_First {.number = 7};
	constexpr gamesave::Ref k_Second {.number = 9};
	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 21, .object = 3};
	struct Case
	{
		components::CreatureFighting starter;
		components::CreatureFighting other;
		Stage starterStage;
		Stage otherStage;
	};
	const std::vector<Case> cases {
	    {FighterAt(true, Stage::FaceOff, 4, Phase::Check), FighterAt(false, Stage::FaceOff, 0, Phase::Done), Stage::FaceOff,
	     Stage::FaceOff},
	    {FighterAt(true, Stage::FaceOff, 6, Phase::Done), FighterAt(false, Stage::FaceOff, 0, Phase::Hold), Stage::FaceOff,
	     Stage::FaceOff},
	    {FighterAt(true, Stage::Taunt, 0, Phase::Idle, true, true), FighterAt(false, Stage::Taunt, 0, Phase::Idle, false, true),
	     Stage::Taunt, Stage::Taunt},
	    {FighterAt(true, Stage::Point, 0, Phase::Idle, true), FighterAt(false, Stage::Ready), Stage::Point, Stage::Ready},
	    {FighterAt(true, Stage::Approach, 0, Phase::Idle, true), FighterAt(false, Stage::Approach, 0, Phase::Idle, true),
	     Stage::Approach, Stage::Approach},
	    {FighterAt(true, Stage::Ready, 0, Phase::Idle, true), FighterAt(false, Stage::Ready), Stage::Ready, Stage::Ready},
	    {FighterAt(true, Stage::Square), FighterAt(false, Stage::Ready), Stage::Square, Stage::Ready},
	    {FighterAt(true, Stage::Square, 3, Phase::Check), FighterAt(false, Stage::Square, 0, Phase::Start), Stage::Square,
	     Stage::Square},
	    {FighterAt(true, Stage::Square, 5, Phase::Done), FighterAt(false, Stage::Square, 0, Phase::Done), Stage::Square,
	     Stage::Square},
	    {FighterAt(true, Stage::Duel), FighterAt(false, Stage::Duel), Stage::Respond, Stage::Respond},
	};
	for (size_t i = 0; i < cases.size(); ++i)
	{
		const auto& each = cases[i];
		std::string why;
		const auto first = FightPlanOf(&mind, each.starter, each.other, k_First, k_Second, why);
		const auto second = FightPlanOf(nullptr, each.other, each.starter, k_Second, k_First, why);
		ASSERT_TRUE(first.has_value() && second.has_value()) << i << ": " << why;
		const auto keptFirst = load::FightOf(*first);
		const auto keptSecond = load::FightOf(*second);
		ASSERT_TRUE(keptFirst.has_value() && keptSecond.has_value()) << i;
		EXPECT_TRUE(keptFirst->startedIt) << i;
		EXPECT_EQ(keptFirst->other, k_Second.number) << i;
		EXPECT_FALSE(keptSecond->startedIt) << i;
		EXPECT_EQ(keptSecond->other, k_First.number) << i;
		const auto loadedFirst = load::LoadedFightingOf(*keptFirst, *keptSecond);
		const auto loadedSecond = load::LoadedFightingOf(*keptSecond, *keptFirst);
		ASSERT_TRUE(loadedFirst.has_value() && loadedSecond.has_value()) << i;
		EXPECT_EQ(loadedFirst->stage, each.starterStage) << i;
		EXPECT_EQ(loadedSecond->stage, each.otherStage) << i;
		EXPECT_TRUE(loadedFirst->madeArena) << i;
		EXPECT_FALSE(loadedSecond->madeArena) << i;
		// what the loaded step starts again with: written there again, it is at the same step not started
		if (IsLeadIn(loadedFirst->stage))
		{
			EXPECT_EQ(loadedFirst->awaitsArena, each.starter.stage < Stage::Point) << i;
			const auto again = FightPlaceOf(*loadedFirst, *loadedSecond, why);
			ASSERT_TRUE(again.has_value()) << i << ": " << why;
			EXPECT_EQ(again->current, first->current) << i;
			EXPECT_EQ(again->state, 0u) << i;
		}
		else
		{
			EXPECT_TRUE(loadedFirst->ended) << i;
		}
	}
	// the drawn gestures come back with the taunts
	std::string whyTaunt;
	const auto taunt = FightPlanOf(&mind, FighterAt(true, Stage::FaceOff, 4, Phase::Check, false, true),
	                               FighterAt(false, Stage::FaceOff, 0, Phase::Done, false, false), k_First, k_Second, whyTaunt);
	ASSERT_TRUE(taunt.has_value()) << whyTaunt;
	const auto kept = load::FightOf(*taunt);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(kept->rude, std::optional(true));
	EXPECT_EQ(kept->otherRude, std::optional(false));
	// the other's step that does not fit the first creature's is none: turning to face it while the first walks in
	EXPECT_FALSE(load::LoadedFightingOf(load::KeptFight {.startedIt = false, .other = 7, .step = 0x06},
	                                    load::KeptFight {.startedIt = true, .other = 9, .step = 9})
	                 .has_value());
	EXPECT_FALSE(load::LoadedFightingOf(load::KeptFight {.startedIt = true, .other = 9, .step = 9},
	                                    load::KeptFight {.startedIt = true, .other = 7, .step = 9})
	                 .has_value());
}

// A fight a script forces keeps its plan of obeying the player at its fight's step once the fight has started, as any
// fight; before, at the request or the other's stand-still, the write stops, naming the taunts not yet drawn
TEST(CreatureWriteScriptPlan, AFightAScriptForcesIsWrittenByItsFightOnceItHasStarted)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr gamesave::Ref k_Opponent {.number = 9};
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer, .action = 21, .object = 9, .forced = true};
	mind.idle.agenda = creature_mind::FightCreature(9);
	std::string why;
	EXPECT_FALSE(ScriptHeldPlanOf(mind, k_Self, NumberRef, why).has_value());
	EXPECT_NE(why.find("taunts"), std::string::npos) << why;
	const auto plan =
	    FightPlanOf(&mind, FighterAt(true, Stage::Approach), FighterAt(false, Stage::Approach), k_Self, k_Opponent, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer));
	EXPECT_EQ(plan->action, 21u);
	EXPECT_EQ(plan->current, 9u);
	const auto kept = ecs::saved_world_load::FightOf(*plan);
	ASSERT_TRUE(kept.has_value());
	EXPECT_TRUE(kept->startedIt);
	EXPECT_EQ(kept->step, 9u);
}

// The winner finishing its last move is written by the plan its win forced, as the game puts that plan in place of the
// fight's at the knock-out: an evil winner's poo on the loser or any other's show-off at it, its plan of obeying its
// player with the action's row. A winner whose mind holds no such plan, or a fighter still in its duel, is not
TEST(CreatureWriteScriptPlan, AFightsWinnerIsWrittenByThePlanItsWinForced)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr uint32_t k_Poo = 19;
	components::CreatureMindState mind;
	EXPECT_FALSE(WrittenByItsWin(FighterAt(true, Stage::Celebrate), &mind));
	EXPECT_FALSE(WrittenByItsWin(FighterAt(true, Stage::Celebrate), nullptr));
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer, .action = k_Poo, .object = 9, .forced = true};
	creature_mind::Step poo;
	poo.kind = creature_mind::Step::Kind::Apply;
	poo.effect = creature_mind::Effect::Poo;
	poo.main = true;
	mind.idle.agenda = {poo};
	EXPECT_TRUE(WrittenByItsWin(FighterAt(true, Stage::Celebrate), &mind));
	EXPECT_TRUE(WrittenByItsWin(FighterAt(false, Stage::Celebrate), &mind));
	EXPECT_FALSE(WrittenByItsWin(FighterAt(true, Stage::Duel), &mind));
	std::string why;
	const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer));
	EXPECT_EQ(plan->action, k_Poo);
	EXPECT_EQ(plan->current, 0u);
	EXPECT_EQ(plan->state, 0u);
	ASSERT_EQ(plan->steps.size(), 1u);
	EXPECT_EQ(plan->steps[0].action, 0xFu);
	// any other winner shows off at the loser: go near it, face it, the impressive pose and the main pose after it, which
	// the load makes again as it was drawn
	constexpr uint32_t k_ShowOff = 39;
	mind.planner.current->action = k_ShowOff;
	mind.idle.agenda = creature_mind::ShowImpressiveAnimation(9, 2.0f, [](uint32_t) { return 1U; });
	const auto showOff = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
	ASSERT_TRUE(showOff.has_value()) << why;
	EXPECT_EQ(showOff->action, k_ShowOff);
	EXPECT_EQ(showOff->count, 4u);
	EXPECT_EQ(showOff->words[1], 3u);
	ASSERT_EQ(showOff->steps.size(), 4u);
	EXPECT_EQ(showOff->steps[0], (gamesave::AgendaStep {.action = 0x3D, .object = {.number = 109}, .real = 6.0f}));
	EXPECT_EQ(showOff->steps[1].action, 0x06u);
	EXPECT_EQ(showOff->steps[2], (gamesave::AgendaStep {.action = 0x0A, .integer = 69}));
	EXPECT_EQ(showOff->steps[3], (gamesave::AgendaStep {.action = 0x0A, .integer = 52}));
	ASSERT_TRUE(ecs::saved_world_load::ForcedPlanOf(*showOff).has_value());
	const auto loaded = ecs::saved_world_load::ShowOffAgendaOf(*showOff, 9u);
	ASSERT_TRUE(loaded.has_value());
	ASSERT_EQ(loaded->size(), 4u);
	EXPECT_EQ(loaded->at(3).animation, 52u);
	EXPECT_FLOAT_EQ(loaded->at(0).movement.maxDistance, 6.0f);
	// a plan of its own is not the win's
	mind.planner.current->forced = false;
	EXPECT_FALSE(WrittenByItsWin(FighterAt(true, Stage::Celebrate), &mind));
}

// The other creature only asked to fight, before the first creature's first order, carries out its own plan in the game
// and is written out of a fight; the first creature, its fight of its own plan at the request or the stand-still, is
// written at that step under its plan's desire, and the load reads it back as a fight before its first order
TEST(CreatureWriteScriptPlan, AFightBeforeItsFirstOrderIsWrittenAsTheOthersOwnPlanAndTheFirstsRequest)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	constexpr gamesave::Ref k_Opponent {.number = 9};
	for (const uint32_t turns : {0U, 1U})
	{
		EXPECT_TRUE(AskedAndNotOrdered(FighterAt(false, Stage::FaceOff), FighterAt(true, Stage::FaceOff, turns)));
	}
	// ordered, or at the order's turn, it obeys
	auto ordered = FighterAt(false, Stage::FaceOff);
	ordered.ordered = true;
	EXPECT_FALSE(AskedAndNotOrdered(ordered, FighterAt(true, Stage::FaceOff, 1)));
	EXPECT_FALSE(AskedAndNotOrdered(FighterAt(false, Stage::FaceOff),
	                                FighterAt(true, Stage::FaceOff, creature_fight::k_FaceOffOrderTurn)));
	EXPECT_FALSE(AskedAndNotOrdered(FighterAt(true, Stage::FaceOff), FighterAt(false, Stage::FaceOff)));

	components::CreatureMindState mind;
	mind.planner.current = creature_planner::Plan {.desire = creature_desires::Desire::Anger, .action = 21, .object = 9};
	std::string why;
	const auto plan =
	    FightPlanOf(&mind, FighterAt(true, Stage::FaceOff, 1), FighterAt(false, Stage::FaceOff), k_Self, k_Opponent, why);
	ASSERT_TRUE(plan.has_value()) << why;
	EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::Anger));
	EXPECT_EQ(plan->current, 1u);
	EXPECT_EQ(plan->count, 18u);
	const auto kept = ecs::saved_world_load::FightRequestOf(*plan);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(kept->step, 1u);
	EXPECT_EQ(kept->other, 9u);
	EXPECT_FALSE(ecs::saved_world_load::FightOf(*plan).has_value());
}

// A fight a script forces, kept before the fight has it, is the fight's agenda from its request or the other's
// stand-still with the two taunts its agenda drew; the load reads it back with those taunts
TEST(CreatureWriteScriptPlan, AFightAScriptForcesIsWrittenAtItsRequestWithTheTauntsItsAgendaDrew)
{
	constexpr gamesave::Ref k_Self {.number = 7};
	components::CreatureMindState mind;
	mind.planner.current =
	    creature_planner::Plan {.desire = creature_desires::Desire::ObeyPlayer, .action = 21, .object = 9, .forced = true};
	// the other creature's draw first: rude, then its own: the taunt
	uint32_t draws = 0;
	mind.idle.agenda = creature_mind::FightCreature(9, [&draws](uint32_t) { return draws++ == 0 ? 1U : 0U; });
	for (const size_t step : {size_t {0}, size_t {1}})
	{
		mind.idle.step = step;
		mind.idle.stepStarted = step == 1;
		std::string why;
		const auto plan = ScriptHeldPlanOf(mind, k_Self, NumberRef, why);
		ASSERT_TRUE(plan.has_value()) << why;
		EXPECT_EQ(plan->desire, static_cast<uint32_t>(creature_desires::Desire::ObeyPlayer));
		EXPECT_EQ(plan->action, 21u);
		EXPECT_EQ(plan->count, 18u);
		EXPECT_EQ(plan->current, step);
		EXPECT_EQ(plan->state, step == 1 ? 1u : 0u);
		EXPECT_EQ(plan->other, (gamesave::Ref {.number = 109}));
		ASSERT_EQ(plan->steps.size(), 18u - step);
		EXPECT_EQ(plan->steps.at(4 - step).integer, 0xDEu);
		EXPECT_EQ(plan->steps.at(5 - step).integer, 0x46u);
		const auto kept = ecs::saved_world_load::FightRequestOf(*plan);
		ASSERT_TRUE(kept.has_value());
		EXPECT_EQ(kept->other, 109u);
		EXPECT_EQ(kept->step, step);
		EXPECT_EQ(kept->otherRude, std::optional(true));
		EXPECT_EQ(kept->rude, std::optional(false));
	}
	// past the stand-still the fight has it
	mind.idle.step = 2;
	std::string why;
	EXPECT_FALSE(ScriptHeldPlanOf(mind, k_Self, NumberRef, why).has_value());
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
