/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The shipped challenge program (Scripts/Quests/challenge.chl under OPENBLACK_GAME_PATH) run in openblack's own script
// machine, in a world with no game in it. Every native is a fake that records its call, takes its arguments and gives
// back zero for each value it returns, as the machine does for a native with no body; the natives that stop scripts
// run as the game's own (they touch only the machine), and a challenge's record takes its arguments as the game's
// does. The program is started as the original starts it for a land, and each family the progress page lists as
// running is started on its own; then a minute of script time is run. What is pinned: the order a new game's first
// scripts start in, as the original's own run of it shows; the globals the land set-ups write; that every script
// a family starts is one its RUN instructions reach; and that at run time no family reaches an unwritten native or
// raises an error. The order of the rest is the world's own (its natives all answer zero), so it is not pinned. A family
// that waits for a creature, a click or a camera stops where it waits: only what it reaches before is checked.

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMFile.h>
#include <gtest/gtest.h>

#include "CHLApi.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptChallengeSnapshots.h"
#include "support/ChallengeProgramScan.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::lhvm;
using namespace openblack::test::challenge_scan;

namespace
{

/// A minute of script time, a tenth of a second a turn
constexpr int k_Turns = 600;

/// The natives that stop scripts: they run as the game's own, which touch only the script machine
constexpr std::array<std::string_view, 5> k_StoppingNatives = {"STOP_ALL_SCRIPTS_EXCLUDING",
                                                               "STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING", "STOP_SCRIPT",
                                                               "STOP_SCRIPTS_IN_FILES", "STOP_SCRIPTS_IN_FILES_EXCLUDING"};

/// One native call: the native, and the script whose task called it
struct NativeCall
{
	uint32_t native {0};
	std::string script;
};

/// A program run in the script machine (Locator::vm) with fake natives: every task that ran, by its number, which is
/// the order the tasks started in; the natives called; and the machine's errors
class ScriptWorld
{
public:
	/// The natives take the names and stack counts of `table`, the game's own table
	explicit ScriptWorld(const std::vector<NativeFunction>& table)
	{
		_natives.reserve(table.size());
		for (uint32_t native = 0; native < table.size(); ++native)
		{
			const auto& entry = table[native];
			_natives.emplace_back(Fake(native, entry), entry.stackIn, static_cast<int32_t>(entry.stackOut), entry.name);
		}
		_vm = &Locator::vm::emplace<LHVM>();
		_vm->Initialise(
		    &_natives, nullptr, nullptr, [this](uint32_t task) { Keep(task); },
		    [this](ErrorCode code, const std::string& text, uint32_t data) {
			    errors.push_back(std::to_string(static_cast<int>(code)) + " " + text + " " + std::to_string(data));
		    },
		    nullptr, nullptr);
	}
	ScriptWorld(const ScriptWorld&) = delete;
	ScriptWorld& operator=(const ScriptWorld&) = delete;
	ScriptWorld(ScriptWorld&&) = delete;
	ScriptWorld& operator=(ScriptWorld&&) = delete;
	~ScriptWorld() = default;

	/// The program read: the scripts that start by themselves are started
	[[nodiscard]] bool Load(const LHVMFile& program)
	{
		const bool loaded = _vm->LoadBinary(program) == EXIT_SUCCESS;
		KeepRunning();
		return loaded;
	}

	/// Every task stopped, and what was recorded until then forgotten
	void StopAll()
	{
		_vm->StopAllTasks();
		tasks.clear();
		calls.clear();
		errors.clear();
	}

	/// A script started by name, as a RUN of it with zero for each of its arguments starts it
	void Start(const VMScript& script)
	{
		for (uint32_t i = 0; i < script.parameterCount; ++i)
		{
			_vm->Pushf(0.0f);
		}
		_vm->StartScript(script.name, ScriptType::All);
		KeepRunning();
	}

	/// A script of no arguments started by name, as the game starts one
	void Start(const std::string& name)
	{
		_vm->StartScript(name, ScriptType::All);
		KeepRunning();
	}

	/// The turns run, every kind of script in each, as outside the temple
	void Turns(int count)
	{
		for (int turn = 0; turn < count; ++turn)
		{
			_vm->LookIn(ScriptType::All);
			KeepRunning();
		}
	}

	/// The scripts in the order their first task started
	[[nodiscard]] std::vector<std::string> StartOrder() const
	{
		std::vector<std::string> order;
		for (const auto& [number, script] : tasks)
		{
			if (std::ranges::find(order, script) == order.end())
			{
				order.push_back(script);
			}
		}
		return order;
	}

	/// A global's value, read as a number
	[[nodiscard]] std::optional<float> Global(std::string_view name) const
	{
		const auto& variables = _vm->GetVariables();
		const auto variable = std::ranges::find_if(variables, [name](const VMVar& v) { return v.name == name; });
		if (variable == variables.end())
		{
			return std::nullopt;
		}
		return variable->value.floatVal;
	}

	std::map<uint32_t, std::string> tasks;
	std::vector<NativeCall> calls;
	std::vector<std::string> errors;

private:
	/// The fake run for a native of the table
	std::function<void()> Fake(uint32_t native, const NativeFunction& entry)
	{
		if (entry.impl && std::ranges::find(k_StoppingNatives, std::string_view(entry.name)) != k_StoppingNatives.end())
		{
			return [this, native, impl = entry.impl]() {
				Record(native);
				impl();
			};
		}
		if (entry.name == "SNAPSHOT" || entry.name == "UPDATE_SNAPSHOT")
		{
			const auto call =
			    entry.name == "SNAPSHOT" ? script::challenge_snapshots::Call::Start : script::challenge_snapshots::Call::Update;
			return [this, native, call]() {
				Record(native);
				auto* vm = _vm;
				std::ignore =
				    script::challenge_snapshots::Read(call, {
				                                                .pop =
				                                                    [vm]() {
					                                                    script::challenge_snapshots::Value value;
					                                                    value.value = vm->Pop(value.type);
					                                                    return value;
				                                                    },
				                                                .text = [vm](uint32_t offset) { return vm->GetString(offset); },
				                                            });
			};
		}
		const auto arguments = std::max(entry.stackIn, 0);
		const auto results = entry.stackOut;
		return [this, native, arguments, results]() {
			Record(native);
			for (int32_t i = 0; i < arguments; ++i)
			{
				_vm->Pop();
			}
			for (uint32_t i = 0; i < results; ++i)
			{
				_vm->Pushf(0.0f);
			}
		};
	}

	void Record(uint32_t native)
	{
		const auto& running = _vm->GetTasks();
		const auto task = running.find(_vm->GetCurrentTaskNumber());
		calls.push_back({.native = native, .script = task != running.end() ? task->second.name : std::string()});
	}

	/// A task that ends is kept as it goes, so one that ends in the turn it started is still seen
	void Keep(uint32_t task) { tasks.emplace(task, _vm->GetTasks().at(task).name); }

	void KeepRunning()
	{
		for (const auto& [number, task] : _vm->GetTasks())
		{
			tasks.emplace(number, task.name);
		}
	}

	std::vector<NativeFunction> _natives;
	LHVM* _vm {nullptr};
};

/// The scripts, by index, that a script's RUN instructions start; its code runs from its first instruction to the next
/// script's
std::vector<uint32_t> RunTargets(const LHVMFile& program, uint32_t index)
{
	const auto& scripts = program.GetScripts();
	const auto& code = program.GetInstructions();
	const auto start = scripts[index].instructionAddress;
	auto end = static_cast<uint32_t>(code.size());
	for (const auto& script : scripts)
	{
		if (script.instructionAddress > start)
		{
			end = std::min(end, script.instructionAddress);
		}
	}
	std::vector<uint32_t> targets;
	for (auto at = start; at < end; ++at)
	{
		// a script is started by its number, the first being 1
		if (code[at].code == Opcode::Run && code[at].data.uintVal > 0)
		{
			targets.push_back(code[at].data.uintVal - 1);
		}
	}
	return targets;
}

/// A family's scripts that none of its scripts starts, in the program's order: those started from outside it
std::vector<uint32_t> Entries(const LHVMFile& program, const std::string& file)
{
	const auto& scripts = program.GetScripts();
	std::set<uint32_t> started;
	for (uint32_t i = 0; i < scripts.size(); ++i)
	{
		if (scripts[i].filename == file)
		{
			for (const auto target : RunTargets(program, i))
			{
				started.insert(target);
			}
		}
	}
	std::vector<uint32_t> entries;
	for (uint32_t i = 0; i < scripts.size(); ++i)
	{
		if (scripts[i].filename == file && !started.contains(i))
		{
			entries.push_back(i);
		}
	}
	return entries;
}

/// The names of the scripts a family reaches: its own, every script they start, and so on
std::set<std::string> ScriptsReached(const LHVMFile& program, const std::string& file)
{
	const auto& scripts = program.GetScripts();
	std::vector<uint32_t> pending;
	for (uint32_t i = 0; i < scripts.size(); ++i)
	{
		if (scripts[i].filename == file)
		{
			pending.push_back(i);
		}
	}
	std::set<uint32_t> seen;
	while (!pending.empty())
	{
		const auto index = pending.back();
		pending.pop_back();
		if (index >= scripts.size() || !seen.insert(index).second)
		{
			continue;
		}
		const auto targets = RunTargets(program, index);
		pending.insert(pending.end(), targets.begin(), targets.end());
	}
	std::set<std::string> names;
	for (const auto index : seen)
	{
		names.insert(scripts[index].name);
	}
	return names;
}

/// The unwritten natives, by name, that the run's tasks of `files`' scripts called
std::set<std::string> UnwrittenCalled(const ScriptWorld& world, const LHVMFile& program, const std::set<std::string>& files,
                                      const std::set<uint32_t>& unwritten, const std::vector<NativeFunction>& table)
{
	std::map<std::string, std::string> fileOf;
	for (const auto& script : program.GetScripts())
	{
		fileOf.emplace(script.name, script.filename);
	}
	std::set<std::string> names;
	for (const auto& call : world.calls)
	{
		const auto file = fileOf.find(call.script);
		if (file != fileOf.end() && files.contains(file->second) && unwritten.contains(call.native))
		{
			names.insert(table.at(call.native).name);
		}
	}
	return names;
}

/// The source files of every family the progress page lists as running
std::set<std::string> RunningFamilyFiles(const LHVMFile& program)
{
	std::set<std::string> files;
	for (const auto family : k_RunningFamilies)
	{
		const auto own = FamilyFiles(program, family);
		files.insert(own.begin(), own.end());
	}
	return files;
}

/// A family run on its own, and how many scripts it has
struct FamilyRun
{
	std::string_view family;
	size_t scripts {0};
};

/// Each running family and its number of scripts, in the progress page's order
std::vector<FamilyRun> FamilyRuns()
{
	return {
	    {"Standard Reminder", 1},
	    {"Vortex Entry", 2},
	    {"Challenge Notify", 6},
	    {"Did You Know", 2},
	    {"Help System", 9},
	    {"Reward", 6},
	    {"Citadel Help", 17},
	    {"Creature Help", 7},
	    {"Hand Demos", 18},
	    {"Setup Computer Creatures", 4},
	    {"Citadel Guide", 2},
	    {"Protect Gate Keys", 5},
	    {"The Sculptor", 9},
	    {"Creature Saving People", 4},
	    {"Leave Through Vortex L1", 1},
	    {"Singing Stone Circle", 15},
	    {"Take Over Villages L1", 1},
	    {"Throwing Stones", 7},
	    {"Magic Mushroom", 1},
	    {"Setup Land 2", 2},
	    {"Slavers Warning", 1},
	    {"Sacrifice", 2},
	    {"Baywatch", 3},
	    {"Land 2Shield Challenge", 1},
	    {"Land 2Fireball Challenge", 2},
	    {"Land 2Computer AI", 7},
	    {"Begin Land 2", 14},
	    {"Learn Worshipping", 4},
	    {"Greedy Farmer", 10},
	    {"Idol Pyre", 1},
	    {"Leave Through Vortex L2", 1},
	    {"Plague", 1},
	    {"Hanoi Flood", 3},
	    {"Tree Puzzle One", 1},
	    {"Learn Gestures", 2},
	    {"Learn Influence", 1},
	    {"Final Gold Scroll", 4},
	    {"Setup Land 3", 2},
	    {"Leave Through Vortex L3", 1},
	    {"Get Through Vortex L2", 3},
	    {"Tree Puzzle Two", 1},
	    {"Setup Land 4", 1},
	    {"Leave Through Vortex L4", 1},
	    {"Take Over Villages L4", 1},
	    {"Fire On High", 3},
	    {"Volcano", 1},
	    {"Japanese Traitor", 2},
	    {"Setup Land 5", 1},
	};
}

/// A land run from its control script
struct LandRun
{
	int land {0};
};

/// Lands 2 to 5, each run from its control script as the story's control script runs it after the land's load
std::vector<LandRun> LaterLandRuns()
{
	return {
	    {2},
	    {3},
	    {4},
	    {5},
	};
}

VMInstruction Make(Opcode code, VMValue data = VMValue(uint32_t {0}), DataType type = DataType::Int,
                   VMMode mode = VMMode::Immediate)
{
	return {code, mode, type, data, 0};
}

/// Two scripts: Alpha sets the global to 7, starts Beta without waiting for it, then calls ONE every turn, forever;
/// Beta gives TWO a value and ends
LHVMFile TwinProgram()
{
	const std::vector<VMInstruction> code {
	    Make(Opcode::Push, VMValue(7.0f), DataType::Float),                           // 0: Alpha: PUSHF 7
	    Make(Opcode::Pop, VMValue(uint32_t {1}), DataType::Float, VMMode::Reference), // 1: POPF flag
	    Make(Opcode::Run, VMValue(uint32_t {2}), DataType::Int, VMMode::Async),       // 2: RUN async Beta
	    Make(Opcode::Sys, VMValue(uint32_t {1})),                                     // 3: CALL ONE
	    Make(Opcode::Pop),                                                            // 4: its value dropped
	    Make(Opcode::Jmp, VMValue(uint32_t {3})),                                     // 5: JMP 3, which ends the turn
	    Make(Opcode::End),                                                            // 6
	    Make(Opcode::Push, VMValue(int32_t {5})),                                     // 7: Beta: PUSHI 5
	    Make(Opcode::Sys, VMValue(uint32_t {2})),                                     // 8: CALL TWO
	    Make(Opcode::End),                                                            // 9
	};
	const std::vector<VMScript> scripts {
	    VMScript("Alpha", "alpha.txt", ScriptType::Script, 1, {}, 0, 0, 1),
	    VMScript("Beta", "beta.txt", ScriptType::Script, 1, {}, 7, 0, 2),
	};
	return {LHVMVersion::BlackAndWhite, {"flag"}, code, {}, scripts, {}};
}

/// The twin's natives: none, ONE (no arguments, a value back) and TWO (one argument), as a table binds them
std::vector<NativeFunction> TwinTable()
{
	return {NativeFunction(nullptr, 0, 0, "NONE"), NativeFunction(nullptr, 0, 1, "ONE"), NativeFunction(nullptr, 1, 0, "TWO")};
}

/// The twin's source: TWO is unwritten
constexpr std::string_view k_TwinSource = "void None() {} // 000 NONE\r\n"
                                          "\r\n"
                                          "void One() // 001 ONE\r\n"
                                          "{\r\n"
                                          "\tPushf(0.0f);\r\n"
                                          "}\r\n"
                                          "\r\n"
                                          "void Two() // 002 TWO\r\n"
                                          "{\r\n"
                                          "\tNotImplemented();\r\n"
                                          "}\r\n"
                                          "\r\n"
                                          "void InitFunctionsTable0()\r\n"
                                          "{\r\n"
                                          "\tCREATE_FUNCTION_BINDING(\"NONE\", 0, 0, None);\r\n"
                                          "\tCREATE_FUNCTION_BINDING(\"ONE\", 0, 1, One);\r\n"
                                          "\tCREATE_FUNCTION_BINDING(\"TWO\", 1, 0, Two);\r\n"
                                          "}\r\n";

} // namespace

TEST(ChallengeVmWorld, ARunRecordsTheScriptsInTheOrderTheyStartedTheNativesTheyCalledAndTheGlobalsTheySet)
{
	const test::RestoreService<Locator::vm> restoreVm;
	const auto table = TwinTable();
	const auto program = TwinProgram();
	ScriptWorld world(table);
	ASSERT_TRUE(world.Load(program));
	world.Start("Alpha");
	world.Turns(3);

	// Beta, started by Alpha's RUN, ran and ended in Alpha's first turn, and is still counted
	EXPECT_EQ(world.StartOrder(), (std::vector<std::string> {"Alpha", "Beta"}));
	EXPECT_EQ(world.tasks.size(), 2u);
	EXPECT_EQ(world.errors, std::vector<std::string> {});
	ASSERT_EQ(world.calls.size(), 4u);
	EXPECT_EQ(world.calls[0].native, 1u);
	EXPECT_EQ(world.calls[0].script, "Alpha");
	EXPECT_EQ(world.calls[1].native, 2u);
	EXPECT_EQ(world.calls[1].script, "Beta");
	EXPECT_EQ(world.calls[3].script, "Alpha");
	EXPECT_EQ(world.Global("flag").value_or(-1.0f), 7.0f);

	// the run sees the unwritten native Beta reached, and the RUN that started it
	const auto unwritten = UnwrittenNatives(k_TwinSource);
	EXPECT_EQ(unwritten, std::set<uint32_t> {2});
	EXPECT_EQ(UnwrittenCalled(world, program, {"beta.txt"}, unwritten, table), std::set<std::string> {"TWO"});
	EXPECT_TRUE(UnwrittenCalled(world, program, {"alpha.txt"}, unwritten, table).empty());
	EXPECT_EQ(Entries(program, "alpha.txt"), std::vector<uint32_t> {0});
	EXPECT_EQ(ScriptsReached(program, "alpha.txt"), (std::set<std::string> {"Alpha", "Beta"}));
	EXPECT_EQ(ScriptsReached(program, "beta.txt"), std::set<std::string> {"Beta"});
}

TEST(ChallengeVmWorld, ANewGameStartsTheStoryAndLand1sFirstScriptsInTheOriginalsOrder)
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
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_EQ(Bindings(source).size(), table.size());

	const test::RestoreService<Locator::vm> restoreVm;
	ScriptWorld world(table);
	// The program read starts its one auto-start script; then a new game on Land 1 starts the story's control script,
	// which waits a turn and runs Land 1's, as the original
	// (docs/wiki/scripts/land-change-and-saving.md#the-programs-start)
	ASSERT_TRUE(world.Load(program));
	world.Start("LandControlAll");
	world.Turns(k_Turns);

	// the story's first scripts start in the original's order, and the singing stones' checks after them in the
	// original's order too, as the original's own run of a new game shows; with every native answering zero the
	// world then goes its own way, so only these are compared (docs/wiki/scripts/land-change-and-saving.md#the-programs-start)
	const auto order = world.StartOrder();
	const std::vector<std::string> first {
	    "CreatureDevelopmentAutoRun", "LandControlAll",    "LandControl1", "SetupLand1", "PlaceInitialLand1DYKS", "DidYouKnow",
	    "SetAztecTownBeliefToZero",   "SingingStoneCircle"};
	ASSERT_GE(order.size(), first.size());
	EXPECT_EQ(std::vector<std::string>(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(first.size())), first);
	const std::vector<std::string> later {"KeepStonesAlive", "CheckStonePositionsAndPlayNotes", "CheckSingingCircleClicked",
	                                      "CheckFormed", "CheckFilled"};
	auto at = order.begin() + static_cast<std::ptrdiff_t>(first.size());
	for (const auto& name : later)
	{
		at = std::find(at, order.end(), name);
		ASSERT_NE(at, order.end()) << name << " does not start after the scripts before it";
	}
	// eleven did-you-knows placed by Land 1's set-up
	EXPECT_EQ(std::ranges::count(world.tasks, std::string("DidYouKnow"), [](const auto& task) { return task.second; }), 11);
	EXPECT_EQ(world.errors, std::vector<std::string> {});
	// Land 1's set-up writes the land's number, and leaves the skip unset when the tutorial is not skipped, as the
	// original (docs/wiki/scripts/land-change-and-saving.md#the-programs-start)
	EXPECT_EQ(world.Global("LandNumber").value_or(-1.0f), 1.0f);
	EXPECT_EQ(world.Global("IsSkippingToCreatureSelect").value_or(-1.0f), 0.0f);
	EXPECT_EQ(UnwrittenCalled(world, program, RunningFamilyFiles(program), UnwrittenNatives(source), table),
	          std::set<std::string> {});
}

TEST(ChallengeVmWorld, EachLaterLandsControlScriptRunsWithNoErrorAndWritesTheLandsNumber)
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
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_EQ(Bindings(source).size(), table.size());
	const auto unwritten = UnwrittenNatives(source);
	const auto families = RunningFamilyFiles(program);

	const test::RestoreService<Locator::vm> restoreVm;
	for (const auto& land : LaterLandRuns())
	{
		ScriptWorld world(table);
		ASSERT_TRUE(world.Load(program));
		// After a land, the story's control script stops every script but its own, loads the next land and runs that
		// land's control script, waiting on it; each land's set-up writes the land's number, as the original
		// (docs/wiki/scripts/land-change-and-saving.md#the-programs-start)
		world.StopAll();
		world.Start("LandControl" + std::to_string(land.land));
		world.Turns(k_Turns);
		EXPECT_EQ(world.errors, std::vector<std::string> {}) << "Land " << land.land;
		EXPECT_EQ(world.Global("LandNumber").value_or(-1.0f), static_cast<float>(land.land)) << "Land " << land.land;
		EXPECT_EQ(UnwrittenCalled(world, program, families, unwritten, table), std::set<std::string> {})
		    << "Land " << land.land;
	}
}

TEST(ChallengeVmWorld, EveryRunningFamilyRunsReachingOnlyWrittenNativesWithNoError)
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
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	ASSERT_EQ(Bindings(source).size(), table.size());
	const auto unwritten = UnwrittenNatives(source);
	const auto& scripts = program.GetScripts();
	const auto runs = FamilyRuns();
	ASSERT_EQ(runs.size(), k_RunningFamilies.size());

	const test::RestoreService<Locator::vm> restoreVm;
	for (size_t i = 0; i < runs.size(); ++i)
	{
		const auto& run = runs[i];
		ASSERT_EQ(run.family, k_RunningFamilies[i]);
		const auto files = FamilyFiles(program, run.family);
		ASSERT_EQ(files.size(), 1u) << run.family << " is not the name of exactly one source file";
		const auto& file = *files.begin();
		EXPECT_EQ(static_cast<size_t>(std::ranges::count(scripts, file, &VMScript::filename)), run.scripts) << run.family;

		ScriptWorld world(table);
		ASSERT_TRUE(world.Load(program));
		world.StopAll();
		// the family's scripts that none of its own starts, each as a RUN with zero arguments starts it
		for (const auto entry : Entries(program, file))
		{
			world.Start(scripts[entry]);
		}
		world.Turns(k_Turns);

		const auto reached = ScriptsReached(program, file);
		for (const auto& [number, script] : world.tasks)
		{
			EXPECT_TRUE(reached.contains(script)) << run.family << " started " << script;
		}
		EXPECT_FALSE(world.calls.empty()) << run.family;
		EXPECT_EQ(world.errors, std::vector<std::string> {}) << run.family;
		std::set<std::string> called;
		for (const auto& call : world.calls)
		{
			if (unwritten.contains(call.native))
			{
				called.insert(table.at(call.native).name);
			}
		}
		EXPECT_EQ(called, std::set<std::string> {}) << run.family << " reached unwritten natives";
	}
}
