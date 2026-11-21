/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Creature/CreatureDesires.h"
#include "TestbedOptions.h"
#include "TestbedScenarioRegistry.h"
#include "TestbedScenarioRunner.h"

// What the Testbed Scenarios window works out before it draws or acts: the facets and scenarios it lists, what stays
// picked, its status line, the scenario seconds of a frame and the run the command line asked for. Pure functions over
// the list of scenarios they are given, tested with hand-made lists.

namespace openblack::testbed_scenarios
{

/// The facets that have at least one scenario, in their order; the others are left out of the list
[[nodiscard]] std::vector<Facet> ListedFacets(std::span<const Scenario> all);
/// The scenarios the list shows, of the facet or all of them, by their place in the whole list
[[nodiscard]] std::vector<size_t> Listed(std::span<const Scenario> all, std::optional<Facet> facet);
/// What is picked once the list is narrowed to a facet: the scenario picked if the facet has it, else the facet's
/// first, and the one picked as it was when the facet has none
[[nodiscard]] size_t PickAfterFacetChange(std::span<const Scenario> all, std::optional<Facet> facet, size_t picked);
/// The scenario picked, kept within the list; none when the list is empty
[[nodiscard]] std::optional<size_t> PickedIn(std::span<const Scenario> all, size_t picked);

/// The scenario seconds a frame moves on by: none while the game is paused, and more as it runs faster (a game speed of
/// 0.5 is twice as fast)
[[nodiscard]] float ScenarioSeconds(float frameSeconds, bool paused, float gameSpeed);

/// The line under the controls: running or stopped, the scenario, its seconds in and how far its commands have got
[[nodiscard]] std::string StatusLine(bool running, const Scenario& scenario, float seconds, const Timeline& timeline);

/// The strongest of a creature's activated desires, none when none is above 0
[[nodiscard]] std::optional<creature_desires::Desire> StrongestDesire(const creature_desires::Desires& desires);

/// The run the command line asked for: the scenario of its id, and how its crowd is measured
struct RequestedRun
{
	size_t index {0};
	BenchmarkSettings settings;
};
/// The request's scenario and benchmark settings, none when there is no scenario of its id, which quits the game. A
/// benchmark quits the game once its frames are measured; without a crowd there is nothing to measure, and the game
/// carries on with the scenario. The results are written only to the path the request gives.
[[nodiscard]] std::optional<RequestedRun> ResolveRequest(std::span<const Scenario> all, const ScenarioRequest& request);
/// The ids there are, for the line logged when the one asked for is not among them
[[nodiscard]] std::string KnownIds(std::span<const Scenario> all);
/// A scenario run from the window, after the command line's: measured as that asked, but the game carries on
[[nodiscard]] BenchmarkSettings ForRunFromWindow(BenchmarkSettings settings);

} // namespace openblack::testbed_scenarios
