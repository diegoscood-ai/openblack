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

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

// An atom's drawn matrix as the original's step keeps it, the form a saved game writes: the rows of the atom's turn
// times its scales, its place, through the frame of the nearest ancestor atom whose group passes its frame down. The
// drawing itself uses the atom's DrawState; this is kept beside it, step by step, so that a save writes the matrix the
// original would have (docs/wiki/engine/saved-games.md#openblack, the particles). Pure maths.
namespace openblack::particles::saved_draw
{

struct SavedDraw
{
	/// The three rows and the translation, as the original's 3x4 matrix lays them out
	std::array<float, 12> matrix {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
	float scale {1.0f};
	uint8_t alpha {0};

	bool operator==(const SavedDraw&) const = default;
};

/// The atom's own matrix: its turn's rows times its scale times its base scale (the second row times its stretch as
/// well), at its place; its scale is the scale times the base scale
[[nodiscard]] SavedDraw Local(const glm::mat3& rotation, float baseScale, float ruleScale, float stretch,
                              const glm::vec3& position);

/// One step's drawn matrix: the local one taken through the frame its parent passes down (none for an atom no
/// ancestor passes a frame to), its scale times the frame's; its alpha the colour's alpha times the collection's over
/// 255, whole, or the colour's alpha as it is when the collection's is 255
[[nodiscard]] SavedDraw Step(const SavedDraw& local, const SavedDraw* frame, uint8_t colourAlpha, uint8_t collectionAlpha);

/// The fraction of the step the draw blends at, as the original's atom draw takes it: 0 below 0, at most 5
inline constexpr float k_MaxBlend = 5.0f;
[[nodiscard]] float BlendFraction(float t);

/// The drawn matrix between the last step's and this step's, as the original's draw blends them: every cell of the
/// matrix and the scale (this - last) x t + last; the alpha byte the last's when both are the same, else the same
/// blend of the two bytes, kept in 0..255 and truncated
[[nodiscard]] SavedDraw Blend(const SavedDraw& last, const SavedDraw& current, float t);

/// An alpha byte through the effect's alpha byte, as the original's draw takes it: as it is at 255, else
/// (effect x byte) >> 8
[[nodiscard]] uint8_t WithEffectAlpha(uint8_t alpha, uint8_t effectAlpha);

} // namespace openblack::particles::saved_draw
