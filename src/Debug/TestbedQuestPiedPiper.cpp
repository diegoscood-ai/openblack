/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/
// Land 1's silver scroll The Pied Piper played past its introduction, as land timelines on the game's own Land 1
// (docs/wiki/project/scenarios.md, "Playing a timeline on a land"). They follow story.c_glade_newborn on the SkipBox's
// answer 2, which skips the tutorial and the creature's training but not its choice: the player's creature is then a
// newborn chosen in the glade, at the size every new creature starts at (0.22), as a natural game reaches the scroll
// with a young creature. With answer 3 the profile's creature is kept, and the default profile's is full grown (size
// 2): at 1.6 or more a creature can't find its way out of its pen to the creche through the ring of trees round it,
// in the original too (docs/wiki/beings/routes.md#the-creatures-walk-on-the-follower), so the quest stalls there.
// After the scroll's tap and
// the introduction, once the piper pipes the player's creature's leash is tied to him, which is how he is caught: the
// quest walks the creature up to him and it grabs him. Then it is moved to his cave and puts him down (the good
// ending), or eats him (the evil one). A third timeline puts the camera beside the creche woman, clicks on the piper,
// then has the creature come up to him untied, so he runs and hides in his cave, and go off again, so he comes out.
// The creature is moved by a script's move rather than told to walk: at this hour of the land it stays at its home
// point whatever walk it is told. The points are map coordinates, taken from the places the quest's script names.

#include <cstddef>
#include <cstdint>

#include <string_view>
#include <utility>
#include <vector>

#include "ScriptHeaders/ScriptEnums.h"
#include "TestbedLandTimeline.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using testbed_land_timeline::Compare;
using testbed_land_timeline::ThingNear;

/// The waits for a task of a script: running, or none running after one was seen
constexpr size_t k_Running = 0;
constexpr size_t k_Finished = 2;

/// The scroll at the Norse creche
constexpr glm::vec2 k_Creche {1868.01f, 2730.12f};
/// Where the piper stands to pipe, near the creche
constexpr glm::vec2 k_PipingSpot {1886.318f, 2736.75f};
/// A spot about 27 from the piping spot, on the side away from his cave: close enough for him to run from the creature
constexpr glm::vec2 k_NearThePiper {1870.0f, 2715.0f};
/// A spot about 65 from the piping spot and well off his way from the cave to it, so that he comes out and pipes again
constexpr glm::vec2 k_AwayFromThePiper {1840.0f, 2690.0f};
/// A spot about 40 from the piping spot, on the same side: beyond the 30 he runs from, and near enough for a newborn,
/// which walks at 3.5 a second or more, to be walked up to him within the 20 seconds the leash holds for
constexpr glm::vec2 k_ByThePiper {1858.1f, 2708.3f};
/// A spot about 20 out from the cave's mouth towards the creche, within 35 of the back of the cave where the script
/// lets him go free
constexpr glm::vec2 k_ByTheCave {1953.0f, 2795.0f};

/// The young ape in the glade beyond the gates, where the glade's script makes it
constexpr ThingNear k_GladeApe {.type = script::ObjectType::Creature, .point = {2233.02f, 3152.32f}, .radius = 2.0f};

/// Where the creche woman wanders
constexpr glm::vec2 k_CrecheWoman {1858.02f, 2728.16f};

/// The piper, a villager of his own kind, where he pipes
constexpr ThingNear k_Piper {.type = script::ObjectType::Villager, .subtype = 63, .point = k_PipingSpot, .radius = 10.0f};

Command WaitTask(std::string_view script, size_t wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = wait, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

Command WaitGlobal(std::string_view global, Compare compare, float value)
{
	return {.kind = Kind::WaitGlobal, .waitUntilFree = true, .amount = value, .global = global, .compare = compare};
}

/// Logs the globals and scripts named, some seconds after the command before it, and again every period when given one
Command LogGlobals(std::string_view globals, std::string_view scripts, float delay = 0.0f, size_t period = 0)
{
	return {.kind = Kind::LogGlobals, .delaySeconds = delay, .value = period, .global = globals, .script = scripts};
}

/// A script's move of the player's creature to a point of the land, under the script's control until released
Command ScriptMove(glm::vec2 point, float delay)
{
	return {.kind = Kind::ScriptMove, .delaySeconds = delay, .point = point};
}

/// The player's creature given back to its own mind once it has arrived
Command ScriptRelease()
{
	return {.kind = Kind::ScriptRelease, .waitUntilFree = true};
}

/// The quest's globals and its scripts, logged at the end and every 20 seconds meanwhile, so a stalled run shows where
/// each script waits
constexpr std::string_view k_Globals =
    "PiedPiperInitiated PiedPiperFinished PiedPiperCompletedNow PiedPiperChildrenInCave PiedPiperFlockSize "
    "PiperCaughtStory";
constexpr std::string_view k_Scripts = "PiedPiper PiedPiperIntelligence PiperFleeFromCreature PiperLeashed PiperCaught "
                                       "PiperSetFree SendTheKidsHome PiperKilledByCreature RewardForPiper";

/// The scroll's tap and the introduction, then the quest's globals logged every 20 seconds
std::vector<Command> Introduction()
{
	return {
	    {.kind = Kind::ClickTexts, .value = 1},
	    WaitTask("PiedPiper", k_Running),
	    // The scroll's tap: the camera 70 south of it and 45 up, the hand on it, then the Action button pressed once the
	    // hand is over it, as the scroll's click test asks
	    {.kind = Kind::CameraTo,
	     .delaySeconds = 1.0f,
	     .eye = {k_Creche.x, 45.0f, k_Creche.y - 70.0f},
	     .look = {k_Creche.x, 5.0f, k_Creche.y}},
	    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = k_Creche, .value = 1},
	    {.kind = Kind::PointerPress,
	     .delaySeconds = 0.5f,
	     .waitUntilFree = true,
	     .value = 3,
	     .thing = ThingNear {.type = script::ObjectType::Highlight, .point = k_Creche, .radius = 15.0f}},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	    WaitText(1410, true),
	    LogGlobals(k_Globals, k_Scripts, 1.0f, 20),
	};
}

/// The introduction and the piper's catch, up to the creature holding him once the first catch's talk is read
std::vector<Command> UpToTheCatch()
{
	auto commands = Introduction();
	const std::vector<Command> chase {
	    // He pipes once a child has joined his flock. The creature is first brought from its home to a spot more than 30
	    // from him, which he does not run from, so that the quest's walk up to him once it is leashed to him takes less
	    // than the 20 seconds the leash holds for; then given back to its own mind. Its way there from the pen goes down
	    // to the shore and round, and it may stand a moment on the way, which ends the wait for it: the move is sent
	    // again twice, so that it is there before the leash is tied
	    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    // The creature's leash tied to him, as the player ties it: his round stops and he runs for his cave, the quest
	    // walks the creature up to him and it grabs him
	    {.kind = Kind::TieLeash, .delaySeconds = 1.0f, .thing = k_Piper},
	    WaitText(1427, true),
	};
	commands.insert(commands.end(), chase.begin(), chase.end());
	return commands;
}
} // namespace

void testbed_scenarios::AddQuestPiedPiperScenarios(std::vector<Scenario>& all)
{
	// The glade's choice on the SkipBox's answer 2: its short scene (no lines on the skip) let play, then the ape pressed
	// on twice to choose it, so that the quest's chapters after it have a newborn for the player's creature
	all.push_back({
	    .id = "story.c_glade_newborn",
	    .name = "Land 1: the choice in the glade on a skip to it (a newborn creature)",
	    .facet = Facet::Story,
	    .description = "Played first on the SkipBox's answer 2, before the Pied Piper's chapters. It lets the glade's "
	                   "scene play, then presses on the ape twice to choose it.",
	    .expected = "The ape pressed on twice is chosen (ChooseYourCreatureFinished becomes 1); the player's creature "
	                "is a newborn of size 0.22 and is put at its home point by the pen.",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            WaitTask("CreaturesInGlade", k_Running),
	            // the scene's camera goes from the gates to the glade and the creatures play, about a minute on the skip;
	            // the presses come once it is over and the hand is the player's again
	            {.kind = Kind::PointerTo, .delaySeconds = 90.0f, .thing = k_GladeApe},
	            {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .waitUntilFree = true, .value = 3, .thing = k_GladeApe},
	            {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	            WaitGlobal("ChooseYourCreatureFinished", Compare::Equal, 1.0f),
	        },
	    .onLand = true,
	});
	{
		auto commands = UpToTheCatch();
		// Given back to the player, the creature is moved to his cave with him in its hand and puts him down there
		commands.push_back(ScriptMove(k_ByTheCave, 2.0f));
		commands.push_back(ScriptRelease());
		commands.push_back({.kind = Kind::PutDown, .delaySeconds = 0.2f});
		commands.push_back(WaitText(1418, true));
		commands.push_back(WaitTask("RewardForPiper", k_Finished));
		commands.push_back(WaitTask("SendTheKidsHome", k_Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper",
		    .name = "Land 1: The Pied Piper (caught and set free)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction; once the piper pipes, the "
		                   "creature is moved to a spot 40 from him and its leash is tied to him, so it is walked up to "
		                   "him and grabs him; it is then moved to his cave with him and puts him down.",
		    .expected = "Leashed, his round stops, he panics and the creature is walked up to him and grabs him (texts "
		                "1422 to 1427); put down by his cave he frees the children "
		                "(texts 1415 to 1418), joins the village and the miracle dispenser reward is built by the creche: "
		                "PiedPiperFinished=1, PiedPiperCompletedNow=1.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = UpToTheCatch();
		// Given back to the player, the creature eats him
		commands.push_back({.kind = Kind::EatHeld, .delaySeconds = 2.0f});
		commands.push_back(WaitTask("PiperKilledByCreature", k_Finished));
		commands.push_back(WaitTask("RewardForPiper", k_Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_eaten",
		    .name = "Land 1: The Pied Piper (caught and eaten)",
		    .facet = Facet::Story,
		    .description = "As story.q_pied_piper up to the creature holding the piper, then it eats him.",
		    .expected = "He stops existing in the creature's hand: the advisors' film (texts 1420, 1421) and the "
		                "lightning miracle dispenser reward by the creche; PiedPiperFinished=1, PiedPiperCompletedNow=1, "
		                "and the children stay in the cave.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = Introduction();
		const std::vector<Command> asides {
		    // The camera beside the creche woman for 15 seconds, as her sobs need it within 10 of her
		    {.kind = Kind::CameraTo,
		     .delaySeconds = 1.0f,
		     .eye = {k_CrecheWoman.x, 4.0f, k_CrecheWoman.y},
		     .look = {k_CrecheWoman.x, 1.0f, k_CrecheWoman.y + 10.0f}},
		    // Then on the piper's spot, and once he pipes the Action button pressed on him: a click, as the player's
		    {.kind = Kind::CameraTo,
		     .delaySeconds = 15.0f,
		     .eye = {k_PipingSpot.x, 30.0f, k_PipingSpot.y - 50.0f},
		     .look = {k_PipingSpot.x, 3.0f, k_PipingSpot.y}},
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_Piper},
		    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_Piper},
		    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
		    // Then the creature brought within 30 of him, untied: he runs and hides in his cave; taken more than 30 away
		    // again, he comes out and goes piping again
		    ScriptMove(k_NearThePiper, 2.0f),
		    WaitText(1434, true),
		    ScriptMove(k_AwayFromThePiper, 1.0f),
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    LogGlobals(k_Globals, k_Scripts, 5.0f),
		};
		commands.insert(commands.end(), asides.begin(), asides.end());
		all.push_back({
		    .id = "story.q_pied_piper_asides",
		    .name = "Land 1: The Pied Piper (the woman's sobs, a click on him, his flight)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction, then puts the camera "
		                   "beside the creche woman for 15 seconds, then on the piper's spot and, once he pipes, clicks "
		                   "on him; then moves the creature within 30 of him and, once he has hidden, more than 30 away.",
		    .expected = "She sobs (one of seven sounds) with the camera beside her; clicked, he says his prodding line "
		                "as a sound from him, without the dialogue box; he runs from the creature and hides (texts 1432 "
		                "to 1434), and comes out to pipe again once it is far.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
}
