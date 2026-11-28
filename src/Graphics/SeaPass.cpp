/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SeaPass.h"

#include "Common/TruncateToInt.h"

namespace openblack::graphics
{

bool sea_pass::ReflectsCreature(glm::vec3 position, float fizz, const shadow_math::BlockAt& blocks)
{
	const int cellX = TruncateToInt(position.x * shadow_math::k_CellScale);
	const int cellZ = TruncateToInt(position.z * shadow_math::k_CellScale);
	if (cellX < 0 || cellX > k_CreatureCellLimit || cellZ < 0 || cellZ > k_CreatureCellLimit)
	{
		return false;
	}
	const auto block = blocks(cellX >> 4, cellZ >> 4); // 16 cells per block side
	if (!block.exists || !block.visible || block.distance > k_CreatureMaxBlockDistance)
	{
		return false;
	}
	return !(position.y >= k_CreatureMaxY) && !(fizz >= k_CreatureMaxFizz);
}

} // namespace openblack::graphics
