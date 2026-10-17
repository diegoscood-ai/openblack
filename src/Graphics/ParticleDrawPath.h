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
#include <cstdint>

#include <array>
#include <string_view>

namespace openblack::psys
{
enum class DrawPath : uint8_t; // Particles/PSys.h
} // namespace openblack::psys

namespace openblack::particles::draw
{

/// How the game draws an effect among the other things that blend: Sorted, each of its things in its own place, the
/// farthest first; Queued, the whole effect in one place at its origin; Immediate, the miracle in the hand, all at once
/// just after the hand. The effects keep it where they are started (psys::DrawPath)
using DrawPath = psys::DrawPath;

constexpr size_t k_DrawPathCount = 3;
/// Indexed by the path's value
constexpr std::array<std::string_view, k_DrawPathCount> k_DrawPathNames {"Sorted", "Queued", "Immediate"};

} // namespace openblack::particles::draw
