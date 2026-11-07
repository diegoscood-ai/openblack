/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 2's silver scroll The Singing Stones as a quest run on the game's own Land 2 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"), played after a quest run's start (quest=SetupLand2/EnterLand2): the quest's
// script is started as the run's timeline begins, as the land's control script starts it beside the arrival. The
// timeline drives it as a player would: once the arrival is over, the scroll on the priest's hut is pressed on with the
// hand and the introduction is let play; then each tune is tapped on the stones with the hand, one stone at a time,
// and its scene is let play: Twinkle Twinkle Little Star by day, which brings the night, then White Christmas and the
// funeral march on the tombstones the night puts in the stones' place. Once the priest has said his line in the
// funeral's scene he is killed inside the ring, so the raising brings him back. The points are map coordinates, taken
// from the places the quest's script names; none has yet been seen in a running game.

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
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
/// The priest's hut, where the scroll is put up and the priest made
constexpr glm::vec2 k_Hut {2134.7f, 3010.71f};
/// The circle's centre, and the nine stones' spots, stone 1 to stone 9
constexpr glm::vec2 k_Centre {2106.779f, 2997.927f};
constexpr std::array<glm::vec2, 9> k_Stones {{
    {2108.5923f, 3010.0273f},
    {2111.929f, 3008.0898f},
    {2115.0037f, 3005.1694f},
    {2116.4893f, 3000.9282f},
    {2117.0657f, 2996.4011f},
    {2114.1128f, 2992.7598f},
    {2110.3665f, 2990.1528f},
    {2105.964f, 2989.086f},
    {2101.9155f, 2989.3716f},
}};
/// By day each stone is a mobile static of the singing stone's info; by night a feature of the tombstone's. How near its
/// spot the pick looks for it: the stones stand about 3.5 apart, each on a base at its own spot
constexpr uint32_t k_SingingStoneInfo = 5;
constexpr uint32_t k_TombstoneInfo = 75;
constexpr float k_StonePickRadius = 1.5f;
/// The priest, a villager of the priest's info, at the spot the funeral's scene walks him to inside the ring
constexpr uint32_t k_Priest = 68;
constexpr glm::vec2 k_PriestInRing {2103.684f, 2999.995f};
/// How near that spot the priest is looked for once the scene is over: he may have stepped off it, but the raising only
/// finds bodies within 10 of the ring's middle
constexpr float k_PriestPickRadius = 8.0f;
/// How near the hut the priest's first walk is read, and how many readings, a tenth of a second apart
constexpr float k_PriestWalkRadius = 8.0f;
constexpr int k_PriestWalkReadings = 40;
/// The camera over the stones: close behind and well above the circle's centre, so each stone is pointed at from above
constexpr float k_StonesEyeHeight = 30.0f;
constexpr float k_StonesEyeBack = 15.0f;
/// The challenge's number, which its entry in the challenge log goes by
constexpr size_t k_Challenge = 12;
/// Seconds between two taps
constexpr float k_TapSeconds = 1.0f;
/// The hour the three tunes' run sets before the funeral march, the hour Twinkle's scene moves the night to
constexpr float k_MarchHour = 23.0f;

/// The tunes, by the stones tapped. Twinkle Twinkle Little Star is fixed stones; White Christmas (one up, one down, one
/// down, one up, one up, one up, one up) and the funeral march (the same four times, three up, one down, the same, two
/// down, the same, one down, one up) are steps, played here from stones 5 and 2
constexpr std::array<int, 14> k_Twinkle {1, 1, 8, 8, 9, 9, 8, 6, 6, 5, 5, 3, 3, 1};
constexpr std::array<int, 8> k_WhiteChristmas {5, 6, 5, 4, 5, 6, 7, 8};
constexpr std::array<int, 11> k_Funeral {2, 2, 2, 2, 5, 4, 4, 2, 2, 1, 2};
/// For the ring's run: one stray tap before Twinkle; on the tombstones, thirteen taps after a wipe whose last three and
/// first four, read across the still-empty fourteenth slot as a stone 0, are White Christmas (1, 2, 1, 0, 1, 2, 3, 4),
/// the six between them two steps apart so they fit no tune; and Twinkle begun at its third note, so that it is read
/// across the ring's end
constexpr std::array<int, 1> k_StrayTap {4};
constexpr std::array<int, 13> k_ChristmasAroundTheGap {1, 2, 3, 4, 9, 7, 9, 7, 9, 7, 1, 2, 1};
constexpr std::array<int, 14> k_TwinkleFromItsThirdNote {8, 8, 9, 9, 8, 6, 6, 5, 5, 3, 3, 1, 1, 1};
/// A tombstone stands on its stone's base, which the press would find at the tombstone's own spot: it is aimed at this
/// share of the tombstone's height instead
constexpr float k_TombstoneAimShare = 0.75f;
/// The camera for the night's taps: lower over the circle than by day, about 0.65 rad down, as a player sees the
/// tombstones standing on their bases; from straight above, the bases' slabs cover the tombstones' feet
constexpr float k_TombstonesEyeHeight = 15.0f;
constexpr float k_TombstonesEyeBack = 20.0f;

/// The globals and scripts the quest is read by
constexpr std::string_view k_Globals =
    "TinkleComplete FuneralComplete WhiteChristmasComplete SSMidnight SSDoneBats SSBringBackToLifeActive SSTapCount";
constexpr std::string_view k_Scripts = "SingingStonesSongsMain ChallengeHighlightNotify Introduction CheckForTap "
                                       "TwinkleSequence FuneralSequence WhiteChristmasSequence BringBackTheDead";

/// Logs the globals and scripts, some seconds after the command before it, and again every period if given
Command Log(float delay, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = k_Globals, .script = k_Scripts};
}

/// A tune tapped with the hand: for each stone, the pointer on it and the Action button pressed once the hand is over
/// it, then the pointer moved to the circle's centre and the button let go there. The press taps the stone; a let go
/// while the hand does nothing remembers again whatever is under it, so a let go on the stone a turn or more after the
/// press would be read by the stones' script as a second tap of that stone
void TapTune(std::vector<Command>& commands, std::span<const int> stones, uint32_t stoneInfo)
{
	for (const int stone : stones)
	{
		const ThingNear pick {.type = stoneInfo == k_SingingStoneInfo ? script::ObjectType::MobileStatic
		                                                              : script::ObjectType::Feature,
		                      .subtype = stoneInfo,
		                      .point = k_Stones.at(static_cast<size_t>(stone - 1)),
		                      .radius = k_StonePickRadius};
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_TapSeconds, .thing = pick});
		commands.push_back(
		    {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .waitUntilFree = true, .value = k_ActionButton, .thing = pick});
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 0.3f, .point = k_Centre, .value = 2});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton});
	}
}

/// A tune tapped on the night's tombstones, each press aimed up the tombstone over its base and let go on it: a tombstone,
/// a feature, is remembered as tapped by the release over it, which the stones' script then reads
void TapTombstones(std::vector<Command>& commands, std::span<const int> stones)
{
	commands.push_back(CameraOn(k_Centre, k_TombstonesEyeHeight, k_TombstonesEyeBack, 0.0f));
	for (const int stone : stones)
	{
		const ThingNear pick {.type = script::ObjectType::Feature,
		                      .subtype = k_TombstoneInfo,
		                      .point = k_Stones.at(static_cast<size_t>(stone - 1)),
		                      .radius = k_StonePickRadius,
		                      .aimShare = k_TombstoneAimShare};
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_TapSeconds, .thing = pick});
		commands.push_back(
		    {.kind = Kind::PointerPress, .delaySeconds = 0.3f, .waitUntilFree = true, .value = k_ActionButton, .thing = pick});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton});
	}
}

/// A tune's scene let play to its end once its taps are given, the globals and scripts logged once it has begun and once
/// it is over, then the camera back over the stones
void PlayScene(std::vector<Command>& commands, std::string_view scene)
{
	commands.push_back(WaitTask(scene));
	commands.push_back(Log(1.0f));
	commands.push_back(WaitTask(scene, TaskWait::Finished));
	commands.push_back(Log(1.0f));
	commands.push_back(CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f));
}

/// The tap ring's quirks and the tunes played again: by day a stray tap, then Twinkle, whose last tap overwrites it; on
/// the tombstones White Christmas read across the empty fourteenth slot; once the day is back, Twinkle again begun at
/// its third note on the stones; then on the new night's tombstones the funeral march twice, the second while the
/// raising the first started still runs; the priest is left alive
std::vector<Command> RingAndRepeats()
{
	std::vector<Command> commands {
	    ClickTexts(),  StartScript("SingingStonesSongsMain"),        WaitTask("EnterLand2", TaskWait::Finished),
	    Log(0.0f, 30), CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 5.0f),
	};
	PressScroll(commands, k_Hut);
	commands.push_back(WaitText(6018));
	commands.push_back(WaitTask("Introduction", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	commands.push_back(CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f));
	TapTune(commands, k_StrayTap, k_SingingStoneInfo);
	TapTune(commands, k_Twinkle, k_SingingStoneInfo);
	PlayScene(commands, "TwinkleSequence");
	commands.push_back(WaitGlobal("SSMidnight", 1.0f));
	commands.push_back(Log(1.0f));
	TapTombstones(commands, k_ChristmasAroundTheGap);
	PlayScene(commands, "WhiteChristmasSequence");
	// The night lasts about a minute of play, too short for four tunes: Twinkle again is tapped on the singing stones once
	// the day has brought them back, and its scene brings a new night for the marches
	commands.push_back(WaitGlobal("SSMidnight", 0.0f));
	commands.push_back(Log(1.0f));
	TapTune(commands, k_TwinkleFromItsThirdNote, k_SingingStoneInfo);
	PlayScene(commands, "TwinkleSequence");
	commands.push_back(WaitGlobal("SSMidnight", 1.0f));
	commands.push_back(Log(1.0f));
	TapTombstones(commands, k_Funeral);
	commands.push_back(WaitTask("FuneralSequence"));
	commands.push_back(WaitText(6019));
	commands.push_back(WaitTask("FuneralSequence", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	commands.push_back(CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f));
	TapTombstones(commands, k_Funeral);
	commands.push_back(WaitTask("FuneralSequence"));
	commands.push_back(WaitText(6019));
	commands.push_back(Log(1.0f));
	commands.push_back(WaitTask("FuneralSequence", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	return commands;
}

std::vector<Command> ThreeTunes()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    StartScript("SingingStonesSongsMain"),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    Log(0.0f, 30),
	    // [camera] the scroll on the priest's hut
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 5.0f),
	};
	// [real] the scroll pressed on
	PressScroll(commands, k_Hut);
	// the introduction: the priest's walk down to the stones and his four lines, then the log at 0
	commands.push_back(WaitText(6018));
	commands.push_back(WaitTask("Introduction", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	// [camera] over the stones
	commands.push_back(CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f));
	// [real] Twinkle Twinkle Little Star tapped by day: its scene brings the night, and the stones become tombstones
	TapTune(commands, k_Twinkle, k_SingingStoneInfo);
	commands.push_back(WaitTask("TwinkleSequence"));
	commands.push_back(WaitTask("TwinkleSequence", TaskWait::Finished));
	commands.push_back(WaitGlobal("SSMidnight", 1.0f));
	commands.push_back(Log(1.0f));
	// [camera] lower over the tombstones; [real] White Christmas tapped on them, each press let go on its tombstone, as
	// the tap is remembered on the release by what the hand is over then: its scene and its snow
	TapTombstones(commands, k_WhiteChristmas);
	commands.push_back(WaitTask("WhiteChristmasSequence"));
	commands.push_back(WaitTask("WhiteChristmasSequence", TaskWait::Finished));
	commands.push_back(Log(1.0f));
	// [shortcut] the hour set back to 23:00, as a script's time of day would set it, in place of waiting for the next
	// night: White Christmas's scene ends near dawn, too late for the funeral march, whose checker reads the slots one
	// start at a time and needs some turns after the last tap; the tombstones stay, and the slots are empty since
	// White Christmas was recognised
	commands.push_back({.kind = Kind::SetHour, .delaySeconds = 1.0f, .hour = k_MarchHour});
	commands.push_back(Log(1.0f));
	// [camera] lower over the tombstones; [real] the funeral march tapped on them, each press let go on its tombstone:
	// its scene walks the priest into the ring, starts the raising and has him say his line
	TapTombstones(commands, k_Funeral);
	commands.push_back(WaitTask("FuneralSequence"));
	commands.push_back(WaitText(6019));
	commands.push_back(WaitTask("FuneralSequence", TaskWait::Finished));
	// [shortcut] the priest killed inside the ring by a script's SET_PROPERTY of his health, in place of the player's
	// blow, once the scene is over: while a script holds him in its cinema his life can't be changed, and the player's
	// hand can't reach him then either. The raising, still running, finds his body and brings him back as a skeleton;
	// his health is read back to show the blow took
	const ThingNear priest {
	    .type = script::ObjectType::Villager, .subtype = k_Priest, .point = k_PriestInRing, .radius = k_PriestPickRadius};
	commands.push_back({.kind = Kind::ScriptSetHealth, .delaySeconds = 1.0f, .amount = 0.0f, .thing = priest});
	commands.push_back({.kind = Kind::ScriptHealth, .delaySeconds = 0.1f, .thing = priest});
	commands.push_back(WaitText(6020));
	commands.push_back(Log(5.0f));
	// the challenge's entry tapped in the challenge log: its reminder, the two advisors' lines
	commands.push_back({.kind = Kind::ReplayChallenge, .delaySeconds = 2.0f, .value = k_Challenge});
	commands.push_back(WaitText(2540));
	commands.push_back(Log(2.0f));
	return commands;
}
/// The player's influence round the ring for the recording's hand, as a village the player won there would give it: the
/// hand takes and throws only within it, and a priest thrown north from the ring lands about 130 away
constexpr float k_RingInfluenceRadius = 150.0f;
/// The camera for the recording's throws: well back and up from the ring's middle, north up the screen
constexpr float k_ThrowEyeHeight = 40.0f;
constexpr float k_ThrowEyeBack = 40.0f;
/// How near the ring's middle the priest, alive or dead, is followed for the recording's takes
constexpr float k_PriestFollowRadius = 150.0f;
/// The rounds of the recording's takes and throws (the throw, then his body brought back), and how long each shot is
/// held
constexpr int k_PriestRounds = 3;
/// How long the tombstones are shown for the march played again, and then how long the run goes on
constexpr float k_MarchAgainSeconds = 40.0f;
constexpr float k_FollowSeconds = 5.0f;
constexpr float k_ThrowShotSeconds = 8.0f;
/// How often the recording's run logs the globals, to read the tap count between taps
constexpr size_t k_WatchSeconds = 2;

/// The three tunes for a recording (tools/checks/realinput/singing_stones_land_2.txt), with no pointer commands: the
/// timeline clicks the texts and places the camera, and the recording's presses do the rest. As the ring's run up to
/// the introduction's end, leaving the scroll's press to the recording; then the camera over the stones by day, lower
/// over the tombstones at night, by day again once the stones are back, at night again for the march. After the
/// march's scene, the player's influence round the ring, and in rounds the camera behind the priest and then well
/// back over the ring, for the recording to take him with the hand and throw him, and to bring his body back to the
/// ring; then the tombstones again, for the march played once more
std::vector<Command> TunesForRecording()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    StartScript("SingingStonesSongsMain"),
	    WaitTask("EnterLand2", TaskWait::Finished),
	    Log(0.0f, k_WatchSeconds),
	    // [camera] the scroll on the priest's hut, for the recording's press
	    CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 5.0f),
	    WaitTask("Introduction", TaskWait::Finished),
	    Log(1.0f),
	    // [camera] over the stones, for Twinkle's taps
	    CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f),
	    WaitTask("TwinkleSequence"),
	    WaitTask("TwinkleSequence", TaskWait::Finished),
	    WaitGlobal("SSMidnight", 1.0f),
	    Log(1.0f),
	    // [camera] lower over the tombstones, for White Christmas's taps
	    CameraOn(k_Centre, k_TombstonesEyeHeight, k_TombstonesEyeBack, 0.0f),
	    WaitTask("WhiteChristmasSequence"),
	    WaitTask("WhiteChristmasSequence", TaskWait::Finished),
	    Log(1.0f),
	    // [camera] over the tombstones again, for the taps left as the night ends
	    CameraOn(k_Centre, k_TombstonesEyeHeight, k_TombstonesEyeBack, 0.0f),
	    WaitGlobal("SSMidnight", 0.0f),
	    Log(1.0f),
	    // [camera] over the stones, for Twinkle's taps again
	    CameraOn(k_Centre, k_StonesEyeHeight, k_StonesEyeBack, 0.0f),
	    WaitTask("TwinkleSequence"),
	    WaitTask("TwinkleSequence", TaskWait::Finished),
	    WaitGlobal("SSMidnight", 1.0f),
	    Log(1.0f),
	    // [camera] over the tombstones, for the march's taps
	    CameraOn(k_Centre, k_TombstonesEyeHeight, k_TombstonesEyeBack, 0.0f),
	    WaitTask("FuneralSequence"),
	    WaitText(6019),
	    WaitTask("FuneralSequence", TaskWait::Finished),
	    Log(1.0f),
	    // [shortcut] the player's influence round the ring, as a village the player won there would give it
	    {.kind = Kind::ScriptInfluence, .delaySeconds = 0.5f, .point = k_Centre, .radius = k_RingInfluenceRadius},
	};
	const ThingNear priest {
	    .type = script::ObjectType::Villager, .subtype = k_Priest, .point = k_Centre, .radius = k_PriestFollowRadius};
	for (int round = 0; round < k_PriestRounds; ++round)
	{
		// [camera] behind the priest, alive or dead, for his take
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 2.0f, .amount = 1.0f, .thing = priest});
		commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = k_FollowSeconds, .value = 1});
		// [camera] well back over the ring, north up the screen, for his throw or his drop
		commands.push_back(CameraOn(k_Centre, k_ThrowEyeHeight, k_ThrowEyeBack, 0.0f));
		commands.push_back(Log(k_ThrowShotSeconds));
	}
	// [camera] over the tombstones, which stay while the raising runs, for the march played again
	commands.push_back(CameraOn(k_Centre, k_TombstonesEyeHeight, k_TombstonesEyeBack, 0.0f));
	commands.push_back(Log(k_MarchAgainSeconds));
	commands.push_back(Log(k_MarchAgainSeconds));
	return commands;
}

/// The priest's first walk out of the hut, read each turn: the three tunes' run up to the scroll's press, then the
/// priest's pose logged every tenth of a second for four seconds, from his making through the walk the introduction
/// starts
std::vector<Command> PriestWalk()
{
	std::vector<Command> commands {
	    ClickTexts(),  StartScript("SingingStonesSongsMain"),        WaitTask("EnterLand2", TaskWait::Finished),
	    Log(0.0f, 30), CameraOn(k_Hut, k_EyeAbove, k_EyeBack, 5.0f),
	};
	PressScroll(commands, k_Hut);
	const ThingNear priest {
	    .type = script::ObjectType::Villager, .subtype = k_Priest, .point = k_Hut, .radius = k_PriestWalkRadius};
	for (int i = 0; i < k_PriestWalkReadings; ++i)
	{
		commands.push_back({.kind = Kind::LogThingPose, .delaySeconds = 0.1f, .thing = priest});
	}
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestSingingStonesLand2Scenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.the_singing_stones_land_2",
	    .name = "Land 2 quest run: The Singing Stones, the three tunes",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt). The texts that wait "
	                   "for a click are clicked on, the arrival's too. It starts SingingStonesSongsMain as the timeline "
	                   "begins, as the land's control script does beside the arrival; once the arrival is over, presses "
	                   "on the scroll on the priest's hut with the hand and lets the introduction play; taps Twinkle "
	                   "Twinkle Little Star on the stones with the hand and lets its scene play; once the night has put "
	                   "the tombstones in their place, taps White Christmas and then the funeral march on them, "
	                   "each press aimed up the tombstone over its base and let go on it, letting "
	                   "each scene play, the hour set back to 23:00 before the march; once the priest has said his line "
	                   "in the ring and the scene is over, kills him there by a script's "
	                   "SET_PROPERTY of his health, standing in for the player's blow; then taps the "
	                   "challenge's entry in the challenge log.",
	    .expected = "The stones, their bases, the whistlers and the scroll are made; the press brings the priest and "
	                "the introduction (texts 6015 to 6018) and the log at 0. Each tap plays the stone's note and its "
	                "sparkle. Twinkle's scene makes the bats and the mist and moves the time to 23:00 (log at 0.33, "
	                "TinkleComplete=1, SSDoneBats=1); the tombstones come (SSMidnight=1). White Christmas's scene makes "
	                "the snow (log at 0.66); the funeral march's walks the priest into the ring, starts the raising "
	                "(SSBringBackToLifeActive=1) and his text 6019 (log at 0.99, then 1). His body is raised as a "
	                "skeleton and the evil advisor says text 6020; the log's entry replayed says texts 2539 and 2540. "
	                "Unverified: that the hand's taps reach the stones "
	                "without the player's influence there, and that the priest's walk ends exactly on the spot the "
	                "funeral's scene waits for.",
	    .commands = ThreeTunes(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_singing_stones_land_2_again",
	    .name = "Land 2 quest run: The Singing Stones, the ring and the tunes played again",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt), as the three tunes' "
	                   "run up to the introduction's end. Then, tapping with the hand: by day one tap on stone 4, then "
	                   "Twinkle Twinkle Little Star, whose last tap overwrites that one; on the night's tombstones, each "
	                   "press aimed up the tombstone over its base and let go on the land away from the stones, 1 2 3 4 9 "
	                   "7 9 7 9 7 1 2 1, White Christmas read across the still-empty fourteenth slot; once the day has "
	                   "brought the stones back, Twinkle again on them, begun at its third note; then on the new night's "
	                   "tombstones the funeral march twice, the second while the raising still runs. Each scene is let "
	                   "play.",
	    .expected = "Twinkle's scene, its bats and the night (log at 0.33); the tombstones (SSMidnight=1); White "
	                "Christmas's scene and snow (log at 0.66); the day and the stones back (SSMidnight=0); Twinkle's "
	                "scene again with a new mist and the night, but no bats and no update of the log; the funeral march's "
	                "scene, the raising and the priest's text 6019 (log at 0.99, "
	                "then 1); the march's scene and text 6019 again with one raising still running "
	                "(SSBringBackToLifeActive=1, one BringBackTheDead task). A night lasts about a minute of play, too "
	                "short for White Christmas, Twinkle and the march together.",
	    .commands = RingAndRepeats(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "realinput.the_singing_stones_land_2",
	    .name = "Land 2 quest run for a recording: The Singing Stones, the tunes tapped and the priest thrown by the hand",
	    .facet = Facet::Story,
	    .description = "For tools/checks/real_input_tap.sh, with no pointer commands: played after a quest run's start "
	                   "(quest=SetupLand2/EnterLand2 on Land2.txt), it starts SingingStonesSongsMain, clicks the texts "
	                   "and only places the camera: on the scroll on the priest's hut, over the stones once the "
	                   "introduction is over, lower over the tombstones once Twinkle's night has come and again after "
	                   "White Christmas's scene, over the stones once the day is back, over the tombstones once the next "
	                   "night has come; after the march's scene it puts the player's influence round the ring (a "
	                   "shortcut for a village won there) and, three rounds, follows the priest for 5 seconds "
	                   "and puts the camera well back over the ring; then the tombstones again. The recording presses "
	                   "the scroll, taps the tunes, throws the priest, brings his body back to the ring and taps the "
	                   "march again.",
	    .expected = "The recording's press starts the introduction; its taps play Twinkle (the night), White Christmas "
	                "(the snow), Twinkle again once the day is back, and the funeral march on the next night (the "
	                "raising and text 6019). Unverified: that the hand takes the priest while the script holds him, "
	                "and that a throw kills him.",
	    .commands = TunesForRecording(),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.the_singing_stones_land_2_priest_walk",
	    .name = "Land 2 quest run: The Singing Stones, the priest's first walk read each turn",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand2/EnterLand2 on Land2.txt), as the three tunes' "
	                   "run up to the press on the scroll. Then the priest the press makes is read every tenth of a "
	                   "second for four seconds: where he stands and how far he moved this turn, the angle he faces, the "
	                   "angles he is drawn at and the clip he plays with its time and speed.",
	    .expected = "The priest made at the hut, made a high detail villager, and walking slowly out of it: his y angle "
	                "and drawn yaws turning to the way he walks, and a walking clip. Unverified: that he never faces "
	                "against the way he walks.",
	    .commands = PriestWalk(),
	    .onLand = true,
	});
}
