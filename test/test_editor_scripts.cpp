/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <bit>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <ChlCompiler.h>
#include <ChlSyntax.h>
#include <LHVM.h>
#include <gtest/gtest.h>

#include "Editor/Scripts/Decompiler.h"
#include "Editor/Scripts/LogRing.h"
#include "Editor/Scripts/ScriptModel.h"

using namespace openblack;
using namespace openblack::editor::scripts;
using lhvm::DataType;
using lhvm::Opcode;
using lhvm::VMInstruction;
using lhvm::VMMode;
using lhvm::VMScript;
using lhvm::VMValue;

namespace
{
VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// A made-up program of two scripts:
/// - Main (id 1) at 0: pushes the string "hello", calls native 1, sets global 1 from local 1, jumps back, runs Helper
/// - Helper (id 2) at 7: a challenge help script that calls native 2 twice
struct FakeProgram
{
	std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(int32_t {0})),                                      // 0: PUSHI 0 "hello"
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                                      // 1: SYS SAY
	    Make(Opcode::Push, VMValue(uint32_t {3}), DataType::Float, VMMode::Reference), // 2: PUSHF count (local)
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Float, VMMode::Reference),  // 3: POPF score (global)
	    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                      // 4: JMP 0x0000
	    Make(Opcode::Run, VMValue(uint32_t {2}), DataType::Int, VMMode::Async),        // 5: RUN async Helper
	    Make(Opcode::End),                                                             // 6
	    Make(Opcode::Sys, VMValue(uint32_t {2})),                                      // 7: SYS WAIT
	    Make(Opcode::Sys, VMValue(uint32_t {2})),                                      // 8
	    Make(Opcode::End),                                                             // 9
	};
	std::vector<VMScript> scripts {
	    VMScript("Main", "land1.txt", lhvm::ScriptType::Script, 2, {"count"}, 0, 0, 1),
	    VMScript("Helper", "help.txt", lhvm::ScriptType::ChallengeHelp, 2, {}, 7, 0, 2),
	};
	std::vector<lhvm::VMVar> globals {
	    lhvm::VMVar(DataType::Float, VMValue(0.0f), "Null variable"),
	    lhvm::VMVar(DataType::Float, VMValue(2.5f), "score"),
	    lhvm::VMVar(DataType::Int, VMValue(int32_t {7}), "lives"),
	};
	std::vector<lhvm::NativeFunction> natives {
	    lhvm::NativeFunction(nullptr, 0, 0, "NONE"),
	    lhvm::NativeFunction(nullptr, 1, 0, "SAY"),
	    lhvm::NativeFunction(nullptr, 0, 1, "WAIT"),
	};
	std::vector<char> data {'h', 'e', 'l', 'l', 'o', '\0', 'x', '\0'};

	[[nodiscard]] Program View() const
	{
		return {.code = code, .scripts = scripts, .globals = globals, .natives = natives, .data = data};
	}
};
} // namespace

TEST(EditorScripts, SortsScriptsIntoKindsAndSearchesThem)
{
	const FakeProgram fake;
	EXPECT_EQ(CategoryOf(lhvm::ScriptType::Script), Category::Script);
	EXPECT_EQ(CategoryOf(lhvm::ScriptType::ChallengeHelp), Category::ChallengeHelp);
	EXPECT_EQ(CategoryOf(lhvm::ScriptType::Unknown5), Category::Other);
	for (size_t i = 0; i < k_CategoryCount; ++i)
	{
		EXPECT_FALSE(Name(static_cast<Category>(i)).empty());
	}

	ScriptFilter filter;
	EXPECT_TRUE(Matches(fake.scripts.at(0), filter));
	filter.text = "land1";
	EXPECT_TRUE(Matches(fake.scripts.at(0), filter));
	EXPECT_FALSE(Matches(fake.scripts.at(1), filter));
	filter.text = "";
	filter.categories.at(static_cast<size_t>(Category::ChallengeHelp)) = false;
	EXPECT_FALSE(Matches(fake.scripts.at(1), filter));
}

TEST(EditorScripts, FindsCodeRangesAndOwners)
{
	const FakeProgram fake;
	const auto main = RangeOf(fake.code, fake.scripts.at(0));
	EXPECT_EQ(main.begin, 0u);
	EXPECT_EQ(main.end, 7u);
	const auto helper = RangeOf(fake.code, fake.scripts.at(1));
	EXPECT_EQ(helper.begin, 7u);
	EXPECT_EQ(helper.end, 10u);
	EXPECT_EQ(ScriptAt(fake.code, fake.scripts, 5), &fake.scripts.at(0));
	EXPECT_EQ(ScriptAt(fake.code, fake.scripts, 8), &fake.scripts.at(1));
	EXPECT_EQ(ScriptAt(fake.code, fake.scripts, 50), nullptr);

	EXPECT_EQ(JumpTarget(fake.code.at(4)), 0u);
	EXPECT_EQ(JumpTarget(Make(Opcode::Wait, VMValue(uint32_t {9}))), 9u);
	EXPECT_FALSE(JumpTarget(fake.code.at(1)).has_value());
}

TEST(EditorScripts, DisassemblesWithNamesResolved)
{
	const FakeProgram fake;
	const auto program = fake.View();
	const auto* main = &fake.scripts.at(0);

	const auto push = Disassemble(program, main, 0);
	ASSERT_EQ(push.size(), 3u);
	EXPECT_EQ(push.at(0).text, "PUSHI");
	EXPECT_EQ(push.at(1).kind, Token::Kind::Number);
	EXPECT_EQ(push.at(2).kind, Token::Kind::String);
	EXPECT_EQ(push.at(2).text, "\"hello\"");

	const auto sys = Disassemble(program, main, 1);
	ASSERT_GE(sys.size(), 2u);
	EXPECT_EQ(sys.at(1).kind, Token::Kind::Native);
	EXPECT_EQ(sys.at(1).text, "SAY");
	EXPECT_EQ(sys.at(1).target, 1u);

	const auto local = Disassemble(program, main, 2);
	EXPECT_EQ(local.at(1).kind, Token::Kind::Local);
	EXPECT_EQ(local.at(1).text, "count");
	const auto global = Disassemble(program, main, 3);
	EXPECT_EQ(global.at(1).kind, Token::Kind::Global);
	EXPECT_EQ(global.at(1).text, "score");

	const auto jump = Disassemble(program, main, 4);
	EXPECT_EQ(jump.at(1).kind, Token::Kind::Jump);
	EXPECT_EQ(jump.at(1).target, 0u);
	EXPECT_EQ(LineText(jump), "JMP 0x0000 // back");

	const auto run = Disassemble(program, main, 5);
	EXPECT_EQ(run.at(0).text, "RUN async");
	EXPECT_EQ(run.at(1).kind, Token::Kind::Script);
	EXPECT_EQ(run.at(1).text, "Helper");
	EXPECT_EQ(run.at(1).target, 2u);

	EXPECT_TRUE(Disassemble(program, main, 1000).empty());
}

TEST(EditorScripts, NamesUnknownOperandsPlainly)
{
	FakeProgram fake;
	fake.code.push_back(Make(Opcode::Sys, VMValue(uint32_t {99})));
	fake.code.push_back(Make(Opcode::Run, VMValue(uint32_t {42})));
	const auto program = fake.View();
	EXPECT_EQ(Disassemble(program, nullptr, 10).at(1).text, "NATIVE_99");
	EXPECT_EQ(Disassemble(program, nullptr, 11).at(1).text, "script_42");
	bool local = true;
	EXPECT_EQ(VariableName(program, nullptr, 40, local), "global_40");
	EXPECT_FALSE(local);
}

TEST(EditorScripts, FindsStringsOnlyWhereTheyStart)
{
	const std::vector<char> data {'h', 'i', '\0', 'a', 'b', 'c', '\0', 'z', '\0', '\x01', 'q', '\0'};
	EXPECT_EQ(StringAt(data, 0), "hi");
	EXPECT_EQ(StringAt(data, 3), "abc");
	// Inside a string, too short, unprintable, or out of the data
	EXPECT_FALSE(StringAt(data, 4).has_value());
	EXPECT_FALSE(StringAt(data, 7).has_value());
	EXPECT_FALSE(StringAt(data, 9).has_value());
	EXPECT_FALSE(StringAt(data, -1).has_value());
	EXPECT_FALSE(StringAt(data, 100).has_value());
}

TEST(EditorScripts, SearchesTheCodeGoingRound)
{
	const FakeProgram fake;
	const auto program = fake.View();
	const auto* main = &fake.scripts.at(0);
	const auto range = RangeOf(fake.code, *main);
	EXPECT_EQ(FindNext(program, main, range, "say", std::nullopt), 1u);
	EXPECT_EQ(FindNext(program, main, range, "helper", std::nullopt), 5u);
	// From after the only match, it goes round to it again
	EXPECT_EQ(FindNext(program, main, range, "say", 1u), 1u);
	EXPECT_EQ(FindNext(program, main, range, "0x0003", std::nullopt), 3u);
	EXPECT_FALSE(FindNext(program, main, range, "nothing like it", std::nullopt).has_value());
	EXPECT_FALSE(FindNext(program, main, range, "", std::nullopt).has_value());
}

TEST(EditorScripts, ReadsAndWritesValuesByType)
{
	EXPECT_EQ(FormatValue(VMValue(int32_t {-3}), DataType::Int), "-3");
	EXPECT_EQ(FormatValue(VMValue(2.5f), DataType::Float), "2.5");
	EXPECT_EQ(FormatValue(VMValue(int32_t {1}), DataType::Boolean), "true");
	EXPECT_EQ(FormatValue(VMValue(uint32_t {12}), DataType::Object), "object 12");

	EXPECT_EQ(ParseValue(" 42 ", DataType::Int)->intVal, 42);
	EXPECT_FLOAT_EQ(ParseValue("1.25", DataType::Float)->floatVal, 1.25f);
	EXPECT_EQ(ParseValue("true", DataType::Boolean)->intVal, 1);
	EXPECT_EQ(ParseValue("0", DataType::Boolean)->intVal, 0);
	EXPECT_EQ(ParseValue("77", DataType::Object)->uintVal, 77u);
	EXPECT_FALSE(ParseValue("", DataType::Int).has_value());
	EXPECT_FALSE(ParseValue("4x", DataType::Int).has_value());
	EXPECT_FALSE(ParseValue("maybe", DataType::Boolean).has_value());
	EXPECT_FALSE(ParseValue("-1", DataType::Object).has_value());
}

TEST(EditorScripts, CountsNativesAndTheirCoverage)
{
	const FakeProgram fake;
	const auto all = NativesCalled(fake.code, {.begin = 0, .end = static_cast<uint32_t>(fake.code.size())});
	ASSERT_EQ(all.size(), 2u);
	EXPECT_EQ(all.at(0).native, 1u);
	EXPECT_EQ(all.at(0).calls, 1u);
	EXPECT_EQ(all.at(1).native, 2u);
	EXPECT_EQ(all.at(1).calls, 2u);

	const std::array<uint32_t, 2> unwritten {2, 5};
	EXPECT_TRUE(IsImplemented(1, unwritten));
	EXPECT_FALSE(IsImplemented(2, unwritten));
	const auto coverage = CoverageOf(all, unwritten);
	EXPECT_EQ(coverage.used, 2u);
	EXPECT_EQ(coverage.implemented, 1u);

	const auto helper = NativesCalled(fake.code, RangeOf(fake.code, fake.scripts.at(1)));
	ASSERT_EQ(helper.size(), 1u);
	EXPECT_EQ(helper.at(0).native, 2u);
}

TEST(EditorScripts, SaysWhatTasksAreDoing)
{
	lhvm::VMTask task;
	task.id = 3;
	EXPECT_EQ(StateOf(task, false), TaskState::Running);
	EXPECT_EQ(StateOf(task, true), TaskState::Held);
	task.sleeping = true;
	EXPECT_EQ(StateOf(task, false), TaskState::Sleeping);
	task.waitingTaskId = 8;
	EXPECT_EQ(StateOf(task, false), TaskState::Waiting);
	EXPECT_EQ(Describe(task, false, "Helper"), "Waits for task 8, Helper to finish");
	task.stop = true;
	EXPECT_EQ(StateOf(task, true), TaskState::Stopping);
	for (const auto state : {TaskState::Running, TaskState::Held, TaskState::Waiting, TaskState::Sleeping,
	                         TaskState::InExceptionHandler, TaskState::Stopping})
	{
		EXPECT_FALSE(Name(state).empty());
	}
}

TEST(EditorScripts, MapsSourceLinesToInstructions)
{
	DecompiledSource source;
	source.lines = {"begin script Main", "  say \"hello\"", "  score = count", "end script Main"};
	source.lineAddresses = {0u, 0u, 2u, 6u};
	EXPECT_EQ(LineOf(source, 0), 0u);
	EXPECT_EQ(LineOf(source, 1), 0u);
	EXPECT_EQ(LineOf(source, 3), 2u);
	EXPECT_EQ(LineOf(source, 9), 3u);
	source.lineAddresses = {std::nullopt, 4u, std::nullopt, std::nullopt};
	EXPECT_FALSE(LineOf(source, 2).has_value());
}

TEST(EditorScripts, HighlightsTheScriptLanguage)
{
	EXPECT_TRUE(IsKeyword("begin"));
	EXPECT_TRUE(IsKeyword("ELSIF"));
	EXPECT_FALSE(IsKeyword("score"));

	const std::string line = "  wait until score > 2.5 and \"done\" // the end";
	const auto tokens = HighlightSource(line);
	std::string joined;
	for (const auto& token : tokens)
	{
		joined += token.text;
	}
	// The pieces together make the whole line
	EXPECT_EQ(joined, line);
	const auto kindOf = [&tokens](std::string_view text) {
		for (const auto& token : tokens)
		{
			if (token.text == text)
			{
				return token.kind;
			}
		}
		return SourceToken::Kind::Text;
	};
	EXPECT_EQ(kindOf("wait"), SourceToken::Kind::Keyword);
	EXPECT_EQ(kindOf("until"), SourceToken::Kind::Keyword);
	EXPECT_EQ(kindOf("and"), SourceToken::Kind::Keyword);
	EXPECT_EQ(kindOf("2.5"), SourceToken::Kind::Number);
	EXPECT_EQ(kindOf("\"done\""), SourceToken::Kind::String);
	EXPECT_EQ(kindOf("// the end"), SourceToken::Kind::Comment);
	EXPECT_TRUE(HighlightSource("").empty());
}

TEST(EditorScripts, KeepsTheLastLinesOfTheLog)
{
	LogRing ring(3);
	ring.Push(LogRing::Level::Info, "one");
	ring.Push(LogRing::Level::Error, "two");
	ring.Push(LogRing::Level::Warning, "three");
	ring.Push(LogRing::Level::Info, "four");
	ASSERT_EQ(ring.Lines().size(), 3u);
	EXPECT_EQ(ring.Lines().front().text, "two");
	EXPECT_EQ(ring.Lines().back().number, 3u);
	EXPECT_EQ(ring.Errors(), 1u);
	EXPECT_EQ(ring.Warnings(), 1u);
	ring.Clear();
	EXPECT_TRUE(ring.Lines().empty());
	EXPECT_EQ(ring.Errors(), 0u);
	EXPECT_EQ(ring.Next(), 4u);
	LogRing none(0);
	none.Push(LogRing::Level::Info, "dropped");
	EXPECT_TRUE(none.Lines().empty());
}

namespace
{
/// A small program compiled from source with no game data: one script that sets a global and tests it
constexpr std::string_view k_CountingSource = "global G\n"
                                              "begin script Main\n"
                                              "start\n"
                                              "\tG = 1\n"
                                              "\tif G == 1\n"
                                              "\t\tG = 2\n"
                                              "\tend if\n"
                                              "end script Main\n";

/// The program loaded into a machine, as the Scripts panel sees it
class CompiledProgramTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::array sources {lhvm::chl::SourceFile {.name = "Main.txt", .text = std::string(k_CountingSource)}};
		const auto compiled = lhvm::chl::Compile(sources);
		ASSERT_TRUE(compiled.Succeeded());
		ASSERT_EQ(_vm.LoadBinary(*compiled.program), EXIT_SUCCESS);
		ASSERT_EQ(_vm.GetScripts().size(), 1u);
	}

	[[nodiscard]] Program View() const
	{
		return {.code = _vm.GetInstructions(),
		        .scripts = _vm.GetScripts(),
		        .globals = _vm.GetVariables(),
		        .natives = {},
		        .data = _vm.GetData()};
	}
	[[nodiscard]] const VMScript& Main() const { return _vm.GetScripts().front(); }

	lhvm::LHVM _vm;
};

/// Whether an instruction's operand is an address in the code, which moves with the code
[[nodiscard]] bool TakesACodeAddress(Opcode code)
{
	// Opcode::Wait is the conditional jump
	return code == Opcode::Jmp || code == Opcode::Wait || code == Opcode::Except;
}

/// The number reading the script model had before it took the script language's own: an optional minus, then
/// decimals; strtof alone would also take leading spaces, a plus sign and hexadecimal
std::optional<float> FormerParseFloat(std::string_view text)
{
	auto digits = text;
	if (digits.starts_with('-'))
	{
		digits.remove_prefix(1);
	}
	if (digits.empty())
	{
		return std::nullopt;
	}
	const auto first = static_cast<unsigned char>(digits.front());
	if (std::isdigit(first) == 0 && first != '.' && first != 'i' && first != 'I' && first != 'n' && first != 'N')
	{
		return std::nullopt;
	}
	if (digits.size() > 1 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
	{
		return std::nullopt;
	}
	const std::string terminated(text);
	char* end = nullptr;
	errno = 0;
	const float value = std::strtof(terminated.c_str(), &end);
	if (end != terminated.c_str() + terminated.size() || (errno == ERANGE && std::isinf(value)))
	{
		return std::nullopt;
	}
	return value;
}
} // namespace

TEST_F(CompiledProgramTest, DecompilesWithEveryLineLinkedToItsCode)
{
	ASSERT_TRUE(HasDecompiler());
	const auto source = Decompile(View(), Main());
	ASSERT_TRUE(source.has_value());
	EXPECT_TRUE(source->diagnostics.empty());
	ASSERT_FALSE(source->lines.empty());
	ASSERT_EQ(source->lineAddresses.size(), source->lines.size());
	const auto codeSize = _vm.GetInstructions().size();
	for (size_t line = 0; line < source->lines.size(); ++line)
	{
		if (source->lines[line].find_first_not_of(' ') == std::string::npos)
		{
			continue;
		}
		ASSERT_TRUE(source->lineAddresses[line].has_value()) << "line " << line << ": " << source->lines[line];
		EXPECT_GE(*source->lineAddresses[line], Main().instructionAddress) << "line " << line;
		EXPECT_LT(*source->lineAddresses[line], codeSize) << "line " << line;
	}
}

TEST_F(CompiledProgramTest, RecompilingTheUnchangedSourceGivesTheSameInstructions)
{
	ASSERT_TRUE(HasCompiler());
	const auto source = Decompile(View(), Main());
	ASSERT_TRUE(source.has_value());
	std::string text;
	for (const auto& line : source->lines)
	{
		text += line;
		text += '\n';
	}

	const auto result = Compile(View(), Main(), text);
	ASSERT_TRUE(result.compiled) << (result.diagnostics.empty() ? "" : result.diagnostics.front());
	ASSERT_NE(result.program, nullptr);

	// the new code goes after the old, so the script's code is moved by the old code's length
	const auto& before = _vm.GetInstructions();
	const auto& after = result.program->GetInstructions();
	const auto oldStart = Main().instructionAddress;
	const auto newStart = static_cast<uint32_t>(before.size());
	ASSERT_EQ(result.program->GetScripts().size(), 1u);
	EXPECT_EQ(result.program->GetScripts().front().instructionAddress, newStart);
	ASSERT_EQ(after.size() - newStart, before.size() - oldStart);
	for (size_t i = 0; i < before.size() - oldStart; ++i)
	{
		const auto& old = before[oldStart + i];
		const auto& now = after[newStart + i];
		EXPECT_EQ(now.code, old.code) << "instruction " << i;
		EXPECT_EQ(now.mode, old.mode) << "instruction " << i;
		EXPECT_EQ(now.type, old.type) << "instruction " << i;
		const auto shift = TakesACodeAddress(old.code) ? newStart - oldStart : 0u;
		EXPECT_EQ(now.data.uintVal, old.data.uintVal + shift) << "instruction " << i;
	}
}

TEST_F(CompiledProgramTest, ASyntaxErrorGivesADiagnosticAndNoProgram)
{
	const auto result = Compile(View(), Main(), "begin script Main\nstart\n\tG = \nend script Main\n");
	EXPECT_FALSE(result.compiled);
	EXPECT_EQ(result.program, nullptr);
	ASSERT_FALSE(result.diagnostics.empty());
	// line:column: message
	const auto& diagnostic = result.diagnostics.front();
	const auto colon = diagnostic.find(':');
	ASSERT_NE(colon, std::string::npos);
	const auto second = diagnostic.find(": ", colon + 1);
	ASSERT_NE(second, std::string::npos);
	EXPECT_EQ(diagnostic.substr(0, colon).find_first_not_of("0123456789"), std::string::npos) << diagnostic;
	EXPECT_EQ(diagnostic.substr(colon + 1, second - colon - 1).find_first_not_of("0123456789"), std::string::npos)
	    << diagnostic;
	EXPECT_GT(diagnostic.size(), second + 2) << diagnostic;
}

TEST(EditorScripts, ReadsNumbersAsTheFormerCopyDid)
{
	constexpr std::array k_Cases = {"0",        "1",     "-1",    "2.5", "-0.25", ".5",    "-.5",  "1.",    "1e3",
	                                "1E-3",     "-2e2",  "+1",    " 1",  "1 ",    "\t1",   "0x10", "-0x10", "0X1",
	                                "1e40",     "-1e40", "1e-50", "inf", "-inf",  "INF",   "nan",  "NaN",   "-nan",
	                                "infinity", "",      "-",     "--1", "abc",   "1.2.3", "1,5",  "e3"};
	for (const std::string_view text : k_Cases)
	{
		const auto former = FormerParseFloat(text);
		const auto shared = lhvm::chl::ParseFloat(text);
		ASSERT_EQ(shared.has_value(), former.has_value()) << "\"" << text << "\"";
		if (former.has_value())
		{
			// the same bits, so that not-a-number compares too
			EXPECT_EQ(std::bit_cast<uint32_t>(*shared), std::bit_cast<uint32_t>(*former)) << "\"" << text << "\"";
		}
	}
	// and the value editor reads floats with it
	const auto value = ParseValue("-2.5", DataType::Float);
	ASSERT_TRUE(value.has_value());
	EXPECT_EQ(value->floatVal, -2.5f);
	EXPECT_FALSE(ParseValue("+2.5", DataType::Float).has_value());
}

namespace
{
/// A machine running a program whose one script counts up a global by one each turn, round and round
class DebuggerTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const std::vector<VMInstruction> code {
		    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 0: PUSHI counter
		    Make(Opcode::Push, VMValue(int32_t {1})),                                    // 1: PUSHI 1
		    Make(Opcode::Add, VMValue(uint32_t {0})),                                    // 2: ADDI
		    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 3: POPI counter
		    Make(Opcode::Jmp, VMValue(uint32_t {0})),                                    // 4: JMP 0, back, which ends the turn
		    Make(Opcode::End),                                                           // 5
		};
		const std::vector<VMScript> scripts {VMScript("Counter", "test.txt", lhvm::ScriptType::Script, 1, {}, 0, 0, 1)};
		_vm.Initialise(&_natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		ASSERT_EQ(_vm.LoadBinary(lhvm::LHVMFile(lhvm::LHVMVersion::BlackAndWhite, {"counter"}, code, {}, scripts, {})),
		          EXIT_SUCCESS);
		_task = _vm.StartScript("Counter", lhvm::ScriptType::All);
		ASSERT_NE(_task, 0u);
	}

	[[nodiscard]] int32_t Counter() const { return _vm.GetVariables().at(1).value.intVal; }
	[[nodiscard]] uint32_t Address() const { return _vm.GetTasks().at(_task).instructionAddress; }

	std::vector<lhvm::NativeFunction> _natives {lhvm::NativeFunction(nullptr, 0, 0, "NONE")};
	lhvm::LHVM _vm;
	uint32_t _task {0};
};
} // namespace

TEST_F(DebuggerTest, RunsFreelyWithoutTheDebugger)
{
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 2);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
}

TEST_F(DebuggerTest, BreakpointsHoldTasksBeforeTheirInstruction)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 0);
	// Held, it sits out the turns
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 0);
}

TEST_F(DebuggerTest, SteppingRunsOneInstructionAtATime)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.StepTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 1);
	EXPECT_EQ(Address(), 4u);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
}

TEST_F(DebuggerTest, ContinuingRunsPastTheBreakpointUntilItComesRound)
{
	_vm.SetBreakpoint(3, true);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.ContinueTask(_task);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 1);
	// The next time round it stops there again
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_TRUE(_vm.IsTaskHeld(_task));
	EXPECT_EQ(Address(), 3u);
	EXPECT_EQ(Counter(), 1);
	_vm.SetBreakpoint(3, false);
	_vm.ContinueTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 3);
}

TEST_F(DebuggerTest, HoldsByHandAndChangesVariables)
{
	_vm.HoldTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 0);
	_vm.SetVariable(1, VMValue(int32_t {40}));
	_vm.ContinueTask(_task);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 41);
	// Stopping a held task forgets it was held
	_vm.HoldTask(_task);
	_vm.StopTask(_task);
	EXPECT_FALSE(_vm.IsTaskHeld(_task));
}

TEST_F(DebuggerTest, UpdatingTheProgramKeepsTasksAndValues)
{
	_vm.LookIn(lhvm::ScriptType::All);
	ASSERT_EQ(Counter(), 1);
	auto code = _vm.GetInstructions();
	auto scripts = _vm.GetScripts();
	// A newer build adds a script that sets a new global, after the existing code
	code.push_back(Make(Opcode::Push, VMValue(int32_t {7})));
	code.push_back(Make(Opcode::Pop, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference));
	code.push_back(Make(Opcode::End));
	scripts.emplace_back("Setter", "test.txt", lhvm::ScriptType::Script, 2, std::vector<std::string> {}, 6, 0, 2);
	const lhvm::LHVMFile newer(lhvm::LHVMVersion::BlackAndWhite, {"counter", "added"}, code, {}, scripts, {});
	ASSERT_EQ(_vm.UpdateProgram(newer), EXIT_SUCCESS);
	EXPECT_EQ(_vm.GetVariables().size(), 3u);
	EXPECT_EQ(Counter(), 1);
	ASSERT_TRUE(_vm.GetTasks().contains(_task));
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(Counter(), 2);
	ASSERT_NE(_vm.StartScript("Setter", lhvm::ScriptType::All), 0u);
	_vm.LookIn(lhvm::ScriptType::All);
	EXPECT_EQ(_vm.GetVariables().at(2).value.intVal, 7);
	// A build that has lost code is refused
	const lhvm::LHVMFile older(lhvm::LHVMVersion::BlackAndWhite, {"counter"}, {}, {}, {}, {});
	EXPECT_EQ(_vm.UpdateProgram(older), EXIT_FAILURE);
	EXPECT_EQ(_vm.GetInstructions().size(), code.size());
}
