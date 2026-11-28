/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Land 1's creature guide's food lesson (the guide's chapter on impressing the Aztec village) as a quest run, played
// after a land entry's quest= has started the lesson on the game's own Land 1 (docs/wiki/project/scenarios.md,
// "Playing a timeline on a land"). The lesson makes the guide itself and lays its gold scroll by the village; the
// timeline presses on the scroll with the hand and clicks the texts that wait for a click. Its film then shows a food
// one-shot miracle, plays the recorded hand demonstration that taps it and pours it on the village store, and makes
// the guide's own miracle for the player to try. The timeline waits past the film's last line, so that the demo's
// check of the hand and its pour are both in the run's log. The points are map coordinates, taken from the places the
// lesson's script names.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::TaskWait;
using testbed_land_timeline::ThingNear;

/// The Action (right) button
constexpr size_t k_ActionButton = 3;
/// Where the camera is put for the scroll: this far above the land and back from it
constexpr float k_EyeAbove = 25.0f;
constexpr float k_EyeBack = 30.0f;
/// How near the hand's press looks for the scroll it presses on
constexpr float k_ScrollPickRadius = 15.0f;

/// The lesson's script
constexpr std::string_view k_LessonScript = "GuideImpressTownLesson";
/// The lesson's gold scroll, 15 m east of the place the lesson impresses the village from
constexpr glm::vec2 k_Scroll {2436.95f, 2634.93f};
/// The globals and scripts the lesson's progress is read by
constexpr std::string_view k_LessonGlobals = "GuideNotImpressing StopPopulating";
constexpr std::string_view k_LessonScripts = "GuideImpressTownLesson ImpressFood GuideCreateMiracle PopulateTown";
/// How long the timeline waits after the film's last line, in seconds: the film's camera, the guide's new miracle
/// and the lesson's first check of the hand
constexpr float k_AfterFilmSeconds = 10.0f;

Command WaitTask(std::string_view script, TaskWait wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = static_cast<size_t>(wait), .script = script};
}

Command WaitText(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true};
}

/// Logs the lesson's globals and scripts, some seconds after the command before it
Command LogLesson(float delay)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .global = k_LessonGlobals, .script = k_LessonScripts};
}

/// The camera put back from a point and above it, looking at it; both heights are above the land there, as the
/// timeline's camera takes them
Command CameraOn(glm::vec2 point)
{
	return {.kind = Kind::CameraTo,
	        .delaySeconds = 1.0f,
	        .eye = {point.x, k_EyeAbove, point.y - k_EyeBack},
	        .look = {point.x, 0.0f, point.y}};
}

/// The gold scroll pressed on with the hand: the pointer on the drawn scroll, then the Action button pressed once the
/// hand is over it and let go, as a player clicks it
void PressScroll(std::vector<Command>& commands, glm::vec2 scroll)
{
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = scroll, .value = 1});
	commands.push_back(
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = k_ActionButton,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = scroll, .radius = k_ScrollPickRadius}});
	commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = k_ActionButton});
}
} // namespace

void testbed_scenarios::AddQuestGuideImpressTownScenarios(std::vector<Scenario>& all)
{
	// The lesson lays its scroll once it has made the guide; the film starts when the scroll is clicked. The texts are
	// clicked on throughout. The hand demonstration plays inside the film, between its lines 4004 and 4005
	std::vector<Command> lesson {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitTask(k_LessonScript, TaskWait::Running),
	    CameraOn(k_Scroll),
	};
	PressScroll(lesson, k_Scroll);
	lesson.push_back(LogLesson(2.0f));
	lesson.push_back(WaitText(4005));
	lesson.push_back(LogLesson(0.0f));
	lesson.push_back(WaitText(4006));
	lesson.push_back(WaitText(4007));
	lesson.push_back(LogLesson(k_AfterFilmSeconds));
	all.push_back({
	    .id = "quest.guide_impress_town",
	    .name = "Land 1 quest run: the creature guide's food lesson",
	    .facet = Facet::Story,
	    .description = "Played after a quest run's start (quest=SetupLand1/GuideImpressTownLesson on Land1.txt). The "
	                   "texts that wait for a click are clicked on. Once the lesson runs, the camera is put on its gold "
	                   "scroll by the Aztec village and the scroll is pressed on with the hand; the film then plays the "
	                   "recorded food hand demonstration (castfood). The timeline waits for the film's lines after the "
	                   "demonstration and 10 seconds past its last, logging the lesson's state.",
	    .expected = "The guide made by the village and its scroll laid; a food one-shot miracle by the village; the "
	                "demonstration's hand taps it, so the miracle goes into the hand and the one on the ground goes, and "
	                "pours the food on the village store; then the guide makes its own miracle for the player to try. "
	                "Unverified: the lesson has not yet been run in a game.",
	    .commands = std::move(lesson),
	    .onLand = true,
	});
}
