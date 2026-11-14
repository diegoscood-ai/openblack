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

std::vector<Part> Split(Run run, const std::unordered_map<uint32_t, uint8_t>& forcedAlphaRefs)
{
	std::vector<Part> parts;
	const uint64_t end = static_cast<uint64_t>(run.offset) + run.count;
	uint64_t first = run.offset;
	const auto flush = [&parts, &first](uint64_t row) {
		if (row > first)
		{
			parts.push_back({.offset = static_cast<uint32_t>(first), .count = static_cast<uint32_t>(row - first)});
		}
	};
	if (!forcedAlphaRefs.empty())
	{
		for (uint64_t row = run.offset; row < end; ++row)
		{
			const auto forced = forcedAlphaRefs.find(static_cast<uint32_t>(row));
			if (forced == forcedAlphaRefs.end())
			{
				continue;
			}
			flush(row);
			parts.push_back({.offset = static_cast<uint32_t>(row), .count = 1, .forcedAlphaRef = forced->second});
			first = row + 1;
		}
	}
	flush(end);
	return parts;
}

} // namespace openblack::graphics::instance_runs
