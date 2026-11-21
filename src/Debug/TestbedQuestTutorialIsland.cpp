/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Gods' Playground, the tutorial island, as a quest run: its control script started on the island's own land, as
// the game starts it once the player has said yes to going there, played through its first four lessons. The timeline
// does what the player would: it clicks the texts that wait for a click, keeps the camera behind the Island Keeper as
// he walks round the mountain, up and down its path and down the valley, turns the view at the screen's edge and grips
// the land when the turning lesson asks, and holds both buttons to zoom out over the island and back in. The points are
// map coordinates, taken from the places the island's script names.

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL_scancode.h>
#include <glm/geometric.hpp>

#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedLand2Quests.h"
#include "TestbedLandTimeline.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;
using namespace openblack::testbed_scenarios::land2_quests;

namespace
{
/// Where the Keeper stands when the first lesson's opening sends him off
constexpr glm::vec2 k_KeeperStart {1952.072f, 2272.131f};
/// The ten places he walks to round the mountain, in turn, in both rounds of the first lesson
constexpr std::array<glm::vec2, 10> k_RoundTheMountain {{
    {1940.548f, 2243.434f},
    {1979.455f, 2296.931f},
    {2058.608f, 2336.029f},
    {2104.069f, 2332.723f},
    {2136.4f, 2312.751f},
    {2130.143f, 2194.607f},
    {2074.154f, 2147.896f},
    {1983.881f, 2164.21f},
    {1957.912f, 2175.403f},
    {1944.722f, 2206.895f},
}};
/// Where the opening of the timed round leaves him facing from, before he walks to the round's first place
constexpr glm::vec2 k_TimedRoundFrom {1944.927f, 2215.655f};
/// The twelve places he walks to down the valley, in the third lesson, and where its opening sends him from
constexpr glm::vec2 k_ValleyFrom {2299.745f, 2540.89f};
constexpr std::array<glm::vec2, 12> k_DownTheValley {{
    {2417.381f, 2584.134f},
    {2565.457f, 2637.583f},
    {2654.33f, 2680.391f},
    {2762.708f, 2772.566f},
    {2712.82f, 2806.224f},
    {2648.625f, 2796.447f},
    {2531.348f, 2689.71f},
    {2508.611f, 2724.61f},
    {2482.636f, 2767.38f},
    {2406.996f, 2668.829f},
    {2350.327f, 2648.332f},
    {2281.814f, 2620.181f},
}};
/// The Keeper, a villager of his own kind, at the foot of the tilting lesson's path, where its opening leaves him, and
/// at its top, where he turns to come down again
constexpr ThingNear k_KeeperAtFoot {
    .type = script::ObjectType::Villager, .subtype = 64, .point = {1957.912f, 2175.403f}, .radius = 60.0f};
constexpr ThingNear k_KeeperAtTop {
    .type = script::ObjectType::Villager, .subtype = 64, .point = {2038.24f, 2194.617f}, .radius = 60.0f};
/// The tilting lesson's last opening line, after which he sets off up the path; the line once he is up with the player
/// keeping him in view, the line before he comes down again, and the line once he is down with the player keeping him
/// in view
constexpr uint32_t k_TiltSetOffLine = 5736;
constexpr uint32_t k_UpInViewLine = 5787;
constexpr uint32_t k_ComeDownLine = 5788;
constexpr uint32_t k_DownInViewLine = 5791;
/// Seconds from the line before he comes down to the end of the cinema's camera move, three seconds long, after which
/// he sets off
constexpr float k_ComeDownCameraSeconds = 4.0f;
/// The camera kept this far behind the place the Keeper walks to, this high above the land, looking this far past it:
/// within the 50 the lessons want, with the Keeper in view and facing away from it as he arrives
constexpr float k_BehindDistance = 20.0f;
constexpr float k_BehindHeight = 12.0f;
constexpr float k_LookPast = 15.0f;
constexpr float k_LookHeight = 2.0f;
/// The turning lesson's lines: its ask to turn, the line once the hand shows the turning arrows, and the one once the
/// land has been gripped
constexpr uint32_t k_TurnAskLine = 5756;
constexpr uint32_t k_ArrowsShownLine = 5757;
constexpr uint32_t k_GrippedLine = 6650;
/// The pointer at the screen's left edge, where the hand shows the turning arrows, and in the middle again
constexpr glm::vec2 k_LeftEdge {0.01f, 0.5f};
constexpr glm::vec2 k_Middle {0.5f, 0.6f};
constexpr size_t k_MoveButton = 1;
/// The valley lesson's last opening line, said as the Keeper sets off; its cinema ends two seconds after his walk starts
constexpr uint32_t k_ValleySetOffLine = 5796;
constexpr float k_ValleyCinemaEndSeconds = 4.0f;
/// The zooming lesson's last line before it waits for the zoom, the line once both buttons are held, and the one once
/// both of its marker stones are in view
constexpr uint32_t k_ZoomAskLine = 5806;
constexpr uint32_t k_BothButtonsLine = 5807;
constexpr uint32_t k_BothStonesLine = 5808;
/// [shortcut] the view over the whole island that the zoom out would give, the island's middle seen from high above its
/// south, and the Keeper's place again, close up, that the zoom back in gives
constexpr glm::vec3 k_OverTheIslandEye {2455.0f, 1400.0f, 1600.0f};
constexpr glm::vec3 k_OverTheIslandLook {2455.0f, 0.0f, 2680.0f};
constexpr glm::vec3 k_BackInEye {2817.293f, 14.0f, 3141.507f};
constexpr glm::vec3 k_BackInLook {2816.9f, 1.0f, 3144.206f};
/// Seconds between the logs of the lessons' state while they play, so that a wait they never get past shows where
constexpr size_t k_LogPeriodSeconds = 30;
/// The island's control script
constexpr std::string_view k_IslandControlScript = "LandControlT";

constexpr std::string_view k_Globals = "TutorialLevel LastScript GuidePosReached GuideWalking RotateTalkDone Restart";
constexpr std::string_view k_Scripts = "LandControlT MoveAroundMountain MoveUpAndDownMountain MoveDownValley ZoomInAndOut "
                                       "PickupTeddyBears DoubleClicking";

[[nodiscard]] Command LogEvery()
{
	return {.kind = Kind::LogGlobals, .value = k_LogPeriodSeconds, .global = k_Globals, .script = k_Scripts};
}

[[nodiscard]] Command WaitAtLeast(std::string_view global, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = Compare::AtLeast};
}

/// [shortcut] The camera put behind the place the Keeper walks to, on his way from the place before, as the player
/// keeps up with him by dragging the land
[[nodiscard]] Command CameraBehind(glm::vec2 from, glm::vec2 to)
{
	const auto ahead = glm::normalize(to - from);
	const auto eye = to - (ahead * k_BehindDistance);
	const auto look = to + (ahead * k_LookPast);
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {eye.x, k_BehindHeight, eye.y},
	        .look = {look.x, k_LookHeight, look.y}};
}

/// Each place the Keeper walks to, from the first given: once the lesson's count says he is on his way there, the
/// camera behind it
void FollowTheKeeper(std::vector<Command>& commands, glm::vec2 from, std::span<const glm::vec2> places, size_t first)
{
	for (size_t place = first; place < places.size(); ++place)
	{
		commands.push_back(WaitAtLeast("GuidePosReached", static_cast<float>(place + 1)));
		commands.push_back(CameraBehind(place == 0 ? from : places[place - 1], places[place]));
	}
}

/// Who plays the lessons after the first round of the mountain
enum class Play : uint8_t
{
	/// The timeline, every lesson
	Timeline,
	/// The player's own mouse turns the view in the turning lesson (the run's recorded input, not the timeline's
	/// pointer); the timeline ends with the tilting lesson, as the zooming lesson's buttons are the timeline's
	TurnByHand,
	/// As TurnByHand, and from the tilting lesson on the recorded input alone moves the camera: the timeline only
	/// clicks the texts and logs where the camera is
	AllByHand,
	/// As AllByHand, through all six lessons from the island's start: the recorded input tilts, zooms out, picks up and
	/// double clicks; the timeline keeps the camera behind the Keeper down the valley as in Timeline, puts it back by the
	/// Keeper once the zoom out has both stones in view, and over the ditch after the teddy bear lesson's opening
	SixLessons,
};
/// Seconds between the logs of where the camera is, and how many, once the recorded input moves it: twenty minutes
constexpr float k_CameraLogSeconds = 0.5f;
constexpr size_t k_CameraLogs = 2400;

std::vector<Command> FirstLessons(Play play)
{
	const bool byHand = play != Play::Timeline;
	std::vector<Command> commands {
	    ClickTexts(),
	    WaitTask("MoveAroundMountain"),
	    LogEvery(),
	};

	// ---- Lesson 1, the first round: the Keeper's opening sends him to the second place; then he goes on round the
	// mountain while the camera is near him. At the fifth place the turning lesson stops him
	commands.push_back(WaitAtLeast("GuidePosReached", 2.0f));
	commands.push_back(CameraBehind(k_KeeperStart, k_RoundTheMountain[1]));
	for (size_t place = 2; place < 4; ++place)
	{
		commands.push_back(WaitAtLeast("GuidePosReached", static_cast<float>(place + 1)));
		commands.push_back(CameraBehind(k_RoundTheMountain[place - 1], k_RoundTheMountain[place]));
	}
	// The turning lesson: the pointer to the screen's left edge, where the hand shows its turning arrows; then, once the
	// line asking to hold the Move button is read and its dialogue over (the script reads the hand only after it), the
	// Move button held, which grips the land, until the next line starts
	commands.push_back(WaitText(k_TurnAskLine));
	if (!byHand)
	{
		commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_LeftEdge});
	}
	commands.push_back(WaitText(k_ArrowsShownLine));
	if (!byHand)
	{
		commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_MoveButton});
		commands.push_back({.kind = Kind::WaitText, .waitUntilFree = true, .value = k_GrippedLine});
		commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = k_MoveButton});
	}
	commands.push_back(WaitText(k_GrippedLine));
	commands.push_back(WaitGlobal("RotateTalkDone", 1.0f));
	commands.push_back(CameraBehind(k_RoundTheMountain[3], k_RoundTheMountain[4]));
	for (size_t place = 5; place < k_RoundTheMountain.size(); ++place)
	{
		commands.push_back(WaitAtLeast("GuidePosReached", static_cast<float>(place + 1)));
		commands.push_back(CameraBehind(k_RoundTheMountain[place - 1], k_RoundTheMountain[place]));
	}

	// ---- Lesson 1, the timed round: the same places from the first, within 50 of him
	commands.push_back(WaitGlobal("GuideWalking", 1.0f));
	commands.push_back(CameraBehind(k_TimedRoundFrom, k_RoundTheMountain[0]));
	FollowTheKeeper(commands, k_TimedRoundFrom, k_RoundTheMountain, 1);
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	if (play == Play::AllByHand)
	{
		// the camera is the player's from here on: only where it is, logged, for the recorded input's steps
		commands.push_back(WaitTask("MoveUpAndDownMountain"));
		for (size_t log = 0; log < k_CameraLogs; ++log)
		{
			commands.push_back({.kind = Kind::LogCameraBookmarks, .delaySeconds = k_CameraLogSeconds});
		}
		return commands;
	}

	if (play == Play::SixLessons)
	{
		// lesson 2 is the recorded input's: the timeline waits for the valley
		commands.push_back(WaitTask("MoveDownValley"));
		commands.push_back(WaitText(k_ValleySetOffLine));
		commands.push_back(CameraBehind(k_ValleyFrom, k_DownTheValley[0]));
		commands.back().delaySeconds = k_ValleyCinemaEndSeconds;
		FollowTheKeeper(commands, k_ValleyFrom, k_DownTheValley, 1);
		commands.push_back(WaitTask("MoveDownValley", TaskWait::Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
		// lesson 4's buttons and zoom out are the recorded input's; the zoom back in is the timeline's
		commands.push_back(WaitTask("ZoomInAndOut"));
		commands.push_back(WaitText(k_BothStonesLine));
		commands.push_back({.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_BackInEye, .look = k_BackInLook});
		commands.push_back(WaitTask("ZoomInAndOut", TaskWait::Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
		return commands;
	}

	// ---- Lesson 2, tilting: the lesson leaves the camera at the path's foot and wants the Keeper on the screen all
	// the way up and down again. [shortcut] The camera kept behind him, as the player keeps him in view by tilting
	// the view and dragging the land, from his setting off until each of the lesson's cinemas, which put the camera
	// where they want it, has said he was kept in view
	commands.push_back(WaitTask("MoveUpAndDownMountain"));
	commands.push_back(WaitText(k_TiltSetOffLine));
	commands.push_back({.kind = Kind::CameraFollow, .delaySeconds = 0.5f, .amount = 1.0f, .thing = k_KeeperAtFoot});
	commands.push_back({.kind = Kind::WaitText, .waitUntilFree = true, .value = k_UpInViewLine});
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	commands.push_back(WaitText(k_ComeDownLine));
	commands.push_back(
	    {.kind = Kind::CameraFollow, .delaySeconds = k_ComeDownCameraSeconds, .amount = 1.0f, .thing = k_KeeperAtTop});
	commands.push_back({.kind = Kind::WaitText, .waitUntilFree = true, .value = k_DownInViewLine});
	commands.push_back({.kind = Kind::CameraFollow, .value = 1});
	if (byHand)
	{
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 60.0f));
		return commands;
	}
	commands.push_back(WaitTask("MoveUpAndDownMountain", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));

	// ---- Lesson 3, the valley: the camera behind him from the opening's end, down to the valley's end
	commands.push_back(WaitTask("MoveDownValley"));
	commands.push_back(WaitText(k_ValleySetOffLine));
	commands.push_back(CameraBehind(k_ValleyFrom, k_DownTheValley[0]));
	commands.back().delaySeconds = k_ValleyCinemaEndSeconds;
	FollowTheKeeper(commands, k_ValleyFrom, k_DownTheValley, 1);
	commands.push_back(WaitTask("MoveDownValley", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));

	// ---- Lesson 4, zooming: both buttons held in the middle of the screen and the mouse moved, as the player zooms;
	// then the view over the whole island, and back in again
	commands.push_back(WaitTask("ZoomInAndOut"));
	commands.push_back(WaitText(k_ZoomAskLine));
	// the lesson looks at the hand only once its dialogue has ended, about four seconds after the line is read; the land
	// is gripped with Move first, and Action is held as well once the grip is under way, which is what zooms
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = k_Middle});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_MoveButton});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = 0.2f, .point = {0.0f, 0.1f}, .amount = 1.0f});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = k_ActionButton});
	commands.push_back({.kind = Kind::PointerRelease, .value = k_MoveButton});
	commands.push_back(WaitText(k_BothButtonsLine));
	commands.push_back({.kind = Kind::CameraTo, .delaySeconds = 2.0f, .eye = k_OverTheIslandEye, .look = k_OverTheIslandLook});
	commands.push_back(WaitText(k_BothStonesLine));
	commands.push_back({.kind = Kind::CameraTo, .delaySeconds = 1.0f, .eye = k_BackInEye, .look = k_BackInLook});
	commands.push_back(WaitTask("ZoomInAndOut", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 60.0f));
	return commands;
}
/// One lesson played by the recorded input alone, as a quest run that sets the island's one-lesson switch and the
/// lesson's number before starting its control script starts it: the texts clicked on and where the camera is logged
/// every half second, nothing else
std::vector<Command> OneLessonByHand()
{
	std::vector<Command> commands {ClickTexts(), LogEvery()};
	for (size_t log = 0; log < k_CameraLogs; ++log)
	{
		commands.push_back({.kind = Kind::LogCameraBookmarks, .delaySeconds = k_CameraLogSeconds});
	}
	return commands;
}

/// The teddy bear lesson's last opening line, said just before its cinema ends, and its closing line, said once the five
/// are in the ditch
constexpr uint32_t k_TeddyLastOpeningLine = 5736;
constexpr uint32_t k_TeddyClosingLine = 5820;
/// Seconds from each of those lines read to past the end of its cinema, which hands the camera back as the cinema had it
constexpr float k_TeddyCinemaEndSeconds = 8.0f;
/// The view high above the plateau behind the bears, looking down past them along the line from the ditch through the
/// script's own view of the ditch, so that the bears, the ditch and the lesson's scroll are all in sight with no ridge
/// between: the eye 195 above the land, the point looked at 9 below the land there (the slope down to the ditch)
constexpr glm::vec3 k_OverTheDitchEye {2801.2f, 194.8f, 3185.4f};
constexpr glm::vec3 k_OverTheDitchLook {2896.9f, -9.0f, 3060.8f};

/// The teddy bear lesson played by the recorded input from one view: the texts clicked on, and the camera put over the
/// ditch once the lesson's opening is over and again once its closing is, as a player who has moved the view there
/// would see it. No pointer, no button: the recorded input picks the bears up, drops them and taps the scrolls
std::vector<Command> TeddyBearsOverTheDitch()
{
	std::vector<Command> commands {ClickTexts(), LogEvery()};
	for (const auto line : std::array {k_TeddyLastOpeningLine, k_TeddyClosingLine})
	{
		commands.push_back(WaitText(line));
		commands.push_back({.kind = Kind::CameraTo,
		                    .delaySeconds = k_TeddyCinemaEndSeconds,
		                    .eye = k_OverTheDitchEye,
		                    .look = k_OverTheDitchLook});
	}
	for (size_t log = 0; log < k_CameraLogs; ++log)
	{
		commands.push_back({.kind = Kind::LogCameraBookmarks, .delaySeconds = k_CameraLogSeconds});
	}
	return commands;
}

/// All six lessons in order from the island's start, played by the recorded input but for the camera placings
/// FirstLessons(Play::SixLessons) and the teddy bear lesson's view over the ditch give
std::vector<Command> SixLessons()
{
	auto commands = FirstLessons(Play::SixLessons);
	// once only, after the opening: the double click lesson follows the closing at once and wants its own view
	commands.push_back(WaitTask("PickupTeddyBears"));
	commands.push_back(WaitText(k_TeddyLastOpeningLine));
	commands.push_back({.kind = Kind::CameraTo,
	                    .delaySeconds = k_TeddyCinemaEndSeconds,
	                    .eye = k_OverTheDitchEye,
	                    .look = k_OverTheDitchLook});
	commands.push_back(WaitTask("DoubleClicking"));
	commands.push_back(WaitTask("DoubleClicking", TaskWait::Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 1.0f));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 60.0f));
	return commands;
}

/// The question box's Yes and No on the run's 1024 by 768 window: the box's 800 by 600 dialog space drawn a pixel a unit
/// in the middle of the screen, each arrow 32 square at (200, 338) and (568, 338), clicked at its centre
constexpr glm::vec2 k_QuestionYes {(112.0f + 216.0f) / 1024.0f, (84.0f + 354.0f) / 768.0f};
constexpr glm::vec2 k_QuestionNo {(112.0f + 584.0f) / 1024.0f, (84.0f + 354.0f) / 768.0f};
/// Seconds after the land's set-up before F2 is pressed, and between the box's showing and the click on it, past its
/// half-second fade
constexpr float k_BeforeTheKey = 10.0f;
constexpr float k_BeforeTheClick = 2.0f;

/// F2 pressed, then the question's answer clicked with the Move button, as the player answers it
void PressF2AndAnswer(std::vector<Command>& commands, glm::vec2 answer, float delay)
{
	commands.push_back({.kind = Kind::TapKey, .delaySeconds = delay, .value = static_cast<size_t>(SDL_SCANCODE_F2)});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_BeforeTheClick, .point = answer});
	commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = k_MoveButton});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_MoveButton});
}
} // namespace

void testbed_scenarios::AddQuestTutorialIslandScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "quest.tutorial_island",
	    .name = "Tutorial island quest run: the first four lessons",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControlT on LandT.txt). The texts that wait for a "
	                   "click are clicked on. Each time the Keeper sets off for the next place round the mountain or down "
	                   "the valley, the camera is put behind that place, looking on past it; in the tilting lesson it is "
	                   "kept behind the Keeper as he climbs the path and comes down. In the turning lesson the "
	                   "pointer goes to the screen's left edge, then the land is gripped and dragged there and in the "
	                   "middle. In the zooming lesson both buttons are held and the mouse moved, then the camera is put "
	                   "high over the island and back by the Keeper. The lessons' state is logged every 30 seconds.",
	    .expected = "The island's set-up (the Keeper, the influence ring, the dispensers), then the first lesson's "
	                "opening, both rounds of the mountain with the turning lesson between them, the tilting lesson, the "
	                "valley and the zooming lesson, each with its lines; the fifth lesson, the teddy bears, starts.",
	    .commands = FirstLessons(Play::Timeline),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.tutorial_island_by_hand",
	    .name = "Tutorial island quest run: the first two lessons, turning by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControlT on LandT.txt), with the player's mouse and "
	                   "keys from a recording. As quest.tutorial_island, the texts are clicked on and the camera is put "
	                   "behind the Keeper's places round the mountain and kept behind him on the tilting lesson's path; "
	                   "the turning lesson has no pointer of the timeline's: the recorded mouse goes to the screen's left "
	                   "edge and holds the Move button there. The timeline ends in the tilting lesson.",
	    .expected = "The first lesson's opening and first round; the turning lesson's arrows shown and the land gripped "
	                "from the recorded mouse, then its second round and the tilting lesson, each with its lines.",
	    .commands = FirstLessons(Play::TurnByHand),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.tutorial_island_all_by_hand",
	    .name = "Tutorial island quest run: the lessons from the tilting one on, played by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControlT on LandT.txt), with the player's mouse and "
	                   "keys from a recording. The first lesson as quest.tutorial_island_by_hand. From the tilting lesson "
	                   "on the timeline only clicks the texts and logs where the camera is, every half second: the "
	                   "recorded input alone tilts, drags, turns and zooms the view, picks up and drops, and double clicks.",
	    .expected = "The first lesson as quest.tutorial_island_by_hand; then each lesson the recorded input plays goes "
	                "on as the player's input takes it, with its lines.",
	    .commands = FirstLessons(Play::AllByHand),
	    .onLand = true,
	});
	all.push_back({
	    .id = "quest.tutorial_island_one_lesson_by_hand",
	    .name = "Tutorial island quest run: one lesson, played by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start that sets the island's one-lesson switch and the lesson's number "
	                   "before its control script (quest=OneOffLevel=1/TutorialLevel=<n>/LandControlT on LandT.txt), "
	                   "with the player's mouse and keys from a recording. The timeline only clicks the texts and logs "
	                   "the lessons' state every 30 seconds and where the camera is every half second.",
	    .expected = "The lesson's opening and, as the recorded input plays it, its lines to its end; then the island's "
	                "free play: the six scrolls.",
	    .commands = OneLessonByHand(),
	    .onLand = true,
	});

	all.push_back({
	    .id = "quest.tutorial_island_teddy_over_the_ditch",
	    .name = "Tutorial island quest run: the teddy bear lesson, played by hand from above the ditch",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start of the teddy bear lesson alone "
	                   "(quest=OneOffLevel=1/TutorialLevel=5/LandControlT on LandT.txt), with the player's mouse and keys "
	                   "from a recording. The timeline clicks the texts, and once the lesson's opening is over, and again "
	                   "once its closing is, puts the camera high above the plateau behind the bears, looking down past "
	                   "them to the ditch; it moves nothing else.",
	    .expected = "The lesson's opening; the five bears, picked up and dropped by the recorded input, in the ditch; the "
	                "lesson's closing lines; then the island's free play, where a scroll the recorded input taps runs its "
	                "lesson again.",
	    .commands = TeddyBearsOverTheDitch(),
	    .onLand = true,
	});

	all.push_back({
	    .id = "quest.tutorial_island_six_lessons_by_hand",
	    .name = "Tutorial island quest run: all six lessons in order, played by hand",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=LandControlT on LandT.txt), with the player's mouse and "
	                   "keys from a recording. The first lesson as quest.tutorial_island_all_by_hand; the recorded input "
	                   "tilts, zooms out, picks up and double clicks. The timeline clicks the texts, keeps the camera "
	                   "behind the Keeper down the valley, puts it back by the Keeper once both zoom stones are in view, "
	                   "and over the ditch after the teddy bear lesson's opening.",
	    .expected = "The six lessons one after the other, each with its lines; then the island's free play.",
	    .commands = SixLessons(),
	    .onLand = true,
	});

	// F2 on the first land, by the keyboard and the mouse: asked and answered No, then asked again and answered Yes
	// the set-up runs and ends within one turn, before a wait sees it run: it is waited on until no task of it runs
	std::vector<Command> trip {
	    WaitTask("SetupLand1", TaskWait::NotRunning),
	    {.kind = Kind::LogGlobals, .global = "LandNumber", .script = "LandControlT"},
	};
	PressF2AndAnswer(trip, k_QuestionNo, k_BeforeTheKey);
	PressF2AndAnswer(trip, k_QuestionYes, k_BeforeTheKey);
	trip.push_back(WaitTask(k_IslandControlScript));
	trip.push_back(LogGlobals("LandNumber TutorialLevel", k_IslandControlScript, 5.0f));
	all.push_back({
	    .id = "story.f2_playground",
	    .name = "Land 1: F2 to the Gods' Playground",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start of the first land's set-up (quest=SetupLand1 on Land1.txt). Ten "
	                   "seconds after the set-up, F2 is pressed and the question's No clicked; ten seconds later F2 is "
	                   "pressed again and its Yes clicked, with the pointer on each arrow and the Move button.",
	    .expected = "Each F2 shows \"Are you sure you want to go to the Gods' Playground?\" with Yes and No and pauses "
	                "the game. No takes the box and the pause away. Yes saves into slot 20 (with a profile), stops the "
	                "scripts, loads the tutorial island and starts its control script, whose first lesson begins.",
	    .commands = std::move(trip),
	    // the question pauses the game until it is answered, and the answer is the timeline's
	    .clockWhilePaused = true,
	    .onLand = true,
	});
}
