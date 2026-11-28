/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "3D/Clouds.h"
#include "ECS/Systems/CloudSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class CloudSystem final: public CloudSystemInterface
{
public:
	void Reset() override;
	void Update(std::chrono::duration<float, std::milli> gameTime) override;
	[[nodiscard]] Clouds* GetLayout() noexcept override { return _clouds.get(); }

private:
	std::unique_ptr<Clouds> _clouds;
};

} // namespace openblack::ecs::systems
