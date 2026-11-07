/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Sea as quest runs on the game's own Land 2 (docs/wiki/project/scenarios.md, "Playing a
// timeline on a land"), each played after a quest run's start (quest=SetupLand2/EnterLand2): once the arrival is
// over, the quest's script is started as the land's watcher starts it once the player holds the two towns it waits
// for, which the run does not wait for. The timeline presses the scroll at the mother's hut with the hand, lets the
// introduction play and brings the camera down to the bay; then it plays one ending. The bay is shut to the player's
// influence, so the creature would fetch the boys: the hand's takes stand in for it. The points are map coordinates,
// taken from the places the quest's script names; none has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedLand2Quests.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// The mother's hut, where the scroll is put up and the mother made, and the spot she wanders round
constexpr glm::vec2 k_Hut {2781.1104f, 3053.49f};
constexpr glm::vec2 k_SoapBox {2786.5073f, 3064.325f};
/// The five spots in the bay the boys are made at, the first being the swimming spot the script watches
constexpr std::array<glm::vec2, 5> k_SwimSpots {{
    {2964.223f, 3084.874f},
    {2968.614f, 3089.957f},
    {2969.778f, 3082.155f},
    {2962.422f, 3078.884f},
    {2960.591f, 3084.722f},
}};
/// The beach the mother is taken to, within 40 of the swimming spot
constexpr glm::vec2 k_Shore {2943.603f, 3095.944f};
/// Where each boy is put down on the cliff top to walk home: about 12 from the hut, so he is not yet home when he lands,
/// in a row through the mother's spot across the way home, 4 apart. A boy let go onto one standing where the last was put
/// down knocks him over, and a walk the script orders while he flies is not taken
constexpr std::array<glm::vec2, 5> k_WalkHomeFrom {{
    {2779.3f, 3067.9f},
    {2782.9f, 3066.1f},
    k_SoapBox,
    {2790.1f, 3062.5f},
    {2793.7f, 3060.8f},
}};
/// The mother, as the script makes her: an Indian housewife (the boys are Indian farmers it makes children)
constexpr uint32_t k_Housewife = 28;
/// The challenge's number, which its entry in the challenge log goes by
constexpr size_t k_Challenge = 9;
/// The camera over the bay: within the 20 of the swimming spot the boys' call needs, the spot on the screen
constexpr float k_BayEyeHeight = 10.0f;
constexpr float k_BayEyeBack = 14.0f;
/// The camera over the cliff top, where a boy is put down
constexpr float k_CliffEyeHeight = 30.0f;
constexpr float k_CliffEyeBack = 40.0f;
/// The introduction's last camera moves and the end of its scene, after its last line
constexpr float k_IntroductionEndSeconds = 12.0f;
/// An effect's blow that leaves a villager no life
constexpr float k_DeadlyBlow = 10.0f;
/// A son who has walked home stands by the hut: picked within this of it
constexpr float k_HomePickRadius = 8.0f;
/// A son the creature put down may be on the shore or swimming again: picked within this of his swimming spot, once
/// the other four are out of the water
constexpr float k_LastBoyPickRadius = 25.0f;
/// The player's creature put on the beach by the bay, where it can wade out to the boys, and beside the mother's spot
constexpr glm::vec2 k_CreatureOnBeach {2948.0f, 3092.0f};
constexpr glm::vec2 k_CreatureByMother {2786.5073f, 3070.325f};

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals = "BaywatchHomeCounter BaywatchSwimmingCounter BaywatchMurder";
constexpr std::string_view k_Scripts = "BaywatchMain BaywatchSwimmingBoy BaywatchSwimmingLaugh ChallengeHighlightNotify";

/// Logs the globals and scripts, some seconds after the command before it, and again every period if given
Command Log(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// A boy at a spot: the script makes him an Indian farmer and then sets his age to 11, so the scripts see a child
[[nodiscard]] ThingNear Boy(glm::vec2 at)
{
	return {.type = script::ObjectType::VillagerChild, .point = at, .radius = 3.0f};
}

/// The mother where she wanders round her spot by the hut
[[nodiscard]] ThingNear Mother()
{
	return {.type = script::ObjectType::Villager, .subtype = k_Housewife, .point = k_SoapBox, .radius = 10.0f};
}

/// [shortcut] An effect's blow at a villager that leaves him no life, through the game's own damage, where the
/// player's miracle or the creature would strike him (a script's setting of the health kills nobody the script holds)
[[nodiscard]] Command Kill(ThingNear villager, float delay = 1.0f)
{
	return {.kind = Kind::EffectHit,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = delay,
	        .amount = k_DeadlyBlow,
	        .thing = villager};
}

/// [shortcut] A thing taken into the hand at once, in place of the creature fetching it, then let go over a point of
/// the land with the camera on it: it falls from the hand there. The hand is first brought over the thing, as a hand
/// reaching for it would be: a son taken while the hand still hangs over the cliff top would be within 5 of the hut
/// as he is lifted, and so counted home before his script sees him held
void TakeAndDrop(std::vector<Command>& commands, ThingNear thing, glm::vec2 to, float eyeHeight, float eyeBack)
{
	commands.push_back(CameraOn(thing.point, k_BayEyeHeight, k_BayEyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = thing.point});
	commands.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = thing});
	commands.push_back(CameraOn(to, eyeHeight, eyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = to});
	commands.push_back({.kind = Kind::HandDrop, .delaySeconds = 1.0f});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

/// The arrival let play, the quest started as the land's watcher starts it, its scroll pressed on with the hand, the
/// introduction let play and the camera brought down over the bay, where a boy calls out
std::vector<Command> Introduction()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("BaywatchMain"),
	    // the scroll's notifier runs once the quest has made the scroll
	    WaitTask("ChallengeHighlightNotify"),
	    Log(0.0f, 30),
	    // [camera] the scroll at the mother's hut
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 0.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Hut);
	commands.push_back(WaitText(1589));
	// [camera] over the bay once the introduction's scene is over
	commands.push_back(CameraOn(k_SwimSpots.at(0), k_BayEyeHeight, k_BayEyeBack, 0.0f));
	commands.back().delaySeconds = k_IntroductionEndSeconds;
	commands.push_back(WaitText(1590));
	commands.push_back(Log(1.0f));
	return commands;
}

/// The challenge's entry tapped in the challenge log: its reminder, the good advisor's line
void Replay(std::vector<Command>& commands)
{
	commands.push_back({.kind = Kind::ReplayChallenge, .delaySeconds = 2.0f, .value = k_Challenge});
	commands.push_back(WaitText(1602));
	commands.push_back(Log(2.0f));
}

std::vector<Command> AllHome()
{
	auto commands = Introduction();
	// the first boy lifted out and let go over the water again: he swims on
	TakeAndDrop(commands, Boy(k_SwimSpots.at(0)), k_SwimSpots.at(0), k_BayEyeHeight, k_BayEyeBack);
	commands.push_back(Log(3.0f));
	// each boy lifted out and put down on the cliff top, each at his own spot by the mother's, so he walks home
	for (size_t i = 0; i < k_SwimSpots.size(); ++i)
	{
		TakeAndDrop(commands, Boy(k_SwimSpots.at(i)), k_WalkHomeFrom.at(i), k_CliffEyeHeight, k_CliffEyeBack);
	}
	// the all-home ending, the mother's thanks and the reward dispenser, waited for at once: the boys reach the hut
	// soon after the last is put down
	commands.push_back(WaitText(1593));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	Replay(commands);
	return commands;
}

std::vector<Command> Beach()
{
	auto commands = Introduction();
	// the mother taken from her spot and let go over the beach, within 40 of the swimming boys
	TakeAndDrop(commands,
	            ThingNear {.type = script::ObjectType::Villager, .subtype = k_Housewife, .point = k_SoapBox, .radius = 10.0f},
	            k_Shore, k_BayEyeHeight, k_BayEyeBack);
	// the beach ending: a boy's cry, the family put back at the hut, the mother's thanks
	commands.push_back(WaitText(1593));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

std::vector<Command> OneKilled()
{
	auto commands = Introduction();
	// [shortcut] the last boy killed in the water by an effect's blow: the advisors' two lines
	commands.push_back(Kill(Boy(k_SwimSpots.at(4))));
	commands.push_back(WaitText(1598));
	// the murder is counted once the advisors' dialogue has ended, a little after the line is read
	commands.push_back(WaitGlobal("BaywatchMurder", 1.0f));
	commands.push_back(Log(1.0f));
	// the other four lifted out and put down on the cliff top, each at his own spot, so they walk home
	for (size_t i = 0; i < 4; ++i)
	{
		TakeAndDrop(commands, Boy(k_SwimSpots.at(i)), k_WalkHomeFrom.at(i), k_CliffEyeHeight, k_CliffEyeBack);
	}
	// the ending with a son lost, waited for at once as the all-home one: the mother's line, no reward
	commands.push_back(WaitText(1594));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

/// A son fetched by the creature, one brought home and killed there, the rest brought home by the hand
std::vector<Command> HomeKilled()
{
	auto commands = Introduction();
	// [shortcut] the player's creature put on the beach, as the leash would lead it there; then it wades out, picks up
	// the last boy and puts him down where it stands, as a player would have it do
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_CreatureOnBeach});
	commands.push_back({.kind = Kind::PickUp, .delaySeconds = 1.0f, .waitUntilFree = true, .thing = Boy(k_SwimSpots.at(4))});
	// put down once its pick-up has ended, with how it ended in the log
	commands.push_back({.kind = Kind::PutDown, .delaySeconds = 2.0f, .waitUntilFree = true, .waitForObjectAction = true});
	commands.push_back(Log(2.0f));
	// the first boy lifted out and put down on the cliff top, so he walks home
	TakeAndDrop(commands, Boy(k_SwimSpots.at(0)), k_WalkHomeFrom.at(0), k_CliffEyeHeight, k_CliffEyeBack);
	commands.push_back(WaitGlobal("BaywatchHomeCounter", 1.0f));
	commands.push_back(Log(1.0f));
	// [shortcut] the son at home killed by an effect's blow: nothing watches him any more
	commands.push_back(Kill({.type = script::ObjectType::VillagerChild, .point = k_Hut, .radius = k_HomePickRadius}, 2.0f));
	commands.push_back(Log(2.0f));
	for (size_t i = 1; i < 4; ++i)
	{
		TakeAndDrop(commands, Boy(k_SwimSpots.at(i)), k_WalkHomeFrom.at(i), k_CliffEyeHeight, k_CliffEyeBack);
	}
	// the last boy lifted out too if the creature put him back in the water; on the shore he walks home by himself
	TakeAndDrop(commands,
	            {.type = script::ObjectType::VillagerChild, .point = k_SwimSpots.at(4), .radius = k_LastBoyPickRadius},
	            k_WalkHomeFrom.at(4), k_CliffEyeHeight, k_CliffEyeBack);
	// the all-home ending all the same: the dead son was counted home
	commands.push_back(WaitText(1593));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

/// [shortcut] The mother killed at her spot by an effect's blow, with the camera on the hut
void KillMother(std::vector<Command>& commands)
{
	commands.push_back(CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back(Kill(Mother()));
	commands.push_back(Log(2.0f));
}

/// The five sons killed in the water, the mother first when `motherKilled`
std::vector<Command> AllKilled(bool motherKilled)
{
	auto commands = Introduction();
	if (motherKilled)
	{
		KillMother(commands);
		commands.push_back(CameraOn(k_SwimSpots.at(0), k_BayEyeHeight, k_BayEyeBack, 0.0f));
	}
	// [shortcut] each boy killed where he swims by an effect's blow
	for (const auto& spot : k_SwimSpots)
	{
		commands.push_back(Kill(Boy(spot)));
	}
	commands.push_back(Log(2.0f));
	commands.push_back(WaitText(motherKilled ? 1599 : 1596));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

/// The mother killed, then every son brought home
std::vector<Command> Orphans()
{
	auto commands = Introduction();
	KillMother(commands);
	for (size_t i = 0; i < k_SwimSpots.size(); ++i)
	{
		TakeAndDrop(commands, Boy(k_SwimSpots.at(i)), k_WalkHomeFrom.at(i), k_CliffEyeHeight, k_CliffEyeBack);
	}
	commands.push_back(WaitText(1600));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

/// The mother killed at her spot and her body taken to the beach by the swimming sons
std::vector<Command> BeachDead()
{
	auto commands = Introduction();
	KillMother(commands);
	// her body taken and let go over the beach, within 40 of the swimming boys
	TakeAndDrop(commands, Mother(), k_Shore, k_BayEyeHeight, k_BayEyeBack);
	commands.push_back(WaitText(1601));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

/// The mother carried to the beach in the creature's hand: the beach ending waits until it lets her go
std::vector<Command> BeachByCreature()
{
	auto commands = Introduction();
	commands.push_back(CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 0.0f));
	// [shortcut] the player's creature put beside the mother, as the leash would lead it there; it picks her up, as a
	// player would have it do
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .point = k_CreatureByMother});
	commands.push_back({.kind = Kind::PickUp, .delaySeconds = 1.0f, .waitUntilFree = true, .thing = Mother()});
	// [shortcut] the creature, the mother in its hand, put on the beach by the swimming boys, as it would walk there,
	// once it has her: moved while it still reaches for her, it lets the pick-up go. Left to itself it then walks her
	// back from the beach, and a put-down given before it stands still is not carried out, so once it stands still it
	// is put on the beach again and puts her down there
	commands.push_back({.kind = Kind::MoveThing,
	                    .delaySeconds = 1.0f,
	                    .waitUntilFree = true,
	                    .point = k_CreatureOnBeach,
	                    .waitForObjectAction = true});
	commands.push_back(CameraOn(k_SwimSpots.at(0), k_BayEyeHeight, k_BayEyeBack, 0.0f));
	commands.push_back({.kind = Kind::MoveThing, .delaySeconds = 1.0f, .waitUntilFree = true, .point = k_CreatureOnBeach});
	commands.push_back({.kind = Kind::PutDown, .delaySeconds = 0.2f, .waitUntilFree = true, .waitForObjectAction = true});
	commands.push_back(Log(1.0f));
	commands.push_back(WaitText(1593));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}
/// The two Indian villages by the bay whose winning the land's watcher waits for before it starts the quest
constexpr ThingNear k_SeaTown {.type = script::ObjectType::Town, .point = {2761.49f, 3102.94f}, .radius = 30.0f};
constexpr ThingNear k_SeaTownWest {.type = script::ObjectType::Town, .point = {2196.11f, 3096.86f}, .radius = 30.0f};
/// More belief in the player than in anyone else, as winning a village gives it
constexpr float k_WinningBelief = 2.0f;

/// A village takes more belief in the player than in anyone else; its next fold hands it to the player
[[nodiscard]] Command WinVillage(const ThingNear& town)
{
	return {.kind = Kind::TownGainsBelief,
	        .player = PlayerNames::PLAYER_ONE,
	        .delaySeconds = 1.0f,
	        .amount = k_WinningBelief,
	        .thing = town};
}

/// The beach the mother is let go over by the player's own hand: 34 west of the swimming spot, on the sand above the
/// water, past the 30 of the anti-influence ring and within the 40 the beach ending needs
constexpr glm::vec2 k_RealShore {2930.0f, 3084.9f};
/// How long the camera stays on a thing for a press of the recording, and over a point for a put-down
constexpr float k_RealTakeSeconds = 8.0f;
constexpr float k_RealDropSeconds = 6.0f;

/// The camera 25 above a point and 4 to the south, looking down at it, so that a press at the screen's middle meets
/// the thing lying or standing there
[[nodiscard]] Command CameraAbove(glm::vec2 at, float delay)
{
	return {.kind = Kind::CameraTo, .delaySeconds = delay, .eye = {at.x, 25.0f, at.y - 4.0f}, .look = {at.x, 0.5f, at.y}};
}

/// The Sea played through the real input path (tools/checks/realinput/the_sea.txt, replayed through
/// tools/checks/real_input_tap.sh): no pointer commands, so the replayed mouse alone drives the hand. The timeline
/// starts the quest as the land's watcher does, clicks the texts and places the camera: over the scroll at the mother's
/// hut; over the bay once the introduction is over; above two of the boys, where the recording's presses find the bay
/// shut to the player's influence; above the mother's spot, for her take; above the beach by the swimming boys, for
/// her put-down. The two villages the watcher waits for are handed to the player first. The presses are the
/// recording's
std::vector<Command> BeachRealInput()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    // [shortcut] the two Indian villages the land's watcher waits for take more belief in the player than in anyone
	    // else, as winning them over would give it; their next fold hands them to the player, whose influence then
	    // covers the mother's hut, as it does whenever the quest starts
	    WinVillage(k_SeaTown),
	    WinVillage(k_SeaTownWest),
	    StartScript("BaywatchMain"),
	    WaitTask("ChallengeHighlightNotify"),
	    Log(0.0f, 30),
	    // [camera] the scroll at the mother's hut, for the recording's press
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 0.0f),
	    WaitText(1589),
	};
	// [camera] over the bay once the introduction's scene is over
	commands.push_back(CameraOn(k_SwimSpots.at(0), k_BayEyeHeight, k_BayEyeBack, 0.0f));
	commands.back().delaySeconds = k_IntroductionEndSeconds;
	commands.push_back(WaitText(1590));
	commands.push_back(Log(1.0f));
	// [camera] above the first and the last boy, for the recording's presses
	commands.push_back(CameraAbove(k_SwimSpots.at(0), 1.0f));
	commands.push_back(CameraAbove(k_SwimSpots.at(4), k_RealDropSeconds));
	// [camera] above the mother's spot, for her take, then above the beach, for her put-down
	commands.push_back(CameraAbove(k_SoapBox, k_RealDropSeconds));
	commands.push_back(CameraAbove(k_RealShore, k_RealTakeSeconds));
	commands.push_back(Log(k_RealDropSeconds));
	// the beach ending, if she was put down there with all five boys swimming
	commands.push_back(WaitText(1593));
	commands.push_back(WaitTask("BaywatchMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestSeaScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_sea",
	    .name = "Land 2 quest run: The Sea, every son brought home",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it starts BaywatchMain, "
	                   "as the land's watcher does; presses on the scroll at the mother's hut with the hand, lets the "
	                   "introduction play and brings the camera down over the bay. The hand then takes the first boy, "
	                   "standing in for the creature, and lets him go over the water again; then takes each boy and "
	                   "lets him go over the cliff top, each at his own spot by the mother's; it lets the ending play "
	                   "and taps the challenge's entry in the challenge log.",
	    .expected = "The anti-influence ring, the scroll; the press makes the mother and the five boys and plays the "
	                "introduction (texts 1588 and 1589, the log at 0). Over the bay a boy says text 1590. The boy let go "
	                "over the water swims on; each boy let go on the cliff says text 1591 and walks home "
	                "(BaywatchHomeCounter up to 5). The mother's text 1593, the Big dispenser by the hut and the log at "
	                "1 with alignment 1; the replay says text 1602. Unverified: that the hand can take the boys inside "
	                "the anti-influence ring.",
	    .commands = AllHome(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_beach",
	    .name = "Land 2 quest run: The Sea, the mother taken to the beach",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, then takes the mother into the hand, standing in "
	                   "for the player's grab, and lets her go over the beach by the swimming boys.",
	    .expected = "With all five boys swimming the beach ending plays: text 1592, the fade, the family put back at the "
	                "hut, text 1593 and the log at 1 with alignment 0.8, with no reward.",
	    .commands = Beach(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_one_killed",
	    .name = "Land 2 quest run: The Sea, one son killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, kills the last boy in the water by a script's "
	                   "SET_PROPERTY of his health, standing in for a blow, then brings the other four home as "
	                   "quest.the_sea does.",
	    .expected = "The death brings texts 1597 and 1598 (BaywatchMurder=1); once the four are home the ending's log "
	                "update to 1 with alignment -0.2 and the mother's text 1594, with no reward.",
	    .commands = OneKilled(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_home_killed",
	    .name = "Land 2 quest run: The Sea, a son fetched by the creature and one killed at home",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, puts the player's creature on the beach and has it "
	                   "pick up the last boy and put him down; brings the first boy home with the hand, kills him by the "
	                   "hut once home with an effect's blow (a shortcut for the player's miracle), then brings the "
	                   "others home.",
	    .expected = "The boy in the creature's hand stops swimming and, once let go, swims again or walks home by where "
	                "he lands. The son killed at home changes no count: once the rest are home the all-home ending plays "
	                "(text 1593, the log at 1 with alignment 1).",
	    .commands = HomeKilled(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_all_killed",
	    .name = "Land 2 quest run: The Sea, all five sons killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, then kills each boy where he swims with an effect's "
	                   "blow (a shortcut for the player's miracles).",
	    .expected = "Texts 1597 and 1598 at the first death; with all five dead and the mother alive, the film at the "
	                "hut, the log update to 1 with alignment -0.6, the mother's 1595 and the evil advisor's 1596, with "
	                "no reward.",
	    .commands = AllKilled(false),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_all_killed_mother_dead",
	    .name = "Land 2 quest run: The Sea, the mother and all five sons killed",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, kills the mother at her spot, then each boy where "
	                   "he swims, with effect's blows (shortcuts for the player's miracles).",
	    .expected = "No film: the evil advisor's 1599 and the log's details updated to 1 with alignment -1.",
	    .commands = AllKilled(true),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_orphans",
	    .name = "Land 2 quest run: The Sea, the mother killed and every son brought home",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, kills the mother at her spot with an effect's blow "
	                   "(a shortcut for the player's miracle), then brings each boy home as quest.the_sea does.",
	    .expected = "Once every son is home, the film at the hut, the log update to 1 with alignment -0.8 and the good "
	                "advisor's 1600, with no reward.",
	    .commands = Orphans(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_beach_mother_dead",
	    .name = "Land 2 quest run: The Sea, the dead mother taken to the beach",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, kills the mother at her spot with an effect's blow "
	                   "(a shortcut for the player's miracle), then takes her body into the hand and lets it go over the "
	                   "beach by the swimming boys.",
	    .expected = "The beach film: the woman's 1601, the fade, her body put back at her spot with the five sons "
	                "around her, the fade in and the log recorded again at 1 with alignment -0.5.",
	    .commands = BeachDead(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_sea_beach_creature",
	    .name = "Land 2 quest run: The Sea, the mother carried to the beach by the creature",
	    .facet = Facet::Story,
	    .description = "Plays the introduction as quest.the_sea does, puts the player's creature beside the mother and "
	                   "has it pick her up, puts the creature on the beach by the swimming boys and, once it has walked "
	                   "off and stood still, puts it there again and has it put her down.",
	    .expected = "While the creature holds her the beach ending waits; once she is put down by the swimming boys the "
	                "beach ending plays (1592, the family put back at the hut, 1593, the log at 1 with alignment 0.8).",
	    .commands = BeachByCreature(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.the_sea",
	    .name = "Land 2: The Sea, the mother taken to the beach by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt), with the player's "
	                   "mouse from a recording: once the arrival is over the timeline hands the two Indian villages by "
	                   "the bay to the player by belief (a shortcut for winning them) and starts BaywatchMain, as the "
	                   "land's watcher does; it clicks the texts and places the camera: over the scroll at the mother's "
	                   "hut, over the bay, above two of the boys, above the mother's spot and above the beach by the "
	                   "boys; the scroll's press, the presses on the boys, the mother's take and her put-down are the "
	                   "recording's.",
	    .expected = "The presses on the boys find the bay shut to the player's influence and take nothing; the mother "
	                "put down on the beach with all five boys swimming starts the beach ending (1592, the family put "
	                "back at the hut, 1593, the log at 1 with alignment 0.8) (tools/checks/realinput/the_sea.txt).",
	    .commands = BeachRealInput(),
	    .onLand = true,
	});
}
