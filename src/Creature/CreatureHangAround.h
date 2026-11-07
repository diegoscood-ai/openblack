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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

// A creature hanging around at home. It walks home and waits there a while, looking about. With no temple of its
// player's it then yawns. With one, it does one of ten lots: the first, a yawn; the next seven, it walks out from home
// away from the temple, faces further out and sits or points there, then turns to the temple and sits; the last two,
// it turns to the temple and sits. A pure agenda, tested on its own.

namespace openblack::creature_hang_around
{

/// It walks home to within its height, or this far when it is taller
constexpr float k_HomeReach = 5.0f;
/// At home it waits two seconds and up to three more, in whole turns
constexpr float k_WaitSeconds = 2.0f;
constexpr float k_WaitExtraSeconds = 3.0f;
/// With a temple it draws one of these lots: the first a yawn, then the lots that send it out from home, and the rest
/// that turn it to the temple
constexpr uint32_t k_Lots = 10;
constexpr uint32_t k_LastOutLot = 7;
/// Out from home it goes this share of the radius of the area that holds it, to within this distance of that point,
/// and faces a point this far out from home
constexpr float k_OutShare = 0.8f;
constexpr float k_OutReach = 2.0f;
constexpr float k_ViewDistance = 50.0f;
/// A sit lasts ten seconds and up to five more
constexpr float k_SitSeconds = 10.0f;
constexpr float k_SitExtraSeconds = 5.0f;
/// Home and the temple this near along both ground axes, it goes out along the x axis
constexpr float k_SameSpot = 0.001f;

/// Where the creature hangs around, and how it measures it
struct Places
{
	/// Its home on the ground, where it walks to
	glm::vec2 home {0.0f};
	/// The heart of its player's temple, on the ground; none when its player has no temple
	std::optional<glm::vec3> templeHeart;
	/// The radius of the area that holds it, 0 when none holds it
	float confinementRadius {0.0f};
	float height {0.0f};
	/// The game's turns in a second, which the wait counts in
	float turnsPerSecond {0.0f};
};

[[nodiscard]] std::vector<creature_mind::Step> Agenda(const Places& places, const creature_mind::Random& random);

/// The way out from home, away from the temple: from the heart to home kept at the height of the sea, as long as one
/// across, along the x axis when the two are at the same spot on the ground
[[nodiscard]] glm::vec3 WayOut(glm::vec2 home, glm::vec3 templeHeart);

} // namespace openblack::creature_hang_around
