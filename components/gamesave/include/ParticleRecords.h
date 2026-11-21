/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <tuple>

#include "SaveFields.h"

// The records of the particle manager and its collections of atoms (see
// docs/wiki/engine/saved-games.md#the-particles-the-spell-icons-and-the-destruction-meshes).
namespace openblack::gamesave
{

/// The two numbers the game saves for a persistent object instead of a reference
struct PersistentId
{
	uint32_t first {0};
	uint32_t second {0};

	static auto FieldsOf(auto& part) { return std::tie(part.first, part.second); }
	bool operator==(const PersistentId&) const = default;
};

/// One target of a spell: a word and its object
struct SpellTarget
{
	uint32_t word {0};
	Ref object;

	static auto FieldsOf(auto& part) { return std::tie(part.word, part.object); }
	bool operator==(const SpellTarget&) const = default;
};

/// A spell's targets: its objects with their count, its points with their count, and a word
struct SpellTargets
{
	Listed<SpellTarget> targets;
	Counted<Point> points;
	uint32_t word {0};

	static auto FieldsOf(auto& part) { return std::tie(part.targets, part.points, part.word); }
	bool operator==(const SpellTargets&) const = default;
};

/// The particle manager
struct ParticleManagerRecord
{
	static constexpr uint32_t k_Type = 0xA4;
	ThingPart thing;
	int32_t word {0};
	Ref object;
	/// Its particle systems, with their count
	RefList systems;
	/// How it processes, 60 bytes
	std::array<uint8_t, 60> processInfo {};
	Point point {};
	uint8_t byte {0};
	float value {0.0f};
	uint32_t secondWord {0};
	std::array<Point, 3> points {};
	std::array<uint32_t, 3> words {};
	Ref other;
	std::array<uint8_t, 2> bytes {};
	PersistentId persistent;
	SpellTargets targets;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.word, record.object, record.systems, record.processInfo, record.point, record.byte, record.value,
		                record.secondWord, record.points[0], record.points[1], record.points[2], record.words[0],
		                record.words[1], record.words[2], record.other, record.bytes[0], record.bytes[1], record.persistent,
		                record.targets);
	}
	bool operator==(const ParticleManagerRecord&) const = default;
};

/// A collection of atoms of the particles
struct AtomCollectionRecord
{
	static constexpr uint32_t k_Type = 0xA3;
	ThingPart thing;
	Listed<PersistentId> persistents;
	RefList systems;
	float value {0.0f};
	Ref object;
	uint32_t word {0};
	uint8_t byte {0};
	Ref other;
	RefList secondSystems;
	Ref third;
	uint32_t lastWord {0};
	uint8_t lastByte {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.persistents, record.systems, record.value, record.object, record.word, record.byte, record.other,
		                record.secondSystems, record.third, record.lastWord, record.lastByte);
	}
	bool operator==(const AtomCollectionRecord&) const = default;
};

} // namespace openblack::gamesave
