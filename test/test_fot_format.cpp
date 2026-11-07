/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footpath files (.fot) that come with each land, read as the game reads them: a list of the links between
// buildings and the footpaths they join, then the list of footpaths, each a list of nodes at map points, every thing
// numbered as it first appears and named by its number after that, with a running checksum. A file written in the test
// reads back field by field; the story lands' own files read whole, every footpath a link joins being one of the land's.
// docs/wiki/project/progress/engine/asset_formats.md

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "FileSystem/MemoryStream.h"
#include "Serializer/GameThingSerializer.h"

using namespace openblack;
using serializer::GameThingSerializer;
using serializer::GameThingType;
using serializer::MapCoords;

namespace
{
/// Writes a footpath file as the game reads one: every value adds its first byte and its size to the running sum
class FotWriter
{
public:
	template <typename T>
	void Put(const T& value)
	{
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		_bytes.insert(_bytes.end(), bytes, bytes + sizeof(T));
		_sum += static_cast<uint32_t>(bytes[0]) + static_cast<uint32_t>(sizeof(T));
	}
	/// A thing's first appearance: its number, its type, its player and the sum so far
	void NewThing(GameThingType type)
	{
		Put(static_cast<uint32_t>(++_things));
		Put(type);
		Put(uint32_t {0});
		Put(_sum);
	}
	/// A thing seen before, by its number
	void Again(uint32_t number) { Put(number); }
	void Base(uint32_t a, uint8_t b)
	{
		Put(a);
		Put(b);
	}
	void Node(MapCoords coords, uint8_t flags)
	{
		NewThing(GameThingType::FootpathNode);
		Base(10, 1);
		Put(coords);
		Put(flags);
	}
	[[nodiscard]] uint32_t Things() const { return _things; }
	[[nodiscard]] std::vector<uint8_t> Bytes() const { return _bytes; }

private:
	std::vector<uint8_t> _bytes;
	uint32_t _sum {0};
	uint32_t _things {0};
};

struct Read
{
	std::vector<GameThingSerializer::FootpathLinkSave> links;
	std::vector<GameThingSerializer::Footpath> footpaths;
};

Read ReadFot(std::vector<uint8_t> bytes)
{
	filesystem::MemoryStream stream(std::move(bytes));
	GameThingSerializer serializer(stream);
	Read read;
	read.links = serializer.DeserializeList<GameThingSerializer::FootpathLinkSave>();
	read.footpaths = serializer.DeserializeList<GameThingSerializer::Footpath>();
	EXPECT_EQ(stream.Position(), stream.Size());
	return read;
}

class FotFormat: public ::testing::Test
{
protected:
	/// What the reader logs, where a sum that does not match shows
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("game");
		}
		_log = std::make_shared<spdlog::sinks::ostream_sink_mt>(_logged);
		spdlog::get("game")->sinks().push_back(_log);
	}
	void TearDown() override
	{
		auto& sinks = spdlog::get("game")->sinks();
		sinks.erase(std::remove(sinks.begin(), sinks.end(), _log), sinks.end());
	}
	/// Whether every checksum read matched the running sum
	[[nodiscard]] bool SumsMatched() const { return _logged.str().find("Failed checksum") == std::string::npos; }

private:
	std::ostringstream _logged;
	std::shared_ptr<spdlog::sinks::ostream_sink_mt> _log;
};
} // namespace

// A file of one link joining a footpath of two nodes, and a second footpath of one node, reads back field by field;
// the footpath the link joins is the land's first, named again by its number
TEST_F(FotFormat, AFileWrittenInTheTestReadsBackFieldByField)
{
	FotWriter writer;
	// the links: one, joining one footpath, made where the link first names it
	writer.Put(uint32_t {1});
	writer.NewThing(GameThingType::FootpathLinkSave);
	writer.Base(7, 2);
	writer.Put(MapCoords {.x = 0x00640000u, .z = 0x00C80000u, .altitude = 1.5f});
	writer.NewThing(GameThingType::FootpathLink);
	writer.Base(8, 3);
	writer.Put(uint32_t {1});
	writer.NewThing(GameThingType::Footpath);
	const uint32_t first = writer.Things();
	writer.Base(9, 4);
	writer.Put(uint32_t {2});
	writer.Node({.x = 0x00010000u, .z = 0x00020000u, .altitude = 0.0f}, 5);
	writer.Node({.x = 0x00030000u, .z = 0x00040000u, .altitude = 2.0f}, 6);
	writer.Put(uint32_t {1}); // active
	// the footpaths: the first again, and one more
	writer.Put(uint32_t {2});
	writer.Again(first);
	writer.NewThing(GameThingType::Footpath);
	writer.Base(11, 0);
	writer.Put(uint32_t {1});
	writer.Node({.x = 0x00050000u, .z = 0x00060000u, .altitude = 3.0f}, 7);
	writer.Put(uint32_t {0}); // not active

	const auto read = ReadFot(writer.Bytes());
	ASSERT_EQ(read.links.size(), 1u);
	const auto& link = read.links.front();
	EXPECT_EQ(link.unknown1, 7u);
	EXPECT_EQ(link.unknown2, 2u);
	EXPECT_EQ(link.coords, (MapCoords {.x = 0x00640000u, .z = 0x00C80000u, .altitude = 1.5f}));
	ASSERT_EQ(link.link.footpaths.size(), 1u);
	ASSERT_EQ(read.footpaths.size(), 2u);
	EXPECT_EQ(read.footpaths.at(0), link.link.footpaths.front());
	const auto& joined = read.footpaths.at(0);
	ASSERT_EQ(joined.nodes.size(), 2u);
	EXPECT_EQ(joined.nodes.at(0).coords, (MapCoords {.x = 0x00010000u, .z = 0x00020000u, .altitude = 0.0f}));
	EXPECT_EQ(joined.nodes.at(0).unknown, 5u);
	EXPECT_EQ(joined.nodes.at(1).coords, (MapCoords {.x = 0x00030000u, .z = 0x00040000u, .altitude = 2.0f}));
	EXPECT_EQ(joined.nodes.at(1).unknown, 6u);
	EXPECT_EQ(joined.unknown, 1u);
	const auto& other = read.footpaths.at(1);
	ASSERT_EQ(other.nodes.size(), 1u);
	EXPECT_EQ(other.nodes.front().coords, (MapCoords {.x = 0x00050000u, .z = 0x00060000u, .altitude = 3.0f}));
	EXPECT_EQ(other.unknown, 0u);
	EXPECT_TRUE(SumsMatched());
}

// Every story land's footpath file reads whole, all but Land 4's with footpaths, and every footpath a link joins is one
// of the land's own
TEST_F(FotFormat, realDataTheStoryLandsFilesReadWholeAndTheirLinksJoinTheirOwnFootpaths)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	for (const auto* name : {"land1.fot", "land2.fot", "land3.fot", "land4.fot", "land5.fot"})
	{
		std::ifstream file(std::filesystem::path(game) / "Data" / "Landscape" / name, std::ios::binary);
		ASSERT_TRUE(file.is_open()) << name;
		std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		const auto read = ReadFot(std::move(bytes));
		// Land 4's file holds links and no footpath of its own
		EXPECT_EQ(read.footpaths.empty(), std::string(name) == "land4.fot") << name;
		for (const auto& link : read.links)
		{
			for (const auto& joined : link.link.footpaths)
			{
				EXPECT_NE(std::find(read.footpaths.begin(), read.footpaths.end(), joined), read.footpaths.end()) << name;
			}
		}
	}
	EXPECT_TRUE(SumsMatched());
}
