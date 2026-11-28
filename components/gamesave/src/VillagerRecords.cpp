/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerRecords.h"

#include <algorithm>

using namespace openblack::gamesave;

namespace
{
using S = VillagerStateSave;

/// The save of each state, as the exe's table of the villager's states names it
// clang-format off
constexpr std::array<VillagerStateSave, k_VillagerStateCount> k_StateSaves {
    S::None, S::None, S::None, S::None, S::InScript, S::Dance, S::Reaction, S::Reaction, S::Reaction,
    S::InspectionReaction, S::InFlying, S::Landed, S::Reaction, S::Dead, S::Dead, S::Dead, S::None, S::None, S::None,
    S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::WaitForAnim, S::InHand, S::Reaction, S::Reaction, S::None,
    S::FootPath, S::FootPath, S::Reaction, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None,
    S::Building, S::Building, S::Building, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None,
    S::Foresting, S::Building, S::None, S::None, S::Building, S::Fishing, S::Fishing, S::None, S::None, S::None,
    S::Worship, S::None, S::None, S::None, S::None, S::None, S::None, S::Farming, S::Farming, S::Farming, S::None,
    S::Football, S::Football, S::Football, S::Football, S::Football, S::Football, S::Football, S::Football,
    S::Football, S::Football, S::Football, S::Football, S::Football, S::Football, S::None, S::None, S::None, S::None,
    S::None, S::Dance, S::Shepherd, S::Shepherd, S::Shepherd, S::Shepherd, S::Shepherd, S::Shepherd, S::Shepherd,
    S::Shepherd, S::Shepherd, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None,
    S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None,
    S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None,
    S::InspectionReaction, S::InspectionReaction, S::None, S::None, S::None, S::None, S::None, S::None, S::Reaction,
    S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::None, S::InspectionReaction, S::InspectionReaction,
    S::InspectionReaction, S::MagicTreeReaction, S::MagicTreeReaction, S::None, S::None, S::Dance, S::Dance,
    S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::None,
    S::None, S::None, S::Reaction, S::Reaction, S::ShieldReaction, S::None, S::None, S::None, S::None, S::None,
    S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::None, S::Building,
    S::ClearArea, S::None, S::None, S::Building, S::Building, S::None, S::None, S::None, S::Dance, S::Reaction,
    S::Reaction, S::Reaction, S::None, S::None, S::ScriptPos, S::ScriptPos, S::Reaction, S::Reaction, S::Reaction,
    S::None, S::Reaction, S::Reaction, S::Reaction, S::Reaction, S::None, S::None, S::None, S::None, S::Worship,
    S::Reaction, S::Reaction, S::Fire, S::Fire, S::Fire, S::Fire, S::Fire, S::DiscipleNothingToDo, S::Football,
    S::Trader, S::Trader, S::Breeder, S::None, S::Reaction, S::Shepherd, S::None, S::Dance, S::Reaction, S::None,
    S::InspectObject, S::None, S::Reaction, S::HideInBuilding, S::HideInBuilding, S::None, S::None, S::None,
    S::Worship, S::None, S::None, S::ScriptPos, S::None, S::None, S::None, S::None, S::None, S::None, S::Reaction,
    S::None, S::Dance, S::None,
};
// clang-format on

/// The most reactions that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostReactions = 0x10000;

bool IsCircleState(uint8_t state)
{
	return state >= k_FirstCircleState && state <= k_LastCircleState;
}

/// Reads one value of its size into the first bytes: the sum counts its first byte and its size, as one value
template <size_t N>
bool GetSized(StreamReader& reader, std::array<uint8_t, 12>& bytes)
{
	std::array<uint8_t, N> value {};
	if (!reader.Get(value))
	{
		return false;
	}
	std::copy(value.begin(), value.end(), bytes.begin());
	return true;
}

template <size_t N>
void PutSized(StreamWriter& writer, const std::array<uint8_t, 12>& bytes)
{
	std::array<uint8_t, N> value {};
	std::copy_n(bytes.begin(), N, value.begin());
	writer.Put(value);
}

bool ReadStateValues(StreamReader& reader, VillagerStateSave save, uint8_t discipleType, std::vector<StateValue>& values,
                     const ReadNewObject& readNew)
{
	values.clear();
	for (const auto size : StateValueSizes(save, discipleType))
	{
		StateValue value {.size = size};
		bool read = false;
		switch (size)
		{
		case k_StateReference:
			read = ReadReference(reader, value.number, readNew);
			break;
		case 1:
			read = GetSized<1>(reader, value.bytes);
			break;
		case 2:
			read = GetSized<2>(reader, value.bytes);
			break;
		case 4:
			read = GetSized<4>(reader, value.bytes);
			break;
		case 8:
			read = GetSized<8>(reader, value.bytes);
			break;
		case 12:
			read = GetSized<12>(reader, value.bytes);
			break;
		default:
			break;
		}
		// Kept even when the read stops at it, so that the values read write back the same
		values.push_back(value);
		if (!read)
		{
			return false;
		}
	}
	return true;
}

void WriteStateValues(StreamWriter& writer, const std::vector<StateValue>& values, const WriteNewObject& writeNew)
{
	for (const auto& value : values)
	{
		switch (value.size)
		{
		case k_StateReference:
			WriteReference(writer, value.number, writeNew);
			break;
		case 1:
			PutSized<1>(writer, value.bytes);
			break;
		case 2:
			PutSized<2>(writer, value.bytes);
			break;
		case 4:
			PutSized<4>(writer, value.bytes);
			break;
		case 8:
			PutSized<8>(writer, value.bytes);
			break;
		case 12:
			PutSized<12>(writer, value.bytes);
			break;
		default:
			break;
		}
	}
}
} // namespace

VillagerStateSave openblack::gamesave::VillagerStateSaveOf(uint8_t state)
{
	return state < k_StateSaves.size() ? k_StateSaves.at(state) : VillagerStateSave::None;
}

std::vector<uint8_t> openblack::gamesave::StateValueSizes(VillagerStateSave save, uint8_t discipleType)
{
	switch (save)
	{
	case S::Breeder:
		return {k_StateReference, 8};
	case S::Building:
		return {k_StateReference, 4};
	case S::ClearArea:
		return {8, 4};
	case S::Dance:
	case S::InspectionReaction:
	case S::MagicTreeReaction:
		return {4, 4};
	case S::Dead:
		return {1};
	case S::DiscipleNothingToDo:
		if (discipleType == k_TraderDisciple)
		{
			return {4, k_StateReference};
		}
		break;
	case S::Farming:
		return {12, k_StateReference};
	case S::Fire:
		return {8, k_StateReference};
	case S::Fishing:
	case S::Foresting:
	case S::InspectObject:
		return {k_StateReference};
	case S::FootPath:
		return {k_StateReference, k_StateReference};
	case S::Football:
		return {1, 1, 1, k_StateReference};
	case S::HideInBuilding:
		return {4, 8};
	case S::InHand:
	case S::Worship:
		return {4};
	case S::InScript:
	case S::ScriptPos:
		return {8, 4, 4, 4};
	case S::Shepherd:
		return {2, 2};
	case S::ShieldReaction:
		return {8};
	case S::Trader:
		return {4, k_StateReference};
	case S::None:
	case S::InFlying:
	case S::Landed:
	case S::Reaction:
	case S::WaitForAnim:
		break;
	}
	return {};
}

bool openblack::gamesave::ReadPart(StreamReader& reader, WallHugPart& part, const ReadNewObject& readNew)
{
	reader.Get(part.turnsToStateChange);
	reader.Get(part.speed);
	reader.Get(part.angle);
	reader.Get(part.moveState);
	part.circle.reset();
	part.turnsToStepRebuild.reset();
	if (IsCircleState(part.moveState))
	{
		// Kept as it is read, so that a read that stops inside writes back what it read
		auto& circle = part.circle.emplace();
		reader.Get(circle.turnsToObject);
		reader.Get(circle.entryDistance);
		if (reader.Failed() || !ReadReference(reader, circle.object.number, readNew))
		{
			return false;
		}
		reader.Get(circle.index);
		reader.Get(circle.coords);
	}
	else if (part.moveState == k_StepThroughState)
	{
		int8_t turns = 0;
		reader.Get(turns);
		part.turnsToStepRebuild = turns;
	}
	if (reader.Failed() || !ReadReference(reader, part.footpath.number, readNew))
	{
		return false;
	}
	reader.Get(part.goal);
	if (reader.Failed() || !ReadReference(reader, part.target.number, readNew))
	{
		return false;
	}
	return reader.Get(part.step);
}

void openblack::gamesave::WritePart(StreamWriter& writer, const WallHugPart& part, const WriteNewObject& writeNew)
{
	writer.Put(part.turnsToStateChange);
	writer.Put(part.speed);
	writer.Put(part.angle);
	writer.Put(part.moveState);
	if (IsCircleState(part.moveState))
	{
		const auto circle = part.circle.value_or(CircleHug {});
		writer.Put(circle.turnsToObject);
		writer.Put(circle.entryDistance);
		WriteReference(writer, circle.object.number, writeNew);
		writer.Put(circle.index);
		writer.Put(circle.coords);
	}
	else if (part.moveState == k_StepThroughState)
	{
		writer.Put(part.turnsToStepRebuild.value_or(int8_t {0}));
	}
	WriteReference(writer, part.footpath.number, writeNew);
	writer.Put(part.goal);
	WriteReference(writer, part.target.number, writeNew);
	writer.Put(part.step);
}

bool openblack::gamesave::ReadPart(StreamReader& reader, LivingFields& part, const ReadNewObject& readNew)
{
	if (!ReadReference(reader, part.footpath.number, readNew) || !ReadReference(reader, part.footpathNode.number, readNew))
	{
		return false;
	}
	reader.Get(part.action);
	if (reader.Failed() || !ReadReference(reader, part.reaction.number, readNew))
	{
		return false;
	}
	reader.Get(part.firstWord);
	reader.Get(part.birthTurn);
	reader.Get(part.firstByte);
	if (reader.Failed() || !ReadReference(reader, part.dataPath.number, readNew) ||
	    !ReadReference(reader, part.scriptReminder.number, readNew))
	{
		return false;
	}
	reader.Get(part.status);
	if (reader.Failed() || !ReadReference(reader, part.flock.number, readNew) ||
	    !ReadReference(reader, part.otherObject.number, readNew) || !ReadReference(reader, part.footpathAgain.number, readNew))
	{
		return false;
	}
	reader.Get(part.secondWord);
	reader.Get(part.specular);
	reader.Get(part.secondByte);
	if (reader.Failed() || !ReadReference(reader, part.danceGroup.number, readNew))
	{
		return false;
	}
	reader.Get(part.thirdByte);
	uint32_t count = 0;
	if (!reader.Get(count) || count > k_MostReactions)
	{
		reader.Fail();
		return false;
	}
	part.reactionsDone.resize(count);
	for (auto& done : part.reactionsDone)
	{
		reader.Get(done.reaction);
		reader.Get(done.turn);
	}
	return !reader.Failed();
}

void openblack::gamesave::WritePart(StreamWriter& writer, const LivingFields& part, const WriteNewObject& writeNew)
{
	WriteReference(writer, part.footpath.number, writeNew);
	WriteReference(writer, part.footpathNode.number, writeNew);
	writer.Put(part.action);
	WriteReference(writer, part.reaction.number, writeNew);
	writer.Put(part.firstWord);
	writer.Put(part.birthTurn);
	writer.Put(part.firstByte);
	WriteReference(writer, part.dataPath.number, writeNew);
	WriteReference(writer, part.scriptReminder.number, writeNew);
	writer.Put(part.status);
	WriteReference(writer, part.flock.number, writeNew);
	WriteReference(writer, part.otherObject.number, writeNew);
	WriteReference(writer, part.footpathAgain.number, writeNew);
	writer.Put(part.secondWord);
	writer.Put(part.specular);
	writer.Put(part.secondByte);
	WriteReference(writer, part.danceGroup.number, writeNew);
	writer.Put(part.thirdByte);
	writer.Put(static_cast<uint32_t>(part.reactionsDone.size()));
	for (const auto& done : part.reactionsDone)
	{
		writer.Put(done.reaction);
		writer.Put(done.turn);
	}
}

bool openblack::gamesave::ReadPart(StreamReader& reader, VillagerValues& part, const ReadNewObject& readNew)
{
	if (!ReadPart(reader, part.move, readNew) || !ReadPart(reader, part.living, readNew))
	{
		return false;
	}
	auto& villager = part.villager;
	reader.Get(villager.flags);
	reader.Get(villager.food);
	reader.Get(villager.lastCheckTurn);
	reader.Get(villager.foodSpeedUp);
	reader.Get(villager.carriedObjectType);
	reader.Get(villager.discipleType);
	reader.Get(villager.foodHeld);
	reader.Get(villager.woodHeld);
	reader.Get(villager.pregnant);
	if (reader.Failed() || !ReadReference(reader, villager.home.number, readNew) ||
	    !ReadReference(reader, villager.town.number, readNew) || !ReadReference(reader, villager.mother.number, readNew))
	{
		return false;
	}
	reader.Get(villager.reserved[0]);
	reader.Get(villager.reserved[1]);
	// The top state's values, then the final state's
	return !reader.Failed() &&
	       ReadStateValues(reader, VillagerStateSaveOf(part.living.action[0]), villager.discipleType, part.topState, readNew) &&
	       ReadStateValues(reader, VillagerStateSaveOf(part.living.action[1]), villager.discipleType, part.finalState, readNew);
}

void openblack::gamesave::WritePart(StreamWriter& writer, const VillagerValues& part, const WriteNewObject& writeNew)
{
	WritePart(writer, part.move, writeNew);
	WritePart(writer, part.living, writeNew);
	const auto& villager = part.villager;
	writer.Put(villager.flags);
	writer.Put(villager.food);
	writer.Put(villager.lastCheckTurn);
	writer.Put(villager.foodSpeedUp);
	writer.Put(villager.carriedObjectType);
	writer.Put(villager.discipleType);
	writer.Put(villager.foodHeld);
	writer.Put(villager.woodHeld);
	writer.Put(villager.pregnant);
	WriteReference(writer, villager.home.number, writeNew);
	WriteReference(writer, villager.town.number, writeNew);
	WriteReference(writer, villager.mother.number, writeNew);
	writer.Put(villager.reserved[0]);
	writer.Put(villager.reserved[1]);
	WriteStateValues(writer, part.topState, writeNew);
	WriteStateValues(writer, part.finalState, writeNew);
}
