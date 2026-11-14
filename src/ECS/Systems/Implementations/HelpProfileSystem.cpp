/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HelpProfileSystem.h"

#include "Help/HelpProfile.h"

using namespace openblack::ecs::systems;

void HelpProfileSystem::Trigger(uint32_t event)
{
	openblack::help_profile::Trigger(static_cast<openblack::help_profile::Event>(event));
}

void HelpProfileSystem::ProcessTurn()
{
	openblack::help_profile::Process();
}

const openblack::help::profile::HelpProfile& HelpProfileSystem::Get() const
{
	return openblack::help_profile::Profile();
}

openblack::help::profile::HelpProfile& HelpProfileSystem::Get()
{
	return openblack::help_profile::Profile();
}
