/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple and realm keys (Camera/ZoomToPlaces.h): a single tap of the temple key, its double tap to the temple,
// the realm key, flying back when pressed again while still looking there, and the game clock the taps are timed by

#include <cmath>
#include <cstdint>

#include <chrono>
#include <numbers>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/ZoomToPlaces.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::zoom_to;
using namespace std::chrono_literals;

namespace
{
constexpr float k_Epsilon = 1e-3f;

/// Looking north (+z) at the middle of the land from 100 back and 50 up
const CameraView k_Start {.origin = {1000.0f, 150.0f, 900.0f}, .focus = {1000.0f, 100.0f, 1000.0f}};
constexpr glm::vec3 k_LookedAtGround {1000.0f, 100.0f, 1000.0f};
constexpr glm::vec3 k_Temple {2000.0f, 40.0f, 1500.0f};
constexpr glm::vec3 k_Realm {2560.0f, 20.0f, 2560.0f};

void ExpectNear(glm::vec3 actual, glm::vec3 expected)
{
	EXPECT_NEAR(actual.x, expected.x, k_Epsilon);
	EXPECT_NEAR(actual.y, expected.y, k_Epsilon);
	EXPECT_NEAR(actual.z, expected.z, k_Epsilon);
}

/// The view a press flew to looks at the ground plus the focus height, from the distance and pitch, keeping the heading
void ExpectViewOver(const CameraView& view, glm::vec3 ground, float heading, float distance, float pitch)
{
	const auto focus = ground + glm::vec3(0.0f, k_FocusAboveGround, 0.0f);
	ExpectNear(view.focus, focus);
	EXPECT_NEAR(glm::distance(view.origin, view.focus), distance, k_Epsilon * distance);
	EXPECT_NEAR(view.origin.y - view.focus.y, distance * std::sin(pitch), k_Epsilon * distance);
	EXPECT_NEAR(HeadingOf(view), heading, 1e-5f);
}

uint32_t g_Now = 0;

uint32_t FakeTicks()
{
	return g_Now;
}
} // namespace

TEST(ZoomToPlaces, HeadingIsTheAngleRoundTheVerticalTowardsTheFocus)
{
	EXPECT_FLOAT_EQ(HeadingOf(k_Start), 0.0f);
	EXPECT_FLOAT_EQ(HeadingOf({.origin = {0.0f, 0.0f, 0.0f}, .focus = {10.0f, 5.0f, 0.0f}}), std::numbers::pi_v<float> / 2.0f);
	// Straight down has no heading
	EXPECT_EQ(HeadingOf({.origin = {3.0f, 50.0f, 4.0f}, .focus = {3.0f, 0.0f, 4.0f}}), 0.0f);
}

TEST(ZoomToPlaces, OriginAroundIsBehindAndAboveTheFocus)
{
	const glm::vec3 focus {100.0f, 10.0f, 200.0f};
	const auto pitch = std::numbers::pi_v<float> / 6.0f;
	const auto origin = OriginAround(focus, 0.0f, 50.0f, pitch);
	ExpectNear(origin, focus + glm::vec3(0.0f, 50.0f * std::sin(pitch), -50.0f * std::cos(pitch)));
	EXPECT_NEAR(HeadingOf({.origin = origin, .focus = focus}), 0.0f, 1e-6f);
	EXPECT_NEAR(HeadingOf({.origin = OriginAround(focus, 1.0f, 80.0f, pitch), .focus = focus}), 1.0f, 1e-5f);
}

TEST(ZoomToPlaces, ASingleTapViewsWhatIsLookedAtFromAPleasingHeight)
{
	ZoomToPlaces zoom;
	const auto view = zoom.PressTemple(10000ms, k_Start, k_LookedAtGround, k_Temple, k_Realm);
	ASSERT_TRUE(view.has_value());
	ExpectViewOver(*view, k_LookedAtGround, HeadingOf(k_Start), k_TapDistance, k_TapPitch);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, ADoubleTapFliesToTheTempleAndAnotherFliesBack)
{
	ZoomToPlaces zoom;
	const auto tap = zoom.PressTemple(10000ms, k_Start, k_LookedAtGround, k_Temple, k_Realm);
	ASSERT_TRUE(tap.has_value());
	const auto toTemple = zoom.PressTemple(10000ms + k_DoubleTapTime, *tap, k_LookedAtGround, k_Temple, k_Realm);
	ASSERT_TRUE(toTemple.has_value());
	ExpectViewOver(*toTemple, k_Temple, HeadingOf(*tap), k_TempleDistance, k_TemplePitch);
	EXPECT_TRUE(zoom.IsZoomedTo());

	// Still looking at the temple, a single tap only puts the view over it, and the double tap flies back to the view
	// the camera had when it was flown there
	const auto overTemple = zoom.PressTemple(20000ms, *toTemple, k_Temple, k_Temple, k_Realm);
	ASSERT_TRUE(overTemple.has_value());
	ExpectViewOver(*overTemple, k_Temple, HeadingOf(*toTemple), k_TapDistance, k_TapPitch);
	const auto back = zoom.PressTemple(20100ms, *overTemple, k_Temple, k_Temple, k_Realm);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->origin, tap->origin);
	EXPECT_EQ(back->focus, tap->focus);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, TapsFurtherApartThanTheDoubleTapTimeAreSingle)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_LookedAtGround, k_Temple, k_Realm).has_value());
	const auto late = zoom.PressTemple(10000ms + k_DoubleTapTime + 1ms, k_Start, k_LookedAtGround, k_Temple, k_Realm);
	ASSERT_TRUE(late.has_value());
	ExpectViewOver(*late, k_LookedAtGround, HeadingOf(k_Start), k_TapDistance, k_TapPitch);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, WithoutATempleTheDoubleTapFliesOverTheRealm)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressTemple(10000ms, k_Start, k_LookedAtGround, std::nullopt, k_Realm).has_value());
	const auto view = zoom.PressTemple(10200ms, k_Start, k_LookedAtGround, std::nullopt, k_Realm);
	ASSERT_TRUE(view.has_value());
	ExpectViewOver(*view, k_Realm, HeadingOf(k_Start), k_RealmDistance, k_RealmPitch);
	EXPECT_TRUE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, TheRealmKeyFliesOverTheRealmAndBack)
{
	ZoomToPlaces zoom;
	const auto toRealm = zoom.PressRealm(k_Start, k_Realm);
	ASSERT_TRUE(toRealm.has_value());
	ExpectViewOver(*toRealm, k_Realm, HeadingOf(k_Start), k_RealmDistance, k_RealmPitch);
	EXPECT_TRUE(zoom.IsZoomedTo());

	const auto back = zoom.PressRealm(*toRealm, k_Realm);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->origin, k_Start.origin);
	EXPECT_EQ(back->focus, k_Start.focus);
	EXPECT_FALSE(zoom.IsZoomedTo());
}

TEST(ZoomToPlaces, LookingAwayTheRealmKeyFliesThereAgain)
{
	ZoomToPlaces zoom;
	ASSERT_TRUE(zoom.PressRealm(k_Start, k_Realm).has_value());
	// The camera has been moved away, further than the return radius: the key flies to the realm again, and back from
	// there to this view
	const CameraView away {.origin = k_Realm + glm::vec3(0.0f, 50.0f, k_ReturnRadius),
	                       .focus = k_Realm + glm::vec3(0.0f, 0.0f, k_ReturnRadius + 1.0f)};
	const auto again = zoom.PressRealm(away, k_Realm);
	ASSERT_TRUE(again.has_value());
	ExpectViewOver(*again, k_Realm, HeadingOf(away), k_RealmDistance, k_RealmPitch);
	const auto back = zoom.PressRealm(*again, k_Realm);
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->focus, away.focus);
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
