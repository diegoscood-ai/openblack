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
#include <vector>

#include <glm/vec2.hpp>

#include "CreatureIdleMind.h"

// What a creature does about a teleport stone it walks past. When a jump from the stone would save it enough of its
// walk, it turns aside, curious, with a plan it must carry out: it walks to within reach of the stone with a puzzled face,
// looking at the stone, and asks the stone for the jump. A refused jump ends the plan as a failure. A jump gives it a
// second plan, of obeying its player: the stone carries it to the far stone (it fades out of sight, is put there with the
// stone's arrive sound and fades back in), it walks on to where it was going with a smile, and the stone lets go of it.
// Pure agendas and the carry's turns, tested on their own.

namespace openblack::creature_mind
{

/// The action of the game's table that takes the creature to the stone, and the one it carries out after the jump
inline constexpr std::string_view k_GoToTeleportAction = "GoToTeleport";
inline constexpr std::string_view k_AfterTeleportAction = "MoveToPos";
/// The creature walks to within this of the stone, and after the jump to within this of where it was going
inline constexpr float k_TeleportWalkWithin = 5.0f;
/// A fade out of sight, and a fade back in, last this long, counted in whole turns
inline constexpr float k_CarrySeconds = 2.0f;

/// The walk to the stone and the jump from it, the jump the main step. The stone's point is its map position in metres.
/// The game's agenda ends with a short puzzled wait after the jump, which no turn ever reaches: the jump either ends the
/// plan or gives the creature a new one, so it is left out.
[[nodiscard]] std::vector<Step> GoToTeleport(uint32_t stone, glm::vec2 stonePoint);

/// The steps after the jump: carried to the far stone (its map position in metres), the walk on to where it was going
/// (none when the area that keeps it refuses the walk), and the stone letting go of it
[[nodiscard]] std::vector<Step> AfterTeleport(glm::vec2 arrival, std::optional<glm::vec2> destination);

/// The turns a fade out, or a fade back in, lasts: the whole turns in a second times two, the fraction dropped
[[nodiscard]] uint16_t CarryTurns(uint32_t turnsPerSecond);

/// One turn of a carry after the turn it started on. Fading out, its count goes down and, while turns are left, it is
/// out of sight by one less the count's share of the fade; once none are left it arrives at the far stone and the count
/// starts again for the fade back in, in which it is out of sight by the count's share, in full sight once none are left
struct CarryTurn
{
	std::optional<float> fizz;
	bool arrive {false};
	bool done {false};
};
[[nodiscard]] CarryTurn StepCarry(uint16_t& turns, bool& fadingIn, uint32_t turnsPerSecond);

} // namespace openblack::creature_mind
