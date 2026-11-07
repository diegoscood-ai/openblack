/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple and realm keys (Camera/ZoomToPlaces.h): a single tap of the temple key, its double tap to the temple,
// the realm key, flying back when pressed again while still looking there (which ends a tap), the flights' rises, the
// pitches' floats, a new world camera starting afresh and the game clock the taps are timed by; and a game's start
// camera at the temple (Camera/StartCamera.h). The views' numbers are the original's, from its own camera functions run
// on the same views.

#include <cmath>
#include <cstdint>

#include <bit>
#include <chrono>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCamera.h"
#include "Camera/StartCamera.h"
#include "Camera/ZoomToPlaces.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::zoom_to;
using namespace std::chrono_literals;

namespace
{
/// Looking north-west down at the land
const CameraView k_Start {.origin = {1123.5f, 160.25f, 905.75f}, .focus = {1010.0f, 95.5f, 1040.25f}};
constexpr glm::vec2 k_Temple {2000.0f, 1500.0f};
constexpr glm::vec2 k_Realm {2560.0f, 2560.0f};
/// How far ahead of k_Start the camera finds the land
constexpr float k_Ahead = 213.375f;

/// The land: 40 at the temple, 20 over the realm and 30 everywhere else
float LandAt(glm::vec2 place)
{
	if (place == k_Temple)
	{
		return 40.0f;
	}
	return place == k_Realm ? 20.0f : 30.0f;
}

void ExpectBits(glm::vec3 actual, uint32_t x, uint32_t y, uint32_t z)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.x), x);
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.y), y);
	EXPECT_EQ(std::bit_cast<uint32_t>(actual.z), z);
}

/// The view a press flew to looks at the place's land plus the focus height, from the distance and pitch, keeping the
/// heading
void ExpectViewOver(const CameraView& view, glm::vec2 place, float heading, float distance, float pitch)
{
	const auto focus = glm::vec3(place.x, LandAt(place) + k_FocusAboveGround, place.y);
	EXPECT_EQ(view.focus, focus);
	EXPECT_EQ(view.origin, script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, pitch));
}

uint32_t g_Now = 0;

uint32_t FakeTicks()
{
	return g_Now;
}
} // namespace

TEST(ZoomToPlaces, TheHeadingIsTheCamerasFromItsOriginAndFocus)
{
	// The original's heading of k_Start
	EXPECT_EQ(std::bit_cast<uint32_t>(HeadingOf(k_Start)), 0x401C33F2u);
	// Straight down has no heading
	EXPECT_EQ(HeadingOf({.origin = {3.0f, 50.0f, 4.0f}, .focus = {3.0f, 0.0f, 4.0f}}), 0.0f);
}

TEST(ZoomToPlaces, ThePointAheadIsAlongTheLineOfSight)
{
	// As the original works it: the unit direction, its squares added z first, times the distance, plus the origin
	ExpectBits(AlongView(k_Start, k_Ahead), 0x4478969Au, 0x42AD25C0u, 0x44845957u);
	EXPECT_EQ(AlongView({.origin = {0.0f, 10.0f, 0.0f}, .focus = {3.0f, 10.0f, 4.0f}}, 10.0f), glm::vec3(6.0f, 10.0f, 8.0f));
	// A camera that looks at itself stays where it is
	EXPECT_EQ(AlongView({.origin = {5.0f, 6.0f, 7.0f}, .focus = {5.0f, 6.0f, 7.0f}}, 10.0f), glm::vec3(5.0f, 6.0f, 7.0f));
}

TEST(ZoomToPlaces, ASingleTapViewsTheLandAheadFromAPleasingHeight)
{
	ZoomToPlaces zoom;
	const auto tap = zoom.PressTemple(10000ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(tap.has_value());
	// The original's view: over the point ahead, at the land's height there plus 3, from 50 at the tap's pitch
	ExpectBits(tap->view.focus, 0x4478969Au, 0x42040000u, 0x44845957u);
	ExpectBits(tap->view.origin, 0x447F91DDu, 0x4267FFFFu, 0x4480365Eu);
	// It does not rise, and is no flight to a place
	EXPECT_EQ(tap->rise, k_TapRise);
	EXPECT_FALSE(tap->toPlace);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, ASingleTapWithoutTheDistanceAheadStartsNoFlightButCounts)
{
	ZoomToPlaces zoom;
	EXPECT_FALSE(zoom.PressTemple(10000ms, k_Start, std::nullopt, k_Temple, k_Realm, LandAt).has_value());
	// The next tap is a double tap
	EXPECT_TRUE(zoom.IsDoubleTap(10200ms));
	const auto toTemple = zoom.PressTemple(10200ms, k_Start, std::nullopt, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(toTemple.has_value());
	ExpectViewOver(toTemple->view, k_Temple, HeadingOf(k_Start), k_TempleDistance, k_TemplePitch);
}

TEST(ZoomToPlaces, ADoubleTapFliesToTheTempleAndAnotherFliesBack)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	const auto toTemple = zoom.PressTemple(10000ms + k_DoubleTapTime, k_Start, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(toTemple.has_value());
	// The original's view of the temple from k_Start's heading
	ExpectBits(toTemple->view.focus, 0x44FA0000u, 0x422C0000u, 0x44BB8000u);
	ExpectBits(toTemple->view.origin, 0x4500B48Au, 0x4306EC82u, 0x44B2B7EAu);
	EXPECT_EQ(std::bit_cast<uint32_t>(toTemple->rise), 0x3ECCCCCDu);
	EXPECT_TRUE(toTemple->toPlace);
	EXPECT_TRUE(zoom.IsZoomedTo());

	// Still looking at the temple, a single tap only puts the view over the land ahead, and the double tap flies back to
	// the view the camera had when it was flown there
	const auto over = zoom.PressTemple(20000ms, toTemple->view, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(over.has_value());
	EXPECT_FALSE(over->toPlace);
	const CameraView nearTemple {.origin = toTemple->view.origin, .focus = {2000.0f + 60.0f, 0.0f, 1500.0f + 70.0f}};
	const auto back = zoom.PressTemple(20100ms, nearTemple, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->view.origin, k_Start.origin);
	EXPECT_EQ(back->view.focus, k_Start.focus);
	EXPECT_EQ(back->rise, k_PlaceRise);
	EXPECT_TRUE(back->toPlace);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, TapsFurtherApartThanTheDoubleTapTimeAreSingle)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	const auto late = zoom.PressTemple(10000ms + k_DoubleTapTime + 1ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(late.has_value());
	EXPECT_FALSE(late->toPlace);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, WithoutATempleTheDoubleTapFliesOverTheRealm)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_Ahead, std::nullopt, k_Realm, LandAt).has_value());
	const auto view = zoom.PressTemple(10200ms, k_Start, k_Ahead, std::nullopt, k_Realm, LandAt);
	ASSERT_TRUE(view.has_value());
	ExpectViewOver(view->view, k_Realm, HeadingOf(k_Start), k_RealmDistance, k_RealmPitch);
	EXPECT_EQ(view->rise, k_PlaceRise);
	EXPECT_TRUE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, TheRealmKeyFliesOverTheRealmAndBack)
{
	ZoomToPlaces zoom;
	const auto toRealm = zoom.PressRealm(k_Start, k_Realm, LandAt);
	// The original's view over the realm from k_Start's heading
	ExpectBits(toRealm.view.focus, 0x45200000u, 0x41B80000u, 0x45200000u);
	ExpectBits(toRealm.view.origin, 0x4571ED3Fu, 0x445839D4u, 0x447BA8F0u);
	EXPECT_EQ(toRealm.rise, k_PlaceRise);
	EXPECT_TRUE(zoom.IsZoomedTo());

	const auto back = zoom.PressRealm(toRealm.view, k_Realm, LandAt);
	EXPECT_EQ(back.view.origin, k_Start.origin);
	EXPECT_EQ(back.view.focus, k_Start.focus);
	EXPECT_EQ(back.rise, k_PlaceRise);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, LookingExactlyTheReturnRadiusAwayStillFliesBack)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressRealm(k_Start, k_Realm, LandAt).toPlace);
	// 60 across and 80 along is the radius itself: the key flies back
	const CameraView atRadius {.origin = {0.0f, 500.0f, 0.0f}, .focus = {k_Realm.x + 60.0f, 0.0f, k_Realm.y + 80.0f}};
	const auto back = zoom.PressRealm(atRadius, k_Realm, LandAt);
	EXPECT_EQ(back.view.focus, k_Start.focus);
}

TEST(ZoomToPlaces, LookingAwayTheRealmKeyFliesThereAgain)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressRealm(k_Start, k_Realm, LandAt).toPlace);
	// The camera has been moved away, further than the return radius: the key flies to the realm again, and back from
	// there to this view
	const CameraView away {.origin = {k_Realm.x, 70.0f, k_Realm.y + k_ReturnRadius},
	                       .focus = {k_Realm.x, 20.0f, k_Realm.y + k_ReturnRadius + 1.0f}};
	const auto again = zoom.PressRealm(away, k_Realm, LandAt);
	ExpectViewOver(again.view, k_Realm, HeadingOf(away), k_RealmDistance, k_RealmPitch);
	const auto back = zoom.PressRealm(again.view, k_Realm, LandAt);
	EXPECT_EQ(back.view.focus, away.focus);
}

TEST(ZoomToPlaces, ThePitchesAndRisesAreTheGamesFloats)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TemplePitch), 0x3F490FD8u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_TapPitch), 0x3F060A90u);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_RealmPitch), 0x3EC90FDBu);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_PlaceRise), 0x3ECCCCCDu);
	EXPECT_EQ(k_TapRise, 0.0f);
}

TEST(ZoomToPlaces, FlyingBackFromTheRealmEndsTheTap)
{
	ZoomToPlaces zoom;
	const auto toRealm = zoom.PressRealm(k_Start, k_Realm, LandAt);
	ASSERT_TRUE(zoom.PressTemple(10000ms, toRealm.view, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	EXPECT_TRUE(zoom.IsDoubleTap(10100ms));
	// Flown back, the next press of the temple key is a single tap however soon it comes
	const auto back = zoom.PressRealm(toRealm.view, k_Realm, LandAt);
	EXPECT_FALSE(zoom.IsDoubleTap(10200ms));
	const auto tap = zoom.PressTemple(10200ms, back.view, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(tap.has_value());
	EXPECT_FALSE(tap->toPlace);
}

TEST(ZoomToPlaces, ANewWorldCameraForgetsTheTapAndTheWayBack)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressRealm(k_Start, k_Realm, LandAt).toPlace);
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	zoom.Restart();
	EXPECT_FALSE(zoom.IsZoomedTo());
	EXPECT_FALSE(zoom.IsDoubleTap(10100ms));
	// Still looking over the realm, the realm key flies there again rather than back
	const auto again = zoom.PressRealm({.origin = k_Start.origin, .focus = {k_Realm.x, 20.0f, k_Realm.y}}, k_Realm, LandAt);
	EXPECT_EQ(again.view.focus, glm::vec3(k_Realm.x, 23.0f, k_Realm.y));
}

TEST(ZoomToPlaces, TapsAreTimedByTheEngineClockOfTheFrame)
{
	g_Now = 7000;
	game_clock::SetTickSource(&FakeTicks);
	game_clock::Reset();
	game_clock::StartEngineTimer();
	g_Now += 1000;
	game_clock::UpdateRealClock();
	const auto first = TapTime();
	// Within a frame the time stays that of its start
	g_Now += 300;
	EXPECT_EQ(TapTime(), first);
	// It runs on in pause
	game_clock::Pause(true);
	game_clock::UpdateRealClock();
	EXPECT_EQ(TapTime() - first, 300ms);
	game_clock::Pause(false);
	game_clock::SetTickSource(nullptr);
	game_clock::Reset();
}

TEST(ZoomToPlaces, AGameStartFliesToTheTempleFromAFreshCamera)
{
	ZoomToPlaces zoom;
	// A tap and a flight of the last game's camera are forgotten
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	const auto start = zoom.StartAtTemple(k_Start, k_Temple, LandAt);
	ExpectBits(start.view.origin, 0x4500B48Au, 0x4306EC82u, 0x44B2B7EAu);
	EXPECT_EQ(start.rise, k_PlaceRise);
	EXPECT_TRUE(start.toPlace);
	EXPECT_TRUE(zoom.IsZoomedTo());
	EXPECT_FALSE(zoom.IsDoubleTap(10000ms));
	// Still looking at the temple, the temple key's double tap flies back to the view the start began from
	ASSERT_TRUE(zoom.PressTemple(20000ms, start.view, k_Ahead, k_Temple, k_Realm, LandAt).has_value());
	const auto back = zoom.PressTemple(20100ms, start.view, k_Ahead, k_Temple, k_Realm, LandAt);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->view.origin, k_Start.origin);
	EXPECT_EQ(back->view.focus, k_Start.focus);
}

TEST(StartCamera, AGameStartsAtTheTempleOnlyWithTheScriptsPlaceAtTheOrigin)
{
	EXPECT_TRUE(start_camera::StartsAtTemple({}));
	EXPECT_TRUE(start_camera::StartsAtTemple({.scriptPlace = {.x = 0, .z = 0, .altitude = 0.0f}}));
	EXPECT_FALSE(start_camera::StartsAtTemple({.scriptPlace = {.x = 1, .z = 0}}));
	EXPECT_FALSE(start_camera::StartsAtTemple({.scriptPlace = {.x = 0, .z = 1}}));
	EXPECT_FALSE(start_camera::StartsAtTemple({.scriptPlace = {.x = 0, .z = 0, .altitude = 1.0f}}));
	// A network game always starts there
	EXPECT_TRUE(start_camera::StartsAtTemple({.networkGame = true, .scriptPlace = {.x = 1, .z = 1}}));
}

TEST(StartCamera, TheStartCameraLooksFromTheScriptPlacesDistance)
{
	const glm::vec3 focus {1774.0f, 31.0f, 3190.0f};
	EXPECT_NEAR(glm::distance(start_camera::OriginOver(focus), focus), start_camera::k_OriginDistance, 1e-3f);
}
