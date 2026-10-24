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

// The records of a player's interface, its hand and leash, what a computer player saw, a player's sub-actions and the
// particles' container (see docs/wiki/engine/saved-games.md#the-planned-buildings-the-players-interface-and-the-particles).
namespace openblack::gamesave
{

/// A player's interface: its position and its status
struct PlayerInterfaceRecord
{
	static constexpr uint32_t k_Type = 0x64;
	ThingPart thing;
	PositionPart position;
	Ref status;

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.status); }
	bool operator==(const PlayerInterfaceRecord&) const = default;
};

/// The word the interface's status starts with
inline constexpr uint32_t k_InterfaceStatusMark = 0xA2C2A;

/// A player's hand: a byte and the objects it holds, with their count
struct MagicHandPart
{
	uint8_t byte {0};
	RefList held;

	static auto FieldsOf(auto& part) { return std::tie(part.byte, part.held); }
	bool operator==(const MagicHandPart&) const = default;
};

/// The status of a player's interface: the hand, its throw, the camera and the leash
struct InterfaceStatusRecord
{
	static constexpr uint32_t k_Type = 0x65;
	ThingPart thing;
	PositionPart position;
	uint32_t mark {k_InterfaceStatusMark};
	uint8_t playerNumber {0};
	uint32_t word {0};
	/// The throw's velocity and turn, the hand's position and angles: one value of 48 bytes
	std::array<float, 12> throwAndHand {};
	MagicHandPart hand;
	uint8_t handHoldingSomething {0};
	std::array<uint8_t, 8> coord {};
	uint32_t secondWord {0};
	uint32_t thirdWord {0};
	Point point {};
	Point cameraPosition {};
	Point cameraFocus {};
	Point reportedHandPosition {};
	Point secondPoint {};
	float handSpeed {0.0f};
	float firstFloat {0.0f};
	float secondFloat {0.0f};
	Point handVelocity {};
	float thirdFloat {0.0f};
	uint32_t fourthWord {0};
	Ref guidance;
	Ref leashStatus;
	uint32_t fifthWord {0};
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.mark, record.playerNumber, record.word, record.throwAndHand, record.hand,
		                record.handHoldingSomething, record.coord, record.secondWord, record.thirdWord, record.point,
		                record.cameraPosition, record.cameraFocus, record.reportedHandPosition, record.secondPoint,
		                record.handSpeed, record.firstFloat, record.secondFloat, record.handVelocity, record.thirdFloat,
		                record.fourthWord, record.guidance, record.leashStatus, record.fifthWord, record.lastWord);
	}
	bool operator==(const InterfaceStatusRecord&) const = default;
};

/// The leash of a player's hand
struct LeashStatusRecord
{
	static constexpr uint32_t k_Type = 0x102;
	ThingPart thing;
	std::array<uint32_t, 3> words {};
	uint8_t byte {0};
	uint32_t fourthWord {0};
	Ref object;
	uint32_t fifthWord {0};
	uint32_t sixthWord {0};
	Ref other;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.words[0], record.words[1], record.words[2], record.byte, record.fourthWord, record.object,
		                record.fifthWord, record.sixthWord, record.other, record.lastWord);
	}
	bool operator==(const LeashStatusRecord&) const = default;
};

/// Something a computer player saw, and a word
struct ComputerSeenRecord
{
	static constexpr uint32_t k_Type = 0x67;
	ThingPart thing;
	Ref object;
	uint32_t word {0};

	static auto FieldsOf(auto& record) { return std::tie(record.object, record.word); }
	bool operator==(const ComputerSeenRecord&) const = default;
};

/// A player's sub-action: a word and its arguments, with their count as a raw word
struct PlayerSubActionRecord
{
	static constexpr uint32_t k_Type = 0xFA;
	ThingPart thing;
	uint32_t word {0};
	RefList arguments;

	static auto FieldsOf(auto& record) { return std::tie(record.word, record.arguments); }
	bool operator==(const PlayerSubActionRecord&) const = default;
};

/// An argument of a player's sub-action: a word, an object, 64 bytes one by one with their count, a word, an object, a
/// word and a point
struct PlayerSubActionArgumentRecord
{
	static constexpr uint32_t k_Type = 0xFB;
	ThingPart thing;
	uint32_t word {0};
	Ref object;
	Counted<uint8_t> bytes;
	uint32_t secondWord {0};
	Ref other;
	uint32_t thirdWord {0};
	Point point {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.word, record.object, record.bytes, record.secondWord, record.other, record.thirdWord,
		                record.point);
	}
	bool operator==(const PlayerSubActionArgumentRecord&) const = default;
};

/// A container of particles: its position, a byte, an object, its particle system and two words
struct ParticleContainerRecord
{
	static constexpr uint32_t k_Type = 0x6E;
	ThingPart thing;
	PositionPart position;
	uint8_t byte {0};
	Ref object;
	Ref system;
	uint32_t word {0};
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.byte, record.object, record.system, record.word, record.lastWord);
	}
	bool operator==(const ParticleContainerRecord&) const = default;
};

/// The game's interface to a particle system: the system
struct ParticleInterfaceRecord
{
	static constexpr uint32_t k_Type = 0xA5;
	ThingPart thing;
	Ref system;

	static auto FieldsOf(auto& record) { return std::tie(record.system); }
	bool operator==(const ParticleInterfaceRecord&) const = default;
};

} // namespace openblack::gamesave
