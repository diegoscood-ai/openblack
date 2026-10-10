/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script camera functions that ask for a draw-list rebuild (CHLApi.cpp): SET_CAMERA_POSITION and SET_CAMERA_FOCUS
// only in the script camera mode, RESTORE_CAMERA_DETAILS and SET_CAMERA_POS_FOC_LENS always, each with a count of 1.
// docs/bw1-notes/original-frame.md §6

#include <cstdint>

#include <algorithm>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Camera/ScriptCamera.h"
#include "Locator.h"
#include "support/DrawListRequests.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class ScriptCameraRebuilds: public ::testing::Test
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
		script_camera::Reset();
	}
	void TearDown() override { script_camera::Reset(); }

	/// Calls the native `name` after pushing `floats` in order
	void Call(std::string_view name, const std::vector<float>& floats)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native =
		    std::ranges::find_if(table, [name](const lhvm::NativeFunction& f) { return std::string_view(f.name) == name; });
		ASSERT_NE(native, table.end()) << name;
		ASSERT_EQ(native->stackIn, static_cast<int>(floats.size())) << name;
		auto& vm = Locator::vm::value();
		for (const auto value : floats)
		{
			vm.Pushf(value);
		}
		native->impl();
	}

	test::RestoreService<Locator::vm> _restoreVm;
	chlapi::CHLApi _api;
	test::DrawListRequests _requests;
};
} // namespace

TEST_F(ScriptCameraRebuilds, PositionAndFocusAskOnlyInTheScriptMode)
{
	// the player's camera: the script's position and focus are refused, and nothing is asked
	Call("SET_CAMERA_POSITION", {1.0f, 2.0f, 3.0f});
	Call("SET_CAMERA_FOCUS", {4.0f, 5.0f, 6.0f});
	EXPECT_TRUE(_requests.Counts().empty());

	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	Call("SET_CAMERA_POSITION", {1.0f, 2.0f, 3.0f});
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	Call("SET_CAMERA_FOCUS", {4.0f, 5.0f, 6.0f});
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1, 1}));
}

TEST_F(ScriptCameraRebuilds, RestoreAndPosFocLensAlwaysAsk)
{
	// in the script mode, so that the player's camera is not needed
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	Call("RESTORE_CAMERA_DETAILS", {});
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1}));
	// the position, the focus, then the lens
	Call("SET_CAMERA_POS_FOC_LENS", {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 1.2f});
	EXPECT_EQ(_requests.Counts(), (std::vector<uint8_t> {1, 1}));
}
