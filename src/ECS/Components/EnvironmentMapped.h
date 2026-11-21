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

/// The entity is drawn with the first environment map in a second texture stage, each primitive taking it as its
/// material's flags say (graphics::material_stages). The game gives it to every scroll and sign a script puts in the
/// world and to the spell seeds drawn as food or as a creature spell's phial, as they are made.
struct EnvironmentMapped
{
};

} // namespace openblack::ecs::components
