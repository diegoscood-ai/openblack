/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameTailRecords.h"

#include <type_traits>

using namespace openblack::gamesave;

namespace
{
/// The most exclusions that a read accepts, so that bad bytes cannot ask for a huge list
constexpr uint32_t k_MostExclusions = 0x10000;

/// The tail's values hold no object part, so their visitors need no thing of their own
const ThingPart k_NoThing {};

/// The save numbers of the camera's modes that write values of their own
constexpr uint32_t k_FollowMode = 0x46;
constexpr uint32_t k_HeadingMode = 0x48;
constexpr uint32_t k_FreeMode = 0x65;

/// Visits the tail's values in the game's order. The visitor reads or writes each value; the exclusions, whose count
/// comes before a word of their own, are left to the caller.
template <typename Visitor, typename Tail>
bool VisitUpToExclusions(Visitor& fields, Tail& part)
{
	bool good = true;
	const auto visit = [&](auto& value) {
		if constexpr (std::is_same_v<decltype(fields(value)), bool>)
		{
			good = good && fields(value);
		}
		else
		{
			fields(value);
		}
	};
	for (auto& list : part.helpLists)
	{
		visit(list);
	}
	for (auto& value : part.helpWords)
	{
		visit(value);
	}
	for (auto& object : part.helpObjects)
	{
		visit(object);
	}
	visit(part.helpHalf);
	visit(part.helpWord);
	visit(part.helpObject);
	for (auto& value : part.helpSecondWords)
	{
		visit(value);
	}
	visit(part.helpValues);
	visit(part.helpThirdWord);
	visit(part.helpArray);
	for (auto& value : part.helpInts)
	{
		visit(value);
	}
	visit(part.helpSecondHalf);
	for (auto& value : part.helpFourthWords)
	{
		visit(value);
	}
	visit(part.helpThirdHalf);
	for (auto& value : part.helpLastWords)
	{
		visit(value);
	}
	visit(part.helpEntries);
	for (auto& object : part.scriptObjects)
	{
		visit(object);
		if (!good)
		{
			return false;
		}
	}
	for (auto& value : part.scriptWords)
	{
		visit(value);
	}
	for (auto& point : part.scriptPoints)
	{
		visit(point);
	}
	for (auto& value : part.scriptSecondWords)
	{
		visit(value);
	}
	for (auto& object : part.scriptRefs)
	{
		visit(object);
	}
	for (auto& value : part.scriptLastWords)
	{
		visit(value);
	}
	visit(part.point);
	for (auto& object : part.objects)
	{
		visit(object);
	}
	visit(part.exclusionVersion);
	for (auto& value : part.exclusionValues)
	{
		visit(value);
	}
	visit(part.forceFieldPoints);
	return good;
}

/// Visits the values after the exclusions, to the end of the file
template <typename Visitor, typename Tail>
bool VisitAfterExclusions(Visitor& fields, Tail& part)
{
	bool good = true;
	const auto visit = [&](auto& value) {
		if constexpr (std::is_same_v<decltype(fields(value)), bool>)
		{
			good = good && fields(value);
		}
		else
		{
			fields(value);
		}
	};
	for (auto& value : part.cameraWords)
	{
		visit(value);
	}
	visit(part.snow);
	visit(part.snowSecond);
	for (auto& value : part.snowWords)
	{
		visit(value);
	}
	visit(part.weather);
	visit(part.word);
	visit(part.scriptRebootRequested);
	for (auto& value : part.words)
	{
		visit(value);
	}
	for (auto& bookmark : part.bookmarks)
	{
		visit(bookmark);
	}
	for (auto& value : part.bookmarkWords)
	{
		visit(value);
	}
	visit(part.landBalance);
	visit(part.handWord);
	visit(part.handObject);
	for (auto& value : part.handWords)
	{
		visit(value);
	}
	visit(part.handPoint);
	for (auto& object : part.handObjects)
	{
		visit(object);
	}
	visit(part.handByte);
	visit(part.sounds);
	for (auto& value : part.audioWords)
	{
		visit(value);
	}
	visit(part.audioObject);
	visit(part.audioWord);
	visit(part.roomsAvailable);
	for (auto& value : part.buttonConfig)
	{
		visit(value);
	}
	for (size_t i = 0; i < part.didYouKnow.size(); ++i)
	{
		visit(part.didYouKnow.at(i));
		visit(part.didYouKnowWords.at(i));
	}
	visit(part.challenges);
	visit(part.creatureCurse);
	return good;
}
} // namespace

bool openblack::gamesave::ReadPart(StreamReader& reader, GameTail& part, const ReadNewObject& readNew)
{
	FieldReader fields(reader, k_NoThing, readNew);
	if (!VisitUpToExclusions(fields, part) || reader.Failed())
	{
		return false;
	}
	uint32_t exclusions = 0;
	if (!reader.Get(exclusions) || exclusions > k_MostExclusions || !reader.Get(part.exclusionNameSize))
	{
		reader.Fail();
		return false;
	}
	part.exclusions.resize(exclusions);
	for (auto& name : part.exclusions)
	{
		if (!fields(name))
		{
			return false;
		}
	}
	return VisitAfterExclusions(fields, part) && !reader.Failed();
}

void openblack::gamesave::WritePart(StreamWriter& writer, const GameTail& part, const WriteNewObject& writeNew)
{
	FieldWriter fields(writer, k_NoThing, writeNew);
	VisitUpToExclusions(fields, part);
	writer.Put(static_cast<uint32_t>(part.exclusions.size()));
	writer.Put(part.exclusionNameSize);
	for (const auto& name : part.exclusions)
	{
		fields(name);
	}
	VisitAfterExclusions(fields, part);
}

bool openblack::gamesave::ReadPart(StreamReader& reader, CameraModePart& part, const ReadNewObject& readNew)
{
	if (!reader.Get(part.mode))
	{
		return false;
	}
	FieldReader fields(reader, k_NoThing, readNew);
	switch (part.mode)
	{
	case 0:
		return true;
	case k_FollowMode:
		for (auto& value : part.words)
		{
			fields(value);
		}
		fields(part.points[0]);
		fields(part.points[1]);
		return fields(part.object) && fields(part.lastWord);
	case k_HeadingMode:
		for (size_t i = 0; i < 4; ++i)
		{
			fields(part.words.at(i));
		}
		return fields(part.object);
	case k_FreeMode:
		for (size_t i = 0; i < 4; ++i)
		{
			fields(part.words.at(i));
		}
		return !reader.Failed();
	default:
		// A mode whose values are not read yet
		reader.Fail();
		return false;
	}
}

void openblack::gamesave::WritePart(StreamWriter& writer, const CameraModePart& part, const WriteNewObject& writeNew)
{
	writer.Put(part.mode);
	FieldWriter fields(writer, k_NoThing, writeNew);
	switch (part.mode)
	{
	case k_FollowMode:
		for (const auto value : part.words)
		{
			fields(value);
		}
		fields(part.points[0]);
		fields(part.points[1]);
		fields(part.object);
		fields(part.lastWord);
		break;
	case k_HeadingMode:
		for (size_t i = 0; i < 4; ++i)
		{
			fields(part.words.at(i));
		}
		fields(part.object);
		break;
	case k_FreeMode:
		for (size_t i = 0; i < 4; ++i)
		{
			fields(part.words.at(i));
		}
		break;
	default:
		break;
	}
}
