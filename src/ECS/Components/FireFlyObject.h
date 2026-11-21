/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// A firefly's object in the map: the fireflies' list keeps what the firefly does, and this entity stands for it in its
/// cell and in the object draw list, at the firefly's own map coordinates (its Transform). Its sprite is another entity,
/// drawn where the firefly's drift takes it. docs/wiki/world/day-and-night.md#night-lights
struct FireFlyObject
{
};

} // namespace openblack::ecs::components
