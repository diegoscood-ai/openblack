/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the player's hand finding its way about: hovering over the land, dragging it, turning and
// zooming the camera, and clicking, the hand put away by a script's cinema bars, the hand's tooltips over a town's
// buildings, and a seed applied by the hand to a creature. The mouse is driven through the same events the real one
// sends.

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

constexpr size_t k_Left = 1;
constexpr size_t k_Middle = 2;
constexpr size_t k_Right = 3;

Command PointerTo(glm::vec2 point, float delay)
{
	return {.kind = Kind::PointerTo, .delaySeconds = delay, .point = point};
}

Command Press(size_t button, float delay)
{
	return {.kind = Kind::PointerPress, .delaySeconds = delay, .value = button};
}

Command Release(size_t button, float delay)
{
	return {.kind = Kind::PointerRelease, .delaySeconds = delay, .value = button};
}

/// The mouse moved by a share of the screen over some seconds
Command Sweep(glm::vec2 by, float seconds, float delay)
{
	return {.kind = Kind::PointerSweep, .delaySeconds = delay, .point = by, .amount = seconds};
}

Command Wheel(size_t notches, bool towards, float delay)
{
	return {.kind = Kind::WheelTurn, .delaySeconds = delay, .value = notches, .ctrl = towards};
}

/// A script's cinema bars coming on or going
Command WideScreen(bool on, float delay)
{
	return {.kind = Kind::WideScreen, .delaySeconds = delay, .value = on ? 1u : 0u};
}

/// The miracle dispenser whose bubble the hand picks up and throws, from the middle of the map
constexpr glm::vec2 k_OrbDispenser {0.0f, 25.0f};
/// How high the middle of the dispenser's bubble is above the land: the dispenser's model stands 3.45 tall, the bubble
/// floats at 1.2 times that above its base, and the bubble's model has its middle 2.23 above where it floats. The
/// camera looks straight at it, so the pointer in the middle of the screen is over the bubble
constexpr float k_OrbCentreHeight = 6.4f;

/// The rock the hand carries among the palms, and the palms, east and west of the middle on one line across the view.
/// The camera looks north at a point half a metre above the middle, so both are on the screen's middle line, five
/// metres either side of the middle being about 0.15 of the screen's width at that distance
constexpr glm::vec2 k_CarriedRock {5.0f, 0.0f};
constexpr glm::vec2 k_Palms {-5.0f, 0.0f};
constexpr glm::vec2 k_RockOnScreen {0.65f, 0.5f};
constexpr float k_RockToPalms = -0.3f;
/// A volcanic rock's footprint radius is 3.91 at scale 1, and the hand lifts no rock whose radius is over 3.6 (it
/// taps it, and it splits), so it is carried at 0.9: a radius of 3.52, and 5.25 m to bend the palms from
constexpr float k_CarriedRockScale = 0.9f;

/// The town the hand's tooltips are read over: its town centre, made first so that the town stands there, with a
/// storage pit and a workshop on either side of it, nearer the camera
constexpr glm::vec2 k_TipCentre {0.0f, 40.0f};
constexpr glm::vec2 k_TipPit {-22.0f, 30.0f};
constexpr glm::vec2 k_TipWorkshop {22.0f, 30.0f};
/// The camera looks north at a point 7 m above the land in the middle of the town centre, from 45 m south of it and 30
/// m up, on the line the town centre's totem scenario looks along, so that the middle of the screen is on the same
/// spot of the centre's roof past the totem. The totem's icon, which that scenario's hand takes hold of 10.5 m above
/// the land, is 0.445 of the way down the screen. Aiming at the pit's middle 1 m above the land and at the workshop's
/// 3 m above it puts them a seventh of the screen in from the left and the right, below the middle
constexpr glm::vec3 k_TipEye {0.0f, 30.4f, -5.0f};
constexpr glm::vec3 k_TipLook {0.0f, 7.0f, 40.0f};
constexpr glm::vec2 k_TipCentreOnScreen {0.5f, 0.5f};
constexpr glm::vec2 k_TipTotemOnScreen {0.5f, 0.445f};
constexpr glm::vec2 k_TipPitOnScreen {0.15f, 0.71f};
constexpr glm::vec2 k_TipWorkshopOnScreen {0.86f, 0.68f};
/// How long the hand rests over each
constexpr float k_TipRest = 2.5f;
} // namespace

void testbed_scenarios::AddHandNavigationScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.rotate_release",
	    .name = "Turn the camera with the middle button and let go",
	    .facet = Facet::Hand,
	    .description = "The pointer rests low on the right of the screen, on bare land. The middle button is held while "
	                   "the mouse moves a third of the screen to the left over a second and a half, then let go; a "
	                   "second later the mouse moves a little up and right.",
	    .expected = "The camera turns about the land under the hand while the mouse moves. The hand and the cursor stay "
	                "where they were on the screen the whole time, the hand easing to the land that comes under it. "
	                "Letting go of the button leaves the hand where it is: it doesn't fly anywhere. The last small move "
	                "moves the hand on from there.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.75f, 0.85f}, 1.0f), Press(k_Middle, 1.0f), Sweep({-0.33f, 0.0f}, 1.5f, 0.2f),
	                 Release(k_Middle, 1.6f), Sweep({0.03f, -0.03f}, 0.3f, 1.0f)},
	});

	all.push_back({
	    .id = "hand.two_buttons",
	    .name = "Turn and zoom the camera with both buttons",
	    .facet = Facet::Hand,
	    .description = "The pointer rests low on the right, on bare land. Both buttons are held while the mouse moves down "
	                   "over a second and a half, then jumps right by a twentieth of the screen in a frame and moves on "
	                   "right slowly for a second; then they are let go.",
	    .expected = "Moving down zooms the camera and, once the mouse has moved a fortieth of the screen's width across in "
	                "a frame, moving across turns it. The hand doesn't grip the land: it keeps its idle hover and stays with "
	                "the cursor where it was on "
	                "the screen, and stays there once the buttons are let go.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.7f, 0.85f}, 1.0f), Press(k_Left, 1.0f), Press(k_Right, 0.0f),
	                 Sweep({0.0f, 0.15f}, 1.5f, 0.2f), Sweep({0.05f, 0.0f}, 0.001f, 1.6f), Sweep({0.1f, 0.0f}, 1.0f, 0.1f),
	                 Release(k_Right, 1.2f), Release(k_Left, 0.0f)},
	});

	all.push_back({
	    .id = "hand.drag",
	    .name = "Drag the land",
	    .facet = Facet::Hand,
	    .description = "The pointer rests low on the right of the screen. The left button is held while the mouse moves "
	                   "up and left over a second, then let go.",
	    .expected = "Pressing grips the land under the hand at once: the hand fades onto the land over 0.13 seconds "
	                "as it changes to its grip. The land then follows the hand, which stays on the spot it "
	                "gripped. Letting go, the hand fades back to hovering at the cursor as quickly, carrying on from how "
	                "far the gripped land was from the camera.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.85f, 0.85f}, 1.0f), Press(k_Left, 1.0f), Sweep({-0.15f, -0.15f}, 1.0f, 0.2f),
	                 Release(k_Left, 1.2f)},
	});

	all.push_back({
	    .id = "hand.edge_hover",
	    .name = "Hover at the edges of the screen",
	    .facet = Facet::Hand,
	    .description = "The pointer rests in the middle, then at the right edge, the bottom edge, the very bottom, and "
	                   "the top of the screen, a second at each.",
	    .expected = "In the middle the hand hovers in its ordinary pose. Near the sides (beyond 45% of the half width), "
	                "near the bottom (below 43%) and at the top (above 49%, or 40% over no land) it shows the turning "
	                "pose, standing up towards where the camera looks; the very bottom (below 49%) offers tilting too, "
	                "but turning shows.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.5f, 0.6f}, 1.0f), PointerTo({0.98f, 0.6f}, 1.0f), PointerTo({0.5f, 0.95f}, 1.0f),
	                 PointerTo({0.5f, 0.995f}, 1.0f), PointerTo({0.5f, 0.004f}, 1.0f), PointerTo({0.5f, 0.6f}, 1.0f)},
	});

	all.push_back({
	    .id = "hand.edge_rotate",
	    .name = "Drag round the edge to turn the camera",
	    .facet = Facet::Hand,
	    .description = "The left button is pressed at the right edge of the screen, and the mouse moves down slowly for a "
	                   "second and a half, then is let go.",
	    .expected = "Once the mouse has moved a fiftieth of the screen the drag turns the camera: the cursor and the hand "
	                "are held on a ring nine tenths of the way out from the middle, and the camera turns by the angle "
	                "they sweep round the middle. The hand shows the turning pose, a third of its height higher.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.98f, 0.55f}, 1.0f), Press(k_Left, 1.0f), Sweep({0.0f, 0.3f}, 1.5f, 0.2f),
	                 Release(k_Left, 1.6f)},
	});

	all.push_back({
	    .id = "hand.edge_pan",
	    .name = "A quick drag in from the edge pans",
	    .facet = Facet::Hand,
	    .description = "The left button is pressed at the right edge of the screen, and the mouse moves quickly in towards "
	                   "the middle, then is let go.",
	    .expected = "Pressed at the edge the hand offers turning, but moving quickly (within 0.3 s) towards the middle "
	                "the drag pans: the hand grips the land and the land follows it.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.98f, 0.6f}, 1.0f), Press(k_Left, 1.0f), Sweep({-0.2f, 0.0f}, 0.2f, 0.0f),
	                 Sweep({-0.2f, 0.0f}, 0.8f, 0.25f), Release(k_Left, 1.0f)},
	});

	all.push_back({
	    .id = "hand.top_pitch",
	    .name = "Drag up and down at the top to tilt",
	    .facet = Facet::Hand,
	    .description = "The left button is pressed at the top of the screen and the mouse moves down slowly, then up, "
	                   "then is let go.",
	    .expected = "The drag tilts the camera, by seven thirds of the field of view across for a screen's height of "
	                "movement, down and then back up; the hand shows the tilting pose and keeps to the cursor.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.5f, 0.004f}, 1.0f), Press(k_Left, 1.0f), Sweep({0.0f, 0.15f}, 1.0f, 0.2f),
	                 Sweep({0.0f, -0.1f}, 1.0f, 1.1f), Release(k_Left, 1.1f)},
	});

	all.push_back({
	    .id = "hand.fast_pan",
	    .name = "Drag the land quickly",
	    .facet = Facet::Hand,
	    .description = "The left button is pressed low in the middle of the screen, the mouse moves a quarter of the "
	                   "screen up and left in a fifth of a second, rests a second, and the button is let go.",
	    .expected = "The land gripped follows the cursor: the camera eases after it over about 0.3 seconds, as the game's "
	                "camera does, so the hand trails the cursor while it moves and settles under it once it stops. "
	                "Letting go, nothing jumps: the hand is already under the cursor.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.5f, 0.8f}, 1.0f), Press(k_Left, 1.0f), Sweep({-0.25f, -0.25f}, 0.2f, 0.2f),
	                 Release(k_Left, 1.2f)},
	});

	all.push_back({
	    .id = "hand.zoom",
	    .name = "Zoom with the wheel",
	    .facet = Facet::Hand,
	    .description = "The pointer rests low on the right of the screen. The wheel turns three notches away, then three back.",
	    .expected = "The camera zooms in towards the land and out again. The hand stays at the cursor on the screen, "
	                "easing to the land as it comes nearer and goes away.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.8f, 0.85f}, 1.0f), Wheel(3, false, 1.0f), Wheel(3, true, 2.0f)},
	});

	all.push_back({
	    .id = "hand.orb_pick_throw",
	    .name = "Pick up a miracle's bubble, swing it and throw it",
	    .facet = Facet::Hand,
	    .description = "A food miracle dispenser stands north of the middle with its bubble, the camera looking straight at "
	                   "the bubble from the south (a village to the west gives the dispenser its town). The pointer rests "
	                   "on the bubble and the right button is held for 0.6 seconds, which picks the bubble itself up; the "
	                   "pointer moves a little up, swings right, left and back, and then the right button is pressed again "
	                   "and let go while the pointer moves quickly up the screen, throwing the bubble north onto the land.",
	    .expected = "Held for more than a moment the bubble comes into the hand whole, without its seed being taken out "
	                "of it, and it comes without a jolt: the holding spring starts from rest. Wherever it is, held, "
	                "tilted as the hand sways with the swings, or tumbling through the air after the throw, the bubble "
	                "stays a round dome turned to the camera about its own middle, with the horn of plenty spinning in "
	                "the middle of it. At the throw it leaves from where the hand was drawing it, with no jump, flies "
	                "north and comes down on the land, still in view. The dispenser makes no other bubble.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_OrbDispenser.x, 30.0f, k_OrbDispenser.y - 40.0f},
	                .look = {k_OrbDispenser.x, k_OrbCentreHeight, k_OrbDispenser.y}},
	    .commands = {PointerTo({0.5f, 0.5f}, 1.0f), Press(k_Right, 1.0f), Release(k_Right, 0.6f),
	                 Sweep({0.0f, -0.05f}, 0.5f, 0.5f), Sweep({0.15f, 0.0f}, 0.5f, 1.0f), Sweep({-0.3f, 0.0f}, 1.0f, 0.8f),
	                 Sweep({0.15f, 0.0f}, 0.5f, 1.2f), Press(k_Right, 1.0f), Sweep({0.0f, -0.06f}, 0.4f, 0.3f),
	                 Release(k_Right, 0.2f)},
	    .fixtures = {.dispensers = {{.magic = MagicType::Food, .at = k_OrbDispenser}},
	                 .villages = {{.at = glm::vec2 {-60.0f, 10.0f}, .huts = 1, .villagers = 0, .storagePit = false}}},
	});

	// The hand carries a rock in among some palms, whose trunks it bends away, then a script's cinema bars come on while
	// the pointer moves about, and go again with the pointer back over the palms
	all.push_back({
	    .id = "hand.put_away_over_trees",
	    .name = "The hand put away by a script's cinema bars, over trees",
	    .facet = Facet::Hand,
	    .description = "A volcanic rock lies east of the middle and four palms stand together west of it, the camera "
	                   "looking at both from the south. The hand picks the rock up with the right button and carries it in "
	                   "among the palms. Two seconds later a script's cinema bars come on, and while they are up the "
	                   "pointer moves down, across and back to where it was over the palms; three and a half seconds "
	                   "after they came on the bars go, and two seconds later the hand carries the rock back east.",
	    .expected = "Among the palms the rock in the hand bends the trunks near it away from it, with the rubbing "
	                "trees' sound. With the bars up the hand is put away: it is not drawn, it does not follow the "
	                "pointer, and the palms it was bending stand up straight again and sway with the wind, as if "
	                "nothing were there. When the bars go the hand is back over the palms with the rock, and bends "
	                "them again until it carries the rock away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 10.0f, -25.0f}, .look = {0.0f, 0.5f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::RockVolcanic, .offset = k_CarriedRock, .scale = k_CarriedRockScale}},
	    .commands = {PointerTo(k_RockOnScreen, 1.0f), Press(k_Right, 1.0f), Release(k_Right, 0.6f),
	                 Sweep({k_RockToPalms, 0.0f}, 1.5f, 0.5f), WideScreen(true, 3.5f), Sweep({0.0f, 0.1f}, 1.0f, 0.5f),
	                 Sweep({0.1f, -0.1f}, 1.0f, 1.0f), Sweep({-0.1f, 0.0f}, 1.0f, 1.0f), WideScreen(false, 1.0f),
	                 Sweep({-k_RockToPalms, 0.0f}, 1.5f, 2.0f)},
	    .fixtures = {.trees = {{.type = TreeInfo::Palm, .at = k_Palms, .count = 4, .spread = 2.0f}}},
	});

	// The hand pulls at a beech (18 metres tall at its full size, gripped a tenth of its height up, weighing 1000): the
	// pointer in the middle of the screen, on the point the camera looks at. The pull moves the hand on a plane through
	// the tree's base, as high as the pointer's line crosses the tree at the press
	constexpr glm::vec2 k_Beech {0.0f, 0.0f};
	all.push_back({
	    .id = "hand.tug_at_grip",
	    .name = "The hand pulls a beech up by its grip",
	    .facet = Facet::Hand,
	    .description = "A beech stands in the middle, the camera south of it looking a little down at its trunk, a tenth "
	                   "of its height up. The right button goes down on the trunk there; the pointer rests a second, then "
	                   "moves right across the screen, the button still down.",
	    .expected = "Pressed at its grip the beech stays in the ground while the pointer rests: the hand closes round "
	                "the trunk, and after its fade (about a sixth of a second) it holds it still. As the pointer moves "
	                "right the hand slides across the land's plane at the grip's height, the beech leans after it about "
	                "its base and its trunk stretches a little towards it, and it comes out, with the tree breaking "
	                "sound and a pile of roots where it stood, once the hand is about a metre from where it grips it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Beech.x, 6.0f, k_Beech.y - 16.0f}, .look = {k_Beech.x, 1.8f, k_Beech.y}},
	    .objects = {{.type = TreeInfo::Beech, .offset = k_Beech}},
	    .commands = {PointerTo({0.5f, 0.5f}, 1.0f), Press(k_Right, 1.0f), Sweep({0.15f, 0.0f}, 1.5f, 1.0f),
	                 Release(k_Right, 1.0f)},
	});
	// Pressed in its crown, below its top leaves, which end about 13 metres above the land as it is drawn: a line through
	// the middle of the screen at 13.5 metres passes over them to the land behind
	all.push_back({
	    .id = "hand.tug_at_crown",
	    .name = "The hand pulls a beech up by its crown",
	    .facet = Facet::Hand,
	    .description = "The same beech, the camera south of it looking a little down at its crown, ten and a half metres "
	                   "up. The right button goes down on the crown and stays down; the pointer does not move.",
	    .expected = "Pressed far above its grip, the beech comes out about a sixth of a second after the press, as soon "
	                "as the hand's fade is over, without the pointer moving: the hand pulls from the crown's height down "
	                "at the grip, harder than the tree weighs.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Beech.x, 12.0f, k_Beech.y - 22.0f}, .look = {k_Beech.x, 10.5f, k_Beech.y}},
	    .objects = {{.type = TreeInfo::Beech, .offset = k_Beech}},
	    .commands = {PointerTo({0.5f, 0.5f}, 1.0f), Press(k_Right, 1.0f), Release(k_Right, 2.0f)},
	});
	all.push_back({
	    .id = "hand.tug_let_go",
	    .name = "The hand lets go of a beech before it comes out",
	    .facet = Facet::Hand,
	    .description = "The beech pressed at its grip as in the first pull, the pointer moved only a little to the right, "
	                   "and the button let go a second later.",
	    .expected = "The beech leans a little after the hand and its trunk stretches a little, but stays in the ground; "
	                "let go, it stands upright again at once, as it stood, with no roots and no sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Beech.x, 6.0f, k_Beech.y - 16.0f}, .look = {k_Beech.x, 1.8f, k_Beech.y}},
	    .objects = {{.type = TreeInfo::Beech, .offset = k_Beech}},
	    .commands = {PointerTo({0.5f, 0.5f}, 1.0f), Press(k_Right, 1.0f), Sweep({0.02f, 0.0f}, 0.5f, 1.0f),
	                 Release(k_Right, 1.0f)},
	});
	// A beech four times the size weighs 4^3 x 1000 = 64000, more than the hand's greatest pull (600000) over gravity
	// (9.81) lets it lift. The camera looks down at it: with a level camera the pointer's line through the middle of the
	// screen runs along the pull's level plane and never meets it, and the hand's place on it is 0 / 0, in the original
	// as here
	all.push_back({
	    .id = "hand.tug_heavy_tree",
	    .name = "The hand pulls at a beech too heavy to come out",
	    .facet = Facet::Hand,
	    .description = "A beech four times its full size (72 metres tall) in the middle, the camera far south of it and "
	                   "above, looking a little down at its crown. The right button goes down on the crown, the pointer "
	                   "sweeps far to the right and back with the button down, and the button is let go.",
	    .expected = "However far the hand pulls, the beech never comes out: it is heavier than the hand can lift. It "
	                "leans hard after the hand about its base and its trunk stretches towards it, up to 1.3 times; let "
	                "go, it stands upright again at once, with no roots and no sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Beech.x, 60.0f, k_Beech.y - 110.0f}, .look = {k_Beech.x, 45.0f, k_Beech.y}},
	    .objects = {{.type = TreeInfo::Beech, .offset = k_Beech, .scale = 4.0f}},
	    .commands = {PointerTo({0.5f, 0.5f}, 1.0f), Press(k_Right, 1.0f), Sweep({0.3f, 0.0f}, 2.0f, 1.0f),
	                 Sweep({-0.3f, 0.0f}, 2.0f, 0.5f), Release(k_Right, 1.0f)},
	});

	all.push_back({
	    .id = "hand.click",
	    .name = "Click the land",
	    .facet = Facet::Hand,
	    .description = "The pointer moves across the screen, then the left button is clicked.",
	    .expected = "The hand follows the cursor, easing in and out to its height above the land. The click grips the "
	                "land for a moment, the hand settling onto it, and lets it go without moving the camera.",
	    .framing = {.shot = Shot::Testbed},
	    .commands = {PointerTo({0.6f, 0.75f}, 1.0f), Sweep({0.3f, 0.1f}, 1.0f, 0.5f), Press(k_Left, 1.5f),
	                 Release(k_Left, 0.1f)},
	});

	// The hand finds the land under the pointer from the plane, across the lake and its shallows, up to the sky, with
	// the camera low south of the lake looking north across it
	const glm::vec2 lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "hand.land_to_sky",
	    .name = "The hand from the land across the water to the sky",
	    .facet = Facet::Hand,
	    .description = "The camera stands 12 above the plane south of the lake, looking north across it at the plane's "
	                   "height. The pointer rests near the bottom of the screen over the plane, sweeps up over four "
	                   "seconds across the bank, the shallows and the lake's water and the plane beyond, to above the "
	                   "horizon, rests there a second, and comes back down over a second to the lake's water.",
	    .expected = "Over the plane the hand stays 3.2 times its size short of the land along the pointer's ray. Over the "
	                "lake and its shallows, which lie at the water's level, it goes down all the way to the water, with "
	                "no gap. Above the horizon, where there is no land under the pointer, it stays as far from the camera "
	                "as where it last was, turning only with the pointer, and nothing pulls it out to its reach. Coming "
	                "back down it eases onto the land again, faster coming closer than going away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {lake.x, 12.0f, lake.y - 120.0f}, .look = {lake.x, 0.0f, lake.y + 200.0f}},
	    // a command's delay runs from the one before it starts: the sweep back waits for the sweep up and the rest
	    .commands = {PointerTo({0.5f, 0.9f}, 1.0f), Sweep({0.0f, -0.85f}, 4.0f, 1.0f), Sweep({0.0f, 0.65f}, 1.0f, 5.0f)},
	});

	// The hand's tooltips over a town's buildings. The scenario's town is the player's and believes in the player as a
	// land's script makes its player's town believe; its buildings are whole, as a script's are, and the town's
	// influence reaches all of them. Its two villagers stand behind the town centre, out of the pointer's way
	all.push_back({
	    .id = "hand.tooltips",
	    .name = "The hand's tooltips over a town's buildings",
	    .facet = Facet::Hand,
	    .description = "A Celtic town of the player's, with its town centre, a storage pit holding 500 food and 500 wood "
	                   "to its south west and a workshop to its south east, all built, and two villagers. The pointer "
	                   "rests two and a half seconds on the storage pit, then on the workshop, then on the town centre's "
	                   "roof, then on the icon of the village totem in the middle of the centre, and last on the town "
	                   "centre again, where it stays.",
	    .expected = "Over the storage pit the hand shows what it stores, 500 food and 500 wood. Over the workshop it shows "
	                "the workshop's text just as it is written, with no number put in. Over the town centre and over its "
	                "totem it shows the same two lines: how far the player leads in believers, and the town's population "
	                "out of its places. None of them shows a button. The shot is taken over the town centre.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = k_TipEye, .look = k_TipLook},
	    .objects =
	        {{.type = AbodeInfo::CelticTownCentre, .offset = k_TipCentre},
	         {.type = AbodeInfo::CelticStoragePit, .offset = k_TipPit},
	         {.type = AbodeInfo::CelticWorkshop, .offset = k_TipWorkshop},
	         {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_TipCentre + glm::vec2(8.0f, 18.0f), .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_TipCentre + glm::vec2(-8.0f, 18.0f), .joinTown = true}},
	    .commands = {PointerTo(k_TipPitOnScreen, 1.0f), PointerTo(k_TipWorkshopOnScreen, k_TipRest),
	                 PointerTo(k_TipCentreOnScreen, k_TipRest), PointerTo(k_TipTotemOnScreen, k_TipRest),
	                 PointerTo(k_TipCentreOnScreen, k_TipRest)},
	    // As a land's script after it makes the player's town: the most the town's belief cap lets it believe
	    .townBelief = 10.0f,
	});

	// A miracle's seed applied by the hand to a creature, the right button pressed on its body; the creature is kept
	// content and still, so that only the spell changes it
	const CreatureSetup target {.label = "target",
	                            .species = CreatureType::Tiger,
	                            .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                            .hold = true,
	                            .pauseMind = true};
	all.push_back({
	    .id = "miracles.hand_big_on_creature",
	    .name = "The hand casts big on a creature",
	    .facet = Facet::Miracles,
	    .description = "A tiger stands still in the middle of the testbed, its mind paused, and the big spell is put in "
	                   "the player's hand. The pointer rests on the tiger's body and the right button is pressed and let "
	                   "go there.",
	    .expected = "The press applies the seed to the tiger: the spell takes hold of it, and it grows towards the "
	                "largest size it can take (2.4 unless its creature file says otherwise).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {target},
	    .commands = {PointerTo({0.5f, 0.45f}, 1.0f), Press(k_Right, 1.0f), Release(k_Right, 0.2f)},
	    .fixtures = {.handSeed = MagicType::CreatureSpellBig},
	});

	all.push_back({
	    .id = "miracles.hand_freeze_on_creature",
	    .name = "The hand casts freeze on a creature",
	    .facet = Facet::Miracles,
	    .description = "A tiger stands still in the middle of the testbed, its mind paused, and the freeze spell is put "
	                   "in the player's hand. The pointer rests on the tiger's body and the right button is pressed and "
	                   "let go there.",
	    .expected = "The press applies the seed to the tiger: the spell takes hold of it, it turns icy as the freeze "
	                "grows, and what it plays (its breathing, its face, its blinking) slows down with it. When the spell "
	                "ends it thaws and plays at its own speed again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {target},
	    .commands = {PointerTo({0.5f, 0.45f}, 1.0f), Press(k_Right, 1.0f), Release(k_Right, 0.2f)},
	    .fixtures = {.handSeed = MagicType::CreatureSpellFreeze},
	});
}
