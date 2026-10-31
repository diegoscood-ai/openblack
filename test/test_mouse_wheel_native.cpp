/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// HAS_MOUSE_WHEEL, as a script calls it: it answers whether the mouse has a wheel, as the tooltips' mouse pictures take
// it, and is no longer an unwritten native. docs/wiki/interface/tooltips.md (the mouse row)

#include <algorithm>
#include <string_view>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Help/InputPromptIcon.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;

namespace
{
class MouseWheelNative: public ::testing::Test
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

	test::RestoreService<Locator::vm> _restoreVm;
	chlapi::CHLApi _api;
};
} // namespace

TEST_F(MouseWheelNative, AnswersWhetherTheMouseHasAWheelAsTheTooltipsTakeIt)
{
	const auto& table = _api.GetFunctionsTable();
	const auto native = std::ranges::find_if(
	    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "HAS_MOUSE_WHEEL"; });
	ASSERT_NE(native, table.end());
	ASSERT_EQ(native->stackIn, 0);
	ASSERT_EQ(native->stackOut, 1u);
	auto& vm = Locator::vm::value();
	// a value under the native's answer, which it must leave there
	vm.Pushi(77);
	native->impl();
	EXPECT_EQ(vm.Pop().intVal != 0, help::input_prompt::k_MouseHasWheel);
	EXPECT_EQ(vm.Pop().intVal, 77);
	// the tooltips' mouse row is the wheel's
	EXPECT_EQ(help::input_prompt::MouseRow(), 2);
	EXPECT_EQ(_api.GetStubCalls().Total(), 0u);
}
