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

// The records of the worship site and what it shows, its totem, a town's artifact, and the dances with their key
// frames, key actions and groups (see docs/wiki/engine/saved-games.md#the-worship-sites-and-the-dances).
namespace openblack::gamesave
{

/// The parts every piece of a citadel writes: a building of several cells, its influence, its citadel and an object
struct CitadelPartFields
{
	MultiMapFixedFields base;
	float influence {0.0f};
	Ref citadel;
	Ref other;

	static auto FieldsOf(auto& part) { return std::tie(part.base, part.influence, part.citadel, part.other); }
	bool operator==(const CitadelPartFields&) const = default;
};

/// A worship site
struct WorshipSiteRecord
{
	static constexpr uint32_t k_Type = 0x3C;
	ThingPart thing;
	CitadelPartFields part;
	uint32_t tribeInfo {0};
	Ref showNeeds;
	Ref dance;
	RefList towns;
	Ref foodPot;
	std::array<uint32_t, 2> words {};
	uint8_t byte {0};
	uint32_t worshippers {0};
	uint32_t word {0};
	uint32_t cheatSpellSeed {0};
	RefList worshipping;
	RefList icons;
	/// The resources on their way, with their count
	Counted<uint32_t> resourcesOnTheWay;
	/// The chants stored, the sacrifices' value, the chants available, used and requested, and the chants' damage
	std::array<float, 6> chants {};
	int32_t infiniteChants {0};
	int32_t freeMaintenance {0};
	uint8_t slot {0};
	uint8_t isSupplyingChants {0};
	RefList villagersGoingHome;
	Ref totem;
	float overloaded {0.0f};
	Ref manaPath;
	float overloadPulsePhase {0.0f};
	float overloadPulse {0.0f};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.part, record.tribeInfo, record.showNeeds, record.dance, record.towns, record.foodPot,
		                record.words[0], record.words[1], record.byte, record.worshippers, record.word, record.cheatSpellSeed,
		                record.worshipping, record.icons, record.resourcesOnTheWay, record.chants[0], record.chants[1],
		                record.chants[2], record.chants[3], record.chants[4], record.chants[5], record.infiniteChants,
		                record.freeMaintenance, record.slot, record.isSupplyingChants, record.villagersGoingHome, record.totem,
		                record.overloaded, record.manaPath, record.overloadPulsePhase, record.overloadPulse);
	}
	bool operator==(const WorshipSiteRecord&) const = default;
};

/// What a worship site shows of its needs: an object and its three visuals, with their count
struct ShowNeedsRecord
{
	static constexpr uint32_t k_Type = 0x3E;
	ThingPart thing;
	Ref object;
	RefList visuals;

	static auto FieldsOf(auto& record) { return std::tie(record.object, record.visuals); }
	bool operator==(const ShowNeedsRecord&) const = default;
};

/// One visual of a worship site's needs: an object's parts, two words, an object and a word
struct ShowNeedsVisualsRecord
{
	static constexpr uint32_t k_Type = 0x3D;
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	std::array<uint32_t, 2> words {};
	Ref other;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.position, record.object, record.words[0], record.words[1], record.other, record.lastWord);
	}
	bool operator==(const ShowNeedsVisualsRecord&) const = default;
};

/// A worship site's totem: a piece of a citadel, its colour and its site
struct WorshipTotemRecord
{
	static constexpr uint32_t k_Type = 0x98;
	ThingPart thing;
	CitadelPartFields part;
	uint32_t specularColour {0};
	Ref site;

	static auto FieldsOf(auto& record) { return std::tie(record.part, record.specularColour, record.site); }
	bool operator==(const WorshipTotemRecord&) const = default;
};

/// A town's artifact: three objects, a word, two objects and two words
struct TownArtifactRecord
{
	static constexpr uint32_t k_Type = 0x103;
	ThingPart thing;
	std::array<Ref, 3> objects {};
	uint32_t word {0};
	std::array<Ref, 2> others {};
	std::array<uint32_t, 2> words {};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.objects[0], record.objects[1], record.objects[2], record.word, record.others[0],
		                record.others[1], record.words[0], record.words[1]);
	}
	bool operator==(const TownArtifactRecord&) const = default;
};

/// What every group behaviour writes after its position: its time, three words, its key frames, its groups and its
/// members, its speeds and its camera
struct GroupBehaviourFields
{
	PositionPart position;
	float time {0.0f};
	std::array<uint32_t, 3> unused {};
	RefList keyFrames;
	uint32_t groups {0};
	std::array<uint32_t, 2> editorValues {};
	uint32_t length {0};
	RefList danceGroups;
	RefList fixedGroups;
	RefList proportionGroups;
	uint32_t dancers {0};
	uint8_t proportionCounter {0};
	uint32_t denominator {0};
	float speed {0.0f};
	float lastSpeed {0.0f};
	RefList members;
	float groupAngle {0.0f};
	/// Whether it uses the dance camera, the camera is off, the camera was made, and it is to be deleted
	std::array<int32_t, 4> flags {};
	float cameraTravelTime {0.0f};
	uint32_t cameraNearTurns {0};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.position, part.time, part.unused[0], part.unused[1], part.unused[2], part.keyFrames, part.groups,
		                part.editorValues[0], part.editorValues[1], part.length, part.danceGroups, part.fixedGroups,
		                part.proportionGroups, part.dancers, part.proportionCounter, part.denominator, part.speed,
		                part.lastSpeed, part.members, part.groupAngle, part.flags[0], part.flags[1], part.flags[2],
		                part.flags[3], part.cameraTravelTime, part.cameraNearTurns);
	}
	bool operator==(const GroupBehaviourFields&) const = default;
};

/// A dance: a group behaviour, its kind and values, its target, its state and times, 16 bytes, a word and its footpath
/// link
struct DanceRecord
{
	static constexpr uint32_t k_Type = 0x27;
	ThingPart thing;
	GroupBehaviourFields group;
	uint32_t type {0};
	uint32_t unused {0};
	float worshipSpeed {0.0f};
	Ref target;
	std::array<uint32_t, 6> words {};
	std::array<uint8_t, 16> label {};
	uint32_t lastWord {0};
	Ref footpathLink;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.group, record.type, record.unused, record.worshipSpeed, record.target, record.words[0],
		                record.words[1], record.words[2], record.words[3], record.words[4], record.words[5], record.label,
		                record.lastWord, record.footpathLink);
	}
	bool operator==(const DanceRecord&) const = default;
};

/// A dance's key frame: a word, its actions with their count, and a word
struct DanceKeyFrameRecord
{
	static constexpr uint32_t k_Type = 0x7A;
	ThingPart thing;
	uint32_t word {0};
	RefList actions;
	uint32_t lastWord {0};

	static auto FieldsOf(auto& record) { return std::tie(record.word, record.actions, record.lastWord); }
	bool operator==(const DanceKeyFrameRecord&) const = default;
};

/// A key frame's action: its words with their count, a word and 56 bytes
struct DanceKeyActionRecord
{
	static constexpr uint32_t k_Type = 0x79;
	ThingPart thing;
	Counted<uint32_t> words;
	uint32_t word {0};
	std::array<uint8_t, 56> values {};

	static auto FieldsOf(auto& record) { return std::tie(record.words, record.word, record.values); }
	bool operator==(const DanceKeyActionRecord&) const = default;
};

/// A group of a dance
struct DanceGroupRecord
{
	static constexpr uint32_t k_Type = 0x7B;
	ThingPart thing;
	std::array<Ref, 2> objects {};
	RefList members;
	std::array<uint32_t, 4> words {};
	std::array<uint8_t, 8> eightBytes {};
	std::array<uint32_t, 3> secondWords {};
	std::array<uint8_t, 16> sixteenBytes {};
	float value {0.0f};
	Ref other;
	uint8_t byte {0};
	uint32_t thirdWord {0};
	int32_t firstInt {0};
	int32_t secondInt {0};
	int32_t thirdInt {0};
	MapCoords firstPosition {};
	MapCoords secondPosition {};
	uint32_t danceType {0};
	uint32_t fourthWord {0};
	/// A text of 64 characters
	std::array<char, 64> text {};
	/// Its proportion, 12 bytes
	std::array<uint8_t, 12> proportion {};
	/// A stack of words, with their count
	Counted<uint32_t> stack;
	uint32_t villagerType {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.objects[0], record.objects[1], record.members, record.words[0], record.words[1], record.words[2],
		                record.words[3], record.eightBytes, record.secondWords[0], record.secondWords[1], record.secondWords[2],
		                record.sixteenBytes, record.value, record.other, record.byte, record.thirdWord, record.firstInt,
		                record.secondInt, record.thirdInt, record.firstPosition, record.secondPosition, record.danceType,
		                record.fourthWord, record.text, record.proportion, record.stack, record.villagerType);
	}
	bool operator==(const DanceGroupRecord&) const = default;
};

} // namespace openblack::gamesave
