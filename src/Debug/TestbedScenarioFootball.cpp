/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the football pitch, with football on as with the Soccer Add-On: a town's pitch put down
// whole, a scaffold of eight that a town builds its own pitch from, and a match played by the town's villagers

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The pitch's middle, north of the scenario town's middle
constexpr glm::vec2 k_Pitch {0.0f, 45.0f};
/// Where the scaffold of eight is put down, on open land north of the town
constexpr glm::vec2 k_Scaffold {0.0f, 45.0f};

/// Six adults of the scenario's town round its middle
std::vector<ObjectSetup> TownPeople()
{
	return {
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 0.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 0.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, 6.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 6.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, -6.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = {4.0f, -6.0f}, .joinTown = true},
	};
}

/// Twelve adults of the scenario's town round its middle: a referee, two sides of five and a watcher
std::vector<ObjectSetup> MatchPeople()
{
	auto people = TownPeople();
	const std::vector<ObjectSetup> more {
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, 3.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {12.0f, 3.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = {-12.0f, -3.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {12.0f, -3.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 10.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -10.0f}, .joinTown = true},
	    // six more, so that some are left over to watch once the referee and both sides are full
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 8.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {18.0f, 8.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = {-18.0f, -8.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {18.0f, -8.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-6.0f, 14.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {6.0f, -14.0f}, .joinTown = true},
	};
	people.insert(people.end(), more.begin(), more.end());
	return people;
}
} // namespace

void testbed_scenarios::AddFootballScenarios(std::vector<Scenario>& all)
{
	// A whole pitch put down for the town, as a land script's abode command puts one down: it is made the town's
	// pitch at once and makes its ball
	std::vector<ObjectSetup> built = TownPeople();
	built.push_back({.type = AbodeInfo::CelticFootballPitch, .offset = k_Pitch});
	all.push_back({
	    .id = "building.football_pitch_built",
	    .name = "A town's football pitch and its ball",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's, six of its adults round its middle, and its football pitch put "
	                   "down whole 45 units north of the middle, as a land script's abode command puts one down, with "
	                   "football on as with the Soccer Add-On.",
	    .expected = "The pitch lies on the land, draped over it. It becomes the town's pitch at once and makes its "
	                "ball: a football lies on the pitch's centre spot from the start. The game log has a \"Football: "
	                "pitch ... made its ball\" line, once. The pitch makes no second ball while that one is about.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 90.0f}}},
	    .objects = built,
	});
	// A scaffold of eight put down gently in the town: with football on, the pitch is what the town wants most for
	// eight scaffolds (more than the wonder seven of them would make)
	all.push_back({
	    .id = "building.football_pitch_from_scaffolds",
	    .name = "A scaffold of eight becomes a football pitch",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's, six of its adults round its middle, with football on as with the "
	                   "Soccer Add-On. Two seconds in, the player's hand puts a scaffold standing for eight scaffolds "
	                   "down gently on open land 45 units north of the middle, as its put-down of a held scaffold "
	                   "ends.",
	    .expected = "The scaffold builds the town's football pitch: a building site laid out with the pitch's outline "
	                "and a round ring of building places, not a wonder's. The log's line for the command says a "
	                "building site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 90.0f}}},
	    .objects = TownPeople(),
	    .commands = {{.kind = Command::Kind::HandPlantScaffold, .delaySeconds = 2.0f, .point = k_Scaffold, .value = 8}},
	});
	// The town's pitch and eighteen of its adults late in the afternoon, the hour standing still where the town wants
	// relaxation most: those who relax take the pitch up and play
	std::vector<ObjectSetup> match = MatchPeople();
	match.push_back({.type = AbodeInfo::CelticFootballPitch, .offset = k_Pitch});
	all.push_back({
	    .id = "building.football_match",
	    .name = "A town's villagers play a football match",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's, eighteen of its adults round its middle, and its football pitch put "
	                   "down whole 45 units north of the middle, with football on as with the Soccer Add-On. The hour "
	                   "stands at a quarter to four in the afternoon, when the town wants relaxation most. Each "
	                   "villager's top state is logged as it changes.",
	    .expected = "Villagers who relax take the pitch up (\"Football: villager ... takes up pitch ... as ...\" lines): "
	                "the first is the referee, the next five the home side, the next five the away side, the rest "
	                "watch from the pitch's edge. The players walk to spots near the centre spot and wait for the "
	                "kick-off (state 72). Once both sides have three, the ball is put on the centre spot 30 s later and "
	                "the kick-off comes when fewer than five players are still walking: the players become attackers, "
	                "defenders and goalies (states 73, 75 and 74), run to the ball and kick it about, attackers "
	                "shooting at the goal they attack. A ball out of the pitch's length is fetched by one player and "
	                "kicked back to the middle (states 78 to 81) while the others pause (82). A goal logs \"Football: "
	                "GOAL!!!!!\", the scorers and the others hold still for 7 s (states 76 and 77), then play goes on. "
	                "The watchers face the pitch, then the ball, and now and then rise in a Mexican wave (84).",
	    .environment = {.hour = 15.75f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 90.0f}}},
	    .objects = match,
	    .logVillagerStates = true,
	    .logFootball = true,
	});
	// The same match with the player's creature by the pitch, told by a script to do the two match actions its play
	// desire knows, each done to one of the town's adults
	constexpr size_t k_GoalieCatchBall = 224;
	constexpr size_t k_GoalieFoul = 225;
	all.push_back({
	    .id = "building.football_match_creature",
	    .name = "The creature joins in a football match",
	    .facet = Facet::Objects,
	    .description = "The football match scenario (a Celtic town's pitch, eighteen of its adults, the hour standing at a "
	                   "quarter to four), with the player's grown tiger south of the pitch, kept content. Two minutes "
	                   "in, a script tells it to do the goalie's catch to the town's second adult; two and a half "
	                   "minutes in, the goalie's foul to the third.",
	    .expected = "Done to an adult who is playing or watching by then: for the catch, the tiger may first show it "
	                "feels playful, walks to the match's ball and picks it up, then throws it at the second goal mouth "
	                "(its side, the player's, is the home side); for the foul, it stomps angrily on the first player of "
	                "the away side. Done to an adult who is in no match, the action fails at once and the tiger goes "
	                "back to its own doings.",
	    .environment = {.hour = 15.75f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -30.0f}, {40.0f, 90.0f}}},
	    .creatures =
	        {{.species = CreatureType::Tiger,
	          .offset = {0.0f, -20.0f},
	          .facingDegrees = 180.0f,
	          .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f},
	          .hold = true}},
	    .objects = match,
	    .commands = {{.kind = Command::Kind::ScriptDoAction, .delaySeconds = 120.0f, .value = k_GoalieCatchBall, .object = 1},
	                 {.kind = Command::Kind::ScriptDoAction, .delaySeconds = 30.0f, .value = k_GoalieFoul, .object = 2}},
	    .logVillagerStates = true,
	    .logFootball = true,
	});
	// The same match run on past three minutes of play, with the town's desire for food boosted once play has begun so
	// that food stays what it wants most, neither relaxation nor playtime
	constexpr size_t k_ForFood = 0;
	all.push_back({
	    .id = "building.football_match_stops",
	    .name = "A town's football match stops when the town wants something else",
	    .facet = Facet::Objects,
	    .description = "The football match scenario (a Celtic town's pitch, eighteen of its adults, the hour standing at a "
	                   "quarter to four). 160 s in, once play has begun, a script boosts the town's desire for food "
	                   "fully, as SET_TOWN_DESIRE_BOOST does (food is already what this town wants most; the boost keeps "
	                   "it so). Each villager's top state, the pitch's play and sides and the town's most wanted desire "
	                   "are logged as they change.",
	    .expected = "The match plays as in the football match scenario. Once it has played three minutes (1800 turns of "
	                "play, the goals' pauses not counted), the town's most wanted desire being food, the match stops: "
	                "the players and the watchers are sent home and the pitch waits for players again (play 0, both "
	                "sides empty, the score back to 0-0).",
	    .environment = {.hour = 15.75f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 90.0f}}},
	    .objects = match,
	    .commands = {{.kind = Command::Kind::TownDesireBoost, .delaySeconds = 160.0f, .value = k_ForFood, .amount = 1.0f}},
	    .logVillagerStates = true,
	    .logFootball = true,
	    .logTownDesire = true,
	});
	// The town's pitch with only six adults: the referee and one side of five at most, so neither side ever has three
	// against three
	std::vector<ObjectSetup> few = TownPeople();
	few.push_back({.type = AbodeInfo::CelticFootballPitch, .offset = k_Pitch});
	all.push_back({
	    .id = "building.football_match_short_side",
	    .name = "Too few villagers for a football match",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's, six of its adults round its middle, and its football pitch put "
	                   "down whole 45 units north of the middle, with football on, the hour standing at a quarter to "
	                   "four. Each villager's top state and the pitch's play and sides are logged as they change.",
	    .expected = "Those who relax take the pitch up: a referee, then the home side; the away side, filled only once "
	                "the home side has five, stays empty. The players wait for the kick-off (72), which never comes: "
	                "after 180 s of waiting each player is sent home (GO_HOME) and leaves its side.",
	    .environment = {.hour = 15.75f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -10.0f}, {40.0f, 90.0f}}},
	    .objects = few,
	    .logVillagerStates = true,
	    .logFootball = true,
	});
}
