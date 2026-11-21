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

#include <vector>

#include <glm/vec3.hpp>

#include "Enums.h"

/// The rules of the hand knocking on a town's building: which buildings take a knock, which of them knock back with a
/// sound and the hand's tap, the knocking sounds in turn, and the read-out of how many people live in each of the
/// town's buildings that a knock brings up for a few seconds. Pure, for the abode knock system and its tests. The
/// game's FPU works at float precision, so every product here is a float product.
/// Wiki: docs/wiki/hand/grab-hold-and-throw.md#tapping-objects.
namespace openblack::ecs::abode_knock
{

/// Every building of a town can be tapped but the village centre, which keeps its own answer (no). Whether it is begun
/// enough to take the tap is the hand's own test (an abode with nothing built is not interactable).
[[nodiscard]] constexpr bool IsTappable(AbodeType type)
{
	return type != AbodeType::TownCentre;
}

/// Only houses (and the windmill, which counts as one) answer a knock with the knocking sound and the hand's tap
[[nodiscard]] constexpr bool KnocksBack(AbodeType type)
{
	return (static_cast<uint32_t>(type) & static_cast<uint32_t>(AbodeType::LivingQuarters)) != 0;
}

/// The knocking sounds, played one after another in this order by every knock in the game. The audio's knock
/// counter (audio::Counter::KnockRoof) keeps the turn.
constexpr uint32_t k_KnockSounds = 9;
/// The sound after the one just played
[[nodiscard]] constexpr uint32_t NextKnockSound(uint32_t sound)
{
	return (sound + 1) % k_KnockSounds;
}

/// The read-out shows for this long, in milliseconds: a second growing in, six held, a second shrinking away
constexpr int32_t k_ReadoutMs = 8000;
constexpr int32_t k_ReadoutGrowMs = 1000;
constexpr int32_t k_ReadoutShrinkMs = 1000;
/// Its size goes in this many steps
constexpr int32_t k_ReadoutSteps = 255;
/// Knocking starts the read-out, or, once it has grown in, holds it for its full time again; while it shrinks away it
/// turns round and grows back from the size it has reached. `remaining` counts down to 0, where nothing shows.
[[nodiscard]] int32_t Knock(int32_t remaining);
/// The time left once a frame of game time has gone by, never below 0
[[nodiscard]] int32_t Tick(int32_t remaining, uint32_t frameMs);
/// How many of the 255 steps the markers have with this much time left: they grow in over the first second and
/// shrink away over the last, the fraction cut off
[[nodiscard]] int32_t ReadoutSteps(int32_t remaining);
/// The markers' size for a number of steps, from nothing to full (1 at 255 steps)
[[nodiscard]] float StepsScale(int32_t steps);
/// The markers' size with this much time left: StepsScale(ReadoutSteps(remaining))
[[nodiscard]] float ReadoutScale(int32_t remaining);

/// Which buildings draw the read-out over themselves when they are drawn: all but the fields, the football pitch, the
/// totem and the spell dispenser, which draw themselves another way
[[nodiscard]] constexpr bool DrawsReadout(AbodeType type)
{
	return type != AbodeType::Field && type != AbodeType::FootballPitch && type != AbodeType::Totem &&
	       type != AbodeType::SpellDispenser;
}
/// Whether a drawn building shows a row: one of the knocked town's while the read-out has steps (no town matching no
/// town), or the home of a villager in the hand
[[nodiscard]] constexpr bool ShowsRow(int32_t steps, bool knockedTown, bool heldVillagersHome)
{
	return (steps != 0 && knockedTown) || heldVillagersHome;
}

/// A marker's colour, 0xAARRGGBB: gold for each adult living there, green for each free place; the home of a villager
/// in the hand shows a single teal one
constexpr uint32_t k_LivedInColour = 0xFFF8D235u;
constexpr uint32_t k_FreePlaceColour = 0xFF5C822Fu;
constexpr uint32_t k_HeldVillagersHomeColour = 0xFF4995A9u;
/// The adults a held villager's home passes for its single teal marker
constexpr int32_t k_HeldVillagersHome = -1;
/// Whether a building's row shows: while the read-out has time left, or for a held villager's home whenever
[[nodiscard]] constexpr bool RowDrawn(int32_t remaining, int32_t adults)
{
	return remaining != 0 || adults == k_HeldVillagersHome;
}
/// The markers stand 1 / 1.3 apart along the screen's sideways line, and are 1 / 1.3 big at full size
constexpr float k_MarkerDivisor = 1.3f;
/// They are never drawn smaller than this
constexpr float k_SmallestMarker = 0.0001f;
/// Over a building, the row is raised past twice its model's half height by this much
constexpr float k_RaisedAbove = 1.5f;
/// The little person's picture: cell 57 of the misc0 sheet's eight by eight (column 1 of row 7)
constexpr uint8_t k_PersonCell = 57;

/// One marker of a row over a building
struct Marker
{
	/// How far along the screen's sideways line from the row's start
	float along;
	uint32_t colour;
	/// Its half size
	float size;
};
/// The markers over a building with `places` places and `adults` adults living there, at a scale. The home of a
/// villager in the hand passes one place and k_HeldVillagersHome: one teal marker at full size, whatever the scale.
[[nodiscard]] std::vector<Marker> Readout(int32_t places, int32_t adults, float scale);
/// The row's point over a building: at the model's position, raised past twice its half height (no scale) by 1.5
[[nodiscard]] glm::vec3 RowPoint(const glm::vec3& position, float halfHeight);
/// The row starts half a place to the left of that point for each place, along the camera's right
[[nodiscard]] glm::vec3 RowStart(const glm::vec3& point, const glm::vec3& right, int32_t places);
/// A marker's point: from the row's start, along the camera's right
[[nodiscard]] glm::vec3 MarkerPoint(const glm::vec3& start, const glm::vec3& right, float along);

} // namespace openblack::ecs::abode_knock
