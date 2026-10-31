/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// The state the player's hand is drawn in, chosen again every frame from what it holds and what the interface is doing.
///
/// Pure functions, tested on their own; the hand's system gathers the facts and applies them.
namespace openblack::hand_state
{

/// The hand's states, as the hand's drawing knows them
inline constexpr int32_t k_Invisible = 0;
inline constexpr int32_t k_Normal = 1;
inline constexpr int32_t k_Tug = 3;
inline constexpr int32_t k_Holding = 4;
inline constexpr int32_t k_Totem = 5;
inline constexpr int32_t k_Creature = 7;
inline constexpr int32_t k_Grain = 8;
inline constexpr int32_t k_PlayAnim = 9;
inline constexpr int32_t k_Citadel = 0xA;

/// What the state is chosen from
struct RequiredStateFacts
{
	/// Inside the temple
	bool insideCitadel {false};
	/// The interface's hand state
	int32_t interfaceState {0};
	/// The hand holds something
	bool holding {false};
	/// The hand's locked select is on a pile
	bool lockedSelectOnPile {false};
	/// What it holds is a miracle's seed
	bool holdsSeed {false};
	/// It is pulling something out of the ground
	bool tugging {false};
	/// A set animation is playing (a tap's, or one forced on the hand)
	bool playingAnimation {false};
	/// A creature is locked for the hand
	bool creatureInHand {false};
};

/// The temple first; then, holding something, hidden while the interface is in its mode 0x19, else holding for a pile in
/// its locked select, the miracle state for a seed, tugging while it pulls, else holding; then a set animation; then by
/// the interface's state: hidden in modes 1, 2, 0xC and 0x19, the totem in 0xE, the creature in 0x13 when a creature is
/// locked for the hand, and normal otherwise
[[nodiscard]] int32_t Required(const RequiredStateFacts& facts) noexcept;

} // namespace openblack::hand_state
