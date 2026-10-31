/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The running state of a saved game's script file, byte for byte as the game's script library writes and reads it.
// Synthetic goldens pin each part of the state, in the library's order: the main stack, the globals, a task and the
// five words that close the file. The game's own script saves are read and written back unchanged when
// OPENBLACK_TEST_GAME_PATH (or OPENBLACK_TEST_BW_ROOT) is a folder that holds them; without it that test skips.
// Creature Isle's saves in the same folder are another game's, and are left out.

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <LHVMFile.h>
#include <gtest/gtest.h>

using namespace openblack::lhvm;

namespace
{

/// The bytes of a file in its order: little-endian words, single bytes and strings ended by a zero
class FileBytes
{
public:
	FileBytes& Word(uint32_t value)
	{
		for (uint32_t shift = 0; shift < 32; shift += 8)
		{
			_bytes.push_back(static_cast<uint8_t>(value >> shift));
		}
		return *this;
	}

	FileBytes& Byte(uint8_t value)
	{
		_bytes.push_back(value);
		return *this;
	}

	FileBytes& Text(std::string_view text)
	{
		_bytes.insert(_bytes.end(), text.begin(), text.end());
		_bytes.push_back(0);
		return *this;
	}

	[[nodiscard]] const std::vector<uint8_t>& Get() const { return _bytes; }

private:
	std::vector<uint8_t> _bytes;
};

constexpr uint32_t k_FloatThree = 0x40400000; // 3.0f

/// A program with nothing in it: the magic, the version, then empty globals, code, autostart, scripts and data
FileBytes EmptyProgram()
{
	FileBytes bytes;
	bytes.Byte('L').Byte('H').Byte('V').Byte('M').Word(static_cast<uint32_t>(LHVMVersion::BlackAndWhite));
	for (int list = 0; list < 5; ++list)
	{
		bytes.Word(0);
	}
	return bytes;
}

/// An empty stack: no places, nothing pushed or popped
FileBytes& EmptyStack(FileBytes& bytes)
{
	return bytes.Word(0).Word(0).Word(0);
}

LHVMFile StateOf(const VMStack& stack, const std::vector<VMVar>& globals, const std::vector<VMTask>& tasks,
                 const std::vector<VMScript>& scripts = {})
{
	return {LHVMVersion::BlackAndWhite, {}, {}, {}, scripts, {}, stack, globals, tasks, 0, 0, 0, 0, 0};
}

[[nodiscard]] std::vector<uint8_t> Written(const LHVMFile& file)
{
	std::ostringstream stream(std::ios::binary);
	file.Write(stream);
	const auto text = stream.str();
	return {text.begin(), text.end()};
}

[[nodiscard]] LHVMFile Read(const std::vector<uint8_t>& bytes)
{
	LHVMFile file;
	file.Open(bytes);
	return file;
}

/// A task of the given script with every word, state and list of a task set, all different
VMTask FullTask(uint32_t scriptId)
{
	VMStack stack;
	stack.values.at(0) = VMValue(9u);
	stack.types.at(0) = DataType::Int;
	stack.count = 1;
	stack.pushCount = 3;
	stack.popCount = 2;

	VMTask task({VMVar(DataType::Object, VMValue(42u), "local")}, scriptId, 0x11, 0x12, 0x15, stack, "", "", ScriptType::Help);
	task.pevInstructionAddress = 0x13;
	task.waitingTaskId = 0x14;
	task.currentExceptionHandlerIndex = 0x16;
	task.ticks = 0x17;
	task.inExceptionHandler = true;
	task.stop = false;
	task.iield = true;
	task.sleeping = false;
	task.exceptionHandlerIps = {0x21, 0x22};
	return task;
}

/// FullTask's bytes, as the library writes a task
FileBytes& FullTaskBytes(FileBytes& bytes, uint32_t scriptId)
{
	// The locals, as the globals are written
	bytes.Word(1).Byte(static_cast<uint8_t>(DataType::Object)).Word(42).Text("local");
	// The task's number, its instruction, its previous instruction, the task it waits for, its variables' offset, its
	// handler index, its ticks, its script and the script's type
	bytes.Word(0x11).Word(0x12).Word(0x13).Word(0x14).Word(0x15).Word(0x16).Word(0x17).Word(scriptId);
	bytes.Word(static_cast<uint32_t>(ScriptType::Help));
	// In a handler, stopped, yielding, sleeping: all bits set when true
	bytes.Byte(0xFF).Byte(0).Byte(0xFF).Byte(0);
	// Its stack, then its handlers' addresses
	bytes.Word(1).Word(3).Word(2).Word(static_cast<uint32_t>(DataType::Int)).Word(9);
	return bytes.Word(2).Word(0x21).Word(0x22);
}

/// The first offset at which two files differ, or their shorter size when one is the start of the other
size_t FirstDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
	const auto difference = std::ranges::mismatch(a, b);
	return static_cast<size_t>(std::distance(a.begin(), difference.in1));
}

} // namespace

TEST(LHVMSaveLayout, TheProgramIsItsGlobalsCodeAutostartScriptsAndDataWithNoStateAfterIt)
{
	const LHVMFile program {LHVMVersion::BlackAndWhite,
	                        {"Counter"},
	                        {VMInstruction(Opcode::Push, VMMode::Immediate, DataType::Float, VMValue(k_FloatThree), 3)},
	                        {1},
	                        {VMScript("First", "first.txt", ScriptType::Help, 4, {"local"}, 0, 2, 1)},
	                        {'a', 'b', '\0'}};

	FileBytes expected;
	expected.Byte('L').Byte('H').Byte('V').Byte('M').Word(static_cast<uint32_t>(LHVMVersion::BlackAndWhite));
	expected.Word(1).Text("Counter");
	// An instruction: its code, its mode, its type, its value and its source line
	expected.Word(1).Word(static_cast<uint32_t>(Opcode::Push)).Word(static_cast<uint32_t>(VMMode::Immediate));
	expected.Word(static_cast<uint32_t>(DataType::Float)).Word(k_FloatThree).Word(3);
	expected.Word(1).Word(1);
	// A script: its name, its file, its type, its variables' offset and names, its first instruction, its number of
	// parameters and its id
	expected.Word(1).Text("First").Text("first.txt").Word(static_cast<uint32_t>(ScriptType::Help)).Word(4);
	expected.Word(1).Text("local").Word(0).Word(2).Word(1);
	expected.Word(3).Byte('a').Byte('b').Byte(0);

	EXPECT_EQ(Written(program), expected.Get());

	const auto read = Read(expected.Get());
	ASSERT_TRUE(read.IsLoaded());
	EXPECT_FALSE(read.HasStatus());
	EXPECT_EQ(Written(read), expected.Get());
}

TEST(LHVMSaveLayout, TheMainStackIsItsCountsThenEachPlaceAsTypeThenValue)
{
	VMStack stack;
	stack.values.at(0) = VMValue(k_FloatThree);
	stack.types.at(0) = DataType::Float;
	stack.values.at(1) = VMValue(8u);
	stack.types.at(1) = DataType::Object;
	stack.count = 2;
	stack.pushCount = 7;
	stack.popCount = 5;

	auto expected = EmptyProgram();
	expected.Word(2).Word(7).Word(5);
	expected.Word(static_cast<uint32_t>(DataType::Float)).Word(k_FloatThree);
	expected.Word(static_cast<uint32_t>(DataType::Object)).Word(8);
	expected.Word(0).Word(0).Word(0).Word(0).Word(0).Word(0).Word(0);

	EXPECT_EQ(Written(StateOf(stack, {}, {})), expected.Get());

	const auto read = Read(expected.Get());
	ASSERT_TRUE(read.HasStatus());
	ASSERT_EQ(read.GetStack().count, 2u);
	EXPECT_EQ(read.GetStack().pushCount, 7u);
	EXPECT_EQ(read.GetStack().popCount, 5u);
	EXPECT_EQ(read.GetStack().types.at(0), DataType::Float);
	EXPECT_EQ(read.GetStack().values.at(0).uintVal, k_FloatThree);
	EXPECT_EQ(read.GetStack().types.at(1), DataType::Object);
	EXPECT_EQ(read.GetStack().values.at(1).uintVal, 8u);
}

TEST(LHVMSaveLayout, EachGlobalIsATypeByteAValueAndItsName)
{
	const std::vector<VMVar> globals {
	    VMVar(DataType::Float, VMValue(k_FloatThree), "Counter"),
	    VMVar(DataType::Boolean, VMValue(1u), "Flag"),
	};

	auto expected = EmptyProgram();
	EmptyStack(expected);
	expected.Word(2);
	expected.Byte(static_cast<uint8_t>(DataType::Float)).Word(k_FloatThree).Text("Counter");
	expected.Byte(static_cast<uint8_t>(DataType::Boolean)).Word(1).Text("Flag");
	expected.Word(0).Word(0).Word(0).Word(0).Word(0).Word(0);

	EXPECT_EQ(Written(StateOf({}, globals, {})), expected.Get());

	const auto read = Read(expected.Get());
	ASSERT_TRUE(read.HasStatus());
	ASSERT_EQ(read.GetVariablesValues().size(), 2u);
	EXPECT_EQ(read.GetVariablesValues()[0].type, DataType::Float);
	EXPECT_EQ(read.GetVariablesValues()[0].value.uintVal, k_FloatThree);
	EXPECT_EQ(read.GetVariablesValues()[0].name, "Counter");
	EXPECT_EQ(read.GetVariablesValues()[1].type, DataType::Boolean);
	EXPECT_EQ(read.GetVariablesValues()[1].value.uintVal, 1u);
	EXPECT_EQ(read.GetVariablesValues()[1].name, "Flag");
}

TEST(LHVMSaveLayout, ATaskIsItsLocalsNineWordsFourStatesItsStackAndItsHandlers)
{
	auto expected = EmptyProgram();
	EmptyStack(expected);
	expected.Word(0).Word(1);
	FullTaskBytes(expected, 0x18);
	expected.Word(0).Word(0).Word(0).Word(0).Word(0);

	EXPECT_EQ(Written(StateOf({}, {}, {FullTask(0x18)})), expected.Get());

	const auto read = Read(expected.Get());
	ASSERT_TRUE(read.HasStatus());
	ASSERT_EQ(read.GetTasks().size(), 1u);
	const auto& task = read.GetTasks().front();
	ASSERT_EQ(task.localVars.size(), 1u);
	EXPECT_EQ(task.localVars[0].type, DataType::Object);
	EXPECT_EQ(task.localVars[0].value.uintVal, 42u);
	EXPECT_EQ(task.localVars[0].name, "local");
	EXPECT_EQ(task.id, 0x11u);
	EXPECT_EQ(task.instructionAddress, 0x12u);
	EXPECT_EQ(task.pevInstructionAddress, 0x13u);
	EXPECT_EQ(task.waitingTaskId, 0x14u);
	EXPECT_EQ(task.variablesOffset, 0x15u);
	EXPECT_EQ(task.currentExceptionHandlerIndex, 0x16u);
	EXPECT_EQ(task.ticks, 0x17u);
	EXPECT_EQ(task.scriptId, 0x18u);
	EXPECT_EQ(task.type, ScriptType::Help);
	EXPECT_TRUE(task.inExceptionHandler);
	EXPECT_FALSE(task.stop);
	EXPECT_TRUE(task.iield);
	EXPECT_FALSE(task.sleeping);
	ASSERT_EQ(task.stack.count, 1u);
	EXPECT_EQ(task.stack.pushCount, 3u);
	EXPECT_EQ(task.stack.popCount, 2u);
	EXPECT_EQ(task.stack.types.at(0), DataType::Int);
	EXPECT_EQ(task.stack.values.at(0).uintVal, 9u);
	EXPECT_EQ(task.exceptionHandlerIps, (std::vector<uint32_t> {0x21, 0x22}));
}

TEST(LHVMSaveLayout, TheFiveWordsCloseTheFile)
{
	const LHVMFile state {LHVMVersion::BlackAndWhite, {}, {}, {}, {}, {}, {}, {}, {}, 101, 102, 103, 104, 105};

	auto expected = EmptyProgram();
	EmptyStack(expected);
	// No globals and no tasks, then the ticks, the current line, the highest task, the highest script and the number
	// of instructions run
	expected.Word(0).Word(0).Word(101).Word(102).Word(103).Word(104).Word(105);

	EXPECT_EQ(Written(state), expected.Get());

	const auto read = Read(expected.Get());
	ASSERT_TRUE(read.HasStatus());
	EXPECT_EQ(read.GetTicks(), 101u);
	EXPECT_EQ(read.GetCurrentLineNumber(), 102u);
	EXPECT_EQ(read.GetHighestTaskId(), 103u);
	EXPECT_EQ(read.GetHighestScriptId(), 104u);
	EXPECT_EQ(read.GetExecutedInstructions(), 105u);
}

TEST(LHVMSaveLayout, AStateThatIsNotZeroReadsAsTrueAndIsWrittenWithAllBitsSet)
{
	auto bytes = EmptyProgram();
	EmptyStack(bytes);
	bytes.Word(0).Word(1).Word(0);
	bytes.Word(1).Word(0).Word(0).Word(0).Word(0).Word(0).Word(1).Word(1).Word(static_cast<uint32_t>(ScriptType::Script));
	bytes.Byte(1).Byte(0x80).Byte(0).Byte(0xFF);
	EmptyStack(bytes).Word(0);
	bytes.Word(0).Word(0).Word(1).Word(0).Word(0);

	const auto read = Read(bytes.Get());
	ASSERT_TRUE(read.HasStatus());
	ASSERT_EQ(read.GetTasks().size(), 1u);
	const auto& task = read.GetTasks().front();
	EXPECT_TRUE(task.inExceptionHandler);
	EXPECT_TRUE(task.stop);
	EXPECT_FALSE(task.iield);
	EXPECT_TRUE(task.sleeping);

	const auto written = Written(read);
	constexpr size_t k_States = 28 + 12 + 4 + 4 + 4 + 9 * 4;
	ASSERT_GT(written.size(), k_States + 3);
	EXPECT_EQ(written[k_States], 0xFFu);
	EXPECT_EQ(written[k_States + 1], 0xFFu);
	EXPECT_EQ(written[k_States + 2], 0u);
	EXPECT_EQ(written[k_States + 3], 0xFFu);
}

TEST(LHVMSaveLayout, ATaskFindsItsScriptByIdNotByPlaceTheLastOneToo)
{
	// Two scripts whose ids are not their places: a task of script 1, the second one, and a task of script 2, whose id
	// is the number of scripts
	const std::vector<VMScript> scripts {
	    VMScript("Second", "second.txt", ScriptType::Script, 0, {}, 0, 0, 2),
	    VMScript("First", "first.txt", ScriptType::Script, 0, {}, 0, 0, 1),
	};
	const auto read = Read(Written(StateOf({}, {}, {FullTask(1), FullTask(2)}, scripts)));

	ASSERT_TRUE(read.HasStatus());
	ASSERT_EQ(read.GetTasks().size(), 2u);
	EXPECT_EQ(read.GetTasks()[0].name, "First");
	EXPECT_EQ(read.GetTasks()[0].filename, "first.txt");
	EXPECT_EQ(read.GetTasks()[1].name, "Second");
	EXPECT_EQ(read.GetTasks()[1].filename, "second.txt");
}

namespace
{

std::optional<std::filesystem::path> GameRoot()
{
	for (const char* name : {"OPENBLACK_TEST_GAME_PATH", "OPENBLACK_TEST_BW_ROOT"})
	{
		const char* root = std::getenv(name);
		if (root != nullptr && *root != '\0' && std::filesystem::is_directory(root))
		{
			return std::filesystem::path(root);
		}
	}
	return std::nullopt;
}

/// A saved game's script file, Script_<slot>.sav, in any case
bool IsScriptSave(const std::filesystem::path& path)
{
	const auto name = path.filename().u8string();
	std::string lower(name.size(), '\0');
	std::ranges::transform(name, lower.begin(),
	                       [](char8_t c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
	return lower.starts_with("script_") && lower.ends_with(".sav");
}

std::vector<std::filesystem::path> ScriptSavesUnder(const std::filesystem::path& root)
{
	std::vector<std::filesystem::path> saves;
	std::error_code error;
	for (auto entry = std::filesystem::recursive_directory_iterator(
	         root, std::filesystem::directory_options::skip_permission_denied, error);
	     !error && entry != std::filesystem::recursive_directory_iterator(); entry.increment(error))
	{
		if (entry->is_regular_file(error) && IsScriptSave(entry->path()))
		{
			saves.push_back(entry->path());
		}
	}
	std::ranges::sort(saves);
	return saves;
}

std::string Shown(const std::filesystem::path& path)
{
	const auto text = path.u8string();
	return {text.begin(), text.end()};
}

std::vector<uint8_t> FileContents(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The file starts as Black & White's script files do: the magic and the game's version. Creature Isle's saves, which
/// its own folder inside the game's keeps, have the next version and are another game's.
bool IsBlackAndWhites(const std::vector<uint8_t>& bytes)
{
	const auto start = EmptyProgram().Get();
	return bytes.size() >= 8 && std::ranges::equal(std::span(bytes).first(8), std::span(start).first(8));
}

} // namespace

// Integration test: needs the game's own saved games (OPENBLACK_TEST_GAME_PATH); skipped without them
TEST(LHVMSaveLayoutGameFiles, EveryScriptSaveReadsAndWritesBackByteForByte)
{
	const auto root = GameRoot();
	if (!root)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH not set";
	}

	int compared = 0;
	for (const auto& path : ScriptSavesUnder(*root))
	{
		const auto bytes = FileContents(path);
		if (!IsBlackAndWhites(bytes))
		{
			continue;
		}
		++compared;
		const auto file = Read(bytes);
		ASSERT_TRUE(file.IsLoaded()) << Shown(path);
		ASSERT_TRUE(file.HasStatus()) << Shown(path);

		const auto written = Written(file);
		EXPECT_EQ(written.size(), bytes.size()) << Shown(path);
		EXPECT_TRUE(written == bytes) << Shown(path) << ": first difference at byte " << FirstDifference(written, bytes);
	}
	if (compared == 0)
	{
		GTEST_SKIP() << "no Script_*.sav of Black & White under " << Shown(*root);
	}
	RecordProperty("ScriptSavesCompared", compared);
}
