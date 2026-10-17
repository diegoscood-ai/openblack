/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The one-press keys (Input/ShortcutKeys.h): which of the temple, realm and villager names keys act, and when none
// does; the middle of the island the realm key flies over; a single tap of the temple key while a fight is watched

#include <cstdint>

#include <bit>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Input/BindableActions.h"
#include "Input/ShortcutKeys.h"

using namespace openblack::input;

namespace
{
constexpr ShortcutGates k_Open {.scriptWideScreen = false,
                                .insideTemple = false,
                                .templeKeyBlocked = false,
                                .realmKeyBlocked = false,
                                .helpKeyBlocked = false};
constexpr ShortcutPresses k_None {.temple = false, .realm = false, .villagerNames = false, .help = false};
constexpr ShortcutPresses k_Temple {.temple = true, .realm = false, .villagerNames = false, .help = false};
constexpr ShortcutPresses k_Realm {.temple = false, .realm = true, .villagerNames = false, .help = false};
constexpr ShortcutPresses k_Names {.temple = false, .realm = false, .villagerNames = true, .help = false};
constexpr ShortcutPresses k_Help {.temple = false, .realm = false, .villagerNames = false, .help = true};
constexpr ShortcutPresses k_All {.temple = true, .realm = true, .villagerNames = true, .help = true};
} // namespace

TEST(ShortcutKeys, TheActionsAreTheOptionsScreensPlaces)
{
	// The script's interface interaction asks by these places: the temple key 17, the realm key 19, the names key 27
	EXPECT_EQ(std::countr_zero(static_cast<uint64_t>(BindableActionMap::ZOOM_TO_TEMPLE)), 17);
	EXPECT_EQ(std::countr_zero(static_cast<uint64_t>(BindableActionMap::ZOOM_TO_REALM)), 19);
	EXPECT_EQ(std::countr_zero(static_cast<uint64_t>(BindableActionMap::SHOW_VILLAGER_NAMES)), 27);
}

TEST(ShortcutKeys, NoPressDoesNothing)
{
	const auto todo = ShortcutActionsFor(k_None, k_Open);
	EXPECT_FALSE(todo.toggleVillagerNames);
	EXPECT_FALSE(todo.zoom.has_value());
}

TEST(ShortcutKeys, TheTempleKeyZoomsToTheTemple)
{
	const auto todo = ShortcutActionsFor(k_Temple, k_Open);
	EXPECT_FALSE(todo.toggleVillagerNames);
	ASSERT_TRUE(todo.zoom.has_value());
	EXPECT_EQ(*todo.zoom, ZoomKey::Temple);
}

TEST(ShortcutKeys, TheRealmKeyZoomsOverTheRealm)
{
	const auto todo = ShortcutActionsFor(k_Realm, k_Open);
	EXPECT_FALSE(todo.toggleVillagerNames);
	ASSERT_TRUE(todo.zoom.has_value());
	EXPECT_EQ(*todo.zoom, ZoomKey::Realm);
}

TEST(ShortcutKeys, TheNamesKeyTogglesTheNames)
{
	const auto todo = ShortcutActionsFor(k_Names, k_Open);
	EXPECT_TRUE(todo.toggleVillagerNames);
	EXPECT_FALSE(todo.zoom.has_value());
}

TEST(ShortcutKeys, TheHelpKeyAsksTheAdvisors)
{
	const auto todo = ShortcutActionsFor(k_Help, k_Open);
	EXPECT_TRUE(todo.askHelp);
	EXPECT_FALSE(todo.toggleVillagerNames);
	EXPECT_FALSE(todo.zoom.has_value());
	EXPECT_FALSE(ShortcutActionsFor(k_Names, k_Open).askHelp);
	// The script's interface interaction can turn it off; the other keys still act
	auto gates = k_Open;
	gates.helpKeyBlocked = true;
	const auto blocked = ShortcutActionsFor(k_All, gates);
	EXPECT_FALSE(blocked.askHelp);
	EXPECT_TRUE(blocked.toggleVillagerNames);
	// It is the options screen's first action
	EXPECT_EQ(std::countr_zero(static_cast<uint64_t>(BindableActionMap::HELP)), 0);
}

TEST(ShortcutKeys, TheTempleKeyGoesBeforeTheRealmKey)
{
	const auto todo = ShortcutActionsFor(k_All, k_Open);
	EXPECT_TRUE(todo.toggleVillagerNames);
	ASSERT_TRUE(todo.zoom.has_value());
	EXPECT_EQ(*todo.zoom, ZoomKey::Temple);
}

TEST(ShortcutKeys, NoKeyActsWhileAScriptHoldsTheCinemaBars)
{
	auto gates = k_Open;
	gates.scriptWideScreen = true;
	for (const auto presses : {k_Temple, k_Realm, k_Names, k_Help, k_All})
	{
		const auto todo = ShortcutActionsFor(presses, gates);
		EXPECT_FALSE(todo.toggleVillagerNames);
		EXPECT_FALSE(todo.zoom.has_value());
		EXPECT_FALSE(todo.askHelp);
	}
}

TEST(ShortcutKeys, NoKeyActsInsideTheTemple)
{
	auto gates = k_Open;
	gates.insideTemple = true;
	for (const auto presses : {k_Temple, k_Realm, k_Names, k_Help, k_All})
	{
		const auto todo = ShortcutActionsFor(presses, gates);
		EXPECT_FALSE(todo.toggleVillagerNames);
		EXPECT_FALSE(todo.zoom.has_value());
		EXPECT_FALSE(todo.askHelp);
	}
}

TEST(ShortcutKeys, TheInterfaceInteractionBlocksThePlaceKeysButNotTheNames)
{
	auto gates = k_Open;
	gates.templeKeyBlocked = true;
	gates.realmKeyBlocked = true;
	const auto todo = ShortcutActionsFor(k_All, gates);
	EXPECT_TRUE(todo.toggleVillagerNames);
	EXPECT_FALSE(todo.zoom.has_value());

	// A blocked temple key leaves the realm key pressed with it
	gates.realmKeyBlocked = false;
	const auto realm = ShortcutActionsFor(k_All, gates);
	ASSERT_TRUE(realm.zoom.has_value());
	EXPECT_EQ(*realm.zoom, ZoomKey::Realm);
}

TEST(ShortcutKeys, TheRealmIsHalfwayBetweenTheFirstAndLastBlocks)
{
	// Blocks from 320 to 1440 along x and 160 to 2400 along z: the extent runs on to 1600 and 2560
	const auto middle = RealmMiddle({320.0f, 160.0f}, {1600.0f, 2560.0f}, 160.0f);
	EXPECT_FLOAT_EQ(middle.x, 880.0f);
	EXPECT_FLOAT_EQ(middle.y, 1280.0f);
	// One block alone: its corner
	const auto one = RealmMiddle({800.0f, 960.0f}, {960.0f, 1120.0f}, 160.0f);
	EXPECT_FLOAT_EQ(one.x, 800.0f);
	EXPECT_FLOAT_EQ(one.y, 960.0f);
}

TEST(ShortcutKeys, ASingleTapDoesNotMoveTheCameraWatchingAFight)
{
	EXPECT_TRUE(TempleKeyMovesCamera(false, false));
	EXPECT_TRUE(TempleKeyMovesCamera(true, false));
	EXPECT_FALSE(TempleKeyMovesCamera(false, true));
	// The double tap still flies to the temple
	EXPECT_TRUE(TempleKeyMovesCamera(true, true));
}
