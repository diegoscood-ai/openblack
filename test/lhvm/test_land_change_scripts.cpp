/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack::lhvm;

namespace
{

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// A challenge whose one script starts by itself and counts a global up by one each turn, forever
LHVMFile Challenge()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 0: PUSHI counter
	    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 1: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 2: ADDI
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 3: POPI counter
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                    // 4: JMP 0, which ends the turn
	    Make(Opcode::End),                                                           // 5
	};
	const std::vector<VMScript> scripts {VMScript("LandControl", "test.txt", ScriptType::Script, 1, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"counter"}, code, {1}, scripts, {}};
}

/// A land's scripts, each starting by itself and waiting a turn at a time, forever; the data holds one string, `kept`
LHVMFile NamedScripts(const std::vector<std::string>& names, const std::string& kept)
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Jmp, VMValue(uint32_t {0})), // 0: JMP 0, which ends the turn
	    Make(Opcode::End),                        // 1
	};
	std::vector<VMScript> scripts;
	std::vector<uint32_t> autostart;
	for (const auto& name : names)
	{
		autostart.push_back(static_cast<uint32_t>(scripts.size() + 1));
		scripts.emplace_back(name, "test.txt", ScriptType::Script, 1, std::vector<std::string> {}, 0, 0, autostart.back());
	}
	std::vector<char> data(kept.begin(), kept.end());
	data.push_back('\0');
	return {LHVMVersion::BlackAndWhite, {"counter"}, code, autostart, scripts, data};
}

class LandChangeScripts: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(Challenge()), EXIT_SUCCESS);
	}

	[[nodiscard]] int32_t Counter() const { return _vm.GetVariables().at(1).value.intVal; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
};

} // namespace

TEST_F(LandChangeScripts, TheChallengesOwnScriptsStartWithIt)
{
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(Counter(), 2);
}

TEST_F(LandChangeScripts, LoadingTheChallengeAgainStartsItsScriptsAfresh)
{
	// A land loaded from the menu reloads the challenge: the last land's task stops and its global goes back to zero,
	// and the script that starts by itself starts once more, from its beginning
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	_vm.LookIn(ScriptType::All);
	ASSERT_EQ(Counter(), 3);

	ASSERT_EQ(_vm.LoadBinary(Challenge()), EXIT_SUCCESS);
	EXPECT_EQ(Counter(), 0);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(_vm.GetTasks().begin()->second.instructionAddress, 0u);

	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(Counter(), 1);
}

// STOP_ALL_SCRIPTS_EXCLUDING stops every running script but those its list names, each name matched whole and with its
// case, as the original (docs/wiki/scripts/land-change-and-saving.md#changing-land-load_map)
TEST_F(LandChangeScripts, StoppingAllScriptsButTheNamedOnesKeepsOnlyThoseNamedExactly)
{
	const openblack::test::RestoreService<openblack::Locator::vm> restoreVm;
	auto& vm = openblack::Locator::vm::emplace<LHVM>();
	vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(NamedScripts({"LandControl", "Keep", "keep", "KeepOut", "Other"}, "LandControl Keep")),
	          EXIT_SUCCESS);
	ASSERT_EQ(vm.GetTasks().size(), 5u);
	openblack::chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native = std::ranges::find_if(
	    table, [](const NativeFunction& f) { return std::string_view(f.name) == "STOP_ALL_SCRIPTS_EXCLUDING"; });
	ASSERT_NE(native, table.end());
	vm.Pushi(0); // the list: the data's string
	native->impl();
	std::vector<std::string> running;
	for (const auto& entry : vm.GetTasks())
	{
		running.push_back(entry.second.name);
	}
	std::ranges::sort(running);
	EXPECT_EQ(running, (std::vector<std::string> {"Keep", "LandControl"}));
}

// The list is split on commas and tabs as well as spaces, and a separator on its own names nothing, as the original
// (docs/wiki/scripts/land-change-and-saving.md#changing-land-load_map)
TEST_F(LandChangeScripts, StoppingAllScriptsButTheNamedOnesSplitsTheListOnSpacesCommasAndTabs)
{
	const openblack::test::RestoreService<openblack::Locator::vm> restoreVm;
	auto& vm = openblack::Locator::vm::emplace<LHVM>();
	vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(NamedScripts({"LandControl", "Keep", "Tabbed", "Other"}, "LandControl,Keep ,\tTabbed")),
	          EXIT_SUCCESS);
	ASSERT_EQ(vm.GetTasks().size(), 4u);
	openblack::chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native = std::ranges::find_if(
	    table, [](const NativeFunction& f) { return std::string_view(f.name) == "STOP_ALL_SCRIPTS_EXCLUDING"; });
	ASSERT_NE(native, table.end());
	vm.Pushi(0); // the list: the data's string
	native->impl();
	std::vector<std::string> running;
	for (const auto& entry : vm.GetTasks())
	{
		running.push_back(entry.second.name);
	}
	std::ranges::sort(running);
	EXPECT_EQ(running, (std::vector<std::string> {"Keep", "LandControl", "Tabbed"}));
}

// STOP_SCRIPT stops the running scripts its list names, split and matched as STOP_ALL_SCRIPTS_EXCLUDING's list, as the
// original (docs/wiki/scripts/land-change-and-saving.md#changing-land-load_map)
TEST_F(LandChangeScripts, StoppingScriptsByNameStopsEachOneTheListNames)
{
	const openblack::test::RestoreService<openblack::Locator::vm> restoreVm;
	auto& vm = openblack::Locator::vm::emplace<LHVM>();
	vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(NamedScripts({"LandControl", "Taunt", "taunt", "TauntAI", "Confront", "Tabbed"},
	                                     "Taunt, Confront ,\tTabbed")),
	          EXIT_SUCCESS);
	ASSERT_EQ(vm.GetTasks().size(), 6u);
	openblack::chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	const auto native =
	    std::ranges::find_if(table, [](const NativeFunction& f) { return std::string_view(f.name) == "STOP_SCRIPT"; });
	ASSERT_NE(native, table.end());
	vm.Pushi(0); // the list: the data's string
	native->impl();
	std::vector<std::string> running;
	for (const auto& entry : vm.GetTasks())
	{
		running.push_back(entry.second.name);
	}
	std::ranges::sort(running);
	EXPECT_EQ(running, (std::vector<std::string> {"LandControl", "TauntAI", "taunt"}));
}
