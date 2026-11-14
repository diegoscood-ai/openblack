/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenariosModel.h"

#include <algorithm>
#include <iterator>
#include <string_view>

#include <fmt/format.h>

using namespace openblack;
using namespace openblack::testbed_scenarios;

std::vector<Facet> testbed_scenarios::ListedFacets(std::span<const Scenario> all)
{
	std::vector<Facet> facets;
	for (size_t i = 0; i < k_FacetCount; ++i)
	{
		const auto facet = static_cast<Facet>(i);
		if (std::ranges::any_of(all, [facet](const Scenario& scenario) { return scenario.facet == facet; }))
		{
			facets.push_back(facet);
		}
	}
	return facets;
}

std::vector<size_t> testbed_scenarios::Listed(std::span<const Scenario> all, std::optional<Facet> facet)
{
	std::vector<size_t> listed;
	for (size_t i = 0; i < all.size(); ++i)
	{
		if (!facet.has_value() || all[i].facet == *facet)
		{
			listed.push_back(i);
		}
	}
	return listed;
}

size_t testbed_scenarios::PickAfterFacetChange(std::span<const Scenario> all, std::optional<Facet> facet, size_t picked)
{
	const auto listed = Listed(all, facet);
	if (!listed.empty() && std::ranges::find(listed, picked) == listed.end())
	{
		return listed.front();
	}
	return picked;
}

std::optional<size_t> testbed_scenarios::PickedIn(std::span<const Scenario> all, size_t picked)
{
	if (all.empty())
	{
		return std::nullopt;
	}
	return std::min(picked, all.size() - 1);
}

float testbed_scenarios::ScenarioSeconds(float frameSeconds, bool paused, float gameSpeed)
{
	return paused ? 0.0f : frameSeconds / std::max(gameSpeed, 0.01f);
}

std::string testbed_scenarios::StatusLine(bool running, const Scenario& scenario, float seconds, const Timeline& timeline)
{
	const auto commands = scenario.commands.empty() ? std::string("no commands")
	                      : timeline.done           ? std::string("every command given")
	                                      : fmt::format("next command {} of {}", timeline.next + 1, scenario.commands.size());
	return fmt::format("{} {}, {:.1f} s in, {}", running ? "Running" : "Stopped", scenario.name, seconds, commands);
}

std::optional<creature_desires::Desire> testbed_scenarios::StrongestDesire(const creature_desires::Desires& desires)
{
	std::optional<creature_desires::Desire> strongest;
	float value = 0.0f;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto& state = desires.desires.at(i);
		if (state.activated && state.value > value)
		{
			value = state.value;
			strongest = static_cast<creature_desires::Desire>(i);
		}
	}
	return strongest;
}

std::optional<RequestedRun> testbed_scenarios::ResolveRequest(std::span<const Scenario> all, const ScenarioRequest& request)
{
	const auto found = std::ranges::find_if(all, [&request](const Scenario& scenario) { return scenario.id == request.id; });
	if (found == all.end())
	{
		return std::nullopt;
	}
	const bool crowd = found->crowd.has_value();
	return RequestedRun {
	    .index = static_cast<size_t>(std::distance(all.begin(), found)),
	    .settings =
	        {
	            .warmUpFrames = request.warmUpFrames,
	            .frames = request.frames,
	            .quitWhenMeasured = crowd,
	            .resultsBase = crowd ? request.results : std::nullopt,
	        },
	};
}

std::string testbed_scenarios::KnownIds(std::span<const Scenario> all)
{
	std::string ids;
	for (const auto& scenario : all)
	{
		ids += fmt::format(" {}", scenario.id);
	}
	return ids;
}

BenchmarkSettings testbed_scenarios::ForRunFromWindow(BenchmarkSettings settings)
{
	settings.quitWhenMeasured = false;
	return settings;
}
