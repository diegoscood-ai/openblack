/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/HelpProfileSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our help profile, which keeps its counters in the script state; it keeps nothing of its own, and Get is the
/// script state's profile
class HelpProfileSystem final: public HelpProfileSystemInterface
{
public:
	void Trigger(uint32_t event) override;
	void ProcessTurn() override;

	[[nodiscard]] const help::profile::HelpProfile& Get() const override;
	[[nodiscard]] help::profile::HelpProfile& Get() override;
};

} // namespace openblack::ecs::systems
