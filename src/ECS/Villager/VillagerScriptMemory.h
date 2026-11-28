/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string_view>

#include <entt/entity/fwd.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ScriptMemory.h"
#include "Enums.h"

// A villager's script memory (components::ScriptMemory): what a villager taken out of a script state keeps of it, and
// gives back when it comes back to that state (docs/wiki/scripts/script-commands.md#the-script-memory)

namespace openblack::ecs::script_memory
{
/// Its memory, made when it has none: each turn in IN_SCRIPT, the wander round a place and a script's dance, each play
/// of a script's clip, and when it is taken out of one of them
components::ScriptMemory& Make(entt::entity villager);
/// Its memory, none when it has never been in a script state
[[nodiscard]] components::ScriptMemory* Of(entt::entity villager);
[[nodiscard]] const components::ScriptMemory* Find(entt::entity villager);

/// What it keeps, taken out of a script state: the thing its walk followed, where its walk was going, the script's
/// place and number and its pair of whole numbers. A walk that never had a destination (all of it 0) stands for one
/// to where the villager is, unless it was kept from the playing state
void KeepInMind(entt::entity villager);
/// What it gets back, coming back to the state it was taken out of: the thing, the destination with its height, the
/// script's place and number and its pair of whole numbers
void GiveBack(entt::entity villager);
/// The clip a state has just put on it, kept when it has a memory
void RecordStateClip(entt::entity villager, int32_t clip);
/// A script forcing a clip on it: its memory, made when it has none, keeps what it has, then no state to take up and
/// the clip as the script gave it, out of the list or not (the clip it puts on again is chosen when it is replayed)
void KeepForcedClip(entt::entity villager, int32_t clip);
/// Whether the debug lines of the script memory are written: the game log at debug
[[nodiscard]] bool DebugOn();
/// One debug line of the script memory, "Villager E: <what>", when DebugOn
void Debug(entt::entity villager, std::string_view what);

/// Whether a villager entering `next` from a state with another entry takes up what its memory kept: only when the
/// memory was kept from that same state (none kept: InvalidState)
[[nodiscard]] constexpr bool Resumes(bool sameEntry, VillagerStates kept, VillagerStates next)
{
	return !sameEntry && kept != VillagerStates::InvalidState && kept == next;
}

/// The clip a memory's clip stands for: itself when it is one of `count` clips, else the first
[[nodiscard]] constexpr int32_t ReplayedClip(int32_t kept, int32_t count)
{
	return kept >= 0 && kept < count ? kept : 0;
}

/// Whether a kept destination is one to walk back to: not all of it 0
[[nodiscard]] constexpr bool HasDestination(const map_coords::MapCoords& destination)
{
	return destination != map_coords::MapCoords {};
}
} // namespace openblack::ecs::script_memory
