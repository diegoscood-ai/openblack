/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureSize.h"

namespace openblack
{

float creature_size::AutoscaleStep(float own, float localBase, float factor)
{
	// in float, step by step in this order; only a size at or above the largest is capped, so a size that is not a
	// number stays as it is
	const float size = (localBase * factor - own) * 0.5f + own;
	return size >= k_AutoscaleLargest ? k_AutoscaleLargest : size;
}

} // namespace openblack
