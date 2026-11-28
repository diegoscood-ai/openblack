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
#include <functional>
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
#include "Camera/CameraChecks.h"
#include "Camera/CameraModeStack.h"
#include "Camera/CameraZones.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/PlayerCameraScript.h"
#include "Camera/ScriptCamera.h"
#include "Common/EventManager.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "GameClock.h"
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
	const auto heading = static_cast<float>(static_cast<double>(camera_stack::k_DualHeading) - affine::GetYAngle(v));
	position = script_camera::PointFromDistanceHeadingAndPitch(focus, distance, heading, camera_stack::k_DualPitch);
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
	EXPECT_EQ(script_camera::Modes().Size(), 2u); // the script mode and one dual camera
	EXPECT_NEAR(script_camera::Get().modeSeconds, 0.1f, 1e-6f);
	script_camera::StartDual(k_B, k_A, {}, {}); // another order is another mode
	EXPECT_EQ(script_camera::Modes().Size(), 3u);
	EXPECT_TRUE(script_camera::UpdateDual(k_A, k_C)); // SetObjects on the current one
	EXPECT_EQ(script_camera::Modes().CurrentDual()->a, k_A);
	EXPECT_EQ(script_camera::Modes().CurrentDual()->b, k_C);
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
	EXPECT_EQ(script_camera::Modes().CurrentDual()->distanceFactor, script_camera::k_DualPointDistanceFactor);
	script_camera::StartDualWithPoint(k_A, point, {}, {}); // same thing and point: not stacked again
	EXPECT_EQ(script_camera::Modes().Size(), 2u);
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
	           script_camera::PointFromDistanceHeadingAndPitch(focus, distance, camera_stack::k_DualHeading,
	                                                           camera_stack::k_DualPitch));
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
	EXPECT_EQ(script_camera::Modes().Size(), 2u);
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
	ASSERT_EQ(script_camera::Modes().Size(), 2u);
	EXPECT_EQ(script_camera::Modes().Modes().front().kind, camera_stack::StackedMode::Kind::Interaction);
	script_camera::Frame(0.1f, 0, 0.0f);
	// The hold's end takes off the current mode, the dual camera: the interaction's camera is current again and goes on
	// from the zoomers as the dual camera left them
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::InteractionCurrent());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
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
	EXPECT_EQ(script_camera::Modes().Modes().back().dual.a, k_A);
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
	// A new player's mode on top, over the dual camera and the interaction's camera; the script mode, no longer valid,
	// keeps its place until the turn's check, which comes before the hold's end can take the new mode off
	EXPECT_TRUE(script_camera::PlayerOverInteraction());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_FALSE(script_camera::Active());
	EXPECT_TRUE(script_camera::Covered());
	ASSERT_EQ(script_camera::Modes().Size(), 4u);
	std::ignore = script_camera::Validate();
	ASSERT_EQ(script_camera::Modes().Size(), 3u);
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
	// the dual camera, the script mode no longer valid until the turn's check, and the new player's mode
	ASSERT_EQ(script_camera::Modes().Size(), 3u);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
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
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), camera_stack::ModeStack::Popped::None);
	ASSERT_TRUE(script_camera::ReleaseDual());
	script_camera::Frame(0.1f, 16, 0.016f);
	const auto position = script_camera::Get().position.GetCurrentValue();
	ASSERT_EQ(script_camera::TakeOffScriptMode(origin, focus), camera_stack::ModeStack::Popped::ModeUnder);
	EXPECT_FALSE(script_camera::Active());
	EXPECT_FALSE(script_camera::HasMode());
	EXPECT_TRUE(script_camera::Get().focusThing == entt::null);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	// the camera under it goes on from the zoomers as the script mode left them, and the field of view stays
	EXPECT_EQ(origin.GetCurrentValue(), position);
	EXPECT_FLOAT_EQ(script_camera::Get().fov.value, 1.0f);
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), camera_stack::ModeStack::Popped::None);
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
	ASSERT_TRUE(script_camera::Modes().PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(script_camera::ReleaseDual());
	}
	ASSERT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::TakeOffScriptMode(origin, focus), camera_stack::ModeStack::Popped::NewPlayersMode);
	EXPECT_FALSE(script_camera::Modes().PlayersModeGone());
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
	ASSERT_TRUE(script_camera::Modes().PlayersModeGone());
	for (int i = 0; i < 11; ++i)
	{
		ASSERT_TRUE(script_camera::ReleaseDual());
	}
	ASSERT_TRUE(script_camera::InteractionCurrent());
	script_camera::Get().modeSeconds = 1.0f; // the new player's mode's seconds start from 0
	EXPECT_EQ(script_camera::Uncover(origin, focus), camera_stack::ModeStack::Popped::NewPlayersMode);
	EXPECT_FALSE(script_camera::Modes().PlayersModeGone());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	EXPECT_EQ(script_camera::Uncover(origin, focus), camera_stack::ModeStack::Popped::None);
}

namespace
{
/// The placed paths as far as the stack asks of them: their keys, the first the lowest, each with its entry on the
/// camera's stack of modes once placed (Place); whether the top one has the camera, and whether it goes at the turn's
/// camera checks
class StackedPaths final: public ecs::systems::CameraPathSystemInterface
{
public:
	std::vector<uint32_t> placed;
	/// A path placed on top, its entry on top of the stack's other entries
	void Place(uint32_t key)
	{
		placed.push_back(key);
		script_camera::Modes().PushPath(key);
	}
	int deleted {0};
	bool holds {false};
	bool topGoes {false};

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
	bool RemoveInvalid() override
	{
		if (!topGoes || placed.empty())
		{
			return false;
		}
		script_camera::Modes().RemovePath(placed.back());
		placed.pop_back();
		topGoes = false;
		const bool went = holds;
		holds = holds && !placed.empty();
		return went;
	}
	[[nodiscard]] bool HoldsCamera() const override { return holds; }
	void ClearPlaced() override
	{
		for (const auto key : placed)
		{
			script_camera::Modes().RemovePath(key);
		}
		placed.clear();
		holds = false;
	}
	[[nodiscard]] std::size_t PlacedCount() const override { return placed.size(); }
	void SetCovered(bool /*covered*/) override {}
	[[nodiscard]] bool IsCovered() const override { return false; }
	bool TakeOffCurrent() override { return false; }
	void DeleteLowest() override
	{
		script_camera::Modes().RemovePath(placed.front());
		placed.erase(placed.begin());
		++deleted;
	}
	[[nodiscard]] std::optional<PlacedReadout> CurrentPlaced() const override { return std::nullopt; }
	void HandlePlayerControl(const PlayerControl& /*control*/) override {}
};

/// Creature Mode's follow as far as the stack asks of it: whether it is on, with its entry on the camera's stack of modes
/// once locked on (LockOn), its end; whether it has the camera's view, and whether it goes at the turn's camera checks
class StackedFollow final: public ecs::systems::CreatureModeSystemInterface
{
public:
	bool active {false};
	int leaves {0};
	/// Locked on, its entry on top of the stack's other entries
	void LockOn()
	{
		active = true;
		script_camera::Modes().PushFollow();
	}
	bool hasView {false};
	bool goes {false};

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
		// as Creature Mode's, the follow and its view go
		active = false;
		hasView = false;
		script_camera::Modes().RemoveFollow();
		++leaves;
	}
	void LockOnNext(entt::entity /*creature*/) override {}
	[[nodiscard]] bool IsActive() const override { return active; }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override { return std::nullopt; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override { return std::nullopt; }
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override
	{
		return hasView ? std::optional(creature_follow::View {}) : std::nullopt;
	}
	[[nodiscard]] std::optional<creature_follow::Style> GetStyle() const override { return std::nullopt; }
	void ClearView() override {}
	bool RemoveInvalid() override
	{
		if (!goes || !active)
		{
			return false;
		}
		const bool went = hasView;
		active = false;
		script_camera::Modes().RemoveFollow();
		hasView = false;
		goes = false;
		return went;
	}
};

/// The turn's camera checks with fakes of the placed paths and of the follow, a camera, and the player's camera's
/// restarts counted
struct CameraChecksWorld
{
	test::RestoreService<Locator::camera> restoreCamera;
	test::RestoreService<Locator::events> restoreEvents;
	test::RestoreService<Locator::cameraPathSystem> restorePaths;
	test::RestoreService<Locator::creatureModeSystem> restoreFollow;
	Camera* camera {nullptr};
	StackedPaths* paths {nullptr};
	StackedFollow* follow {nullptr};
	int restarts {0};

	CameraChecksWorld()
	{
		camera = &Locator::camera::emplace(glm::vec3(0.0f));
		camera->GetOriginZoomer().SetPosition({900, 60, 900});
		camera->GetFocusZoomer().SetPosition({1000, 0, 1000});
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler(
		    std::function([this](const ecs::events::PlayerCameraRestarted& /*restarted*/) { ++restarts; }));
		paths = &static_cast<StackedPaths&>(Locator::cameraPathSystem::emplace<StackedPaths>());
		follow = &static_cast<StackedFollow&>(Locator::creatureModeSystem::emplace<StackedFollow>());
	}
};
} // namespace

TEST(CameraChecksPass, ADualCameraGoneOverAPathLeavesThePathTheCameraWithNoRestartOfThePlayersCamera)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	CameraChecksWorld world;
	world.paths->placed = {1};
	world.paths->holds = true;
	script_camera::StartDual(k_A, k_B, world.camera->GetOriginZoomer(), world.camera->GetFocusZoomer());
	script_camera::Frame(0.1f, 0, 0.0f);
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dual);
	t.things[k_B].available = false;
	camera_checks::CheckModes();
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Path);
	// The dual camera's cleanup: the zoomers handed back as it left them, for the path to go on from
	EXPECT_EQ(world.camera->GetOriginZoomer().GetCurrentValue(), script_camera::Get().position.GetCurrentValue());
	EXPECT_EQ(world.restarts, 0);
}

TEST(CameraChecksPass, AReleasedDualCameraOverAPathLeavesThePathTheCameraWithNoRestartOfThePlayersCamera)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	CameraChecksWorld world;
	world.paths->placed = {1};
	world.paths->holds = true;
	script_camera::StartDual(k_A, k_B, world.camera->GetOriginZoomer(), world.camera->GetFocusZoomer());
	script_camera::Frame(0.1f, 0, 0.0f);
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dual);
	EXPECT_TRUE(script_camera::ReleaseDual());
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Path);
	// the zoomers handed back as the dual camera left them, for the path to go on from, and nothing restarted
	EXPECT_EQ(world.camera->GetOriginZoomer().GetCurrentValue(), script_camera::Get().position.GetCurrentValue());
	EXPECT_EQ(world.restarts, 0);
}

// The mode's time is one count for the stack: a dual camera over the follow zeroes it and it runs on while the dual
// camera has the view; the turn's check that takes the dual camera off does not zero it, so the follow goes on from it
TEST(CameraChecksPass, ADualCameraGoneOverTheFollowLeavesTheFollowTheModesTimeAsItRanOn)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	std::ignore = world.camera->SetModel(std::make_unique<CreatureCameraModel>(
	    glm::vec3(900, 60, 900), glm::vec3(1000, 0, 1000), glm::vec3(1000, 0, 1000), 15.0f));
	script_camera::StartDual(k_A, k_B, world.camera->GetOriginZoomer(), world.camera->GetFocusZoomer());
	for (int frame = 0; frame < 5; ++frame)
	{
		script_camera::Frame(0.1f, 0, 0.0f);
	}
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dual);
	const float ranOn = script_camera::Get().modeSeconds;
	ASSERT_GT(ranOn, 0.0f);
	t.things[k_B].available = false;
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Follow);
	const auto* follow = dynamic_cast<const CreatureCameraModel*>(&world.camera->GetModel());
	ASSERT_NE(follow, nullptr);
	EXPECT_EQ(follow->GetSecondsInMode(), ranOn);
	EXPECT_EQ(world.restarts, 0);
}

// The map's clear takes every mode off the stack, the follow and the placed paths with the script camera's, with no
// fly-back of the player's camera
TEST(CameraChecksPass, TheMapsClearTakesTheFollowAndThePathsOffWithTheScriptCamerasModes)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	world.paths->placed = {1, 2};
	world.paths->holds = true;
	script_camera::StartDual(k_A, k_B, world.camera->GetOriginZoomer(), world.camera->GetFocusZoomer());
	camera_checks::ClearStack(false);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_TRUE(world.paths->placed.empty());
	EXPECT_FALSE(world.paths->HoldsCamera());
	EXPECT_FALSE(world.follow->IsActive());
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.restarts, 0);
}

TEST(CameraChecksPass, APathGoneOverTheFollowRestartsNothing)
{
	Things t;
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	world.paths->placed = {2};
	world.paths->holds = true;
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Path);
	world.paths->topGoes = true;
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Follow);
	EXPECT_EQ(world.restarts, 0);
}

TEST(CameraChecksPass, APathAndTheFollowUnderItGoneInOneTurnRestartThePlayersCameraOnce)
{
	Things t;
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	world.follow->goes = true;
	world.paths->placed = {2};
	world.paths->holds = true;
	world.paths->topGoes = true;
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.restarts, 1);
}

TEST(CameraChecksPass, ADualCameraAndThePathUnderItGoneInOneTurnRestartThePlayersCameraOnce)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	CameraChecksWorld world;
	world.paths->placed = {1};
	world.paths->holds = true;
	script_camera::StartDual(k_A, k_B, world.camera->GetOriginZoomer(), world.camera->GetFocusZoomer());
	t.things[k_A].available = false;
	world.paths->topGoes = true;
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.restarts, 1);
	// and nothing is left for a later check
	camera_checks::CheckModes();
	EXPECT_EQ(world.restarts, 1);
}

TEST(CameraChecksPass, AFollowGoneUnderAPathRestartsNothingUntilThePathGoes)
{
	Things t;
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	world.follow->goes = true;
	world.paths->placed = {2};
	world.paths->holds = true;
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Path);
	EXPECT_EQ(world.restarts, 0);
	world.paths->topGoes = true;
	camera_checks::CheckModes();
	EXPECT_EQ(world.restarts, 1);
}

TEST(CameraChecksPass, AFollowGoneWithTheCameraRestartsThePlayersCamera)
{
	Things t;
	CameraChecksWorld world;
	world.follow->active = true;
	world.follow->hasView = true;
	world.follow->goes = true;
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Follow);
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.restarts, 1);
}

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
	paths.Place(1);
	follow.LockOn();
	paths.Place(3);
	Zoomer3 origin;
	Zoomer3 focus;
	// With the player's own mode, the three and eight dual cameras fill the stack; the ninth deletes the player's mode
	for (int i = 0; i < 9; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Modes().PlayersModeGone());
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
	EXPECT_EQ(script_camera::Modes().Size(), 12u);
	script_camera::StartDualWithPoint(k_A, {103, 0, 0}, origin, focus);
	EXPECT_EQ(script_camera::Modes().Size(), 12u);
	EXPECT_EQ(paths.deleted, 2);
	EXPECT_EQ(follow.leaves, 1);
}

TEST(ScriptCameraDual, AScriptModeOverAFullStackDeletesTheLowestFollowThroughCreatureMode)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	const test::RestoreService<Locator::creatureModeSystem> restoreFollow;
	Locator::cameraPathSystem::reset();
	auto& follow = static_cast<StackedFollow&>(Locator::creatureModeSystem::emplace<StackedFollow>());
	follow.LockOn();
	Zoomer3 origin;
	Zoomer3 focus;
	// Dual cameras over the follow until the player's own mode is deleted and the stack is full again
	for (int i = 0; i < 11; ++i)
	{
		script_camera::StartDualWithPoint(k_A, {static_cast<float>(i), 0, 0}, origin, focus);
	}
	ASSERT_TRUE(script_camera::Modes().PlayersModeGone());
	EXPECT_EQ(follow.leaves, 0);
	// The script mode going on deletes the follow, the lowest, through Creature Mode: no place is kept for it
	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	EXPECT_EQ(follow.leaves, 1);
	ASSERT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_FALSE(script_camera::Modes().FollowOnStack());
	EXPECT_EQ(script_camera::Modes().Places(), camera_stack::ModeStack::k_Places);
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
	// with the script mode, no longer valid, in its place until the turn's check
	ASSERT_EQ(script_camera::Modes().Size(), 3u);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
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
	EXPECT_EQ(script_camera::Modes().Size(), 3u);
	EXPECT_TRUE(script_camera::Get().focusThing == entt::null); // a new script mode follows nothing
	EXPECT_TRUE(script_camera::End());
	// The first script mode and the dual camera stay under the new player's mode: neither has the camera; the ended
	// script mode keeps its place until the turn's check
	ASSERT_EQ(script_camera::Modes().Size(), 4u);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
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
	EXPECT_EQ(script_camera::Modes().CurrentDual()->b, k_B);
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
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
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
	script_camera::ApplyShake(camera, camera.GetOrigin(), camera.GetFocus());
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_LT(camera_shake::Checkers().front().remainingMs, 1000);
	camera_shake::Reset();
}

// While the falling spell's path camera is drawn, the game camera's draw has no shake of its own: the path's camera
// takes it in its place (its draws of the random stream once a frame, not twice); the shakes still tick once a frame
TEST(CameraShake, TheGameCameraIsNotShakenWhileTheFallingSpellsPathCameraIsDrawn)
{
	camera_shake::Reset();
	camera_shake::Create(500.0f, {0, 0, 0}, 1.0f, 1000, false);
	const auto sequenceBefore = game_clock::SequenceMode();
	game_clock::SetSequenceMode(game_clock::k_SequenceModeFallingSpell);
	Camera camera;
	camera.GetOriginZoomer().SetPosition({0.0f, 50.0f, 0.0f});
	camera.GetFocusZoomer().SetPosition({0.0f, 0.0f, 50.0f});
	const auto origin = camera.GetOriginZoomer().GetCurrentValue();
	script_camera::ApplyShake(camera, origin, camera.GetFocus());
	EXPECT_EQ(camera.GetOrigin(), origin);
	ASSERT_EQ(camera_shake::Checkers().size(), 1u);
	EXPECT_LT(camera_shake::Checkers().front().remainingMs, 1000);

	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	script_camera::ApplyShake(camera, origin, camera.GetFocus());
	EXPECT_NE(camera.GetOrigin(), origin);
	game_clock::SetSequenceMode(sequenceBefore);
	camera_shake::Reset();
}

// While the falling spell runs, the camera's update compares the camera drawn the frame before (the path's) with
// itself held over the land, not with the game camera, and reads the weather there; outside it, the game camera's own
// held place is compared and the weather is read at the camera's position
TEST(CameraShake, WhileTheFallingSpellRunsTheCamerasUpdateReadsTheDrawnCamera)
{
	camera_shake::Reset();
	const auto sequenceBefore = game_clock::SequenceMode();
	Camera camera;
	camera.GetOriginZoomer().SetPosition({0.0f, 50.0f, 0.0f});
	camera.GetFocusZoomer().SetPosition({0.0f, 0.0f, 50.0f});
	const glm::vec3 path {300.0f, 80.0f, -20.0f};
	const glm::vec3 pathFocus {310.0f, 70.0f, 0.0f};

	game_clock::SetSequenceMode(game_clock::k_SequenceModeFallingSpell);
	script_camera::ApplyShake(camera, path, pathFocus);
	EXPECT_FALSE(script_camera::Get().drawnMoved);
	EXPECT_EQ(script_camera::Get().drawnBefore, path);
	EXPECT_EQ(camera.GetWeatherPoint(), path);
	// the game camera's own position is not drawn, and stays as it was
	EXPECT_EQ(camera.GetOrigin(), glm::vec3(0.0f, 50.0f, 0.0f));

	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	script_camera::ApplyShake(camera, path, pathFocus);
	EXPECT_TRUE(script_camera::Get().drawnMoved);
	EXPECT_EQ(camera.GetWeatherPoint(), camera.GetOrigin());
	game_clock::SetSequenceMode(sequenceBefore);
	camera_shake::Reset();
}

// The debug camera on a drawn camera: mode 2 puts the position and the focus, 1 the focus only, 0 neither
TEST(DebugCamera, ModeTwoMovesThePositionAndTheFocusAndModeOneTheFocus)
{
	script_camera::SetDebugCameraPosition({1.0f, 2.0f, 3.0f});
	script_camera::SetDebugCameraFocus({4.0f, 5.0f, 6.0f});
	for (const auto mode : {0, 1, 2})
	{
		script_camera::SetDebugCameraMode(mode);
		glm::vec3 position {10.0f};
		glm::vec3 focus {20.0f};
		script_camera::ApplyDebugCamera(position, focus);
		EXPECT_EQ(position, mode == 2 ? glm::vec3(1.0f, 2.0f, 3.0f) : glm::vec3(10.0f)) << mode;
		EXPECT_EQ(focus, mode == 0 ? glm::vec3(20.0f) : glm::vec3(4.0f, 5.0f, 6.0f)) << mode;
	}
	script_camera::SetDebugCameraMode(0);
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

// In the normal game the weather at the camera is read at the game camera held over the land, as it is given to be
// drawn: the shake moves only what is drawn, never that point
TEST(CameraShake, TheWeatherAtTheCameraIsReadAtTheHeldCameraNotTheShakenOne)
{
	camera_shake::Reset();
	camera_shake::Create(500.0f, {0, 0, 0}, 4.0f, 1000, false);
	camera_shake::Tick(500);
	const auto sequenceBefore = game_clock::SequenceMode();
	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	Camera camera;
	camera.GetOriginZoomer().SetPosition({0.0f, 50.0f, 0.0f});
	camera.GetFocusZoomer().SetPosition({0.0f, 0.0f, 50.0f});
	const auto held = camera.GetOriginZoomer().GetCurrentValue();
	script_camera::ApplyShake(camera, held, camera.GetFocus());
	// the drawn camera is shaken, the weather's point is not
	EXPECT_NE(camera.GetOrigin(), held);
	EXPECT_EQ(camera.GetWeatherPoint(), held);
	game_clock::SetSequenceMode(sequenceBefore);
	camera_shake::Reset();
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

namespace
{
/// The turn's camera checks of the dance camera, with a dance in a registry of its own
struct DanceCameraWorld
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	CameraChecksWorld checks;
	ecs::Registry* registry {nullptr};
	entt::entity dance {entt::null};

	DanceCameraWorld()
	{
		registry = &Locator::entitiesRegistry::emplace<ecs::Registry>();
		dance = registry->Create();
		registry->Assign<ecs::components::Dance>(dance, ecs::components::Dance {.useCamera = true, .cameraCalled = true});
	}
};
} // namespace

// The dance camera goes on top of the player's mode and holds the view: the frame step moves no zoomer of its own. It
// is refused, with nothing done, while a script's camera mode still goes
TEST(DanceCamera, ItGoesOnTopAndHoldsTheViewAndIsRefusedWhileTheCurrentModeCannotBeLeft)
{
	Things t;
	DanceCameraWorld world;
	auto& camera = *world.checks.camera;
	ASSERT_TRUE(camera_checks::MakeDanceCamera(world.dance));
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dance);
	EXPECT_EQ(camera_checks::CurrentFrameUpdate(false), camera_checks::FrameUpdate::Dance);
	const auto origin = camera.GetOriginZoomer().GetCurrentValue();
	camera_checks::UpdateDanceCamera(camera, std::chrono::milliseconds(100), 3);
	EXPECT_EQ(camera.GetOriginZoomer().GetCurrentValue(), origin);
	camera_checks::CheckModes(); // still valid: it stays
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dance);
	EXPECT_EQ(world.checks.restarts, 0);
	camera_checks::ClearStack(false);

	ASSERT_TRUE(script_camera::Begin({900, 50, 900}, {1000, 0, 1000}));
	ASSERT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Script);
	EXPECT_FALSE(camera_checks::MakeDanceCamera(world.dance));
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Script);
	EXPECT_FALSE(script_camera::Modes().CurrentDance().has_value());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 0u);
}

// The hand gripping the land ends it: it stays the current mode until the turn's check, which deletes it and restarts
// the player's camera
TEST(DanceCamera, TheHandGrippingTheLandEndsItAtTheTurnsCheck)
{
	Things t;
	DanceCameraWorld world;
	ASSERT_TRUE(camera_checks::MakeDanceCamera(world.dance));
	camera_checks::UpdateDanceCamera(*world.checks.camera, std::chrono::milliseconds(100), 20);
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Dance);
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.checks.restarts, 1);
}

// Its dance gone, or marked to go, the turn's check deletes it
TEST(DanceCamera, ItsDanceGoneOrMarkedToGoTheTurnsCheckDeletesIt)
{
	Things t;
	DanceCameraWorld world;
	ASSERT_TRUE(camera_checks::MakeDanceCamera(world.dance));
	world.registry->Assign<ecs::components::Unavailable>(world.dance);
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.checks.restarts, 1);

	world.registry->Remove<ecs::components::Unavailable>(world.dance);
	ASSERT_TRUE(camera_checks::MakeDanceCamera(world.dance));
	world.registry->Destroy(world.dance);
	camera_checks::UpdateDanceCamera(*world.checks.camera, std::chrono::milliseconds(100), 3);
	camera_checks::CheckModes();
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.checks.restarts, 2);
}

// Its dance no longer asking for the camera clears the camera's whole stack, a dual camera under it with it, and
// restarts nothing
TEST(DanceCamera, ItsDanceNoLongerAskingForTheCameraClearsTheStack)
{
	Things t;
	t.things[k_A] = MakeThing({1000, 0, 1000}, 2.0f, std::nullopt);
	t.things[k_B] = MakeThing({1100, 0, 1000}, 2.0f, std::nullopt);
	DanceCameraWorld world;
	script_camera::StartDual(k_A, k_B, world.checks.camera->GetOriginZoomer(), world.checks.camera->GetFocusZoomer());
	ASSERT_TRUE(camera_checks::MakeDanceCamera(world.dance));
	EXPECT_FALSE(script_camera::DualCurrent());
	world.registry->Get<ecs::components::Dance>(world.dance).useCamera = false;
	camera_checks::UpdateDanceCamera(*world.checks.camera, std::chrono::milliseconds(100), 3);
	EXPECT_FALSE(script_camera::Modes().CurrentDance().has_value());
	EXPECT_EQ(script_camera::Modes().Size(), 0u);
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	EXPECT_EQ(world.checks.restarts, 0);
}
