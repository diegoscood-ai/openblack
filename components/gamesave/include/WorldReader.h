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

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "SavedRecords.h"
#include "VanillaSaveFile.h"

// A read of the game's world file the way the game loads it: from the game object, each object's record at its first
// reference, depth first. The stream has no lengths, so the read can go only as far as the classes it knows; it stops
// at the first record of another class and says where. Each record's stored running sum is compared with the reader's
// own, which proves the layout of everything read before it.
namespace openblack::gamesave
{

/// What a read of a world file found
/// What a read says when it reads the world file to its last byte
inline constexpr std::string_view k_ReadToTheEnd = "read to the end of the file";

struct WorldRead
{
	VanillaFraming framing;
	/// The records read, by their number in the stream less one (the game object is 1). A record that the read stopped
	/// inside holds the values read before the stop.
	std::vector<std::optional<SavedRecord>> objects;
	/// Where each record starts in the file, by the same index
	std::vector<size_t> positions;
	/// The numbers of the records in the order their reads ended: a record read inside another ends before it, and
	/// the game object last. The game puts its objects' links back in this order once all are read
	std::vector<ObjectNumber> loadOrder;
	/// The stored sums read, and where the first one that differs from the reader's own sum is
	uint32_t sumsRead {0};
	std::optional<size_t> firstWrongSum;
	/// Where the read stopped, from the start of the file
	size_t stoppedAt {0};
	/// The class of the record the reader does not know, when that stopped it, and the number the object has
	std::optional<uint32_t> unreadType;
	ObjectNumber unreadNumber {0};
	/// Why the read stopped
	std::string stop;

	/// The number of records read of each class
	[[nodiscard]] std::map<uint32_t, uint32_t> Census() const;
};

/// Reads the world file as far as the classes the reader knows
[[nodiscard]] WorldRead ReadWorld(std::span<const uint8_t> bytes);

/// Writes the start of the file and the records read again, in the game's order, up to the place where the read
/// stopped. A file read by the game's layout writes back to the same bytes.
[[nodiscard]] std::vector<uint8_t> WriteWorld(const WorldRead& world);

/// A name for the class's record, for a person to read, or the class's number when the reader does not know it
[[nodiscard]] std::string TypeName(uint32_t type);

} // namespace openblack::gamesave
