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

#include <optional>
#include <span>
#include <vector>

/// The physique file the game writes beside a creature's mind file (named "Physique" and the mind file's name): what the
/// body looks like, for the front end's tattoo editor. It is a little-endian stream of 32-bit numbers: the species row,
/// the size the body is drawn at, its strength, fatness and alignment, then two counted lists of numbers its skin keeps.
/// The game never reads it back into a creature.
namespace openblack::creaturemind
{

/// The most numbers either list holds
constexpr uint32_t k_PhysiqueListCapacity = 1024;

struct PhysiqueFileData
{
	uint32_t speciesRow {0};
	float size {1.0f};
	float strength {0.0f};
	float fatness {0.0f};
	float alignment {0.0f};
	/// The two lists the skin keeps
	std::vector<uint32_t> listA;
	std::vector<uint32_t> listB;
};

/// The file's data, or none when the bytes end early, a list is longer than the game keeps, or bytes are left over
[[nodiscard]] std::optional<PhysiqueFileData> ReadPhysique(std::span<const uint8_t> bytes);
/// The file's bytes; a list longer than the game keeps is cut to its capacity
[[nodiscard]] std::vector<uint8_t> WritePhysique(const PhysiqueFileData& data);

} // namespace openblack::creaturemind
