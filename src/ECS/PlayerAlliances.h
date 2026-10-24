/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>

#include "ECS/Components/PlayerAlliances.h"
#include "Enums.h"

// The alliances between the players (components::PlayerAlliances): how strongly each player is allied to each other.
// The rules over one player's alliances are pure; the rest reads and writes them where the game keeps them.

namespace openblack::ecs::alliances
{
/// The player's strength towards the other; 0 for a slot out of range
[[nodiscard]] constexpr float Strength(const components::PlayerAlliances& of, PlayerNames other)
{
	const auto index = static_cast<size_t>(other);
	return index < of.strength.size() ? of.strength.at(index) : 0.0f;
}

/// Whether the player is allied to the other: their strength is above 0. A player is not their own ally unless a
/// script makes them so
[[nodiscard]] constexpr bool IsAllied(const components::PlayerAlliances& of, PlayerNames other)
{
	return Strength(of, other) > 0.0f;
}

/// Whether the player may act within the other's influence as within their own: the strength is above 0.1, as a
/// double (so a strength of exactly 0.1f, a little above it, is enough)
[[nodiscard]] constexpr bool MayUseInfluenceOf(const components::PlayerAlliances& of, PlayerNames other)
{
	return static_cast<double>(Strength(of, other)) > 0.1;
}

/// The influence a player gets from their allies where they have none of their own: over the player slots in order
/// (the neutral one last), each ally whose influence the player may use (MayUseInfluenceOf) gives its own raw influence
/// (`rawOf`); the first above 0 is taken, else 0
[[nodiscard]] float AlliesInfluence(const components::PlayerAlliances& of, const std::function<float(PlayerNames)>& rawOf);

/// The player's alliances where the game keeps them: on the player's entity, else what the player system keeps for
/// them, else none (every strength 0, as without a player system)
[[nodiscard]] components::PlayerAlliances AlliancesOf(PlayerNames player);
/// The player's strength towards the other, and whether they are allied, as kept
[[nodiscard]] float Strength(PlayerNames player, PlayerNames other);
[[nodiscard]] bool IsAllied(PlayerNames player, PlayerNames other);

/// Sets two players' alliance, both ways: each one's strength towards the other becomes `strength`, as it is (a
/// script's percentage is not scaled). The same player twice sets their strength towards themselves
void SetAlliance(PlayerNames player, PlayerNames other, float strength);
} // namespace openblack::ecs::alliances
