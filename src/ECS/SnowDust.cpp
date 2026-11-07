/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SnowDust.h"

#include <algorithm>

#include <glm/vec2.hpp>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "Locator.h"
#include "Physics/TurnRules.h"

using namespace openblack;
using namespace openblack::ecs;

int32_t snow_dust::SnowAt(glm::vec3 point)
{
	if (!Locator::snowSystem::has_value())
	{
		return 0;
	}
	// The depth is cut to a whole number before it is kept within a byte
	const auto depth = static_cast<int32_t>(Locator::snowSystem::value().GetDepth(glm::vec2(point.x, point.z)));
	return std::clamp(depth, 0, 255);
}

uint32_t snow_dust::Tint(uint32_t argb, int32_t snow)
{
	if (snow == 0)
	{
		return argb;
	}
	return physics::turn::BlendColour(argb, land_light::CurrentTable().GetLandColour(), snow);
}
