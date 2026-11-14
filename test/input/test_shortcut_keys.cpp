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

#include <array>
#include <bit>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Input/BindableActions.h"
#include "Input/PlaceKeys.h"
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
constexpr ShortcutPresses k_Details {
    .temple = false, .realm = false, .villagerNames = false, .help = false, .villagerDetails = true};
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

TEST(ShortcutKeys, TheDetailsKeyTogglesTheDetailsOfTheLivingThingUnderTheHandOrOfEveryVillager)
{
	EXPECT_EQ(std::countr_zero(static_cast<uint64_t>(BindableActionMap::SHOW_VILLAGER_DETAILS)), 28);
	const auto todo = ShortcutActionsFor(k_Details, k_Open);
	EXPECT_TRUE(todo.toggleVillagerDetails);
	EXPECT_FALSE(todo.toggleVillagerNames);
	EXPECT_FALSE(todo.zoom.has_value());
	EXPECT_FALSE(ShortcutActionsFor(k_Names, k_Open).toggleVillagerDetails);
	// a living thing under the hand takes the key; anything else, or nothing, turns every villager's on or off
	EXPECT_TRUE(DetailsKeyTakesTheThing(true, true));
	EXPECT_FALSE(DetailsKeyTakesTheThing(true, false));
	EXPECT_FALSE(DetailsKeyTakesTheThing(false, false));
	// not while a script holds the cinema bars, nor from the key inside the temple; a mouse button bound to it does
	auto gates = k_Open;
	gates.scriptWideScreen = true;
	EXPECT_FALSE(ShortcutActionsFor(k_Details, gates).toggleVillagerDetails);
	gates = k_Open;
	gates.insideTemple = true;
	EXPECT_FALSE(ShortcutActionsFor(k_Details, gates).toggleVillagerDetails);
	EXPECT_TRUE(ShortcutActionsFor(k_Details, gates, k_Details).toggleVillagerDetails);
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

TEST(ShortcutKeys, AMouseButtonBoundToAKeysActionStillActsInsideTheTemple)
{
	auto gates = k_Open;
	gates.insideTemple = true;
	const auto help = ShortcutActionsFor(k_Help, gates, k_Help);
	EXPECT_TRUE(help.askHelp);
	const auto names = ShortcutActionsFor(k_Names, gates, k_Names);
	EXPECT_TRUE(names.toggleVillagerNames);
	const auto realm = ShortcutActionsFor(k_Realm, gates, k_Realm);
	ASSERT_TRUE(realm.zoom.has_value());
	EXPECT_EQ(*realm.zoom, ZoomKey::Realm);

	// Only the mouse button's press acts: the temple key pressed with it on the keyboard does not
	const auto mouseRealm = ShortcutActionsFor(k_All, gates, k_Realm);
	ASSERT_TRUE(mouseRealm.zoom.has_value());
	EXPECT_EQ(*mouseRealm.zoom, ZoomKey::Realm);
	EXPECT_FALSE(mouseRealm.askHelp);
	EXPECT_FALSE(mouseRealm.toggleVillagerNames);

	// The script's cinema bars and the interface interaction still hold a mouse button's press
	auto bars = gates;
	bars.scriptWideScreen = true;
	EXPECT_FALSE(ShortcutActionsFor(k_Help, bars, k_Help).askHelp);
	auto blocked = gates;
	blocked.helpKeyBlocked = true;
	EXPECT_FALSE(ShortcutActionsFor(k_Help, blocked, k_Help).askHelp);

	// Outside the temple a mouse button's press acts as the key's does
	EXPECT_TRUE(ShortcutActionsFor(k_Help, k_Open, k_Help).askHelp);
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

TEST(ShortcutKeys, EveryKeyDownOfAPlaceKeyFliesAHeldKeysRepeatsToo)
{
	// A held Space sends its first key down and the system's repeats: each one asks for a flight
	constexpr std::array k_HeldTemple {BindableActionMap::ZOOM_TO_TEMPLE, BindableActionMap::ZOOM_TO_TEMPLE,
	                                   BindableActionMap::ZOOM_TO_TEMPLE};
	EXPECT_EQ(PlaceKeysInOrder(k_HeldTemple, k_Open, k_None),
	          (std::vector {ZoomKey::Temple, ZoomKey::Temple, ZoomKey::Temple}));
	// Key downs of other actions ask for none
	constexpr std::array k_Others {BindableActionMap::SHOW_VILLAGER_NAMES, BindableActionMap::HELP};
	EXPECT_TRUE(PlaceKeysInOrder(k_Others, k_Open, k_None).empty());
}

TEST(ShortcutKeys, BothPlaceKeysInOneFrameBothFlyInTheOrderTheyCame)
{
	constexpr std::array k_RealmThenTemple {BindableActionMap::ZOOM_TO_REALM, BindableActionMap::ZOOM_TO_TEMPLE};
	EXPECT_EQ(PlaceKeysInOrder(k_RealmThenTemple, k_Open, k_None), (std::vector {ZoomKey::Realm, ZoomKey::Temple}));
	constexpr std::array k_TempleThenRealm {BindableActionMap::ZOOM_TO_TEMPLE, BindableActionMap::ZOOM_TO_REALM};
	EXPECT_EQ(PlaceKeysInOrder(k_TempleThenRealm, k_Open, k_None), (std::vector {ZoomKey::Temple, ZoomKey::Realm}));
}

TEST(ShortcutKeys, AMouseButtonsPlacePressComesAfterTheFramesKeys)
{
	constexpr std::array k_Realm {BindableActionMap::ZOOM_TO_REALM};
	EXPECT_EQ(PlaceKeysInOrder(k_Realm, k_Open, k_Temple), (std::vector {ZoomKey::Realm, ZoomKey::Temple}));
	// Inside the temple only the mouse's press flies
	auto inside = k_Open;
	inside.insideTemple = true;
	EXPECT_EQ(PlaceKeysInOrder(k_Realm, inside, k_Temple), (std::vector {ZoomKey::Temple}));
}

TEST(ShortcutKeys, NoPlaceKeyDownFliesUnderTheCinemaBarsOrWhenTurnedOff)
{
	constexpr std::array k_Both {BindableActionMap::ZOOM_TO_TEMPLE, BindableActionMap::ZOOM_TO_REALM};
	auto bars = k_Open;
	bars.scriptWideScreen = true;
	EXPECT_TRUE(PlaceKeysInOrder(k_Both, bars, k_Temple).empty());
	auto templeOff = k_Open;
	templeOff.templeKeyBlocked = true;
	EXPECT_EQ(PlaceKeysInOrder(k_Both, templeOff, k_None), (std::vector {ZoomKey::Realm}));
}

TEST(ShortcutKeys, TheNamesKeyTogglesOnEveryKeyDownAHeldKeysRepeatsToo)
{
	// N held: its first key down and two of the system's repeats toggle the names three times
	constexpr std::array k_HeldNames {BindableActionMap::SHOW_VILLAGER_NAMES, BindableActionMap::SHOW_VILLAGER_NAMES,
	                                  BindableActionMap::SHOW_VILLAGER_NAMES};
	EXPECT_EQ(NamesKeyToggles(k_HeldNames, k_Open, false), 3);
	constexpr std::array k_TempleDown {BindableActionMap::ZOOM_TO_TEMPLE};
	EXPECT_EQ(NamesKeyToggles(k_TempleDown, k_Open, false), 0);
	// A mouse button bound to it toggles once more
	EXPECT_EQ(NamesKeyToggles(k_HeldNames, k_Open, true), 4);
	// Under the cinema bars nothing; inside the temple only the mouse's
	auto bars = k_Open;
	bars.scriptWideScreen = true;
	EXPECT_EQ(NamesKeyToggles(k_HeldNames, bars, true), 0);
	auto inside = k_Open;
	inside.insideTemple = true;
	EXPECT_EQ(NamesKeyToggles(k_HeldNames, inside, true), 1);
}
