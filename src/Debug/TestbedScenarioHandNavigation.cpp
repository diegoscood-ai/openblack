/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the player's hand finding its way about: hovering over the land, dragging it, turning and
// zooming the camera, and clicking, and a seed applied by the hand to a creature. The mouse is driven through the same
// events the real one sends.

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

/// The miracle dispenser whose bubble the hand picks up and throws, from the middle of the map
constexpr glm::vec2 k_OrbDispenser {0.0f, 25.0f};
/// How high the middle of the dispenser's bubble is above the land: the dispenser's model stands 3.45 tall, the bubble
/// floats at 1.2 times that above its base, and the bubble's model has its middle 2.23 above where it floats. The
/// camera looks straight at it, so the pointer in the middle of the screen is over the bubble
constexpr float k_OrbCentreHeight = 6.4f;
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
}
