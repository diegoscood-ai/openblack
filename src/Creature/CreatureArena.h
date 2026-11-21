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

#include <functional>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

/// Where creatures fight: an arena, a circle on the land.
///
/// Two creatures about to fight look for an arena from the middle between them. The nearest arena closer than a
/// distance that grows with the starting creature's size and running speed is taken, shrunk to the size the fight wants
/// if it is bigger. Without one, a clear place is searched for nearby, for a circle of the size wanted, then of nine,
/// eight, seven and six tenths of it; the new arena stands there for that fight only.
namespace openblack::creature_arena
{
/// The distance within which an arena is taken is this many times the creature's running pace
constexpr float k_ReusePerPace = 10.0f;

/// The distance within which an arena is taken: ten times the pace the starting creature runs at, from its species'
/// running share and its size
[[nodiscard]] float ReuseDistance(float runShare, float size);

/// The nearest of the arenas there are (by their middles in 16.16 map units, in the order they are looked through) to a
/// point closer than a distance in metres, the first of those equally near: its place in the list
[[nodiscard]] std::optional<size_t> Nearest(std::span<const glm::ivec2> arenas, glm::ivec2 point, float within);

/// An arena is looked for this far round the middle between the fighters
constexpr float k_SearchRange = 500.0f;
/// Without room for the arena wanted, a smaller one is tried, a tenth smaller each time, while it is more than this share
constexpr float k_ScaleStep = 0.1f;
constexpr float k_SmallestScale = 0.59f;

/// The middle of the nearest clear area so wide round the start, in metres, if any
using FindClearArea = std::function<std::optional<glm::vec2>(float width)>;
/// Where a new arena goes and its radius: found clear for one of the sizes tried, from the size wanted
struct Placed
{
	glm::vec2 centre;
	float radius;
	float scale;
};
[[nodiscard]] std::optional<Placed> Place(float wantedRadius, const FindClearArea& find);
} // namespace openblack::creature_arena
