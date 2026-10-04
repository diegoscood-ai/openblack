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

/// GameThing flags +0xA bit 0 (GAME_THING_FLAG_UNAVAILABLE): set by GameThing::ToBeDeleted 0x56FB70; the thing waits
/// on the dead list (g_game +0x205D1C) and is freed by GameThing::ProcessDeadList 0x56FB10. Readers ask
/// GameThing::IsAvailable (GameThing.h:361): ecs::IsAvailable (ECS/ToBeDeleted.h)
struct Unavailable
{
};

} // namespace openblack::ecs::components
