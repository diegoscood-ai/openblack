/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedDispenserGrid.h"

#include <glm/common.hpp>

#include "Magic/DispenserRules.h"

using namespace openblack;

std::span<const MagicType> testbed_dispensers::GridMagicTypes()
{
	return magic::DispensableMiracles();
}

std::vector<glm::vec2> testbed_dispensers::GridOffsets(size_t count)
{
	std::vector<glm::vec2> offsets;
	offsets.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const auto column = static_cast<float>(i % k_GridColumns);
		const auto row = static_cast<float>(i / k_GridColumns);
		offsets.emplace_back(k_GridOrigin + glm::vec2(column, row) * k_GridSpacing);
	}
	return offsets;
}

bool testbed_dispensers::InGridArea(glm::vec2 offset)
{
	const auto offsets = GridOffsets(GridMagicTypes().size());
	glm::vec2 low = offsets.front();
	glm::vec2 high = offsets.front();
	for (const auto& point : offsets)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	low -= glm::vec2(k_GridClearance);
	high += glm::vec2(k_GridClearance);
	return offset.x >= low.x && offset.x <= high.x && offset.y >= low.y && offset.y <= high.y;
}
