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
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

// What a spell icon draws while it charges, as pure functions of the icon's charge and the clocks: the colour added to
// the icon's light, the sheet of light round it and the pulse ring (Worship/WorshipSpellIcon.cpp draws them).
// Every product and sum is a single precision one, in the game's order; sines and cosines are taken in double and
// rounded once by the operation that uses them.

namespace openblack::worship::charge_visual
{

/// The charge as the visual reads it
struct ChargeLevel
{
	/// 0 to 1: 1 with a seed held, else the icon's charge fraction when its store is not empty, else 0; while charging
	/// it is lifted to (0.2 + f) / 1.2, so a charge just started shows a sixth
	float fraction {0.0f};
	/// A seed held, or a store with nothing more needed (needed <= 0)
	bool full {false};
};

/// The charge from the icon's state: whether a seed of it is held, its store, its charge fraction, the chants it still
/// needs and whether it is charging. The fraction and the need are only asked for with a store
[[nodiscard]] ChargeLevel Level(bool seedHeld, float store, float fraction, float needed, bool charging);

/// The seconds since the charge started: (the whole turns since then + the turn's fraction) x the ms of a turn x 0.001
[[nodiscard]] float ChargeSeconds(int32_t turnsSinceStart, float turnFraction, uint32_t msPerTurn);

/// The brightness the player's colour is scaled by, 0 to 255, or nothing when the icon is neither full nor charging.
/// Full: 255 (0.1 + 0.5 (sin(4 pi phase) + 1) 0.5), between 26 and 153, a pulse at twice the phase's rate (phase:
/// the particle system's 0..1 over 3.33 s). Charging: two flashes over the first 1.5 s,
/// 50 + 205 clamp((1 - cos 8s) 0.5 (1 - s / 1.5), 0, 1), then 50. Rounded to the nearest
[[nodiscard]] std::optional<int> Brightness(bool full, bool charging, float phase, float chargeSeconds);

/// The colour added to the icon's light (its specular), 0xAARRGGBB: each of the player colour's red, green and blue
/// times the brightness, over 256, its alpha kept; 0 when the icon is neither full nor charging. An icon with a
/// colour that is not 0 is drawn with the white tint and this added to the land's specular
[[nodiscard]] uint32_t Tint(uint32_t playerColour, bool full, bool charging, float phase, float chargeSeconds);

/// The sheet of light round the icon (particles::LightSheet): its points, how high it stands, how far its top is spread
/// out from their middle, and how often the charge fed in at its first point moves one point along. It is fed the
/// charge's fraction (ChargeLevel) every frame and moved on by the frame's game time
inline constexpr size_t k_SheetPoints = 10;
inline constexpr float k_SheetHeight = 6.0f;
inline constexpr float k_SheetSpread = 1.5f;
inline constexpr float k_SheetShiftSeconds = 0.03f;

/// The sheet's points: a circle of radius 1 in the icon's own x-z plane through its matrix (model: the icon's object
/// matrix, its columns the original's rows), point i at the angle i x 2 pi / 9, so the last is the first again
[[nodiscard]] std::array<glm::vec3, k_SheetPoints> SheetPoints(const glm::mat4& model);

/// The pulse ring over a charging icon on one frame
struct RingPose
{
	float t {0.0f};     ///< How far through its pulse, 0 to 1
	float scale {0.0f}; ///< 3 (1 - t): it shrinks to nothing
	float raise {0.0f}; ///< How far up the icon's own y axis: 0.1 x the icon mesh's whole height (2 x its half height)
	uint8_t alpha {0};  ///< 255 t, cut to a whole number: it fades in as it shrinks
};

/// The ring on a frame of a charge at the phase (the particle system's 0..1 over 3.33 s): t = (the phase less 0.1,
/// taken round to 0..1) x 3.33, so a pulse every 3.33 s, lasting 0.3 of it; nothing when t is not between 0 and 1.
/// meshHalfHeight: the icon mesh's half height, unscaled
[[nodiscard]] std::optional<RingPose> Ring(float phase, float meshHalfHeight);

/// The ring's matrix from the icon's (both object matrices, their columns the original's rows): raised along the
/// icon's y axis (that axis times the raise added to its position), then its three axes times the ring's scale
[[nodiscard]] glm::mat4 RingModel(const glm::mat4& iconModel, const RingPose& pose);

} // namespace openblack::worship::charge_visual
