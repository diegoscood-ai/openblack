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

#include <bit>
#include <optional>

/// How the see-through ghost of a held scaffold's building is drawn in a frame: its size over the cross-fade, its turn
/// between two game turns and its alpha. Only the frames read these rules; the game turns never do. Each step is the
/// original's single precision arithmetic, one rounding a step. Wiki:
/// docs/wiki/world/building-sites.md#the-phantom-in-the-hand
namespace openblack::ecs::scaffold_ghost
{

/// The milliseconds of one game turn the drawing counts into the turn with
inline constexpr uint32_t k_TurnMs = 100;
/// The share of its full size the ghost changes by in each millisecond of the cross-fade (the 400 ms fade timer's
/// inverse, the float 0x3B23D70A)
inline constexpr float k_SizePerMs = std::bit_cast<float>(0x3B23D70AU); // 0.0025
/// The share of a game turn's spin each millisecond into the turn is (the float 0x3C23D70A)
inline constexpr float k_TurnPerMs = std::bit_cast<float>(0x3C23D70AU); // 0.01
/// The ghost's alpha is the land light's alpha times two thirds (the float 0x3F2AAAAB)
inline constexpr float k_HeldAlphaPart = std::bit_cast<float>(0x3F2AAAABU); // 0.666666687

/// The ghost's size and its Y angle in a frame
struct HeldPose
{
	float scale {1.0f};
	float yAngle {0.0f};
};

/// What the drawing reads of a held scaffold's phantom
struct HeldPhantom
{
	/// The cross-fade timer, as the hand's turn left it (400 down to 0 in steps of 100)
	int32_t fade {0};
	/// The new ghost is growing in (the fade phase); otherwise the old one is shrinking away
	bool appearing {false};
	/// The phantom's full size and its Y angle, as the hand's turn left them
	float scale {1.0f};
	float yAngle {0.0f};
};

/// The ghost of a held scaffold `visualMs` milliseconds into the game's drawn time, turning by `spinPerTurn` each game
/// turn. The timer less the part of the turn drawn so far (both unsigned, the smaller taken) is what is left of the
/// fade: while some is left, a growing ghost has 1 - left / 400 of its size and a shrinking one left / 400; with none
/// left only a grown ghost is drawn, at its full size, and a shrunk one is not drawn at all. Between two turns it turns
/// on by the share of the turn drawn so far times a turn's spin, smoothly, as the original draws it
[[nodiscard]] std::optional<HeldPose> HeldPoseOf(const HeldPhantom& phantom, uint32_t visualMs, float spinPerTurn) noexcept;

/// The ghost's alpha from the land light at the hand (an ARGB colour): its alpha byte times two thirds, truncated
[[nodiscard]] uint8_t HeldAlpha(uint32_t landLight) noexcept;

} // namespace openblack::ecs::scaffold_ghost
