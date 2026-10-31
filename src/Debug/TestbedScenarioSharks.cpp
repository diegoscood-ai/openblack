/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a shark in the lake: one swimming and walking no track, its part above the water never
// drawn, and one walking a track of the game's cameras, its part above the water drawn only while it walks

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// Where the shark lies, in the lake's open water by its north western corner: where its walk ends
constexpr glm::vec2 k_InTheLake {-49.4f, 263.2f};

/// The track of the game's camera file whose focus crosses the testbed's lake: a fly-over of the first land, its focus
/// running over the shallows north east of the lake's north western corner and into the water at the corner. The walk
/// takes that stretch of it, from 39% of the track's time to 40.8%: four turns, from the shallows about 30 m north east
/// of where the shark lies back to it
constexpr size_t k_Track = 0;
constexpr float k_WalkFrom = 0.39f;
constexpr float k_WalkTo = 0.408f;
/// A walk every so often, so that one of them is under way when the checks' shot is taken
constexpr float k_WalkEvery = 2.3f;

/// Low over the water from the south east, the shark and the shallows north east of it in view
Framing AcrossTheWater()
{
	return {.shot = Shot::Placed, .eye = {-15.0f, 10.0f, 238.0f}, .look = {-42.0f, 0.0f, 270.0f}};
}

/// Noon, the clock standing still, and no dispensers in the way
Environment StillNoon()
{
	return {.clockRuns = false, .dispenserGrid = false};
}

std::vector<ObjectSetup> OneShark()
{
	return {{.type = MobileObjectInfo::Whale, .offset = k_InTheLake}};
}
} // namespace

void testbed_scenarios::AddSharkScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "sharks.swimming",
	    .name = "Sharks: one swimming, walking no track",
	    .facet = Facet::Animals,
	    .description = "A shark made in the lake's open water by its north western corner, as a script's CREATE of a "
	                   "whale makes it, at noon, seen low across the water from the south east. Nothing tells it to walk "
	                   "a track.",
	    .expected = "Only its part under the water is drawn, dark blue through the water, swimming with its clip, white "
	                "rings spreading off its back. Nothing of it shows above the water line, neither fin nor tail. It "
	                "stays where it was made: a shark has no mind of its own.",
	    .environment = StillNoon(),
	    .framing = AcrossTheWater(),
	    .objects = OneShark(),
	});

	all.push_back({
	    .id = "sharks.walks_track",
	    .name = "Sharks: one walking a track into the lake",
	    .facet = Facet::Animals,
	    .description = "The same shark in the lake. Every 2.3 seconds a script's WALK_PATH sends it along the game's "
	                   "camera track 0, from 39% to 40.8% of the track's time: that stretch runs from the shallows about "
	                   "30 m north east of it back to where it lies.",
	    .expected = "Between its walks only its part under the water is drawn, dark blue, nothing of it above the water. "
	                "As each walk starts it is at once on the shallows and its part above the water is drawn as well, its "
	                "fin and tail showing, while it races back into the lake in four turns, at the track's own speed of "
	                "about 100 m a second. Once there its walk ends, and its part above the water is hidden again. The "
	                "checks' shot falls during its sixth walk, with its part above the water drawn.",
	    .environment = StillNoon(),
	    .framing = AcrossTheWater(),
	    .objects = OneShark(),
	    .commands = {{.kind = Kind::ScriptWalkPath,
	                  .delaySeconds = k_WalkEvery,
	                  .value = k_Track,
	                  .object = 0,
	                  .pathFrom = k_WalkFrom,
	                  .pathTo = k_WalkTo}},
	    .repeatFrom = 0,
	});
}
