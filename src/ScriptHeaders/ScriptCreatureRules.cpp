/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptCreatureRules.h"

#include "3D/MapCoords.h"

namespace openblack::script::creature_rules
{

Confinement ConfinementFor(const glm::vec3& point, float radius)
{
	return {
	    .centre = {map_coords::Quantise(point.x), 0.0f, map_coords::Quantise(point.z)},
	    .radius = radius,
	};
}

} // namespace openblack::script::creature_rules
