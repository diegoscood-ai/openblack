/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The division and the remainder as the game's script library does them: whole numbers as unsigned bits, a float
// remainder of the numbers truncated, and by 0 (or by a float that does not compare) the dividend left on the stack under
// the 0 pushed; of a type the operations have no case for, a 0 pushed over the operands.

#include <cstdint>
#include <cstdlib>

#include <bit>
#include <limits>
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
/// A value left on the task's stack: its bits and its type
struct Left
{
	uint32_t bits;
	DataType type;
	bool operator==(const Left&) const = default;
};

Left Int(int32_t value)
{
	return {std::bit_cast<uint32_t>(value), DataType::Int};
}

Left Float(float value)
{
	return {std::bit_cast<uint32_t>(value), DataType::Float};
}

Left Vector(float value)
{
	return {std::bit_cast<uint32_t>(value), DataType::Vector};
}

/// Pushes the values, runs the one operation of the type, and gives what the task's stack holds after it
std::vector<Left> StackAfter(const std::vector<Left>& pushed, Opcode op, DataType type)
{
	std::vector<VMInstruction> code;
	for (const auto& value : pushed)
	{
		code.emplace_back(Opcode::Push, VMMode::Immediate, value.type, VMValue(value.bits), 0);
	}
	code.emplace_back(op, VMMode::Immediate, type, VMValue(uint32_t {0}), 0);
	code.emplace_back(Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0);
	code.emplace_back(Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0);
	const std::vector<VMScript> scripts {VMScript("Sum", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};

	std::vector<Left> left;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(
	        [&]() {
		        const auto& stack = vm.GetTasks().begin()->second.stack;
		        for (uint32_t i = 0; i < stack.count; ++i)
		        {
			        left.push_back({stack.values.at(i).uintVal, stack.types.at(i)});
		        }
	        },
	        0, 0, "LOOK"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	const lhvm::LHVMFile file(lhvm::LHVMVersion::BlackAndWhite, {}, code, {}, scripts, {});
	EXPECT_EQ(vm.LoadBinary(file), EXIT_SUCCESS);
	EXPECT_NE(vm.StartScript("Sum", lhvm::ScriptType::All), 0u);
	vm.LookIn(lhvm::ScriptType::All);
	return left;
}
} // namespace

TEST(LhvmArithmetic, WholeNumbersAreDividedAsUnsignedBits)
{
	EXPECT_EQ(StackAfter({Int(17), Int(5)}, Opcode::Div, DataType::Int), std::vector<Left> {Int(3)});
	EXPECT_EQ(StackAfter({Int(17), Int(5)}, Opcode::Mod, DataType::Int), std::vector<Left> {Int(2)});
	// -8 is 4294967288 unsigned: over 3 that is 1431655762, remainder 2
	EXPECT_EQ(StackAfter({Int(-8), Int(3)}, Opcode::Div, DataType::Int), std::vector<Left> {Int(1431655762)});
	EXPECT_EQ(StackAfter({Int(-8), Int(3)}, Opcode::Mod, DataType::Int), std::vector<Left> {Int(2)});
}

TEST(LhvmArithmetic, ByZeroTheDividendStaysUnderTheZeroPushed)
{
	EXPECT_EQ(StackAfter({Int(9), Int(0)}, Opcode::Div, DataType::Int), (std::vector<Left> {Int(9), Int(0)}));
	EXPECT_EQ(StackAfter({Int(9), Int(0)}, Opcode::Mod, DataType::Int), (std::vector<Left> {Int(9), Int(0)}));
	EXPECT_EQ(StackAfter({Float(9.0f), Float(0.0f)}, Opcode::Div, DataType::Float),
	          (std::vector<Left> {Float(9.0f), Float(0.0f)}));
	EXPECT_EQ(StackAfter({Float(9.0f), Float(-0.0f)}, Opcode::Mod, DataType::Float),
	          (std::vector<Left> {Float(9.0f), Float(0.0f)}));
	// a divisor that does not compare is taken as 0
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(StackAfter({Float(9.0f), Float(nan)}, Opcode::Div, DataType::Float),
	          (std::vector<Left> {Float(9.0f), Float(0.0f)}));
	// a position over 0 has all four taken and three zeros pushed
	EXPECT_EQ(StackAfter({Vector(1.0f), Vector(2.0f), Vector(3.0f), Float(0.0f)}, Opcode::Div, DataType::Vector),
	          (std::vector<Left> {Vector(0.0f), Vector(0.0f), Vector(0.0f)}));
	EXPECT_EQ(StackAfter({Vector(2.0f), Vector(4.0f), Vector(6.0f), Float(2.0f)}, Opcode::Div, DataType::Vector),
	          (std::vector<Left> {Vector(1.0f), Vector(2.0f), Vector(3.0f)}));
}

TEST(LhvmArithmetic, AFloatRemainderIsOfTheNumbersTruncated)
{
	// 7.5 % 2: 7 / 2 = 3, 7.5 - 6 = 1.5
	EXPECT_EQ(StackAfter({Float(7.5f), Float(2.0f)}, Opcode::Mod, DataType::Float), std::vector<Left> {Float(1.5f)});
	// 7.5 % 2.5: the divisor taken as 2, so 1.5 again, where the remainder of the floats would be 0
	EXPECT_EQ(StackAfter({Float(7.5f), Float(2.5f)}, Opcode::Mod, DataType::Float), std::vector<Left> {Float(1.5f)});
	// -7.5 % 2: -7 / 2 = -3, -7.5 + 6 = -1.5
	EXPECT_EQ(StackAfter({Float(-7.5f), Float(2.0f)}, Opcode::Mod, DataType::Float), std::vector<Left> {Float(-1.5f)});
	// a divisor below 1 truncates to 0: 0, the dividend taken
	EXPECT_EQ(StackAfter({Float(7.5f), Float(0.5f)}, Opcode::Mod, DataType::Float), std::vector<Left> {Float(0.0f)});
	// a position's remainder: three zeros over the operands, which stay
	EXPECT_EQ(
	    StackAfter({Vector(1.0f), Vector(2.0f), Vector(3.0f), Float(2.0f)}, Opcode::Mod, DataType::Vector),
	    (std::vector<Left> {Vector(1.0f), Vector(2.0f), Vector(3.0f), Float(2.0f), Vector(0.0f), Vector(0.0f), Vector(0.0f)}));
}

TEST(LhvmArithmetic, OfATypeWithNoCaseAZeroIsPushedOverTheOperands)
{
	const std::vector<Left> operands {Int(4), Int(6)};
	for (const auto op : {Opcode::Add, Opcode::Sub, Opcode::Mul, Opcode::Div, Opcode::Mod})
	{
		SCOPED_TRACE(static_cast<int>(op));
		EXPECT_EQ(StackAfter(operands, op, DataType::Object), (std::vector<Left> {Int(4), Int(6), Float(0.0f)}));
	}
	EXPECT_EQ(StackAfter({Int(4)}, Opcode::Neg, DataType::Object), (std::vector<Left> {Int(4), Float(0.0f)}));
}
