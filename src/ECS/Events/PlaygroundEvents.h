/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::events
{
/// One of the Gods' Playground's lessons has started (its developer function, or the trip to the island): from now on
/// Escape on the island stops it, until the island's scripts restart (input/PlaygroundEscape.h).
/// See docs/wiki/project/progress/story/tutorial_island.md
struct PlaygroundLessonStarted
{
};
} // namespace openblack::ecs::events
