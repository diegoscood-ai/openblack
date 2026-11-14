/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

/// The sources of a creature's desires that follow what it is and what happens to it, rather than fading after events
/// push them. Here so far: how interesting the creature finds its player, from the hand's movement and what it holds.
/// Everything here is pure.
namespace openblack::creature_desire_sources
{

/// How interesting the creature finds its player: the hand's recent movement and what it holds
struct PlayerInterest
{
	/// It finds the player most interesting at first
	float value {1.0f};
	/// The distance the hand has moved lately, fading
	float handMovement {0.0f};
	/// Where the hand was at the last turn, none before the first
	std::optional<glm::vec3> lastHand;
};
/// The hand's movement counts up to this, and fades by this each turn
constexpr float k_MostHandMovement = 60.0f;
constexpr float k_HandMovementFade = 0.98f;
/// A held thing's radius across the land counts up to this
constexpr float k_MostHeldRadius = 30.0f;

/// How interesting the player is this turn, from where the hand is and the radius of what it holds (none when it holds
/// nothing); 0 for a creature without a player, whose hand is not passed
[[nodiscard]] float InterestNow(PlayerInterest& interest, const std::optional<glm::vec3>& hand,
                                std::optional<float> heldRadius);
/// A player more interesting than before is so at once; less interesting, the creature loses half its interest a turn
void UpdateInterest(PlayerInterest& interest, float now);

} // namespace openblack::creature_desire_sources
