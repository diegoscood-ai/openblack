/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a temple being destroyed, and of the blows its heart takes or passes on

#include <vector>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The temple stands north of the middle
constexpr glm::vec2 k_Temple {0.0f, 40.0f};
/// Where the challenge room's first picture is on the screen, as the room's camera first frames it, as fractions of
/// the screen from the top left
constexpr glm::vec2 k_FirstPicture {0.425f, 0.72f};

/// A boulder thrown at the temple from 30 m south of it, up and along, every six seconds
ThrowSetup AtTheTemple()
{
	return {
	    .object = 0,
	    .from = k_Temple - glm::vec2 {0.0f, 30.0f},
	    .height = 6.0f,
	    .velocity = {0.0f, 9.0f, 16.0f},
	    .delaySeconds = 2.0f,
	    .repeatSeconds = 6.0f,
	};
}
} // namespace

void testbed_scenarios::AddTempleScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "temple.deleted_by_script",
	    .name = "Your temple deleted by a script with its explosion",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle. After 2 seconds a script's OBJECT_DELETE takes its "
	                   "heart with the temple's explosion (mode 3); 3 seconds later the same again.",
	    .expected = "The log says the first delete starts the destruction and the second only breaks the heart's model "
	                "up, the destruction going on. From the next turn the temple's loop sound plays over it and plasma "
	                "beams leap between points of its upper surface, more and more often, for 14 seconds (the beams "
	                "draw nothing yet). Your heartbeat quickens and is held fast. At 10 seconds a glow comes over the "
	                "temple for 6 seconds; at 21.5 the loop gives way to the explosion's sound and the citadel's "
	                "explosion is seen; at 22 a large evil smoke rises and the temple goes, with its entrance and its "
	                "doves or bats, and your heartbeat is let go. Your temple being destroyed ends the game: the "
	                "GameOver script is started once, on the first turn. The heart's fade and the shell over it are not "
	                "drawn yet.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands = {{.kind = Command::Kind::ScriptDeleteTemple, .delaySeconds = 2.0f},
	                 {.kind = Command::Kind::ScriptDeleteTemple, .delaySeconds = 3.0f}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	all.push_back({
	    .id = "script.challenge_snapshots",
	    .name = "A script's snapshots keep a challenge's record",
	    .facet = Facet::Examples,
	    .description = "After 1 second a script's SNAPSHOT starts the record of challenge 90 at success 0.25 and "
	                   "alignment 0.5, with one argument for its reminder script. After 1 more second an "
	                   "UPDATE_SNAPSHOT brings it to success 1 at alignment -0.75; after another an "
	                   "UPDATE_SNAPSHOT_PICTURE gives it success 0.3 at alignment 0.1, taking its picture.",
	    .expected = "The log lists one record after each call. The first leaves your alignment as it was; the update "
	                "moves it at once towards evil by -0.75 times the game's script scale, damped or boosted by half "
	                "your alignment, and the record is done. The picture's call keeps it done (success 1), moves your "
	                "alignment by 0.1 the same way, and empties its reminder script and arguments. No picture is "
	                "taken yet, and nothing on the land changes.",
	    .environment = {.dispenserGrid = false},
	    .commands = {{.kind = Command::Kind::ScriptChallengeSnapshot,
	                  .delaySeconds = 1.0f,
	                  .value = 0,
	                  .amount = 0.25f,
	                  .alignment = 0.5f},
	                 {.kind = Command::Kind::ScriptChallengeSnapshot,
	                  .delaySeconds = 1.0f,
	                  .value = 1,
	                  .amount = 1.0f,
	                  .alignment = -0.75f},
	                 {.kind = Command::Kind::ScriptChallengeSnapshot,
	                  .delaySeconds = 1.0f,
	                  .value = 2,
	                  .amount = 0.3f,
	                  .alignment = 0.1f}},
	});

	all.push_back({
	    .id = "temple.blows_on_the_heart",
	    .name = "A boulder thrown at your temple again and again",
	    .facet = Facet::Physics,
	    .description = "Your temple stands built north of the middle, and you have no building and no homeless villager. "
	                   "A boulder is thrown at it from 30 m south every six seconds, as the hand throws it.",
	    .expected = "The boulder strikes the temple and comes off it: the temple's heart is a body of the physics now, "
	                "of mass 10000, that nothing moves. With nothing of yours to pass the blow on to, the heart "
	                "takes it, a hit of its speed times its weight, at most a fifth of the heart's life a blow, put down "
	                "to you; it beams at a point on itself (at most every two seconds; the beams draw nothing yet). Once "
	                "hurt, it beams across itself every turn it is due, the oftener the less life it has, with one of "
	                "five spark sounds in turn from six tenths of its life down. When its life reaches nothing its "
	                "destruction starts, as with the script's delete, and the game's end with it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .objects = {{.type = MobileStaticInfo::Boulder1Lime, .offset = k_Temple - glm::vec2 {0.0f, 30.0f}}},
	    .throws = {AtTheTemple()},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	all.push_back({
	    .id = "temple.blow_passed_to_a_hut",
	    .name = "Your temple passes a boulder's blow on to your hut",
	    .facet = Facet::Physics,
	    .description = "Your temple stands built north of the middle and a Celtic hut of your town 50 m east of it. A "
	                   "boulder is thrown at the temple from 30 m south every six seconds, as the hand throws it.",
	    .expected = "The boulder strikes the temple and comes off it, but the temple's heart is not hurt: it passes the "
	                "blow on to the hut, the first of your buildings standing with life, which takes it as if struck "
	                "there by the boulder, breaking as a struck building breaks. The heart beams at the hut, from its top "
	                "to the hut's, with one of five spark sounds in turn, at most every two seconds (the beam draws "
	                "nothing yet).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {70.0f}}},
	    .objects = {{.type = MobileStaticInfo::Boulder1Lime, .offset = k_Temple - glm::vec2 {0.0f, 30.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = k_Temple + glm::vec2 {50.0f, 0.0f}}},
	    .throws = {AtTheTemple()},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	all.push_back({
	    .id = "temple.challenge_choose",
	    .name = "The challenge room's pictures of the challenges found",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle. Half a second in, a script takes snapshots of two "
	                   "challenges: the first half done, the second a quest already won. A second in, the challenge "
	                   "room's key takes you into the challenge room; at 8 seconds the pointer goes to the first "
	                   "challenge's picture and clicks on it.",
	    .expected = "The room shows only the two challenges' slots: the first with its picture in a silver frame and its "
	                "button, the second with its picture in a gold frame and no button (it is won); every other slot "
	                "shows only its bronze frame. The click on the first picture turns the room's camera to it with a "
	                "woosh: it looks at the picture from 25 units out.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands = {{.kind = Command::Kind::ScriptSnapshot, .delaySeconds = 0.5f, .value = 1, .amount = 0.5f},
	                 {.kind = Command::Kind::ScriptSnapshot, .delaySeconds = 0.0f, .value = 2, .amount = 1.0f, .quest = true},
	                 {.kind = Command::Kind::PressKey,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM)},
	                 {.kind = Command::Kind::PointerTo, .delaySeconds = 7.0f, .point = k_FirstPicture},
	                 {.kind = Command::Kind::PointerPress, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Command::Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1},
	                 {.kind = Command::Kind::PointerTo, .delaySeconds = 1.0f, .point = k_FirstPicture}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	    .clockWhilePaused = true,
	});

	all.push_back({
	    .id = "temple.library_history",
	    .name = "The temple's library with tips read and a story history",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle. Scripts show seven help texts of different "
	                   "speakers: the big voice, a man, Khazar, a woman and the two advisors. The player reads four tips: "
	                   "two of village life, one of the creature and one of did you know. Then the library's key takes "
	                   "the player into the library. Four seconds later the camera looks at the last scroll (Story "
	                   "History), as a press on it does.",
	    .expected = "The village life scroll lists its two tips in the order read, the creature scroll and the did you "
	                "know scroll one tip each, the other help scrolls only their titles. Close to the Story History "
	                "scroll, its text shows the five newest lines, the newest first: who said each line, then the line. "
	                "Each line after the first starts with how many lines ago it was. The buttons Previous Line (left "
	                "arrow, bottom left), Latest Line (bottom middle) and Next Line (right arrow, bottom right) fade "
	                "in. Previous Line shows one older line first, Next Line one newer line, and Latest Line the newest "
	                "line. The buttons fade out as the camera goes back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands = {{.kind = Command::Kind::ShowText, .delaySeconds = 0.5f, .value = 4411},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 1250},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 1717},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 1240},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 6675},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 6676},
	                 {.kind = Command::Kind::ShowText, .delaySeconds = 0.2f, .value = 4411},
	                 {.kind = Command::Kind::ReadTip, .delaySeconds = 0.2f, .value = 4119, .amount = 2.0f},
	                 {.kind = Command::Kind::ReadTip, .delaySeconds = 0.2f, .value = 4120, .amount = 2.0f},
	                 {.kind = Command::Kind::ReadTip, .delaySeconds = 0.2f, .value = 4121, .amount = 1.0f},
	                 {.kind = Command::Kind::ReadTip, .delaySeconds = 0.2f, .value = 4122, .amount = 4.0f},
	                 {.kind = Command::Kind::PressKey,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_LIBRARY)},
	                 {.kind = Command::Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 6}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	    // the temple pauses the game
	    .clockWhilePaused = true,
	});
}
