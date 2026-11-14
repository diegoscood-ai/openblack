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
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerMind.h"

// What a computer player's mind sees round its hand (ComputerMind::seen): each turn of the mind it looks over the map
// cells in a spiral from the hand, as far as a reach, and remembers every forest, tree and miracle's seed it has not
// seen yet, the newest first. Every turn, even while the mind is off, it forgets what is gone; now and then it forgets
// what it saw long ago. Pure rules over what they read of the land; the game's side is the computer player system's.

namespace openblack::computer_player
{

/// What the mind's look round its hand reads of the land
class SeenWorld
{
public:
	virtual ~SeenWorld() = default;

	/// The things in a map cell, in the order the game walks them; none when the cell is off the land
	[[nodiscard]] virtual std::optional<std::vector<entt::entity>> CellObjects(uint16_t x, uint16_t z) const = 0;
	/// What the mind remembers a thing as, when it is still there: a tree as its forest when it is in one, else as
	/// itself, a miracle's seed as itself; nothing for anything else
	[[nodiscard]] virtual std::optional<ecs::components::ComputerMindSeen> SeenAs(entt::entity thing) const = 0;
	/// The thing seen, or its forest, is still there
	[[nodiscard]] virtual bool StillThere(const ecs::components::ComputerMindSeen& seen) const = 0;
};

/// How far the mind looks round its hand, in metres, beyond the 10 metres of a cell
inline constexpr float k_SeenReach = 30.0f;
/// The mind forgets what it saw over 8000 turns ago, at every 500th turn while it remembers more than 50 things
inline constexpr uint32_t k_SeenOld = 8000;
inline constexpr uint32_t k_SeenForgetEvery = 500;
inline constexpr uint32_t k_SeenMost = 50;

/// The two are the same thing seen: the same forest, or the same tree or seed
[[nodiscard]] bool SameSeen(const ecs::components::ComputerMindSeen& a, const ecs::components::ComputerMindSeen& b);

/// The mind's look round its hand, one turn of it: first, at every 500th turn while it remembers more than 50 things, it
/// forgets what it saw over 8000 turns ago; then over the cells in a spiral from the hand's, while each cell on the land
/// is nearer the hand than 10 metres more than the reach, every thing of the cell it has not seen yet goes first in what
/// it has seen, with the turn. A cell off the land is passed over
void ScanAroundHand(ecs::components::ComputerMind& mind, const SeenWorld& world, const map_coords::MapCoords& hand,
                    uint32_t turn);

/// The mind forgets what it saw more than `old` turns before the turn (a turn after it counts as long ago)
void ForgetSeenBefore(std::vector<ecs::components::ComputerMindSeen>& seen, uint32_t turn, uint32_t old);

/// The mind forgets what is no longer there
void ForgetSeenGone(ecs::components::ComputerMind& mind, const SeenWorld& world);

} // namespace openblack::computer_player
