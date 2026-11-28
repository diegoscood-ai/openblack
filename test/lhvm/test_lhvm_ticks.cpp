/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <array>
#include <string>
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

TEST_F(LhvmTicks, TheElapsedTimeIsTheTurnsCountedTimesATenthAsAFloat)
{
	// Every turn of the scripts counts, whether a task runs or not; the time is the count times a tenth, rounded once
	// to a float
	const auto elapsed = [this]() {
		_vm.PushElaspedTime();
		DataType type {};
		const auto time = _vm.Pop(type).floatVal;
		EXPECT_EQ(type, DataType::Float);
		return time;
	};
	EXPECT_EQ(elapsed(), 0.0f);
	for (int turn = 0; turn < 3; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	EXPECT_EQ(elapsed(), 0.3f);
	_vm.StartScript("Waiter", ScriptType::All);
	for (int turn = 0; turn < 7; ++turn)
	{
		_vm.LookIn(ScriptType::All);
	}
	EXPECT_EQ(elapsed(), 1.0f);
}

namespace
{
/// One script of each of the seven kinds, each going on to its next instruction and then ending its turn there, so
/// that a task that ran this turn is one instruction on from its start
constexpr std::array k_Kinds {ScriptType::Script,         ScriptType::Help,          ScriptType::ChallengeHelp,
                              ScriptType::TempleHelp,     ScriptType::TempleSpecial, ScriptType::Unknown5,
                              ScriptType::MultiplayerHelp};

class LhvmKinds: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::vector<VMInstruction> code {
		    Make(Opcode::Jmp, VMValue(uint32_t {1}), DataType::Int, VMMode::Forward), // 0: JMP 1
		    Make(Opcode::Jmp, VMValue(uint32_t {1})),                                 // 1: JMP 1, ending the turn
		};
		std::vector<VMScript> scripts;
		for (uint32_t k = 0; k < k_Kinds.size(); ++k)
		{
			scripts.emplace_back("Kind" + std::to_string(k), "test.txt", k_Kinds.at(k), 0, std::vector<std::string> {}, 0, 0,
			                     k + 1);
		}
		_vm.Initialise(
		    &_natives, nullptr, nullptr, [this](uint32_t task) { _stopped.push_back(task); }, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(LHVMFile(LHVMVersion::BlackAndWhite, {}, code, {}, scripts, {})), EXIT_SUCCESS);
		for (uint32_t k = 0; k < k_Kinds.size(); ++k)
		{
			_tasks.at(k) = _vm.StartScript("Kind" + std::to_string(k), ScriptType::All);
		}
	}

	/// Whether the task of the kind at `k` has run: it is one instruction on from its start
	[[nodiscard]] bool Ran(size_t k) const { return _vm.GetTasks().at(_tasks.at(k)).instructionAddress == 1; }

	std::vector<NativeFunction> _natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM _vm;
	std::array<uint32_t, k_Kinds.size()> _tasks {};
	/// The tasks the stop callback was told of, in order
	std::vector<uint32_t> _stopped;
};
} // namespace

TEST_F(LhvmKinds, TheTemplesPausedTurnRunsOnlyTheTempleHelpAndSpecialScripts)
{
	// The temple's turn, while the game is paused, looks in with the temple help and temple special kinds (0x18)
	_vm.LookIn(static_cast<ScriptType>(0x18));
	for (size_t k = 0; k < k_Kinds.size(); ++k)
	{
		EXPECT_EQ(Ran(k), k_Kinds.at(k) == ScriptType::TempleHelp || k_Kinds.at(k) == ScriptType::TempleSpecial) << k;
	}
}

TEST_F(LhvmKinds, TheWorldsTurnRunsEveryKind)
{
	// The world's turn looks in with every kind (0x7F)
	_vm.LookIn(static_cast<ScriptType>(0x7F));
	for (size_t k = 0; k < k_Kinds.size(); ++k)
	{
		EXPECT_TRUE(Ran(k)) << k;
	}
}

TEST_F(LhvmKinds, StoppingTheTasksOfAKindStopsOnlyThose)
{
	// The tasks of the kinds given stop, each through the stop callback; the others go on
	_vm.StopTasksOfType(ScriptType::Help | ScriptType::TempleHelp);
	EXPECT_EQ(_vm.GetTasks().size(), k_Kinds.size() - 2);
	for (size_t k = 0; k < k_Kinds.size(); ++k)
	{
		const bool ofThoseKinds = k_Kinds.at(k) == ScriptType::Help || k_Kinds.at(k) == ScriptType::TempleHelp;
		EXPECT_EQ(_vm.GetTasks().contains(_tasks.at(k)), !ofThoseKinds) << k;
	}
	EXPECT_EQ(_stopped, (std::vector<uint32_t> {_tasks.at(1), _tasks.at(3)}));
}
