/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/MissionaryBoat.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// One block of 17 x 17 cells: (0, 0) bright with a reddish colour, (0, 2) dark with a bluish one, the rest dark
land_light::Cells TwoLightCells()
{
	land_light::Cells cells;
	cells.firstCell = glm::ivec2(0);
	cells.size = glm::ivec2(17);
	cells.cells.assign(17 * 17, 0x10000000u);
	cells.covered.assign(17 * 17, 1);
	cells.blocks[0] = 1;
	cells.cells[0] = 0xF0400000u;      // (x 0, z 0): luminosity 0xF0
	cells.cells[2 * 17] = 0x10000040u; // (x 0, z 2): luminosity 0x10
	return cells;
}

/// A light table with a distinct light at every luminosity, from a synthetic palette
LandLightTable Table()
{
	std::vector<uint8_t> palette(4096);
	for (size_t i = 0; i < palette.size(); ++i)
	{
		palette[i] = static_cast<uint8_t>((i * 73 + (i >> 7) * 29 + 11) & 255);
	}
	LandLightTable table;
	table.Build(LandLightPalette(palette), 0.0f, 0.0f, 0.0f);
	return table;
}
} // namespace

// The boat's people and things copy the colour the hull takes at its own place: the land light where the hull stands,
// not where they stand, and none of its specular (they keep the 0 they are made with)
TEST(MissionaryBoatDeck, TheDeckTakesTheHullsColour)
{
	const auto cells = TwoLightCells();
	const auto table = Table();
	ASSERT_NE(table.GetRaw(0xF0) | 0xFF000000u, table.GetRaw(0x10) | 0xFF000000u);

	const glm::vec3 hull(0.0f, 4.0f, 0.0f);
	const glm::vec3 onDeck(0.0f, 14.0f, 20.0f); // a passenger over the dark cell
	const auto hullLight = land_light::At(cells, table, glm::vec2(hull.x, hull.z));
	ASSERT_EQ(hullLight.diffuse, table.GetRaw(0xF0) | 0xFF000000u);
	ASSERT_NE(hullLight.specular, 0u);
	ASSERT_NE(land_light::At(cells, table, glm::vec2(onDeck.x, onDeck.z)).diffuse, hullLight.diffuse);

	const auto colours = missionary_boat::PeopleColoursAt(cells, table, {hull, false});
	EXPECT_EQ(colours.colour, hullLight.diffuse);
	EXPECT_EQ(colours.specular, 0u);
	// off the map the hull has the full light, and so has its deck
	const auto offTheMap = missionary_boat::PeopleColoursAt(cells, table, {glm::vec3(-30.0f, 0.0f, 5.0f), false});
	EXPECT_EQ(offTheMap.colour, land_light::At(cells, table, glm::vec2(-30.0f, 5.0f)).diffuse);
	EXPECT_EQ(offTheMap.specular, 0u);
}

// A sailor launching the boat takes the land light of its own place and no specular. It asks for the colour and the
// specular into the same field, the specular last only where the land has no light: there it is drawn black
TEST(MissionaryBoatDeck, TheSailorsAtTheLaunchTakeTheLightOfTheirPlace)
{
	const auto cells = TwoLightCells();
	const auto table = Table();
	const glm::vec3 sailor(0.0f, 2.0f, 20.0f);
	const auto own = missionary_boat::PeopleColoursAt(cells, table, {sailor, true});
	EXPECT_EQ(own.colour, table.GetRaw(0x10) | 0xFF000000u);
	EXPECT_EQ(own.specular, 0u);

	ASSERT_FALSE(land_light::OnTheMap(cells, glm::vec2(-30.0f, 5.0f)));
	const auto offTheMap = missionary_boat::PeopleColoursAt(cells, table, {glm::vec3(-30.0f, 0.0f, 5.0f), true});
	EXPECT_EQ(offTheMap.colour, 0xFF000000u);
	EXPECT_EQ(offTheMap.specular, 0u);
	// in a block the land does not have
	ASSERT_FALSE(land_light::OnTheMap(cells, glm::vec2(200.0f, 5.0f)));
	EXPECT_EQ(missionary_boat::PeopleColoursAt(cells, table, {glm::vec3(200.0f, 0.0f, 5.0f), true}).colour, 0xFF000000u);
}
