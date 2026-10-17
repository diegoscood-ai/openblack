/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/AdvisorSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our advisor spirits, which the script state holds; it keeps nothing of its own and forwards to them
class AdvisorSystem final: public AdvisorSystemInterface
{
public:
	void Load() override;
	[[nodiscard]] bool IsLoaded() const override;

	void Update(const Frame& frame) override;
	void ProcessTurn() override;

	[[nodiscard]] help::spirits::AdvisorSpiritController& GetController() override;
	[[nodiscard]] const help::spirits::AdvisorSpiritController& GetController() const override;

	void FillOverlay(graphics::OverlayFrame& frame) override;
	[[nodiscard]] const help::HelpDudeFile* GetFile(int advisor) const override;
};

} // namespace openblack::ecs::systems
