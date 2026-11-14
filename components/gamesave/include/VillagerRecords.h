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
#include <optional>
#include <tuple>
#include <vector>

#include "SaveFields.h"

// The records of the villagers: the part every moving thing that walks round obstacles writes, a living thing's part,
// the villager's own values and the values of its two states. A villager's state decides which values its state
// writes; the table of the states is the exe's own, which differs in many states from the earlier version the
// decompilation follows. See docs/wiki/engine/saved-games.md#the-villagers.
namespace openblack::gamesave
{

/// The object a thing walks around, and where: written only in the move states that circle
struct CircleHug
{
	uint8_t turnsToObject {0};
	uint32_t entryDistance {0};
	Ref object;
	int32_t index {0};
	MapCoords coords {};

	bool operator==(const CircleHug&) const = default;
};

/// One step of a move: x and z in 16.16 parts of a cell, with the height between them
struct MoveStep
{
	int32_t x {0};
	float altitude {0.0f};
	int32_t z {0};

	bool operator==(const MoveStep&) const = default;
};
static_assert(sizeof(MoveStep) == 12);

/// The move states for which the record keeps the object the thing walks around, and the state that steps through
inline constexpr uint8_t k_FirstCircleState = 0x0C;
inline constexpr uint8_t k_LastCircleState = 0x12;
inline constexpr uint8_t k_StepThroughState = 0x0B;

/// What a moving thing that walks round obstacles adds after the moving thing's word
struct WallHugPart
{
	int16_t turnsToStateChange {0};
	uint16_t speed {0};
	uint16_t angle {0};
	uint8_t moveState {0};
	/// Present only in the circle states
	std::optional<CircleHug> circle;
	/// Present only in the state that steps through
	std::optional<int8_t> turnsToStepRebuild;
	Ref footpath;
	MapCoords goal {};
	Ref target;
	MoveStep step {};

	bool operator==(const WallHugPart&) const = default;
};

/// A reaction that the living thing did, and the turn: two values
struct ReactionDone
{
	uint32_t reaction {0};
	uint32_t turn {0};

	bool operator==(const ReactionDone&) const = default;
};

/// What a living thing adds
struct LivingFields
{
	Ref footpath;
	Ref footpathNode;
	/// The top, final and third states, a byte the game does not use and the turns since the state changed
	std::array<uint8_t, 6> action {};
	Ref reaction;
	uint32_t firstWord {0};
	uint32_t birthTurn {0};
	uint8_t firstByte {0};
	Ref dataPath;
	Ref scriptReminder;
	uint16_t status {0};
	Ref flock;
	Ref otherObject;
	Ref footpathAgain;
	uint32_t secondWord {0};
	/// Blue, green, red and alpha
	std::array<uint8_t, 4> specular {};
	uint8_t secondByte {0};
	Ref danceGroup;
	uint8_t thirdByte {0};
	std::vector<ReactionDone> reactionsDone;

	bool operator==(const LivingFields&) const = default;
};

/// What a villager adds
struct VillagerFields
{
	uint16_t flags {0};
	float food {0.0f};
	int32_t lastCheckTurn {0};
	uint8_t foodSpeedUp {0};
	uint8_t carriedObjectType {0};
	uint8_t discipleType {0};
	int16_t foodHeld {0};
	int16_t woodHeld {0};
	int16_t pregnant {0};
	Ref home;
	Ref town;
	Ref mother;
	/// Two words the game writes as zero
	std::array<uint32_t, 2> reserved {};

	bool operator==(const VillagerFields&) const = default;
};

/// The values that each of the villager's states saves, by the function its state's entry names
enum class VillagerStateSave : uint8_t
{
	None,
	Breeder,
	Building,
	ClearArea,
	Dance,
	Dead,
	DiscipleNothingToDo,
	Farming,
	Fire,
	Fishing,
	FootPath,
	Football,
	Foresting,
	HideInBuilding,
	InFlying,
	InHand,
	InScript,
	InspectObject,
	InspectionReaction,
	Landed,
	MagicTreeReaction,
	Reaction,
	ScriptPos,
	Shepherd,
	ShieldReaction,
	Trader,
	WaitForAnim,
	Worship,
};

/// The villager's states in the exe's table
inline constexpr size_t k_VillagerStateCount = 255;
/// The save of a villager's state, from the exe's table (none for a state past it)
[[nodiscard]] VillagerStateSave VillagerStateSaveOf(uint8_t state);

/// The trader's discipline: its state of nothing to do saves what a trader saves
inline constexpr uint8_t k_TraderDisciple = 9;

/// The size of a reference among a state's values
inline constexpr uint8_t k_StateReference = 0;

/// The values a state's save writes, in order: their sizes, k_StateReference for a reference
[[nodiscard]] std::vector<uint8_t> StateValueSizes(VillagerStateSave save, uint8_t discipleType);

/// One value of a state's save: a reference, or one value of 1 to 12 bytes
struct StateValue
{
	uint8_t size {k_StateReference};
	std::array<uint8_t, 12> bytes {};
	ObjectNumber number {0};

	bool operator==(const StateValue&) const = default;
};

/// Everything a villager writes after the moving thing's word, its states' values included
struct VillagerValues
{
	WallHugPart move;
	LivingFields living;
	VillagerFields villager;
	/// The values its top state saves, then those of its final state
	std::vector<StateValue> topState;
	std::vector<StateValue> finalState;

	bool operator==(const VillagerValues&) const = default;
};

bool ReadPart(StreamReader& reader, WallHugPart& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const WallHugPart& part, const WriteNewObject& writeNew);
bool ReadPart(StreamReader& reader, LivingFields& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const LivingFields& part, const WriteNewObject& writeNew);
bool ReadPart(StreamReader& reader, VillagerValues& part, const ReadNewObject& readNew);
void WritePart(StreamWriter& writer, const VillagerValues& part, const WriteNewObject& writeNew);

/// A villager
struct VillagerRecord
{
	static constexpr uint32_t k_Type = 0x6;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	VillagerValues values;

	static auto FieldsOf(auto& record) { return std::tie(record.position, record.object, record.mobile, record.values); }
	bool operator==(const VillagerRecord&) const = default;
};

/// A special villager: a villager and the row of its kind
struct SpecialVillagerRecord
{
	static constexpr uint32_t k_Type = 0x76;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MobilePart mobile;
	VillagerValues values;
	uint32_t infoIndex {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.mobile, record.values, record.infoIndex);
	}
	bool operator==(const SpecialVillagerRecord&) const = default;
};

} // namespace openblack::gamesave
