/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script functions that read and keep the camera, as a script calls them: GET_CAMERA_POSITION and GET_CAMERA_FOCUS
// give the camera as it is drawn, shake included; STORE_CAMERA_DETAILS keeps that drawn position and focus;
// GET_STORED_CAMERA_POSITION and GET_STORED_CAMERA_FOCUS give back what was kept, and RESTORE_CAMERA_DETAILS puts the
// camera there in the player's own mode too. Each point goes on the script's stack as x, then y, then z.
// GET_FACING_CAMERA_POSITION gives a point a distance along the camera's view; SET_CAMERA_LENS, SET_CAMERA_POS_FOC_LENS
// and MOVE_CAMERA_POS_FOC_LENS set the lens, and SET_FOCUS_FOLLOW keeps the view on a thing.
// docs/wiki/camera/script-camera.md#opcodes

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/DrawListRequests.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
/// Where the camera is drawn from and what it looks at, before the shake moves both
constexpr glm::vec3 k_Origin {1873.5f, 141.25f, 2535.75f};
constexpr glm::vec3 k_Focus {1900.0f, 40.5f, 2600.25f};
/// The shake's offsets of the drawn position and focus this frame
constexpr glm::vec3 k_OriginShake {0.5f, -0.25f, 1.0f};
constexpr glm::vec3 k_FocusShake {-1.0f, 0.5f, 0.25f};

class ScriptCameraDetails: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		Locator::vm::emplace<lhvm::LHVM>();
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		_camera->SetOrigin(k_Origin).SetFocus(k_Focus);
		_camera->SetDrawOffset(k_OriginShake, k_FocusShake);
		script_camera::Reset();
	}
	void TearDown() override { script_camera::Reset(); }

	/// Calls a native that takes nothing and leaves `pushed` values on the stack
	void Call(std::string_view name, unsigned pushed)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		ASSERT_NE(native, table.end()) << name;
		ASSERT_EQ(native->stackIn, 0) << name;
		ASSERT_EQ(native->stackOut, pushed) << name;
		native->impl();
	}

	/// The native `name`, checked to take `in` values and leave `out`
	const lhvm::NativeFunction* Native(std::string_view name, int in, unsigned out)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		EXPECT_NE(native, table.end()) << name;
		if (native == table.end())
		{
			return nullptr;
		}
		EXPECT_EQ(native->stackIn, in) << name;
		EXPECT_EQ(native->stackOut, out) << name;
		return &*native;
	}

	/// Calls a native that takes the `floats`, pushed in order, and leaves `pushed` values on the stack
	void CallWith(std::string_view name, const std::vector<float>& floats, unsigned pushed)
	{
		const auto* native = Native(name, static_cast<int>(floats.size()), pushed);
		ASSERT_NE(native, nullptr);
		auto& vm = Locator::vm::value();
		for (const auto value : floats)
		{
			vm.Pushf(value);
		}
		native->impl();
	}

	/// Takes a point off the stack: z comes off first
	static glm::vec3 PopPoint()
	{
		auto& vm = Locator::vm::value();
		const float z = vm.Popf();
		const float y = vm.Popf();
		const float x = vm.Popf();
		return {x, y, z};
	}

	/// Calls a native that gives a point, and takes the point off the stack: z comes off first
	glm::vec3 CallForPoint(std::string_view name)
	{
		Call(name, 3);
		auto& vm = Locator::vm::value();
		const float z = vm.Popf();
		const float y = vm.Popf();
		const float x = vm.Popf();
		return {x, y, z};
	}

	Camera* _camera {nullptr};
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::camera> _restoreCamera;
	chlapi::CHLApi _api;
	test::DrawListRequests _requests;
};
} // namespace

TEST_F(ScriptCameraDetails, TheCameraIsReadAsItIsDrawnShakeIncluded)
{
	EXPECT_EQ(CallForPoint("GET_CAMERA_POSITION"), k_Origin + k_OriginShake);
	EXPECT_EQ(CallForPoint("GET_CAMERA_FOCUS"), k_Focus + k_FocusShake);
}

TEST_F(ScriptCameraDetails, StoringKeepsTheDrawnPositionAndFocusUntilTheNextStore)
{
	Call("STORE_CAMERA_DETAILS", 0);
	// The camera moves on and stops shaking: what was kept stays
	_camera->SetOrigin({100.0f, 200.0f, 300.0f}).SetFocus({400.0f, 0.0f, 600.0f});
	_camera->SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	EXPECT_EQ(CallForPoint("GET_STORED_CAMERA_POSITION"), k_Origin + k_OriginShake);
	EXPECT_EQ(CallForPoint("GET_STORED_CAMERA_FOCUS"), k_Focus + k_FocusShake);
	// Read again, the same
	EXPECT_EQ(CallForPoint("GET_STORED_CAMERA_POSITION"), k_Origin + k_OriginShake);

	// A second store keeps the camera as it is now
	Call("STORE_CAMERA_DETAILS", 0);
	EXPECT_EQ(CallForPoint("GET_STORED_CAMERA_POSITION"), glm::vec3(100.0f, 200.0f, 300.0f));
	EXPECT_EQ(CallForPoint("GET_STORED_CAMERA_FOCUS"), glm::vec3(400.0f, 0.0f, 600.0f));
}

TEST_F(ScriptCameraDetails, RestoringPutsThePlayersCameraWhereItWasStored)
{
	Call("STORE_CAMERA_DETAILS", 0);
	_camera->SetOrigin({100.0f, 200.0f, 300.0f}).SetFocus({400.0f, 0.0f, 600.0f});
	_camera->SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	// No script has the camera: the restore sets it all the same, at once
	ASSERT_FALSE(script_camera::Active());
	Call("RESTORE_CAMERA_DETAILS", 0);
	EXPECT_EQ(_camera->GetOrigin(), k_Origin + k_OriginShake);
	EXPECT_EQ(_camera->GetFocus(), k_Focus + k_FocusShake);
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), k_Origin + k_OriginShake);
	EXPECT_EQ(script_camera::Get().focus.GetCurrentValue(), k_Focus + k_FocusShake);
}

TEST_F(ScriptCameraDetails, SettingTheCameraWhereItIsDrawnLeavesAGlideGoing)
{
	// a glide of the script's position under way, from where the camera is drawn
	const glm::vec3 drawn = k_Origin + k_OriginShake;
	const glm::vec3 drawnFocus = k_Focus + k_FocusShake;
	script_camera::Get().position.SetPosition(drawn);
	script_camera::MovePosition({0.0f, 50.0f, 0.0f}, 2.0f);
	ASSERT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(0.0f, 50.0f, 0.0f));
	// set where it is drawn: nothing changes
	script_camera::SetPositionAndFocus(drawn, drawnFocus);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(0.0f, 50.0f, 0.0f));
	// anywhere else: the zoomers are set there at once
	script_camera::SetPositionAndFocus(drawn + glm::vec3(1.0f, 0.0f, 0.0f), drawnFocus);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), drawn + glm::vec3(1.0f, 0.0f, 0.0f));
	// a part that is not a number compares as equal
	EXPECT_TRUE(script_camera::DrawnThereAlready({std::nanf(""), 1.0f, 2.0f}, {5.0f, 1.0f, 2.0f}));
	EXPECT_FALSE(script_camera::DrawnThereAlready({4.0f, 1.0f, 2.0f}, {5.0f, 1.0f, 2.0f}));
}

TEST_F(ScriptCameraDetails, AFacingPositionIsTheDistanceAlongTheView)
{
	// The camera looks along z: the point is the distance ahead of it, or behind it for a negative distance
	_camera->SetOrigin({100.0f, 50.0f, 200.0f}).SetFocus({100.0f, 50.0f, 264.0f});
	_camera->SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	CallWith("GET_FACING_CAMERA_POSITION", {16.0f}, 3);
	EXPECT_EQ(PopPoint(), glm::vec3(100.0f, 50.0f, 216.0f));
	CallWith("GET_FACING_CAMERA_POSITION", {-32.0f}, 3);
	EXPECT_EQ(PopPoint(), glm::vec3(100.0f, 50.0f, 168.0f));
	// Looking along x, the view as it is drawn, shake included
	_camera->SetOrigin({100.0f, 50.0f, 200.0f}).SetFocus({227.0f, 50.0f, 200.0f});
	_camera->SetDrawOffset(glm::vec3(0.0f), {1.0f, 0.0f, 0.0f});
	CallWith("GET_FACING_CAMERA_POSITION", {64.0f}, 3);
	EXPECT_EQ(PopPoint(), glm::vec3(164.0f, 50.0f, 200.0f));
}

TEST_F(ScriptCameraDetails, AFacingPositionGoesAlongTheViewDirectionAsTheOriginalRoundsIt)
{
	// The point is the drawn position plus the distance times the camera's unit view direction, one rounding per
	// product and sum. Looking straight down (or with the focus on the position) the direction's x is pushed a little
	// off zero first, keeping its sign (0 counting as negative), so the point moves a tiny way along x. Each case's
	// numbers are the original's, every value as its bits: position, focus, distance and the point pushed.
	struct Case
	{
		std::array<uint32_t, 3> origin;
		std::array<uint32_t, 3> focus;
		uint32_t distance;
		std::array<uint32_t, 3> point;
	};
	constexpr std::array<Case, 7> k_Cases {{
	    // a camera over the land looking down at a slant, 16 m
	    {{0x44EA3000u, 0x430D4000u, 0x451E7C00u},
	     {0x44ED8000u, 0x42220000u, 0x45228400u},
	     0x41800000u,
	     {0x44EA9EBCu, 0x43001803u, 0x451F02C3u}},
	    // another slant, 1234.567 m
	    {{0x451C44CDu, 0x42BF6666u, 0x44E10333u},
	     {0x45232E66u, 0x41466666u, 0x44DCA666u},
	     0x449A5225u,
	     {0x455808B5u, 0xC41C20A6u, 0x44BB4B5Cu}},
	    // straight down with the focus a hair along +x: x pushed to +1e-4 before normalising, 50 m
	    {{0x41200000u, 0x43960000u, 0x41200000u},
	     {0x41200034u, 0x00000000u, 0x41200000u},
	     0x42480000u,
	     {0x41200011u, 0x437A0000u, 0x41200000u}},
	    // the focus on the position: the direction is -x, 25 m
	    {{0x43960000u, 0x42F00000u, 0x43C80000u},
	     {0x43960000u, 0x42F00000u, 0x43C80000u},
	     0x41C80000u,
	     {0x43898000u, 0x42F00000u, 0x43C80000u}},
	    // x a hair off zero but looking along z: no push, 70 m
	    {{0x40A00000u, 0x42480000u, 0x40A00000u},
	     {0x40A00015u, 0x42200000u, 0x42B40000u},
	     0x428C0000u,
	     {0x40A00011u, 0x422748D2u, 0x42950A85u}},
	    // straight down from 300 m: x pushed to -1e-4, 50 m
	    {{0x40000000u, 0x43960000u, 0x40400000u},
	     {0x40000000u, 0x00000000u, 0x40400000u},
	     0x42480000u,
	     {0x3FFFFF74u, 0x437A0000u, 0x40400000u}},
	    // straight down from 1 m: x pushed to -1e-4, 100 m
	    {{0x447A0000u, 0x3F800000u, 0x447A0000u},
	     {0x447A0000u, 0x00000000u, 0x447A0000u},
	     0x42C80000u,
	     {0x4479FF5Cu, 0xC2C60000u, 0x447A0000u}},
	}};
	_camera->SetDrawOffset(glm::vec3(0.0f), glm::vec3(0.0f));
	const auto point = [](const std::array<uint32_t, 3>& bits) {
		return glm::vec3 {std::bit_cast<float>(bits[0]), std::bit_cast<float>(bits[1]), std::bit_cast<float>(bits[2])};
	};
	for (const auto& c : k_Cases)
	{
		_camera->SetOrigin(point(c.origin)).SetFocus(point(c.focus));
		CallWith("GET_FACING_CAMERA_POSITION", {std::bit_cast<float>(c.distance)}, 3);
		const auto pushed = PopPoint();
		EXPECT_EQ(std::bit_cast<uint32_t>(pushed.x), c.point[0]) << std::hex << c.origin[0];
		EXPECT_EQ(std::bit_cast<uint32_t>(pushed.y), c.point[1]) << std::hex << c.origin[0];
		EXPECT_EQ(std::bit_cast<uint32_t>(pushed.z), c.point[2]) << std::hex << c.origin[0];
	}
}

TEST_F(ScriptCameraDetails, ResettingTheLensTakesItsArgumentAsTheTime)
{
	auto& fov = script_camera::Get().fov;
	script_camera::SetFov(0.5f, 0.0f);
	// The one argument is how long the lens takes to go back to 70 degrees
	CallWith("SET_CAMERA_LENS", {2.0f}, 0);
	EXPECT_EQ(fov.destination, script_camera::k_DefaultFov);
	EXPECT_EQ(fov.duration, 2.0f);
	EXPECT_EQ(fov.value, 0.5f);
	// No time: back at once
	CallWith("SET_CAMERA_LENS", {0.0f}, 0);
	EXPECT_EQ(fov.value, script_camera::k_DefaultFov);
}

TEST_F(ScriptCameraDetails, SettingPositionFocusAndLensTakesTheLensUnconverted)
{
	// The player's camera is put there too, and the lens is set at once to the number as it is, not turned from degrees
	ASSERT_FALSE(script_camera::Active());
	CallWith("SET_CAMERA_POS_FOC_LENS", {10.0f, 20.0f, 30.0f, 40.0f, 5.0f, 60.0f, 1.5f}, 0);
	EXPECT_EQ(_camera->GetOrigin(), glm::vec3(10.0f, 20.0f, 30.0f) + k_OriginShake);
	EXPECT_EQ(_camera->GetFocus(), glm::vec3(40.0f, 5.0f, 60.0f) + k_FocusShake);
	EXPECT_EQ(script_camera::Get().position.GetDestination(), glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_EQ(script_camera::Get().focus.GetDestination(), glm::vec3(40.0f, 5.0f, 60.0f));
	EXPECT_EQ(script_camera::Get().fov.value, 1.5f);
	EXPECT_EQ(script_camera::Get().fov.destination, 1.5f);
}

TEST_F(ScriptCameraDetails, MovingPositionFocusAndLensGlidesAllThreeOverTheTimeInTheScriptsCameraOnly)
{
	auto& state = script_camera::Get();
	const float lens = state.fov.value;
	// The player's camera: nothing moves
	CallWith("MOVE_CAMERA_POS_FOC_LENS", {10.0f, 20.0f, 30.0f, 40.0f, 5.0f, 60.0f, 1.5f, 3.0f}, 0);
	EXPECT_EQ(state.fov.destination, lens);
	EXPECT_NE(state.position.GetDestination(), glm::vec3(10.0f, 20.0f, 30.0f));

	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	CallWith("MOVE_CAMERA_POS_FOC_LENS", {10.0f, 20.0f, 30.0f, 40.0f, 5.0f, 60.0f, 1.5f, 3.0f}, 0);
	EXPECT_EQ(state.position.GetDestination(), glm::vec3(10.0f, 20.0f, 30.0f));
	EXPECT_EQ(state.position.GetCurrentValue(), glm::vec3(0.0f, 10.0f, 0.0f));
	EXPECT_EQ(state.position.axis[0].duration, 3.0f);
	EXPECT_EQ(state.focus.GetDestination(), glm::vec3(40.0f, 5.0f, 60.0f));
	EXPECT_EQ(state.focus.axis[2].duration, 3.0f);
	// The lens as it is, over the same time
	EXPECT_EQ(state.fov.destination, 1.5f);
	EXPECT_EQ(state.fov.duration, 3.0f);
	EXPECT_EQ(state.fov.value, lens);
}

TEST_F(ScriptCameraDetails, TheFocusFollowsAThingAsItMovesInTheScriptsCameraOnly)
{
	test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	// a script's object 0 is no object, so the thing followed is not the registry's first
	[[maybe_unused]] const auto notFollowed = registry.Create();
	const auto thing = registry.Create();
	// The thing as the camera reads it: drawn 5 m east of where it stands, 2 m tall
	std::map<entt::entity, script_camera::ThingInfo> things;
	const auto place = [&things, thing](const glm::vec3& at) {
		script_camera::ThingInfo info;
		info.mapPoint = at;
		info.height = 2.0f;
		info.drawnPoint = at + glm::vec3(5.0f, 0.0f, 0.0f);
		things[thing] = info;
	};
	place({300.0f, 10.0f, 300.0f});
	script_camera::detail::SetThingReaderForTests([&things](entt::entity e) -> std::optional<script_camera::ThingInfo> {
		const auto it = things.find(e);
		return it != things.end() ? std::optional(it->second) : std::nullopt;
	});
	// The things are read from the registry again when the test ends, also after a failed assertion
	struct ReaderReset
	{
		ReaderReset() = default;
		~ReaderReset() { script_camera::detail::SetThingReaderForTests(nullptr); }
		ReaderReset(const ReaderReset&) = delete;
		ReaderReset& operator=(const ReaderReset&) = delete;
		ReaderReset(ReaderReset&&) = delete;
		ReaderReset& operator=(ReaderReset&&) = delete;
	} readerReset;
	const auto* native = Native("SET_FOCUS_FOLLOW", 1, 0);
	ASSERT_NE(native, nullptr);
	const auto follow = [native, thing]() {
		Locator::vm::value().Pusho(static_cast<uint32_t>(thing));
		native->impl();
	};
	auto& state = script_camera::Get();

	// The player's camera: nothing is followed
	follow();
	EXPECT_TRUE(state.focusThing == entt::null);

	ASSERT_TRUE(script_camera::Begin({100.0f, 50.0f, 100.0f}, {200.0f, 0.0f, 100.0f}));
	follow();
	EXPECT_EQ(state.focusThing, thing);
	// Not put there at once
	EXPECT_EQ(state.focus.GetCurrentValue(), glm::vec3(200.0f, 0.0f, 100.0f));
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto first = state.focus.GetDestination();
	EXPECT_NEAR(first.x, 305.0f, 1e-3f);
	EXPECT_NEAR(first.y, 11.0f, 1e-3f);
	EXPECT_NEAR(first.z, 300.0f, 1e-3f);
	// It moves: the focus goes after it
	place({400.0f, 10.0f, 250.0f});
	script_camera::Frame(0.1f, 0, 0.0f);
	const auto second = state.focus.GetDestination();
	EXPECT_NEAR(second.x, 405.0f, 1e-3f);
	EXPECT_NEAR(second.y, 11.0f, 1e-3f);
	EXPECT_NEAR(second.z, 250.0f, 1e-3f);
}
