/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Step 3 of the script camera: the dual camera on two objects (start, update, validity, release and the stacked
// modes), the camera shakes and the camera exclusion zone (ReadZoneFile and the fence it sets, through
// camera_zones::CrossFence)

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "Blast/CameraShake.h"
#include "Camera/Camera.h"
#include "Camera/CameraZones.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
struct Things
{
	std::map<entt::entity, script_camera::ThingInfo> things;

	Things()
	{
		script_camera::Reset();
		script_camera::detail::SetThingReaderForTests([this](entt::entity e) -> std::optional<script_camera::ThingInfo> {
			const auto it = things.find(e);
			return it != things.end() ? std::optional(it->second) : std::nullopt;
		});
	}
	~Things()
	{
		script_camera::detail::SetThingReaderForTests(nullptr);
		script_camera::Reset();
	}
	Things(const Things&) = delete;
	Things& operator=(const Things&) = delete;
	Things(Things&&) = delete;
	Things& operator=(Things&&) = delete;
};

constexpr auto k_A = static_cast<entt::entity>(11);
constexpr auto k_B = static_cast<entt::entity>(12);
constexpr auto k_C = static_cast<entt::entity>(13);

script_camera::ThingInfo MakeThing(glm::vec3 mapPoint, float height, std::optional<float> radius)
{
	script_camera::ThingInfo info;
	info.mapPoint = mapPoint;
	info.height = height;
	info.radius2d = radius;
	info.drawnPoint = mapPoint + glm::vec3(5.0f, 0.0f, 0.0f); // not read by the dual camera
	return info;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b, float tolerance = 1e-3f)
{
	EXPECT_NEAR(a.x, b.x, tolerance);
	EXPECT_NEAR(a.y, b.y, tolerance);
	EXPECT_NEAR(a.z, b.z, tolerance);
}

/// The expected destinations of the dual camera's update, written out again independently
void ExpectedDual(const glm::vec3& a, float ha, std::optional<float> ra, const glm::vec3& b, std::optional<float> hb,
                  std::optional<float> rb, float factor, glm::vec3& focus, glm::vec3& position)
{
	const glm::vec3 middle = (b + a) * 0.5f;
	const float mean = (ha + hb.value_or(1.0f)) * 0.5f;
	const float tallest = std::max(ha, hb.value_or(0.0f));
	focus = {middle.x, middle.y + mean * 0.5f, middle.z};
	const float apart = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
	const float distance = (apart + rb.value_or(30.0f) + ra.value_or(30.0f)) * factor + tallest * 1.4f;
	const glm::vec3 v = b - a;
	const auto heading = static_cast<float>(static_cast<double>(script_camera::k_DualHeading) - affine::GetYAngle(v));
	position = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, script_camera::k_DualPitch);
}

/// A Lionhead segment file with one "cameraexc" segment
std::vector<uint8_t> ZoneFile(int32_t drawForceField, const std::vector<glm::vec3>& points, int32_t recordSize = 0x24)
{
	std::vector<uint8_t> data;
	const auto put = [&data](const void* p, size_t n) {
		const auto* bytes = static_cast<const uint8_t*>(p);
		data.insert(data.end(), bytes, bytes + n);
	};
	const int32_t header = 1;
	const int32_t flag = 0;
	const int32_t one = 1;
	const float limitA = 255.2f;
	const float limitB = 139.3f;
	const auto count = static_cast<int32_t>(points.size());
	const int32_t records = 0;
	put(&header, 4);
	put(&flag, 4);
	put(&drawForceField, 4);
	put(&one, 4);
	put(&flag, 4);
	put(&limitA, 4);
	put(&limitB, 4);
	put(&count, 4);
	for (const auto& p : points)
	{
		put(&p.x, 4);
		put(&p.y, 4);
		put(&p.z, 4);
	}
	put(&records, 4);
	put(&recordSize, 4);
	std::vector<uint8_t> file(8 + 32 + 4);
	std::memcpy(file.data(), "LiOnHeAd", 8);
	std::memcpy(file.data() + 8, "cameraexc", 9);
	const auto size = static_cast<uint32_t>(data.size());
	std::memcpy(file.data() + 40, &size, 4);
	file.insert(file.end(), data.begin(), data.end());
	return file;
}

const std::vector<glm::vec3> k_Square {{0, 5, 0}, {100, 5, 0}, {100, 5, 100}, {0, 5, 100}};
} // namespace

// ---- Dual camera -----------------------------------------------------------------------------------------------------

TEST(ScriptCameraDual, TwoThingsMiddleAndDistance)
{
	Things t;
	const glm::vec3 a(1000, 10, 2000);
	const glm::vec3 b(1040, 20, 2030);
	t.things[k_A] = MakeThing(a, 2.0f, 1.0f);
	t.things[k_B] = MakeThing(b, 4.0f, 3.0f);
	ASSERT_TRUE(script_camera::Begin({900, 50, 1900}, {1000, 0, 2000}));
	script_camera::StartDual(k_A, k_B, {}, {}); // over the script mode: the zoomers are not taken
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::ScriptModeCurrent()); // the script opcodes now meet "the wrong camera mode"
	EXPECT_TRUE(script_camera::HasMode());
	script_camera::Frame(0.05f, 16, 0.016f);
	glm::vec3 focus;
	glm::vec3 position;
	ExpectedDual(a, 2.0f, 1.0f, b, 4.0f, 3.0f, 1.0f, focus, position);
	ExpectNear(focus, {1020, 16.5f, 2015}); // middle (1020, 15, 2015) + (2 + 4) / 2 / 2
	ExpectNear(script_camera::Get().focus.GetDestination(), focus);
	ExpectNear(script_camera::Get().position.GetDestination(), position);
	// distance: (50 + 3 + 1) x 1 + 4 x 1.4 = 59.6 from the focus
	EXPECT_NEAR(glm::length(position - focus), 59.6f, 1e-3f);
}

TEST(ScriptCameraDual, PaceFromTwoSecondsToOne)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({900, 50, 900}, {1000, 0, 1000});
	script_camera::StartDual(k_A, k_B, {}, {});
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f); // a new mode starts its clock at 0
	script_camera::Frame(0.075f, 0, 0.0f);
	// 0.075 / 1.5 x (1 - 2) + 2 = 1.95 s
	EXPECT_NEAR(script_camera::Get().focus.axis[0].duration, 1.95f, 1e-5f);
	for (int i = 0; i < 20; ++i)
	{
		script_camera::Frame(0.1f, 0, 0.0f);
	}
	EXPECT_EQ(script_camera::Get().focus.axis[0].duration, 1.0f); // past 1.5 s: 1
}

TEST(ScriptCameraDual, SameThingsKeepTheMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_C] = MakeThing({1200, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	script_camera::StartDual(k_A, k_B, {}, {}); // the same pair again: the new mode deletes itself
	EXPECT_EQ(script_camera::Get().modes.Size(), 2u); // the script mode and one dual camera
	EXPECT_NEAR(script_camera::Get().modeSeconds, 0.1f, 1e-6f);
	script_camera::StartDual(k_B, k_A, {}, {}); // another order is another mode
	EXPECT_EQ(script_camera::Get().modes.Size(), 3u);
	EXPECT_TRUE(script_camera::UpdateDual(k_A, k_C)); // SetObjects on the current one
	EXPECT_EQ(script_camera::Get().modes.CurrentDual()->a, k_A);
	EXPECT_EQ(script_camera::Get().modes.CurrentDual()->b, k_C);
}

TEST(ScriptCameraDual, ReleasePopsBackToTheScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	EXPECT_FALSE(script_camera::ReleaseDual()); // the current mode is not a dual camera
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f); // popping back restarts the mode's clock
	EXPECT_FALSE(script_camera::UpdateDual(k_A, k_B));
}

TEST(ScriptCameraDual, EndReleasesOneDualThenTheScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	EXPECT_TRUE(script_camera::End()); // the dual, then the script mode
	EXPECT_FALSE(script_camera::HasMode());
	// Two dual cameras: End releases one, the other is current -> "wrong camera mode - excep", the script mode stays
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::StartDual(k_B, k_A, {}, {});
	EXPECT_FALSE(script_camera::End());
	EXPECT_TRUE(script_camera::Active());
	EXPECT_TRUE(script_camera::DualCurrent());
}

TEST(ScriptCameraDual, PointCamera)
{
	Things t;
	const glm::vec3 a(1000, 10, 2000);
	const glm::vec3 point(1060, 0, 2080);
	t.things[k_A] = MakeThing(a, 3.0f, 2.0f);
	script_camera::Begin({}, {});
	script_camera::StartDualWithPoint(k_A, point, {}, {});
	EXPECT_EQ(script_camera::Get().modes.CurrentDual()->distanceFactor, script_camera::k_DualPointDistanceFactor);
	script_camera::StartDualWithPoint(k_A, point, {}, {}); // same thing and point: not stacked again
	EXPECT_EQ(script_camera::Get().modes.Size(), 2u);
	script_camera::Frame(0.1f, 0, 0.0f);
	glm::vec3 focus;
	glm::vec3 position;
	// B's heights are 1 for the mean and 0 for the largest; B's radius 30
	ExpectedDual(a, 3.0f, 2.0f, point, std::nullopt, std::nullopt, 1.2f, focus, position);
	ExpectNear(script_camera::Get().focus.GetDestination(), focus);
	ExpectNear(script_camera::Get().position.GetDestination(), position);
}

TEST(ScriptCameraDual, ContainersHaveRadiusThirty)
{
	Things t;
	const glm::vec3 a(1000, 0, 1000);
	const glm::vec3 b(1000, 0, 1050);
	t.things[k_A] = MakeThing(a, 0.0f, std::nullopt);
	t.things[k_B] = MakeThing(b, 0.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto focus = script_camera::Get().focus.GetDestination();
	EXPECT_NEAR(glm::length(script_camera::Get().position.GetDestination() - focus), 50.0f + 30.0f + 30.0f, 1e-3f);
}

TEST(ScriptCameraDual, ThingOverTheOtherKeepsTheHeading)
{
	Things t;
	const glm::vec3 a(1000, 0, 1000);
	const glm::vec3 b(1000.005f, 40, 1000.005f); // |x|, |z| <= 0.01
	t.things[k_A] = MakeThing(a, 2.0f, 1.0f);
	t.things[k_B] = MakeThing(b, 2.0f, 1.0f);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto focus = script_camera::Get().focus.GetDestination();
	const float distance = (std::sqrt(0.005f * 0.005f * 2.0f) + 1.0f + 1.0f) + 2.0f * 1.4f;
	ExpectNear(script_camera::Get().position.GetDestination(),
	           script_camera::PointFromDistanceHeadingAndPitch(focus, distance, script_camera::k_DualHeading,
	                                                           script_camera::k_DualPitch));
}

TEST(ScriptCameraDual, GoneThingDropsTheModeOnTheTurn)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	t.things[k_B].available = false; // no longer available
	script_camera::Validate();       // the stacked modes are checked first
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	// a point camera with no thing moves nothing and goes at the next turn
	script_camera::StartDualWithPoint(entt::null, {0, 0, 0}, {}, {});
	const auto before = script_camera::Get().focus.GetDestination();
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_EQ(script_camera::Get().focus.GetDestination(), before);
	script_camera::Validate();
	EXPECT_FALSE(script_camera::DualCurrent());
}

TEST(ScriptCameraDual, DualDrivesWithoutAScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	// The player's zoomers, still heading somewhere: the dual camera goes on from them as they are (as BeginFrom)
	Zoomer3 origin;
	Zoomer3 focus;
	origin.SetPosition({900, 60, 900});
	focus.SetPosition({1000, 0, 1000});
	origin.SetDestinationWithTime({950, 60, 900}, 2.0f);
	origin.Update(0.5f);
	script_camera::StartDual(k_A, k_B, origin, focus); // over the player's mode
	EXPECT_TRUE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), origin.GetCurrentValue());
	EXPECT_EQ(script_camera::Get().position.GetSpeed(), origin.GetSpeed());
	EXPECT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(950, 60, 900));
	EXPECT_EQ(script_camera::Get().focus.GetCurrentValue(), glm::vec3(1000, 0, 1000));
	// Another one on top does not take them again
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto moved = script_camera::Get().position.GetCurrentValue();
	script_camera::StartDual(k_B, k_A, origin, focus);
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), moved);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_FALSE(script_camera::HasMode());
}

TEST(ScriptCameraDual, UnderTheCreatureInteractionsCameraItWaitsAndGoesOnFromTheCamerasZoomers)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	origin.SetPosition({900, 60, 900});
	focus.SetPosition({1000, 0, 1000});
	script_camera::StartDual(k_A, k_B, origin, focus); // over the player's mode
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto destination = script_camera::Get().focus.GetDestination();
	// The creature interaction's camera takes the view: the camera's zoomers go on from the dual camera's as they are
	Zoomer3 cameraOrigin;
	Zoomer3 cameraFocus;
	script_camera::Cover(cameraOrigin, cameraFocus);
	EXPECT_EQ(cameraOrigin.GetCurrentValue(), script_camera::Get().position.GetCurrentValue());
	EXPECT_EQ(cameraFocus.GetDestination(), destination);
	EXPECT_TRUE(script_camera::Covered());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::DualCurrent());
	// Meanwhile it is not updated, the script can neither change nor release it, and no script mode starts
	t.things[k_A] = MakeThing({1000, 0, 1400}, 2.0f, std::nullopt);
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_EQ(script_camera::Get().focus.GetDestination(), destination);
	EXPECT_FALSE(script_camera::UpdateDual(k_B, k_A));
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_FALSE(script_camera::Begin({}, {}));
	// the dual camera and the interaction's camera over it
	EXPECT_EQ(script_camera::Get().modes.Size(), 2u);
	// Given back, it is current again from the camera's zoomers as the interaction left them, and follows its things
	cameraOrigin.SetPosition({500, 40, 500});
	cameraFocus.SetPosition({600, 0, 600});
	script_camera::Uncover(cameraOrigin, cameraFocus);
	EXPECT_FALSE(script_camera::Covered());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), glm::vec3(500, 40, 500));
	EXPECT_EQ(script_camera::Get().focus.GetCurrentValue(), glm::vec3(600, 0, 600));
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_NE(script_camera::Get().focus.GetDestination(), destination);
}

TEST(ScriptCameraDual, OverTheCreatureInteractionsCameraItGoesOnTopAndItsEndLeavesThatCameraCurrent)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	const test::RestoreService<Locator::camera> restoreCamera;
	auto& camera = Locator::camera::emplace(glm::vec3(0.0f));
	camera.GetOriginZoomer().SetPosition({900, 60, 900});
	camera.GetFocusZoomer().SetPosition({1000, 0, 1000});
	// The interaction's camera takes the view over the player's mode and cannot be left: no script mode starts
	script_camera::Cover(camera.GetOriginZoomer(), camera.GetFocusZoomer());
	EXPECT_TRUE(script_camera::InteractionCurrent());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Begin({}, {}));
	// A dual camera never asks whether the current mode can be left: it goes on top, from the camera's zoomers, and
	// moves the camera
	script_camera::StartDual(k_A, k_B, camera.GetOriginZoomer(), camera.GetFocusZoomer());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_TRUE(script_camera::HasMode());
	EXPECT_TRUE(script_camera::Covered());
	EXPECT_FALSE(script_camera::InteractionCurrent());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), glm::vec3(900, 60, 900));
	ASSERT_EQ(script_camera::Get().modes.Size(), 2u);
	EXPECT_EQ(script_camera::Get().modes.Modes().front().kind, script_camera::StackedMode::Kind::Interaction);
	script_camera::Frame(0.1f, 0, 0.0f);
	// The hold's end takes off the current mode, the dual camera: the interaction's camera is current again and goes on
	// from the zoomers as the dual camera left them
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::InteractionCurrent());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_EQ(script_camera::Get().modes.Size(), 1u);
	EXPECT_EQ(camera.GetOriginZoomer().GetCurrentValue(), script_camera::Get().position.GetCurrentValue());
	EXPECT_EQ(camera.GetFocusZoomer().GetDestination(), script_camera::Get().focus.GetDestination());
}

TEST(ScriptCameraDual, TheInteractionsCameraLeftWithTheViewHoldsItUntilALaterHoldsEndTakesItOff)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	script_camera::StartDual(k_A, k_B, origin, focus); // before the hold
	script_camera::Cover(origin, focus);
	script_camera::StartDual(k_B, k_A, origin, focus); // over the interaction's camera
	EXPECT_TRUE(script_camera::ReleaseDual());         // the hold's end takes it off
	// Left with the view, the interaction's camera cannot be left: no script mode starts, the dual camera under it is
	// neither current nor released, and nothing else takes it off
	EXPECT_TRUE(script_camera::InteractionCurrent());
	EXPECT_FALSE(script_camera::Begin({}, {}));
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_FALSE(script_camera::DualCurrent());
	// A later hold cannot take the view from it, and that hold's end takes it off: the dual camera under it is current
	// again
	script_camera::Uncover(origin, focus);
	EXPECT_FALSE(script_camera::Covered());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Get().modes.Modes().back().dual.a, k_A);
}

TEST(ScriptCameraDual, AScriptsEndOverTheInteractionsCameraPutsANewPlayersModeOnTopThatTheHoldsEndTakesOff)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	script_camera::Cover(origin, focus);
	script_camera::StartDual(k_A, k_B, origin, focus);         // over the interaction's camera
	ASSERT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10})); // over the dual camera
	ASSERT_TRUE(script_camera::End());
	// The script mode deleted and a new player's mode on top, over the dual camera and the interaction's camera
	EXPECT_TRUE(script_camera::PlayerOverInteraction());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_TRUE(script_camera::Covered());
	ASSERT_EQ(script_camera::Get().modes.Size(), 3u);
	// The hold's end takes it off: the dual camera is current again, from the camera's zoomers as they are
	origin.SetPosition({500, 40, 500});
	focus.SetPosition({600, 0, 600});
	EXPECT_TRUE(script_camera::TakeOffPlayersModeOverInteraction(origin, focus));
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), glm::vec3(500, 40, 500));
	EXPECT_EQ(script_camera::Get().focus.GetCurrentValue(), glm::vec3(600, 0, 600));
	EXPECT_FALSE(script_camera::TakeOffPlayersModeOverInteraction(origin, focus));
	// Its release leaves the interaction's camera current
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::InteractionCurrent());
}

TEST(ScriptCameraDual, WithoutTheInteractionsCameraAScriptsEndPutsANewPlayersModeOverTheRest)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	script_camera::StartDual(k_A, k_B, origin, focus);
	ASSERT_TRUE(script_camera::Begin({0, 10, 0}, {0, 0, 10}));
	ASSERT_TRUE(script_camera::End());
	ASSERT_EQ(script_camera::Get().modes.Size(), 2u);
	EXPECT_TRUE(script_camera::Get().modes.PlayerCurrent());
	EXPECT_FALSE(script_camera::PlayerOverInteraction());
	EXPECT_FALSE(script_camera::HasMode());
}

TEST(ScriptCameraDual, CoveredWithNoModeOfItsOwnTheScriptCannotTakeTheCamera)
{
	Things t;
	Zoomer3 cameraOrigin;
	Zoomer3 cameraFocus;
	cameraOrigin.SetPosition({900, 60, 900});
	script_camera::Cover(cameraOrigin, cameraFocus); // over the player's camera: its zoomers are left alone
	EXPECT_EQ(cameraOrigin.GetCurrentValue(), glm::vec3(900, 60, 900));
	EXPECT_FALSE(script_camera::Begin({}, {}));
	EXPECT_FALSE(script_camera::Active());
	script_camera::Uncover(cameraOrigin, cameraFocus);
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_TRUE(script_camera::Begin({}, {}));
}

TEST(ScriptCameraDual, TakenOffByAnotherTheScriptModeGoesAndLeavesTheFieldOfViewAlone)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	script_camera::FocusFollow(k_A);
	script_camera::SetFov(1.0f, 0.0f);
	script_camera::Frame(0.1f, 16, 0.016f);
	// With a dual camera on top the script mode is not the current one
	script_camera::StartDual(k_A, k_B, {}, {});
	Zoomer3 origin;
	Zoomer3 focus;
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), script_camera::ModeStack::Popped::None);
	ASSERT_TRUE(script_camera::ReleaseDual());
	script_camera::Frame(0.1f, 16, 0.016f);
	const auto position = script_camera::Get().position.GetCurrentValue();
	ASSERT_EQ(script_camera::TakeOffScriptMode(origin, focus), script_camera::ModeStack::Popped::ModeUnder);
	EXPECT_FALSE(script_camera::Active());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_TRUE(script_camera::Get().focusThing == entt::null);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	// the camera under it goes on from the zoomers as the script mode left them, and the field of view stays
	EXPECT_EQ(origin.GetCurrentValue(), position);
	EXPECT_FLOAT_EQ(script_camera::Get().fov.value, 1.0f);
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), script_camera::ModeStack::Popped::None);
}

TEST(ScriptCameraModeStack, EachNewModeGoesOnTopAndOnlyTheTopIsCurrent)
{
	script_camera::ModeStack stack;
	EXPECT_TRUE(stack.Empty());
	EXPECT_EQ(stack.Current(), nullptr);
	EXPECT_TRUE(stack.CanLeaveCurrent()); // the player's mode can always be left
	EXPECT_FALSE(stack.PopDual());
	EXPECT_EQ(stack.PopScript(), script_camera::ModeStack::Popped::None);
	EXPECT_EQ(stack.EndScript(), 0u);
	stack.Push({.kind = script_camera::StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}});
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_TRUE(stack.CanLeaveCurrent()); // a dual camera can be left
	EXPECT_FALSE(stack.ScriptOnStack());
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	EXPECT_TRUE(stack.ScriptCurrent());
	EXPECT_FALSE(stack.CanLeaveCurrent()); // a living script mode cannot
	EXPECT_EQ(stack.CurrentDual(), nullptr);
	EXPECT_FALSE(stack.PopDual()); // only a current dual camera is released
	EXPECT_EQ(stack.Size(), 2u);
	stack.Push({.kind = script_camera::StackedMode::Kind::Dual, .dual = {.a = k_B, .b = k_C}});
	ASSERT_NE(stack.CurrentDual(), nullptr);
	EXPECT_EQ(stack.CurrentDual()->b, k_C);
	EXPECT_TRUE(stack.ScriptOnStack());
	EXPECT_EQ(stack.EndScript(), 0u); // the script mode is not the current one
	EXPECT_TRUE(stack.PopDual());     // back to the script mode under it
	EXPECT_TRUE(stack.ScriptCurrent());
	EXPECT_NE(stack.EndScript(), 0u); // and a new player's mode goes over the dual camera under the script mode
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_EQ(stack.Modes().front().kind, script_camera::StackedMode::Kind::Dual);
}

TEST(ScriptCameraModeStack, TheTurnsCheckDeletesInvalidDualCamerasWhereverTheyAre)
{
	script_camera::ModeStack stack;
	stack.Push({.kind = script_camera::StackedMode::Kind::Dual, .dual = {.a = k_A, .b = k_B}});
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	stack.Push({.kind = script_camera::StackedMode::Kind::Dual, .dual = {.a = k_B, .b = k_C}});
	const auto withoutA = [](const script_camera::DualMode& dual) { return dual.a != k_A && dual.b != k_A; };
	EXPECT_FALSE(stack.RemoveInvalid(withoutA)); // the bottom one goes; the current one stays
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_EQ(stack.Modes().front().kind, script_camera::StackedMode::Kind::Script);
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_TRUE(stack.RemoveInvalid([](const script_camera::DualMode&) { return false; })); // the current one goes
	EXPECT_TRUE(stack.ScriptCurrent()); // the script mode is never removed by the check
	EXPECT_EQ(stack.Size(), 1u);
}

namespace
{
script_camera::StackedMode NumberedDual(int number)
{
	return {.kind = script_camera::StackedMode::Kind::Dual,
	        .dual = {.a = k_A, .b = k_B, .point = {static_cast<float>(number), 0.0f, 0.0f}}};
}
} // namespace

TEST(ScriptCameraModeStack, TwelveModesFitWithThePlayersModeCounted)
{
	using Dropped = script_camera::ModeStack::Dropped;
	script_camera::ModeStack stack;
	EXPECT_EQ(stack.Places(), 1u); // the player's own mode
	for (int i = 0; i < 11; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i)), Dropped::None);
	}
	EXPECT_EQ(stack.Places(), script_camera::ModeStack::k_Places);
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Size(), 11u);
}

TEST(ScriptCameraModeStack, TheThirteenthModeDeletesTheBottomOne)
{
	using Dropped = script_camera::ModeStack::Dropped;
	script_camera::ModeStack stack;
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	// The player's own mode goes first, then the lowest of the stacked modes, the rest moving down a place
	EXPECT_EQ(stack.Push(NumberedDual(11)), Dropped::PlayersMode);
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Size(), 12u);
	EXPECT_EQ(stack.Places(), 12u);
	EXPECT_EQ(stack.Push(NumberedDual(12)), Dropped::Own);
	ASSERT_EQ(stack.Size(), 12u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.CurrentDual()->point.x, 12.0f);
	// Pops then go down to the bottom mode, and an emptied stack takes a new player's mode, once
	for (int i = 12; i > 0; --i)
	{
		ASSERT_NE(stack.CurrentDual(), nullptr);
		EXPECT_EQ(stack.CurrentDual()->point.x, static_cast<float>(i));
		EXPECT_FALSE(stack.NewPlayersModeIfGone());
		EXPECT_TRUE(stack.PopDual());
	}
	EXPECT_TRUE(stack.Empty());
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_TRUE(stack.NewPlayersModeIfGone());
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_FALSE(stack.NewPlayersModeIfGone());
	EXPECT_EQ(stack.Places(), 1u);
}

TEST(ScriptCameraModeStack, TheOtherSystemsModesTakePlacesUnderTheseOnes)
{
	using Dropped = script_camera::ModeStack::Dropped;
	script_camera::ModeStack stack;
	// Creature Mode's follow and a placed path under them: nine dual cameras fill the stack
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i), 2), Dropped::None);
	}
	EXPECT_EQ(stack.Push(NumberedDual(9), 2), Dropped::PlayersMode);
	// The lowest is then one of theirs, which this module does not delete
	EXPECT_EQ(stack.Push(NumberedDual(10), 2), Dropped::OtherSystemsMode);
	EXPECT_EQ(stack.Size(), 11u);
	// which their own systems are to delete
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 1u);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
	// Nor does a new player's mode come while they are on the stack
	stack.Clear();
	EXPECT_FALSE(stack.PlayersModeGone());
}

TEST(ScriptCameraModeStack, AScriptModeOverAFollowKeepsTheFollowsPlace)
{
	using Dropped = script_camera::ModeStack::Dropped;
	script_camera::ModeStack stack;
	// The follow is counted among the others as the script mode takes over from it, and by the script mode from then on
	EXPECT_EQ(stack.Push({.kind = script_camera::StackedMode::Kind::Script, .overFollow = true}, 1), Dropped::None);
	EXPECT_EQ(stack.Places(), 3u);
	for (int i = 0; i < 9; ++i)
	{
		EXPECT_EQ(stack.Push(NumberedDual(i)), Dropped::None);
	}
	EXPECT_EQ(stack.Push(NumberedDual(9)), Dropped::PlayersMode);
	// The follow under the script mode is the bottom then: it goes, and the script mode stays
	EXPECT_EQ(stack.Push(NumberedDual(10)), Dropped::OtherSystemsMode);
	EXPECT_FALSE(stack.Modes().front().overFollow);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u); // the follow was given up already
	EXPECT_TRUE(stack.ScriptOnStack());
	EXPECT_EQ(stack.Push(NumberedDual(11)), Dropped::Own);
	EXPECT_FALSE(stack.ScriptOnStack());
}

TEST(ScriptCameraModeStack, AScriptsEndPutsANewPlayersModeOverTheModesUnderIt)
{
	script_camera::ModeStack stack;
	stack.Push(NumberedDual(0));
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	const auto id = stack.EndScript(true);
	ASSERT_NE(id, 0u);
	// The script mode deleted; the dual camera under it stays, and the player's mode it went over is kept for it
	ASSERT_EQ(stack.Size(), 2u);
	EXPECT_TRUE(stack.PlayerCurrent());
	EXPECT_TRUE(stack.PlayersModeCurrent());
	EXPECT_FALSE(stack.ScriptOnStack());
	EXPECT_TRUE(stack.KeepsModel(id));
	EXPECT_EQ(stack.Places(), 3u); // the bottom player's mode, the dual camera and the new one
	// Popped, it gives the kept model back, and the dual camera is current again
	const auto popped = stack.PopPlayersMode();
	ASSERT_TRUE(popped.has_value());
	EXPECT_EQ(popped->id, id);
	EXPECT_TRUE(popped->keptUnder);
	EXPECT_FALSE(stack.KeepsModel(id));
	EXPECT_TRUE(stack.DualCurrent());
	EXPECT_FALSE(stack.PopPlayersMode().has_value()); // a dual camera is not the player's mode
}

TEST(ScriptCameraModeStack, EachScriptsEndPutsAnotherPlayersModeOnTop)
{
	script_camera::ModeStack stack;
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	const auto first = stack.EndScript(true);
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	const auto second = stack.EndScript(true);
	ASSERT_NE(first, second);
	EXPECT_EQ(stack.Size(), 2u);
	EXPECT_EQ(stack.Places(), 3u);
	// Popped one by one down to the bottom one, which leaves nothing under it
	EXPECT_EQ(stack.PopPlayersMode()->id, second);
	EXPECT_EQ(stack.PopPlayersMode()->id, first);
	const auto bottom = stack.PopPlayersMode();
	ASSERT_TRUE(bottom.has_value());
	EXPECT_EQ(bottom->id, 0u);
	EXPECT_FALSE(bottom->keptUnder);
	EXPECT_TRUE(stack.Empty());
}

TEST(ScriptCameraModeStack, AScriptsEndOverAGoneBottomPlayersModeKeepsNothing)
{
	script_camera::ModeStack stack;
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	ASSERT_TRUE(stack.PlayersModeGone());
	// The new player's mode goes on top while the deleted script mode still has its place: the lowest dual camera goes
	const auto id = stack.EndScript(true);
	EXPECT_FALSE(stack.KeepsModel(id));
	EXPECT_TRUE(stack.PlayersModeGone());
	ASSERT_EQ(stack.Size(), 11u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_TRUE(stack.PlayerCurrent());
}

TEST(ScriptCameraModeStack, AFullStackDeletingTheBottomPlayersModeLosesItsKeptModel)
{
	script_camera::ModeStack stack;
	const auto id = stack.PushPlayer(true);
	ASSERT_TRUE(stack.KeepsModel(id));
	for (int i = 0; i < 10; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	EXPECT_EQ(stack.Places(), script_camera::ModeStack::k_Places);
	EXPECT_TRUE(stack.KeepsModel(id));
	EXPECT_EQ(stack.Push(NumberedDual(10)), script_camera::ModeStack::Dropped::PlayersMode);
	EXPECT_FALSE(stack.KeepsModel(id)); // the bottom one it kept is gone
	// The new player's mode is the lowest then, and goes next: the one put over the rest keeps nothing of it either
	const auto over = stack.PushPlayer(true);
	EXPECT_FALSE(stack.KeepsModel(over));
	EXPECT_EQ(stack.Modes().front().kind, script_camera::StackedMode::Kind::Dual);
}

TEST(ScriptCameraModeStack, AScriptModeUnderANewPlayersModeIsNoLongerTheScriptsCamera)
{
	script_camera::ModeStack stack;
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	stack.Push(NumberedDual(0));
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	EXPECT_NE(stack.EndScript(), 0u);
	// The first script mode stays under the new player's mode, which a script's camera can be started over
	ASSERT_EQ(stack.Size(), 3u);
	EXPECT_EQ(stack.Modes().front().kind, script_camera::StackedMode::Kind::Script);
	EXPECT_FALSE(stack.ScriptOnStack());
	EXPECT_TRUE(stack.CanLeaveCurrent());
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	EXPECT_TRUE(stack.ScriptOnStack());
}

TEST(ScriptCameraModeStack, APopLeavesTheModeUnderCurrentOrANewPlayersMode)
{
	using script_camera::AfterPop;
	// A dual camera or a placed path under it is current again, whatever player's mode lies under them
	EXPECT_EQ(script_camera::AfterPopOf(true, false, true), AfterPop::ModeUnder);
	EXPECT_EQ(script_camera::AfterPopOf(false, true, false), AfterPop::ModeUnder);
	// Else the player's mode under it restarts, or a new player's mode is made when there is none
	EXPECT_EQ(script_camera::AfterPopOf(false, false, true), AfterPop::PlayersModeUnder);
	EXPECT_EQ(script_camera::AfterPopOf(false, false, false), AfterPop::NewPlayersMode);
}

TEST(ScriptCameraModeStack, TheLastModePoppedOffWithThePlayersModeGoneMakesANewOne)
{
	using Popped = script_camera::ModeStack::Popped;
	// The creature interaction's camera at the bottom, then dual cameras over it until the player's own mode is deleted
	script_camera::ModeStack stack;
	stack.Push({.kind = script_camera::StackedMode::Kind::Interaction});
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	// Its pop empties the stack: a new player's mode is made, the bottom from then on
	EXPECT_EQ(stack.PopInteraction(), Popped::NewPlayersMode);
	EXPECT_TRUE(stack.Empty());
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(stack.Places(), 1u);
	EXPECT_EQ(stack.PopInteraction(), Popped::None);
	// The script mode the same
	stack.Push({.kind = script_camera::StackedMode::Kind::Script});
	for (int i = 0; i < 11; ++i)
	{
		stack.Push(NumberedDual(i));
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	EXPECT_EQ(stack.PopScript(), Popped::NewPlayersMode);
	EXPECT_FALSE(stack.PlayersModeGone());
}

TEST(ScriptCameraModeStack, APopOverAModeLeftUnderItMakesNoNewPlayersMode)
{
	using Popped = script_camera::ModeStack::Popped;
	script_camera::ModeStack stack;
	// With the player's own mode there, the pop leaves it current
	stack.Push({.kind = script_camera::StackedMode::Kind::Interaction});
	EXPECT_EQ(stack.PopInteraction(), Popped::ModeUnder);
	EXPECT_FALSE(stack.PlayersModeGone());
	// With it gone, a mode of this module's or another system's (here a placed path) left under it is current again
	stack.Push(NumberedDual(0), 1);
	stack.Push({.kind = script_camera::StackedMode::Kind::Script}, 1);
	for (int i = 1; i < 10; ++i)
	{
		stack.Push(NumberedDual(i), 1);
	}
	ASSERT_TRUE(stack.PlayersModeGone());
	for (int i = 1; i < 10; ++i)
	{
		ASSERT_TRUE(stack.PopDual());
	}
	EXPECT_EQ(stack.PopScript(1), Popped::ModeUnder);
	EXPECT_TRUE(stack.PlayersModeGone());
	ASSERT_TRUE(stack.PopDual());
	stack.Push({.kind = script_camera::StackedMode::Kind::Interaction}, 1);
	EXPECT_EQ(stack.PopInteraction(1), Popped::ModeUnder);
	EXPECT_TRUE(stack.PlayersModeGone());
}

TEST(ScriptCameraDual, TheScriptModeTakenOffAnEmptiedStackWithThePlayersModeGoneMakesANewOne)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	Zoomer3 origin;
	Zoomer3 focus;
	// Eleven dual cameras over the script mode: the last deletes the player's own mode
	for (int i = 0; i < 11; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Get().modes.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(script_camera::ReleaseDual());
	}
	ASSERT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), script_camera::ModeStack::Popped::NewPlayersMode);
	EXPECT_FALSE(script_camera::Get().modes.PlayersModeGone());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
}

TEST(ScriptCameraDual, TheInteractionsCameraUncoveringAnEmptiedStackWithThePlayersModeGoneMakesANewOne)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	script_camera::Cover(origin, focus);
	// Dual cameras started over the interaction's camera until the player's own mode is deleted, then released
	for (int i = 0; i < 11; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Get().modes.PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(script_camera::ReleaseDual());
	}
	ASSERT_TRUE(script_camera::InteractionCurrent());
	script_camera::Get().modeSeconds = 1.0f; // the new player's mode's seconds start from 0
	EXPECT_EQ(script_camera::Uncover(origin, focus), script_camera::ModeStack::Popped::NewPlayersMode);
	EXPECT_FALSE(script_camera::Get().modes.PlayersModeGone());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	EXPECT_EQ(script_camera::Uncover(origin, focus), script_camera::ModeStack::Popped::None);
}

namespace
{
/// The placed paths as far as the stack asks of them: their numbers in the order of pushes, the first the lowest
class StackedPaths final: public ecs::systems::CameraPathSystemInterface
{
public:
	std::vector<uint32_t> placed;
	int deleted {0};

	void Start(entt::id_type /*id*/) override {}
	void Stop() override {}
	void Play() override {}
	void Pause() override {}
	void Update(const std::chrono::microseconds& /*dt*/) override {}
	bool IsPathing() override { return !placed.empty(); }
	bool IsPaused() override { return false; }
	bool Begin(PathOwner /*owner*/, entt::resource<CameraPath> /*path*/, const glm::mat4& /*placement*/,
	           float /*pauseSeconds*/) override
	{
		return false;
	}
	void FollowAt(PathOwner /*owner*/, int32_t /*pathMilliseconds*/, bool /*playing*/) override {}
	void Release(PathOwner /*owner*/) override {}
	void CheckPaths() override {}
	[[nodiscard]] bool HoldsCamera() const override { return false; }
	[[nodiscard]] std::size_t PlacedCount() const override { return placed.size(); }
	void SetCovered(bool /*covered*/) override {}
	[[nodiscard]] bool IsCovered() const override { return false; }
	bool TakeOffCurrent() override { return false; }
	[[nodiscard]] std::optional<uint32_t> LowestPushed() const override
	{
		return placed.empty() ? std::nullopt : std::optional(placed.front());
	}
	void DeleteLowest() override
	{
		placed.erase(placed.begin());
		++deleted;
	}
	void LeaveUnderPlayersCamera(uint32_t /*playersMode*/) override {}
	void UncoverFromPlayersCamera(uint32_t /*playersMode*/) override {}
	[[nodiscard]] std::optional<PlacedReadout> CurrentPlaced() const override { return std::nullopt; }
	void HandlePlayerControl(const PlayerControl& /*control*/) override {}
};

/// Creature Mode's follow as far as the stack asks of it: whether it is on, its number in the order of pushes, its end
class StackedFollow final: public ecs::systems::CreatureModeSystemInterface
{
public:
	bool active {false};
	uint32_t pushed {0};
	int leaves {0};

	void Update(std::chrono::microseconds /*dt*/, const Frame& /*frame*/) override {}
	void PressCreatureKey() override {}
	bool Enter(entt::entity /*creature*/) override { return false; }
	bool Restore(entt::entity /*thing*/, const creature_follow::View& /*view*/,
	             const creature_follow::Style& /*style*/) override
	{
		return false;
	}
	void Leave() override
	{
		active = false;
		++leaves;
	}
	void LockOnNext(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsActive() const override { return active; }
	[[nodiscard]] std::optional<uint32_t> PushedAt() const override { return active ? std::optional(pushed) : std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_follow::Style> GetStyle() const override { return std::nullopt; }
	void ClearView() override {}
};
} // namespace

TEST(ScriptCameraDual, AFullStackWithThePlayersModeGoneDeletesTheLowestFollowOrPathThroughItsOwnSystem)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	const test::RestoreService<Locator::creatureModeSystem> restoreFollow;
	auto& paths = static_cast<StackedPaths&>(Locator::cameraPathSystem::emplace<StackedPaths>());
	auto& follow = static_cast<StackedFollow&>(Locator::creatureModeSystem::emplace<StackedFollow>());
	// A path, then the follow, then another path, pushed in that order under the dual cameras
	paths.placed = {1, 3};
	follow.active = true;
	follow.pushed = 2;
	Zoomer3 origin;
	Zoomer3 focus;
	// With the player's own mode, the three and eight dual cameras fill the stack; the ninth deletes the player's mode
	for (int i = 0; i < 9; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Get().modes.PlayersModeGone());
	EXPECT_EQ(paths.deleted, 0);
	EXPECT_EQ(follow.leaves, 0);
	// Then the lowest of them goes, through its own system: the first path, the follow, the other path
	script_camera::StartDualWithPoint(k_A, {100, 0, 0}, origin, focus);
	EXPECT_EQ(paths.placed, std::vector<uint32_t> {3});
	EXPECT_EQ(follow.leaves, 0);
	script_camera::StartDualWithPoint(k_A, {101, 0, 0}, origin, focus);
	EXPECT_EQ(follow.leaves, 1);
	EXPECT_FALSE(follow.active);
	EXPECT_EQ(paths.deleted, 1);
	script_camera::StartDualWithPoint(k_A, {102, 0, 0}, origin, focus);
	EXPECT_TRUE(paths.placed.empty());
	EXPECT_EQ(paths.deleted, 2);
	// Then the lowest dual camera
	EXPECT_EQ(script_camera::Get().modes.Size(), 12u);
	script_camera::StartDualWithPoint(k_A, {103, 0, 0}, origin, focus);
	EXPECT_EQ(script_camera::Get().modes.Size(), 12u);
	EXPECT_EQ(paths.deleted, 2);
	EXPECT_EQ(follow.leaves, 1);
}

TEST(ScriptCameraDual, AScriptModeTakingOverFromADeletedFollowNoLongerKeepsItsPlace)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	const test::RestoreService<Locator::creatureModeSystem> restoreFollow;
	Locator::cameraPathSystem::reset();
	auto& follow = static_cast<StackedFollow&>(Locator::creatureModeSystem::emplace<StackedFollow>());
	follow.active = true;
	follow.pushed = 1;
	Zoomer3 origin;
	Zoomer3 focus;
	// Dual cameras over the follow until the player's own mode is deleted and the stack is full again
	for (int i = 0; i < 11; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Get().modes.PlayersModeGone());
	EXPECT_EQ(follow.leaves, 0);
	// The script mode taking over from the follow deletes it, the lowest: no place is kept for it
	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	EXPECT_EQ(follow.leaves, 1);
	ASSERT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_FALSE(script_camera::Get().modes.Current()->overFollow);
	EXPECT_EQ(script_camera::Get().modes.Places(), script_camera::ModeStack::k_Places);
}

TEST(ScriptCameraDual, AScriptModeStartedOverADualCameraGoesOnTopOfIt)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	Zoomer3 origin;
	Zoomer3 focus;
	origin.SetPosition({900, 60, 900});
	focus.SetPosition({1000, 0, 1000});
	script_camera::StartDual(k_A, k_B, origin, focus); // over the player's mode
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto position = script_camera::Get().position.GetCurrentValue();
	const auto destination = script_camera::Get().position.GetDestination();
	// The dual camera can be left: the script mode goes on top of it, from the zoomers the dual camera has
	Zoomer3 player;
	player.SetPosition({0, 0, 0});
	ASSERT_TRUE(script_camera::BeginFrom(player, player));
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), position);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), destination);
	// The script's release finds no current dual camera
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_FALSE(script_camera::UpdateDual(k_B, k_A));
	// and the dual camera under it is not updated
	t.things[k_A] = MakeThing({1000, 0, 1400}, 2.0f, std::nullopt);
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), destination);
	// The end of control deletes the script mode; the new player's mode goes on top, and the dual camera stays under
	// it, current again only if that mode is popped
	EXPECT_TRUE(script_camera::End());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Active());
	ASSERT_EQ(script_camera::Get().modes.Size(), 2u);
	EXPECT_TRUE(script_camera::Get().modes.PlayerCurrent());
}

TEST(ScriptCameraDual, ASecondScriptModeGoesOverADualCameraOverTheFirst)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	EXPECT_FALSE(script_camera::Begin({}, {})); // a current script mode cannot be left
	script_camera::FocusFollow(k_A);
	script_camera::StartDual(k_A, k_B, {}, {});
	ASSERT_TRUE(script_camera::Begin({}, {})); // a dual camera can
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::Get().modes.Size(), 3u);
	EXPECT_TRUE(script_camera::Get().focusThing == entt::null); // a new script mode follows nothing
	EXPECT_TRUE(script_camera::End());
	// The first script mode and the dual camera stay under the new player's mode: neither has the camera
	ASSERT_EQ(script_camera::Get().modes.Size(), 3u);
	EXPECT_TRUE(script_camera::Get().modes.PlayerCurrent());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_FALSE(script_camera::HasMode());
}

TEST(ScriptCameraDual, AReleasedDualCameraGivesTheCameraBackToTheDualCameraUnderIt)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({}, {});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::StartDualWithPoint(k_A, {1000, 0, 1200}, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::DualCurrent()); // the two things camera under it
	EXPECT_EQ(script_camera::Get().modes.CurrentDual()->b, k_B);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
}

TEST(ScriptCameraDual, AGoneThingDropsADualCameraUnderTheScriptMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::StartDual(k_A, k_B, {}, {}); // over the player's mode
	ASSERT_TRUE(script_camera::Begin({}, {}));
	script_camera::Frame(0.1f, 0, 0.0f);
	const float seconds = script_camera::Get().modeSeconds;
	t.things[k_A].available = false;
	script_camera::Validate();
	EXPECT_EQ(script_camera::Get().modes.Size(), 1u);
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, seconds); // the current mode stayed: its seconds go on
}

TEST(ScriptCameraDual, ArrivedIgnoresThePathUnderneath)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	script_camera::Begin({1050, 30, 900}, {1050, 0, 1000});
	script_camera::StartDual(k_A, k_B, {}, {});
	script_camera::Frame(0.1f, 0, 0.0f);
	EXPECT_FALSE(script_camera::ScriptArrived()); // arrival is judged on the zoomers
	// the zoomers on their destinations (the dual camera sets new ones every frame, so it only arrives when still)
	const auto& state = script_camera::Get();
	script_camera::SetPositionAndFocus(state.position.GetDestination(), state.focus.GetDestination());
	EXPECT_TRUE(script_camera::ScriptArrived());
}

// ---- Camera shake ----------------------------------------------------------------------------------------------------

TEST(CameraShake, CreateAndTick)
{
	camera_shake::Reset();
	camera_shake::StartCameraShake({10, 0, 10}, 50.0f, 2.0f, 0.4004f); // rounded: 400.4 -> 400 ms
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 400);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 400);
	EXPECT_FALSE(camera_shake::Checkers().front().yOnly);
	camera_shake::Create(5.0f, {0, 0, 0}, 1.0f, 100, true);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 100); // newest first
	// 100 - 100 = 0: freed
	camera_shake::Tick(100);
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 300);
	camera_shake::Tick(299);
	EXPECT_EQ(camera_shake::Checkers().size(), 1u);
	camera_shake::Tick(1);
	EXPECT_TRUE(camera_shake::Checkers().empty());
}

TEST(CameraShake, AnEffectsShakeCutsItsMillisecondsAndMayBeUpAndDownOnly)
{
	camera_shake::Reset();
	// the blast's beam: 200 m, 0.7 s, up and down only
	camera_shake::StartEffectShake({10, 0, 10}, 200.0f, 1.0f, 0.7f, true);
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 700);
	EXPECT_EQ(camera_shake::Checkers().front().maxDistance, 200.0f);
	EXPECT_TRUE(camera_shake::Checkers().front().yOnly);
	// cut, where the script's shake rounds: 2999.9 ms is 2999
	camera_shake::StartEffectShake({0, 0, 0}, 50.0f, 1.0f, 2.9999f, false);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 2999);
	EXPECT_FALSE(camera_shake::Checkers().front().yOnly);
	camera_shake::StartCameraShake({0, 0, 0}, 50.0f, 1.0f, 2.9999f);
	EXPECT_EQ(camera_shake::Checkers().front().totalMs, 3000);
	camera_shake::Reset();
}

TEST(CameraShake, OneTickAFrame)
{
	// The script camera's frame does not tick the shakes; ApplyShake does, once per rendered frame
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 1.0f, 1000, false);
	Camera camera;
	static_cast<void>(script_camera::UpdateCamera(camera, 0.016f, 16, 0.016f));
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_EQ(camera_shake::Checkers().front().remainingMs, 1000);
	script_camera::ApplyShake(camera, camera.GetOrigin());
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_LT(camera_shake::Checkers().front().remainingMs, 1000);
	camera_shake::Reset();
}

TEST(CameraSphere, ThePlayersCameraStaysInTheWorld)
{
	// The camera's update pulls any mode's position destination back to the sphere round (2560, 0, 2560): it tests 3500
	// squared but scales by the slightly different k_DiscScale, so the point lands at 1 / k_DiscScale (about 3499)
	Camera camera;
	camera.GetOriginZoomer().SetPosition({2560.0f + 5000.0f, 0.0f, 2560.0f});
	camera.UpdateZoomers(std::nullopt, 0.016f);
	const auto destination = camera.GetOriginZoomer().GetDestination();
	EXPECT_NEAR(destination.x, 2560.0f + 1.0f / script_camera::k_DiscScale, 0.01f);
	EXPECT_EQ(destination.z, 2560.0f);
	camera.GetOriginZoomer().SetPosition({3000.0f, 50.0f, 3000.0f}); // inside: untouched
	camera.UpdateZoomers(std::nullopt, 0.016f);
	EXPECT_EQ(camera.GetOriginZoomer().GetDestination(), glm::vec3(3000.0f, 50.0f, 3000.0f));
}

TEST(CameraShake, OnlyWithinTheRadiusAndLessAndLess)
{
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 4.0f, 1000, false);
	glm::vec3 position(100, 0, 0);
	glm::vec3 target(110, 0, 0);
	camera_shake::Adjust({100, 0, 0}, position, target); // 100 from the point: no shake
	EXPECT_EQ(position, glm::vec3(100, 0, 0));
	EXPECT_EQ(target, glm::vec3(110, 0, 0));
	camera_shake::Tick(750); // a = 250 / 1000 x 4 = 1
	for (int i = 0; i < 50; ++i)
	{
		glm::vec3 p(10, 0, 0);
		glm::vec3 f(20, 0, 0);
		camera_shake::Adjust({10, 0, 0}, p, f);
		EXPECT_LE(std::abs(p.x - 10.0f), 1.0f);
		EXPECT_LE(std::abs(p.y), 1.0f);
		EXPECT_LE(std::abs(p.z), 1.0f);
		EXPECT_LE(std::abs(f.x - 20.0f), 1.0f);
	}
}

TEST(CameraShake, YOnlyMovesY)
{
	camera_shake::Reset();
	camera_shake::Create(50.0f, {0, 0, 0}, 4.0f, 1000, true);
	glm::vec3 p(10, 0, 0);
	glm::vec3 f(20, 0, 0);
	camera_shake::Adjust({10, 0, 0}, p, f);
	EXPECT_EQ(p.x, 10.0f);
	EXPECT_EQ(p.z, 0.0f);
	EXPECT_EQ(f.x, 20.0f);
	EXPECT_EQ(f.z, 0.0f);
	camera_shake::Reset();
}

TEST(CameraShake, TheNearestDecides)
{
	camera_shake::Reset();
	camera_shake::Create(1000.0f, {0, 0, 0}, 4.0f, 1000, false); // far one with a big radius
	camera_shake::Create(5.0f, {30, 0, 0}, 4.0f, 1000, false);   // nearer, but its radius does not reach
	glm::vec3 p(20, 0, 0);
	glm::vec3 f(25, 0, 0);
	camera_shake::Adjust({20, 0, 0}, p, f); // nearest is the second (10 < 20): 10 >= 5, nothing
	EXPECT_EQ(p, glm::vec3(20, 0, 0));
	camera_shake::Reset();
}

// ---- Camera exclusion zone -------------------------------------------------------------------------------------------

TEST(PlayerCameraZone, ReadsTheSegment)
{
	const auto file = player_camera::ReadZoneFile(ZoneFile(0, k_Square));
	EXPECT_TRUE(file.whole);
	const auto& zone = file.zone;
	EXPECT_EQ(zone.header, 1);
	EXPECT_EQ(zone.drawForceField, 0);
	EXPECT_EQ(zone.useMaxAltitude, 1);
	EXPECT_EQ(zone.maxAltitude, 255.2f);
	EXPECT_EQ(zone.heightAboveLand, 139.3f);
	EXPECT_EQ(zone.forceFieldPoints.size(), 4u);
	EXPECT_TRUE(zone.exclusions.empty());
	// no LiOnHeAd: nothing read, the zone as reset
	const auto none = player_camera::ReadZoneFile({});
	EXPECT_FALSE(none.whole);
	EXPECT_FALSE(none.headerRead);
	EXPECT_TRUE(none.zone.forceFieldPoints.empty());
	EXPECT_EQ(none.zone.maxAltitude, 500.0f);
	EXPECT_EQ(none.zone.useExclusions, 1);
}

// A zone with no file is the game's reset: exclusions on, no force field, both height limits off at 500
TEST(PlayerCameraZone, AZoneStartsAsTheGameResetsIt)
{
	const auto zone = player_camera::ReadZoneFile({}).zone;
	EXPECT_EQ(zone.useExclusions, 1);
	EXPECT_EQ(zone.drawForceField, 0);
	EXPECT_EQ(zone.useMaxAltitude, 0);
	EXPECT_EQ(zone.useHeightAboveLand, 0);
	EXPECT_EQ(std::bit_cast<uint32_t>(zone.maxAltitude), 0x43FA0000u);
	EXPECT_EQ(std::bit_cast<uint32_t>(zone.heightAboveLand), 0x43FA0000u);
}

TEST(PlayerCameraZone, TheResourcesKeepAZoneFileAsItIsRead)
{
	const auto bytes = ZoneFile(1, k_Square);
	resources::CameraZoneManager zones;
	constexpr auto k_Id = entt::hashed_string("camera/zones/Square.exc");
	zones.Load(k_Id.value(), resources::CameraZoneLoader::FromBufferTag {}, bytes);
	ASSERT_TRUE(zones.Contains(k_Id.value()));
	const auto& file = *zones.Handle(k_Id.value());
	EXPECT_TRUE(file.whole);
	EXPECT_TRUE(file.headerRead);
	EXPECT_EQ(file.zone.header, 1);
	EXPECT_EQ(file.zone.drawForceField, 1);
	EXPECT_EQ(file.zone.heightAboveLand, 139.3f);
	ASSERT_EQ(file.zone.forceFieldPoints.size(), 4u);
	EXPECT_EQ(file.zone.forceFieldPoints.at(2), k_Square.at(2));
}

TEST(PlayerCameraZone, ACutFileKeepsWhatCameBeforeTheCut)
{
	const std::vector<uint8_t> noSegment {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	EXPECT_FALSE(player_camera::ReadZoneFile(noSegment).headerRead);
	// a segment cut short keeps what was read before the cut: here all but the record count and size
	auto shortFile = ZoneFile(1, k_Square);
	shortFile.resize(shortFile.size() - 8);
	uint32_t size = 0;
	std::memcpy(&size, shortFile.data() + 40, sizeof(size)); // after "LiOnHeAd" and the segment's 32-byte name
	size -= 8;
	std::memcpy(shortFile.data() + 40, &size, sizeof(size));
	const auto file = player_camera::ReadZoneFile(shortFile);
	EXPECT_FALSE(file.whole);
	EXPECT_EQ(file.zone.header, 1);
	EXPECT_EQ(file.zone.drawForceField, 1);
	EXPECT_EQ(file.zone.forceFieldPoints.size(), 4u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
// No synthetic twin: a sweep over every zone file of the game; ReadsTheSegment covers the reader
TEST(PlayerCameraZone, TheGamesZoneFilesAreReadWhole)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	const auto folder = std::filesystem::path(game) / "Data" / "Zones";
	if (!std::filesystem::is_directory(folder))
	{
		GTEST_SKIP() << "no Data/Zones";
	}
	size_t files = 0;
	for (const auto& entry : std::filesystem::directory_iterator(folder))
	{
		std::ifstream stream(entry.path(), std::ios::binary);
		const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
		const auto file = player_camera::ReadZoneFile(bytes);
		SCOPED_TRACE(entry.path().filename().string());
		EXPECT_TRUE(file.whole);
		EXPECT_EQ(file.zone.header, 1);
		EXPECT_TRUE(file.zone.exclusions.empty());
		++files;
	}
	EXPECT_EQ(files, 9u);
}

TEST(PlayerCameraZone, TheFenceAZoneFileSets)
{
	const auto zone = player_camera::ReadZoneFile(ZoneFile(1, k_Square)).zone;
	const auto cross = [&zone](glm::vec3 p, glm::vec3 dir, bool on = true) {
		return camera_zones::CrossFence(zone.forceFieldPoints, on, p, dir);
	};
	auto crossing = cross({50, 0, 50}, {1, 0, 0});
	EXPECT_TRUE(crossing.inside);
	// the crossing ahead on the edge (100, 0)-(100, 100); every corner is farther, so it stays there
	ExpectNear(crossing.closest, {100, 0, 50});
	ExpectNear(crossing.normal, {-100, 0, 0}); // e = (100, 0) - (100, 100): (e.z, 0, -e.x)
	EXPECT_FALSE(cross({150, 0, 50}, {1, 0, 0}).inside);
	EXPECT_FALSE(cross({-50, 0, 50}, {0, 0, 1}).inside);
	EXPECT_TRUE(cross({0, 0, 0}, {1, 0, 0}).inside); // on a point
	// near a corner, the corner is nearer than the crossing
	crossing = cross({95, 0, 2}, {0, 0, 1});
	EXPECT_TRUE(crossing.inside);
	ExpectNear(crossing.closest, {100, 5, 0});
	// no force field: always inside, at the point itself
	crossing = cross({500, 0, 500}, {1, 0, 0}, false);
	EXPECT_TRUE(crossing.inside);
	EXPECT_EQ(crossing.closest, glm::vec3(500, 0, 500));
}

