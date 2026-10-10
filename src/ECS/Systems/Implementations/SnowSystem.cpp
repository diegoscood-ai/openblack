/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SnowSystem.h"

#include <algorithm>

using namespace openblack::ecs::systems;

SnowSystem::SnowSystem()
    : _depths(snow_cover::k_Cells, 0.0f)
{
}

void SnowSystem::Reset()
{
	const bool hadSnow = _hasSnow;
	std::ranges::fill(_depths, 0.0f);
	_melting = {};
	if (hadSnow)
	{
		Changed();
	}
}

void SnowSystem::AddStorm(glm::vec2 centre, float innerRadius, float outerRadius, float amount)
{
	if (amount == 0.0f)
	{
		return;
	}
	snow_cover::AddStorm(_depths, centre, innerRadius, outerRadius, amount);
	Changed();
}

void SnowSystem::Melt(float seconds)
{
	if (snow_cover::Melt(_depths, _melting, seconds))
	{
		Changed();
	}
}

void SnowSystem::Changed()
{
	_hasSnow = std::ranges::any_of(_depths, [](float depth) { return depth > 0.0f; });
	++_revision;
}
