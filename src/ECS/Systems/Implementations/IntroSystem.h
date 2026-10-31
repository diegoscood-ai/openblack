/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/IntroSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

/// Our opening specials, which keep their state with the world effects; it keeps nothing of its own
class IntroSystem final: public IntroSystemInterface
{
public:
	void Play(int32_t special) override;
	void ReleaseAll() override;
	[[nodiscard]] bool HasLiftFinished() const override;
	void Update(uint32_t milliseconds) override;
	void FillFrame(graphics::IntroLightOverlay& out) const override;
};

} // namespace openblack::ecs::systems
