/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <tuple>
#include <vector>

#include "VillagerRecords.h"

// The record of a creature in the world file: a living thing's parts, then the creature's own values. Its mind is not
// here: the game keeps it in a mind file beside the world file. See docs/wiki/engine/saved-games.md#the-creature.
namespace openblack::gamesave
{

/// The version of the creature's values that the game writes. The mind file of the same game has the same version.
inline constexpr uint32_t k_CreatureVersion = 33;
/// The mind's processing word comes only after this version
inline constexpr uint32_t k_LastCreatureVersionWithoutProcessing = 25;
/// The number of creature spells, in the order of the game's tables (freeze first)
inline constexpr size_t k_CreatureSpellCount = 16;
/// The number of hand glow types that keep a creed power
inline constexpr size_t k_CreedPowerCount = 4;
/// The most steps that a creature's agenda has
inline constexpr uint32_t k_MostAgendaSteps = 32;

/// One creature spell: its place, its phase (0 off, 1 waits to start, 2 starts, 3 holds, 4 finishes), the turns left
/// of the phase, the value it found when it started, the turns it holds for (-1 for all time) and its miracle
struct CreatureSpellSlot
{
	uint32_t spell {0};
	uint32_t phase {0};
	int32_t turnsLeft {0};
	float found {0.0f};
	int32_t holdTurns {0};
	Ref miracle;

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.spell, part.phase, part.turnsLeft, part.found, part.holdTurns, part.miracle);
	}
	bool operator==(const CreatureSpellSlot&) const = default;
};

/// A spell that waits for another of its kind to finish: the spell, the seconds it holds for and its miracle
struct QueuedCreatureSpell
{
	uint32_t spell {0};
	float seconds {0.0f};
	Ref miracle;

	static auto FieldsOf(auto& part) { return std::tie(part.spell, part.seconds, part.miracle); }
	bool operator==(const QueuedCreatureSpell&) const = default;
};

/// A step of the agenda of the creature's plan
struct AgendaStep
{
	uint32_t action {0};
	Ref object;
	uint32_t integer {0};
	Point point {};
	float real {0.0f};
	uint32_t spell {0};
	/// A word the game sets to 0 only
	uint32_t word {0};
	Ref other;
	uint32_t orderKind {0};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.action, part.object, part.integer, part.point, part.real, part.spell, part.word, part.other,
		                part.orderKind);
	}
	bool operator==(const AgendaStep&) const = default;
};

/// The plan a creature keeps in the save: what it wants, what it does, the objects of its activity, its action and what
/// it uses, and its agenda, whose steps run from the current one to the count
struct KeptCreaturePlan
{
	uint32_t desire {0};
	uint32_t action {0};
	std::array<Ref, 3> objects {};
	uint32_t firstWord {0};
	uint32_t current {0};
	uint32_t state {0};
	uint32_t count {0};
	std::array<uint32_t, 3> words {};
	std::vector<AgendaStep> steps;
	Ref creature;
	Ref other;

	bool operator==(const KeptCreaturePlan&) const = default;
};

/// The scale and the power of a creed
struct CreedPower
{
	float scale {0.0f};
	float power {0.0f};

	static auto FieldsOf(auto& part) { return std::tie(part.scale, part.power); }
	bool operator==(const CreedPower&) const = default;
};

/// The creature's own values, after a living thing's part
struct CreatureFields
{
	uint32_t version {k_CreatureVersion};
	/// 1 when the game was saved in the temple
	uint32_t savedInTemple {0};
	/// Its place in the game's list of creatures, which names its mind file, and its player
	uint8_t index {0xFF};
	uint8_t owner {0xFF};
	std::array<CreatureSpellSlot, k_CreatureSpellCount> spells {};
	std::vector<QueuedCreatureSpell> queued;
	/// 0 while a spell or the lesson choice holds the mind still; not written up to the version without it
	uint32_t mindProcessing {1};
	float largestSize {0.0f};
	float smallestSize {0.0f};
	uint8_t mayGoThroughVortex {0};
	uint8_t fizzedForGood {0};
	float fizz {0.0f};
	float fizzTarget {0.0f};
	float fizzPerSecond {0.0f};
	float freeze {0.0f};
	uint32_t word {0};
	/// The visual effects of the spells on it, and the particles in its two hands while it casts
	std::array<Ref, 3> spellVisuals {};
	std::array<Ref, 2> handParticles {};
	/// Where an order of the player aims it, the particles there and the object
	Point targetPoint {};
	Ref targetParticles;
	Ref targetObject;
	uint8_t hasTarget {0};
	Ref arena;
	/// Its pen
	MapCoords home {};
	float fightHealth {0.0f};
	uint32_t autoFight {0};
	/// The villagers that dance with it, with their count
	RefList dancePartners;
	Ref dance;
	uint32_t canDie {0};
	float strength {0.0f};
	float size {0.0f};
	float alignment {0.0f};
	float animationPace {0.0f};
	uint32_t autoScale {0};
	float autoScaleShare {0.0f};
	/// Whether the plan follows: a script controls it, it fights, or villagers dance with it
	uint32_t keepsPlan {0};
	KeptCreaturePlan plan;
	std::array<CreedPower, k_CreedPowerCount> creedPowers {};

	bool operator==(const CreatureFields&) const = default;
};

bool ReadPart(StreamReader& reader, CreatureFields& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const CreatureFields& part, const WriteNewObject& writeNew);

/// A creature
struct CreatureRecord
{
	static constexpr uint32_t k_Type = 0x69;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	WallHugPart move;
	LivingFields living;
	CreatureFields creature;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.mobile, record.move, record.living, record.creature);
	}
	bool operator==(const CreatureRecord&) const = default;
};

} // namespace openblack::gamesave
