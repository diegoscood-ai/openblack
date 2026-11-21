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

#include <array>
#include <span>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The shell over a citadel heart as it is destroyed, from the fade at fourteen seconds until the smoke: the creed model
/// at the heart, unturned, its glowing shell (see particles::volume_glow) in the owner's colour. The model swells and
/// the shell reaches out along the destruction's clock, the shell beating with the local player's heart. Free of state,
/// so it is tested on its own; every step is worked in single precision, one rounding a step, as the game works it.
namespace openblack::heart_shell
{

/// The moments of the clock, in seconds, the model's size and the shell's reach are drawn straight between
inline constexpr std::array<float, 4> k_Moments {0.0f, 1.0f, 20.0f, 22.0f};
/// The model's size at each moment, before k_SizeScale
inline constexpr std::array<float, 4> k_Sizes {0.0f, 1.0f, 1.0f, 2.0f};
inline constexpr float k_SizeScale = 0.3f;
/// How far the shell reaches out at each moment, in parts of the model's half diagonal, before the heartbeat's pulse
inline constexpr std::array<float, 4> k_Reaches {0.0f, 5.0f, 5.0f, 10.0f};
/// How fast the shell's texture slides across and up, in sheets a second of the game's time
inline constexpr glm::vec2 k_SlideSpeed {0.6f, 0.2f};
/// How much of the texture the shell spans across and up
inline constexpr glm::vec2 k_TextureFraction {0.7f, 0.1f};
/// The shell's texture, by its number among the glowing shells' textures (particles::volume_glow::k_Textures)
inline constexpr int k_Texture = 6;
/// The grey the shell adds at its inner edge, 0xRRGGBB
inline constexpr uint32_t k_Added = 0x141414u;

/// A value drawn straight between the moments: the first one's up to the first moment, the last one's from the last
/// moment on (a clock that is not a number takes the first)
[[nodiscard]] float Along(const std::array<float, 4>& values, float clock);
/// The model's size at the clock
[[nodiscard]] float Size(float clock);
/// How far the shell reaches out at the clock, by the heartbeat's pulse
[[nodiscard]] float Reach(float clock, float pulse);
/// The model's matrix: its size on every axis, unturned, at the heart's point
[[nodiscard]] glm::mat4 Model(glm::vec3 heart, float size);

/// Where a shown shell is drawn in a frame: at its heart's row while the heart has one; at the heart's drawn point
/// once the heart itself is no longer drawn (from a little after the explosion), and so has no row; else not
enum class Placement : uint8_t
{
	AtHeart,
	Alone,
	None,
};
[[nodiscard]] constexpr Placement PlacementOf(bool heartHasRow, bool heartHidden)
{
	if (heartHasRow)
	{
		return Placement::AtHeart;
	}
	return heartHidden ? Placement::Alone : Placement::None;
}

/// The order the hearts are drawn in, and so their shells, by their creation stamps (Temple::creationStamp): newest
/// first, as the hearts' list takes each new heart at its head. Gives the places in `stamps`
[[nodiscard]] std::vector<size_t> DrawOrder(std::span<const uint32_t> stamps);

/// A heart in the hearts' pass: its creation stamp, and whether its shell and its model are drawn this frame
struct PassHeart
{
	uint32_t creationStamp {0};
	bool shell {false};
	bool model {false};
};
/// One draw of the hearts' pass: a heart's shell or its model
enum class PassStep : uint8_t
{
	Shell,
	Model,
};
struct PassDraw
{
	/// The heart's place in the hearts handed in
	size_t heart {0};
	PassStep step {PassStep::Shell};
	bool operator==(const PassDraw&) const = default;
};
/// The hearts' pass, before the water rings and the models: the hearts newest first (DrawOrder), each one's shell
/// first, then its model, so that each heart comes right after its own shell
[[nodiscard]] std::vector<PassDraw> Pass(std::span<const PassHeart> hearts);

} // namespace openblack::heart_shell
