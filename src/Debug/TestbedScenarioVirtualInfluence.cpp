/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenario of what the player's hand keeps of their influence once it reaches past the border

#include <cstddef>
#include <cstdint>

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The land's number: on the third land the player's temple reaches 250 m
constexpr int32_t k_LandNumber = 3;
/// The hand walks north over the land from inside the temple's reach to past it, a step at a time
constexpr float k_WalkFrom = 230.0f;
constexpr float k_WalkStep = 2.0f;
constexpr size_t k_WalkSteps = 20;
constexpr float k_WalkStepSeconds = 0.3f;
/// The pointer rests by the temple first, then this long before the walk
constexpr float k_RestSeconds = 7.5f;
} // namespace

void testbed_scenarios::AddVirtualInfluenceScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	// The pointer rests near the temple, inside the player's influence; then it walks the hand north on the land, two
	// metres at a time, from twenty metres inside the border to eighteen past it, so the last place it was inside is
	// right behind it as it goes out
	std::vector<Command> commands {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.68f}}};
	for (size_t step = 0; step < k_WalkSteps; ++step)
	{
		commands.push_back({.kind = Kind::PointerTo,
		                    .delaySeconds = step == 0 ? k_RestSeconds : k_WalkStepSeconds,
		                    .point = {0.0f, k_WalkFrom + k_WalkStep * static_cast<float>(step)},
		                    .value = 2});
	}
	// The player's temple stands in the middle, built whole, so its border shows, on the third land, where it reaches
	// 250 m. The camera stands high south of it looking north past it. No land script runs, so what the hand keeps past
	// the border is on, at full strength, as a game starts it
	all.push_back({
	    .id = "hand.virtual_influence",
	    .name = "The hand reaching past the player's border",
	    .facet = Facet::Hand,
	    .description = "The player's temple, built, in the middle of the land, on the third land (its reach 250 m), with "
	                   "the camera high to its south looking north past it. The pointer rests near the temple for eight "
	                   "seconds, inside the player's influence, then walks the hand north over the land, two metres "
	                   "every 0.3 s, from 230 m to 268 m from the temple: out past the border's curtain about three "
	                   "seconds before the shot.",
	    .expected = "Near the temple the hand is in the player's influence. Past the border it still counts as in it for a "
	                "while: the land within ten metres of the hand, shrinking with the strength, is the player's. While "
	                "it does, a hum plays, lower as the strength drops, and sparks in the player's colour run from the "
	                "hand back towards where it was last inside, weaving over the land. Each turn out the strength "
	                "drops, the more the further the hand is from that place and the longer it has been out; once under "
	                "a tenth it is gone at once, and the hum and the sparks stop. The game log, at debug level, gives "
	                "the strength each time it passes a tenth; OPENBLACK_VIRTUAL_INFLUENCE_TRACE=1 gives what the "
	                "hand's draw finds once a second while it is out.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.68f}, .landNumber = k_LandNumber},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 120.0f, -160.0f}, .look = {0.0f, 0.0f, 120.0f}},
	    .commands = std::move(commands),
	    .fixtures = {.temples = {{.at = glm::vec2 {0.0f, 0.0f}}}},
	});
}
