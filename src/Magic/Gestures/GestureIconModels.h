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
#include <optional>

#include <glm/ext/matrix_double4x4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "GestureIcons.h"

// The 3D model inside a gesture icon, in the box above its symbol: the mesh of the leash the icon picks (turning on
// its own), the miracle bubble for the spiral, or the miracle's seed. It is drawn with a camera of its own, on the
// world's lens, at the middle of the screen, and moved on the screen to its box; the depth in that box is reset first,
// so the model shows over the land. Wiki: docs/wiki/magic/gestures.md#the-gesture-icons.

namespace openblack::magic::gestures
{
/// What an icon shows above its symbol
enum class IconModelKind : uint8_t
{
	None,
	Leash,       ///< the leash's mesh, turning
	SeedGraphic, ///< the miracle's seed graphic at its power-up level (GestureIconSeeds.h)
	Bubble,      ///< the miracle bubble, for the spiral with no seed
};

/// The seed an icon shows: the creature's miracles' seed for the inverse spiral, else its own
inline constexpr int k_InverseSpiralSeed = 22;
[[nodiscard]] int IconModelSeed(const GestureIcon& icon);
/// The model of an icon: its leash's when it has one, else its seed's, else the bubble for the spiral
[[nodiscard]] IconModelKind IconModelKindOf(const GestureIcon& icon);

/// The models' camera: at the eye, looking at the focus, k_IconDepth along z, a fifth of that above it
inline constexpr glm::vec3 k_IconModelEye {0.0f, 2.0f, 0.0f};
inline constexpr glm::vec3 k_IconModelFocus {0.0f, 0.0f, k_IconDepth};
/// The pixels every vertex of the model is moved by: from the middle of the screen (its halves in whole pixels) to
/// the middle of the box above the icon's
[[nodiscard]] glm::vec2 IconModelScreenOffset(const IconBox& box, glm::ivec2 screen);
/// The box above the icon, as big as it, whose depth is reset before the model: its corners in pixels, the top left
/// first and then round the box
[[nodiscard]] std::array<glm::vec2, 4> IconModelDepthBox(const IconBox& box);
/// A point of the models' camera's space where the world's view draws it, moved on the screen as every vertex of the
/// icon's model is: to the screen through `iconToClip` (the world's projection times the models' view), moved by
/// `offset` pixels and held on the screen (x 0 to the width less one, y alike), then back into the world through
/// `clipToWorld` (the inverse of the world's projection times its view) at the same depth. A point behind the camera
/// keeps its place in clip space
[[nodiscard]] glm::vec3 IconPointInWorld(const glm::dmat4& iconToClip, const glm::dmat4& clipToWorld, glm::vec2 offset,
                                         glm::vec2 screen, glm::vec3 point);

/// A leash's spin: made unturned the first time an icon shows that leash, kept until the map is cleared, and turned on
/// a little about three axes each time an icon draws it
struct LeashSpin
{
	std::array<glm::vec3, 3> rows {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
};
/// One spin for each leash number 0 to 3
using LeashSpins = std::array<std::optional<LeashSpin>, 4>;
/// One draw of a leash's model: the frame's seconds turn it about x by (1 - 0.05 l) 0.65823, about y by
/// (1 + 0.05 l) and about z by (1 - 0.06 l)(1 + 0.05 l) 0.79543 radians a second, l the leash number, each a float
void StepLeashSpin(LeashSpin& spin, int leash, uint32_t frameMs);
/// The leash model's matrix: its spin's rows times 0.15, k_IconDepth along z
[[nodiscard]] glm::mat4 LeashModelMatrix(const LeashSpin& spin);
/// The band of the leash texture the model shows (its v offset), as the temple's posts: 0.375 for leash 1, else
/// (leash - 1) / 8
[[nodiscard]] float LeashModelBand(int leash);
/// The bubble's matrix: a tenth of a unit, a quarter turn about x, k_IconDepth along z
[[nodiscard]] glm::mat4 BubbleModelMatrix();
/// Whether the bubble is drawn: the object's draw leaves a model out when its half diagonal times its scale (0.1) times
/// 300, squared, is short of its squared distance from the camera
[[nodiscard]] bool BubbleModelDrawn(float halfDiagonal);
/// The model's colour is white with the icons' alpha byte: their alpha times 255, truncated
[[nodiscard]] uint8_t IconModelAlpha(float alpha);

/// What one drawn icon's model is this frame
struct IconModelDraw
{
	IconModelKind kind {IconModelKind::None};
	glm::mat4 matrix {1.0f}; ///< in the models' camera's space
	float band {0.0f};       ///< the leash texture's v offset
	uint8_t alpha {255};
};
/// The model of a drawn icon this frame: a leash's spin is made at its first draw and turns on by the frame; the bubble
/// is left out by its far cut; a seed graphic is GestureIconSeeds.h's (only its kind here). `bubbleHalfDiagonal` is
/// the bubble mesh's
[[nodiscard]] IconModelDraw StepIconModel(const GestureIcon& icon, LeashSpins& spins, float alpha, uint32_t frameMs,
                                          float bubbleHalfDiagonal);
/// The leashes' spins (the miracles' service's hand magic state), kept until the map is cleared
[[nodiscard]] LeashSpins& IconLeashSpins();
/// The map is cleared: every leash's spin goes, and is made unturned again at that leash's next draw, and every seed
/// graphic goes with its holder effect (ResetIconSeeds)
void ResetIconModels();
} // namespace openblack::magic::gestures
