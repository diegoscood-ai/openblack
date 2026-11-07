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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/PlayerSparkles.h"

using namespace openblack;
using namespace openblack::player_sparkles;
using openblack::physics::turn::PuffKind;

TEST(PlayerSparkles, OnlyAComputerGodWhoseMindPlaysOrAPersonInTheGameShowsThem)
{
	EXPECT_TRUE(Shows(Played::Computer, true, false));
	EXPECT_FALSE(Shows(Played::Computer, false, true));
	EXPECT_TRUE(Shows(Played::Person, false, true));
	EXPECT_FALSE(Shows(Played::Person, true, false));
	EXPECT_FALSE(Shows(Played::Nobody, true, true));
	EXPECT_FALSE(Shows(Played::Neutral, true, true));
}

TEST(PlayerSparkles, ASparkleIsTheGodsColourAtHalfStrengthChangedByItsTint)
{
	// the game's own sums on the first two players' colours, tints 0 to 6
	constexpr std::array<uint32_t, 7> k_Red {0x80BF5959u, 0x80BF9159u, 0x80879159u, 0x805F1A1Au,
	                                         0x807F2323u, 0x807F2323u, 0x807F2323u};
	constexpr std::array<uint32_t, 7> k_Green {0x8091875Du, 0x8091BF5Du, 0x8059BF5Du, 0x801A5F1Fu,
	                                           0x80237F2Au, 0x80237F2Au, 0x80237F2Au};
	for (uint32_t tint = 0; tint < 7; ++tint)
	{
		EXPECT_EQ(Colour(0xFFFF4646u, tint), k_Red.at(tint)) << "tint " << tint;
		EXPECT_EQ(Colour(0xFF47FF54u, tint), k_Green.at(tint)) << "tint " << tint;
	}
	// the colour's own alpha is not kept
	EXPECT_EQ(Colour(0x00FF4646u, 4), 0x807F2323u);
	EXPECT_EQ(Colour(0xFFFFFFFFu, 0), 0x80BF8787u);
}

TEST(PlayerSparkles, ARollOfNoneReversesTheMotesFrames)
{
	EXPECT_EQ(KindOf(0), PuffKind::MoteReversed);
	EXPECT_EQ(KindOf(1), PuffKind::Mote);
}

TEST(PlayerSparkles, TheSymbolTurnsByTheFramesGameTimeWithinAHalfTurnEachWay)
{
	EXPECT_FLOAT_EQ(TurnSymbol(0.0f, 16), 0.008f);
	EXPECT_FLOAT_EQ(TurnSymbol(1.0f, 0), 1.0f);
	// past a half turn it comes back by a whole one, once
	EXPECT_FLOAT_EQ(TurnSymbol(3.14f, 16), (16.0f * 0.0005f + 3.14f) - 6.2831855f);
	EXPECT_FLOAT_EQ(TurnSymbol(-3.2f, 0), -3.2f + 6.2831855f);
	EXPECT_FLOAT_EQ(TurnSymbol(3.1415926f, 0), 3.1415926f);
}

TEST(PlayerSparkles, TheSymbolIsTheGodsCellOfASheetOfFourToARowAtHalfSizeThree)
{
	EXPECT_EQ(SymbolCell(1), 1);
	EXPECT_EQ(SymbolCell(2), 2);
	EXPECT_EQ(SymbolCell(65), 1);
	EXPECT_EQ(k_SymbolCellsPerRow, 4);
	EXPECT_FLOAT_EQ(k_SymbolHalfSize, 3.0f);
	// drawn black, faintly
	EXPECT_EQ(k_SymbolArgb, 0x30000000u);
	EXPECT_FLOAT_EQ(k_Size, 10.0f);
	EXPECT_EQ(k_Tints, 7);
}

TEST(PlayerSparkles, BeforeItsHandReportsAGodsCameraLooksAtItsCitadelFromAboveIt)
{
	const auto camera = CameraOverCitadel({2509.12f, 31.5f, 1779.93f});
	EXPECT_FLOAT_EQ(camera.x, 2509.12f);
	EXPECT_FLOAT_EQ(camera.y, 51.5f);
	EXPECT_FLOAT_EQ(camera.z, 1799.93f);
}
