/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

// The scripts' switch that hides the sun (SET_SUN_DRAW), kept on the sun's entity (components::Sun)
// (docs/wiki/world/sky.md#the-sun-and-its-glare).

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::sun_draw
{
/// SET_SUN_DRAW's value: any value but nought hides the sun and its glare, nought shows them. Nothing without a sun
void SetFromScript(Registry& registry, entt::entity sun, uint32_t value);

/// The sun shows again: the scripts restart, or a frame is drawn inside the temple. Nothing without a sun
void ShowAgain(Registry& registry, entt::entity sun);

/// Whether a script has hidden the sun; false without a sun
[[nodiscard]] bool IsHidden(const Registry& registry, entt::entity sun);
} // namespace openblack::ecs::sun_draw
