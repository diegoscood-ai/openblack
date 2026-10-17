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
// player's keys, the weather, the game's own land change, or a building the testbed does not have. One sets its
// few objects out by hand instead, so that its commands can name them: a villager's walk round a tree that a storage
// pit then takes

#include <array>
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

	// The game speed is kept from one land to the next, as in the original
	all.push_back({
	    .id = "examples.speed_across_land",
	    .name = "The game speed through a land change",
	    .facet = Facet::Examples,
	    .description = "The game speed is set to 2 (twice as fast); 5 seconds later the game changes to the land of "
	                   "Land2.txt through its own land change. It runs only on a copy of the game's data.",
	    .expected = "The log says the game speed is 2.00, then the second land loads and the log says the game speed is "
	                "2.00 still: a land change keeps the speed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::SetGameSpeed, .delaySeconds = 0.5f, .gameSpeed = 2.0f},
	                 {.kind = Kind::ChangeLand, .delaySeconds = 5.0f, .landScript = "Land2.txt"}},
	    .fixtures = {.writesGameData = true},
	});

	// What the game shows of its own speed: the words for slowing or speeding time, written near the top of the screen
	all.push_back({
	    .id = "examples.game_speed_indicator",
	    .name = "The game speed shown on the screen",
	    .facet = Facet::Examples,
	    .description = "The game is slowed to half speed after a second, sped up to twice the speed 3 seconds later, "
	                   "and put back to normal 8 seconds after that. The delays are the scenario's seconds, which follow "
	                   "the speed: about 1, 6 and 4 seconds of real time.",
	    .expected = "While slowed, the words for slowing time (HELP_TEXT_GAME_SLOWMO) fade in near the top of the "
	                "screen in white, their letters tumbling one after another; on speeding up they fade out and the "
	                "words for speeding time (HELP_TEXT_GAME_FASTMO) fade in; back at normal speed they fade out and "
	                "nothing is shown. The log says each speed as it is set.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::SetGameSpeed, .delaySeconds = 1.0f, .gameSpeed = 0.5f},
	                 {.kind = Kind::SetGameSpeed, .delaySeconds = 3.0f, .gameSpeed = 2.0f},
	                 {.kind = Kind::SetGameSpeed, .delaySeconds = 8.0f, .gameSpeed = 1.0f}},
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

	// What the object draw list draws: a town made a member a frame after the list's first rebuild, which the list takes
	// in only at its next rebuild by turns, while the trees and the pile set out before it are drawn from the start
	all.push_back({
	    .id = "examples.objects_join_the_draw_list",
	    .name = "A new town joining the object draw list",
	    .facet = Facet::Examples,
	    .description = "A still overview of the middle of the testbed, with a wood pile and a few beeches set out before "
	                   "the first frame. A town of fifty villagers is then made in the middle, a member a frame: its "
	                   "storage pit, a ring of ten huts and the villagers, all made within the first second (62 frames "
	                   "of 16 ms). Nothing in it asks the object draw list for a rebuild. Run for 60 frames, the shot "
	                   "20 frames before the end is at frame 40, 0.64 s in.",
	    .expected = "The pile and the trees are drawn from the first frame: the list's first rebuild, which the land's "
	                "load asks for, takes them in. The town is not drawn at first: what is made after that rebuild joins "
	                "the list only at its next one, 11 turns later (1.0 to 1.1 s in), as the original; nothing asks for one "
	                "sooner. So the shot at frame 40 shows only the buildings' shadows on the land where the town is going up "
	                "(the land's shadows are outside the list, as the original's), and about a second in "
	                "the town appears at once (a villager made on the frame of that rebuild or after it, a second later). "
	                "With Land Island > Object draw list > Draw only what the list draws off, the town goes up a member a "
	                "frame, and the shot at frame 40 shows its storage pit, its ten huts and the first villagers.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-100.0f, -60.0f}, {100.0f, 70.0f}}},
	    // One town, in the middle, of 62 members (the town, its pit, 10 huts, 50 villagers) made a member a frame: they
	    // are all made before the list's first rebuild by turns, 62 to 69 frames in
	    .crowd = Crowd {.kind = Crowd::Kind::Villagers, .count = 50, .seed = 1, .perFrame = 1},
	    .fixtures = {.trees = {{.type = TreeInfo::Beech, .at = glm::vec2 {-85.0f, 40.0f}, .count = 4, .spread = 12.0f}},
	                 .piles = {{.type = PotInfo::WoodPile_1, .at = glm::vec2 {85.0f, 40.0f}}}},
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

	// The two lanterns at the temple's door come with the temple's first frame drawn more than 7 % built
	all.push_back({
	    .id = "examples.temple_lanterns",
	    .name = "Your temple's two lanterns at its door",
	    .facet = Facet::Examples,
	    .description = "Your temple stands north of the middle at ten at night, the clock standing still, only 5 % built. "
	                   "After 4 seconds it is 7 % built, after 8 seconds 8 %.",
	    .expected = "The temple has no lanterns while it is drawn 5 % and 7 % built. Once it is 8 % built two town lamps "
	                "stand beside its door, each with its two flames and glow lighting the land "
	                "round it. Making them draws twelve numbers from the game's random stream, so every run is the same.",
	    .environment = {.hour = 22.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, 0.0f}, {50.0f, 80.0f}}},
	    .commands = {{.kind = Command::Kind::TempleBuilt, .delaySeconds = 4.0f, .built = 0.07f},
	                 {.kind = Command::Kind::TempleBuilt, .delaySeconds = 8.0f, .built = 0.08f}},
	    .fixtures = {.temples = {{.at = glm::vec2 {0.0f, 40.0f}, .built = 0.05f}}},
	});

	// A partly built house: cut at its build height, its walls given an inner side and capped over the cut
	all.push_back({
	    .id = "examples.abode_partly_built",
	    .name = "A Celtic hut drawn partly built",
	    .facet = Facet::Examples,
	    .description = "A Celtic hut is put down north of the middle as a plan of the town being built, nothing of it "
	                   "built. After 2 seconds it is 10 % built, after 5 seconds 50 %, after 8 seconds 85 %.",
	    .expected =
	        "Nothing of the hut is drawn at first, only its mark on the land. At 10 % its scaffold rises half out "
	        "of the ground and the walls are drawn up to a tenth of its height; at 50 % the scaffold stands and the "
	        "walls stop half way up. Where they stop, each wall has an inner side 0.35 in from the outer one (0.2 for a "
	        "two-sided material) and a cap joins the two along the cut, textured as the wall but not shaded by the light: "
	        "it is evenly darker, three quarters of the hut's colour, whichever way it faces. At 85 % the scaffold is cut "
	        "from its top.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-8.0f, 32.0f}, {8.0f, 48.0f}}, .distance = 0.4f},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, 40.0f}, .underConstruction = true}},
	    .commands = {{.kind = Command::Kind::AbodeBuilt, .delaySeconds = 2.0f, .built = 0.1f},
	                 {.kind = Command::Kind::AbodeBuilt, .delaySeconds = 3.0f, .built = 0.5f},
	                 {.kind = Command::Kind::AbodeBuilt, .delaySeconds = 3.0f, .built = 0.85f}},
	});

	// A wonder being built: its own wooden scaffold (a sub-mesh of the wonder's model) rises under a fifth built, stands
	// whole up to four fifths, then is cut from its top as the stone goes up; one scene for each percent, for its shot
	struct WonderStep
	{
		std::string_view id;
		float built;
		std::string_view expected;
	};
	static constexpr std::array<WonderStep, 4> k_WonderSteps {{
	    {"examples.wonder_built_10", 0.1f,
	     "The wooden scaffold stands sunk half its height into the ground round the wonder's mark, and the stone is "
	     "drawn up to a tenth of the model's height (scaffold included), about 1.1 of its 10.8. Before, nothing of it "
	     "is drawn, only its mark on the land."},
	    {"examples.wonder_built_50", 0.5f,
	     "The scaffold stands whole round the wonder, up to the top of its middle mast. The stone is cut at half the "
	     "model's height, 5.4: the outer ring of stones, about as high, stands nearly whole, and the inner stones, 7.5 "
	     "high, stop at the cut."},
	    {"examples.wonder_built_90", 0.9f,
	     "The stone stands whole: the cut, at nine tenths of the model's height (9.7), is above it. The scaffold is cut "
	     "at half its height (5.4): its middle mast and the spars over the inner stones are gone, and its outer cage, "
	     "under 4.8 high, stays whole round the outer ring."},
	    {"examples.wonder_built_100", 1.0f, "The wonder stands whole in stone, with no scaffold left."},
	}};
	for (const auto& step : k_WonderSteps)
	{
		all.push_back({
		    .id = step.id,
		    .name = "A Celtic wonder being built",
		    .facet = Facet::Examples,
		    .description = "A Celtic wonder is put down north of the middle as a plan of the town being built, nothing of "
		                   "it built; after 2 seconds it is set to the scene's percent built.",
		    .expected = step.expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 25.0f}, {25.0f, 75.0f}}, .distance = 0.6f},
		    .objects = {{.type = AbodeInfo::CelticWonder, .offset = {0.0f, 50.0f}, .underConstruction = true}},
		    .commands = {{.kind = Command::Kind::AbodeBuilt, .delaySeconds = 2.0f, .built = step.built}},
		    .logBuildingsEvery = 1.0f,
		});
	}

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

	// A tree a villager walks round taken by a storage pit, as a tree thrown into one is, while the villager hugs it.
	// The tree stands in the middle of a cell, so that it is filed in the cell it stands in whatever its size, with
	// nothing else near the walk for the villager to meet first
	constexpr glm::vec2 k_WalkFrom {5.0f, 12.0f};
	constexpr glm::vec2 k_Tree {5.0f, 25.0f};
	constexpr glm::vec2 k_WalkTo {5.0f, 48.0f};
	constexpr glm::vec2 k_Pit {-40.0f, 30.0f};
	all.push_back({
	    .id = "examples.villager_tree_gone",
	    .name = "A villager walking round a tree that is taken away",
	    .facet = Facet::Examples,
	    .description = "A villager walks north to a point straight behind an oak, so that its walk goes round the tree. "
	                   "Half a second after it starts round it, a storage pit to the west takes the tree, as a tree thrown "
	                   "into a pit is taken: the pit gets its wood and the tree is deleted at once.",
	    .expected = "The villager walks up to the oak and starts round it; the tree goes, with its ghost for half a "
	                "second and the mulch sound, and the villager walks on straight to its goal and stands there. The game "
	                "does not crash (it used to, reading the tree that had gone).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_WalkFrom, k_Tree, k_WalkTo, k_Pit}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = k_WalkFrom},
	                {.type = TreeInfo::Oak, .offset = k_Tree},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit}},
	    .commands = {{.kind = Kind::VillagerWalkTo, .point = k_WalkTo, .object = 0},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 0.5f, .waitUntilFree = true, .value = 2, .object = 1}},
	});
	all.push_back({
	    .id = "examples.store_takes_each_class",
	    .name = "A storage pit taking one thing of each kind, each leaving its dissolving ghost",
	    .facet = Facet::Examples,
	    .description = "A Celtic storage pit stands among an oak, a dead tree, a cow, a short Celtic fence, a pile of wood "
	                   "in the hand's kind and one of food; one second apart, the pit takes each of them as a thrown "
	                   "thing that lands in it is taken.",
	    .expected = "Each thing goes at once and leaves a ghost of itself where it stood for half a second, which breaks "
	                "up into a moving pattern of holes and is gone, the same for every kind: the dissolve of the "
	                "original's ghost, not a copy that stays whole and vanishes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 18.0f, -30.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 12.0f}},
	                {.type = TreeInfo::Oak, .offset = {-12.0f, 0.0f}},
	                {.type = MobileStaticInfo::DeadTree, .offset = {-6.0f, -4.0f}},
	                {.type = AnimalInfo::Cow, .offset = {0.0f, -6.0f}},
	                {.type = MobileStaticInfo::CeltFenceShort, .offset = {6.0f, -4.0f}},
	                {.type = PotInfo::HandWood, .offset = {10.0f, 0.0f}},
	                {.type = PotInfo::HandFood, .offset = {13.0f, 3.0f}}},
	    .commands = {{.kind = Kind::StoreTakes, .delaySeconds = 2.0f, .value = 0, .object = 1},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 1.0f, .value = 0, .object = 2},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 1.0f, .value = 0, .object = 3},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 1.0f, .value = 0, .object = 4},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 1.0f, .value = 0, .object = 5},
	                 {.kind = Kind::StoreTakes, .delaySeconds = 1.0f, .value = 0, .object = 6}},
	});
}
