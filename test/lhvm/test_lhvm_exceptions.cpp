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

constexpr uint32_t k_Counter = 1;
constexpr uint32_t k_Done = 2;

/// A script whose loop counts a global up by one each turn "until" it reaches 3, then sets another global and ends
LHVMFile LoopUntilThree()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Except, VMValue(uint32_t {7})),                                // 0: EXCEPT handler
	    Make(Opcode::Push, VMValue(k_Counter), DataType::Int, VMMode::Reference),   // 1: PUSHI counter
	    Make(Opcode::Push, VMValue(int32_t {1})),                                   // 2: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                   // 3: ADDI
	    Make(Opcode::Pop, VMValue(k_Counter), DataType::Int, VMMode::Reference),    // 4: POPI counter
	    Make(Opcode::Jmp, VMValue(uint32_t {1})),                                   // 5: JMP 1, ending the turn
	    Make(Opcode::End),                                                          // 6
	    Make(Opcode::Push, VMValue(k_Counter), DataType::Int, VMMode::Reference),   // 7: handler: PUSHI counter
	    Make(Opcode::Push, VMValue(int32_t {3})),                                   // 8: PUSHI 3
	    Make(Opcode::Eq, VMValue(uint32_t {0})),                                    // 9: EQ
	    Make(Opcode::Wait, VMValue(uint32_t {13}), DataType::Int, VMMode::Forward), // 10: JZ 13
	    Make(Opcode::BrkExcept),                                                    // 11: BRKEXCEPT
	    Make(Opcode::Jmp, VMValue(uint32_t {14}), DataType::Int, VMMode::Forward),  // 12: JMP 14
	    Make(Opcode::FailExcept),                                                   // 13: FAILEXCEPT
	    Make(Opcode::Push, VMValue(int32_t {1})),                                   // 14: PUSHI 1
	    Make(Opcode::Pop, VMValue(k_Done), DataType::Int, VMMode::Reference),       // 15: POPI done
	    Make(Opcode::End),                                                          // 16
	};
	const std::vector<VMScript> scripts {VMScript("CountToThree", "test.txt", ScriptType::Script, 2, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"counter", "done"}, code, {1}, scripts, {}};
}

constexpr uint32_t k_OuterChecks = 3;

/// The loop of LoopUntilThree inside a block whose own handler counts the turns it is checked; after the loop the
/// script waits, a turn at a time, so the block's handler goes on being checked
LHVMFile LoopInABlock()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Except, VMValue(uint32_t {15})),                                 // 0: EXCEPT the block's handler
	    Make(Opcode::Except, VMValue(uint32_t {7})),                                  // 1: EXCEPT the loop's handler
	    Make(Opcode::Push, VMValue(k_Counter), DataType::Int, VMMode::Reference),     // 2: PUSHI counter
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 3: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                     // 4: ADDI
	    Make(Opcode::Pop, VMValue(k_Counter), DataType::Int, VMMode::Reference),      // 5: POPI counter
	    Make(Opcode::Jmp, VMValue(uint32_t {2})),                                     // 6: JMP 2, ending the turn
	    Make(Opcode::Push, VMValue(k_Counter), DataType::Int, VMMode::Reference),     // 7: loop's handler
	    Make(Opcode::Push, VMValue(int32_t {3})),                                     // 8: PUSHI 3
	    Make(Opcode::Eq, VMValue(uint32_t {0})),                                      // 9: EQ
	    Make(Opcode::Wait, VMValue(uint32_t {13}), DataType::Int, VMMode::Forward),   // 10: JZ 13
	    Make(Opcode::BrkExcept),                                                      // 11: BRKEXCEPT
	    Make(Opcode::Jmp, VMValue(uint32_t {21}), DataType::Int, VMMode::Forward),    // 12: JMP 21
	    Make(Opcode::FailExcept),                                                     // 13: FAILEXCEPT
	    Make(Opcode::End),                                                            // 14
	    Make(Opcode::Push, VMValue(k_OuterChecks), DataType::Int, VMMode::Reference), // 15: block's handler
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 16: PUSHI 1
	    Make(Opcode::Add, VMValue(uint32_t {0})),                                     // 17: ADDI
	    Make(Opcode::Pop, VMValue(k_OuterChecks), DataType::Int, VMMode::Reference),  // 18: POPI outerChecks
	    Make(Opcode::FailExcept),                                                     // 19: FAILEXCEPT
	    Make(Opcode::End),                                                            // 20
	    Make(Opcode::Push, VMValue(int32_t {1})),                                     // 21: PUSHI 1
	    Make(Opcode::Pop, VMValue(k_Done), DataType::Int, VMMode::Reference),         // 22: POPI done
	    Make(Opcode::Jmp, VMValue(uint32_t {23})),                                    // 23: JMP 23, a turn at a time
	};
	const std::vector<VMScript> scripts {VMScript("LoopInABlock", "test.txt", ScriptType::Script, 3, {}, 0, 0, 1)};
	return {LHVMVersion::BlackAndWhite, {"counter", "done", "outerChecks"}, code, {1}, scripts, {}};
}

class LhvmExceptions: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(LoopUntilThree()), EXIT_SUCCESS);
	}

	[[nodiscard]] int32_t Global(uint32_t id) const { return _vm.GetVariables().at(id).value.intVal; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
};

} // namespace

TEST_F(LhvmExceptions, ALoopsUntilConditionIsCheckedEveryTurnAndEndsTheLoop)
{
	for (int turn = 0; turn < 6; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	// The condition is checked before the loop's turn, so the loop stops as soon as the count reaches three
	EXPECT_EQ(Global(k_Counter), 3);
	EXPECT_EQ(Global(k_Done), 1);
}

TEST(LhvmExceptionsNested, BreakingOutOfALoopKeepsTheBlocksHandler)
{
	std::vector<NativeFunction> natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM vm;
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(LoopInABlock()), EXIT_SUCCESS);
	const auto global = [&vm](uint32_t id) { return vm.GetVariables().at(id).value.intVal; };
	for (int turn = 0; turn < 4; ++turn)
	{
		vm.LookIn(ScriptType::All);
	}
	ASSERT_EQ(global(k_Counter), 3);
	ASSERT_EQ(global(k_Done), 1);
	const auto checksWhenTheLoopEnded = global(k_OuterChecks);
	// Only the loop's own handler goes with the break: the block's is still checked every turn
	for (int turn = 0; turn < 3; ++turn)
	{
		vm.LookIn(ScriptType::All);
	}
	EXPECT_EQ(global(k_OuterChecks), checksWhenTheLoopEnded + 3);
	EXPECT_EQ(global(k_Counter), 3);
}
