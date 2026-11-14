/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include <glm/vec3.hpp>

#include "ECS/Systems/MistSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class MistSystem final: public MistSystemInterface
{
public:
	/// Whether a mist of this size at this position is on screen
	using InViewTest = std::function<bool(const glm::vec3& position, float size)>;

	/// With the game camera's test (mists::InView)
	MistSystem();
	/// With another test (tests)
	explicit MistSystem(InViewTest inView);

	void Update(std::chrono::duration<float, std::milli> gameTime) override;

private:
	InViewTest _inView;
};

} // namespace openblack::ecs::systems
