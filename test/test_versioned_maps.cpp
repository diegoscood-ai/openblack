/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "3D/VersionedMaps.h"

using openblack::VersionedMaps;

// The land's first map is version 0; each change keeps the map it replaces under the version it had, and the current
// map answers for its own version and every later one
TEST(VersionedMaps, AChangeKeepsTheMapItReplacesUnderItsVersion)
{
	VersionedMaps<std::string> maps;
	maps.Reset(std::make_unique<std::string>("first"));
	EXPECT_EQ(maps.Version(), 0u);
	EXPECT_EQ(maps.Current(), "first");
	EXPECT_EQ(maps.At(0), "first");

	maps.Replace(std::make_unique<std::string>("second"));
	maps.Replace(std::make_unique<std::string>("third"));
	EXPECT_EQ(maps.Version(), 2u);
	EXPECT_EQ(maps.Current(), "third");
	EXPECT_EQ(maps.At(0), "first");
	EXPECT_EQ(maps.At(1), "second");
	EXPECT_EQ(maps.At(2), "third");
	EXPECT_EQ(maps.At(7), "third");

	// a new land starts again with nothing kept
	maps.Reset(std::make_unique<std::string>("other land"));
	EXPECT_EQ(maps.Version(), 0u);
	EXPECT_EQ(maps.At(1), "other land");
}
