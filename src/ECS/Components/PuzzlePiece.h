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

/// An animal a puzzle game moves as a piece (the lion maze's wolf and sheep): it decides nothing for itself, has no
/// needs and never becomes a physics object, so it stands where the puzzle puts it and goes only where the puzzle sends
/// it. Wiki: docs/wiki/world/lion-maze.md
struct PuzzlePiece
{
};

} // namespace openblack::ecs::components
