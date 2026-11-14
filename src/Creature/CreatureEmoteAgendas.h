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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The emotes of a creature and the rules they share. Here so far: following the point of focus of its player's camera,
/// which the emotes and the actions with the player that go to where the player looks share; and the game's random
/// seconds and their rounding to whole turns, which the odd actions' faint uses.
/// The emotes of a creature and the rules they share. Here so far: following the point of focus of its player's
/// camera, which the emotes and the actions with the player that go to where the player looks share; and the game's
/// random seconds and their rounding to whole turns, which the odd actions' faint uses.
namespace openblack::creature_emote
{
using creature_mind::Random;

/// Following the camera lasts thirty seconds at most. The creature arrives within twice one more than its height.
constexpr float k_FollowCameraSeconds = 30.0f;
[[nodiscard]] float FollowCameraReach(float height);
/// Walking to the point of focus, it stops this much nearer than the reach
constexpr float k_FollowCameraStopShort = 2.0f;

/// What a creature that follows the camera does on one turn
struct FollowCameraView
{
	/// The point of focus of its player's camera, on the land; none without a player
	std::optional<glm::vec3> focus;
	glm::vec2 position {0.0f};
	/// The body is busy: walking, or playing an action
	bool moving {false};
	/// While it moves: whether the point of focus is less than a sixth of a turn from the way its body faces, or in its
	/// own map cell
	bool focusAhead {false};
};
enum class FollowCameraAction : uint8_t
{
	/// The step is done
	Done,
	/// It keeps on as it goes
	Continue,
	/// It walks to the point of focus again
	MoveAgain,
	/// The point of focus is off the map: the rest of the agenda is no use
	Fail,
};
/// One turn of following the camera. With no player, or with its time up, it is done. With the point of focus off the
/// map it fails. Standing, it is done once it is within the reach, along the ground in the map's own units, or when the
/// point of focus is at the map's origin, else it walks again. Moving, it walks again when the point of focus is no
/// longer ahead of it.
[[nodiscard]] FollowCameraAction FollowCameraTurn(const FollowCameraView& view, uint32_t& turnsLeft, float reach);

/// A random share of some seconds, from 0 to the most, drawn as the game draws a float
[[nodiscard]] float FloatRandom(const Random& random, float most);

/// The seconds cut down to the whole turns in them; as they are without any turns in a second
[[nodiscard]] float WholeTurnSeconds(float seconds, float turnsPerSecond);

} // namespace openblack::creature_emote
