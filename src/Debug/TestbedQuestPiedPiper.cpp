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
// ending), or eats him, or throws him into the sea (the evil ones). Another is put down away from the cave first, so
// he runs back to it, and is caught a second time. A third timeline puts the camera beside the creche woman, clicks on
// the piper, takes a child he is calling out of the way with the hand, then has the creature come up to him untied, so
// he runs and hides in his cave, and go off again, so he comes out. In another the leash is tied to him with the
// creature still at its home, too far to reach him before the leash's time runs out. The last one stands in for the
// long wait until thirty children are shut in his cave.
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
/// A spot on the beach below the creature's pen, on dry land at the water's edge, and a point of the sea about 32
/// beyond it (from the land's water cells)
constexpr glm::vec2 k_OnTheBeach {1805.0f, 2470.0f};
constexpr glm::vec2 k_InTheSea {1785.0f, 2445.0f};
/// A spot about 26 short of where he pipes, seen by a camera on his spot, where a child taken from him is put down
constexpr glm::vec2 k_ChildPutDown {1895.0f, 2712.0f};

/// The young ape in the glade beyond the gates, where the glade's script makes it
constexpr ThingNear k_GladeApe {.type = script::ObjectType::Creature, .point = {2233.02f, 3152.32f}, .radius = 2.0f};

/// Where the creche woman wanders
constexpr glm::vec2 k_CrecheWoman {1858.02f, 2728.16f};

/// The piper, a villager of his own kind, where he pipes
constexpr ThingNear k_Piper {.type = script::ObjectType::Villager, .subtype = 63, .point = k_PipingSpot, .radius = 10.0f};

/// The creche woman, a villager of her kind about the spot she wanders; and the creche, the abode at the scroll's point
constexpr ThingNear k_CrecheLeader {
    .type = script::ObjectType::Villager, .subtype = 49, .point = k_CrecheWoman, .radius = 8.0f};
constexpr ThingNear k_CrecheHouse {.type = script::ObjectType::Abode, .point = k_Creche, .radius = 10.0f};
/// The creche damaged below the health it had when the quest began
constexpr float k_DamagedCreche = 0.5f;

/// [shortcut] The creche woman killed, or the creche damaged, by a script's SET_PROPERTY of the health, in place of the
/// player's or the creature's blow
Command KillCrecheLeader(float delay)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = 0.0f, .thing = k_CrecheLeader};
}

Command DamageCreche(float delay)
{
	return {.kind = Kind::ScriptSetHealth, .delaySeconds = delay, .amount = k_DamagedCreche, .thing = k_CrecheHouse};
}

/// The child he has called, on its way to him: the one nearest his spot. It walks to him at about 3.6 a second from
/// about 27 away and joins his dance within 10, so about 3 seconds after it has been called it is 16 or so from him,
/// nearer than the children playing by the creche
constexpr ThingNear k_CalledChild {.type = script::ObjectType::VillagerChild, .point = k_PipingSpot, .radius = 18.0f};

/// After the script's wait until a text is read, it releases the creature; a command to the creature given before it
/// does is undone by that release
constexpr float k_AfterTheRelease = 1.0f;

/// Long enough for the creature's put-down to have let go of what it held
constexpr float k_PutDownSeconds = 3.0f;

Command WaitTask(std::string_view script, size_t wait)
{
	return {.kind = Kind::WaitTask, .waitUntilFree = true, .value = wait, .script = script};
}

Command WaitText(uint32_t text, bool read)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = read};
}

/// Waits for a text to be said to its end, as the script's own wait until read takes it
Command WaitSpoken(uint32_t text)
{
	return {.kind = Kind::WaitText, .waitUntilFree = true, .value = text, .read = true, .voice = true};
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

/// The creature moved to a point and given back to its own mind once it is there. It may stand a moment on the way,
/// which ends the wait for it, so the move is sent again
std::vector<Command> MoveTo(glm::vec2 point, float delay)
{
	return {ScriptMove(point, delay), ScriptRelease(), ScriptMove(point, 1.0f), ScriptRelease()};
}

void Append(std::vector<Command>& commands, const std::vector<Command>& more)
{
	commands.insert(commands.end(), more.begin(), more.end());
}

/// The quest's globals and its scripts, logged at the end and every 20 seconds meanwhile, so a stalled run shows where
/// each script waits
constexpr std::string_view k_Globals =
    "PiedPiperInitiated PiedPiperFinished PiedPiperCompletedNow PiedPiperChildrenInCave PiedPiperFlockSize "
    "PiperCaughtStory";
constexpr std::string_view k_Scripts = "PiedPiper PiedPiperIntelligence PiperFleeFromCreature PiperLeashed PiperCaught "
                                       "PiperSetFree SendTheKidsHome PiperKilledByCreature RewardForPiper "
                                       "PiedPiperKilled30Children";

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
	    // the film ends as its last line has been said, which the camera must wait for
	    WaitSpoken(1410),
	    LogGlobals(k_Globals, k_Scripts, 1.0f, 20),
	};
}

/// Once he pipes, the creature brought from its home to a spot more than 30 from him, which he does not run from, so
/// that the quest's walk up to him once it is leashed to him takes less than the 20 seconds the leash holds for; then
/// its leash tied to him, as the player ties it: his round stops and he runs for his cave, the quest walks the creature
/// up to him and it grabs him. Its way there from the pen goes down to the shore and round, and it may stand a moment
/// on the way, which ends the wait for it: the move is sent again twice, so that it is there before the leash is tied
std::vector<Command> Chase()
{
	return {
	    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    ScriptMove(k_ByThePiper, 1.0f),
	    ScriptRelease(),
	    {.kind = Kind::TieLeash, .delaySeconds = 1.0f, .thing = k_Piper},
	};
}

/// The introduction and the piper's catch, up to the creature holding him once the first catch's talk is over and the
/// script has given the creature back
std::vector<Command> UpToTheCatch()
{
	auto commands = Introduction();
	Append(commands, Chase());
	commands.push_back(WaitSpoken(1427));
	return commands;
}

/// Given back to the player, the creature is moved to his cave with him in its hand and puts him down there: he frees
/// the children and joins the village, and the reward is built
std::vector<Command> FreedAtTheCave()
{
	// Still under the script's move once it has arrived, it puts him down: given back to its own mind first, its next
	// plan would end the put-down before it lets go
	std::vector<Command> commands {ScriptMove(k_ByTheCave, k_AfterTheRelease)};
	commands.push_back({.kind = Kind::PutDown, .delaySeconds = 0.2f, .waitUntilFree = true});
	// The release waits for the walk and the body, not the hands: it is given once the put-down's animation is over
	auto release = ScriptRelease();
	release.delaySeconds = k_PutDownSeconds;
	commands.push_back(release);
	commands.push_back(WaitSpoken(1418));
	commands.push_back(WaitTask("RewardForPiper", k_Finished));
	commands.push_back(WaitTask("SendTheKidsHome", k_Finished));
	commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
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
	            // on the skip the glade has no scene of its own: the choice is listened for at once, and the creatures and
	            // the camera settle within about a minute; the presses come after that
	            {.kind = Kind::PointerTo, .delaySeconds = 90.0f, .thing = k_GladeApe},
	            // aimed again a second later, as the ape moves about: the hand has come over it by the press, whose
	            // release is what the choice hears
	            {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_GladeApe},
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
		Append(commands, FreedAtTheCave());
		all.push_back({
		    .id = "story.q_pied_piper",
		    .name = "Land 1: The Pied Piper (caught and set free)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction; once the piper pipes, the "
		                   "creature is moved to a spot 40 from him and its leash is tied to him, so it is walked up to "
		                   "him and grabs him; once the script gives it back, it is moved to his cave with him and puts "
		                   "him down.",
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
		commands.push_back({.kind = Kind::EatHeld, .delaySeconds = k_AfterTheRelease});
		commands.push_back(WaitTask("PiperKilledByCreature", k_Finished));
		commands.push_back(WaitTask("RewardForPiper", k_Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_eaten",
		    .name = "Land 1: The Pied Piper (caught and eaten)",
		    .facet = Facet::Story,
		    .description = "As story.q_pied_piper up to the script giving the creature back, holding the piper, then it "
		                   "eats him.",
		    .expected = "He stops existing in the creature's hand: the advisors' film (texts 1420, 1421) and the "
		                "lightning miracle dispenser reward by the creche; PiedPiperFinished=1, PiedPiperCompletedNow=1, "
		                "and the children stay in the cave.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = UpToTheCatch();
		// Given back to the player, the creature is moved with him down to the beach below its pen and throws him into
		// the sea
		Append(commands, MoveTo(k_OnTheBeach, k_AfterTheRelease));
		commands.push_back({.kind = Kind::ThrowAt, .delaySeconds = 0.5f, .point = k_InTheSea});
		commands.push_back(WaitSpoken(2136));
		commands.push_back(WaitTask("RewardForPiper", k_Finished));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_drowned",
		    .name = "Land 1: The Pied Piper (caught and thrown into the sea)",
		    .facet = Facet::Story,
		    .description = "As story.q_pied_piper up to the script giving the creature back, holding the piper, then it "
		                   "is moved with him to the beach below its pen and throws him into the sea.",
		    .expected = "He lands in the sea and drowns: he fades away, the evil advisor's line (text 2136) and the "
		                "lightning miracle dispenser reward by the creche; PiedPiperFinished=1, PiedPiperCompletedNow=1, "
		                "and the children stay in the cave.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = UpToTheCatch();
		// Given back to the player, the creature puts him down where it stands, far from his cave: he runs back to it.
		// The creature is taken to the spot 40 from where he pipes, so that he comes out and pipes again; once he does,
		// he is caught a second time, then carried to the cave
		commands.push_back({.kind = Kind::PutDown, .delaySeconds = k_AfterTheRelease});
		commands.push_back(WaitSpoken(1419));
		Append(commands, MoveTo(k_ByThePiper, 1.0f));
		Append(commands, Chase());
		// the second catch's only line after the scream, once the creature has him
		commands.push_back(WaitSpoken(1424));
		Append(commands, FreedAtTheCave());
		all.push_back({
		    .id = "story.q_pied_piper_again",
		    .name = "Land 1: The Pied Piper (set down away from his cave, caught again, set free)",
		    .facet = Facet::Story,
		    .description = "As story.q_pied_piper up to the script giving the creature back, holding the piper; it puts "
		                   "him down where it stands, then is moved 40 from his piping spot; once he pipes again its "
		                   "leash is tied to him again and, caught a second time, he is carried to his cave and put down.",
		    .expected = "Put down far from the cave he runs back to it (text 1419) and hides (texts 1432 to 1434), comes "
		                "out once the creature is far and pipes again; the second catch says only texts 1428 and 1424; "
		                "put down by his cave he frees the children (texts 1415 to 1418) and the reward is built: "
		                "PiedPiperFinished=1, PiedPiperCompletedNow=1.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = Introduction();
		const std::vector<Command> asides {
		    // The camera beside the creche woman for 25 seconds, as her sobs need it within 10 of her, and the first only
		    // comes about 10 seconds after the film
		    {.kind = Kind::CameraTo,
		     .delaySeconds = 2.0f,
		     .eye = {k_CrecheWoman.x, 4.0f, k_CrecheWoman.y},
		     .look = {k_CrecheWoman.x, 1.0f, k_CrecheWoman.y + 10.0f}},
		    // Then on the piper's spot
		    {.kind = Kind::CameraTo,
		     .delaySeconds = 25.0f,
		     .eye = {k_PipingSpot.x, 30.0f, k_PipingSpot.y - 50.0f},
		     .look = {k_PipingSpot.x, 3.0f, k_PipingSpot.y}},
		    // Once he pipes, the child he called taken into the hand on its way to him, 3 seconds after it was called,
		    // then put down short of him
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    {.kind = Kind::HandOver, .delaySeconds = 1.0f, .point = k_ChildPutDown},
		    {.kind = Kind::HandTake, .delaySeconds = 2.0f, .thing = k_CalledChild},
		    {.kind = Kind::HandDrop, .delaySeconds = 2.0f},
		    {.kind = Kind::HandOver, .delaySeconds = 0.5f, .value = 1},
		    // Then the Action button pressed on him: a click, as the player's
		    {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .thing = k_Piper},
		    {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 3, .thing = k_Piper},
		    {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
		    // Then the creature brought within 30 of him, untied: he runs and hides in his cave, and taunts it from there
		    // 15 seconds later; taken more than 30 away again, he comes out and goes piping again
		    ScriptMove(k_NearThePiper, 2.0f),
		    WaitText(1434, true),
		    ScriptMove(k_AwayFromThePiper, 30.0f),
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    LogGlobals(k_Globals, k_Scripts, 5.0f),
		};
		Append(commands, asides);
		all.push_back({
		    .id = "story.q_pied_piper_asides",
		    .name = "Land 1: The Pied Piper (the woman's sobs, a click on him, a child taken from him, his flight)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction, then puts the camera "
		                   "beside the creche woman for 25 seconds, then on the piper's spot and, once he pipes, takes "
		                   "the child he called into the hand and clicks on him; then moves the creature within 30 of him "
		                   "and, 30 seconds after he has hidden, more than 30 away.",
		    .expected = "She sobs (one of seven sounds) with the camera beside her; clicked, he says his prodding line "
		                "as a sound from him, without the dialogue box; the child taken, he plays his disappointed "
		                "animation; he runs from the creature and hides (texts 1432 to 1434), taunts it from his cave, "
		                "and comes out to pipe again once it is far.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = Introduction();
		const std::vector<Command> leash {
		    // Once he pipes, the leash tied to him with the creature still at its home, about 217 from him: the quest's
		    // walk up to him can't get there in the 20 seconds the leash holds for
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    {.kind = Kind::TieLeash, .delaySeconds = 1.0f, .thing = k_Piper},
		    // let go, he runs and hides; the creature being far, he comes out again and pipes
		    WaitSpoken(1434),
		    WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
		    LogGlobals(k_Globals, k_Scripts, 5.0f),
		};
		Append(commands, leash);
		all.push_back({
		    .id = "story.q_pied_piper_leash",
		    .name = "Land 1: The Pied Piper (the leash tied from too far)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction; once the piper pipes, "
		                   "ties the creature's leash to him while it is still at its home, far from him.",
		    .expected = "Leashed, he panics and the creature is walked towards him, but the leash's 20 seconds run out "
		                "first: it is let go, he says his line about not being kept tied up, runs and hides (texts 1432 "
		                "to 1434) and, the creature being far, comes out and pipes again.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		auto commands = Introduction();
		// [shortcut] The count of children shut in his cave set to 35, as thirty children taken on his rounds would
		// leave it: the rounds would take most of an hour of play
		commands.push_back(
		    {.kind = Kind::SetGlobal, .delaySeconds = 2.0f, .amount = 35.0f, .global = "PiedPiperChildrenInCave"});
		commands.push_back(WaitSpoken(1407));
		commands.push_back(WaitGlobal("PiedPiperCompletedNow", Compare::Equal, 1.0f));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_failure",
		    .name = "Land 1: The Pied Piper (thirty children lost)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction, then sets the count of "
		                   "children shut in his cave to 35 (a shortcut for his rounds).",
		    .expected = "The failure's film with the second generic script music (texts 1406, 1407); "
		                "PiedPiperFinished=2, PiedPiperCompletedNow=1, and no reward.",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		// the creche damaged while the woman lives, then the woman killed
		auto commands = Introduction();
		commands.push_back(DamageCreche(2.0f));
		commands.push_back(WaitSpoken(1411));
		commands.push_back(KillCrecheLeader(5.0f));
		commands.push_back(WaitSpoken(1398));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_creche_damaged",
		    .name = "Land 1: The Pied Piper (the creche damaged, then the creche woman killed)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction, then damages the creche "
		                   "and later kills the creche woman (shortcuts of the player's blows).",
		    .expected = "The damage: she faces the camera and complains (text 1411), once; her death: the film with the "
		                "evil advisor (texts 1397, 1398).",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	{
		// the creche woman killed, then the creche damaged
		auto commands = Introduction();
		commands.push_back(KillCrecheLeader(2.0f));
		commands.push_back(WaitSpoken(1398));
		commands.push_back(DamageCreche(5.0f));
		commands.push_back(WaitSpoken(1412));
		commands.push_back(LogGlobals(k_Globals, k_Scripts, 5.0f));
		all.push_back({
		    .id = "story.q_pied_piper_creche_leader_dead",
		    .name = "Land 1: The Pied Piper (the creche woman killed, then the creche damaged)",
		    .facet = Facet::Story,
		    .description = "Taps the scroll at the Norse creche and waits for the introduction, then kills the creche "
		                   "woman and later damages the creche (shortcuts of the player's blows).",
		    .expected = "Her death: the film with the evil advisor (texts 1397, 1398); the damage with her dead: the evil "
		                "advisor's line instead (text 1412).",
		    .commands = std::move(commands),
		    .onLand = true,
		});
	}
	// The glade's choice and the leash tied from too far, for the real input path
	// (tools/checks/realinput/pied_piper_real.txt, replayed through tools/checks/real_input_tap.sh): no pointer commands,
	// so the replayed mouse and keys alone drive the hand. The timelines click the texts, wait for the quest's tasks and
	// texts and only place the camera: the glade's own camera for the ape's two presses, the camera over the creche's
	// scroll for its press, then behind the piper, following him, for the leash key, the double click that ties the
	// leash to him and the presses on the land beside him that lead the creature
	all.push_back({
	    .id = "story.c_glade_newborn_real",
	    .name = "Land 1: the choice in the glade on a skip to it, played by hand",
	    .facet = Facet::Story,
	    .description = "Played first on the SkipBox's answer 2, with the player's mouse from a recording. It lets the "
	                   "glade's scene play and waits for the choice; the ape is pressed on twice by the recording.",
	    .expected = "The ape pressed on twice is chosen (ChooseYourCreatureFinished becomes 1); the player's creature "
	                "is a newborn put at its home point by the pen.",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            WaitTask("CreaturesInGlade", k_Running),
	            WaitGlobal("ChooseYourCreatureFinished", Compare::Equal, 1.0f),
	        },
	    .onLand = true,
	});
	all.push_back({
	    .id = "story.q_pied_piper_leash_real",
	    .name = "Land 1: The Pied Piper, the leash tied to him by hand",
	    .facet = Facet::Story,
	    .description = "Played after story.c_glade_newborn_real, with the player's mouse and keys from a recording. The "
	                   "camera is put over the creche's scroll, then, once he pipes, behind the piper, following him; "
	                   "the scroll's press, the leash key, the double click on him and the presses on the land that lead "
	                   "the creature are the recording's.",
	    .expected = "The scroll pressed starts the introduction; the leash tied to him from the creature's home, he "
	                "panics, the leash's 20 seconds run out (sound 1430), he runs and hides (texts 1432 to 1434) and "
	                "comes out again; the creature led within 30 of him starts his flight again, and tied to him from "
	                "near it catches him.",
	    .commands =
	        {
	            {.kind = Kind::ClickTexts, .value = 1},
	            WaitTask("PiedPiper", k_Running),
	            // [camera] the scroll, 70 south of it and 45 up, as the pointer chapter places it
	            {.kind = Kind::CameraTo,
	             .delaySeconds = 1.0f,
	             .eye = {k_Creche.x, 45.0f, k_Creche.y - 70.0f},
	             .look = {k_Creche.x, 5.0f, k_Creche.y}},
	            WaitSpoken(1410),
	            LogGlobals(k_Globals, k_Scripts, 1.0f, 20),
	            WaitGlobal("PiedPiperFlockSize", Compare::AtLeast, 2.0f),
	            // [camera] behind the piper, following him wherever he goes
	            {.kind = Kind::CameraFollow, .delaySeconds = 0.5f, .amount = 1.0f, .thing = k_Piper},
	            WaitSpoken(1434),
	            WaitTask("PiperSetFree", k_Finished),
	            LogGlobals(k_Globals, k_Scripts, 5.0f),
	        },
	    .onLand = true,
	});
}
