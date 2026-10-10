/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's example scenarios, the models to copy for new ones: each sets its scene out with a few fixtures
// (Debug/TestbedFixtures.h) rather than object by object, and shows one way of driving it: a recorded hand demo, the
// player's keys, the weather, the game's own land change, or a building the testbed does not have

#include <vector>

#include "Input/BindableActions.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
namespace fixtures = testbed_fixtures;

/// The player pressing the key of a bindable action, as the options screen's presses are made
Command Press(input::BindableActionMap action, float delay)
{
	return {.kind = Kind::PressKey, .delaySeconds = delay, .value = static_cast<size_t>(action)};
}

/// The creature walking or running to a point from the middle of the map, once it is free of what it did before
Command Go(Kind kind, glm::vec2 point, float delay)
{
	return {.kind = kind, .delaySeconds = delay, .waitUntilFree = true, .point = point};
}
} // namespace

void testbed_scenarios::AddExampleScenarios(std::vector<Scenario>& all)
{
	// A recorded hand demo drives the hand, and the fixtures stand where it presses
	all.push_back({
	    .id = "examples.miracle_near_village",
	    .name = "A miracle cast by a hand demo beside a village",
	    .facet = Facet::Examples,
	    .description = "A village stands just beyond where the recorded demo MiracleCast presses the action button, and "
	                   "a fireball seed is put in the player's hand; the demo then moves the camera and the hand and casts "
	                   "it.",
	    .expected = "The demo plays from the start, moving the camera as it was recorded; at its press the fireball is "
	                "cast from the hand and lands beside the village rather than on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .fixtures = {.villages = {{.at = fixtures::DemoPress {.press = 0, .nudge = {0.0f, 30.0f}}}},
	                 .handSeed = MagicType::Fireball,
	                 .handDemo = fixtures::HandDemo {.name = "MiracleCast"}},
	});

	// The creature held by a fixture, and the player's keys acting on it through the game's own controls
	all.push_back({
	    .id = "examples.leashed_creature",
	    .name = "Your creature on the leash by a wood pile",
	    .facet = Facet::Examples,
	    .description = "Your creature stands on the learning leash north of the middle, beside a wood pile and a few "
	                   "trees. The leash key L is pressed after 4 seconds and again 4 seconds later, over and over.",
	    .expected = "A rope runs from the hand to its collar. The first press takes the leash off and the creature goes "
	                "its own way about the pile and the trees; the next puts the learning leash back on, and the rope "
	                "pulls it back towards the hand once taut.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, 20.0f}, {60.0f, 80.0f}}},
	    .commands = {Press(input::BindableActionMap::LEASH_UNLEASH_CREATURE, 4.0f),
	                 Press(input::BindableActionMap::LEASH_UNLEASH_CREATURE, 4.0f)},
	    .repeatFrom = 0,
	    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 40.0f}, .hold = fixtures::Hold::Leashed}},
	                 .trees = {{.type = TreeInfo::Oak, .at = glm::vec2 {-35.0f, 55.0f}, .count = 4, .spread = 15.0f}},
	                 .piles = {{.type = PotInfo::WoodPile_1, .at = glm::vec2 {30.0f, 45.0f}}}},
	});

	// The weather as a fixture: a storm over the scene, with its lightning
	all.push_back({
	    .id = "examples.storm_lightning",
	    .name = "A storm with lightning over a village",
	    .facet = Facet::Examples,
	    .description = "A storm stands over a village and a wood north of the middle for a minute: full rain, fork "
	                   "lightning every 4 to 8 seconds and sheet lightning every 3 to 6.",
	    .expected = "Clouds gather over the village and rain falls within the storm's radius. Forks of lightning strike "
	                "within it now and then, and sheet lightning flashes the sky, each with its thunder.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-90.0f, 0.0f}, {90.0f, 140.0f}}},
	    .fixtures = {.storms = {{.at = glm::vec2 {0.0f, 60.0f},
	                             .innerRadius = 60.0f,
	                             .outerRadius = 150.0f,
	                             .lifeSeconds = 60.0f,
	                             .forkSeconds = glm::vec2 {4.0f, 8.0f},
	                             .sheetSeconds = glm::vec2 {3.0f, 6.0f}}},
	                 .villages = {{.at = glm::vec2 {0.0f, 60.0f}, .villagers = 6}},
	                 .trees = {{.type = TreeInfo::Beech, .at = glm::vec2 {60.0f, 90.0f}, .count = 6, .spread = 20.0f}}},
	});

	// The fixtures at the first and the last press of a demo of four presses
	all.push_back({
	    .id = "examples.hand_throw_target",
	    .name = "The hand demo throwing food at a village",
	    .facet = Facet::Examples,
	    .description = "The recorded demo castfood plays: a food pile stands where it first presses, and a village where "
	                   "it last presses.",
	    .expected = "The hand picks food up from the pile and throws it; the food lands at the village.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .fixtures = {.villages = {{.at = fixtures::DemoPress {.press = 3}}},
	                 .piles = {{.type = PotInfo::FoodPile, .at = fixtures::DemoPress {.press = 0}}},
	                 .handDemo = fixtures::HandDemo {.name = "castfood"}},
	});

	// A scenario that writes into the game's folder says so, and so runs only on a copy of the game's data
	all.push_back({
	    .id = "examples.land_change_creature",
	    .name = "Your creature through a land change",
	    .facet = Facet::Examples,
	    .description = "Your creature stands on the testbed; after 5 seconds the game changes to the land of Land2.txt "
	                   "through its own land change, which saves the creature to the profile first. It runs only on a "
	                   "copy of the game's data.",
	    .expected = "The second land loads, the scenario ends, and the creature stands on the new land as it was saved.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::ChangeLand, .delaySeconds = 5.0f, .landScript = "Land2.txt"}},
	    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 40.0f}}}, .writesGameData = true},
	});

	// A fight by the temple: the player's creature in its temple's pen (a creature fixture, so numbered after the
	// scenario's own creature) fights a second creature standing by the pen, both fighting by themselves
	all.push_back({
	    .id = "creature.fights_in_temple_pen",
	    .name = "Your creature fighting in its temple's pen",
	    .facet = Facet::Examples,
	    .description = "Your creature in its temple's pen north of the middle is told to fight another player's tiger "
	                   "standing by the pen, and both fight by themselves.",
	    .expected = "Your creature is drawn at the pen size near the pen; the two walk to their places, face each other "
	                "and fight, with their blows, blocks and steps, until one faints.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 40.0f}, {40.0f, 110.0f}}},
	    .creatures = {{.label = "opponent",
	                   .species = CreatureType::Tiger,
	                   .offset = {25.0f, 55.0f},
	                   .facingDegrees = 270.0f,
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .alignment = -0.45f}},
	    .commands = {{.kind = Kind::StartFight, .creature = 1, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::FightAuto, .creature = 1, .delaySeconds = 0.2f, .value = 1}},
	    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 80.0f}, .hold = fixtures::Hold::TemplePen}},
	                 .temples = {{.at = glm::vec2 {0.0f, 80.0f}}}},
	});

	// The creature by its own temple: the game keeps its home at the pen and draws it at the pen's size there
	all.push_back({
	    .id = "creature.walks_in_temple_pen",
	    .name = "Your creature walking in its temple's pen",
	    .facet = Facet::Examples,
	    .description = "Your creature free in its temple's pen north of the middle for a minute, drawn at the pen size.",
	    .expected = "It is drawn small (the pen size) while near its pen; when it walks there, its legs keep pace with "
	                "how far it goes, at the small body's speed and stride, with no sliding.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 50.0f}, {40.0f, 110.0f}}},
	    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 80.0f}, .hold = fixtures::Hold::TemplePen}},
	                 .temples = {{.at = glm::vec2 {0.0f, 80.0f}}}},
	});

	// A building the testbed does not have, made by a fixture: the player's temple, and the creature its posts need
	// The creature stands 60 m east of the temple: the temple reaches at most about 37 m from its middle, so even at the
	// far edge of its 12 m pen the creature stays clear of it
	all.push_back({
	    .id = "examples.temple_leash_posts",
	    .name = "Your temple's three leash posts",
	    .facet = Facet::Examples,
	    .description = "Your temple stands built north of the middle, made as a land's script makes it. Your creature is "
	                   "penned 60 m east of it and knows the three leashes, the evil, the learning and the compassion one, "
	                   "so that the temple shows all three of its leash posts.",
	    .expected = "Three leash posts stand at the temple, each a collar turning slowly with its smoke above it. "
	                "Tapping one picks its leash: its smoke turns orange and a click is heard. Making the temple draws "
	                "twelve numbers from the game's random stream for its posts, so every run is the same.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-80.0f, -30.0f}, {80.0f, 120.0f}}},
	    .fixtures = {.creatures = {{.at = glm::vec2 {60.0f, 40.0f},
	                                .hold = fixtures::Hold::Penned,
	                                .knows = {LeashType::Evil, LeashType::Rope, LeashType::Good}}},
	                 .temples = {{.at = glm::vec2 {0.0f, 40.0f}}}},
	});

	// The pen's size at its open end: the creature walks out of its pen and back, then runs out and back
	all.push_back({
	    .id = "creature.leaves_temple_pen",
	    .name = "Your creature walking and running out of its temple's pen",
	    .facet = Facet::Examples,
	    .description = "Your creature in its temple's pen north of the middle walks out through the pen's open end to "
	                   "25 metres from its pen point and back, then runs out and back.",
	    .expected = "Drawn at the pen size in its pen, it grows to its own size over the two metres between 14 and 16 "
	                "metres from its pen point as it goes out, and shrinks over the same two metres as it comes back; "
	                "the same at a run, over fewer turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 20.0f}, {40.0f, 110.0f}}},
	    .commands = {Go(Kind::WalkTo, {-19.69f, 36.82f}, 1.0f), Go(Kind::WalkTo, {-9.32f, 59.56f}, 2.0f),
	                 Go(Kind::RunTo, {-19.69f, 36.82f}, 2.0f), Go(Kind::RunTo, {-9.32f, 59.56f}, 2.0f)},
	    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 80.0f}, .hold = fixtures::Hold::TemplePen}},
	                 .temples = {{.at = glm::vec2 {0.0f, 80.0f}}}},
	});
}
