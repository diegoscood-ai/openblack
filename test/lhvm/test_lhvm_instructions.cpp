/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <limits>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

/// The virtual machine with its instructions open to the tests. Each instruction works on the main stack.
class TestVm: public LHVM
{
public:
	using LHVM::Opcode04Add;
	using LHVM::Opcode08Mul;
	using LHVM::Opcode09Div;
	using LHVM::Opcode10Mod;
	using LHVM::Opcode14Eq;
	using LHVM::Opcode15Neq;
	using LHVM::Opcode16Geq;
	using LHVM::Opcode17Leq;
	using LHVM::Opcode18Gt;
	using LHVM::Opcode19Lt;
	using LHVM::Opcode21Sleep;
	using LHVM::Opcode23Cast;
	using LHVM::Opcode29Swap;

	[[nodiscard]] uint32_t Count() const { return _mainStack.count; }
	[[nodiscard]] uint32_t PushCount() const { return _mainStack.pushCount; }
	[[nodiscard]] uint32_t PopCount() const { return _mainStack.popCount; }
};

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

VMInstruction Typed(Opcode code, DataType type, VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, VMValue(uint32_t {0}), 0};
}

class LhvmOneInstruction: public ::testing::Test
{
protected:
	void SetUp() override { _vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr); }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	TestVm _vm;
	VMTask _task;
};

} // namespace

TEST_F(LhvmOneInstruction, AStackHoldsThirtyOneValues)
{
	for (int32_t i = 0; i < 40; ++i)
	{
		_vm.Pushi(i);
	}
	ASSERT_EQ(_vm.Count(), 31u);
	EXPECT_EQ(_vm.Pop().intVal, 30);
}

TEST_F(LhvmOneInstruction, ADivisionByZeroKeepsTheDividendUnderAZero)
{
	_vm.Pushf(5.0f);
	_vm.Pushf(0.0f);
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Float));
	ASSERT_EQ(_vm.Count(), 2u);
	EXPECT_EQ(_vm.Popf(), 0.0f);
	EXPECT_EQ(_vm.Popf(), 5.0f);
}

TEST_F(LhvmOneInstruction, ANotANumberDivisorCountsAsZero)
{
	_vm.Pushf(5.0f);
	_vm.Pushf(std::numeric_limits<float>::quiet_NaN());
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Float));
	ASSERT_EQ(_vm.Count(), 2u);
	EXPECT_EQ(_vm.Popf(), 0.0f);
}

TEST_F(LhvmOneInstruction, IntegersDivideAndCompareAsUnsignedValues)
{
	_vm.Pushi(-8);
	_vm.Pushi(2);
	_vm.Opcode09Div(_task, Typed(Opcode::Div, DataType::Int));
	EXPECT_EQ(_vm.Pop().uintVal, 0x7FFFFFFCu);

	_vm.Pushi(-1);
	_vm.Pushi(1);
	_vm.Opcode18Gt(_task, Typed(Opcode::Gt, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 1);

	_vm.Pushi(-1);
	_vm.Pushi(1);
	_vm.Opcode19Lt(_task, Typed(Opcode::Lt, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmOneInstruction, AFloatCompareWithNotANumberGivesTheFpuResult)
{
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto compare = [this](float a, float b, Opcode code) {
		_vm.Pushf(a);
		_vm.Pushf(b);
		const auto instruction = Typed(code, DataType::Float);
		switch (code)
		{
		case Opcode::Eq:
			_vm.Opcode14Eq(_task, instruction);
			break;
		case Opcode::Ne:
			_vm.Opcode15Neq(_task, instruction);
			break;
		case Opcode::Ge:
			_vm.Opcode16Geq(_task, instruction);
			break;
		case Opcode::Le:
			_vm.Opcode17Leq(_task, instruction);
			break;
		case Opcode::Gt:
			_vm.Opcode18Gt(_task, instruction);
			break;
		default:
			_vm.Opcode19Lt(_task, instruction);
			break;
		}
		EXPECT_EQ(_vm.Count(), 1u);
		return _vm.Pop().intVal;
	};

	// The FPU cannot put NaN in order: "equal", "greater or equal" and "greater" are true, the others are false
	for (const auto& [a, b] : {std::pair {nan, 1.0f}, std::pair {1.0f, nan}})
	{
		EXPECT_EQ(compare(a, b, Opcode::Eq), 1);
		EXPECT_EQ(compare(a, b, Opcode::Ne), 0);
		EXPECT_EQ(compare(a, b, Opcode::Ge), 1);
		EXPECT_EQ(compare(a, b, Opcode::Gt), 1);
		EXPECT_EQ(compare(a, b, Opcode::Le), 0);
		EXPECT_EQ(compare(a, b, Opcode::Lt), 0);
	}

	// Values in order compare as usual
	EXPECT_EQ(compare(1.0f, 2.0f, Opcode::Eq), 0);
	EXPECT_EQ(compare(1.0f, 2.0f, Opcode::Ne), 1);
	EXPECT_EQ(compare(1.0f, 2.0f, Opcode::Ge), 0);
	EXPECT_EQ(compare(2.0f, 2.0f, Opcode::Ge), 1);
	EXPECT_EQ(compare(1.0f, 2.0f, Opcode::Gt), 0);
	EXPECT_EQ(compare(3.0f, 2.0f, Opcode::Gt), 1);
	EXPECT_EQ(compare(2.0f, 2.0f, Opcode::Le), 1);
	EXPECT_EQ(compare(1.0f, 2.0f, Opcode::Lt), 1);
}

TEST_F(LhvmOneInstruction, AFloatRemainderOfTwoToThe31ByMinusOneDoesNotStop)
{
	// The game's integer division stops the game here (vanilla-bugs 1385). openblack gives a value and continues.
	_vm.Pushf(2147483648.0f);
	_vm.Pushf(-1.0f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Count(), 1u);
}

TEST_F(LhvmOneInstruction, AFloatRemainderUsesTheTruncatedValues)
{
	// 7 divided by 2 gives 3, so the remainder is 7.5 - 6
	_vm.Pushf(7.5f);
	_vm.Pushf(2.5f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), 1.5f);

	// A divisor less than one truncates to zero
	_vm.Pushf(3.0f);
	_vm.Pushf(0.5f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), 0.0f);
	EXPECT_EQ(_vm.Count(), 0u);

	_vm.Pushf(-7.5f);
	_vm.Pushf(2.0f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Float));
	EXPECT_EQ(_vm.Popf(), -1.5f);
}

TEST_F(LhvmOneInstruction, AVectorRemainderTakesNothingAndGivesZeros)
{
	_vm.Pushf(9.0f);
	_vm.Opcode10Mod(_task, Typed(Opcode::Mod, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 4u);
	DataType type;
	EXPECT_EQ(_vm.Pop(type).floatVal, 0.0f);
	EXPECT_EQ(type, DataType::Vector);
}

TEST_F(LhvmOneInstruction, AnInstructionOfAnUnknownTypeTakesNothingAndGivesZero)
{
	_vm.Pushi(4);
	_vm.Pushi(5);
	_vm.Opcode04Add(_task, Typed(Opcode::Add, DataType::Object));
	ASSERT_EQ(_vm.Count(), 3u);
	DataType type;
	EXPECT_EQ(_vm.Pop(type).floatVal, 0.0f);
	EXPECT_EQ(type, DataType::Float);
}

TEST_F(LhvmOneInstruction, ComparisonsOfObjectsAndVectorsTakeNothing)
{
	_vm.Pusho(7);
	_vm.Pusho(7);
	_vm.Opcode14Eq(_task, Typed(Opcode::Eq, DataType::Object));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 0);

	_vm.Opcode15Neq(_task, Typed(Opcode::Ne, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 1);

	_vm.Opcode16Geq(_task, Typed(Opcode::Ge, DataType::Vector));
	ASSERT_EQ(_vm.Count(), 3u);
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmOneInstruction, AVectorTimesANumberScalesEachPart)
{
	_vm.Pushf(2.0f);
	_vm.Pushv(1.0f);
	_vm.Pushv(3.0f);
	_vm.Pushv(5.0f);
	_vm.Opcode08Mul(_task, Typed(Opcode::Mul, DataType::Vector));
	EXPECT_EQ(_vm.Popf(), 10.0f);
	EXPECT_EQ(_vm.Popf(), 6.0f);
	EXPECT_EQ(_vm.Popf(), 2.0f);
}

TEST_F(LhvmOneInstruction, ASleepWithANegativeTimeDoesNotEnd)
{
	// The negative count of tenths becomes a very large unsigned count
	_task.ticks = 1000;
	_vm.Pushf(-1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 0);
	EXPECT_TRUE(_task.sleeping);

	_task.ticks = 11;
	_vm.Pushf(1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 1);
	EXPECT_FALSE(_task.sleeping);

	_task.ticks = 10;
	_vm.Pushf(1.0f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmOneInstruction, ASleepOfNotANumberOrOfMoreThanTheConversionHoldsEndsAtOnce)
{
	// Neither converts to a 64-bit integer, so the count of tenths is the low half of the lowest one: zero
	for (const auto seconds : {std::numeric_limits<float>::quiet_NaN(), 1.0e30f, -1.0e30f})
	{
		_task.ticks = 1;
		_vm.Pushf(seconds);
		_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
		EXPECT_EQ(_vm.Pop().intVal, 1);
		EXPECT_FALSE(_task.sleeping);
	}

	// Minus a tenth of a second is a count of tenths one below a full 32-bit count: still sleeping after a million turns
	_task.ticks = 1000000;
	_vm.Pushf(-0.1f);
	_vm.Opcode21Sleep(_task, Typed(Opcode::Sleep, DataType::Float));
	EXPECT_EQ(_vm.Pop().intVal, 0);
	EXPECT_TRUE(_task.sleeping);
}

TEST_F(LhvmOneInstruction, ACastToAnIntegerTruncatesThroughSixtyFourBits)
{
	_vm.Pushf(-2.75f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, -2);

	_vm.Pushf(5.0e9f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().uintVal, static_cast<uint32_t>(static_cast<int64_t>(5.0e9f)));

	_vm.Pushf(1.0e30f);
	_vm.Opcode23Cast(_task, Typed(Opcode::Cast, DataType::Int));
	EXPECT_EQ(_vm.Pop().intVal, 0);
}

TEST_F(LhvmOneInstruction, AZeroOfAFloatGoesThroughTheStackAndAZeroOfAnotherTypeDoesNothing)
{
	std::vector<uint32_t> released;
	_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr,
	               [&released](uint32_t object) { released.push_back(object); });
	_task.localVars = {VMVar(DataType::Object, VMValue(uint32_t {7}), "held"),
	                   VMVar(DataType::Int, VMValue(int32_t {5}), "count")};

	// A zero of a float puts a float 0 on the stack and takes it off into the variable, letting go of its object
	_vm.Opcode23Cast(_task, {Opcode::Cast, VMMode::Zero, DataType::Float, VMValue(uint32_t {1}), 0});
	EXPECT_EQ(_task.localVars.at(0).type, DataType::Float);
	EXPECT_EQ(_task.localVars.at(0).value.floatVal, 0.0f);
	EXPECT_EQ(released, std::vector<uint32_t> {7});
	EXPECT_EQ(_vm.Count(), 0u);
	EXPECT_EQ(_vm.PushCount(), 1u);
	EXPECT_EQ(_vm.PopCount(), 1u);

	// A zero of any other type leaves the variable and the stack as they were
	for (const auto type : {DataType::Int, DataType::Vector, DataType::Object, DataType::Boolean})
	{
		_vm.Opcode23Cast(_task, {Opcode::Cast, VMMode::Zero, type, VMValue(uint32_t {2}), 0});
		EXPECT_EQ(_task.localVars.at(1).type, DataType::Int);
		EXPECT_EQ(_task.localVars.at(1).value.intVal, 5);
	}
	EXPECT_EQ(_vm.PushCount(), 1u);
	EXPECT_EQ(_vm.PopCount(), 1u);
}

TEST_F(LhvmOneInstruction, AZeroOnAFullStackTakesTheTopValueIntoTheVariable)
{
	_task.localVars = {VMVar(DataType::Float, VMValue(1.0f), "value")};
	for (int i = 0; i < 31; ++i)
	{
		_vm.Pushi(i);
	}
	// The zero's push is dropped, so the variable takes the value that was on top
	_vm.Opcode23Cast(_task, {Opcode::Cast, VMMode::Zero, DataType::Float, VMValue(uint32_t {1}), 0});
	EXPECT_EQ(_task.localVars.at(0).type, DataType::Int);
	EXPECT_EQ(_task.localVars.at(0).value.intVal, 30);
	EXPECT_EQ(_vm.Count(), 30u);
}

namespace
{

constexpr uint32_t k_Runs = 1;

/// The first script stops itself through a native function. The second counts its runs.
LHVMFile StopsItself()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                              // 0: stop me
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                              // 1
	    Make(Opcode::Push, VMValue(k_Runs), DataType::Int, VMMode::Reference), // 2: runs + 1
	    Make(Opcode::Push, VMValue(int32_t {1})),                              // 3
	    Make(Opcode::Add, VMValue(uint32_t {0})),                              // 4
	    Make(Opcode::Pop, VMValue(k_Runs), DataType::Int, VMMode::Reference),  // 5
	    Make(Opcode::Jmp, VMValue(uint32_t {2})),                              // 6
	};
	const std::vector<VMScript> scripts {
	    VMScript("StopsItself", "test.txt", ScriptType::Script, 1, {"held"}, 0, 0, 1),
	    VMScript("Counts", "test.txt", ScriptType::Script, 1, {}, 2, 0, 2),
	};
	return {LHVMVersion::BlackAndWhite, {"runs"}, code, {1, 2}, scripts, {}};
}

/// One script puts an object in its own variable, then stops itself through a native function
LHVMFile HoldsAnObjectAndStopsItself()
{
	constexpr uint32_t k_Held = 2;
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(uint32_t {7}), DataType::Object),             // 0: the object 7
	    Make(Opcode::Pop, VMValue(k_Held), DataType::Object, VMMode::Reference), // 1: into its variable
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                                // 2: stop me
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                // 3
	};
	const std::vector<VMScript> scripts {
	    VMScript("HoldsAndStops", "test.txt", ScriptType::Script, 1, {"held"}, 0, 0, 1),
	};
	return {LHVMVersion::BlackAndWhite, {"runs"}, code, {1}, scripts, {}};
}

} // namespace

TEST_F(LhvmOneInstruction, ATaskThatStopsItselfEndsTheStepOfTheOtherTasks)
{
	std::vector<uint32_t> stopped;
	std::vector<NativeFunction> natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    NativeFunction([this]() { _vm.StopTask(_vm.GetCurrentTaskNumber()); }, 0, 0, "STOP_ME"),
	};
	_vm.Initialise(
	    &natives, nullptr, nullptr, [&stopped](uint32_t task) { stopped.push_back(task); }, nullptr, nullptr, nullptr);
	ASSERT_EQ(_vm.LoadBinary(StopsItself()), EXIT_SUCCESS);

	_vm.LookIn(ScriptType::All);
	ASSERT_EQ(_vm.GetTasks().size(), 1u);
	EXPECT_EQ(stopped, std::vector<uint32_t> {1});
	// The game finds no task after the one that stopped itself, so the other task waits for the next turn
	EXPECT_EQ(_vm.GetVariables().at(k_Runs).value.intVal, 0);

	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(_vm.GetVariables().at(k_Runs).value.intVal, 1);
}

TEST_F(LhvmOneInstruction, ATaskCanStopEveryTaskDuringItsRun)
{
	std::vector<NativeFunction> natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    NativeFunction([this]() { _vm.StopAllTasks(); }, 0, 0, "STOP_ALL"),
	};
	_vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(_vm.LoadBinary(StopsItself()), EXIT_SUCCESS);

	_vm.LookIn(ScriptType::All);
	EXPECT_TRUE(_vm.GetTasks().empty());
}

TEST_F(LhvmOneInstruction, ATaskThatStopsItselfLetsGoOfItsObjectsOnceAndRunsNoMore)
{
	std::vector<uint32_t> stopped;
	std::vector<uint32_t> released;
	std::vector<NativeFunction> natives {
	    NativeFunction(nullptr, 0, 0, "NONE"),
	    NativeFunction(
	        [this]() {
		        _vm.StopTask(_vm.GetCurrentTaskNumber());
		        // A second stop of the same task, as a native stopping scripts by name could ask, changes nothing
		        _vm.StopTask(_vm.GetCurrentTaskNumber());
	        },
	        0, 0, "STOP_ME"),
	};
	_vm.Initialise(
	    &natives, nullptr, nullptr, [&stopped](uint32_t task) { stopped.push_back(task); }, nullptr, nullptr,
	    [&released](uint32_t object) { released.push_back(object); });
	ASSERT_EQ(_vm.LoadBinary(HoldsAnObjectAndStopsItself()), EXIT_SUCCESS);

	_vm.LookIn(ScriptType::All);
	EXPECT_TRUE(_vm.GetTasks().empty());
	EXPECT_EQ(stopped, std::vector<uint32_t> {1});
	EXPECT_EQ(released, std::vector<uint32_t> {7});

	// Nothing is let go of again on the next turn
	_vm.LookIn(ScriptType::All);
	EXPECT_EQ(released, std::vector<uint32_t> {7});
}
