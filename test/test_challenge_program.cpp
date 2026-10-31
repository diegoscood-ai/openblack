/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shipped challenge program (Scripts/Quests/challenge.chl under OPENBLACK_GAME_PATH) as a whole: its size, and
// which of its script families reach only natives openblack has written. A family is the scripts of one source file,
// named as the progress page names it; it reaches the natives its own scripts call and those of every script they
// start, and so on. A native counts as unwritten when its own body in src/CHLApi.cpp logs "not implemented" with no
// case; one that logs it only for some cases (a property number) counts as written, as on the progress page.
// Then two of the program's rows on their own: CREATE_TIMER, and the globals every task shares.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMFile.h>
#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/ChallengeProgramScan.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::lhvm;
using namespace openblack::test::challenge_scan;

namespace
{

/// The natives a family's scripts call, and those of every script they start, and so on: each script's code runs from
/// its first instruction to the next script's
std::set<uint32_t> NativesReached(const LHVMFile& program, std::string_view file)
{
	const auto& scripts = program.GetScripts();
	const auto& code = program.GetInstructions();
	std::vector<uint32_t> starts;
	starts.reserve(scripts.size());
	std::ranges::transform(scripts, std::back_inserter(starts), [](const VMScript& s) { return s.instructionAddress; });
	std::ranges::sort(starts);
	const auto codeEnd = [&](uint32_t start) {
		const auto next = std::ranges::upper_bound(starts, start);
		return next == starts.end() ? static_cast<uint32_t>(code.size()) : *next;
	};

	std::vector<uint32_t> pending; // indices into the scripts
	for (uint32_t i = 0; i < scripts.size(); ++i)
	{
		if (scripts[i].filename == file)
		{
			pending.push_back(i);
		}
	}
	std::set<uint32_t> seen;
	std::set<uint32_t> natives;
	while (!pending.empty())
	{
		const auto index = pending.back();
		pending.pop_back();
		if (index >= scripts.size() || !seen.insert(index).second)
		{
			continue;
		}
		const auto start = scripts[index].instructionAddress;
		for (auto at = start; at < codeEnd(start) && at < code.size(); ++at)
		{
			if (code[at].code == Opcode::Sys)
			{
				natives.insert(code[at].data.uintVal);
			}
			else if (code[at].code == Opcode::Run && code[at].data.uintVal > 0)
			{
				// a script is started by its number, the first being 1
				pending.push_back(code[at].data.uintVal - 1);
			}
		}
	}
	return natives;
}

/// The names of the natives of `reached` that are unwritten, or past the end of the table
std::vector<std::string> UnwrittenReached(const std::set<uint32_t>& reached, const std::set<uint32_t>& unwritten,
                                          const std::vector<std::pair<std::string, std::string>>& bindings)
{
	std::vector<std::string> names;
	for (const auto native : reached)
	{
		if (native >= bindings.size())
		{
			names.push_back("native " + std::to_string(native));
		}
		else if (unwritten.contains(native))
		{
			names.push_back(bindings[native].first);
		}
	}
	return names;
}

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// A program of three scripts: Alpha calls native 1 and starts Helper; Beta calls native 2; Helper, of another file,
/// calls native 3
LHVMFile SmallProgram()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Sys, VMValue(uint32_t {1})), // 0: Alpha
	    Make(Opcode::Run, VMValue(uint32_t {3})), // 1: starts Helper, the third script
	    Make(Opcode::End),                        // 2
	    Make(Opcode::Sys, VMValue(uint32_t {2})), // 3: Beta
	    Make(Opcode::End),                        // 4
	    Make(Opcode::Sys, VMValue(uint32_t {3})), // 5: Helper
	    Make(Opcode::End),                        // 6
	};
	const std::vector<VMScript> scripts {
	    VMScript("Alpha", "Alpha One.txt", ScriptType::Script, 1, {}, 0, 0, 1),
	    VMScript("Beta", "beta.txt", ScriptType::Script, 1, {}, 3, 0, 2),
	    VMScript("Helper", "Helper.txt", ScriptType::Script, 1, {}, 5, 0, 3),
	};
	return {LHVMVersion::BlackAndWhite, {"flag"}, code, {}, scripts, {}};
}

/// A source with four natives: none, one written, one unwritten, and one unwritten for one case only; the binding of
/// the last spans two lines
constexpr std::string_view k_SmallSource = "void None() {} // 000 NONE\r\n"
                                           "\r\n"
                                           "void One() // 001 ONE\r\n"
                                           "{\r\n"
                                           "\tPushf(1.0f);\r\n"
                                           "}\r\n"
                                           "\r\n"
                                           "void Two() // 002 TWO\r\n"
                                           "{\r\n"
                                           "\tNotImplemented();\r\n"
                                           "}\r\n"
                                           "\r\n"
                                           "void Three() // 003 THREE\r\n"
                                           "{\r\n"
                                           "\tNotImplemented(5);\r\n"
                                           "}\r\n"
                                           "\r\n"
                                           "void InitFunctionsTable0()\r\n"
                                           "{\r\n"
                                           "\tCREATE_FUNCTION_BINDING(\"NONE\", 0, 0, None);\r\n"
                                           "\tCREATE_FUNCTION_BINDING(\"ONE\", 0, 1, One);\r\n"
                                           "\tCREATE_FUNCTION_BINDING(\"TWO\", -1, 0, Two);\r\n"
                                           "\tCREATE_FUNCTION_BINDING(\"THREE\", 1, 0,\r\n"
                                           "\t                        Three);\r\n"
                                           "}\r\n";

} // namespace

TEST(ChallengeProgram, TheShippedProgramHas514Scripts398GlobalsAnd157160Instructions)
{
	const auto path = ChallengePath();
	if (!path)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH does not point at the game";
	}
	LHVMFile program;
	program.Open(*path);
	ASSERT_TRUE(program.IsLoaded());
	EXPECT_EQ(program.GetScripts().size(), 514u);
	EXPECT_EQ(program.GetVariablesNames().size(), 398u);
	EXPECT_EQ(program.GetInstructions().size(), 157160u);
}

TEST(ChallengeProgram, AProgramReadBackHasTheScriptsGlobalsAndInstructionsItWasWrittenWith)
{
	const auto written = SmallProgram();
	std::ostringstream stream(std::ios::binary);
	written.Write(stream);
	const auto bytes = stream.str();
	LHVMFile program;
	program.Open(std::vector<uint8_t>(bytes.begin(), bytes.end()));
	ASSERT_TRUE(program.IsLoaded());
	EXPECT_EQ(program.GetScripts().size(), 3u);
	EXPECT_EQ(program.GetVariablesNames().size(), 1u);
	EXPECT_EQ(program.GetInstructions().size(), 7u);
}

TEST(ChallengeProgram, TheSourcesBindingsAreTheNativeTableInItsOrder)
{
	const auto source = ReadText(SourceDir() / "src" / "CHLApi.cpp");
	ASSERT_FALSE(source.empty()) << "src/CHLApi.cpp not found under " << SourceDir();
	const auto bindings = Bindings(source);
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_EQ(bindings.size(), table.size());
	for (size_t i = 0; i < table.size(); ++i)
	{
		EXPECT_EQ(bindings[i].first, table[i].name) << "native " << i;
	}
	// every native runs a function the source defines
	const auto bodies = FunctionBodies(source);
	for (const auto& [name, function] : bindings)
	{
		EXPECT_TRUE(std::ranges::any_of(bodies, [&](const auto& body) { return body.first == function; }))
		    << name << " runs " << function << ", which is not defined at the start of a line";
	}
	// the delete has no unwritten path left: a puzzle game is deleted too
	const auto del = std::ranges::find_if(bindings, [](const auto& b) { return b.first == k_DeleteNative; });
	ASSERT_NE(del, bindings.end());
	const auto delBody = std::ranges::find_if(bodies, [&](const auto& b) { return b.first == del->second; });
	ASSERT_NE(delBody, bodies.end());
	EXPECT_EQ(delBody->second.find(k_UnwrittenCall), std::string::npos);
}

TEST(ChallengeProgram, ANativeIsUnwrittenWhenItsOwnBodyLogsWithNoCase)
{
	const auto bindings = Bindings(k_SmallSource);
	ASSERT_EQ(bindings.size(), 4u);
	EXPECT_EQ(bindings[3], std::make_pair(std::string("THREE"), std::string("Three")));
	EXPECT_EQ(UnwrittenNatives(k_SmallSource), std::set<uint32_t> {2});
}

TEST(ChallengeProgram, AFamilyReachesTheNativesOfTheScriptsItStarts)
{
	const auto program = SmallProgram();
	const auto bindings = Bindings(k_SmallSource);
	const auto unwritten = UnwrittenNatives(k_SmallSource);

	// the page's name for a file folds case, spaces and the extension away
	const auto alphaFiles = FamilyFiles(program, "Alpha One");
	ASSERT_EQ(alphaFiles, std::set<std::string> {"Alpha One.txt"});
	const auto alpha = NativesReached(program, *alphaFiles.begin());
	EXPECT_EQ(alpha, (std::set<uint32_t> {1, 3}));
	EXPECT_TRUE(UnwrittenReached(alpha, unwritten, bindings).empty());

	const auto betaFiles = FamilyFiles(program, "Beta");
	ASSERT_EQ(betaFiles, std::set<std::string> {"beta.txt"});
	const auto beta = NativesReached(program, *betaFiles.begin());
	EXPECT_EQ(UnwrittenReached(beta, unwritten, bindings), std::vector<std::string> {"TWO"});

	// a native past the table's end is never written
	EXPECT_EQ(UnwrittenReached({9}, unwritten, bindings), std::vector<std::string> {"native 9"});
}

TEST(ChallengeProgram, EveryRunningFamilyOfTheShippedProgramCallsOnlyWrittenNatives)
{
	const auto path = ChallengePath();
	if (!path)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH does not point at the game";
	}
	LHVMFile program;
	program.Open(*path);
	ASSERT_TRUE(program.IsLoaded());
	const auto source = ReadText(SourceDir() / "src" / "CHLApi.cpp");
	ASSERT_FALSE(source.empty()) << "src/CHLApi.cpp not found under " << SourceDir();
	const auto bindings = Bindings(source);
	const auto unwritten = UnwrittenNatives(source);

	for (const auto family : k_RunningFamilies)
	{
		const auto files = FamilyFiles(program, family);
		ASSERT_EQ(files.size(), 1u) << family << " is not the name of exactly one source file";
		const auto reached = NativesReached(program, *files.begin());
		EXPECT_FALSE(reached.empty()) << family;
		EXPECT_EQ(UnwrittenReached(reached, unwritten, bindings), std::vector<std::string> {})
		    << family << " reaches unwritten natives";
	}
}

TEST(ChallengeProgram, TheGlobalsAreSharedByEveryTask)
{
	// One task sets a global and ends; another, started with it, keeps copying that global into a second one
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(int32_t {7})),                                    // 0: Writer: PUSHI 7
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference),  // 1: POPI flag
	    Make(Opcode::End),                                                           // 2
	    Make(Opcode::Push, VMValue(uint32_t {1}), DataType::Int, VMMode::Reference), // 3: Reader: PUSHI flag
	    Make(Opcode::Pop, VMValue(uint32_t {2}), DataType::Int, VMMode::Reference),  // 4: POPI seen
	    Make(Opcode::Jmp, VMValue(uint32_t {3})),                                    // 5: JMP 3, which ends the turn
	    Make(Opcode::End),                                                           // 6
	};
	const std::vector<VMScript> scripts {
	    VMScript("Writer", "writer.txt", ScriptType::Script, 2, {}, 0, 0, 1),
	    VMScript("Reader", "reader.txt", ScriptType::Script, 2, {}, 3, 0, 2),
	};
	const LHVMFile program {LHVMVersion::BlackAndWhite, {"flag", "seen"}, code, {1, 2}, scripts, {}};
	std::vector<NativeFunction> natives {NativeFunction(nullptr, 0, 0, "NONE")};
	LHVM vm;
	vm.Initialise(&natives, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	ASSERT_EQ(vm.LoadBinary(program), EXIT_SUCCESS);
	vm.LookIn(ScriptType::All);
	vm.LookIn(ScriptType::All);
	EXPECT_EQ(vm.GetVariables().at(1).value.intVal, 7);
	EXPECT_EQ(vm.GetVariables().at(2).value.intVal, 7);
}

namespace
{

/// The scripts' table of objects, as far as CREATE_TIMER reaches it: what each Register was given
class RegisteredThings final: public ecs::systems::ScriptObjectsSystemInterface
{
public:
	bool Register(entt::entity object, bool createdByScript) override
	{
		registered.emplace_back(object, createdByScript);
		return true;
	}
	void EnterNative(uint32_t /*native*/) override {}
	entt::entity Fetch(entt::entity object) override { return object; }
	void AddReference(entt::entity /*object*/) override {}
	void RemoveReference(entt::entity /*object*/) override {}
	void ReleaseFromScript(entt::entity /*object*/) override {}
	void Replace(entt::entity /*from*/, entt::entity /*to*/) override {}
	void Reset() override {}
	void ReleaseUnreferenced() override {}
	bool Disband(entt::entity /*container*/) override { return false; }
	[[nodiscard]] std::vector<ecs::systems::ScriptObjectPlace> Places() const override { return {}; }
	[[nodiscard]] uint32_t TimesFull() const override { return 0; }
	void ClearForNewLand() override {}

	std::vector<std::pair<entt::entity, bool>> registered;
};

class ChallengeTimer: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		// entity 0 is "no thing" for a script id
		registry.Create();
		Locator::vm::emplace<LHVM>();
		_things = &static_cast<RegisteredThings&>(Locator::scriptObjects::emplace<RegisteredThings>());
		game_clock::Reset();
	}
	void TearDown() override { game_clock::Reset(); }

	RegisteredThings* _things {nullptr};
	chlapi::CHLApi _api;

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::scriptObjects> _restoreScriptObjects;
};

} // namespace

TEST_F(ChallengeTimer, CreateTimerMakesATimerHeldByTheScriptRunningForTheSeconds)
{
	game_clock::SetTurn(50);
	const auto& table = _api.GetFunctionsTable();
	const auto native =
	    std::ranges::find_if(table, [](const NativeFunction& f) { return std::string_view(f.name) == "CREATE_TIMER"; });
	ASSERT_NE(native, table.end());
	auto& vm = Locator::vm::value();
	vm.Pushf(30.0f);
	native->impl();
	DataType type {};
	const auto given = vm.Pop(type);
	EXPECT_EQ(type, DataType::Object);
	const auto timer = static_cast<entt::entity>(given.uintVal);
	ASSERT_NE(given.uintVal, 0u);

	// a timer set at this turn for 30 seconds of 100 ms turns, as the original
	// (docs/wiki/scripts/script-commands.md#creating-things)
	const auto* component = Locator::entitiesRegistry::value().TryGet<const ecs::components::ScriptTimer>(timer);
	ASSERT_NE(component, nullptr);
	EXPECT_EQ(component->timer.setTurn, 50u);
	EXPECT_EQ(component->timer.turns, 300);
	// held by the script as a thing it created
	ASSERT_EQ(_things->registered.size(), 1u);
	EXPECT_EQ(_things->registered[0], std::make_pair(timer, true));
}
