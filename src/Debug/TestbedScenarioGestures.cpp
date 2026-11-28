/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the gestures drawn with the hand: each gesture is drawn as a path through the same
// recogniser the cursor goes through, so the debug window's gesture overlay shows it being drawn and matched

#include <string_view>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// A gesture takes a second or two to draw; the next command waits for it and the hand's rest after it
constexpr float k_DrawSeconds = 3.0f;
/// The circle is drawn again this long after the last drawing ends, once the miracles have forgotten the last circle
/// (five seconds): the second circle is then recognised about 12 seconds in, so that its sheet of stars stands at its
/// tallest at the checks' shot, 14 seconds in
constexpr float k_CircleAgainSeconds = 5.8f;

/// The hidden phone box's places on Land 1, from the script that makes them: the first box, then the first two of the
/// five boxes that rise once it sinks
constexpr glm::vec2 k_PhoneBox {3130.035f, 1639.275f};
constexpr glm::vec2 k_PhoneBoxLand1 {3124.9204f, 1644.276f};
constexpr glm::vec2 k_PhoneBoxLand2 {3127.4368f, 1644.2347f};
/// The influence ring put round the boxes once the script has ended, so that the hand's press on one is refused by the
/// box's own flags and not by the influence
constexpr float k_BoxesInfluenceRadius = 40.0f;
/// From the box's third message to the camera over the first of the five boxes: the fourth press, the box sinking and
/// the five rising
constexpr float k_SinkSeconds = 90.0f;
/// From the glade's last line read to the creature given to the player, its home set and the glade's camera let go
constexpr float k_GivenSeconds = 15.0f;
/// The phone box scripts whose tasks the real input run logs
constexpr std::string_view k_PhoneBoxScripts = "HiddenScript PlayHiddenMessage PlayHiddenSelectionMessage HiddenToLands";

/// The camera 10 over the land and 20 back from a phone box, looking at its middle, 1.2 over the land, so that the box
/// is at the middle of the 1024 x 768 window, at (512, 384)
Command CameraOnBox(glm::vec2 box, float delaySeconds)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delaySeconds,
	        .eye = {box.x, 10.0f, box.y - 20.0f},
	        .look = {box.x, 1.2f, box.y}};
}

/// Waits until a task of the script runs, or until the one running has ended
Command WaitTask(std::string_view script, testbed_land_timeline::TaskWait wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

Command HoldSeed(SpellSeedType seed, float delaySeconds)
{
	return {.kind = Kind::HoldSeed, .delaySeconds = delaySeconds, .value = static_cast<size_t>(seed)};
}

/// A command to the hand that waits until the hand has finished drawing, its delay counting from then
Command AfterDrawing(Command command)
{
	command.waitUntilFree = true;
	return command;
}

Command Draw(GestureType gesture, float delaySeconds = k_DrawSeconds)
{
	return {.kind = Kind::DrawGesture, .delaySeconds = delaySeconds, .value = static_cast<size_t>(gesture)};
}

CreatureSetup Tiger()
{
	return {.label = "your tiger",
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 50.0f},
	        .facingDegrees = 0.0f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	        .hold = true};
}
} // namespace

void testbed_scenarios::AddGestureScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.gesture_circle_shield",
	    .name = "Size a shield with a circle",
	    .facet = Facet::Miracles,
	    .description = "A shield seed is put in the hand, the pointer is brought onto the land in the middle of the "
	                   "screen, inside the player's influence, and a circle is drawn round the middle of the screen with "
	                   "the Action button held, which readies the shield; it is drawn again once the circle is forgotten. "
	                   "Open the Gestures debug window to see the path and the template it matches. Hold a shield or "
	                   "storm seed and draw a circle with the right button held to try it.",
	    .expected = "The press over the land, inside the influence, takes the seed: the circle is recognised, the log "
	                "gives its middle on the land and its radius across the land, and the miracles take it for the "
	                "shield, which is cast there at that size on letting go (the shield stands on the shot). It is "
	                "remembered for five seconds. The hand draws the circle: while it holds the shield seed a faint "
	                "ribbon in the player's colour trails behind it, each part fading and narrowing away over two "
	                "seconds. On recognition the gesture's sound plays and its trail appears on the land: 234 small "
	                "glows in the player's colour grow from both ends of the drawn path over 0.6 seconds while flowing "
	                "onto a true circle laid on the land, raised a little towards the camera, dim and wiggling; a sheet "
	                "of stars in the player's colour rises along the circle in a wave, tallest two seconds in and gone "
	                "by four; at 2.4 seconds the glows flash to full brightness and the hand glows the player's colour "
	                "for half a second, the flash dying away by 4.5 seconds; from three seconds the wiggle shrinks away "
	                "over two seconds and the trail goes at seven.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    // The press takes the point under the pointer as the last frame drew it, so the pointer is on the land, inside
	    // the influence, before the circle starts. The seed is given again for when the last one was cast; while the hand
	    // still holds it, the new one is refused
	    .commands = {HoldSeed(SpellSeedType::Shield, 1.0f),
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.5f}},
	                 Draw(GestureType::Circle, 1.0f),
	                 AfterDrawing(HoldSeed(SpellSeedType::Shield, k_CircleAgainSeconds)),
	                 Draw(GestureType::Circle, 1.0f)},
	    .repeatFrom = 3,
	});

	all.push_back({
	    .id = "miracles.gesture_power_up_storm",
	    .name = "Power up a storm",
	    .facet = Facet::Miracles,
	    .description = "A storm seed is summoned from the player's worship into the hand (only such a seed can be powered "
	                   "up) and its first power-up gesture, the inverse spiral, is drawn, then its second, the spiral.",
	    .expected =
	        "Each spiral is recognised as a power-up, level 0 then level 1, for the miracles to raise the seed "
	        "to (the band and the announcer's voice). A circle drawn with the Action button held would size the storm instead.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::SummonSeed,
	                  .delaySeconds = k_DrawSeconds,
	                  .value = static_cast<size_t>(SpellSeedType::Storm)},
	                 Draw(GestureType::InverseSpiral, 1.0f),
	                 Draw(GestureType::Spiral)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "miracles.gesture_scribble_cancel",
	    .name = "Cancel a held miracle with a scribble",
	    .facet = Facet::Miracles,
	    .description = "A fireball seed is put in the hand and a scribble is drawn: quick strokes from side to side.",
	    .expected = "The scribble is recognised; the miracles drop the seed back where it came from, refunding its "
	                "prayer power (or call off a power-up asked for first).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    // The next seed waits until the scribble has been drawn to its end
	    .commands = {AfterDrawing(HoldSeed(SpellSeedType::Fire, 1.0f)), Draw(GestureType::Scribble, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "miracles.repeat_after_spent_lightning",
	    .name = "Repeat a lightning that ran out in the hand",
	    .facet = Facet::Miracles,
	    .description = "A lightning seed is put in the hand over the land and its bolt held with the Action button for "
	                   "eight seconds, past the six or so the miracle lasts, so that its end deletes the seed in the "
	                   "hand; then the repeat gesture, an R, is drawn with the empty hand.",
	    .expected = "The seed leaves the hand as its miracle ends, and its type becomes the last seed type (the Land "
	                "Island debug window's \"Last seed type\" gives the lightning's). The R is taken as the repeat: a "
	                "lightning seed is asked for from the player's worship and comes into the hand once charged.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 50.0f}}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = {0.0f, 35.0f},
	                  .handOffset = {0.0f, 35.0f},
	                  .handHeight = 15.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 8.0f,
	                  .byHand = true}},
	    // Drawn once the hand has let go, the seed long gone
	    .commands = {Draw(GestureType::RShape, 11.0f)},
	    // the readout: the last seed type becomes the lightning's (6) as the seed leaves the hand
	    .logLastSeedType = true,
	});

	all.push_back({
	    .id = "leash.gesture_picker",
	    .name = "The leash gesture and the leash picker",
	    .facet = Facet::Leash,
	    .description = "Your tiger is taught all three leashes, its leash is taken off, and the leash gesture (a square "
	                   "spiral) is drawn. With the picker open the learning leash's gesture (an E) is drawn; then the "
	                   "leash gesture again and the aggression leash's (a vertical scribble); then a scribble with the "
	                   "empty hand.",
	    .expected = "The square spiral puts the picked leash on and opens the picker, as the tiger knows more than one "
	                "leash. The E changes it to the learning rope and closes the picker. The square spiral opens the "
	                "picker again, the vertical scribble changes to the aggression leash, and the scribble shakes it "
	                "off. The picker closes by itself after 25 seconds.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::EShape),
	                 Draw(GestureType::SquareSpirial),
	                 Draw(GestureType::VerticalScribble),
	                 Draw(GestureType::Scribble)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.gesture_scribbles",
	    .name = "Scribbles with the leash picker open",
	    .facet = Facet::Leash,
	    .description = "Your tiger, which knows every leash, has its rope taken off. The leash gesture (a square spiral) "
	                   "is drawn, then a scribble with the empty hand, then another.",
	    .expected = "The square spiral puts the rope on and opens the picker. The first scribble only closes the picker, "
	                "as in the game, and the rope stays on; the second shakes it off.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Rope},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::Scribble),
	                 Draw(GestureType::Scribble)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "story.hidden_phone_box",
	    .name = "The square wave starts the hidden phone box",
	    .facet = Facet::Story,
	    .description = "The land counts as Land 1 of the story. Your tiger learns the aggression and compassion leashes, "
	                   "and its leash comes off. The hand draws the leash gesture (a square spiral), which opens the "
	                   "leash picker. Then the hand draws the square wave.",
	    .expected = "The square wave shows no trail, makes no sound and shows no hint. The hidden script starts. The two "
	                "advisors appear and say four lines. After 20 seconds the phone plays its first messages. The script "
	                "makes the phone box at its fixed point on Land 1, which is in the sea of the testbed. To see the "
	                "box, do the same steps on Land 1.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::SquareWave)},
	});

	// A gesture drawn inside the temple, set out for a recorded mouse and keys (tools/checks/realinput/temple_gesture.txt
	// through tools/checks/real_input_tap.sh) or a person's: a shield's seed in the hand from the start and the
	// player's temple far to the west, turned as Land 1's, with no pointer commands
	all.push_back({
	    .id = "realinput.temple_gesture",
	    .name = "A circle drawn in the temple, then on the land, by hand",
	    .facet = Facet::Miracles,
	    .description = "A shield's seed is in the player's hand from the start, over the testbed's land, and the player's "
	                   "temple stands far to the west. Nothing drives the pointer or the keys: go into the temple's main "
	                   "room (F4), draw a circle with the Action button held, leave (Escape), and draw the same circle "
	                   "on the land.",
	    .expected = "In the temple the circle is not recognised and no shield is cast: the game is paused there, so the "
	                "cursor gives the gestures no samples, and every pass of the interface wipes the gesture path (the "
	                "Gestures debug window says \"Path wiped every pass: inside the temple\"). On the land the same "
	                "circle is recognised and the shield is cast there on letting go. The log gives the miracles each "
	                "second, the last seed type and the hand's state.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .logMiraclesEvery = 1.0f,
	    .logLastSeedType = true,
	    .logHandState = true,
	    .fixtures = {.temples = {{.at = glm::vec2 {-300.0f, 100.0f}, .rotation = 36000}}, .handSeed = MagicType::Shield},
	});

	// The hidden phone box played through the real input path (tools/checks/realinput/hidden_phone_box.txt, through
	// tools/checks/real_input_tap.sh), after the glade's choice made by hand: the timeline only clicks the texts, teaches
	// the creature the leashes and places the camera over each box in turn, with no pointer commands, so the leash, the
	// two gestures and the presses on the boxes are the replayed mouse's
	all.push_back({
	    .id = "realinput.hidden_phone_box",
	    .name = "Land 1 real input: the hidden phone box",
	    .facet = Facet::Story,
	    .description = "Played after the glade's choice in a quest run (quest=SetupLand1/CreaturesInGlade on Land1.txt), "
	                   "the choice and every gesture, key and press given by the replayed input. Once the creature is "
	                   "chosen it is taught the leashes and the camera is put over the phone box's place; once the box "
	                   "has played its third message and sunk, over the first of the five boxes, and once that one has "
	                   "played a message, over the second.",
	    .expected = "The square spiral drawn with no leash on puts the leash on and opens the leash picker; the square "
	                "wave then starts the hidden script, which makes the phone box (6477, 6481, 6480, 6478), and a second "
	                "square wave only says \"Already done\". The presses on the box play its messages, then it sinks "
	                "and five boxes rise; a press on the first plays another message and one on the second ends with "
	                "\"Sorry, the boss got the cheat removed\". Held and dragged within the influence, the second box, "
	                "the one left standing, is never taken into the hand.",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            // the glade's last line, "We have chosen a creature", then the creature given to the player
	            {.kind = Kind::WaitText, .waitUntilFree = true, .value = 4110, .read = true},
	            // [shortcut] the creature knows the three leashes, as the lessons a quest run skips teach them
	            {.kind = Kind::KnowLeash, .delaySeconds = k_GivenSeconds, .leash = LeashType::Rope},
	            {.kind = Kind::KnowLeash, .leash = LeashType::Evil},
	            {.kind = Kind::KnowLeash, .leash = LeashType::Good},
	            CameraOnBox(k_PhoneBox, 1.0f),
	            {.kind = Kind::LogGlobals, .delaySeconds = 1.0f, .value = 5, .script = k_PhoneBoxScripts},
	            // the box's first three messages, each played by a task of its own
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Running),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Finished),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Running),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Finished),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Running),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Finished),
	            CameraOnBox(k_PhoneBoxLand1, k_SinkSeconds),
	            // the first of the five boxes plays a message, then the second is pressed
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Running),
	            WaitTask("PlayHiddenMessage", testbed_land_timeline::TaskWait::Finished),
	            CameraOnBox(k_PhoneBoxLand2, 0.5f),
	            // the script's end; then [shortcut] the player's influence over the boxes and the camera over the second,
	            // the one left standing, which the hand holds and drags
	            WaitTask("HiddenToLands", testbed_land_timeline::TaskWait::Running),
	            WaitTask("HiddenToLands", testbed_land_timeline::TaskWait::Finished),
	            {.kind = Kind::ScriptInfluence,
	             .delaySeconds = 1.0f,
	             .point = k_PhoneBoxLand2,
	             .radius = k_BoxesInfluenceRadius},
	            CameraOnBox(k_PhoneBoxLand2, 1.0f),
	        },
	    .logHandState = true,
	    .logHandGrab = true,
	    .onLand = true,
	});
}
