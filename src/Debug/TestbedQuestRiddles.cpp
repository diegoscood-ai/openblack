/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Riddles as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"): after the arrival (quest=SetupLand2/EnterLand2), the Indian village with town id 13 is handed to
// the player, as winning it would, and the quest's script is started as the land's control script starts it once its
// check has seen the village won. The scroll at Annika's hut is pressed on with the hand and her riddle's texts are
// clicked on; then the riddle's four things are put in the ring of toadstools at once: a creature's dropping and a wolf
// by the scripts' own create and set position, where the creature and the hand would bring them, a shield miracle drawn
// over the ring with the hand, and a fireball thrown into it with the hand. The hour is set to night and day around it
// for the night wolves. Then the creature swap the zebra offers is played with the hand's presses and clicks, the
// player's creature put by the zebra; and a run of its own hurts Annika's hut first. The points are map coordinates,
// taken from the places the quest's script names and the wolves the land's map script makes; none has yet been seen in a
// running game.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
using testbed_land_timeline::ScriptArgument;

/// The second Indian village, where the land's map script makes it
constexpr glm::vec2 k_Village {2771.34f, 2444.15f};
constexpr ThingNear k_VillageThing {.type = script::ObjectType::Town, .point = k_Village, .radius = 30.0f};
/// The belief the village is given, more than anyone holds in it
constexpr float k_WinningBelief = 2.0f;
/// Annika's hut, where the scroll is put up
constexpr glm::vec2 k_Hut {2645.4934f, 2477.0095f};
/// The centre of the ring of toadstools, and a spot inside the ring a little off it where the fireball is thrown
constexpr glm::vec2 k_Ring {2630.444f, 2524.9048f};
constexpr glm::vec2 k_FireSpot {2633.444f, 2527.9048f};
/// A wolf of the pack the land's map script makes east of the ring
constexpr ThingNear k_WolfThing {
    .type = script::ObjectType::Animal, .subtype = 2, .point = {2681.0f, 2392.0f}, .radius = 30.0f};
/// The scripts' type of a creature's dropping
constexpr size_t k_PooType = static_cast<size_t>(script::ObjectType::Poo);
/// Annika's hut, as the pick finds the building, and the effect's blow that leaves it below full health
constexpr ThingNear k_HutThing {.type = script::ObjectType::Abode, .point = k_Hut, .radius = 5.0f};
constexpr float k_HutHit = 0.2f;
/// The hours the night wolves come out at and go back in
constexpr float k_NightHour = 22.0f;
constexpr float k_DayHour = 10.0f;
/// Seconds given to the night pack to come out and reach the water, or to go back into the forest and be removed
constexpr float k_PackWalkSeconds = 40.0f;
/// The player's creature put 30 north of the zebra, within the 50 the swap needs; it then walks to within 15 of the
/// zebra, so after the swap the old body stands north of the ring's centre and is picked there, and the zebra, now the
/// player's, near the centre
constexpr glm::vec2 k_NearTheZebra {2630.444f, 2554.9048f};
constexpr ThingNear k_ZebraThing {.type = script::ObjectType::Creature, .point = k_Ring, .radius = 8.0f, .aimShare = 0.5f};
constexpr ThingNear k_OldBodyThing {
    .type = script::ObjectType::Creature, .point = {2630.444f, 2549.9048f}, .radius = 12.0f, .aimShare = 0.5f};
/// The swap's lines: the good advisor's question, the evil advisor's after the swap
constexpr uint32_t k_SwapQuestionLine = 2420;
constexpr uint32_t k_SwappedLine = 5105;
/// Seconds past the good advisor's first question, longer than the 110 the swap waits for a click before it lets go
constexpr float k_SwapCancelSeconds = 120.0f;
/// Seconds from a press on the swap's scroll to the click on a creature: the walk takes at most 5, the question comes
/// at once and comes again only 30 seconds on
constexpr float k_ClickAfterPressSeconds = 15.0f;
/// Seconds given to the swap back's scene and the evil advisor's line after it
constexpr float k_SwapBackSeconds = 30.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "PuzzleSolved WolfPackUnwanted";
constexpr std::string_view k_Scripts = "LostTreasureMain Riddle CheckObjects NightWolves SwapCreatures";

/// The village takes more belief in the player than in anyone else, as a gift gives it; its next fold hands it over
Command WinVillage()
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 1.0f,
	        .amount = k_WinningBelief,
	        .thing = k_VillageThing};
}

/// [shortcut] The hour of the day set, as the clock would reach it by waiting
[[nodiscard]] Command SetHourTo(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}

/// A night and the next day: the pack made in the forest, its walk down to the water, and its walk back and removal
void NightAndDay(std::vector<Command>& commands)
{
	commands.push_back(SetHourTo(k_NightHour, 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PackWalkSeconds));
	commands.push_back(SetHourTo(k_DayHour, 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PackWalkSeconds));
}

/// A creature clicked with the hand, as the player clicks it: the pointer on it, the Action button pressed once the hand
/// is over it and let go
void ClickCreature(std::vector<Command>& commands, ThingNear creature, float delay)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = delay, .thing = creature});
	commands.push_back(
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = creature});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}

/// The arrival let play, the village handed over and the quest started; the camera on Annika's hut
std::vector<Command> RiddlesStart()
{
	return {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    // [shortcut] the village takes more belief in the player than in anyone else, as winning it over would
	    // give it; its next fold hands it to the player (a direct change of hands is undone by that fold)
	    WinVillage(),
	    // the quest's script, started as the land's control script starts it once its check sees the village won
	    StartScript("LostTreasureMain"),
	    WaitTask("ChallengeHighlightNotify"),
	    {.kind = Kind::LogGlobals, .delaySeconds = 1.0f, .value = 60, .global = k_Globals, .script = k_Scripts},
	    // [camera] the scroll over Annika's hut
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 5.0f),
	};
}

/// The swap the zebra offers: the player's creature put near it and its scroll pressed; the good advisor's question
/// let run out once; the new scroll pressed and the zebra clicked; then the scroll over the old body pressed and the old
/// body clicked, which swaps back. A repeated line can't be waited for, as it has been shown already: each click comes
/// a fixed time after its press, once the creatures have met and the question has come
void Swaps(std::vector<Command>& commands)
{
	// [shortcut] the player's creature put near the zebra, as the leash would lead it there
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 2.0f, .point = k_NearTheZebra});
	// [real] the zebra's scroll pressed: the creature walks up to the zebra and the good advisor asks; no click, so the
	// offer lets go after its wait
	PressScroll(commands, k_Ring);
	commands.push_back(WaitText(k_SwapQuestionLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_SwapCancelSeconds));
	// [real] the new scroll pressed, and the zebra clicked once the good advisor asks again: the swap
	PressScroll(commands, k_Ring);
	ClickCreature(commands, k_ZebraThing, k_ClickAfterPressSeconds);
	commands.push_back(WaitText(k_SwappedLine));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	// [real] the scroll over the old body pressed, and the old body clicked once the good advisor asks: swapped back
	PressScroll(commands, k_OldBodyThing.point);
	ClickCreature(commands, k_OldBodyThing, k_ClickAfterPressSeconds);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_SwapBackSeconds));
}

std::vector<Command> Riddles()
{
	auto commands = RiddlesStart();
	// [real] the scroll pressed on
	PressScroll(commands, k_Hut);
	// the riddle: Annika's lines and clues, and the good advisor's line after it
	commands.push_back(WaitText(1628));
	commands.push_back(WaitText(1623));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	// [camera] the ring of toadstools, from above, with the forest the night pack comes from behind the camera
	commands.push_back(CameraOn(k_Ring, 40.0f, 30.0f, 0.0f));
	// [shortcut] a night and a day, before the riddle is solved
	NightAndDay(commands);
	// [camera] the ring of toadstools, from above
	commands.push_back(CameraOn(k_Ring, 40.0f, 30.0f, 0.0f));
	// [shortcut] a creature's dropping made in the ring by a script's create, where the creature would leave one
	commands.push_back({.kind = Kind::ScriptCreate, .delaySeconds = 1.0f, .point = k_Ring, .value = k_PooType});
	// [real] a shield miracle given to the hand, as if from a bubble, and drawn as a circle over the ring
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Shield)});
	commands.push_back(
	    {.kind = Kind::DrawGesture, .delaySeconds = 1.0f, .point = k_Ring, .value = static_cast<size_t>(GestureType::Circle)});
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 3.0f));
	// a fire miracle given to the hand, as if from a bubble
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Fire)});
	// [shortcut] a wolf put into the ring, where the hand would carry one
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_Ring, .thing = k_WolfThing});
	// [real] the fireball thrown into the ring at once, before the wolf walks off
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 0.3f, .point = k_FireSpot});
	// the riddle solved: the zebra's arrival and the advisors' lines on it
	commands.push_back(WaitText(1626));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	// [shortcut] a night, a day and another night after the riddle is solved: the pack still comes
	NightAndDay(commands);
	commands.push_back(SetHourTo(k_NightHour, 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_PackWalkSeconds));
	commands.push_back(SetHourTo(k_DayHour, 1.0f));
	Swaps(commands);
	return commands;
}

/// Annika's hut hurt before its scroll is pressed: she won't tell the riddle
std::vector<Command> DamagedHut()
{
	auto commands = RiddlesStart();
	// [shortcut] the hut hurt by an effect's blow, as a player's miracle would hurt it
	commands.push_back({.kind = Kind::EffectHit,
	                    .player = PlayerNames::PLAYER_ONE,
	                    .delaySeconds = 2.0f,
	                    .amount = k_HutHit,
	                    .thing = k_HutThing});
	// [real] the scroll pressed on
	PressScroll(commands, k_Hut);
	// the evil advisor's line, and Annika's walk home
	commands.push_back(WaitText(1633));
	commands.push_back(WaitTask("Riddle", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
	return commands;
}
/// Where the real input path's dropping is made: 15 from the ring's centre towards Annika's hut, outside the 10 the
/// riddle looks within, so that only the hand's put-down brings it in
constexpr glm::vec2 k_RealPooSpot {2635.0f, 2510.7f};
/// Where that dropping comes to rest, as the probe runs found it (the run is the same each time up to the takes)
constexpr glm::vec2 k_RealPooRest {2633.34f, 2512.10f};
/// The water's edge the night pack walks down to and wanders at, as the quest's script marks it, and a wolf of that pack
/// there: the wolves the land's map script puts east of the ring stand inside a forest, where a press meets the forest
constexpr glm::vec2 k_WaterEdge {2619.457f, 2336.141f};
constexpr ThingNear k_NightWolf {.type = script::ObjectType::Animal, .subtype = 2, .point = k_WaterEdge, .radius = 40.0f};
/// How long the camera stays on the water's edge for a press on the land there, which reads the player's influence
constexpr float k_RealEdgeSeconds = 4.0f;
/// How long the camera stays on a thing for a take of the recording, and over the ring for a put-down
constexpr float k_RealTakeSeconds = 8.0f;
constexpr float k_RealDropSeconds = 6.0f;

/// The camera 25 above a point and 4 to the south, looking down at it, so that a press at the screen's middle meets
/// the thing lying or standing there
[[nodiscard]] Command CameraAbove(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 25.0f, at.y - 4.0f}, .look = {at.x, 0.5f, at.y}};
}

/// The Riddles' wolf and dropping brought by the real input path (tools/checks/realinput/the_riddles.txt, replayed
/// through tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The
/// timeline starts the quest as quest.the_riddles does, clicks the texts and places the camera: over the scroll at
/// Annika's hut; above the dropping for its take and above the ring for its put-down; once the hour is set to night,
/// above the water's edge the night pack walks to, behind a wolf of it for its take, and above the ring again for its
/// put-down. The presses are the recording's
std::vector<Command> RiddlesRealInput()
{
	auto commands = RiddlesStart();
	// the riddle: Annika's lines and clues, and the good advisor's line after it
	commands.push_back(WaitText(1628));
	commands.push_back(WaitText(1623));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 2.0f));
	// [shortcut] a creature's dropping made by a script's create near the ring, where the creature would leave one
	commands.push_back({.kind = Kind::ScriptCreate, .delaySeconds = 1.0f, .point = k_RealPooSpot, .value = k_PooType});
	// [camera] above the dropping for its take, then above the ring for its put-down
	commands.push_back(CameraAbove(k_RealPooRest, 1.0f));
	commands.push_back(CameraAbove(k_Ring, k_RealTakeSeconds));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RealDropSeconds));
	// [shortcut] the hour set to night, as the clock would reach it by waiting: the night pack comes out of the forest
	// and walks down to the water's edge
	commands.push_back(SetHourTo(k_NightHour, 0.0f));
	// [camera] above the water's edge once the pack has had time to get there, for a press on the land; then behind a
	// wolf of the pack for its take, and above the ring for its put-down
	commands.push_back(CameraAbove(k_WaterEdge, k_PackWalkSeconds));
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_RealEdgeSeconds, .amount = 1.0f, .thing = k_NightWolf});
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_RealTakeSeconds, .value = 1});
	commands.push_back(CameraAbove(k_Ring, 0.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, k_RealDropSeconds));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 10.0f));
	return commands;
}
} // namespace

void testbed_scenarios::land2_quests::AddRiddlesScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_riddles",
	    .name = "Land 2 quest run: The Riddles, the riddle solved",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it hands the Indian "
	                   "village with town id 13 to the player and starts the quest's script, as the land's control "
	                   "script does after its check; presses on the scroll at Annika's hut with the hand and lets the "
	                   "riddle play. Then it makes a creature's dropping in the ring of toadstools by a script's create, "
	                   "draws a shield miracle there with the hand, puts a wolf into the ring by a script's set position "
	                   "and throws a fireball into it with the hand. Around the riddle the hour is set to night and back "
	                   "to day, before it twice and after it once more. Once the zebra has come, it puts the "
	                   "player's creature 30 north of it and presses on its scroll; lets the swap's question run out, "
	                   "presses the new scroll and clicks the zebra; then presses the scroll over the old body and "
	                   "clicks the old body.",
	    .expected = "The eight toadstools and the scroll are made; the press plays Annika's scene (1622, 1645 to 1649, "
	                "1628) with the log at 0 and the good advisor's 1623. With the wolf, the dropping, the shield and "
	                "the fire in the ring at once the riddle is solved: the wolf and the dropping fade, the log at 1, "
	                "the zebra made and waving, and the advisors' 1623 to 1627. Each night five wolves come out of "
	                "the forest and go down to the water, each day they go back and are removed, before and after the "
	                "solve. The zebra's scroll draws 5103 with the camera near; pressed with the player's creature "
	                "near, the creature walks up, the two look each other over and the good "
	                "advisor asks (2420); unanswered, the offer lets go and a new scroll comes; the zebra clicked swaps "
	                "the creatures in a two-creature scene (5105), and the old body's scroll and click swap them back. "
	                "Unverified: that a thrown "
	                "fireball leaves a fire in the ring while the wolf is still there, and that the drawn shield stands "
	                "within 5 of the ring's centre.",
	    .commands = Riddles(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_riddles_damaged_hut",
	    .name = "Land 2 quest run: The Riddles, Annika's hut damaged",
	    .facet = Facet::Story,
	    .description = "As quest.the_riddles up to the scroll, but first hurts Annika's hut with an effect's blow (a "
	                   "shortcut for the player's miracle), then presses on the scroll with the hand.",
	    .expected = "With the hut below full health the scene plays the evil advisor's 1633 in place of the riddle, and "
	                "Annika walks back into her hut and fades away; the good advisor's 1623 follows.",
	    .commands = DamagedHut(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.the_riddles",
	    .name = "Land 2: The Riddles, the wolf and the dropping brought by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt), with the player's "
	                   "mouse from a recording: the timeline hands the village over and starts the quest as "
	                   "quest.the_riddles does, makes a creature's dropping 15 from the ring by a script's create, "
	                   "clicks the texts and places the camera: over the scroll at Annika's hut, above the dropping, "
	                   "above the ring; sets the hour to night (a shortcut for the clock) and places the camera above "
	                   "the water's edge, behind a wolf of the night pack there and above the ring again; the scroll's "
	                   "press and each take and put-down are the recording's.",
	    .expected = "The dropping and the wolf put down inside the ring: the riddle's watch finds the wolf within 10 of "
	                "the centre, then the dropping (tools/checks/realinput/the_riddles.txt).",
	    .commands = RiddlesRealInput(),
	    .onLand = true,
	});
}
