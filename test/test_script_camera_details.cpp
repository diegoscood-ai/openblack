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
// docs/wiki/camera/script-camera.md#opcodes

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
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
