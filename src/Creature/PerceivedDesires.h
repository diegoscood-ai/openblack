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
#include <functional>
#include <optional>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// What a creature thinks its player wants, from what it sees the player do: each of the player's miracles it can see
// shows the desires the miracle's table says it answers, a stroke shows compassion and a slap anger, and a villager of
// the player's seeing to a town's need shows that need. What it thinks fades slowly, and the creature's scroll and the
// temple say what it thinks its god wants most. Pure, tested on its own.

namespace openblack::creature_perceived_desires
{

/// One for each of the creature's desires, and one for each of a town's
inline constexpr size_t k_PlayerDesires = 40;
inline constexpr size_t k_TownDesires = 17;
/// Each creature turn every one is multiplied by this
inline constexpr float k_TurnFade = 0.9995f;

/// The game also keeps a ring of past values for each desire, read only by the mind's save; they come with the save
struct PerceivedDesires
{
	std::array<float, k_PlayerDesires> player {};
	std::array<float, k_TownDesires> town {};
};

/// Seen to want a desire more: the weight is added, then a sum below 0 (or not a number) becomes 0 and one above 1
/// becomes 1. A player desire of 40 or more is nothing, and so is a town desire outside 0 to 16. The game writes before
/// the player array for a negative player desire; here that is nothing too, an openblack guard.
void Increase(PerceivedDesires& desires, int32_t desire, float weight);
void IncreaseTown(PerceivedDesires& desires, int32_t desire, float weight);
/// A creature turn: every value fades a little
void Fade(PerceivedDesires& desires);
/// The desire it thinks its player wants most: the last of the player desires it feels itself (activated) that it
/// thinks the player wants at all, each of which it then forgets; none if there are none
[[nodiscard]] std::optional<size_t> TakeDominant(PerceivedDesires& desires, const std::function<bool(size_t)>& activated);

/// Whether a creature sees a point: within 0x2AA game angles (2048 to a turn) either way of where it looks, or in its
/// own map cell, however far. The difference of the two angles is taken whole, and folded once past a half turn.
inline constexpr uint32_t k_SeeHalfAngle = 0x2AA;
[[nodiscard]] bool CanSeePos(uint16_t lookAngle, uint16_t angleToPoint, bool sameCell);
/// The same from where the creature stands and the way it looks (a yaw as the body's: 0 along -z, a quarter turn along
/// +x), to a point on the map
[[nodiscard]] bool CanSeePos(float lookYaw, const map_coords::MapCoords& from, const map_coords::MapCoords& point);
/// The way a creature looks, as a yaw: towards what its head looks at, unless it looks at nothing (no point, or the
/// origin) or at the very point its body stands on, when it is the way its body faces
[[nodiscard]] float LookYaw(const std::optional<glm::vec3>& lookAt, const glm::vec3& position, float bodyYaw);

} // namespace openblack::creature_perceived_desires
