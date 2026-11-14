/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple room keys (input::PressedTempleRoom): F4 to F9 by default, each to its room, the first pressed in their
// order. docs/wiki/project/progress/interface/key_bindings.md

#include <set>
#include <vector>

#include <gtest/gtest.h>

#include "Input/TempleRoomKeys.h"

using namespace openblack;
using openblack::input::BindableActionMap;

TEST(TempleRoomKeys, EachKeyTakesThePlayerToItsRoom)
{
	const std::vector<std::pair<BindableActionMap, TempleRoom>> expected {
	    {BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, TempleRoom::Main},
	    {BindableActionMap::ZOOM_TO_CREATURE_ROOM, TempleRoom::CreatureCave},
	    {BindableActionMap::ZOOM_TO_CHALLENGE_ROOM, TempleRoom::Challenge},
	    {BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM, TempleRoom::SaveGame},
	    {BindableActionMap::ZOOM_TO_OPTIONS_ROOM, TempleRoom::Options},
	    {BindableActionMap::ZOOM_TO_LIBRARY, TempleRoom::Credits},
	};
	for (const auto& [key, room] : expected)
	{
		const auto pressed = input::PressedTempleRoom([key](BindableActionMap action) { return action == key; });
		ASSERT_TRUE(pressed.has_value());
		EXPECT_EQ(*pressed, room);
	}
}

// Inside the temple each key down goes to its room in the order the keys came, a held key's repeats too, and a key down
// bound to nothing of the temple's goes nowhere
TEST(TempleRoomKeys, InsideTheTempleEachKeyDownGoesToItsRoomInTheOrderTheyCame)
{
	const std::vector<BindableActionMap> downs {BindableActionMap::ZOOM_TO_OPTIONS_ROOM, BindableActionMap::NONE,
	                                            BindableActionMap::ZOOM_TO_CREATURE_ROOM,
	                                            BindableActionMap::ZOOM_TO_CREATURE_ROOM};
	EXPECT_EQ(input::TempleRoomsInOrder(downs),
	          (std::vector<TempleRoom> {TempleRoom::Options, TempleRoom::CreatureCave, TempleRoom::CreatureCave}));
	EXPECT_TRUE(input::TempleRoomsInOrder({}).empty());
}

TEST(TempleRoomKeys, NoKeyPressedGoesNowhere)
{
	EXPECT_FALSE(input::PressedTempleRoom([](BindableActionMap) { return false; }).has_value());
}

TEST(TempleRoomKeys, TheFirstKeyPressedInTheirOrderWinsAndTheRestAreNotAsked)
{
	// the creature's room and the options both pressed: the creature's room, read first
	const std::set<BindableActionMap> down {BindableActionMap::ZOOM_TO_OPTIONS_ROOM, BindableActionMap::ZOOM_TO_CREATURE_ROOM};
	std::vector<BindableActionMap> asked;
	const auto pressed = input::PressedTempleRoom([&](BindableActionMap action) {
		asked.push_back(action);
		return down.contains(action);
	});
	ASSERT_TRUE(pressed.has_value());
	EXPECT_EQ(*pressed, TempleRoom::CreatureCave);
	EXPECT_EQ(asked, (std::vector<BindableActionMap> {BindableActionMap::ZOOM_TO_INSIDE_TEMPLE,
	                                                  BindableActionMap::ZOOM_TO_CREATURE_ROOM}));
}

// Outside the temple the keys do nothing while a script holds the wide screen, as the game's every bindable key, and the
// keys are not even read; without it the first pressed in their order goes, as PressedTempleRoom.
// docs/wiki/interface/screen-and-readouts.md
TEST(TempleRoomKeys, OutsideTheTempleNoKeyActsWhileAScriptHoldsTheWideScreen)
{
	std::vector<BindableActionMap> asked;
	const auto pressed = [&asked](BindableActionMap action) {
		asked.push_back(action);
		return action == BindableActionMap::ZOOM_TO_OPTIONS_ROOM;
	};
	EXPECT_FALSE(input::TempleRoomToEnter(true, pressed).has_value());
	EXPECT_TRUE(asked.empty());
	const auto room = input::TempleRoomToEnter(false, pressed);
	ASSERT_TRUE(room.has_value());
	EXPECT_EQ(*room, TempleRoom::Options);
}
