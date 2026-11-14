/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a script's variables live. A variable number up to the script's offset is one of the program's globals, shared
// by every script; above it, one of the running task's own locals, which each task of a script has a copy of, starting
// at 0. A task's parameters come off the stack of whoever started it onto the task's own stack, which its first lines
// pop into its locals.

#include <cstdint>
#include <cstdlib>

#include <tuple>
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
/// The program's one global, G
constexpr uint32_t k_Global = 1;
/// The writer's one local, L, past its offset of one variable
constexpr uint32_t k_Local = 2;

/// When a task called the native that looks: the turn, the task and its local (-1 without one)
using Look = std::tuple<int, uint32_t, float>;

/// What a task saw when it called the native that looks
struct Seen
{
	uint32_t task;
	float global;
	float local;
	uint32_t stackCount;
};
} // namespace

TEST(LhvmVariables, GlobalsAreSharedByEveryScriptAndEachTaskHasItsOwnLocalsAndParameters)
{
	const std::vector<VMInstruction> code {
	    // Writer(p): L = p, look, G = L, look, end
	    {Opcode::Pop, VMMode::Reference, DataType::Float, VMValue(k_Local), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::Push, VMMode::Reference, DataType::Float, VMValue(k_Local), 0},
	    {Opcode::Pop, VMMode::Reference, DataType::Float, VMValue(k_Global), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	    // Reader: look, end
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	};
	const std::vector<VMScript> scripts {
	    VMScript("Writer", "test.txt", lhvm::ScriptType::Script, 1, {"L"}, 0, 1, 1),
	    VMScript("Reader", "test.txt", lhvm::ScriptType::Script, 1, {}, 6, 0, 2),
	};

	std::vector<Seen> seen;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(
	        [&]() {
		        const auto& task = vm.GetTasks().at(vm.GetCurrentTaskNumber());
		        seen.push_back({
		            .task = task.id,
		            .global = vm.GetVariables().at(k_Global).value.floatVal,
		            .local = task.localVars.empty() ? -1.0f : task.localVars.at(0).value.floatVal,
		            .stackCount = task.stack.count,
		        });
	        },
	        0, 0, "LOOK"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"G"}, code, {}, scripts, {})), EXIT_SUCCESS);

	// Two writers, each given its parameter on the stack it is started from, then a reader
	vm.Pushf(3.0f);
	const auto first = vm.StartScript("Writer", lhvm::ScriptType::All);
	vm.Pushf(7.0f);
	const auto second = vm.StartScript("Writer", lhvm::ScriptType::All);
	const auto reader = vm.StartScript("Reader", lhvm::ScriptType::All);
	ASSERT_NE(first, 0u);
	ASSERT_NE(second, 0u);
	ASSERT_NE(reader, 0u);

	// Each task starts with its parameter on its own stack and its locals at 0, the global at 0
	const auto& tasks = vm.GetTasks();
	EXPECT_EQ(tasks.at(first).stack.count, 1u);
	EXPECT_EQ(tasks.at(first).stack.values.at(0).floatVal, 3.0f);
	EXPECT_EQ(tasks.at(second).stack.values.at(0).floatVal, 7.0f);
	EXPECT_EQ(tasks.at(first).localVars.size(), 1u);
	EXPECT_EQ(tasks.at(first).localVars.at(0).value.floatVal, 0.0f);
	EXPECT_EQ(tasks.at(reader).localVars.size(), 0u);
	EXPECT_EQ(vm.GetVariables().at(k_Global).value.floatVal, 0.0f);

	vm.LookIn(lhvm::ScriptType::All);

	// The first writer's local takes its own parameter, the global its local; the second's local takes its own, and
	// the global, shared, then holds the second's, which the reader, another script, reads
	ASSERT_EQ(seen.size(), 5u);
	const std::vector<Seen> expected {
	    {first, 0.0f, 3.0f, 0u},  {first, 3.0f, 3.0f, 0u},   {second, 3.0f, 7.0f, 0u},
	    {second, 7.0f, 7.0f, 0u}, {reader, 7.0f, -1.0f, 0u},
	};
	for (size_t i = 0; i < expected.size(); ++i)
	{
		SCOPED_TRACE(i);
		EXPECT_EQ(seen.at(i).task, expected.at(i).task);
		EXPECT_EQ(seen.at(i).global, expected.at(i).global);
		EXPECT_EQ(seen.at(i).local, expected.at(i).local);
		EXPECT_EQ(seen.at(i).stackCount, expected.at(i).stackCount);
	}
}

// A script that runs another gives it its parameters off its own stack. Run and waited for, the caller stops at the
// call until the task it started has ended, and goes on past it in the turn after the turn that task ended in; run
// alongside, it goes on at once, and the new task, started after it, runs in the same turn
TEST(LhvmVariables, ARunScriptIsWaitedForUntilItEndsOrRunsAlongside)
{
	for (const auto mode : {VMMode::Sync, VMMode::Async})
	{
		SCOPED_TRACE(static_cast<int>(mode));
		const std::vector<VMInstruction> code {
		    // Caller: push 5, run Callee(5), look, end
		    {Opcode::Push, VMMode::Immediate, DataType::Float, VMValue(5.0f), 0},
		    {Opcode::Run, mode, DataType::Int, VMValue(uint32_t {2}), 0},
		    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
		    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
		    // Callee(p): L = p, look, yield to the next turn, look, end
		    {Opcode::Pop, VMMode::Reference, DataType::Float, VMValue(k_Local), 0},
		    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
		    {Opcode::EndExcept, VMMode::Yield, DataType::Int, VMValue(uint32_t {0}), 0},
		    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
		    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
		};
		const std::vector<VMScript> scripts {
		    VMScript("Caller", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1),
		    VMScript("Callee", "test.txt", lhvm::ScriptType::Script, 1, {"L"}, 4, 1, 2),
		};

		std::vector<Look> seen;
		int turn = 0;
		lhvm::LHVM vm;
		const std::vector<lhvm::NativeFunction> natives {
		    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
		    lhvm::NativeFunction(
		        [&]() {
			        const auto& task = vm.GetTasks().at(vm.GetCurrentTaskNumber());
			        seen.emplace_back(turn, task.id, task.localVars.empty() ? -1.0f : task.localVars.at(0).value.floatVal);
		        },
		        0, 0, "LOOK"),
		};
		vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		const lhvm::LHVMFile file(lhvm::LHVMVersion::BlackAndWhite, {"G"}, code, {}, scripts, {});
		ASSERT_EQ(vm.LoadBinary(file), EXIT_SUCCESS);
		const auto caller = vm.StartScript("Caller", lhvm::ScriptType::All);
		ASSERT_EQ(caller, 1u);
		const uint32_t callee = 2;
		for (turn = 1; turn <= 4; ++turn)
		{
			vm.LookIn(lhvm::ScriptType::All);
		}

		std::vector<Look> expected {{1, callee, 5.0f}, {2, callee, 5.0f}, {3, caller, -1.0f}};
		if (mode == VMMode::Async)
		{
			expected = {{1, caller, -1.0f}, {1, callee, 5.0f}, {2, callee, 5.0f}};
		}
		EXPECT_EQ(seen, expected);
		EXPECT_TRUE(vm.GetTasks().empty());
	}
}

// A task runs its lines until one waits, sleeps or yields, and takes up again there the next turn. A sleep asks whether
// its seconds times 10, truncated, are below the turns the task has counted, which start at 1 and go up at the end of
// every turn; until they are it pushes false, and the wait after it jumps back and yields
TEST(LhvmVariables, ATaskSleepsUntilItsTurnsPassItsSecondsInTenthsAndGoesOnFromThereTheNextTurn)
{
	const std::vector<VMInstruction> code {
	    // Sleeper: until sleep 0.3 s, look, yield, look, end
	    {Opcode::Push, VMMode::Immediate, DataType::Float, VMValue(0.3f), 0},
	    {Opcode::Sleep, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	    {Opcode::Wait, VMMode::Backward, DataType::Int, VMValue(uint32_t {0}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::EndExcept, VMMode::Yield, DataType::Int, VMValue(uint32_t {0}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	};
	const std::vector<VMScript> scripts {VMScript("Sleeper", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};

	std::vector<std::pair<int, uint32_t>> looks;
	int turn = 0;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction([&]() { looks.emplace_back(turn, vm.GetTasks().at(vm.GetCurrentTaskNumber()).ticks); }, 0, 0,
	                         "LOOK"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	const lhvm::LHVMFile file(lhvm::LHVMVersion::BlackAndWhite, {"G"}, code, {}, scripts, {});
	ASSERT_EQ(vm.LoadBinary(file), EXIT_SUCCESS);
	const auto task = vm.StartScript("Sleeper", lhvm::ScriptType::All);
	ASSERT_NE(task, 0u);
	EXPECT_EQ(vm.GetTasks().at(task).ticks, 1u);
	for (turn = 1; turn <= 6; ++turn)
	{
		vm.LookIn(lhvm::ScriptType::All);
		if (turn < 4)
		{
			// Asleep: it is back at its first line, its turns counted on
			ASSERT_TRUE(vm.GetTasks().contains(task));
			EXPECT_EQ(vm.GetTasks().at(task).instructionAddress, 0u) << turn;
			EXPECT_TRUE(vm.GetTasks().at(task).sleeping) << turn;
			EXPECT_EQ(vm.GetTasks().at(task).ticks, static_cast<uint32_t>(turn + 1)) << turn;
		}
	}
	// At its fourth turn 3 is below its 4 turns: the wait lets it on, starting its count again at 1, and it yields
	// after its first look; its second look is the next turn's
	const std::vector<std::pair<int, uint32_t>> expected {{4, 1u}, {5, 2u}};
	EXPECT_EQ(looks, expected);
	EXPECT_TRUE(vm.GetTasks().empty());
}
