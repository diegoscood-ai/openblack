/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a temple being destroyed, of the blows its heart takes or passes on, and of a creature
// taking a sacrifice to its temple's altar, and of the colours a land gives its players' temples' borders

#include <array>
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

/// Where the town centre stands for the sacrifice, far enough north of the temple for a long walk to its altar
constexpr glm::vec2 k_FarCentre = k_Temple + glm::vec2 {0.0f, 90.0f};
/// The first of the town's people, by its place in the scenario's objects
constexpr size_t k_FirstVillager = 3;
/// The creature's action of the game's table that sacrifices a living thing, by its row
constexpr size_t k_Sacrifice = 282;

/// A Celtic town centre of the player's, two huts and eight of its people about it
std::vector<ObjectSetup> Town(glm::vec2 centre)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = centre},
	    {.type = AbodeInfo::CelticHut, .offset = centre + glm::vec2(-16.0f, 6.0f), .yawDegrees = 160.0f},
	    {.type = AbodeInfo::CelticHut, .offset = centre + glm::vec2(16.0f, 6.0f), .yawDegrees = 200.0f},
	};
	constexpr std::array<VillagerInfo, 4> k_People {VillagerInfo::CelticHousewifeFemale, VillagerInfo::CelticFarmerMale,
	                                                VillagerInfo::CelticForesterMale, VillagerInfo::CelticShepherdMale};
	for (size_t i = 0; i < 8; ++i)
	{
		const auto across = static_cast<float>(i % 4) * 5.0f - 7.5f;
		const auto down = i < 4 ? -10.0f : -15.0f;
		objects.push_back(
		    {.type = k_People.at(i % k_People.size()), .offset = centre + glm::vec2(across, down), .joinTown = true});
	}
	return objects;
}

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
	                "alignment by 0.1 the same way, and empties its reminder script and arguments. The SNAPSHOT and "
	                "the picture's call each ask for the challenge's picture, the update does not (pictures asked 1, "
	                "1, 2), each taken of the next frame drawn; nothing on the land changes.",
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
	                "shows only its bronze frame. Each picture shows the land as it was drawn when its snapshot was "
	                "taken, in low detail. The click on the first picture turns the room's camera to it with a woosh: it "
	                "looks at the picture from 25 units out, the picture is drawn in high detail, and the first "
	                "challenge's title, its time taken and 50% success are written at the top of the screen.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands = {{.kind = Command::Kind::ScriptSnapshot, .delaySeconds = 0.5f, .value = 1, .amount = 0.5f, .subtype = 2427},
	                 {.kind = Command::Kind::ScriptSnapshot,
	                  .delaySeconds = 0.0f,
	                  .value = 2,
	                  .amount = 1.0f,
	                  .subtype = 2429,
	                  .quest = true},
	                 {.kind = Command::Kind::OpenTempleRooms, .delaySeconds = 0.5f},
	                 {.kind = Command::Kind::PressKey,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM)},
	                 {.kind = Command::Kind::PointerTo, .delaySeconds = 7.0f, .point = k_FirstPicture},
	                 {.kind = Command::Kind::PointerPress, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Command::Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1},
	                 {.kind = Command::Kind::PointerTo, .delaySeconds = 1.0f, .point = k_FirstPicture}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	    .clockWhilePaused = true,
	});

	// Four challenges recorded by a script's snapshots, each with the camera over a different place of the land: The Sea
	// and The Ogre under way, The Lost Brother done, and The Riddles a quest half done
	const std::vector<Command> challengeSnapshots {
	    {.kind = Command::Kind::ScriptSnapshot,
	     .delaySeconds = 1.0f,
	     .point = {-50.0f, -50.0f},
	     .value = 1,
	     .amount = 0.25f,
	     .subtype = 2427},
	    {.kind = Command::Kind::ScriptSnapshot,
	     .delaySeconds = 0.5f,
	     .point = {200.0f, -150.0f},
	     .value = 3,
	     .amount = 0.0f,
	     .subtype = 2429},
	    {.kind = Command::Kind::ScriptSnapshot,
	     .delaySeconds = 0.5f,
	     .point = {-250.0f, 200.0f},
	     .value = 15,
	     .amount = 1.0f,
	     .subtype = 2437},
	    {.kind = Command::Kind::ScriptSnapshot,
	     .delaySeconds = 0.5f,
	     .point = {150.0f, 250.0f},
	     .value = 16,
	     .amount = 0.5f,
	     .subtype = 2438,
	     .quest = true},
	};

	all.push_back({
	    .id = "temple.challenge_room",
	    .name = "The temple's challenge room with four challenges, one replayed",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle. A script records four challenges, the camera over "
	                   "a different place of the land for each: The Sea and The Ogre under way, The Lost Brother done, "
	                   "and The Riddles a quest half done. The challenge room's key takes you into the challenge room; at "
	                   "8 seconds the pointer clicks on the first challenge's picture, and 3 seconds later on it again.",
	    .expected = "The scroll lists the four titles (four discovered, one completed). The first four slots show their "
	                "pictures in silver frames but The Riddles' gold one; every one but The Lost Brother's has its "
	                "button. The first click turns the camera to the first picture with a woosh, and the challenge's "
	                "title and its time taken and success are written at the top of the screen, fading in as the camera "
	                "comes close. The second click fades the screen to white with a woosh; once it is white you leave "
	                "the temple, and the camera is where it was over the first challenge's place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands =
	        [&challengeSnapshots] {
		        auto commands = challengeSnapshots;
		        commands.push_back({.kind = Command::Kind::OpenTempleRooms, .delaySeconds = 0.5f});
		        commands.push_back({.kind = Command::Kind::PressKey,
		                            .delaySeconds = 0.0f,
		                            .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_CHALLENGE_ROOM)});
		        for (const float delay : {7.0f, 3.0f})
		        {
			        commands.push_back({.kind = Command::Kind::PointerTo, .delaySeconds = delay, .point = k_FirstPicture});
			        commands.push_back({.kind = Command::Kind::PointerPress, .delaySeconds = 0.5f, .value = 1});
			        commands.push_back({.kind = Command::Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1});
		        }
		        return commands;
	        }(),
	    .fixtures = {.temples = {{.at = k_Temple}}},
	    .clockWhilePaused = true,
	});

	all.push_back({
	    .id = "temple.challenge_markers",
	    .name = "The temple's map marking the challenges not done",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle. The same four challenges as the challenge room's "
	                   "scenario are recorded, then the main room's key takes you into the temple's main room. Half a "
	                   "second in, a click at the top of the screen cuts the camera's way in short, and the pointer then "
	                   "rests on the screen's bottom edge, which leans the camera down to the pool.",
	    .expected = "The camera leans down over the pool until it looks down at the island's map (about five seconds). "
	                "With the challenges' button by the pool on, the map marks the three challenges not done (not The "
	                "Lost Brother) with the white challenge marker, turning, at the places the camera looked at for "
	                "their pictures; with no glow under them, as the game draws their glow at the challenge's place in "
	                "the world instead.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}}},
	    .commands =
	        [&challengeSnapshots] {
		        auto commands = challengeSnapshots;
		        commands.push_back({.kind = Command::Kind::PressKey,
		                            .delaySeconds = 0.5f,
		                            .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_INSIDE_TEMPLE)});
		        // A click takes the camera over from its way into the room, as the player's does, on the dome where it
		        // presses nothing; then the screen's bottom edge leans the camera down to the pool and its map
		        commands.push_back({.kind = Command::Kind::PointerTo, .delaySeconds = 0.3f, .point = {0.5f, 0.05f}});
		        commands.push_back({.kind = Command::Kind::PointerPress, .delaySeconds = 0.2f, .value = 1});
		        commands.push_back({.kind = Command::Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1});
		        commands.push_back({.kind = Command::Kind::PointerTo, .delaySeconds = 0.2f, .point = {0.5f, 1.0f}});
		        return commands;
	        }(),
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
	                 {.kind = Command::Kind::OpenTempleRooms, .delaySeconds = 0.5f},
	                 {.kind = Command::Kind::PressKey,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_LIBRARY)},
	                 {.kind = Command::Kind::LookAtTempleScroll, .delaySeconds = 4.0f, .value = 6}},
	    .fixtures = {.temples = {{.at = k_Temple}}},
	    // the temple pauses the game
	    .clockWhilePaused = true,
	});

	all.push_back({
	    .id = "objects.creature_sacrifice",
	    .name = "The creature takes a villager to the altar",
	    .facet = Facet::Objects,
	    .description = "A grown tiger of the player's stands by a Celtic town of the player's, far north of the player's "
	                   "temple. After a second the town's worship site is built at the temple, as a map script's command "
	                   "builds it. Two seconds later a script tells the tiger to sacrifice the first of the town's people.",
	    .expected = "The tiger picks the villager up with an angry face, and looks at the villager as it goes to it. It "
	                "walks south with the villager in its hand and looks ahead. It stops three of its heights from the "
	                "altar of the worship site, turns to face the altar and looks at it. Then it puts the villager down. "
	                "The villager lands alive and walks away: as in the original game, the altar does not take it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Temple - glm::vec2 {40.0f}, k_FarCentre + glm::vec2 {30.0f}}},
	    .creatures = {{.offset = k_FarCentre + glm::vec2(0.0f, -30.0f)}},
	    .objects = Town(k_FarCentre),
	    .commands = {{.kind = Command::Kind::CreateWorshipSite,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(Tribe::CELTIC)},
	                 {.kind = Command::Kind::ScriptDoAction,
	                  .creature = 0,
	                  .delaySeconds = 2.0f,
	                  .value = k_Sacrifice,
	                  .object = k_FirstVillager}},
	    // As a land's script after it makes the player's town: the most the town's belief cap lets it believe
	    .townBelief = 10.0f,
	    .fixtures = {.temples = {{.at = k_Temple}}},
	});

	// Three players' temples side by side, built whole so that their borders show, on the second land: it gives the
	// second and third players the colours of the fifth and sixth, and leaves the first its own
	all.push_back({
	    .id = "worship.land_colour_index",
	    .name = "The second land draws its players in other players' colours",
	    .facet = Facet::Objects,
	    .description = "The land number is set to 2, as the second land's script sets it. Three temples stand built in a "
	                   "row north of the middle, 150 m apart: the player's in the west, the second player's in the middle "
	                   "and the third player's in the east.",
	    .expected = "Each temple's influence border shows round it. The player's is red, as on every land. The second "
	                "player's is yellow, not its own green, and the third player's blue, not its own magenta: the second "
	                "land draws them in the fifth and sixth players' colours. The log says the land number is 2; once the "
	                "scenario stops the testbed's own land number is back.",
	    .environment = {.dispenserGrid = false, .landNumber = 2},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {220.0f, 60.0f}, k_Temple + glm::vec2 {220.0f, 60.0f}}},
	    .fixtures = {.temples = {{.at = k_Temple - glm::vec2 {150.0f, 0.0f}},
	                             {.at = k_Temple, .owner = PlayerNames::PLAYER_TWO},
	                             {.at = k_Temple + glm::vec2 {150.0f, 0.0f}, .owner = PlayerNames::PLAYER_THREE}}},
	});
}
