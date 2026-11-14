/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/AtomSavedDraw.h"

using namespace openblack::particles;

namespace
{
float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

std::array<uint32_t, 13> BitsOf(const saved_draw::SavedDraw& saved)
{
	std::array<uint32_t, 13> bits {};
	for (size_t i = 0; i < saved.matrix.size(); ++i)
	{
		bits.at(i) = std::bit_cast<uint32_t>(saved.matrix.at(i));
	}
	bits[12] = std::bit_cast<uint32_t>(saved.scale);
	return bits;
}

/// A turn by its rows, as the original lays them out
glm::mat3 Rows(const std::array<uint32_t, 9>& rows)
{
	glm::mat3 rotation;
	for (glm::length_t row = 0; row < 3; ++row)
	{
		for (glm::length_t column = 0; column < 3; ++column)
		{
			rotation[row][column] = F(rows.at(static_cast<size_t>(row * 3 + column)));
		}
	}
	return rotation;
}
} // namespace

// An atom no ancestor passes a frame to: its drawn matrix is its turn's rows times its scales at its place, and its
// alpha the colour's times the collection's over 255. The values are a saved atom's of one of the user's saved games
// (an atom core of profile D's slot 14, its first save), and the drawn matrix the one its file holds, bit for bit;
// every one of the 1953 saved atoms of the 19 world files read gives back its file's matrix, scale and alpha the same
// way
TEST(ParticleSavedDraw, AnAtomWithNoFrameIsDrawnAtItsPlaceTurnedAndScaledAsTheOriginalKeepsIt)
{
	const auto rotation =
	    Rows({0x3F7FFD61, 0x00000000, 0x3C127071, 0x00000000, 0x3F800000, 0x00000000, 0xBC127071, 0x00000000, 0x3F7FFD61});
	const auto local = saved_draw::Local(rotation, F(0x3FAA4064), F(0x3F800000), F(0x3F800000),
	                                     glm::vec3(F(0x451AF361), F(0x41B7FFBA), F(0x450C8318)));
	const auto saved = saved_draw::Step(local, nullptr, 0xFA, 204);
	constexpr std::array<uint32_t, 13> k_Saved {0x3FAA3EA6, 0x00000000, 0x3C42C701, 0x00000000, 0x3FAA4064,
	                                            0x00000000, 0xBC42C701, 0x00000000, 0x3FAA3EA6, 0x451AF361,
	                                            0x41B7FFBA, 0x450C8318, 0x3FAA4064};
	EXPECT_EQ(BitsOf(saved), k_Saved);
	EXPECT_EQ(saved.alpha, 200u);
}

// An atom its parent passes a frame to: its local matrix taken through the frame (the local one first), its scale
// times the frame's; an opaque collection leaves the colour's alpha as it is. A saved atom of the same file, bit for
// bit
TEST(ParticleSavedDraw, AnAtomInItsParentsFrameIsTakenThroughItAsTheOriginalKeepsIt)
{
	const auto local = saved_draw::Local(glm::mat3(1.0f), F(0x40400000), F(0x3F800000), F(0x3F800000),
	                                     glm::vec3(F(0xBE42A482), 0.0f, F(0x3E1C18F0)));
	saved_draw::SavedDraw frame;
	frame.matrix = {F(0x3E4CCCCD), 0.0f, 0.0f,          0.0f,          F(0x3E4CCCCD), 0.0f,
	                0.0f,          0.0f, F(0x3E4CCCCD), F(0x44A9AF66), F(0x4254F766), F(0x45168F50)};
	frame.scale = F(0x3E4CCCCD);
	const auto saved = saved_draw::Step(local, &frame, 0xBE, 0xFF);
	constexpr std::array<uint32_t, 13> k_Saved {0x3F19999A, 0x00000000, 0x00000000, 0x00000000, 0x3F19999A,
	                                            0x00000000, 0x00000000, 0x00000000, 0x3F19999A, 0x44A9AE2F,
	                                            0x4254F766, 0x45168FCD, 0x3F19999A};
	EXPECT_EQ(BitsOf(saved), k_Saved);
	EXPECT_EQ(saved.alpha, 0xBEu);
}

// The second row also takes the stretch
TEST(ParticleSavedDraw, TheStretchScalesTheSecondRowOnly)
{
	const auto saved = saved_draw::Local(glm::mat3(1.0f), 2.0f, 1.5f, 4.0f, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(saved.matrix, (std::array<float, 12> {3.0f, 0.0f, 0.0f, 0.0f, 12.0f, 0.0f, 0.0f, 0.0f, 3.0f, 1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(saved.scale, 3.0f);
}

// The blend of the two kept matrices is the original's SetBlend: every cell (this - last) x t + last, each operation a
// float. The cells are the original's own answer for these two matrices at 0.3, run on the exe through the oracle
TEST(ParticleSavedDraw, TheTwoKeptMatricesAreBlendedAsTheOriginalBlendsThem)
{
	saved_draw::SavedDraw last;
	last.matrix = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
	last.scale = 2.0f;
	last.alpha = 10;
	saved_draw::SavedDraw current;
	current.matrix = {0.1f, 3e-8f, 1e7f, -4.0f, 5.5f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 13.0f};
	current.scale = 3.0f;
	current.alpha = 20;
	const auto blended = saved_draw::Blend(last, current, 0.3f);
	constexpr std::array<uint32_t, 12> k_Oracle {0x3F3AE148, 0x3FB33333, 0x4A371B09, 0x3FCCCCCC, 0x40A4CCCD, 0x40C00000,
	                                             0x40E00000, 0x41000000, 0x41100000, 0x41200000, 0x41300000, 0x4144CCCD};
	for (size_t i = 0; i < k_Oracle.size(); ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(blended.matrix.at(i)), k_Oracle.at(i)) << i;
	}
	EXPECT_EQ(blended.scale, (3.0f - 2.0f) * 0.3f + 2.0f);
	// the alpha: the same blend, truncated, kept in 0..255; the last one's when both are the same
	EXPECT_EQ(blended.alpha, 13u);
	EXPECT_EQ(saved_draw::Blend(last, current, 5.0f).alpha, 60u);
	current.alpha = 255;
	EXPECT_EQ(saved_draw::Blend(last, current, 5.0f).alpha, 255u);
	current.alpha = 10;
	EXPECT_EQ(saved_draw::Blend(last, current, 0.7f).alpha, 10u);
}

// The draw's fraction is kept in 0..5 (NaN as 0), and the effect's alpha takes the byte as the original's draw does
TEST(ParticleSavedDraw, TheDrawsFractionAndTheEffectsAlphaAreTheOriginals)
{
	EXPECT_EQ(saved_draw::BlendFraction(-1.0f), 0.0f);
	EXPECT_EQ(saved_draw::BlendFraction(std::bit_cast<float>(0x7FC00000u)), 0.0f);
	EXPECT_EQ(saved_draw::BlendFraction(2.5f), 2.5f);
	EXPECT_EQ(saved_draw::BlendFraction(7.0f), 5.0f);
	EXPECT_EQ(saved_draw::WithEffectAlpha(200, 255), 200u);
	EXPECT_EQ(saved_draw::WithEffectAlpha(200, 128), 100u);
	EXPECT_EQ(saved_draw::WithEffectAlpha(255, 254), 253u);
}
