/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

constexpr uint32_t k_Waiter = 1;
constexpr uint32_t k_Ender = 2;

/// A script that goes round a loop, a turn at a time, and one that ends on its first turn
LHVMFile WaiterAndEnder()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Jmp, VMValue(uint32_t {0})), // 0: JMP 0, ending the turn
	    Make(Opcode::Jmp, VMValue(uint32_t {0})), // 1: JMP 0
	    Make(Opcode::End),                        // 2: the ender
	};
	const std::vector<VMScript> scripts {
	    VMScript("Waiter", "test.txt", ScriptType::Script, 0, {}, 0, 0, k_Waiter),
	    VMScript("Ender", "test.txt", ScriptType::Script, 0, {}, 2, 0, k_Ender),
	};
	return {LHVMVersion::BlackAndWhite, {}, code, {}, scripts, {}};
}

class LhvmTicks: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(WaiterAndEnder()), EXIT_SUCCESS);
	}

	[[nodiscard]] uint32_t Ticks(uint32_t task) const { return _vm.GetTasks().at(task).ticks; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
};

} // namespace

TEST_F(LhvmTicks, EachTaskEndingLaterInTheListGivesAnEarlierTaskATurnMore)
{
	const auto waiter = _vm.StartScript("Waiter", ScriptType::All);
	_vm.StartScript("Ender", ScriptType::All);
	_vm.StartScript("Ender", ScriptType::All);
	ASSERT_EQ(Ticks(waiter), 1u);
	_vm.LookIn(ScriptType::All);
	// Its own turn, and one more for each of the two tasks after it that ended
	EXPECT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(Ticks(waiter), 4u);
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(Ticks(waiter), 5u);
}

TEST_F(LhvmTicks, TasksEndingEarlierInTheListGiveALaterTaskNothingMore)
{
	_vm.StartScript("Ender", ScriptType::All);
	_vm.StartScript("Ender", ScriptType::All);
	const auto waiter = _vm.StartScript("Waiter", ScriptType::All);
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(Ticks(waiter), 2u);
}

TEST(LhvmInstructions, EachInstructionRunCountsOnceTheOneThatEndsTheTurnToo)
{
	// Two jumps on to the next instruction and a jump back that ends the turn: three instructions a turn
	const std::vector<VMInstruction> code {
	    Make(Opcode::Jmp, VMValue(uint32_t {1}), DataType::Int, VMMode::Forward), // 0: JMP 1
	    Make(Opcode::Jmp, VMValue(uint32_t {2}), DataType::Int, VMMode::Forward), // 1: JMP 2
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                 // 2: JMP 0, ending the turn
	    Make(Opcode::End),                                                        // 3: the ender
	};
	const std::vector<VMScript> scripts {
	    VMScript("Looper", "test.txt", ScriptType::Script, 0, {}, 0, 0, 1),
	    VMScript("Ender", "test.txt", ScriptType::Script, 0, {}, 3, 0, 2),
	};
	std::vector<NativeFunction> natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM vm;
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(LHVMFile(LHVMVersion::BlackAndWhite, {}, code, {}, scripts, {})), EXIT_SUCCESS);
	vm.StartScript("Looper", ScriptType::All);
	const auto before = vm.GetExecutedInstructions();
	vm.LookIn(ScriptType::All);
	EXPECT_EQ(vm.GetExecutedInstructions() - before, 3u);
	vm.LookIn(ScriptType::All);
	EXPECT_EQ(vm.GetExecutedInstructions() - before, 6u);
	// The END that stops a task is run, so it counts too
	vm.StartScript("Ender", ScriptType::All);
	vm.LookIn(ScriptType::All);
	EXPECT_EQ(vm.GetExecutedInstructions() - before, 10u);
}
