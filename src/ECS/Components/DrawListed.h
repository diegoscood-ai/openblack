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

/// The object's listed mark in the landscape's object draw list: a rebuild that meets the object again skips it. The
/// mark stays with the object, not with the list: emptying the list when the map is cleared leaves it, and a pass that
/// nulls the object's entry leaves it too. It goes with the object, so an entity made later with the same index is not
/// listed. Only the draw list reads it, not the renderer. docs/bw1-notes/original-frame.md §6, "Stage B"
struct DrawListed
{
};

} // namespace openblack::ecs::components
