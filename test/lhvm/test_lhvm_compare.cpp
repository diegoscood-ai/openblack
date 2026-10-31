/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script machine's equal and not-equal, by the type the instruction compares: integers and booleans by their
// bits, floats with a pair that can't be ordered (a NaN) counting as equal; any other type takes nothing off the stack
// and gives false for equal, true for not equal, as the original's script library does.

#include <cstdint>
#include <cstdlib>

#include <limits>
#include <optional>
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
struct Compared
{
	bool result;
	DataType type;
	/// The values left on the stack under the result
	uint32_t under;
};

/// Runs PUSH a, PUSH b, the comparison on type, then a native that reads the top of the stack and what is under it
std::optional<Compared> RunCompare(Opcode compare, DataType type, VMValue a, VMValue b, DataType pushed)
{
	const std::vector<VMInstruction> code {
	    {Opcode::Push, VMMode::Immediate, pushed, a, 0},
	    {Opcode::Push, VMMode::Immediate, pushed, b, 0},
	    {compare, VMMode::Immediate, type, VMValue(uint32_t {0}), 0},
	    {Opcode::Sys, VMMode::Immediate, DataType::Int, VMValue(uint32_t {1}), 0},
	    {Opcode::End, VMMode::Immediate, DataType::Int, VMValue(uint32_t {0}), 0},
	};
	const std::vector<VMScript> scripts {VMScript("Compare", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};

	std::optional<Compared> seen;
	lhvm::LHVM vm;
	const std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(
	        [&]() {
		        DataType popped {};
		        const auto value = vm.Pop(popped);
		        seen = Compared {.result = value.intVal != 0, .type = popped, .under = vm.GetCurrentStack()->count};
	        },
	        1, 0, "READ"),
	};
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"unused"}, code, {}, scripts, {})), EXIT_SUCCESS);
	EXPECT_NE(vm.StartScript("Compare", lhvm::ScriptType::All), 0u);
	vm.LookIn(lhvm::ScriptType::All);
	return seen;
}
} // namespace

// Integers and booleans compare their bits, and both values are taken
TEST(LhvmCompare, IntegersAndBooleansCompareTheirBits)
{
	for (const auto type : {DataType::Int, DataType::Boolean})
	{
		const auto same = RunCompare(Opcode::Eq, type, VMValue(int32_t {3}), VMValue(int32_t {3}), type);
		ASSERT_TRUE(same.has_value());
		EXPECT_TRUE(same->result);
		EXPECT_EQ(same->type, DataType::Boolean);
		EXPECT_EQ(same->under, 0u);
		EXPECT_FALSE(RunCompare(Opcode::Ne, type, VMValue(int32_t {3}), VMValue(int32_t {3}), type)->result);
		EXPECT_FALSE(RunCompare(Opcode::Eq, type, VMValue(int32_t {3}), VMValue(int32_t {4}), type)->result);
		EXPECT_TRUE(RunCompare(Opcode::Ne, type, VMValue(int32_t {3}), VMValue(int32_t {4}), type)->result);
	}
}

// Floats compare as numbers; a pair that can't be ordered counts as equal
TEST(LhvmCompare, FloatsCompareAsNumbersAndANanCountsAsEqual)
{
	EXPECT_TRUE(RunCompare(Opcode::Eq, DataType::Float, VMValue(0.5f), VMValue(0.5f), DataType::Float)->result);
	EXPECT_FALSE(RunCompare(Opcode::Eq, DataType::Float, VMValue(0.5f), VMValue(0.25f), DataType::Float)->result);
	EXPECT_TRUE(RunCompare(Opcode::Ne, DataType::Float, VMValue(0.5f), VMValue(0.25f), DataType::Float)->result);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(RunCompare(Opcode::Eq, DataType::Float, VMValue(nan), VMValue(1.0f), DataType::Float)->result);
	EXPECT_FALSE(RunCompare(Opcode::Ne, DataType::Float, VMValue(nan), VMValue(1.0f), DataType::Float)->result);
	EXPECT_EQ(RunCompare(Opcode::Eq, DataType::Float, VMValue(0.5f), VMValue(0.5f), DataType::Float)->under, 0u);
}

// Any other type (an object, a vector, none) takes nothing off the stack: equal gives false, not equal true
TEST(LhvmCompare, AnyOtherTypeTakesNothingAndGivesFalseForEqualTrueForNotEqual)
{
	for (const auto type : {DataType::Object, DataType::Vector, DataType::None})
	{
		const auto eq = RunCompare(Opcode::Eq, type, VMValue(uint32_t {7}), VMValue(uint32_t {7}), DataType::Object);
		ASSERT_TRUE(eq.has_value());
		EXPECT_FALSE(eq->result);
		EXPECT_EQ(eq->under, 2u);
		const auto ne = RunCompare(Opcode::Ne, type, VMValue(uint32_t {7}), VMValue(uint32_t {8}), DataType::Object);
		ASSERT_TRUE(ne.has_value());
		EXPECT_TRUE(ne->result);
		EXPECT_EQ(ne->under, 2u);
	}
}
