/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's village event The Spiritual Healer as quest runs on the game's own Land 2 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"), each played after a quest run's start (quest=SetupLand2/EnterLand2): once the
// arrival is over, the event's script is started as the land's watcher starts it once the player holds the Greek
// village it waits for, which the run does not wait for. The healer's first look at the village's age makes its
// children grown up (an age of 16 is past a villager's grown-up age), so in one run two of its villagers are made
// children again, where the village would have new ones born, and the camera watches the village while the healer
// takes them; then that run picks him up and puts him down by the village, then twice far away, banishing him; another
// kills him; a third throws him within the village, then puts him down twice far away with the camera far from him.
// The hand's takes stand in for the player's grab outside the player's influence. The points are map coordinates,
// taken from the places the event's scripts name.

#include <cstddef>
#include <cstdint>

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
/// The healer's spot by the village centre, which his rounds go about
constexpr glm::vec2 k_Village {2017.1619f, 2594.753f};
/// A spot of the land far from the village, more than 200 from it: a did-you-know sign's, east of it
constexpr glm::vec2 k_FarAway {2739.382f, 3052.035f};
/// A spot of the village 25 from where he is taken, towards one of its fields, where the healer is tossed: a short
/// throw he lands from alive (a longer one, to the field 65 away, killed him)
constexpr glm::vec2 k_Field {2001.0f, 2575.7f};
/// The healer, a villager of the priest's info, found near a point: anywhere on his rounds, or where he was put down
constexpr uint32_t k_Priest = 68;
constexpr float k_HealerPickRadius = 80.0f;
/// Landed far away, he goes back to the walk his rounds last gave him, towards the village: half a minute later he is
/// found anywhere on the way
constexpr float k_HealerOnHisWayRadius = 1000.0f;
[[nodiscard]] constexpr ThingNear HealerNear(glm::vec2 point, float radius = k_HealerPickRadius)
{
	return {.type = script::ObjectType::Villager, .subtype = k_Priest, .point = point, .radius = radius};
}
constexpr ThingNear k_HealerPick = HealerNear(k_Village);
/// A grown villager of the village, a Greek housewife (none of another village lives within this reach of it)
constexpr uint32_t k_GreekHousewife = 42;
constexpr ThingNear k_GrownVillager {
    .type = script::ObjectType::Villager, .subtype = k_GreekHousewife, .point = k_Village, .radius = 150.0f};
/// The age a script gives a child it makes, below a villager's grown-up age
constexpr float k_ChildAge = 10.0f;
/// How long after the healer's first look at the village's age its two villagers are made children
constexpr float k_ChildrenAfterSeconds = 5.0f;
/// The camera on the village: well back and up, within the 100 and the 150 the healer's lines need, and him on screen
constexpr float k_EyeHeight = 40.0f;
constexpr float k_EyeBack = 60.0f;
/// The camera on the far spot, where the healer lands
constexpr float k_FarEyeHeight = 30.0f;
constexpr float k_FarEyeBack = 40.0f;
/// The camera on the far spot from well away, more than the 100 the end's scene needs: the spot on screen, the healer
/// not near
constexpr float k_UnseenEyeHeight = 100.0f;
constexpr float k_UnseenEyeBack = 120.0f;
/// How long he is held: his being held is seen at once, the next turn
constexpr float k_HoldSeconds = 5.0f;
/// How long after a landing he is taken again, once its lines are said
constexpr float k_AfterLandingSeconds = 5.0f;
/// How long after his first banishment he is taken again: past the half minute the event waits before it looks for
/// its next child, from where he is
constexpr float k_AfterFirstBanishmentSeconds = 35.0f;
/// The player's influence round the village, which is the player's when the event starts, so that the hand may let
/// go of him there
constexpr float k_VillageInfluenceRadius = 100.0f;
/// The four villagers made at his house three minutes after the end
constexpr float k_NewVillagersSeconds = 185.0f;

/// The globals and scripts the event is read by
constexpr std::string_view k_Globals = "SaidSpiritualHealerChildKillCount";
constexpr std::string_view k_Scripts = "SpiritualHealerMain Walkabout TownAge KillChild HealerKilledOrBanished "
                                       "FirstTimeBanished LandedSafeInTown";

/// Logs the globals and scripts, some seconds after the command before it, and again every period if given
Command Log(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// The arrival let play, the event started as the land's watcher starts it, and the camera on the village while the
/// healer takes his first children
std::vector<Command> Beginning()
{
	return {
	    ClickTexts(),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    StartScript("SpiritualHealerMain"),
	    Log(0.0f, 30),
	    // [camera] the village
	    CameraOn(k_Village, k_EyeHeight, k_EyeBack, 0.0f),
	};
}

/// [shortcut] The healer, found near a point, taken into the hand at once, in place of the player's grab, held, then
/// let go over a point of the land with the camera on it: he falls from the hand there. His landing's lines are said
/// the turn he lands, before a wait for their script could start, so the next take comes a few seconds later
void TakeHealerTo(std::vector<Command>& commands, glm::vec2 from, glm::vec2 to, float eyeHeight, float eyeBack,
                  float fromRadius = k_HealerPickRadius)
{
	commands.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = HealerNear(from, fromRadius)});
	commands.push_back(CameraOn(to, eyeHeight, eyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = to});
	commands.push_back({.kind = Kind::HandDrop, .delaySeconds = k_HoldSeconds});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
	commands.push_back(Log(k_AfterLandingSeconds));
}

std::vector<Command> Banished()
{
	auto commands = Beginning();
	// [shortcut] two grown villagers of the village made children again by a script's SET_PROPERTY of their age, just
	// after the healer's first look at the village's age, as children born there would be until his next
	commands.push_back(
	    {.kind = Kind::ScriptSetAge, .delaySeconds = k_ChildrenAfterSeconds, .amount = k_ChildAge, .thing = k_GrownVillager});
	commands.push_back({.kind = Kind::ScriptSetAge, .delaySeconds = 0.5f, .amount = k_ChildAge, .thing = k_GrownVillager});
	// he takes both, half a minute apart, with the advisors' lines; once he is done with the second he is picked up
	commands.push_back(WaitGlobal("SaidSpiritualHealerChildKillCount", 2.0f));
	commands.push_back(WaitTask("KillChild", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	// put down by the village: he lands safe and his rounds start again
	TakeHealerTo(commands, k_Village, k_Village, k_EyeHeight, k_EyeBack);
	// put down far away: his first warning; his rounds are not started again, so he stays there and, half a minute
	// later, looks for the village's next child from there
	TakeHealerTo(commands, k_Village, k_FarAway, k_FarEyeHeight, k_FarEyeBack);
	commands.push_back(Log(k_AfterFirstBanishmentSeconds));
	// put down far away again: banished for good, with the camera on him, and the village dies
	TakeHealerTo(commands, k_FarAway, k_FarAway, k_FarEyeHeight, k_FarEyeBack, k_HealerOnHisWayRadius);
	commands.push_back(WaitText(1720));
	commands.push_back(WaitTask("HealerKilledOrBanished", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	// [camera] kept on him as he lives on as a villager, where his age-keeping still calls out every three minutes
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 0.5f, .amount = 1.0f, .thing = HealerNear(k_FarAway)});
	// three minutes later four villagers come to his house
	commands.push_back(WaitTask("SpiritualHealerMain", TaskWait::Finished));
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	commands.push_back(Log(2.0f));
	return commands;
}

/// Thrown within the village, then put down twice far away with the camera far from him, so that his banishment has
/// no scene
std::vector<Command> Unseen()
{
	auto commands = Beginning();
	// [shortcut] taken into the hand at once, in place of the player's grab; then, with the camera on the village's
	// fields, the pointer over his spot and [real] him tossed a short way through the mouse, as the player throws
	// [shortcut] the player's influence round the village, as owning it gives, so that the hand may throw him there
	commands.push_back(
	    {.kind = Kind::ScriptInfluence, .delaySeconds = 9.0f, .point = k_Village, .radius = k_VillageInfluenceRadius});
	commands.push_back({.kind = Kind::HandTake, .delaySeconds = 1.0f, .thing = k_HealerPick});
	commands.push_back(CameraOn(k_Field, k_EyeHeight, k_EyeBack, 0.0f));
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_Village});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = k_Field});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 3.0f, .value = 1});
	commands.push_back(Log(k_AfterLandingSeconds));
	// put down far away twice, the camera well away from him: his first warning, then his banishment with no scene
	TakeHealerTo(commands, k_Field, k_FarAway, k_UnseenEyeHeight, k_UnseenEyeBack);
	commands.push_back(Log(k_AfterFirstBanishmentSeconds));
	TakeHealerTo(commands, k_FarAway, k_FarAway, k_UnseenEyeHeight, k_UnseenEyeBack, k_HealerOnHisWayRadius);
	// three minutes later four villagers come to his house
	commands.push_back(WaitTask("SpiritualHealerMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}

std::vector<Command> Killed()
{
	auto commands = Beginning();
	// [shortcut] the healer killed by a script's SET_PROPERTY of his health, in place of the player's blow, before he
	// takes his first child
	commands.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 15.0f, .amount = 0.0f, .thing = k_HealerPick});
	commands.push_back(WaitText(1720));
	commands.push_back(WaitTask("HealerKilledOrBanished", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	commands.push_back(WaitTask("SpiritualHealerMain", TaskWait::Finished));
	commands.push_back(Log(2.0f));
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestSpiritualHealerScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_spiritual_healer",
	    .name = "Land 2 quest run: The Spiritual Healer, banished",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. Once the arrival is over it starts "
	                   "SpiritualHealerMain, as the land's watcher does, makes two of the village's housewives children "
	                   "by a script's SET_PROPERTY of their age, standing in for children born there, and watches the "
	                   "village while the healer takes them; then takes the healer into the hand, standing in for the "
	                   "player's grab, holds him and lets him go by the village, then twice far away from it, follows "
	                   "him with the camera and waits for the four villagers.",
	    .expected = "The healer is made at his house and walks his rounds, calling out text 1722 near the camera; every "
	                "half minute he takes a child of the village, the first two with the advisors' texts 1726 and 1727 "
	                "and his 1721. Held, he says 1718; let go by the village, 1723 and his rounds again; far away, "
	                "1724; far away again the banishment scene (1719, the fade, the village's villagers dead, 1720). "
	                "His age-keeping goes on, calling out 1722 by him. Three minutes later four villagers are made at "
	                "his house. Unverified: that the hand can take him outside the player's influence.",
	    .commands = Banished(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_spiritual_healer_killed",
	    .name = "Land 2 quest run: The Spiritual Healer, killed",
	    .facet = Facet::Story,
	    .description = "Plays the beginning as quest.the_spiritual_healer does, then kills the healer by a script's "
	                   "SET_PROPERTY of his health, standing in for the player's blow, before he takes a child.",
	    .expected = "The killed scene: text 1725, the fade, the village's villagers dead, texts 1723 and 1720; three "
	                "minutes later four villagers are made at his house.",
	    .commands = Killed(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_spiritual_healer_unseen",
	    .name = "Land 2 quest run: The Spiritual Healer, thrown, then banished unseen",
	    .facet = Facet::Story,
	    .description = "Plays the beginning as quest.the_spiritual_healer does, then gives the player influence round "
	                   "the village (a script's INFLUENCE_POSITION, as owning it gives), takes the healer into the hand, "
	                   "standing in for the player's grab, and tosses him a short way within the village through the "
	                   "mouse; then lets him go twice far away from the village with the camera well away from him, 35 "
	                   "seconds apart, and waits for the four villagers.",
	    .expected = "Thrown, the camera follows him through the air until he lands; landed in the village, text 1723 and "
	                "his rounds again; far away, 1724, and half a minute later he looks for the village's next child; far away "
	                "again, with the camera not near him, no scene: the "
	                "village's villagers die at once and he joins no other village. Three minutes later four villagers "
	                "are made at his house. Unverified: that the throw leaves him alive.",
	    .commands = Unseen(),
	    .onLand = true,
	});
}
