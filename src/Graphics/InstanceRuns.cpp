/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstanceRuns.h"

namespace openblack::graphics::instance_runs
{

std::vector<Run> Runs(std::span<const uint8_t> drawn, uint32_t offset, uint32_t count)
{
	std::vector<Run> runs;
	const auto isDrawn = [drawn](uint64_t row) { return row >= drawn.size() || drawn[row] != 0; };
	const uint64_t end = static_cast<uint64_t>(offset) + count;
	uint64_t row = offset;
	while (row < end)
	{
		if (!isDrawn(row))
		{
			++row;
			continue;
		}
		const uint64_t first = row;
		while (row < end && isDrawn(row))
		{
			++row;
		}
		runs.push_back({.offset = static_cast<uint32_t>(first), .count = static_cast<uint32_t>(row - first)});
	}
	return runs;
}

} // namespace openblack::graphics::instance_runs
