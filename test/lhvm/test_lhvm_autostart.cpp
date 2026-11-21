/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script machine as a program loads: the scripts its auto-start list names are running tasks once it is loaded,
// the others not, and a name the list gives no script for is an error; taking a value off the stack into a variable
// that held an object releases that object, the new one taken up first; and the whole program starting again, as the
// scripts' restart does, stops every task, puts every global back and starts the auto-start list again.
// docs/wiki/project/progress/engine/script_vm.md

#include <cstdint>
#include <cstdlib>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMScript;
using lhvm::VMValue;

namespace
{
const VMInstruction k_End {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0};

/// Three scripts that each only end, the first, second and third
std::vector<VMScript> ThreeScripts()
{
	return {VMScript("First", "test.txt", lhvm::ScriptType::Script, 0, {}, 0, 0, 1),
	        VMScript("Second", "test.txt", lhvm::ScriptType::Script, 0, {}, 1, 0, 2),
	        VMScript("Third", "test.txt", lhvm::ScriptType::Script, 0, {}, 2, 0, 3)};
}

/// The three scripts with one global, the first script on the auto-start list
lhvm::LHVMFile ProgramWithACounter()
{
	const std::vector<std::string> globals {"counter"};
	return lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, globals, {k_End, k_End, k_End}, {1}, ThreeScripts(), {});
}
} // namespace

// The scripts the auto-start list names are running once the program is loaded; the others are not
TEST(LhvmAutostart, TheScriptsTheAutoStartListNamesRunOnceTheProgramIsLoaded)
{
	lhvm::LHVM vm;
	vm.Initialise(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(
	    vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, {k_End, k_End, k_End}, {1, 3}, ThreeScripts(), {})),
	    EXIT_SUCCESS);
	std::set<std::string> running;
	for (const auto& [id, task] : vm.GetTasks())
	{
		running.insert(task.name);
	}
	EXPECT_EQ(running, (std::set<std::string> {"First", "Third"}));
}

// A number in the auto-start list with no script is an error, and starts nothing
TEST(LhvmAutostart, ANumberWithNoScriptIsAnError)
{
	lhvm::LHVM vm;
	std::vector<lhvm::ErrorCode> errors;
	vm.Initialise(
	    nullptr, nullptr, nullptr, nullptr,
	    [&errors](lhvm::ErrorCode code, const std::string /*text*/, uint32_t /*value*/) { errors.push_back(code); }, nullptr,
	    nullptr);
	ASSERT_EQ(
	    vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {}, {k_End, k_End, k_End}, {9}, ThreeScripts(), {})),
	    EXIT_SUCCESS);
	EXPECT_TRUE(vm.GetTasks().empty());
	EXPECT_EQ(errors, std::vector {lhvm::ErrorCode::ErrScriptIdNotFound});
}

// Taking an object off the stack into a variable that held one takes the new object up, then releases the old one
TEST(LhvmAutostart, PoppingIntoAVariableReleasesTheObjectItHeld)
{
	const VMValue first(uint32_t {5});
	const VMValue second(uint32_t {9});
	const std::vector<VMInstruction> code {
	    {Opcode::Push, VMMode::Immediate, DataType::Object, first, 0},
	    {Opcode::Pop, VMMode::Reference, DataType::Object, VMValue(uint32_t {1}), 0},
	    {Opcode::Push, VMMode::Immediate, DataType::Object, second, 0},
	    {Opcode::Pop, VMMode::Reference, DataType::Object, VMValue(uint32_t {1}), 0},
	    k_End,
	};
	std::vector<std::pair<char, uint32_t>> references;
	lhvm::LHVM vm;
	vm.Initialise(
	    nullptr, nullptr, nullptr, nullptr, nullptr, [&references](uint32_t object) { references.emplace_back('+', object); },
	    [&references](uint32_t object) { references.emplace_back('-', object); });
	const std::vector<VMScript> scripts {VMScript("Hold", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};
	ASSERT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"held"}, code, {}, scripts, {})), EXIT_SUCCESS);
	ASSERT_NE(vm.StartScript("Hold", lhvm::ScriptType::All), 0u);
	vm.LookIn(lhvm::ScriptType::All);
	const std::vector<std::pair<char, uint32_t>> expected {{'+', 5}, {'+', 9}, {'-', 5}};
	EXPECT_EQ(references, expected);
}

// The program starting again stops every task it ran, puts its globals back to 0, and starts the scripts of its
// auto-start list again, their task numbers counted from the start
TEST(LhvmRestart, TheProgramStartingAgainStopsEveryTaskAndStartsTheAutoStartListAfresh)
{
	std::vector<uint32_t> stopped;
	lhvm::LHVM vm;
	vm.Initialise(
	    nullptr, nullptr, nullptr, [&stopped](uint32_t task) { stopped.push_back(task); }, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(ProgramWithACounter()), EXIT_SUCCESS);
	ASSERT_NE(vm.StartScript("Second", lhvm::ScriptType::All), 0u);
	vm.SetVariable(1, VMValue(5.0f));
	ASSERT_EQ(vm.GetTasks().size(), 2u);

	ASSERT_EQ(vm.Restart(ProgramWithACounter()), EXIT_SUCCESS);

	EXPECT_EQ(stopped.size(), 2u);
	ASSERT_EQ(vm.GetTasks().size(), 1u);
	const auto& [id, task] = *vm.GetTasks().begin();
	EXPECT_EQ(task.name, "First");
	EXPECT_EQ(id, 1u);
	ASSERT_EQ(vm.GetVariables().size(), 2u);
	EXPECT_EQ(vm.GetVariables()[1].value.floatVal, 0.0f);
}

// A program that cannot be read again leaves the machine with none: no task, no script and no global
TEST(LhvmRestart, AProgramThatCannotBeReadAgainLeavesNone)
{
	lhvm::LHVM vm;
	vm.Initialise(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(ProgramWithACounter()), EXIT_SUCCESS);
	ASSERT_FALSE(vm.GetTasks().empty());

	EXPECT_NE(vm.Restart(lhvm::LHVMFile {}), EXIT_SUCCESS);

	EXPECT_TRUE(vm.GetTasks().empty());
	EXPECT_TRUE(vm.GetScripts().empty());
	EXPECT_TRUE(vm.GetVariables().empty());
	EXPECT_TRUE(vm.GetInstructions().empty());
}
