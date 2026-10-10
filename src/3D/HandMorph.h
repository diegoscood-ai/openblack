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
#include <span>

/// The god hand shows its player's alignment. Its base mesh is pulled towards its evil mesh (alignment below 0) or its
/// good mesh (0 and above) by how far the alignment is from 0: the vertices and normals, and its skin blended a 4-bit
/// channel at a time, alpha too, in whole steps. With the game's meshes the good hand only changes colour, to gold,
/// and the evil hand turns red with ridged, spiked fingers.
///
/// The hand is not eased: each drawn frame, once the player's alignment is k_RefreshThreshold or more away from the
/// one it is drawn at, it jumps straight to it. It starts neutral. Its skin is also blended again as it goes into or
/// out of its player's influence (see Advance). The hand system (HandMorph.cpp) loads the meshes and uploads the
/// result.
namespace openblack::hand_morph
{
/// How far the alignment must move from the one the hand is drawn at before the hand is drawn anew
constexpr float k_RefreshThreshold = 0.03f;

/// The meshes the base is pulled towards
enum class Look : uint8_t
{
	Evil,
	Good,
};

/// The alignment the hand is to show, clamped to -1 (evil) .. 1 (good)
[[nodiscard]] float Target(float playerAlignment);

/// The alignment the hand is drawn at once it has caught up with target, or none while it is near enough
[[nodiscard]] std::optional<float> Refresh(float drawn, float target);

/// The hand's morph between frames
struct State
{
	/// The alignment it last took from its player, clamped to its range
	float target {0.0f};
	/// The alignment its shape and skin are drawn at
	float drawn {0.0f};
	/// Whether the interface's texture set was last the one for its player's influence. It starts as if it were.
	bool inInfluence {true};
};

/// What a frame changes: the alignment the skin is blended anew for, and whether the shape is drawn anew at drawn
struct Change
{
	std::optional<float> skin;
	bool shape {false};
};

/// A drawn frame of the hand. Going into or out of its player's influence (none when that isn't known this frame)
/// blends the skin again at the alignment it is drawn at. Then the frame's alignment is taken, and once far enough
/// from the drawn one the skin and the shape both catch up with it.
[[nodiscard]] Change Advance(State& state, float playerAlignment, std::optional<bool> inInfluence);

/// The mesh the hand drawn at an alignment is pulled towards, and how far, 0 to 1
[[nodiscard]] Look LookOf(float drawn);
[[nodiscard]] float Weight(float drawn);

/// The skin's blend weight for an alignment: |drawn| in 256 steps, truncated, at most 255
[[nodiscard]] uint32_t BlendWeight(float drawn);
/// One texel of the skin blend: each 4-bit channel ((from (255 - weight) + to weight) / 255), rounded down
[[nodiscard]] uint16_t BlendTexel(uint16_t from, uint16_t to, uint32_t weight);

/// The hand's skin drawn at an alignment: the base skin blended towards the evil or good one (look), each texel of 4
/// bits a channel
void BlendSkin(std::span<const uint16_t> base, std::span<const uint16_t> look, float drawn, std::span<uint16_t> blended);
} // namespace openblack::hand_morph
