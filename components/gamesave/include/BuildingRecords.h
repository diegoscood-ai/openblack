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

#include "TownRecords.h"

// The records of a town's buildings that its lists pull in after its forests: the creche, the graveyard, the abodes
// and the fields, each an abode's parts with what its class adds. See
// docs/wiki/engine/saved-games.md#the-towns-buildings.
namespace openblack::gamesave
{

/// An abode
struct AbodeRecord
{
	static constexpr uint32_t k_Type = 0x7;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode); }
	bool operator==(const AbodeRecord&) const = default;
};

/// A town's creche, which writes an abode's record
struct CrecheRecord
{
	static constexpr uint32_t k_Type = 0x50;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode); }
	bool operator==(const CrecheRecord&) const = default;
};

/// A town's graveyard: an abode and its graves
struct GraveyardRecord
{
	static constexpr uint32_t k_Type = 0x51;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	uint32_t graves {0};

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode, record.graves); }
	bool operator==(const GraveyardRecord&) const = default;
};

/// A town's football pitch: an abode, then its match
struct FootballPitchRecord
{
	static constexpr uint32_t k_Type = 0x45;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	/// A script's cup final's: a thing, its ten and twenty slots and one more (a town's own match leaves them none)
	Ref finalThing;
	std::array<Ref, 10> finalSlots {};
	std::array<Ref, 20> finalPlaces {};
	Ref finalLast;
	/// Twelve values of 12 bytes each, written one after another, that nothing but the save and the load reads or
	/// writes: a new pitch's are 0, as the game's zero-filled one, and a loaded one keeps them as read
	std::array<uint8_t, 144> blocks {};
	/// 1 once the pitch is its town's
	uint32_t functional {0};
	/// Values the match's start clears and nothing else writes, kept as read
	uint32_t firstWord {0};
	uint8_t firstByte {0};
	/// The match's length the belief at its end reads, which nothing but the match's start writes (0)
	uint32_t matchLength {0};
	uint8_t secondByte {0};
	uint32_t secondWord {0};
	uint8_t thirdByte {0};
	uint32_t thirdWord {0};
	/// What the match is doing: 0 waiting, 1 playing, 3 after a goal
	uint32_t playState {0};
	/// 1 once the ball is on the middle for the kick-off
	uint32_t kickOffReady {0};
	/// The turns it has waited for players, then been played
	uint32_t turns {0};
	/// The turns of a dead ball (always 0 again)
	uint32_t deadBallTurns {0};
	/// The turns the Mexican wave rests after going round
	uint32_t waveRest {0};
	/// The goals of each side's tally, home then away
	uint8_t homeTally {0};
	uint8_t awayTally {0};
	/// How many of the 40 places round the pitch were taken, and the next to rise in the wave
	uint32_t placesTaken {0};
	uint32_t waveIndex {0};
	/// The first villager to come, who keeps the game
	Ref referee;
	/// Each side's players, then each side ranked by distance to the goal it attacks
	RefList homeTeam;
	RefList awayTeam;
	RefList homeRanked;
	RefList awayRanked;
	/// Each side's attacker and defender nearest the ball
	std::array<Ref, 2> nearestAttackers {};
	std::array<Ref, 2> nearestDefenders {};
	/// The player fetching a ball that has left the pitch
	Ref ballChaser;
	/// The watchers in the 40 places round the pitch
	std::array<Ref, 40> places {};
	/// The ball's heading the goal test last found
	Point ballHeading {};
	/// The ball it plays with
	Ref ball;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.abode, record.finalThing, record.finalSlots, record.finalPlaces, record.finalLast,
		                record.blocks, record.functional, record.firstWord, record.firstByte, record.matchLength,
		                record.secondByte, record.secondWord, record.thirdByte, record.thirdWord, record.playState,
		                record.kickOffReady, record.turns, record.deadBallTurns, record.waveRest, record.homeTally,
		                record.awayTally, record.placesTaken, record.waveIndex, record.referee, record.homeTeam,
		                record.awayTeam, record.homeRanked, record.awayRanked, record.nearestAttackers, record.nearestDefenders,
		                record.ballChaser, record.places, record.ballHeading, record.ball);
	}
	bool operator==(const FootballPitchRecord&) const = default;
};

/// A field: an abode with its crop's kind, its town and its farmers
struct FieldRecord
{
	static constexpr uint32_t k_Type = 0x32;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	/// The number of the field's kind in the game's tables of kinds
	uint32_t typeInfo {0};
	uint32_t firstWord {0};
	uint32_t secondWord {0};
	Ref town;
	int32_t thirdWord {0};
	uint8_t firstByte {0};
	uint32_t fourthWord {0};
	RefList farmers;
	/// The low byte of a word the field keeps: the game writes one of its four bytes
	uint8_t lastByte {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.abode, record.typeInfo, record.firstWord, record.secondWord, record.town,
		                record.thirdWord, record.firstByte, record.fourthWord, record.farmers, record.lastByte);
	}
	bool operator==(const FieldRecord&) const = default;
};

} // namespace openblack::gamesave
