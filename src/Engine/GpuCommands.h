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

/// (openblack engine) GPU work made by the game's logic at runtime (a generated mesh's buffers, a re-uploaded
/// texture...). With the logic and the drawing on two threads (docs/bw1-notes/engine-loop.md §6) no bgfx call may run
/// on the logic thread: the command is queued and the drawing thread runs it before it encodes its frame. On one
/// thread (today) it runs at once, so nothing changes.
///
/// Rules: the command owns its data (moved into the lambda) and touches only bgfx and its own resource, never the
/// registry or any game state; what the turn needs at once (a cache entry, CPU data, bounds) is made before Submit.
namespace openblack::engine::gpu
{

void Submit(std::function<void()> command);

/// The drawing thread, before it encodes a frame: runs the queued commands in their order (no-op on one thread)
void Flush();

} // namespace openblack::engine::gpu
