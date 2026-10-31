/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/Graphics/MeshStatus.h: the submeshes an object's status shows (a graveyard's graves). Synthetic numbers.

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Graphics/MeshStatus.h"

using openblack::graphics::mesh_status::Drawn;
using openblack::graphics::mesh_status::ShownBy;

TEST(MeshStatus, anObjectShowsEveryPartUpToItsStatus)
{
	EXPECT_TRUE(ShownBy(0, 0));
	EXPECT_FALSE(ShownBy(1, 0));
	EXPECT_TRUE(ShownBy(3, 3));
	EXPECT_FALSE(ShownBy(4, 3));
	// the scaffold part (status 10) is above any status an object keeps (3 bits: 0 to 7)
	EXPECT_FALSE(ShownBy(10, 7));
}

TEST(MeshStatus, theCommonDrawTakesOnlyStatusZero)
{
	EXPECT_TRUE(Drawn(0, std::nullopt));
	EXPECT_FALSE(Drawn(1, std::nullopt));
	EXPECT_FALSE(Drawn(10, std::nullopt));
}

TEST(MeshStatus, theStatusDrawTakesTheAddedPartsOnly)
{
	EXPECT_FALSE(Drawn(0, std::optional<uint8_t> {2}));
	EXPECT_TRUE(Drawn(1, std::optional<uint8_t> {2}));
	EXPECT_TRUE(Drawn(2, std::optional<uint8_t> {2}));
	EXPECT_FALSE(Drawn(3, std::optional<uint8_t> {2}));
}

TEST(MeshStatus, theTwoDrawsTogetherShowWhatTheObjectShows)
{
	for (uint32_t object = 1; object <= 7; ++object)
	{
		for (uint32_t part = 0; part <= 63; ++part)
		{
			const bool both = Drawn(part, std::nullopt) || Drawn(part, static_cast<uint8_t>(object));
			EXPECT_EQ(both, ShownBy(part, object)) << "object " << object << " part " << part;
		}
	}
}
