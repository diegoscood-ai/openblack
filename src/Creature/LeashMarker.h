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
#include <glm/vec3.hpp>

#include "Camera/BookmarkPulse.h"

/// The marker over where the leash last sent the local player's creature: a yellow ring holding the species' footprint,
/// facing the camera, throbbing with the camera bookmarks' shared pulse. It is drawn each frame the land is drawn, while
/// the creature has a target, over the thing it was sent to (worked out again from where the thing is now) or else
/// over the point kept for the target. Only its player sees it, and it changes nothing in the game.
namespace openblack::leash_marker
{

/// The ring's picture on the sheet of markers and footprints (the bookmarks' sheet, eight pictures to a row)
constexpr uint8_t k_RingCell = 56;
/// The footprint is drawn at this share of the pulse, its width and height swapped
constexpr float k_FootprintShare = 0.7f;
/// and turned a quarter turn, so that its local x runs up the screen
constexpr float k_FootprintAngle = -1.57079637f;

/// The footprint picture of a species, by its row in the game's creature tables (creature::InfoRow): the row's print
/// on the land (creature_footprints::k_SpeciesPrints); a row past the eighteenth takes the first row's. The marker
/// never shows the first of April's smiley face.
[[nodiscard]] uint8_t FootprintCell(size_t speciesRow);

/// The ring's half width and half height: a camera bookmark marker's (bookmark_pulse::MarkerHalfSize)
[[nodiscard]] glm::vec2 RingHalfSize(const bookmark_pulse::Pulse& pulse);
/// The footprint's half width and half height before its turn: the half width is k_FootprintShare of the pulse's height
/// (at least bookmark_pulse::k_SmallestSize), and the half height that times the stretch, k_FootprintShare of the
/// pulse's width over its height, each step rounded to a float
[[nodiscard]] glm::vec2 FootprintHalfSize(const bookmark_pulse::Pulse& pulse);

/// A thing the creature was sent to: where it stands now, and its height
struct Thing
{
	glm::vec3 place;
	float height;
};
/// Where the marker stands: over the thing (2 m up from its place, then its height on top), else the point kept
[[nodiscard]] glm::vec3 Position(const std::optional<Thing>& thing, const glm::vec3& kept);

} // namespace openblack::leash_marker
