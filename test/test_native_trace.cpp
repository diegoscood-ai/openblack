/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "ScriptHeaders/NativeTrace.h"

using namespace openblack;
using lhvm::DataType;
using lhvm::VMValue;

// Each stack value is written by its type: an int, a float, an object, a boolean, and any other type by its bits
TEST(NativeTrace, EachValueIsWrittenByItsType)
{
	const std::array values {VMValue(int32_t {-3}), VMValue(1.5f),        VMValue(uint32_t {42}),
	                         VMValue(int32_t {1}),  VMValue(int32_t {0}), VMValue(uint32_t {0x10})};
	const std::array types {DataType::Int,     DataType::Float,   DataType::Object,
	                        DataType::Boolean, DataType::Boolean, DataType::Unk5};
	EXPECT_EQ(script::FormatNativeValues(values, types), "-3, 1.5, obj:42, true, false, UNK2:0x10");
}

// No values write nothing; a float keeps its shortest exact text
TEST(NativeTrace, NoValuesWriteNothingAndAFloatKeepsItsShortestText)
{
	EXPECT_EQ(script::FormatNativeValues({}, {}), "");
	const std::array values {VMValue(0.1f), VMValue(1000.0f)};
	const std::array types {DataType::Float, DataType::Vector};
	EXPECT_EQ(script::FormatNativeValues(values, types), "0.1, 1000");
}

// A call's line names the native, its arguments, what it gave back, the turn and the task's script
TEST(NativeTrace, ACallsLineNamesTheNativeItsValuesTheTurnAndTheScript)
{
	EXPECT_EQ(script::FormatNativeCall("CREATE", "1, 3, 1500, 0, 1600", "obj:7", 120, "ChooseYourCreature"),
	          "native CREATE(1, 3, 1500, 0, 1600) -> obj:7, turn 120, task ChooseYourCreature");
	EXPECT_EQ(script::FormatNativeCall("START_CAMERA_CONTROL", "", "true", 5, "LandControl1"),
	          "native START_CAMERA_CONTROL() -> true, turn 5, task LandControl1");
}
