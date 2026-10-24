/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of how the hand looks for its player's alignment: evil, neutral and good, and turning from
// one to the other; and of the light it carries over the land as the evening darkens

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The hand hovers over the land in the middle of the window
constexpr glm::vec2 k_Cursor {0.5f, 0.5f};

Environment Looking(float alignment)
{
	return {.dispenserGrid = false, .playerAlignment = alignment, .cursor = k_Cursor};
}

/// Looking down at the middle of the map from close by, where the hand hovers
Framing Close()
{
	return {.shot = Shot::Overview, .include = {{0.0f, 0.0f}}, .distance = 0.2f};
}

Command Align(float alignment, float delay)
{
	return {.kind = Kind::SetAlignment, .delaySeconds = delay, .alignment = alignment};
}

Command AtHour(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}
} // namespace

void testbed_scenarios::AddHandLookScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.look_evil",
	    .name = "The hand of an evil player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to -1, fully evil, with the hand over the land in the middle of the "
	                   "window.",
	    .expected = "The hand is dark red and veined, its fingers and knuckles ridged and spiked: wholly the evil hand's shape "
	                "and skin.",
	    .environment = Looking(-1.0f),
	    .framing = Close(),
	});

	all.push_back({
	    .id = "hand.look_neutral",
	    .name = "The hand of a neutral player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to 0, with the hand over the land in the middle of the window.",
	    .expected = "The hand is the plain orange-brown hand with smooth fingers: the base hand untouched.",
	    .environment = Looking(0.0f),
	    .framing = Close(),
	});

	all.push_back({
	    .id = "hand.look_good",
	    .name = "The hand of a good player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to 1, fully good, with the hand over the land in the middle of the "
	                   "window.",
	    .expected = "The hand is pale gold, the same smooth shape as the neutral hand: the good hand only changes its skin.",
	    .environment = Looking(1.0f),
	    .framing = Close(),
	});

	// From neutral, jumps to either end and back, then a slow sweep from evil to good and back again
	std::vector<Command> turning {Align(-1.0f, 3.0f), Align(0.0f, 3.0f),  Align(1.0f, 3.0f),
	                              Align(0.0f, 3.0f),  Align(-0.5f, 3.0f), Align(0.5f, 3.0f)};
	constexpr int k_SweepSteps = 200;
	constexpr float k_SweepDelay = 0.05f;
	for (int i = 0; i <= k_SweepSteps; ++i)
	{
		turning.push_back(Align(-1.0f + (2.0f * static_cast<float>(i) / k_SweepSteps), i == 0 ? 3.0f : k_SweepDelay));
	}
	for (int i = k_SweepSteps - 1; i >= 0; --i)
	{
		turning.push_back(Align(-1.0f + (2.0f * static_cast<float>(i) / k_SweepSteps), k_SweepDelay));
	}
	turning.push_back(Align(0.0f, 3.0f));
	all.push_back({
	    .id = "hand.look_turning",
	    .name = "The hand turning evil and good",
	    .facet = Facet::Hand,
	    .description = "Every three seconds the player's alignment jumps: to -1, 0, 1, 0, -0.5 and 0.5. Then it sweeps "
	                   "from -1 to 1 and back by a hundredth every twentieth of a second, and returns to 0.",
	    .expected = "Each jump changes the hand at once, with no fading: red and ridged at -1, plain at 0, pale gold at 1, "
	                "half way at -0.5 and 0.5. In the sweep the hand changes in steps of three or four hundredths, its "
	                "ridges smoothing away as it nears neutral and its skin turning from red through orange-brown to gold.",
	    .environment = Looking(0.0f),
	    .framing = Close(),
	    .commands = turning,
	    .repeatFrom = 0,
	});

	// The part of the evening where the whole mean of the land's colour runs from 119 to 105, the only part where the
	// hand light's strength depends on how the mean is rounded: from 16.72 h (a mean of 119, just dark) to 17.359 h
	// (105). A step moves the sum of the colour's three bytes by two at most, so no whole value of the mean is skipped;
	// most values last ten steps or more, and the mean goes back up by one for a few steps where the blue rises before
	// the red falls. Seven frames of the checks' 16 ms step pass between steps, so the shot of a 900 frame run, at frame
	// 880, falls on 17.044 h, a mean of 112, in the middle of the range
	constexpr float k_EveningFrom = 16.72f;
	constexpr float k_EveningStep = 0.003f;
	constexpr int k_EveningSteps = 213;
	constexpr float k_EveningHold = 1.99f;
	constexpr float k_EveningDelay = 0.11f;
	std::vector<Command> evening {AtHour(k_EveningFrom, 0.0f)};
	for (int i = 1; i <= k_EveningSteps; ++i)
	{
		evening.push_back(
		    AtHour(k_EveningFrom + (k_EveningStep * static_cast<float>(i)), i == 1 ? k_EveningHold : k_EveningDelay));
	}
	evening.push_back(AtHour(k_EveningFrom, 3.0f));
	all.push_back({
	    .id = "hand.night_light",
	    .name = "The hand's light as the evening darkens",
	    .facet = Facet::Hand,
	    .description = "The clock stands still at 16.72 h with the player neutral, the camera looking down at the middle "
	                   "of the map from the south and the hand over it in the middle of the window. After two seconds the "
	                   "hour steps on by 0.003 h every 0.11 seconds to 17.359 h, then holds there for three seconds and "
	                   "starts again. The land's colour, its mean and the hand light's strength are logged each time "
	                   "the colour changes. Run it for 900 frames (the default): the shot, at frame 880, is at 17.044 h, "
	                   "a mean of 112.",
	    .expected = "At 16.72 h the mean of the land's colour is 119, just under 120: the lights are on, but still too "
	                "weak to show on the land. From about 16.98 h (a mean of 113) a faint warm patch about four cells "
	                "across lies on the land under the hand; at the shot of a 900 frame run (17.044 h, a mean of 112) it is "
	                "still faint, about five cells across. It grows wider and warmer as the evening darkens, about a whole "
	                "value of the mean every second and a half, and is full from about 17.34 h, when the mean is 105, "
	                "about eight cells across and nearly the full warm colour in its middle. The light "
	                "steps with each whole value of the mean, a fifteenth of full strength each.",
	    .environment =
	        {.hour = k_EveningFrom, .clockRuns = false, .dispenserGrid = false, .playerAlignment = 0.0f, .cursor = k_Cursor},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 90.0f, -80.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .commands = evening,
	    .repeatFrom = 0,
	    .logHandLight = true,
	});
}
