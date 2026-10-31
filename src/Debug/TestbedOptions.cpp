/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedOptions.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <map>
#include <set>

using namespace openblack;

namespace
{
/// The whole text as a number, none for anything else
template <typename T>
std::optional<T> ParseNumber(std::string_view text)
{
	T value {};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (text.empty() || error != std::errc {} || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return value;
}

/// Not empty, and only letters, digits and the given punctuation
bool IsPlainName(std::string_view text, std::string_view punctuation)
{
	return !text.empty() && std::ranges::all_of(text, [punctuation](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
		       punctuation.find(c) != std::string_view::npos;
	});
}

bool ApplyHandDemo(std::string_view value, LandRun& run)
{
	if (!IsPlainName(value, "._-"))
	{
		return false;
	}
	run.handDemo = std::string(value);
	return true;
}

bool ApplySkipAnswer(std::string_view value, LandRun& run)
{
	const auto slash = value.find('/');
	if (slash == std::string_view::npos)
	{
		return false;
	}
	const auto answer = ParseNumber<int>(value.substr(0, slash));
	const auto frame = ParseNumber<uint32_t>(value.substr(slash + 1));
	// the SkipBox's four answers (Game::HandleInterfaceAction)
	if (!answer || !frame || *answer < 0 || *answer > 3)
	{
		return false;
	}
	run.skipAnswer = SkipAnswer {.answer = *answer, .frame = *frame};
	return true;
}

bool ApplyMenuAt(std::string_view value, LandRun& run)
{
	const auto frame = ParseNumber<uint32_t>(value);
	if (!frame)
	{
		return false;
	}
	run.menuAt = *frame;
	return true;
}

bool ApplyCitadel(std::string_view value, LandRun& run)
{
	const auto slash = value.find('/');
	const auto in = ParseNumber<uint32_t>(value.substr(0, slash));
	// no way out without one: 0, as the variable's "<in>" alone
	const auto out =
	    slash == std::string_view::npos ? std::optional<uint32_t> {0} : ParseNumber<uint32_t>(value.substr(slash + 1));
	// the hook counts its turns from 1, and a way out given comes after the way in
	if (!in || !out || *in == 0 || (slash != std::string_view::npos && *out <= *in))
	{
		return false;
	}
	run.citadel = ecs::audio_queries::CitadelTestTurns {.in = *in, .out = *out};
	return true;
}

/// Names separated by slashes, each a plain name with the characters given besides; none when one is not
std::optional<std::vector<std::string>> SlashedNames(std::string_view value, std::string_view extra)
{
	std::vector<std::string> names;
	for (auto rest = value;;)
	{
		const auto slash = rest.find('/');
		const auto name = rest.substr(0, slash);
		if (!IsPlainName(name, extra))
		{
			return std::nullopt;
		}
		names.emplace_back(name);
		if (slash == std::string_view::npos)
		{
			break;
		}
		rest = rest.substr(slash + 1);
	}
	return names;
}

bool ApplyTimeline(std::string_view value, LandRun& run)
{
	// scenarios' ids, as the list's own entries give them
	auto ids = SlashedNames(value, "._-");
	if (!ids.has_value())
	{
		return false;
	}
	run.timeline = std::move(*ids);
	return true;
}

bool ApplyQuest(std::string_view value, LandRun& run)
{
	// the scripts by their names, as the challenge file gives them, each with its arguments if it takes any
	std::vector<testbed_land_timeline::QuestScript> scripts;
	for (auto rest = value;;)
	{
		const auto slash = rest.find('/');
		auto script = testbed_land_timeline::ParseQuestScript(rest.substr(0, slash));
		if (!script.has_value())
		{
			return false;
		}
		scripts.push_back(std::move(*script));
		if (slash == std::string_view::npos)
		{
			break;
		}
		rest = rest.substr(slash + 1);
	}
	run.quest = std::move(scripts);
	return true;
}

/// The keys a land entry knows: a new key is a row here (LandOptions in the header says what else it needs)
constexpr std::array k_LandOptions = {
    testbed_scenarios::LandOption {.key = "demo", .value = "<name>", .apply = ApplyHandDemo},
    testbed_scenarios::LandOption {.key = "skip", .value = "<answer>/<frame>", .apply = ApplySkipAnswer},
    testbed_scenarios::LandOption {.key = "menu", .value = "<frame>", .apply = ApplyMenuAt},
    testbed_scenarios::LandOption {.key = "citadel", .value = "<in>[/<out>]", .apply = ApplyCitadel},
    testbed_scenarios::LandOption {.key = "timeline", .value = "<id>[/<id>...]", .apply = ApplyTimeline},
    testbed_scenarios::LandOption {.key = "quest", .value = "<script>[(<arg>,...)][/<script>...]", .apply = ApplyQuest},
};

std::string KnownLandKeys()
{
	std::string known;
	for (const auto& option : k_LandOptions)
	{
		known += (known.empty() ? "" : ", ") + std::string(option.key) + "=" + std::string(option.value);
	}
	return known;
}
} // namespace

std::span<const testbed_scenarios::LandOption> testbed_scenarios::LandOptions()
{
	return k_LandOptions;
}

std::optional<testbed_scenarios::LandEntry> testbed_scenarios::ParseLandEntry(std::string_view item, std::string& error)
{
	const auto fail = [&error, item](const std::string& reason) -> std::optional<LandEntry> {
		error = "land entry \"" + std::string(item) + "\": " + reason;
		return std::nullopt;
	};
	if (!item.starts_with(k_LandEntryPrefix))
	{
		return fail("it does not start with " + std::string(k_LandEntryPrefix));
	}
	// the land, the frames, then the options, each after a ':'
	std::vector<std::string_view> fields;
	for (auto rest = item.substr(k_LandEntryPrefix.size());;)
	{
		const auto colon = rest.find(':');
		fields.push_back(rest.substr(0, colon));
		if (colon == std::string_view::npos)
		{
			break;
		}
		rest = rest.substr(colon + 1);
	}
	if (fields.size() < 2)
	{
		return fail("it needs land:<file>:<frames>");
	}
	LandEntry entry;
	if (!IsPlainName(fields[0], "._-/") || fields[0].front() == '/')
	{
		return fail("bad land file \"" + std::string(fields[0]) + "\"");
	}
	entry.run.land = std::string(fields[0]);
	if (const auto frames = ParseNumber<uint32_t>(fields[1]); frames && *frames > 0)
	{
		entry.frames = *frames;
	}
	else
	{
		return fail("bad frames \"" + std::string(fields[1]) + "\" (a number above 0)");
	}
	std::set<std::string_view> given;
	for (size_t i = 2; i < fields.size(); ++i)
	{
		const auto equals = fields[i].find('=');
		const auto key = fields[i].substr(0, equals);
		const auto option = std::ranges::find(k_LandOptions, key, &LandOption::key);
		if (equals == std::string_view::npos || option == k_LandOptions.end())
		{
			return fail("unknown option \"" + std::string(fields[i]) + "\" (known: " + KnownLandKeys() + ")");
		}
		if (!given.insert(key).second)
		{
			return fail(std::string(key) + "= given twice");
		}
		if (!option->apply(fields[i].substr(equals + 1), entry.run))
		{
			return fail("bad value in \"" + std::string(fields[i]) + "\" (" + std::string(option->key) + "=" +
			            std::string(option->value) + ")");
		}
	}
	return entry;
}

std::string testbed_scenarios::LandRunName(const LandRun& run)
{
	if (run.handDemo.has_value())
	{
		return *run.handDemo;
	}
	return !run.timeline.empty() ? run.timeline.front() : std::filesystem::path(run.land).stem().string();
}

std::optional<ScenarioRequest> testbed_scenarios::MakeScenarioRequest(const std::optional<std::string>& scenario,
                                                                      uint32_t warmUpFrames, uint32_t frames,
                                                                      const std::optional<std::string>& resultsBase)
{
	if (!scenario.has_value())
	{
		return std::nullopt;
	}
	ScenarioRequest request {
	    .id = *scenario,
	    .warmUpFrames = warmUpFrames,
	    .frames = std::max<uint32_t>(frames, 1),
	};
	if (resultsBase.has_value())
	{
		request.results = std::filesystem::path(*resultsBase);
	}
	return request;
}

std::optional<std::vector<ScenarioListEntry>> testbed_scenarios::MakeScenarioList(std::string_view list, uint32_t defaultFrames,
                                                                                  const std::string& outPrefix,
                                                                                  uint32_t warmUpFrames, uint32_t frames,
                                                                                  const std::optional<std::string>& resultsBase,
                                                                                  std::string* error)
{
	const auto fail = [error](std::string reason) -> std::optional<std::vector<ScenarioListEntry>> {
		if (error != nullptr)
		{
			*error = std::move(reason);
		}
		return std::nullopt;
	};
	std::vector<ScenarioListEntry> entries;
	// how many times each id came, for the names of its outputs
	std::map<std::string, uint32_t, std::less<>> seen;
	while (!list.empty())
	{
		const auto comma = list.find(',');
		const auto item = list.substr(0, comma);
		list = comma == std::string_view::npos ? std::string_view {} : list.substr(comma + 1);
		std::optional<ScenarioRequest> request;
		std::string id;
		uint32_t runFrames = defaultFrames;
		if (item.starts_with(k_LandEntryPrefix))
		{
			// a land run: no scenario, its outputs named after its demo or its land
			std::string reason;
			auto land = ParseLandEntry(item, reason);
			if (!land)
			{
				return fail(reason);
			}
			runFrames = land->frames;
			id = LandRunName(land->run);
			request = ScenarioRequest {.land = std::move(land->run)};
		}
		else
		{
			const auto colon = item.find(':');
			id = std::string(item.substr(0, colon));
			if (id.empty())
			{
				return fail("an entry without an id");
			}
			if (colon != std::string_view::npos)
			{
				const auto given = ParseNumber<uint32_t>(item.substr(colon + 1));
				if (!given)
				{
					return fail("bad frames in \"" + std::string(item) + "\"");
				}
				runFrames = *given;
			}
			if (runFrames == 0)
			{
				return fail("no frames for \"" + std::string(item) + "\" (a number above 0)");
			}
			request = MakeScenarioRequest(id, warmUpFrames, frames, resultsBase);
		}
		const auto count = ++seen[id];
		const auto name = count == 1 ? id : id + "." + std::to_string(count);
		ScenarioListEntry entry {
		    .request = std::move(*request),
		    .frames = runFrames,
		    .out = std::filesystem::path(outPrefix + name),
		    .stateHash = std::filesystem::path(outPrefix + name + "_hash.txt"),
		};
		if (runFrames > k_ListShotBeforeEnd)
		{
			entry.screenshot =
			    std::make_pair(runFrames - k_ListShotBeforeEnd, std::filesystem::path(outPrefix + name + ".png"));
		}
		entries.push_back(std::move(entry));
		// a list ending in a comma has an empty last id
		if (comma != std::string_view::npos && list.empty())
		{
			return fail("an entry without an id");
		}
	}
	if (entries.empty())
	{
		return fail("an empty list");
	}
	return entries;
}
