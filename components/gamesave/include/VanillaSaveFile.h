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

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The start of the world file of a saved game as the game writes it (<slot>.sav): a word that the running sum does not
// count, then two texts, each a length with its end mark and its letters. The game object's record follows.
namespace openblack::gamesave
{

/// The text that the game writes after the first word of its world file
inline constexpr std::string_view k_VanillaRevision = "$Revision: 183 $";

/// The start of the game's world file
struct VanillaFraming
{
	/// 1 while the game writes the file, 0 when the write completed. A load adds 1 and refuses a file at 2 or more.
	uint32_t attempts {0};
	std::string revision;
	/// The land's file, as the game opened it
	std::string landscape;
	/// Where the game object starts, and the running sum there
	size_t body {0};
	uint32_t checksum {0};

	bool operator==(const VanillaFraming&) const = default;
};

[[nodiscard]] std::optional<VanillaFraming> ReadVanillaFraming(std::span<const uint8_t> bytes);
/// The start of a file as the game writes it: the first word 0, then the revision and the land's file
[[nodiscard]] std::vector<uint8_t> WriteVanillaFraming(std::string_view landscape);

} // namespace openblack::gamesave
