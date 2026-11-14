/******************************************************************************
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

/// The entity's mesh isn't shaded by the light every model is lit with (the sun by day, the hand's light in full
/// night): it is drawn in the colour of the land's light where it stands, hazed with distance. Black & White draws its
/// trees, dead trees, big forests and flowers this way. Its draw still takes its own tint and specular
/// (argb_colour::PackInstanceUnlit)
struct Unlit
{
};

} // namespace openblack::ecs::components
