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
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// The light bulb a creature shows over its head as it learns from what it sees: how far it has got, as a percent,
/// grows, rises and fades away over six seconds, the bulb blinking six times
namespace openblack::creature_lightbulb
{

/// A bulb lives this long, and fades over the last part of it
constexpr float k_LifeMs = 6000.0f;
constexpr float k_FadeStartMs = 2500.0f;
constexpr float k_FadeMs = 3500.0f;
/// It rises this much of its size every millisecond
constexpr float k_RiseSpeed = 0.003f;
/// Its text stands this many of its sizes over it, this many pixels higher on the screen, at this size
constexpr float k_TextHeight = 1.5f;
constexpr float k_TextOffset = 7.5f;
constexpr float k_TextSize = 15.0f;
/// The bulb hangs this far under its glow, drawn at a third of the glow's size
constexpr float k_BulbHeight = 2.6f;
constexpr float k_BulbScale = 3.0f;
/// It blinks this many times in its life
constexpr float k_Blinks = 6.0f;
/// The glow is yellow, the bulb white with its blink as its blue, the text yellow over a black shadow this many pixels
/// right and down, all as 0xRRGGBB
constexpr uint32_t k_GlowColour = 0xFFFF00u;
constexpr uint32_t k_BulbColour = 0xFFFF00u;
constexpr uint32_t k_TextColour = 0xFFFF00u;
constexpr uint32_t k_ShadowColour = 0x000000u;
constexpr float k_ShadowOffset = 1.0f;
/// A creature makes at most one bulb in this many turns
constexpr uint32_t k_TurnsBetween = 100;
/// It is made this far above the head, its size this share of the creature's height
constexpr float k_AboveHead = 1.0f;
constexpr float k_HeightShare = 0.1f;
/// A miracle seen this few times past its learning still shows a full bulb
constexpr float k_RecentlyLearntSightings = 3.0f;
/// A normal action seen but not learnt yet shows half a bulb
constexpr float k_NearlyLearntNormal = 0.5f;

/// One bulb as it lives: where its glow is, its size, how opaque it may be, how long it has lived, and its text
struct Bulb
{
	glm::vec3 position {0.0f};
	float size {0.0f};
	float opacity {1.0f};
	uint32_t ageMs {0};
	std::u16string text;
};

/// How a bulb is drawn this frame: its glow's size and the bulb's, the text's and the glow's alpha, and the bulb's blink
struct Look
{
	glm::vec3 glow {0.0f};
	float glowSize {0.0f};
	glm::vec3 bulb {0.0f};
	float bulbScale {0.0f};
	glm::vec3 text {0.0f};
	uint8_t alpha {0xFF};
	/// The bulb's blink, 0 to 255, the blue of its colour, and the glow's alpha, which is the other half of it
	uint8_t blink {0};
	uint8_t glowAlpha {0};
};

/// Whether a creature may make a bulb now: more than 100 turns since the last it made
[[nodiscard]] bool MayMake(uint32_t lastTurn, uint32_t turn);
/// The bulb made for a creature's head at how far it has got (0 to 1): above the head, a tenth of its height, its text
/// the share as a whole percent
[[nodiscard]] Bulb Make(const glm::vec3& head, float creatureHeight, float progress);
/// The bulb's text: the share as a whole percent, as the original's "%.0f%%"
[[nodiscard]] std::u16string Text(float progress);
/// A frame of the game's time (milliseconds) on a bulb: it ages, rises and grows; none once its life is over
[[nodiscard]] std::optional<Look> Step(Bulb& bulb, uint32_t milliseconds);
/// A frame of the game's time on every bulb a creature shows, in their order: those whose life is over go, and how the
/// others are drawn this frame comes back, in the same order
[[nodiscard]] std::vector<Look> StepAll(std::vector<Bulb>& bulbs, uint32_t milliseconds);

/// The glow's colour, 0xAARRGGBB: yellow at the glow's alpha
[[nodiscard]] constexpr uint32_t GlowArgb(const Look& look) noexcept
{
	return (static_cast<uint32_t>(look.glowAlpha) << 24u) | k_GlowColour;
}
/// The bulb's colour, 0xRRGGBB, in place of the land's light: white, its blue the blink
[[nodiscard]] constexpr uint32_t BulbRgb(const Look& look) noexcept
{
	return k_BulbColour | look.blink;
}
/// The bulb's matrix: at its point, scaled alike on every axis, not turned
[[nodiscard]] glm::mat4 BulbModel(const Look& look);
/// Where a point falls on a screen of `resolution` pixels (y from the top) from its clip-space point: none when it is
/// nearer the eye than the near plane, wherever it falls across, on the screen or off it
[[nodiscard]] std::optional<glm::vec2> ScreenPoint(const glm::vec4& clip, float nearPlane, glm::vec2 resolution);
/// Where the text starts on the screen, from where its point falls: centred across by its width, k_TextOffset pixels up
[[nodiscard]] glm::vec2 TextPlace(glm::vec2 screenPoint, float textWidth);

/// The bulb a sighting of a normal action shows: a full one when it is learnt now and was not known before, half of one
/// while it is not yet learnt and not known; none when it is too young to learn it or knows it already
[[nodiscard]] std::optional<float> ForAction(bool learnt, bool knownBefore, bool tooYoung);
/// The bulb a sighting of a miracle shows, old enough to learn it: a full one when it is learnt and seen fewer than three
/// times past what it needed, its share of the sightings while not yet learnt; none otherwise
[[nodiscard]] std::optional<float> ForMiracle(bool learnt, float seen, float needed);

} // namespace openblack::creature_lightbulb
