/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the sky: the sun's glare, dimmed by the land and by what stands near the camera between
// it and the sun; the light of the day and of the night on a row of trees; and the moon at night, looked at from low on
// the land towards the east, where it keeps its place beside the camera, and over the lake, where its mirror image and
// its glow's show in the water

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The camera stands in the lake, 3 above its water near the corner furthest from the sun, so that the line to the sun,
/// far out beyond the map's corner at its lowest x and z, crosses the open water and then the lake's bank, whose top is
/// about 205 units away and seen about 5.2 degrees up from it
constexpr glm::vec3 k_LakeEye {45.0f, 3.0f, 265.0f};
/// It looks along that line 100 units out, at the sun's middle at the hour below
constexpr glm::vec3 k_TowardsTheSun {-25.47f, 12.56f, 194.05f};
/// The hour the sun stands about 4425 high: its middle and its two upper samples clear the bank's top, and the two
/// lower ones, 500 below, do not
constexpr float k_SunOverTheBank = 9.54f;
/// A pillar of rock in the water 40 units out along the line to the sun, well within the depth the glare's samples
/// are hidden by what is drawn in front of them
constexpr glm::vec2 k_PillarInTheLake {16.8f, 236.6f};

/// A row of five kinds of tree north of the middle, 6 apart from west to east, and a pillar of rock to the west of
/// them, lit as every model is, beside them
std::vector<ObjectSetup> TreeRow()
{
	return {
	    {.type = TreeInfo::Oak, .offset = {-12.0f, 30.0f}},  {.type = TreeInfo::Beech, .offset = {-6.0f, 30.0f}},
	    {.type = TreeInfo::Birch, .offset = {0.0f, 30.0f}},  {.type = TreeInfo::Pine, .offset = {6.0f, 30.0f}},
	    {.type = TreeInfo::Cedar, .offset = {12.0f, 30.0f}}, {.type = FeatureInfo::FatPilarChalk, .offset = {-20.0f, 26.0f}}};
}
/// The camera stands to the south east of the row and looks north west across it. By day the light every model is lit
/// with is the sun, far off to the south west and high: the crowns' south sides, on the left of the shot, face it, and
/// their east sides, on the right, are turned away from it
constexpr glm::vec3 k_TreesEye {24.0f, 8.0f, 6.0f};
constexpr glm::vec3 k_TreesLook {0.0f, 6.0f, 30.0f};
/// At night that light is the hand's: held still by the east end of the row, so that the trees nearest it are lit from
/// the east and those further off from further along the row
constexpr glm::vec2 k_HandByTheRow {14.0f, 24.0f};

/// A camera low on the land looking a little north of east and a little up, so the moon at midnight, 4000 east and 950
/// up, stands right of the middle of the screen, clear of the scenario window
const Framing k_LookingEast {.shot = Shot::Placed, .eye = {0.0f, 10.0f, 0.0f}, .look = {400.0f, 105.0f, 90.0f}};

/// A few trees to the east, dark against the sky, which the moon shows behind
std::vector<ObjectSetup> TreesToTheEast()
{
	return {
	    {.type = TreeInfo::Conifer, .offset = {60.0f, -12.0f}},
	    {.type = TreeInfo::Oak, .offset = {75.0f, 10.0f}},
	    {.type = TreeInfo::Conifer, .offset = {90.0f, 30.0f}},
	};
}

/// The camera stands 3 above the lake's water near its west bank and looks level, a little north of east, across the
/// water. The moon at midnight stands about 13 degrees up, right of the middle of the screen, and its mirror image as
/// far down, in the water about 13 units out from the camera, with the near half of its glow's about it
const Framing k_OverTheLake {.shot = Shot::Placed, .eye = {-45.0f, 3.0f, 220.0f}, .look = {45.0f, 3.0f, 240.0f}};

/// A few trees on the lake's north east bank, left of the moon and clear of it, dark against the sky
std::vector<ObjectSetup> TreesOnTheFarBank()
{
	return {
	    {.type = TreeInfo::Conifer, .offset = {70.0f, 275.0f}},
	    {.type = TreeInfo::Oak, .offset = {82.0f, 288.0f}},
	    {.type = TreeInfo::Conifer, .offset = {62.0f, 296.0f}},
	};
}
} // namespace

void testbed_scenarios::AddSkyScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "sky.sun_glare_behind_bank",
	    .name = "The sun's glare over the lake's bank, behind a pillar of rock",
	    .facet = Facet::Sky,
	    .description = "The clock stands still at 9.54 h in clear weather. The camera stands 3 above the lake's water "
	                   "near the corner furthest from the sun, looking at the sun, which shows just over the lake's bank "
	                   "on the other side, about 205 units away; a pillar of rock stands in the water 40 units out, "
	                   "between the camera and the sun. Which of the glare's samples the land hides is logged each time "
	                   "it changes.",
	    .expected = "The glare looks at the sun's middle and four points 500 units from it, and dims by a fifth for each "
	                "one hidden. The bank hides the two lower points and not the middle or the upper two; the log says "
	                "which: two of five, (-500, -500) and (500, -500). The pillar stands in front of the other three, "
	                "drawn nearer the camera than the depth that hides a sample, so the drawing hides them too, answered "
	                "a frame or two late: the glare fades out at once and is not drawn, and the pillar shows with no glare "
	                "over it. Without the pillar the glare would stand at three fifths.",
	    .environment = {.hour = k_SunOverTheBank, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_LakeEye, .look = k_TowardsTheSun},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = k_PillarInTheLake}},
	    .logSunGlare = true,
	});

	all.push_back({
	    .id = "sky.trees_by_day",
	    .name = "A row of trees by day",
	    .facet = Facet::Sky,
	    .description = "The clock stands still at noon in clear weather. Five trees stand in a row north of the middle, an "
	                   "oak, a beech, a birch, a pine and a cedar, 6 apart, with a pillar of rock to the west of them; the "
	                   "camera looks north west across the row from its south east.",
	    .expected = "The trees are drawn in the land's light where each stands, hazed a little with distance and times "
	                "their brightness for the way the camera looks; the sun does not shade them, so each crown is as "
	                "bright on its east side, turned away from the sun, as on its south side, facing it. The pillar of "
	                "rock is shaded by the sun: brighter on the side facing it, darker on the side turned away.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_TreesEye, .look = k_TreesLook},
	    .objects = TreeRow(),
	});

	all.push_back({
	    .id = "sky.trees_at_night",
	    .name = "A row of trees at night, by the hand's light",
	    .facet = Facet::Sky,
	    .description = "The same row of trees, pillar of rock and camera as sky.trees_by_day, with the clock standing still "
	                   "at midnight, when the light every model is lit with goes to the player's hand; the hand is held "
	                   "still 6 above the land by the east end of the row.",
	    .expected = "The trees are drawn in the land's dark night light where each stands, hazed and times their "
	                "brightness, and the hand's light does not shade them: each crown is evenly dark, the side facing the "
	                "hand no brighter than the side turned away. The pillar of rock is lit by the hand's light on the side "
	                "facing it.",
	    .environment = {.hour = 0.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_TreesEye, .look = k_TreesLook},
	    .objects = TreeRow(),
	    .hand = HandHold {.offset = k_HandByTheRow, .height = 6.0f},
	});

	all.push_back({
	    .id = "sky.fireflies_evening",
	    .name = "Fireflies over the row of trees in the evening",
	    .facet = Facet::Sky,
	    .description = "The same row of trees, pillar of rock and camera as sky.trees_by_day, with the clock standing still "
	                   "at half past ten at night, when the fireflies come out to the trees and rocks.",
	    .expected = "Small light blue sparks, the fireflies, circle about the trees and the rock, each fading out from 100 "
	                "to 300 units from the camera: the blue of their sprite cell, not white.",
	    .environment = {.hour = 22.5f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_TreesEye, .look = k_TreesLook},
	    .objects = TreeRow(),
	});

	all.push_back({
	    .id = "sky.moon_midnight",
	    .name = "The moon at midnight",
	    .facet = Facet::Sky,
	    .description = "The clock stands at midnight under a clear sky, the camera low on the land looking east at the "
	                   "moon.",
	    .expected = "A half sphere of moon, about 270 across, hangs high in the east at its full strength, with a soft "
	                "glow about it, tinted the night's moon colour. Its lit part matches the real moon tonight, and "
	                "scripts are told 0 at a full moon and 1 at a new moon once it has shown with that phase. It stays the "
	                "same size and place as the camera moves, and its mirror image shows in the sea. The trees in front of "
	                "it hide it where they stand against it.",
	    .environment = {.hour = 0.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = k_LookingEast,
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.moon_night",
	    .name = "The moon through a night",
	    .facet = Facet::Sky,
	    .description = "The clock runs from 19:00 under a clear sky, the camera low on the land looking east.",
	    .expected = "No moon until about 19:20 script time, when it fades in low in the south east; it is at its full "
	                "strength from about 21:00, highest at midnight due east, and fades out after 03:00, gone by about "
	                "04:40 low in the north east. Its phase does not change through the night.",
	    .environment = {.hour = 19.0f, .clockRuns = true, .dispenserGrid = false},
	    .framing = k_LookingEast,
	    .objects = TreesToTheEast(),
	});

	all.push_back({
	    .id = "sky.moon_over_sea",
	    .name = "The moon over the water",
	    .facet = Facet::Sky,
	    .description = "The clock stands at midnight under a clear sky. The camera stands 3 above the lake's water near "
	                   "its west bank, looking level across the water a little north of east; a few trees stand on the "
	                   "north east bank, left of the moon.",
	    .expected = "The moon hangs about 13 degrees up, right of the middle of the screen, with its glow about it. Its "
	                "mirror image shows as far down in the water, about 13 units out, with its own glow: the mirrored "
	                "moon is drawn first and the glow over it, as in the sky the glow is drawn first and the moon over "
	                "it, so where the two overlap the water's copy is brighter than the sky's. The mirrored moon is the "
	                "sky's turned upside down, its lit part with it.",
	    .environment = {.hour = 0.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = k_OverTheLake,
	    .objects = TreesOnTheFarBank(),
	});
}
