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
/// An object made from a saved game keeps the record it was made from, whole: the values openblack does not model yet
/// are written back from it, so that a game loaded and saved again keeps them
template <typename Record>
struct LoadedRecord
{
	Record record;
};
} // namespace openblack::ecs::components
