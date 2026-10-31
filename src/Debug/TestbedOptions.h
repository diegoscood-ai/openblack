/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Debug/TestbedQuestScript.h"
#include "ECS/AudioQueries.h"

namespace openblack
{

/// The SkipBox's answer a land run gives, and the frame of the run it is given at, counted from the run's first frame
struct SkipAnswer
{
	int answer {0};
	uint32_t frame {0};
};

/// A land run of a --scenarios list: a new game on a land, loaded as a launch loads its start level, with the test
/// options its entry gives in place of their environment variables
struct LandRun
{
	/// The land's script, under the game's Scripts folder, as -s names it
	std::string land;
	/// demo=: the hand demo the test hooks play once the land exists, in place of OPENBLACK_TEST_HAND_DEMO's
	std::optional<std::string> handDemo;
	/// skip=: the SkipBox's answer and its frame, in place of OPENBLACK_TEST_SKIP_ANSWER's
	std::optional<SkipAnswer> skipAnswer;
	/// menu=: the frame of the run the escape menu opens at, in place of OPENBLACK_TEST_MENU_AT's
	std::optional<uint32_t> menuAt;
	/// citadel=: the hook turns the temple is entered and left at, in place of OPENBLACK_AUDIO_TEST_CITADEL's
	std::optional<ecs::audio_queries::CitadelTestTurns> citadel;
	/// timeline=: the testbed scenarios whose commands are played on the land once it is loaded, one after the other,
	/// by their ids; none plays without it
	std::vector<std::string> timeline;
	/// quest=: a quest run. The land's story script is not started; these scripts are, by name, one after the other,
	/// each once the one before has finished and with the arguments given it, the last left running while the
	/// timeline plays. None without it
	std::vector<testbed_land_timeline::QuestScript> quest;
};

/// A testbed scenario asked for on the command line: its id, and for a benchmark the frames to let settle, the frames to
/// measure and where to write the results; a benchmark's game quits once its frames are measured
struct ScenarioRequest
{
	std::string id;
	uint32_t warmUpFrames {120};
	uint32_t frames {600};
	/// --benchmark-out: the results are written only when it is given
	std::optional<std::filesystem::path> results;
	/// A list's land run instead of a scenario (no id): its land loads in place of the testbed, and the scenarios'
	/// window never gets the request
	std::optional<LandRun> land;
};

/// One scenario of a list run in one launch (--scenarios), each a new game: its request, the frames it runs for, the
/// file of its state hash and its screenshot, and the name of its outputs (the log marks its start and end by it)
struct ScenarioListEntry
{
	ScenarioRequest request;
	uint32_t frames {0};
	/// The outputs without their endings: <out>_hash.txt and <out>.png
	std::filesystem::path out;
	std::filesystem::path stateHash;
	std::optional<std::pair</* frame number */ uint32_t, /* output */ std::filesystem::path>> screenshot;
};

namespace testbed_scenarios
{

/// How many frames before a list entry's end its screenshot is taken
inline constexpr uint32_t k_ListShotBeforeEnd = 20;

/// What starts a land entry in a --scenarios list
inline constexpr std::string_view k_LandEntryPrefix = "land:";

/// One option key of a land entry: the key, the form of its value (for the error and the help) and what it sets in the
/// run, false when the value is malformed
struct LandOption
{
	std::string_view key;
	std::string_view value;
	bool (*apply)(std::string_view value, LandRun& run);
};

/// The keys a land entry knows, in the order an error lists them (the table is in TestbedOptions.cpp):
///   demo=<name>              the hand demo Data/HandDemo/<name>.hnd, played as OPENBLACK_TEST_HAND_DEMO plays it
///   skip=<answer>/<frame>    the SkipBox answered (0 to 3) at the run's frame, as OPENBLACK_TEST_SKIP_ANSWER does
///   menu=<frame>             the escape menu opened at the run's frame, as OPENBLACK_TEST_MENU_AT does
///   citadel=<in>[/<out>]     the temple entered at hook turn <in> (above 0) and left at <out> (after <in>), as
///                            OPENBLACK_AUDIO_TEST_CITADEL="<in>[,<out>]" does
///   timeline=<id>[/<id>...]  the testbed scenarios of those ids, their commands played on the loaded land in turn, the
///                            next once the last is done (Runner::StartOnLand)
///   quest=<script>[(<arg>,...)][/<script>...]  a quest run: the land's story script is not started, these scripts
///                            are, in turn, each once the one before has finished, each with its arguments: numbers,
///                            marker@<x>;<y>;<z>, thing@<type>;<x>;<z>[;<radius>] (QuestChapter, before the timeline)
/// A new key is a row of the table, its field in LandRun, and the place in the game that reads that field
[[nodiscard]] std::span<const LandOption> LandOptions();

/// A land entry with its frames
struct LandEntry
{
	LandRun run;
	uint32_t frames {0};
};

/// One land entry of a --scenarios list: "land:<file>:<frames>" followed by any number of ":<key>=<value>" options
/// (LandOptions), e.g. "land:Land1.txt:900:demo=Gestures:skip=3/30". <file> is the land's script under the game's
/// Scripts folder (letters, digits, '.', '_', '-' and '/' between folders); <frames> is a number above 0. A value with
/// several numbers separates them with '/', since ',' separates the list's entries and ':' an entry's options. None,
/// with the reason in `error`, for anything else: an unknown key (the error lists the known ones), a key given twice,
/// a malformed value
[[nodiscard]] std::optional<LandEntry> ParseLandEntry(std::string_view item, std::string& error);

/// The name a land run's outputs take: its hand demo's, else its timeline's first scenario's id, else its land script's
/// stem
[[nodiscard]] std::string LandRunName(const LandRun& run);

/// The command line's --scenarios "id[:frames],..." with --scenarios-out <prefix>: one entry per id, in the list's
/// order, running `frames` frames (`defaultFrames` without them), with its state hash in <prefix><id>_hash.txt and its
/// shot k_ListShotBeforeEnd frames before its end in <prefix><id>.png (none when it runs that many frames or fewer). An
/// id given again names its outputs <prefix><id>.2, .3 and so on. The benchmark's settings go with every entry, as with
/// --scenario. An item starting with k_LandEntryPrefix is a land entry instead (ParseLandEntry): a request with its
/// land run and no id, its outputs named by LandRunName. None for an empty list, an empty id, frames that are not a
/// number above 0 or a malformed land entry, with the reason in `error` when it is given
[[nodiscard]] std::optional<std::vector<ScenarioListEntry>>
MakeScenarioList(std::string_view list, uint32_t defaultFrames, const std::string& outPrefix, uint32_t warmUpFrames,
                 uint32_t frames, const std::optional<std::string>& resultsBase, std::string* error = nullptr);

/// The command line's --scenario, --benchmark-warmup, --benchmark-frames and --benchmark-out as a request: none without
/// a scenario, and at least one frame measured
[[nodiscard]] std::optional<ScenarioRequest> MakeScenarioRequest(const std::optional<std::string>& scenario,
                                                                 uint32_t warmUpFrames, uint32_t frames,
                                                                 const std::optional<std::string>& resultsBase);

} // namespace testbed_scenarios
} // namespace openblack
