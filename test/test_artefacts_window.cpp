/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "Debug/ArtefactsModel.h"

using namespace openblack;
using namespace openblack::debug::artefacts;

TEST(ArtefactsWindow, anArtefactShowsPlainThenGlowsThenShowsItsOwnersSymbol)
{
	EXPECT_EQ(LookOf(0.5f), "plain");
	EXPECT_EQ(LookOf(1.0f), "plain");
	EXPECT_EQ(LookOf(1.5f), "glows");
	EXPECT_EQ(LookOf(2.5f), "glows, symbol");
}

TEST(ArtefactsWindow, anArtefactBelongsToItsTownThenItsSiteElseNowhere)
{
	const auto town = entt::entity {3};
	const auto site = entt::entity {9};
	EXPECT_EQ(WhereOf({.town = town, .site = site, .inTown = true, .inSite = true}), "town 3");
	EXPECT_EQ(WhereOf({.town = town, .site = site, .inSite = true}), "site 9");
	EXPECT_EQ(WhereOf({.town = town, .site = site}), "none");
}

TEST(ArtefactsWindow, theRowsKeepTheEntitiesOrderAndSayHowEachDances)
{
	const std::vector<ArtefactAt> artefacts {
	    {.entity = entt::entity {7}, .player = PlayerNames::PLAYER_ONE, .value = 2.5f, .dancing = true, .dancers = 4u},
	    {.entity = entt::entity {2}, .value = 0.2f},
	    {.entity = entt::entity {5}, .value = 1.2f, .dancers = 1u},
	};
	const auto rows = Rows(artefacts);
	ASSERT_EQ(rows.size(), 3u);
	EXPECT_EQ(rows[0].entity, entt::entity {2});
	EXPECT_EQ(rows[0].owner, "none");
	EXPECT_EQ(rows[0].dance, "none");
	EXPECT_EQ(rows[1].dance, "waiting, 1");
	EXPECT_EQ(rows[1].look, "glows");
	EXPECT_EQ(rows[2].owner, "0");
	EXPECT_EQ(rows[2].dance, "dancing, 4");
	EXPECT_EQ(rows[2].look, "glows, symbol");
}

TEST(ArtefactsWindow, aDanceSaysHowManyFootpathsWereLaidToIt)
{
	const std::vector<ArtefactAt> artefacts {
	    {.entity = entt::entity {1}, .dancers = 2u, .footpaths = 1},
	    {.entity = entt::entity {2}, .dancing = true, .dancers = 3u, .footpaths = 2},
	    {.entity = entt::entity {3}, .footpaths = 1},
	};
	const auto rows = Rows(artefacts);
	ASSERT_EQ(rows.size(), 3u);
	EXPECT_EQ(rows[0].dance, "waiting, 2, 1 footpath");
	EXPECT_EQ(rows[1].dance, "dancing, 3, 2 footpaths");
	// no dance: nothing to lay a footpath to
	EXPECT_EQ(rows[2].dance, "none");
}
