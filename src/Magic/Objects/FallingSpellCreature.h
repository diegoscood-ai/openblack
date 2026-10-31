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

#include <functional>
#include <optional>

/// The falling spell's copy of the player's creature, as pure rules of the film time. Wiki:
/// docs/wiki/magic/falling-spell.md#the-falling-spell-what-fallingspell-draws.
///
/// - The copy is made at the film's start, at the world's origin facing heading 0, and plays one individual action.
///   Each update it advances by the film's milliseconds since the last update, when that is more than 0.
/// - Each drawn frame it is not drawn from 19 550 to 27 350 ms of film (both included). Otherwise three local draws
///   pick a flicker colour, and the copy is drawn with a diffuse colour that darkens from white towards orange-brown and
///   a specular colour that rises from black to the flicker, both by a level of film time.
/// - Its two hand glows (the first and the third) are lit at Init and get a power of 1 + 2x in their windows of film
///   time, x rising over one second and falling over two; outside the windows, and while the copy is hidden, they keep
///   the last power given.
/// - Its hair moves on each drawn frame, hidden or not, by the frame's game time, and is drawn in the body's tint.
namespace openblack::magic::falling_spell
{

/// The copy is hidden from this film time ...
inline constexpr int32_t k_CreatureHiddenFromMs = 19550;
/// ... to this one, both included
inline constexpr int32_t k_CreatureHiddenToMs = 27350;
/// The tint level rises from 0 to 255 over the first 19 450 ms of film
inline constexpr int32_t k_TintRiseMs = 19450;
/// From 19 450 ms it starts again from 0, a full rise being 7900 ms (it is hidden at 19 550 ms, and from 27 350 ms the
/// level is 0)
inline constexpr int32_t k_TintSecondRiseMs = 7900;
/// The individual action the copy plays from the start, played once and not mirrored: the second of the scripts' own
/// actions, the animation at this place in the creature spec file (creature_layers::animations), whichever clip each
/// species' file puts there
inline constexpr int32_t k_CreatureAction = 0xD7;
/// The film time the copy's clock starts from (its first update advances it by the film's ms less this)
inline constexpr int32_t k_CreatureStartMs = 100;
/// The step of its time the copy is given as it is made, before its time becomes the film's
inline constexpr int32_t k_CreatureFirstStepMs = 1;

/// How many milliseconds the copy's animations move on by in a step of its time of `milliseconds`. Until its time is the
/// film's (`filmTimed`, set just after its first step), at its size's rate: the milliseconds times 1.6 less half its
/// size times 0.85, truncated toward zero. Once it is, the milliseconds themselves
[[nodiscard]] int32_t CreatureAnimationMs(int32_t milliseconds, float size, bool filmTimed) noexcept;

/// Whether the copy is not drawn at this film time
[[nodiscard]] constexpr bool CreatureHidden(int32_t filmMs) noexcept
{
	return filmMs >= k_CreatureHiddenFromMs && filmMs <= k_CreatureHiddenToMs;
}

/// The tint level 0..255 at a film time (integer division, truncated toward zero)
[[nodiscard]] int32_t CreatureTintLevel(int32_t filmMs) noexcept;

/// The flicker of one drawn frame: three local draws, in this order: red of 32, green of 8, blue of 4
struct Flicker
{
	uint32_t red {0};
	uint32_t green {0};
	uint32_t blue {0};
};
/// The three draws' ranges, in their order
inline constexpr int32_t k_FlickerRedRange = 0x20;
inline constexpr int32_t k_FlickerGreenRange = 8;
inline constexpr int32_t k_FlickerBlueRange = 4;

/// The two colours the copy's body is drawn with, as 0xAARRGGBB
struct CreatureTint
{
	uint32_t colour {0xFFFFFFFFu};   ///< the diffuse colour
	uint32_t specular {0xFF000000u}; ///< the colour added to the light

	bool operator==(const CreatureTint&) const = default;
};

/// The tint at a level 0..255. The specular is the flicker colour (0x20 + red, 8 + green, blue) with each channel
/// times level / 256 (truncated), alpha 0xFF. The diffuse is white less (127, 191, 255) x level / 256, each channel
/// rounded up: (0x80, 0x40, 0) at level 255. Alpha 0xFF.
[[nodiscard]] CreatureTint TintOf(int32_t level, const Flicker& flicker) noexcept;

/// The copy's tint at a film time: nothing while it is hidden (and then no draws); otherwise the three draws are made
/// with `localRand` (a range, returns 0..range - 1) in their order, and the tint of the level
[[nodiscard]] std::optional<CreatureTint> CreatureTintAt(int32_t filmMs, const std::function<uint32_t(int32_t)>& localRand);

/// The copy's two hand glows
enum class FallGlow : uint8_t
{
	First = 0,
	Third = 2,
};
/// The power each hand glow is lit with at the film's start
inline constexpr float k_GlowStartPower = 1.0f;
/// The scale each hand glow is given, at the start and with each power
inline constexpr float k_GlowScale = 1.0f;

/// The power a hand glow is given at a film time, while the copy is drawn: 1 + 2x, x = clamp(ms into the window x
/// 0.001, 0, 1), the ms into the window rising over 1000 ms, then falling by half a ms a ms over 2000 ms from 1000.
/// Nothing (the glow keeps its last power) when that is 0: outside its windows, or at a window's first ms. The windows:
/// the first glow 16 350..19 350 and 31 650..32 650 (rise only), the third 17 200..20 200 and 36 500..37 500 (rise
/// only); each end excluded.
[[nodiscard]] std::optional<float> GlowPower(FallGlow glow, int32_t filmMs) noexcept;

/// The seconds the copy's hair moves on by in each frame the film draws, hidden or not, as any creature's hair outside
/// the citadel: the frame's game milliseconds (none while the game is paused, as it is through the film), or its real
/// ones while a box has the mouse, times 0.001 and held to 0..1
[[nodiscard]] float CreatureHairSeconds(bool boxHasMouse, uint32_t frameRealMs, uint32_t frameGameMs) noexcept;

/// What the update advances the copy by: the film ms less the last update's, when that is more than 0. The last
/// update's ms becomes this one's either way.
[[nodiscard]] constexpr std::optional<int32_t> CreatureAdvance(int32_t lastMs, int32_t filmMs) noexcept
{
	// in 32 bits, wrapping
	const auto delta = static_cast<int32_t>(static_cast<uint32_t>(filmMs) - static_cast<uint32_t>(lastMs));
	return delta > 0 ? std::optional(delta) : std::nullopt;
}

} // namespace openblack::magic::falling_spell
