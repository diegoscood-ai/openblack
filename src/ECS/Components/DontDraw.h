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

/// The object's own draw is turned off, as SET_HIGH_GRAPHICS_DETAIL does while a SuperVillager draws the thing in
/// its place. The landscape's object draw list leaves such an object out. It is not NotDrawn (a building at 0 % or
/// the invisible hand): the renderer does not read this tag. docs/bw1-notes/original-frame.md §6
struct DontDraw
{
};

} // namespace openblack::ecs::components
