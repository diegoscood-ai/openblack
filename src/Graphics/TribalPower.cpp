/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TribalPower.h"

#include <cstdio>
#include <cstdlib>

#include "Locator.h"

using namespace openblack;

const ecs::systems::MiracleFxSystemInterface& graphics::tribal_power::Source()
{
	if (!Locator::miracleFxSystem::has_value())
	{
		std::fputs("renderer: no miracle FX system in the locator (Locator::miracleFxSystem)\n", stderr);
		std::abort();
	}
	return Locator::miracleFxSystem::value();
}
