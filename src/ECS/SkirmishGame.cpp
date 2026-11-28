/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkirmishGame.h"

#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "Locator.h"

bool openblack::ecs::IsSkirmishGame()
{
	return Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Context().skirmish;
}
