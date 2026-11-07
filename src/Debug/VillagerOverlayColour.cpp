/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerOverlayColour.h"

#include <glm/common.hpp>
#include <glm/gtx/compatibility.hpp>

#include "Gui/StatisticsBoxRules.h"

using namespace openblack;

glm::vec4 debug::gui::villager_overlay::Colour(std::optional<PlayerNames> owner, bool female, int32_t landNumber)
{
	glm::vec4 colour {1.0f};
	if (owner.has_value())
	{
		const auto player = openblack::gui::statistics_box::PlayerColour(*owner, landNumber);
		if (glm::vec3(player) != glm::vec3(0.0f))
		{
			colour = glm::vec4(glm::vec3(player), 1.0f);
		}
	}
	// a female villager's colour is lighter
	if (female)
	{
		colour += glm::vec4((glm::vec3(1.0f) - glm::vec3(colour)) * glm::vec3(144.0f / 255.0f), colour.a);
		colour = glm::saturate(colour);
	}
	return colour;
}
