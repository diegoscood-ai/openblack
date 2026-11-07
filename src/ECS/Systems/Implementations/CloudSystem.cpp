/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CloudSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;

void CloudSystem::Reset()
{
	// The new layout draws its numbers from the C runtime's stream, before the last land's clouds go
	_clouds = std::make_unique<Clouds>();
}

void CloudSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	if (_clouds)
	{
		_clouds->Update(gameTime.count());
	}
}
