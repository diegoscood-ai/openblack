/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// WITHIN_ROTATION, as a script calls it: whether the player's camera shows any of its helper icons by the hand, offered
// or in use, as the camera keeps them for the frame. docs/wiki/camera/player-camera.md (the tricons: what is shown)

#include <cstdint>

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Hand/HandTricons.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class WithinRotationNative: public ::testing::Test
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
	}

	/// The native's answer, with a value under it that it must leave there
	bool Call()
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "WITHIN_ROTATION"; });
		EXPECT_NE(native, table.end());
		EXPECT_EQ(native->stackIn, 0);
		EXPECT_EQ(native->stackOut, 1u);
		auto& vm = Locator::vm::value();
		vm.Pushi(77);
		native->impl();
		const bool answer = vm.Pop().intVal != 0;
		EXPECT_EQ(vm.Pop().intVal, 77);
		return answer;
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::camera> _restoreCamera;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(WithinRotationNative, AnyIconShownOfferedOrInUseIsWithin)
{
	auto& camera = Locator::camera::emplace(glm::vec3(0.0f));
	namespace icon = hand_tricons::icon;
	for (const uint32_t icons : {icon::k_Rotate, icon::k_Rotate | (icon::k_Rotate << icon::k_OfferedShift), icon::k_Pitch,
	                             icon::k_Zoom, icon::k_Cross})
	{
		camera.SetShownHandIcons(icons);
		EXPECT_TRUE(Call()) << icons;
	}
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}

TEST_F(WithinRotationNative, NoIconShownIsNotWithin)
{
	auto& camera = Locator::camera::emplace(glm::vec3(0.0f));
	// a new camera shows none
	EXPECT_FALSE(Call());
	camera.SetShownHandIcons(hand_tricons::icon::k_Rotate);
	camera.SetShownHandIcons(0);
	EXPECT_FALSE(Call());
}

TEST_F(WithinRotationNative, NoCameraIsNotWithin)
{
	Locator::camera::reset();
	EXPECT_FALSE(Call());
}
