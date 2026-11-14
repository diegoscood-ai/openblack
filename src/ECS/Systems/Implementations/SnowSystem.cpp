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
	_changes = {};
	_marked.clear();
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
	snow_cover::AddStorm(_depths, centre, innerRadius, outerRadius, amount, &_changes);
	Changed();
}

void SnowSystem::Melt(float seconds)
{
	if (snow_cover::Melt(_depths, _melting, seconds, &_changes, &_marked))
	{
		Changed();
	}
}

void SnowSystem::Restore(std::span<const float> depths, std::span<const float> blockChanges, float meltClock, int32_t meltBand)
{
	if (depths.size() == _depths.size())
	{
		std::ranges::copy(depths, _depths.begin());
	}
	_changes = {};
	if (blockChanges.size() == _changes.change.size())
	{
		std::ranges::copy(blockChanges, _changes.change.begin());
	}
	_melting = {.clock = meltClock, .band = meltBand};
	_marked.clear();
	Changed();
}

SavedSnow SnowSystem::Saved() const
{
	return {.depths = _depths,
	        .blockChanges = {_changes.change.begin(), _changes.change.end()},
	        .meltClock = _melting.clock,
	        .meltBand = _melting.band};
}

void SnowSystem::Changed()
{
	_hasSnow = std::ranges::any_of(_depths, [](float depth) { return depth > 0.0f; });
	++_revision;
}
