/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerColours.h"

#include "ECS/Systems/MapScriptSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

int32_t player_colours::LandNumber()
{
	return Locator::mapScriptSystem::has_value() ? Locator::mapScriptSystem::value().Globals().landNumber : 0;
}

PlayerNames player_colours::OnThisLand(PlayerNames player)
{
	return Remapped(player, LandNumber());
}
