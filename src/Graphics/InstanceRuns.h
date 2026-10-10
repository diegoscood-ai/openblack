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

#include <span>
#include <vector>

// A range of instance rows drawn as one batch per run of rows that are drawn, so that rows left out of a frame split
// the batch instead of being drawn. Pure: values only.
namespace openblack::graphics::instance_runs
{

/// A run of consecutive drawn rows: the first row and how many
struct Run
{
	uint32_t offset;
	uint32_t count;

	[[nodiscard]] bool operator==(const Run&) const = default;
};

/// The runs of drawn rows in [offset, offset + count), in order. `drawn` holds a byte per row from row 0, 0 for a row
/// left out; a row at or past its end is drawn, so an empty mask gives the whole range as one run (none for an empty
/// range)
[[nodiscard]] std::vector<Run> Runs(std::span<const uint8_t> drawn, uint32_t offset, uint32_t count);

} // namespace openblack::graphics::instance_runs
