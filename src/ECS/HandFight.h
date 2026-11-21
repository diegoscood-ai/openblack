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

#include <optional>

#include <glm/vec2.hpp>

#include "Input/GamePackets.h"

/// The hand while the player's creature fights and the player's camera watches the fight, with nothing in the hand: the
/// interface's fight state. Each button's press goes to the fight and to nothing else. On the player's creature it
/// blocks, on its opponent it strikes high, in the middle or low, and on the arena's ground it steps; the grab button's
/// move goes in place of the moves queued, the action button's after them. A press on a body charges its blow until its
/// button is let go. Wiki: docs/wiki/creature/fights.md#the-players-presses.
namespace openblack::ecs::hand_fight
{

/// The interface's state number while it directs the fight, and while the press of each button on a body is held
inline constexpr int32_t k_FightState = 0x10;
inline constexpr int32_t k_ActionHeldState = 0x16;
inline constexpr int32_t k_GrabHeldState = 0x17;

enum class Button : uint8_t
{
	/// The button that grips the land elsewhere
	Grab,
	/// The button that picks things up elsewhere
	Action,
};

[[nodiscard]] constexpr int32_t HeldState(Button button)
{
	return button == Button::Grab ? k_GrabHeldState : k_ActionHeldState;
}
/// The grab button's move goes in place of the moves queued, the action button's after them
[[nodiscard]] constexpr bool ReplacesQueue(Button button)
{
	return button == Button::Grab;
}
/// The packet of a press on a fighter's body
[[nodiscard]] constexpr game_packets::Type BodyPacket(Button button)
{
	return button == Button::Grab ? game_packets::Type::FightMoveReplace : game_packets::Type::FightMoveAdd;
}
/// The packet of a press on the arena's ground
[[nodiscard]] constexpr game_packets::Type GroundPacket(Button button)
{
	return button == Button::Grab ? game_packets::Type::FightSelectPoint : game_packets::Type::FightApplyPoint;
}

/// A press is taken only while fewer of the player's creature's presses than this show their marks
inline constexpr size_t k_MostWaiting = 12;
/// The special move, as the scripts number the moves, which the fight's special-move gesture sends as the action
/// button's press on a body does
inline constexpr int32_t k_SpecialMove = 4;

/// Whose body a press lands on
enum class Body : uint8_t
{
	Own,
	Opponent,
	/// Any other creature: the press does nothing
	Other,
};
/// The move a press on a body makes, numbered as the scripts number them: on the player's own creature a block, on its
/// opponent a blow high, in the middle or low by how far up its body the press lands; none on any other creature
[[nodiscard]] std::optional<uint32_t> BodyMove(Body body, float heightShare);

/// Whether a point on the ground lies inside the arena, strictly within its radius of its middle, worked as the game's
/// floating point does
[[nodiscard]] bool InsideArena(glm::vec2 centre, float radius, glm::vec2 point);

/// How long a press was held, in milliseconds: the visual clock's milliseconds since it, at most a hundred for each turn
/// since it and one more
[[nodiscard]] constexpr int32_t HeldMs(int32_t visualMs, uint32_t turns)
{
	const auto most = (static_cast<int32_t>(turns) + 1) * 100;
	return most <= visualMs ? most : visualMs;
}

} // namespace openblack::ecs::hand_fight
