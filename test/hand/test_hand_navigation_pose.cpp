/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <limits>
#include <utility>

#include <gtest/gtest.h>

#include "3D/HandNavigationPose.h"

using namespace openblack;
using namespace openblack::hand_navigation_pose;
namespace tricon = camera_drag::tricon;
namespace feature = camera_drag::feature;

TEST(HandNavigationPose, GrippingTheLandShowsTheGrip)
{
	EXPECT_EQ(WhileDragging(true, tricon::k_Rotate), Pose::Grip);
	EXPECT_EQ(WhileDragging(true, 0), Pose::Grip);
}

TEST(HandNavigationPose, DraggingByTheEdgeShowsTurningTiltingOrZooming)
{
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileDragging(false, tricon::k_Pitch), Pose::Pitch);
	EXPECT_EQ(WhileDragging(false, tricon::k_Zoom | tricon::k_Pitch), Pose::Zoom);
	EXPECT_EQ(WhileDragging(false, 0), Pose::Idle);
	// Without the camera help's turning, there is no turning pose, and tilting is not tried instead
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate | tricon::k_Pitch, feature::k_Pitch), Pose::Idle);
	// Without zooming, the zoom hint shows the ordinary pose, even with the tilt hint
	EXPECT_EQ(WhileDragging(false, tricon::k_Zoom | tricon::k_Pitch, feature::k_Pitch), Pose::Idle);
	EXPECT_EQ(WhileDragging(false, tricon::k_Pitch, feature::k_Rotate), Pose::Idle);
}

TEST(HandNavigationPose, HoveringAtTheEdgeOffersTurning)
{
	EXPECT_EQ(WhileHovering(tricon::k_Idle), std::nullopt);
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Turning), Pose::Rotate);
	// Turning wins over tilting at the very bottom and the top
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileHovering(tricon::k_Pitch), Pose::Pitch);
	// The zoom hint offers nothing while hovering
	EXPECT_EQ(WhileHovering(tricon::k_Zoom), std::nullopt);
	// Without turning, the rotate hint offers nothing, not even the tilt beside it
	EXPECT_EQ(WhileHovering(tricon::k_Rotate | tricon::k_Pitch, feature::k_Pitch), std::nullopt);
}

TEST(HandNavigationPose, EachPoseHasItsAnimation)
{
	EXPECT_EQ(ClipName(Pose::Idle), "Cwiggle");
	EXPECT_EQ(ClipName(Pose::Grip), "Cgrip");
	EXPECT_EQ(ClipName(Pose::Rotate), "Crotate");
	EXPECT_EQ(ClipName(Pose::Pitch), "Cpitch");
	EXPECT_EQ(ClipName(Pose::Zoom), "Czoom");
}

TEST(HandNavigationPose, StandsUpTowardsTheFocusByThreeTenths)
{
	const auto up = UpTowardsFocus({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	// (10, 3, 0) made a unit vector
	const float inverse = 1.0f / std::sqrt(109.0f);
	EXPECT_EQ(up.x, 10.0f * inverse);
	EXPECT_EQ(up.y, 3.0f * inverse);
	EXPECT_EQ(up.z, 0.0f);
}

TEST(HandNavigationPose, TheLengthsSumYThenXThenZ)
{
	const glm::vec3 from(1.5f, -2.25f, 7.0f);
	const glm::vec3 focus(-3.125f, 4.5f, 0.375f);
	const float x = focus.x - from.x;
	const float y = focus.y - from.y;
	const float z = focus.z - from.z;
	const float xx = x * x;
	const float zz = z * z;
	const float raised = std::sqrt((y * y + xx) + zz) * 0.3f + y;
	const float inverse = 1.0f / std::sqrt((raised * raised + xx) + zz);
	const auto up = UpTowardsFocus(from, focus);
	EXPECT_EQ(up.x, x * inverse);
	EXPECT_EQ(up.y, raised * inverse);
	EXPECT_EQ(up.z, z * inverse);
}

TEST(HandNavigationPose, AtTheFocusTheUpIsLeftAsItIs)
{
	EXPECT_EQ(UpTowardsFocus({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}), glm::vec3(0.0f));
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const auto up = UpTowardsFocus({0.0f, 0.0f, 0.0f}, {nan, nan, nan});
	EXPECT_TRUE(std::isnan(up.x) && std::isnan(up.y) && std::isnan(up.z));
}

TEST(HandNavigationPose, AtTheRingCursorTheUpStandsTowardsTheFocusAsTheOriginalWorksIt)
{
	// the original's own answers (its stretch run natively at the game's precision): from, focus, then the up, as bits
	struct Case
	{
		std::array<uint32_t, 3> from;
		std::array<uint32_t, 3> focus;
		std::array<uint32_t, 3> up;
	};
	constexpr std::array<Case, 7> k_Cases {{
	    {{0x42C88000, 0x41480000, 0x4348C000}, {0x43020000, 0x41000000, 0x43820000}, {0x3EDFC2D1, 0x3E6831B7, 0x3F5ED222}},
	    {{0x44BB899A, 0x4206CCCD, 0x45034E66}, {0x44BE0333, 0x4224CCCD, 0x4500C666}, {0x3ECB7A88, 0x3ED9F8A0, 0xBF501A9B}},
	    {{0xC0533333, 0x40F66666, 0x3DCCCCCD}, {0x400CCCCD, 0xBFF33333, 0x4119999A}, {0x3EE7A0F3, 0xBEDC0D9A, 0x3F480B01}},
	    {{0x41200000, 0x42480000, 0x41200000}, {0x41200000, 0x41200000, 0x41200000}, {0x00000000, 0xBF800000, 0x00000000}},
	    // with these the hover's order of the squares gives other bits
	    {{0x44C2A823, 0x44E7CEF6, 0x44F87F7C}, {0x44CB817B, 0x44EC9B3E, 0x450078E5}, {0x3F16AD11, 0x3F14D247, 0x3F0FD293}},
	    {{0x44455EE6, 0x44966179, 0x44DC3588}, {0x4433A697, 0x449FE1FB, 0x44D2AA9A}, {0xBEEA35EC, 0x3F3D8269, 0xBEFC420E}},
	    {{0x44EA4F9A, 0x4504033C, 0x4234ACE4}, {0x44F010D2, 0x4502ACAB, 0x4266ED6A}, {0x3F75370F, 0xBDF3B22D, 0x3E85D47E}},
	}};
	const auto vec = [](const std::array<uint32_t, 3>& bits) {
		return glm::vec3(std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2]));
	};
	for (const auto& c : k_Cases)
	{
		const auto up = UpTowardsFocusAtRing(vec(c.from), vec(c.focus));
		EXPECT_EQ(std::bit_cast<uint32_t>(up.x), c.up[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(up.y), c.up[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(up.z), c.up[2]);
	}
	EXPECT_NE(UpTowardsFocusAtRing(vec(k_Cases[4].from), vec(k_Cases[4].focus)),
	          UpTowardsFocus(vec(k_Cases[4].from), vec(k_Cases[4].focus)));
	// at the focus the way is left as it is
	EXPECT_EQ(UpTowardsFocusAtRing({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}), glm::vec3(0.0f));
}

TEST(HandNavigationPose, TheTurningLiftIsAThirdOfTheHandsHeight)
{
	EXPECT_EQ(RotateLift(1.0f), 3.2f * 0.33f);
	EXPECT_EQ(RotateLift(0.5f), 3.2f * 0.5f * 0.33f);
}

TEST(HandNavigationPose, EachChangeOfTheCameraStatesPoseStartsTheBlendAndTheGripWithNothingUnderTheHandTwice)
{
	// no change, no blend
	EXPECT_EQ(ChangeOfCameraPose(Pose::Grip, Pose::Grip, false).blendStarts, 0);
	EXPECT_EQ(ChangeOfCameraPose(Pose::Idle, Pose::Idle, true).blendStarts, 0);
	// to the grip with nothing under the hand: to the land under the cursor, blended twice
	const auto toLand = ChangeOfCameraPose(Pose::Idle, Pose::Grip, false);
	EXPECT_TRUE(toLand.toLandUnderCursor);
	EXPECT_EQ(toLand.blendStarts, 2);
	// with an object, a screen object or the leash under it: the hand holds on where it is, blended once
	const auto inPlace = ChangeOfCameraPose(Pose::Rotate, Pose::Grip, true);
	EXPECT_FALSE(inPlace.toLandUnderCursor);
	EXPECT_EQ(inPlace.blendStarts, 1);
	// every other change, from the grip too, once
	constexpr std::array<std::pair<Pose, Pose>, 4> k_Changes {
	    {{Pose::Grip, Pose::Idle}, {Pose::Idle, Pose::Rotate}, {Pose::Rotate, Pose::Zoom}, {Pose::Zoom, Pose::Pitch}}};
	for (const auto& [from, to] : k_Changes)
	{
		const auto change = ChangeOfCameraPose(from, to, false);
		EXPECT_FALSE(change.toLandUnderCursor);
		EXPECT_EQ(change.blendStarts, 1);
	}
}

TEST(HandNavigationPose, TheCameraStatesHandStartsWhereTheClearViewPutItElseWhereItWasLeft)
{
	constexpr glm::vec3 k_Left {10.0f, 2.0f, 30.0f};
	constexpr glm::vec3 k_OnTheObject {40.0f, 5.0f, 60.0f};
	EXPECT_EQ(CameraStatePlace(k_Left, k_OnTheObject), k_OnTheObject);
	EXPECT_EQ(CameraStatePlace(k_Left, std::nullopt), k_Left);
}
