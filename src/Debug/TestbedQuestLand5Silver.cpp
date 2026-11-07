/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 5's silver scrolls as quest runs, each played after a land entry's quest= has started it: Swap to Brown Bear,
// The Explorers Again, The Japanese Traitor, The Heavenly Fire, The Magic Dragon and Stanley the Wolf. The timelines do
// what the player would: they click the texts that wait for a click, press the scrolls with the hand, pick up the dung,
// set the hour for the stranger's evenings, keep the camera over the village the meteors are aimed at, heal the
// crusaders, light their pyre and look at the cave's blowholes. The points are map coordinates, taken from the places the
// challenge scripts name.

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
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
/// Seconds between the logs of a quest's state while it plays, so that a wait it never gets past shows where
constexpr size_t k_LogPeriodSeconds = 30;

/// Logs the globals and scripts named every period while the timeline goes on
[[nodiscard]] Command LogEvery(std::string_view globals, std::string_view scripts)
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = globals, .script = scripts};
}

/// [shortcut] The hour of the day set, as the clock would reach it by waiting
[[nodiscard]] Command SetHourTo(float hour, float delay)
{
	return {.kind = Kind::SetHour, .delaySeconds = delay, .hour = hour};
}

// --- The creature swap the quests offer ---

/// The player's creature, picked within this of where the land's start puts it (no other creature stands that near)
constexpr glm::vec2 k_MyCreatureStart {1298.227f, 3575.326f};
constexpr float k_MyCreaturePickRadius = 150.0f;
/// The evil advisor's lines when an offered creature's scroll is seen and when it is pressed with the player's creature
/// too far, the good advisor's question once the creature is by it, and the evil advisor's line after the swap
constexpr uint32_t k_OfferSeenLine = 5103;
constexpr uint32_t k_CreatureTooFarLine = 5104;
constexpr uint32_t k_SwapQuestionLine = 2420;
constexpr uint32_t k_SwappedLine = 5105;

/// The swap a quest offers at a creature: its scroll pressed first with the player's creature far away, then with it
/// put near the offered creature; when the good advisor asks, the offered creature pressed on confirms
void SwapAt(std::vector<Command>& commands, glm::vec2 offered, glm::vec2 nearIt)
{
	// the offered creature picked where it stands, pressed on at the middle of its height
	const ThingNear offeredThing {.type = script::ObjectType::Creature, .point = offered, .radius = 6.0f, .aimShare = 0.5f};
	commands.push_back(WaitTask("SwapCreatures"));
	commands.push_back(CameraOn(offered, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back(WaitText(k_OfferSeenLine));
	PressScroll(commands, offered);
	commands.push_back(WaitText(k_CreatureTooFarLine));
	// [shortcut] the player's creature put near the offered one, as the leash would lead it there
	commands.push_back(
	    {.kind = Kind::MoveThing,
	     .delaySeconds = 2.0f,
	     .point = nearIt,
	     .thing =
	         ThingNear {.type = script::ObjectType::Creature, .point = k_MyCreatureStart, .radius = k_MyCreaturePickRadius}});
	PressScroll(commands, offered);
	commands.push_back(WaitText(k_SwapQuestionLine));
	// [real] the pointer on the offered creature, the Action button pressed once the hand is over it and let go
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = offeredThing});
	commands.push_back({.kind = Kind::PointerPress,
	                    .delaySeconds = 0.5f,
	                    .waitUntilFree = true,
	                    .value = k_ActionButton,
	                    .thing = offeredThing});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
	commands.push_back(WaitText(k_SwappedLine));
}

// --- Swap to Brown Bear ---

/// The farmer's house, under the scroll, and how high the scroll stands over the land there
constexpr glm::vec2 k_FarmerHouse {2359.32f, 3390.025f};
constexpr float k_HouseRaised = 4.0f;
/// The eight piles of dung, each made once the one before is picked up
constexpr std::array<glm::vec2, 8> k_DungPiles {{
    {2254.907f, 3386.621f},
    {2226.376f, 3404.887f},
    {2240.606f, 3434.66f},
    {2272.533f, 3463.427f},
    {2243.217f, 3454.875f},
    {2213.621f, 3489.385f},
    {2198.929f, 3518.456f},
    {2217.4f, 3533.416f},
}};
/// A pile rolls a little once made on a slope; only one pile lies at a time, so a wide pick finds the one there is
constexpr float k_DungPickRadius = 40.0f;
/// The advisors' last line after the introduction, once the trail can be followed
constexpr uint32_t k_TrailStartsLine = 1880;
/// The evil advisor's line after the seventh pile, said before the eighth is made
constexpr uint32_t k_OneMorePileLine = 1886;
constexpr size_t k_LastPileRetries = 3;
/// Seconds the timeline keeps logging once the quest's script has ended, for the swap offer's first reminder
constexpr float k_SwapOfferSeconds = 70.0f;
constexpr std::string_view k_BearScripts = "SwapToBrownBear ChallengeHighlightNotify HelpSpritesTalk HelpSpritesTalk2 "
                                           "SwapCreatures";

/// The brown bear the quest gives, where it stands once the swap is offered, and the player's creature put 30 north of
/// it for the swap, within the 50 the swap needs
constexpr glm::vec2 k_BrownBear {2239.368f, 3574.824f};
constexpr glm::vec2 k_NearTheBrownBear {2239.368f, 3604.824f};

/// The quest's reminder, its log entry's line from the introduction on
constexpr uint32_t k_BearReminderLine = 2513;

/// The quest played to its end: the scroll pressed, the eight piles taken, and the quest's script waited out; with
/// `replayReminder`, the scroll pressed again once the trail can be followed, for its reminder
[[nodiscard]] std::vector<Command> DungTrail(bool replayReminder = false)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("SwapToBrownBear"),
	    LogEvery("", k_BearScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_FarmerHouse, k_EyeAbove + k_HouseRaised, k_EyeBack, k_HouseRaised),
	};
	PressScroll(commands, k_FarmerHouse);
	commands.push_back(WaitText(k_TrailStartsLine));
	if (replayReminder)
	{
		// the advisors' talk holds the dialogue until its last line is read, and while a script holds it a press on the
		// scroll replays nothing: the press waits for the talk to end
		commands.push_back(WaitTask("HelpSpritesTalk", TaskWait::NotRunning));
		// the introduction's cut-scene left the camera elsewhere: back on the scroll over the house first
		commands.push_back(CameraOn(k_FarmerHouse, k_EyeAbove + k_HouseRaised, k_EyeBack, k_HouseRaised));
		ReplayReminder(commands, k_FarmerHouse, k_BearReminderLine);
	}
	// [shortcut] each pile taken into the hand where it lies, as the player's hand would pick it up
	for (size_t pile = 0; pile < k_DungPiles.size(); ++pile)
	{
		if (pile + 1 == k_DungPiles.size())
		{
			commands.push_back(WaitText(k_OneMorePileLine));
		}
		commands.push_back(
		    {.kind = Kind::HandTake,
		     .delaySeconds = 3.0f,
		     .thing = ThingNear {.type = script::ObjectType::Poo, .point = k_DungPiles.at(pile), .radius = k_DungPickRadius}});
	}
	// the last pile is made only once the evil advisor's line has gone, which may be after the take above: taken again
	// a little later, which finds nothing if it was already taken
	for (size_t retry = 0; retry < k_LastPileRetries; ++retry)
	{
		commands.push_back(
		    {.kind = Kind::HandTake,
		     .delaySeconds = 5.0f,
		     .thing = ThingNear {.type = script::ObjectType::Poo, .point = k_DungPiles.back(), .radius = k_DungPickRadius}});
	}
	commands.push_back(WaitTask("SwapToBrownBear", TaskWait::NotRunning));
	commands.push_back(LogGlobals("", k_BearScripts, 1.0f));
	return commands;
}

void AddSwapToBrownBear(std::vector<Scenario>& all)
{
	auto commands = DungTrail(true);
	commands.push_back(LogGlobals("", k_BearScripts, k_SwapOfferSeconds));
	all.push_back({
	    .id = "quest.swap_to_brown_bear",
	    .name = "Land 5 quest run: Swap to Brown Bear",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/SwapToBrownBear on Land5.txt). The "
	                   "texts that wait for a click are clicked on. It presses on the scroll over the farmer's house, "
	                   "waits for the introduction and the advisors' lines, presses the scroll again for its reminder, "
	                   "then takes each pile of dung into the hand where it lies, eight in turn, and waits for the quest "
	                   "to end.",
	    .expected = "The press brings the farmer out, his complaint, the log entry at 0 and the advisors' lines. Each "
	                "pile in the hand goes and the next appears, with the advisors' lines after the first and the "
	                "seventh; after the eighth the brown bear appears with its scene, a heal chest falls from the sky, the "
	                "entry is done and the creature swap is offered at the bear.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddSwapToBrownBearSwapped(std::vector<Scenario>& all)
{
	auto commands = DungTrail();
	SwapAt(commands, k_BrownBear, k_NearTheBrownBear);
	commands.push_back(LogGlobals("", k_BearScripts, 1.0f));
	commands.push_back(LogGlobals("", k_BearScripts, k_SwapOfferSeconds));
	all.push_back({
	    .id = "quest.swap_to_brown_bear_swapped",
	    .name = "Land 5 quest run: Swap to Brown Bear, the swap taken",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/SwapToBrownBear on Land5.txt). As "
	                   "quest.swap_to_brown_bear to the quest's end; then at the brown bear it presses the scroll with the "
	                   "player's creature far away, puts the creature 30 north of the bear (a shortcut), presses the "
	                   "scroll again and, when the good advisor asks, presses on the bear.",
	    .expected = "As quest.swap_to_brown_bear, then: pressed from afar the bear's scroll gives the evil advisor's "
	                "\"You'll need to bring our Creature, Boss.\"; with the creature near, the press walks it to within 15 "
	                "of the bear, both look each other over and the good advisor asks; the press on the bear swaps the "
	                "minds in a short scene with the sound, and the evil advisor's line follows; the old body stays under "
	                "a scroll. Unverified: whether the ground 30 north of the bear is open land.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// The good advisor's single line when the farmer's house is damaged during the trail, the house's health it is brought
/// down to (below what it had after the introduction), and how long the timeline logs after the line
constexpr uint32_t k_HouseDamagedLine = 1884;
constexpr float k_HurtHouseHealth = 0.5f;
constexpr float k_HouseRadius = 10.0f;
constexpr float k_AfterHouseLineSeconds = 10.0f;

void AddSwapToBrownBearHouseDamaged(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("SwapToBrownBear"),
	    LogEvery("", k_BearScripts),
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_FarmerHouse, k_EyeAbove + k_HouseRaised, k_EyeBack, k_HouseRaised),
	};
	PressScroll(commands, k_FarmerHouse);
	commands.push_back(WaitText(k_TrailStartsLine));
	// [shortcut] the farmer's house hurt by a script's SET_PROPERTY of its health, as a player's blow or miracle would
	// hurt it
	commands.push_back(
	    {.kind = Kind::ScriptSetHealth,
	     .delaySeconds = 2.0f,
	     .amount = k_HurtHouseHealth,
	     .thing = ThingNear {.type = script::ObjectType::Abode, .point = k_FarmerHouse, .radius = k_HouseRadius}});
	commands.push_back(WaitText(k_HouseDamagedLine));
	commands.push_back(LogGlobals("", k_BearScripts, k_AfterHouseLineSeconds));
	all.push_back({
	    .id = "quest.swap_to_brown_bear_house_damaged",
	    .name = "Land 5 quest run: Swap to Brown Bear, the farmer's house damaged",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/SwapToBrownBear on Land5.txt). As "
	                   "quest.swap_to_brown_bear to the advisors' lines after the introduction; then the farmer's house "
	                   "is brought to half its health (a shortcut for the player's blow), and the timeline waits for the "
	                   "good advisor's line and logs the quest's state 10 seconds later.",
	    .expected = "The good advisor steps out, points at the house and says \"The owner of that house will be a little "
	                "distraught.\", once; the trail goes on.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// --- The Explorers Again ---

/// Seconds before the scroll is looked at, while the quest builds the sailors' town and peoples it
constexpr float k_TownBuiltSeconds = 10.0f;
/// The scroll by the wreck stands 2 m over the land inside the wreck's hull, between its floor and its deck, so the
/// hull is nearer than the scroll's click sphere along almost every ray. The sphere is sized by the camera's distance
/// across the land, so it is full size only from 30 m or more away. From the north, low, a ray passing just over the
/// hull's east side, about 2.2 m east of the sphere's centre and within its 2.8 m, meets the sphere's depth before any
/// of the hull: the camera looks along that ray, which the pointer at the middle of the screen follows (worked out
/// from the wreck's triangles and the sphere as the hand's pick tests them, with some 0.17 m to spare). Map
/// coordinates, the heights over the land
constexpr glm::vec3 k_WreckEye {1587.06f, 20.12f, 3963.91f};
constexpr glm::vec3 k_WreckLook {1585.55f, 4.61f, 3919.80f};
constexpr std::string_view k_ExplorersScripts = "MissionariesReturned SwapCreatures";

/// The polar bear the quest brings, where it stands once the swap is offered, and the player's creature put 30 north of
/// it for the swap: from the camera south of the bear the bear stands in front of it. Put south, the creature stood
/// between the camera and the bear, and the press meant for the bear found the creature
constexpr glm::vec2 k_PolarBear {1560.915f, 3934.426f};
constexpr glm::vec2 k_NearThePolarBear {1560.915f, 3964.426f};

/// The quest played to its end: the scroll by the wreck pressed and the quest's script waited out
[[nodiscard]] std::vector<Command> SailorsReturn()
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("MissionariesReturned"),
	    LogEvery("", k_ExplorersScripts),
	    // [camera] the player's view of the scroll past the hull's east side
	    {.kind = Kind::CameraTo, .delaySeconds = k_TownBuiltSeconds, .eye = k_WreckEye, .look = k_WreckLook},
	    // [real] the pointer where the scroll shows past the hull, and the Action button pressed there and let go
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = k_ActionButton},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton},
	};
	commands.push_back(WaitTask("MissionariesReturned", TaskWait::NotRunning));
	commands.push_back(LogGlobals("", k_ExplorersScripts, 1.0f));
	return commands;
}

void AddExplorersAgain(std::vector<Scenario>& all)
{
	auto commands = SailorsReturn();
	commands.push_back(LogGlobals("", k_ExplorersScripts, k_SwapOfferSeconds));
	all.push_back({
	    .id = "quest.the_explorers_again",
	    .name = "Land 5 quest run: The Explorers Again",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/MissionariesReturned on "
	                   "Land5.txt). The texts that wait for a click are clicked on. Once the quest has built the "
	                   "sailors' town it presses on the scroll by the wreck and waits for the quest to end.",
	    .expected = "The wreck lies on the shore and a Norse town of eleven buildings and twenty villagers stands above "
	                "it. The press plays the film: three sailors dance and talk, a polar bear is brought and the "
	                "advisors speak; the log entry is done and the creature swap is offered at the bear.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

void AddExplorersAgainSwapped(std::vector<Scenario>& all)
{
	auto commands = SailorsReturn();
	SwapAt(commands, k_PolarBear, k_NearThePolarBear);
	commands.push_back(LogGlobals("", k_ExplorersScripts, 1.0f));
	commands.push_back(LogGlobals("", k_ExplorersScripts, k_SwapOfferSeconds));
	all.push_back({
	    .id = "quest.the_explorers_again_swapped",
	    .name = "Land 5 quest run: The Explorers Again, the swap taken",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/MissionariesReturned on "
	                   "Land5.txt). As quest.the_explorers_again to the quest's end; then at the polar bear it presses "
	                   "the scroll with the player's creature far away, puts the creature 30 north of the bear (a "
	                   "shortcut), presses the scroll again and, when the good advisor asks, presses on the bear.",
	    .expected = "As quest.the_explorers_again, then: pressed from afar the bear's scroll gives the evil advisor's "
	                "\"You'll need to bring our Creature, Boss.\"; with the creature near, the press walks it to within 15 "
	                "of the bear, both look each other over and the good advisor asks; the press on the bear swaps the "
	                "minds in a short scene with the sound, and the evil advisor's line follows. Unverified: whether the "
	                "ground 30 north of the bear is open land.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// --- The Japanese Traitor ---

/// The three Wonders the land's control script builds as the land begins, which the stranger's secrets are about
constexpr std::array<glm::vec2, 3> k_Wonders {{
    {2663.767f, 3671.004f},
    {2331.919f, 2581.005f},
    {1758.438f, 2337.047f},
}};
constexpr float k_WonderDesire = 0.6f;
/// Where the stranger prays, by his fire, and the scroll made over him
constexpr glm::vec2 k_PrayingPlace {2197.82f, 3502.809f};
/// The advisor's daytime and night remarks on the fire, the last line of each
constexpr uint32_t k_DaytimeRemarkLine = 5573;
constexpr uint32_t k_NightRemarkLine = 5571;
/// Seconds the advisor waits after the daytime remark before it remarks again
constexpr float k_RemarkWaitSeconds = 65.0f;
/// The hours of the day the timeline sets: noon for the daytime remark, the evening for the night remark, morning for
/// the stranger to go once he has talked, and the next evening for him to come back
constexpr float k_Noon = 12.0f;
constexpr float k_Night = 19.0f;
constexpr float k_Morning = 10.0f;
constexpr float k_Evening = 17.0f;
/// The line of each night's talk that only that talk says, about one of the Wonders
constexpr std::array<uint32_t, 3> k_SecretLines {5583, 5585, 5587};
/// Seconds for the stranger to walk to his fire before the scroll is pressed, for the talk's end after its secret, and
/// for him to walk away before the next evening
constexpr float k_ArrivalSeconds = 30.0f;
constexpr float k_TalkEndSeconds = 5.0f;
constexpr float k_LeavingSeconds = 60.0f;
constexpr std::string_view k_TraitorGlobals = "MetTraitor";
constexpr std::string_view k_TraitorScripts = "JapaneseTraitor AdvisorSpotsFire";

void AddJapaneseTraitor(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("BeginLand5"),
	    WaitTask("BeginLand5", TaskWait::Finished),
	};
	// The land's control script builds the three Wonders as the land begins, before it starts the stranger's script
	for (const auto& wonder : k_Wonders)
	{
		commands.push_back(
		    {.kind = Kind::ScriptBuildBuilding, .delaySeconds = 1.0f, .point = wonder, .amount = k_WonderDesire});
	}
	commands.push_back(SetHourTo(k_Noon, 1.0f));
	commands.push_back(CameraOn(k_PrayingPlace, k_EyeAbove, k_EyeBack, 0.0f));
	commands.push_back(StartScript("JapaneseTraitor"));
	commands.push_back(WaitTask("JapaneseTraitor"));
	commands.push_back(LogEvery(k_TraitorGlobals, k_TraitorScripts));
	// the fire seen by day, then by night once the advisor's wait is over; the evening brings the stranger
	commands.push_back(WaitText(k_DaytimeRemarkLine));
	commands.push_back(SetHourTo(k_Night, k_RemarkWaitSeconds));
	commands.push_back(WaitText(k_NightRemarkLine));
	for (size_t night = 0; night < k_SecretLines.size(); ++night)
	{
		if (night > 0)
		{
			commands.push_back(SetHourTo(k_Evening, k_LeavingSeconds));
		}
		commands.push_back(CameraOn(k_PrayingPlace, k_EyeAbove, k_EyeBack, 0.0f));
		commands.back().delaySeconds = k_ArrivalSeconds;
		PressScroll(commands, k_PrayingPlace);
		commands.push_back(WaitText(k_SecretLines.at(night)));
		commands.push_back(SetHourTo(k_Morning, k_TalkEndSeconds));
	}
	commands.push_back(WaitTask("JapaneseTraitor", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_TraitorGlobals, k_TraitorScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_japanese_traitor",
	    .name = "Land 5 quest run: The Japanese Traitor",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5 on Land5.txt). Once the land's "
	                   "arrival has ended it builds the three Wonders as the land's control script does and starts the "
	                   "stranger's script. The texts that wait for a click are clicked on. At noon the camera is put by "
	                   "the fire, and the hour is set to the evening after the advisor's wait; each night the scroll "
	                   "over the stranger is pressed, and after his secret the hour is set to the morning, then to the "
	                   "next evening, three nights in all.",
	    .expected = "The advisor remarks on the fire by day and by night. Each evening the stranger walks to the "
	                "lantern and prays under a scroll; the press plays his talk, the first with his introduction, each "
	                "about one Wonder in turn. After the third he goes for good and the script ends.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// --- The Heavenly Fire ---

/// The Greek village's centre, which the camera must look at from within 150 with the opening shot's point out of
/// view: looked at from its north, the opening shot's point lies behind the camera
constexpr glm::vec2 k_GreekTownCentre {2307.239f, 2589.891f};
constexpr float k_TownEyeHeight = 25.0f;
constexpr float k_TownEyeBack = 30.0f;
/// The good advisor's last line of the warning shot's scene, after which the camera is put back over the village
constexpr uint32_t k_WarningShotLastLine = 1844;
constexpr std::string_view k_FireScripts = "FireOnHighMain FireOnHighMeteor MakeSmokeComeOutOfMeteor rewardfromsky";

[[nodiscard]] Command CameraOverGreekTown(float delay)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = delay,
	        .eye = {k_GreekTownCentre.x, k_TownEyeHeight, k_GreekTownCentre.y + k_TownEyeBack},
	        .look = {k_GreekTownCentre.x, 0.0f, k_GreekTownCentre.y}};
}

void AddHeavenlyFire(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FireOnHighMain"),
	    LogEvery("GoodCatch", k_FireScripts),
	    CameraOverGreekTown(1.0f),
	    WaitText(k_WarningShotLastLine),
	    CameraOverGreekTown(5.0f),
	    WaitTask("FireOnHighMain", TaskWait::NotRunning),
	    LogGlobals("GoodCatch", k_FireScripts, 1.0f),
	};
	all.push_back({
	    .id = "quest.the_heavenly_fire",
	    .name = "Land 5 quest run: The Heavenly Fire",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/FireOnHighMain on Land5.txt). The "
	                   "texts that wait for a click are clicked on. The camera is put over the Greek village's centre, "
	                   "looking at it from the north, and put there again after the warning shot's scene; no meteor is "
	                   "caught and nothing is done to protect the village.",
	    .expected = "Five minutes in, the opening shot lands by the village with the evil advisor's call, the record "
	                "opens and the warning shot follows; then five volleys of five meteors, each announced, hit the "
	                "village. The advisors judge the dead and the damage, a well-protected village earns a chest from "
	                "the sky, and the record closes with the judged alignment.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

/// How the run takes a meteor of a volley. The script launches a volley's meteors one by one, each as a task of its
/// own that checks six seconds after the launch whether the meteor is in the hand and then waits until it is in the hand
/// or has stopped flying. A meteor launched in the background comes a fixed time after the one before; one launched as a
/// script the main task waits for comes only once the task before it has ended.
enum class Catch : uint8_t
{
	/// Taken two seconds after the volley's line, or the volley's spacing after the throw before, and thrown at once
	Timed,
	/// As Timed, then held until no meteor's task runs: the task before it ended and its own checked it in the hand
	TimedHeld,
	/// Taken two seconds after its task starts, then held until no meteor's task runs
	AtLaunchHeld,
	/// Taken two seconds after its task starts, and thrown at once
	AtLaunch,
};
/// The volleys of meteors after the warning shot: the line each starts with, the seconds between its background
/// launches, and how each meteor the hand has time for is caught, as the quest's script launches them
struct Volley
{
	uint32_t line;
	float spacing;
	std::array<std::optional<Catch>, 5> catches;
};
constexpr std::array<Volley, 5> k_Volleys {{
    // the first in the background, the other four each waited for, 8 seconds after the task before ends
    {.line = 5605,
     .spacing = 8.0f,
     .catches = {Catch::AtLaunchHeld, Catch::AtLaunchHeld, Catch::AtLaunchHeld, Catch::AtLaunchHeld, Catch::AtLaunchHeld}},
    {.line = 5606, .spacing = 8.0f, .catches = {Catch::Timed, Catch::Timed, Catch::Timed, Catch::Timed, Catch::Timed}},
    // the fourth and the fifth waited for, the fifth 6 seconds after the fourth's task ends
    {.line = 5607, .spacing = 6.0f, .catches = {Catch::Timed, Catch::Timed, Catch::Timed, Catch::TimedHeld, Catch::AtLaunch}},
    {.line = 5608, .spacing = 5.0f, .catches = {Catch::Timed, Catch::Timed, Catch::Timed, Catch::Timed, Catch::Timed}},
    // a second apart: one hand has time for the first only
    {.line = 5604, .spacing = 1.0f, .catches = {Catch::Timed}},
}};
/// The script that launches each meteor, one task per meteor
constexpr std::string_view k_MeteorScript = "FireOnHighMeteor";
/// A meteor in flight, picked nearest the village's centre within this: two seconds after its launch it is over the
/// village's edge, and it lands within the village after four. The script makes it from the mobile static table's rock
/// row, which makes a thing of the rock class, so it is a rock to the scripts, not a mobile static: a mobile static of
/// that row is a piece knocked off a building, which the hand can never take. Only a flying one is picked, not the
/// small rock of that row lying by the village
constexpr ThingNear k_MeteorInFlight {
    .type = script::ObjectType::Rock, .subtype = 2, .point = k_GreekTownCentre, .radius = 300.0f, .flyingOnly = true};
constexpr float k_TakeAfterLaunchSeconds = 2.0f;
constexpr float k_ThrowAfterTakeSeconds = 0.5f;
/// Where each caught meteor is thrown: over 500 from the village and away from the land's other towns
constexpr glm::vec2 k_SafeLanding {2800.0f, 2450.0f};
/// [shortcut] The player's influence over the village, as winning it gives it: the land's story starts the quest once
/// the player has won the village, which this run skips, so the village is still the rival's and far from the player's
/// towns. Out of the influence the hand's press and release do nothing, and a meteor taken there could never be let go.
/// It reaches past the land the camera shows round the village
constexpr float k_VillageInfluenceRadius = 150.0f;

void AddHeavenlyFireCaught(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("FireOnHighMain"),
	    LogEvery("GoodCatch", k_FireScripts),
	    CameraOverGreekTown(1.0f),
	    WaitText(k_WarningShotLastLine),
	    CameraOverGreekTown(5.0f),
	    {.kind = Kind::ScriptInfluence, .delaySeconds = 0.5f, .point = k_GreekTownCentre, .radius = k_VillageInfluenceRadius},
	};
	for (const auto& volley : k_Volleys)
	{
		commands.push_back(WaitText(volley.line));
		bool first = true;
		for (const auto& meteor : volley.catches)
		{
			if (!meteor.has_value())
			{
				continue;
			}
			const bool atLaunch = *meteor == Catch::AtLaunchHeld || *meteor == Catch::AtLaunch;
			const bool held = *meteor == Catch::AtLaunchHeld || *meteor == Catch::TimedHeld;
			if (atLaunch)
			{
				commands.push_back(WaitTask(k_MeteorScript));
			}
			// [shortcut] the meteor taken into the hand where it flies, as the player's hand would catch it
			const float sinceBefore = first || atLaunch ? k_TakeAfterLaunchSeconds : volley.spacing - k_ThrowAfterTakeSeconds;
			commands.push_back({.kind = Kind::HandTake, .delaySeconds = sinceBefore, .thing = k_MeteorInFlight});
			if (held)
			{
				// held through its task's check, so the script sees it caught and its burst waits
				commands.push_back(WaitTask(k_MeteorScript, TaskWait::NotRunning));
			}
			// [real] thrown away from the village with the hand
			commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = k_ThrowAfterTakeSeconds, .point = k_SafeLanding});
			first = false;
		}
	}
	commands.push_back(WaitTask("FireOnHighMain", TaskWait::NotRunning));
	commands.push_back(LogGlobals("GoodCatch", k_FireScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_heavenly_fire_caught",
	    .name = "Land 5 quest run: The Heavenly Fire, the meteors caught",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/FireOnHighMain on Land5.txt). As "
	                   "quest.the_heavenly_fire to the warning shot's end; then the player is given an influence over "
	                   "the village (a shortcut for winning it, which starts the quest in a natural game: the hand acts "
	                   "only in the player's influence) and, at each volley's line, every meteor the hand has time for "
	                   "is taken into the hand two seconds after its launch (a shortcut for catching it) and thrown away "
	                   "from the village with the hand. A meteor the script waits for is taken two seconds after its task "
	                   "starts and, but for the third volley's last, held until no meteor's task runs, so its task sees "
	                   "it in the hand: all five of the first volley, the first of them for the praise. Every meteor of "
	                   "the first four volleys is caught, the last volley's first only.",
	    .expected = "The first meteor held past six seconds gives the good advisor's \"Oh I say. Well caught.\"; each "
	                "meteor in the hand does not burst until thrown, and bursts where it comes to rest, away from the "
	                "village. 20 of the volleys' 25 are caught; the opening meteor bursts where it is made, by the village, "
	                "and the warning shot and the last volley's other four fall on it. The script counts no catches: it "
	                "judges the villagers lost (none and more than one left: the good advisor's \"You kept the Village "
	                "safe!\"; fewer than 8: "
	                "\"most people survived\"; more: the evil advisor's line) and the share of the village's health lost "
	                "(22% or less: the reward chest from the sky and a good alignment; 30% or less: the good advisor's "
	                "line; more: an evil alignment), and closes the record with it. Unverified: how long a throw takes, so "
	                "whether each take finds its meteor still in flight; the log says what each take and throw found.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// --- The Magic Dragon ---

/// The Tibetan village by the crusaders' cave, whose influence reaches towards their camp once it is the player's
constexpr glm::vec2 k_TibetanVillage {1718.52f, 2337.6f};
constexpr float k_TibetanPickRadius = 100.0f;
/// More belief in the player than in anyone else, as winning a village gives
constexpr float k_WinningBelief = 2.0f;
/// The crusaders' camp, and the scroll put up 5 to its side and 20 above it
constexpr glm::vec2 k_Camp {1896.454f, 2346.574f};
constexpr glm::vec2 k_CampScroll {1901.454f, 2351.574f};
constexpr float k_ScrollEyeHeight = 35.0f;
constexpr float k_ScrollEyeBack = 35.0f;
constexpr float k_ScrollLookHeight = 15.0f;
/// The pyre by the cave's mouth
constexpr glm::vec2 k_Pyre {1883.299f, 2350.002f};
/// The camera looking over the land between the village and the camp, high enough to see both
constexpr glm::vec2 k_VillageToCamp {1830.0f, 2345.0f};
constexpr float k_WideEyeHeight = 80.0f;
constexpr float k_WideEyeBack = 100.0f;
/// The last lines of the introduction and of the pyre's scene, after which the crusaders sit down again
constexpr uint32_t k_HealUsLine = 5611;
constexpr uint32_t k_PyreSceneLastLine = 5616;
constexpr float k_SitDownSeconds = 8.0f;
/// The cave's five blowholes, in the order the fight is followed, and the camera put close above each, looking at it
constexpr std::array<glm::vec2, 5> k_Blowholes {{
    {1826.135f, 2305.804f},
    {1744.944f, 2225.273f},
    {1663.577f, 2166.369f},
    {1581.013f, 2122.308f},
    {1466.07f, 2083.855f},
}};
constexpr float k_BlowholeEyeHeight = 10.0f;
constexpr float k_BlowholeEyeBack = 10.0f;
/// Seconds after the charge's scene before the first blowhole is looked at, and the camera's stay at each, longer than
/// each blowhole's lines take
constexpr float k_AfterChargeSeconds = 3.0f;
constexpr float k_BlowholeStaySeconds = 25.0f;
/// The crusaders' leader's last line by the cave's exit
constexpr uint32_t k_TreasureLastLine = 5619;
constexpr std::array<std::string_view, 5> k_CrusaderScripts {
    "Crusader1Actions", "Crusader2Actions", "Crusader3Actions", "Crusader4Actions", "Crusader5Actions",
};
constexpr std::string_view k_DragonGlobals = "StopActing ChargeIn AllHealedUp";
constexpr std::string_view k_DragonScripts = "Crusaders Crusader1Actions Crusader2Actions Crusader3Actions "
                                             "Crusader4Actions Crusader5Actions ChallengeHighlightNotify "
                                             "GiveSpellDispenserReward";

/// [shortcut] A miracle given to the hand, as if from a bubble, and the hand put over a point by the testbed, or at the
/// edge of the player's influence nearest it; then the Action button is clicked there
void CastOver(std::vector<Command>& commands, SpellSeedType seed, glm::vec2 point)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(seed)});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = point, .value = 2});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1});
}

/// [shortcut] A fireball given to the hand, as if from a bubble, and thrown by the testbed at a point from the edge of
/// the player's influence nearest it; from a point within the influence it is let go over the point and falls on it
void ThrowFireballAt(std::vector<Command>& commands, glm::vec2 point)
{
	commands.push_back({.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Fire)});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = point, .value = 2});
	commands.push_back({.kind = Kind::HandThrowAt, .delaySeconds = 1.0f, .point = point});
	commands.push_back({.kind = Kind::HandOver, .delaySeconds = 3.0f, .value = 1});
}

void AddMagicDragon(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("Crusaders"),
	    LogEvery(k_DragonGlobals, k_DragonScripts),
	    // [shortcut] the Tibetan village takes more belief in the player than in anyone else, as winning it gives, and
	    // its next fold hands it over, for the player's influence the miracles are cast from
	    {.kind = Kind::TownGainsBelief,
	     .player = PlayerNames::PLAYER_ONE,
	     .delaySeconds = 1.0f,
	     .amount = k_WinningBelief,
	     .thing = ThingNear {.type = script::ObjectType::Town, .point = k_TibetanVillage, .radius = k_TibetanPickRadius}},
	    WaitTask("ChallengeHighlightNotify"),
	    CameraOn(k_CampScroll, k_ScrollEyeHeight, k_ScrollEyeBack, k_ScrollLookHeight),
	};
	PressScroll(commands, k_CampScroll);
	// the introduction; then a heal miracle cast over the camp
	commands.push_back(WaitText(k_HealUsLine));
	commands.push_back(CameraOn(k_Camp, k_EyeAbove, k_EyeBack, 0.0f));
	commands.back().delaySeconds = k_SitDownSeconds;
	CastOver(commands, SpellSeedType::Heal, k_Camp);
	// the pyre's scene; then a fireball let go over the pyre, which falls on it
	commands.push_back(WaitText(k_PyreSceneLastLine));
	commands.push_back(CameraOn(k_VillageToCamp, k_WideEyeHeight, k_WideEyeBack, 0.0f));
	commands.back().delaySeconds = k_SitDownSeconds;
	ThrowFireballAt(commands, k_Pyre);
	// the charge's scene ends once the last crusader has gone into the cave; then each blowhole is looked at in turn
	commands.push_back(WaitGlobal("ChargeIn", 1.0f));
	for (const auto script : k_CrusaderScripts)
	{
		commands.push_back(WaitTask(script, TaskWait::NotRunning));
	}
	for (size_t hole = 0; hole < k_Blowholes.size(); ++hole)
	{
		auto camera = CameraOn(k_Blowholes.at(hole), k_BlowholeEyeHeight, k_BlowholeEyeBack, 0.0f);
		camera.delaySeconds = hole == 0 ? k_AfterChargeSeconds : k_BlowholeStaySeconds;
		commands.push_back(camera);
	}
	// the treasure's scene, then the survivors' wait to be healed, which is left to run out
	commands.push_back(WaitText(k_TreasureLastLine));
	commands.push_back(WaitTask("Crusaders", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_DragonGlobals, k_DragonScripts, 1.0f));
	all.push_back({
	    .id = "quest.the_magic_dragon",
	    .name = "Land 5 quest run: The Magic Dragon",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/Crusaders on Land5.txt). The texts "
	                   "that wait for a click are clicked on. It hands the Tibetan village by the cave to the player, "
	                   "presses on the scroll over the crusaders' camp, casts a heal miracle over the camp after the "
	                   "introduction, lets a fireball go over the pyre after its scene, then, once the crusaders have "
	                   "charged into the cave, puts the camera close above each of the five blowholes in turn, and waits "
	                   "for the quest to end.",
	    .expected = "The press plays the introduction: the crusaders ask to be healed. Healed, they ask for the pyre to "
	                "be lit; lit, its smoke pours out of the five blowholes and the crusaders charge in. At each blowhole "
	                "the fight is heard; at the fifth three crusaders stagger out of the far exit with a flying-flock "
	                "miracle dispenser, the player gains an influence ring there, the scroll is done, and after two "
	                "minutes the survivors walk back into the exit.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}

// --- Stanley the Wolf ---

/// The lion maze's place, which the scroll stands over
constexpr glm::vec2 k_LionMaze {1620.0453f, 2969.1855f};
/// The evil advisor's line that ends the man's introduction, after which the quest waits for the maze to be solved
constexpr uint32_t k_RingTheBellsLine = 5597;
/// The middle of the maze's grid of seven squares, each two half squares of 2 metres along x and z from its place
constexpr glm::vec2 k_MazeMiddle {1634.0453f, 2983.1855f};
/// [shortcut] The player's influence over the maze and its bells, as a town won near it would give it: the hand takes
/// hold of a bell only in it
constexpr float k_MazeInfluenceRadius = 80.0f;
/// The camera straight above the grid's middle, high enough to see every bell round it
constexpr float k_MazeEyeAbove = 70.0f;
constexpr float k_MazeEyeBack = 10.0f;
/// The four bells that send the wolf a way, by the way: +y, +x, -y, -x; each stands a square and a half beyond the
/// middle of a side, and a pick this near finds only its own
enum class Bell : uint8_t
{
	PlusY = 0,
	PlusX = 1,
	MinusY = 2,
	MinusX = 3,
};
constexpr std::array<glm::vec2, 4> k_Bells {{
    {1634.0453f, 3003.1855f},
    {1654.0453f, 2983.1855f},
    {1634.0453f, 2963.1855f},
    {1614.0453f, 2983.1855f},
}};
constexpr float k_BellPickRadius = 5.0f;
/// The bells a player rings to bring the wolf from its square to the sheep's, the shortest way: each slide stops at a
/// wall, the last on the sheep
constexpr std::array<Bell, 8> k_Solution {Bell::MinusY, Bell::PlusX,  Bell::PlusY,  Bell::MinusX,
                                          Bell::PlusY,  Bell::MinusX, Bell::MinusY, Bell::PlusX};
/// Seconds from one bell to the next: the wolf's longest slide, five squares at 4 m/s, and its turn take less. A bell
/// rung while the wolf walks is not taken
constexpr float k_BellSeconds = 12.0f;
/// The lion the quest gives, where it is made
constexpr glm::vec2 k_Lion {1572.228f, 2988.446f};
/// Where the player's creature is put for the swap: 30 north of the lion, within the 50 the swap needs, so that from
/// the camera south of the lion the lion stands in front of it once the swap has walked it to within 15
constexpr glm::vec2 k_BehindTheLion {1572.228f, 3018.446f};
/// Seconds the timeline keeps logging once the swap is done
constexpr float k_StanleySwapSeconds = 10.0f;
constexpr std::string_view k_StanleyGlobals = "PuzzlePlayed PuzzleStatus TamerDead GlobalCurrentCreature";
constexpr std::string_view k_StanleyScripts = "LionPuzzle LPSitDown ChallengeHighlightNotifyWithEndVariable DidYouKnow "
                                              "SwapCreatures";

void AddStanleyTheWolf(std::vector<Scenario>& all)
{
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("LionPuzzle"),
	    LogEvery(k_StanleyGlobals, k_StanleyScripts),
	    WaitTask("ChallengeHighlightNotifyWithEndVariable"),
	    CameraOn(k_LionMaze, k_EyeAbove, k_EyeBack, 0.0f),
	};
	PressScroll(commands, k_LionMaze);
	// the introduction plays to the evil advisor's line
	commands.push_back(WaitText(k_RingTheBellsLine));
	// [shortcut] the player's influence over the maze
	commands.push_back(
	    {.kind = Kind::ScriptInfluence, .delaySeconds = 2.0f, .point = k_MazeMiddle, .radius = k_MazeInfluenceRadius});
	// [camera] straight above the grid
	commands.push_back(CameraOn(k_MazeMiddle, k_MazeEyeAbove, k_MazeEyeBack, 0.0f));
	commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 1.0f});
	// [real] each bell of the solution tapped with the hand: the pointer on it, the Action button pressed once the hand
	// is over it and let go
	for (size_t ring = 0; ring < k_Solution.size(); ++ring)
	{
		const ThingNear bell {.type = script::ObjectType::Abode,
		                      .subtype = static_cast<uint32_t>(AbodeInfo::Belltower),
		                      .point = k_Bells.at(static_cast<size_t>(k_Solution.at(ring))),
		                      .radius = k_BellPickRadius};
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = ring == 0 ? 1.0f : k_BellSeconds, .thing = bell});
		commands.push_back(
		    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = k_ActionButton, .thing = bell});
		commands.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 0.2f});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
		commands.push_back(LogGlobals(k_StanleyGlobals, k_StanleyScripts, 1.0f));
	}
	// the wolf on the sheep: the man's thanks, the lion, the log at 1 and the advisors' lines, then the quest's end
	commands.push_back(WaitTask("LionPuzzle", TaskWait::NotRunning));
	commands.push_back(LogGlobals(k_StanleyGlobals, k_StanleyScripts, 1.0f));
	// the swap: the lion's scroll pressed first with the player's creature far away, then with it by the lion; when the
	// good advisor asks, the lion pressed on confirms
	SwapAt(commands, k_Lion, k_BehindTheLion);
	commands.push_back(LogGlobals(k_StanleyGlobals, k_StanleyScripts, 1.0f));
	commands.push_back(LogGlobals(k_StanleyGlobals, k_StanleyScripts, k_StanleySwapSeconds));
	all.push_back({
	    .id = "quest.stanley_the_wolf",
	    .name = "Land 5 quest run: Stanley the Wolf",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand5/BeginLand5/LionPuzzle on Land5.txt). The texts "
	                   "that wait for a click are clicked on. It presses on the scroll over the lion maze, lets the man's "
	                   "introduction play to the evil advisor's line, gives the player influence over the maze (a "
	                   "shortcut), and taps the eight bells that bring the wolf to the sheep with the hand, 12 seconds "
	                   "apart; then it waits for the quest to end. At the lion it presses the scroll with the "
	                   "player's creature far away, puts the creature 30 behind the lion (a shortcut), presses the "
	                   "scroll again and, when the good advisor asks, presses on the lion.",
	    .expected = "The man sits by his fire next to the maze, the scroll over it. Its press starts the scene: the man "
	                "stands, the did-you-know signpost, the log at 0 and his three lines, the camera high over the grid "
	                "and the evil advisor's line. Each tapped bell rings and the wolf slides its way to the next wall; "
	                "the eighth brings it onto the sheep: the lion is made, the man's thanks, the log at 1, the lion's "
	                "wave, the swap offer and the evil advisor's line. Pressed from afar the lion's scroll gives the evil "
	                "advisor's \"You'll need to bring our Creature, Boss.\"; with the creature near, the press walks it "
	                "to within 15 of the lion, both look each other over and the good advisor asks; the press on the "
	                "lion swaps the minds in a short scene with the sound, and the evil advisor's line follows.",
	    .commands = std::move(commands),
	    .onLand = true,
	});
}
} // namespace

void testbed_scenarios::AddQuestLand5SilverScenarios(std::vector<Scenario>& all)
{
	AddSwapToBrownBear(all);
	AddSwapToBrownBearSwapped(all);
	AddSwapToBrownBearHouseDamaged(all);
	AddExplorersAgain(all);
	AddExplorersAgainSwapped(all);
	AddJapaneseTraitor(all);
	AddHeavenlyFire(all);
	AddHeavenlyFireCaught(all);
	AddMagicDragon(all);
	AddStanleyTheWolf(all);
}
