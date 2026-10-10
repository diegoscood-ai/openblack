/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The globe's looks (src/Magic/MiracleVisuals.h) against the modules that draw the globes today: the glint's cell is the
// bubble's frame (frame_anim::OneOffFrame), the rings are the seed graphic's bands (worship::seed_graphic: as many, as
// faint, turned the same way), and where the two differ (the globe's alpha) ours is pinned. Then the globes' draws in
// the frame's single queue (src/Graphics/Globes.h): an empty list queues nothing, and each draw is one Z object keyed
// at its sort point as the models are, whose entry is not a model. Synthetic values only.

#include <cstddef>
#include <cstdint>

#include <array>
#include <bit>
#include <span>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/Globes.h"
#include "Graphics/ZObject.h"
#include "Graphics/ZSort.h"
#include "Magic/MiracleVisuals.h"
#include "Worship/SpellSeedGraphic.h"

using namespace openblack;
using namespace openblack::magic::visuals;

namespace
{
constexpr float k_Epsilon = 1e-4f;
constexpr glm::vec3 k_Camera {0.0f, 0.0f, 0.0f};
/// The angles the rings and bands are checked at, in radians
constexpr std::array<float, 5> k_Spins = {0.0f, 0.7f, 1.5f, 3.1f, 6.2f};
} // namespace

TEST(MiracleVisuals, TheGlintRunsThroughItsSixteenCellsEighteenASecond)
{
	EXPECT_NEAR(StepGlint(0.0f, 0.5f), 9.0f, k_Epsilon);
	EXPECT_NEAR(StepGlint(15.0f, 0.1f), 0.8f, k_Epsilon);
	// Cell 6 is the third across and the second down
	EXPECT_NEAR(GlintUvOffset(6.4f).x, 0.5f, k_Epsilon);
	EXPECT_NEAR(GlintUvOffset(6.4f).y, 0.25f, k_Epsilon);
	EXPECT_NEAR(GlintUvOffset(15.9f).y, 0.75f, k_Epsilon);
}

TEST(MiracleVisuals, AnExtremeGlobeHasARingForEachPowerUp)
{
	EXPECT_EQ(RingCount(-1), 0);
	EXPECT_EQ(RingCount(0), 1);
	EXPECT_EQ(RingCount(1), 2);
	// 60 of 256 of what they are drawn in
	EXPECT_EQ(RingAlpha(150), 35);
	EXPECT_EQ(RingAlpha(255), 59);
}

TEST(GlobeVisualsAgainstOurs, TheGlintCellIsTheBubblesFrame)
{
	// every quarter frame through the sheet: no time passes, so the bubble's clock stays where it is put
	for (int quarter = 0; quarter < 64; ++quarter)
	{
		const float frame = static_cast<float>(quarter) * 0.25f;
		float phase = frame;
		const auto ours = graphics::frame_anim::OneOffFrame(phase, 0.0f);
		const auto his = GlintUvOffset(frame);
		EXPECT_EQ(phase, frame);
		EXPECT_EQ(his.x, ours.x) << "frame " << frame;
		EXPECT_EQ(his.y, ours.y) << "frame " << frame;
	}
}

TEST(GlobeVisualsAgainstOurs, TheRingsAreTheBandLevels)
{
	for (int powerUp = -2; powerUp <= 4; ++powerUp)
	{
		EXPECT_EQ(RingCount(powerUp), worship::seed_graphic::BandLevels(powerUp)) << "power-up " << powerUp;
	}
}

TEST(GlobeVisualsAgainstOurs, TheRingsAreAsFaintAsTheBands)
{
	for (int alpha = 0; alpha <= 255; ++alpha)
	{
		const auto drawn = static_cast<uint8_t>(alpha);
		EXPECT_EQ(RingAlpha(drawn), worship::seed_graphic::BandAlpha(drawn)) << "alpha " << alpha;
	}
}

TEST(GlobeVisualsAgainstOurs, TheGlobesAlphaIsTheOriginals)
{
	// the globe's tint is the orb's (0x96 in its top byte), multiplied with the white it is drawn in: 149
	EXPECT_EQ((static_cast<uint32_t>(k_GlobeAlpha) << 24u) | 0x00FFFFFFu, 0x96FFFFFFu);
	const auto drawn = static_cast<uint8_t>(argb_colour::Alpha(argb_colour::MultiplyArgbShift8(0xFF000000u, 0x96FFFFFFu)));
	EXPECT_EQ(drawn, 0x95);
	// the bands round an orb take that 149: 34, as ours are drawn
	EXPECT_EQ(worship::seed_graphic::BandAlpha(drawn), 34);
	EXPECT_EQ(RingAlpha(drawn), 34);
	// a draw that took the tint itself as the globe's alpha would make the rings 35: ours is kept
	EXPECT_EQ(RingAlpha(k_GlobeAlpha), 35);
	EXPECT_NE(RingAlpha(k_GlobeAlpha), worship::seed_graphic::BandAlpha(drawn));
}

TEST(GlobeVisualsAgainstOurs, OurBandsAreTurnedBitForBit)
{
	// Our band turn, pinned to the bit: the comparison with his below allows for a different order of the sums, this
	// does not
	struct Case
	{
		float spin;
		size_t band;
		std::array<std::array<uint32_t, 3>, 3> bits;
	};
	const std::array<Case, 2> cases {{
	    {0.0f,
	     0,
	     {{{0x3EE4F38Eu, 0xBEC8CB64u, 0xBF4DCB99u},
	       {0x3F531F61u, 0xBE2B2FC9u, 0x3F0A5140u},
	       {0xBEB14C14u, 0xBF679205u, 0x3E7EA3CAu}}}},
	    {1.25f,
	     1,
	     {{{0xBF642581u, 0x3E7006E4u, 0x3EC6D71Eu},
	       {0xBE96057Bu, 0x3EB65281u, 0xBF63277Bu},
	       {0xBEB14C14u, 0xBF679205u, 0xBE7EA3CAu}}}},
	}};
	for (const auto& [spin, band, bits] : cases)
	{
		const glm::mat3 ours = worship::seed_graphic::BandRotation(spin, band);
		for (int column = 0; column < 3; ++column)
		{
			for (int row = 0; row < 3; ++row)
			{
				EXPECT_EQ(std::bit_cast<uint32_t>(ours[column][row]), bits.at(column).at(row))
				    << "band " << band << ", spin " << spin << ", [" << column << "][" << row << "]";
			}
		}
	}
}

TEST(GlobeVisualsAgainstOurs, TheRingsAreTurnedAsTheBands)
{
	// the ring's turn is the band's rows as columns: the same matrix, transposed. The second ring and any after it turn
	// as the second band level
	for (int ring = 0; ring < 3; ++ring)
	{
		for (const float spin : k_Spins)
		{
			const glm::mat3 his = RingTurn(ring, spin);
			const glm::mat3 ours = glm::transpose(worship::seed_graphic::BandRotation(spin, static_cast<size_t>(ring)));
			for (int column = 0; column < 3; ++column)
			{
				for (int row = 0; row < 3; ++row)
				{
					EXPECT_NEAR(his[column][row], ours[column][row], 1.0e-6f)
					    << "ring " << ring << ", spin " << spin << ", [" << column << "][" << row << "]";
				}
			}
		}
	}
}

TEST(GlobesQueue, AnEmptyListQueuesNothing)
{
	graphics::zsort::Queue<graphics::ZObject> queue;
	queue.Begin();
	const std::vector<graphics::globes::GlobeDraw> draws;
	graphics::globes::Submit(std::span<const graphics::globes::GlobeDraw>(draws), queue, k_Camera,
	                         [](int i) { return graphics::ZObject {.globe = i}; });
	EXPECT_TRUE(queue.Empty());
}

TEST(GlobesQueue, ADrawIsOneZObjectKeyedAsAModel)
{
	const std::vector<graphics::globes::GlobeDraw> draws {
	    {.kind = graphics::globes::GlobeDrawKind::Bubble, .mesh = 77, .alpha = 0x95, .sortPoint = {3.0f, 12.0f, 4.0f}},
	};
	graphics::zsort::Queue<graphics::ZObject> queue;
	queue.Begin();
	graphics::globes::Submit(std::span<const graphics::globes::GlobeDraw>(draws), queue, k_Camera,
	                         [](int i) { return graphics::ZObject {.globe = i}; });

	const auto drained = queue.Drain();
	ASSERT_EQ(drained.size(), 1u);
	EXPECT_EQ(drained[0].item->globe, 0);
	// a globe's entry is not a model instance: the drain does not draw it as one
	EXPECT_FALSE(drained[0].item->IsModel());
	// the models' key at its sort point: (x^2 + z^2) + y^2 from the camera
	EXPECT_EQ(drained[0].key, 169.0f);
	EXPECT_EQ(drained[0].key, graphics::zsort::Key(draws[0].sortPoint, k_Camera, graphics::zsort::SumOrder::XZY));
}
