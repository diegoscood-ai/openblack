/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's physics scenarios: things lying still, one thing thrown, a crowd of things thrown and dropped together,
// buildings broken by thrown rocks, rocks split by hard landings, things dropped into the sea and things reflected in it
// as they fly, an immovable rock let go in the air, and a tree put down gently and planted again. Each throw is the
// player's hand's, so the same rules run as in a game; run with --frame-stats to see what a frame costs while they fly
// and once they have come to rest.

#include <cmath>

#include <array>
#include <numbers>
#include <string_view>
#include <vector>

#include "3D/FlatLand.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// Where the physics happens: south of the middle, in front of the camera and clear of the lake
constexpr glm::vec2 k_Field {0.0f, -40.0f};
/// How much closer than its overview a scenario is framed for the sounds of its field to be heard
constexpr float k_WithinEarshot = 0.75f;
/// A throw towards the field's middle from this far away lands near it
constexpr float k_ThrowFrom = 30.0f;
/// The hand's speed for a throw across the field, up and along
constexpr float k_ThrowUp = 9.0f;
constexpr float k_ThrowAlong = 16.0f;

/// The things thrown in the crowd, in turn: the kinds whose bodies the physics builds differently
const std::array<ObjectSetup, 10> k_Kinds {
    ObjectSetup {.type = MobileStaticInfo::RockChalk},
    ObjectSetup {.type = MobileStaticInfo::Boulder1Lime},
    ObjectSetup {.type = MobileObjectInfo::EgyptPotA},
    ObjectSetup {.type = PotInfo::FoodPile, .amount = 200},
    ObjectSetup {.type = PotInfo::WoodPile_1, .amount = 200},
    ObjectSetup {.type = MobileObjectInfo::EgyptBarrel},
    ObjectSetup {.type = VillagerInfo::CelticFarmerMale},
    ObjectSetup {.type = TreeInfo::Birch, .scale = 0.6f},
    ObjectSetup {.type = MobileObjectInfo::Ball},
    ObjectSetup {.type = MobileStaticInfo::SharprockSandstone},
};

/// A throw from a point on a ring round the field's middle, towards it
ThrowSetup Inward(size_t object, float angle, float delay, std::optional<float> repeat)
{
	const glm::vec2 out {std::cos(angle), std::sin(angle)};
	return {
	    .object = object,
	    .from = k_Field + (out * k_ThrowFrom),
	    .height = 4.0f,
	    .velocity = {-out.x * k_ThrowAlong, k_ThrowUp, -out.y * k_ThrowAlong},
	    .delaySeconds = delay,
	    .repeatSeconds = repeat,
	};
}

void AddIdleField(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	for (size_t i = 0; i < 40; ++i)
	{
		auto object = k_Kinds.at(i % k_Kinds.size());
		object.offset =
		    k_Field + glm::vec2 {(static_cast<float>(i % 8) - 3.5f) * 9.0f, (static_cast<float>(i / 8) - 2.0f) * 9.0f};
		objects.push_back(object);
	}
	all.push_back({
	    .id = "physics.idle",
	    .name = "Things lying still",
	    .facet = Facet::Physics,
	    .description = "Forty rocks, boulders, pots, piles, barrels, villagers, trees and balls lie on the land in a grid, "
	                   "and nothing throws them.",
	    .expected = "Nothing is in the physics: a frame costs what the land, the things and the villagers cost, and the "
	                "physics adds nothing to it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	});
}

void AddOneThrow(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "physics.throw_one",
	    .name = "One rock thrown",
	    .facet = Facet::Physics,
	    .description = "A rock is thrown across the field every eight seconds, from the west towards the middle.",
	    .expected = "It flies in an arc, lands with a thump and dust, rolls and comes to rest, after which it costs nothing "
	                "until it is thrown again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = k_Field}},
	    .throws = {Inward(0, std::numbers::pi_v<float>, 1.0f, 8.0f)},
	});

	all.push_back({
	    .id = "physics.throw_in_snow",
	    .name = "A rock thrown into the snow",
	    .facet = Facet::Physics,
	    .description = "Snow falls over the whole island; a rock is thrown across the field every eight seconds, from the "
	                   "west towards the middle.",
	    .expected = "As the snow comes to lie, the dust of each landing turns from the land's brown towards the land's "
	                "light, the more so the deeper the snow is where it lands.",
	    .environment = {.weather = Weather::Snow, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = k_Field}},
	    .throws = {Inward(0, std::numbers::pi_v<float>, 1.0f, 8.0f)},
	});
}

void AddCrowdThrow(std::vector<Scenario>& all)
{
	constexpr size_t k_Count = 60;
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < k_Count; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(k_Count);
		auto object = k_Kinds.at(i % k_Kinds.size());
		object.offset = k_Field + glm::vec2 {std::cos(angle), std::sin(angle)} * k_ThrowFrom;
		objects.push_back(object);
		throws.push_back(Inward(i, angle, 1.0f, 12.0f));
	}
	all.push_back({
	    .id = "physics.throw_many",
	    .name = "Sixty things thrown together",
	    .facet = Facet::Physics,
	    .description = "Sixty rocks, boulders, pots, piles, barrels, villagers, trees and balls on a ring are all thrown "
	                   "at once towards its middle, and again every twelve seconds.",
	    .expected = "They fly in, strike one another and pile up in the middle, then come to rest one by one; once they "
	                "have, the physics costs nothing until the next throw.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddBuildingBreak(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	constexpr std::array<float, 3> k_Huts {-20.0f, 0.0f, 20.0f};
	for (const float x : k_Huts)
	{
		objects.push_back({.type = AbodeInfo::CelticHut, .offset = k_Field + glm::vec2 {x, 10.0f}});
	}
	for (size_t i = 0; i < k_Huts.size() * 2; ++i)
	{
		const float x = k_Huts.at(i % k_Huts.size()) + (i < k_Huts.size() ? -2.0f : 2.0f);
		objects.push_back({.type = MobileStaticInfo::Boulder2Chalk, .offset = k_Field + glm::vec2 {x, -20.0f}});
		throws.push_back({
		    .object = objects.size() - 1,
		    .from = k_Field + glm::vec2 {x, -20.0f},
		    .height = 4.0f,
		    .velocity = {0.0f, 4.0f, 20.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 0.5f,
		    .repeatSeconds = 6.0f,
		});
	}
	all.push_back({
	    .id = "physics.building_breaks",
	    .name = "Huts broken by thrown boulders",
	    .facet = Facet::Physics,
	    .description = "Three huts in a row, and two boulders thrown at each from the south, again and again.",
	    .expected = "A boulder knocks holes in a hut with a knock or a crash, pieces of it fly off and come to rest, and they "
	                "fade away in time.",
	    .environment = {.dispenserGrid = false},
	    // Closer than the overview of the field: the knocks and crashes don't start further than 150 m from the camera
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}},
	                .distance = k_WithinEarshot},
	    .objects = objects,
	    .throws = throws,
	});
}

/// Six big rocks in a row across the field, each hurled down at the ground from high up, again and again
void HurlRocks(std::vector<ObjectSetup>& objects, std::vector<ThrowSetup>& throws)
{
	for (size_t i = 0; i < 6; ++i)
	{
		const glm::vec2 offset = k_Field + glm::vec2 {(static_cast<float>(i) - 2.5f) * 12.0f, 0.0f};
		objects.push_back({.type = MobileStaticInfo::RockVolcanic, .offset = offset, .scale = 2.0f});
		throws.push_back({
		    .object = i,
		    .from = offset,
		    .height = 30.0f,
		    .velocity = {0.0f, -60.0f, 0.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 0.3f,
		    .repeatSeconds = 5.0f,
		});
	}
}

/// A hut of the town broken by a boulder, then repaired by its villagers, and struck again while they repair it
void AddBuildingRepaired(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticHut, .offset = k_Field + glm::vec2 {0.0f, 10.0f}},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = k_Field + glm::vec2 {-12.0f, 20.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticForesterMale, .offset = k_Field + glm::vec2 {12.0f, 20.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Field + glm::vec2 {0.0f, 28.0f}, .joinTown = true},
	    {.type = MobileStaticInfo::Boulder2Chalk, .offset = k_Field + glm::vec2 {-2.0f, -20.0f}},
	    {.type = MobileStaticInfo::Boulder2Chalk, .offset = k_Field + glm::vec2 {2.0f, -20.0f}},
	    // the town's store, whose wood the builders take to the repair
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Field + glm::vec2 {-20.0f, 30.0f}},
	};
	std::vector<ThrowSetup> throws {
	    // the first breaks it; the second comes back again and again while it is repaired
	    {.object = 4,
	     .from = k_Field + glm::vec2 {-2.0f, -20.0f},
	     .height = 4.0f,
	     .velocity = {0.0f, 4.0f, 20.0f},
	     .delaySeconds = 1.0f},
	    {.object = 5,
	     .from = k_Field + glm::vec2 {2.0f, -20.0f},
	     .height = 4.0f,
	     .velocity = {0.0f, 4.0f, 20.0f},
	     .delaySeconds = 30.0f,
	     .repeatSeconds = 15.0f},
	};
	all.push_back({
	    .id = "physics.building_repaired",
	    .name = "A broken hut repaired, then struck again",
	    .facet = Facet::Physics,
	    .description = "A hut of the town with three of its villagers and its store of wood, struck by a boulder; from "
	                   "30 s another boulder strikes it every 15 s.",
	    .expected = "The first boulder knocks holes in the hut and the town makes it a repair site; its villagers walk "
	                "to it and build, and its life rises. A strike while it is still less than a fifth repaired breaks "
	                "the broken hut further; one after it is a fifth repaired or more puts the hut back whole first, as "
	                "it was before the first blow, and breaks that. Struck again while its life is very low, the hut "
	                "is destroyed.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}},
	                .distance = k_WithinEarshot},
	    .objects = objects,
	    .logBuildingsEvery = 5.0f,
	    .throws = throws,
	});
}

void AddRockSplit(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	HurlRocks(objects, throws);
	all.push_back({
	    .id = "physics.rock_split",
	    .name = "Rocks hurled at the ground",
	    .facet = Facet::Physics,
	    .description = "Six big rocks hurled down at the ground from high up, again and again.",
	    .expected = "Each hard landing wears a rock down, and a worn rock splits into halves that fly apart and come to "
	                "rest.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddThrownVillagerLands(std::vector<Scenario>& all)
{
	// A villager of the scenario's town, thrown from the west across the flat field as the other throws are, lands hard
	// near the field's middle, well away from the lake
	all.push_back({
	    .id = "physics.thrown_villager_lands",
	    .name = "A thrown villager lands",
	    .facet = Facet::Physics,
	    .description = "A villager of the town is thrown hard across the flat field from the west every ten seconds, and "
	                   "lands on dry land near the field's middle.",
	    .expected = "It flies in an arc and lands hard: each landing takes some of its life. It comes to rest, lying on its "
	                "feet, side or back, gets up where it lies and goes on with what it was doing, until a landing takes "
	                "the last of its life.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Field - glm::vec2 {k_ThrowFrom + 5.0f, 15.0f}, k_Field + glm::vec2 {15.0f}}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = k_Field, .joinTown = true}},
	    .throws = {Inward(0, std::numbers::pi_v<float>, 1.0f, 10.0f)},
	});
}

void AddBurningRockSplit(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	HurlRocks(objects, throws);
	testbed_fixtures::Fixtures fixtures;
	for (size_t i = 0; i < objects.size(); ++i)
	{
		fixtures.fires.push_back({.what = i});
	}
	all.push_back({
	    .id = "physics.burning_rock_split",
	    .name = "Burning rocks hurled at the ground",
	    .facet = Facet::Physics,
	    .description = "The six big rocks of the rocks hurled at the ground, each set alight before the first throw. Run "
	                   "with OPENBLACK_FIRE_TRACE=1 to log the fires passed on and the blazes they leave. A baseline "
	                   "for fire on rocks: a rock never reaches its combustion point, so none burns and none splits "
	                   "while lit, and the split's fire order shows nowhere yet.",
	    .expected = "A burning rock that splits passes its fire to both halves, each as hot as the rock was. The halves "
	                "burn on as one blaze: no half's fire leaves the blaze as the halves fly apart, and the rock's own "
	                "fire goes with the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .objects = objects,
	    .throws = throws,
	    .fixtures = fixtures,
	});
}

void AddVillagersWatchThrow(std::vector<Scenario>& all)
{
	// A rock lobbed slowly from the west of the field over villagers of the town: the near ones are closer than it flies
	// in two seconds, the far ones further, and the last one stands beyond the reach of a thrown thing. It is let go in
	// the middle of a cell of the map, and each villager stands in the middle of a cell whole cells from it: a thrown
	// thing's reaction reaches the cells whose middles are within 25 m of it
	const glm::vec2 release = k_Field + glm::vec2 {-15.0f, 5.0f};
	all.push_back({
	    .id = "physics.villagers_watch_throw",
	    .name = "Villagers watch a thrown rock",
	    .facet = Facet::Physics,
	    .description = "A rock is lobbed gently eastwards every eight seconds from the west of the field, over five villagers "
	                   "of the town: a man and a woman stand close to where it is let go, a man and a woman further off, "
	                   "and a man far to the west.",
	    .expected = "The man and the woman close by run from the rock; the two further off turn to it and point, look about "
	                "or are scared stiff, and go back to what they were doing once it has come to rest. The man far to "
	                "the west takes no notice.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {release - glm::vec2 {65.0f, 25.0f}, release + glm::vec2 {25.0f}}},
	    .objects =
	        {{.type = MobileStaticInfo::RockChalk, .offset = release},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = release + glm::vec2 {0.0f, 10.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticHousewifeFemale, .offset = release + glm::vec2 {-10.0f, 0.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = release + glm::vec2 {0.0f, 20.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticHousewifeFemale, .offset = release + glm::vec2 {20.0f, -10.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = release - glm::vec2 {60.0f, 0.0f}, .joinTown = true}},
	    // Let go 4 m up at about 7.2 m/s: it flies 14.4 m in two seconds, and lands a few metres east
	    .throws = {{.object = 0,
	                .from = release,
	                .height = 4.0f,
	                .velocity = {4.0f, 6.0f, 0.0f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 8.0f}},
	});
}

void AddThrownRockCaught(std::vector<Scenario>& all)
{
	// The same lob once, past two wolves close to where it is let go, nearer than twice its speed: of the land's animals
	// only the predators react to a flying thing, and a fleeing animal looks for the reaction every turn. The player's
	// hand, held still to the south east, takes the rock out of the air soon after its peak
	const glm::vec2 release = k_Field + glm::vec2 {-15.0f, 5.0f};
	all.push_back({
	    .id = "physics.thrown_rock_caught",
	    .name = "A thrown rock caught in the air",
	    .facet = Facet::Physics,
	    .description = "A rock is lobbed gently eastwards from the west of the field, past two wolves standing 5 and 7 m "
	                   "from where it is let go; 0.6 s later, still in the air, the player's hand, held still to the south "
	                   "east, takes it.",
	    .expected = "The wolves take the rock's flying-object reaction as it flies. Once the hand has caught it, the "
	                "rock's flying-object reactions end at once: the log's reactions line no longer lists a type 9 for "
	                "object 0 from the catch, and the wolves no longer follow it, while the rock stays in the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {release - glm::vec2 {25.0f}, release + glm::vec2 {40.0f, 25.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = release},
	                {.type = AnimalInfo::Wolf, .offset = release + glm::vec2 {3.0f, -4.0f}},
	                {.type = AnimalInfo::Wolf, .offset = release + glm::vec2 {0.0f, 7.0f}}},
	    .commands = {{.kind = Command::Kind::HandTake, .delaySeconds = 2.6f, .object = 0}},
	    .hand = HandHold {.offset = release + glm::vec2 {30.0f, -15.0f}, .height = 6.0f},
	    // the readout: the rock's flying-object reaction (type 9) goes from its list at the catch
	    .logReactions = true,
	    // Let go 4 m up at 4 m/s along and 6 m/s up: at its peak 0.6 s later, landing about 1.7 s after the throw
	    .throws = {{.object = 0, .from = release, .height = 4.0f, .velocity = {4.0f, 6.0f, 0.0f}, .delaySeconds = 2.0f}},
	});
}

void AddRockStrikesCreature(std::vector<Scenario>& all)
{
	// A grown tiger of the player's stands in the middle of the field, its mind left alone so that it stays where it
	// is. A rock thrown from the west reaches it a few metres above the land; a toy ball is thrown the same way, a
	// little to the north, between the rock's throws
	const auto ball = k_Field + glm::vec2 {0.0f, 4.0f};
	ThrowSetup toy = Inward(1, std::numbers::pi_v<float>, 5.0f, 8.0f);
	toy.from = *toy.from + glm::vec2 {0.0f, 4.0f};
	all.push_back({
	    .id = "physics.rock_strikes_creature",
	    .name = "A rock thrown at a creature",
	    .facet = Facet::Physics,
	    .description = "The player's tiger stands in the middle of the field. A rock is thrown at it from the "
	                   "west every eight seconds, and a toy ball the same way four seconds after each rock. "
	                   "The creature spawner shows its life (body), its attitude to the player (hand), and the "
	                   "sources of its desires on hovering over fear and anger: fear from damage is source 18, "
	                   "anger from damage source 9.",
	    .expected = "A rock that strikes it hard enough takes some of its life, and makes it think less of the "
	                "player, more afraid and angrier: both sources grow with every such strike, by a hundredth "
	                "of how many times its own weight the strike was. A strike too soft to count hurts nothing. "
	                "Every rock's strike, even a soft one, leans its body the way the rock pushed it, and the "
	                "body swings back and settles within a few seconds. The toy ball never sways it, hurts it, "
	                "nor changes its attitude, its fear or its anger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Field - glm::vec2 {k_ThrowFrom + 5.0f, 15.0f}, k_Field + glm::vec2 {15.0f}}},
	    .creatures = {{.label = "struck", .offset = k_Field, .pauseMind = true}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = k_Field - glm::vec2 {k_ThrowFrom, 0.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = ball - glm::vec2 {k_ThrowFrom, 0.0f}}},
	    .throws = {Inward(0, std::numbers::pi_v<float>, 1.0f, 8.0f), toy},
	});
}

void AddSinking(std::vector<Scenario>& all)
{
	// One of each kind the physics treats differently in the water, dropped side by side into the lake's open water
	const glm::vec2 lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const std::array<ObjectSetup, 12> kinds {
	    ObjectSetup {.type = MobileStaticInfo::RockChalk},
	    ObjectSetup {.type = MobileStaticInfo::Boulder1Lime},
	    ObjectSetup {.type = MobileStaticInfo::RockVolcanic, .scale = 2.0f},
	    ObjectSetup {.type = MobileObjectInfo::EgyptPotA},
	    ObjectSetup {.type = MobileObjectInfo::WaterJug},
	    ObjectSetup {.type = MobileObjectInfo::Champi},
	    ObjectSetup {.type = MobileObjectInfo::EgyptBarrel},
	    ObjectSetup {.type = TreeInfo::Birch, .scale = 0.6f},
	    ObjectSetup {.type = MobileObjectInfo::Ball},
	    ObjectSetup {.type = MobileStaticInfo::SharprockSandstone},
	    ObjectSetup {.type = VillagerInfo::CelticFarmerMale},
	    ObjectSetup {.type = AnimalInfo::Sheep},
	};
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < kinds.size(); ++i)
	{
		const glm::vec2 at = lake + glm::vec2 {(static_cast<float>(i % 6) - 2.5f) * 12.0f, i < 6 ? -10.0f : 10.0f};
		auto object = kinds.at(i);
		// They wait on the bank south of the lake until they are dropped
		object.offset = lake + glm::vec2 {(static_cast<float>(i) - 5.5f) * 6.0f, -flat_land::k_LakeHalfExtent.y - 40.0f};
		objects.push_back(object);
		throws.push_back({
		    .object = i,
		    .from = at,
		    .height = 8.0f,
		    .velocity = {0.0f, -2.0f, 0.0f},
		    .delaySeconds = 1.0f + static_cast<float>(i) * 0.2f,
		});
	}
	all.push_back({
	    .id = "physics.sinking",
	    .name = "Things dropped into the sea",
	    .facet = Facet::Physics,
	    .description = "Rocks, boulders, a pot, a water jug, a mushroom, a barrel, a tree, a ball, a villager and a sheep are "
	                   "dropped side by side into the lake's open water.",
	    .expected = "Each splashes in; light things float and bob until they soak up enough water, heavy ones sink. A thing "
	                "that sinks is hidden by the water as it goes under and is gone once it is deep below; the villager "
	                "drowns and the sheep dies.",
	    .environment = {.dispenserGrid = false},
	    // Low over the lake's south shore, looking across the water at where they go in
	    .framing = {.shot = Shot::Placed, .eye = {lake.x - 10.0f, 9.0f, lake.y - 45.0f}, .look = {lake.x + 4.0f, 0.0f, lake.y}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddCreatureHit(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < 8; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / 8.0f;
		objects.push_back({.type = MobileStaticInfo::RockChalk,
		                   .offset = k_Field + glm::vec2 {std::cos(angle), std::sin(angle)} * k_ThrowFrom});
		throws.push_back(Inward(i, angle, 2.0f + static_cast<float>(i), 10.0f));
	}
	all.push_back({
	    .id = "physics.creature_hit",
	    .name = "A creature struck by thrown rocks",
	    .facet = Facet::Physics,
	    .description = "A cow stands in the middle of a ring of rocks, which are thrown at it one after another.",
	    .expected = "A rock that strikes it hurts it and it sways from the blow; the rocks bounce off and come to rest.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}}},
	    .creatures = {{.species = CreatureType::Cow, .offset = k_Field}},
	    .objects = objects,
	    .throws = throws,
	});
}

void AddCreatureCatch(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "physics.creature_catches",
	    .name = "A creature catching thrown villagers",
	    .facet = Facet::Physics,
	    .description = "Villagers are thrown at a tiger, one straight at it and one passing nine metres to "
	                   "its side, every ten seconds.",
	    .expected = "As each villager flies the tiger stops what it is doing and turns to it, waits breathing, steps across "
	                "to where the wide one will pass (moving as it steps), and catches each in its hand as it arrives, "
	                "or gives up when one has gone past it or landed short.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {20.0f}, k_Field + glm::vec2 {20.0f, 40.0f}}},
	    .creatures = {{.species = CreatureType::Tiger, .offset = k_Field, .facingDegrees = 180.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = k_Field + glm::vec2 {-10.0f, 34.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Field + glm::vec2 {10.0f, 34.0f}}},
	    .throws = {{.object = 0,
	                .from = k_Field + glm::vec2 {0.0f, 20.0f},
	                .height = 14.0f,
	                .velocity = {0.0f, 4.0f, -22.0f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 10.0f},
	               {.object = 1,
	                .from = k_Field + glm::vec2 {8.8f, 40.0f},
	                .height = 14.0f,
	                .velocity = {0.0f, 8.0f, -24.0f},
	                .delaySeconds = 8.0f,
	                .repeatSeconds = 10.0f}},
	});
}

void AddTempleStruck(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects;
	std::vector<ThrowSetup> throws;
	for (size_t i = 0; i < 6; ++i)
	{
		const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / 6.0f;
		objects.push_back({.type = MobileStaticInfo::Boulder2Chalk,
		                   .offset = k_Field + glm::vec2 {std::cos(angle), std::sin(angle)} * k_ThrowFrom});
		throws.push_back(Inward(i, angle, 2.0f + static_cast<float>(i) * 1.5f, 9.0f));
	}
	all.push_back({
	    .id = "physics.temple_struck",
	    .name = "A temple struck by thrown boulders",
	    .facet = Facet::Physics,
	    .description = "Another player's temple, with no town to pass its blows on to, stands in a ring of boulders thrown at "
	                   "it one after another.",
	    .expected =
	        "Every two seconds at most, a boulder striking it makes its heart fire a plasma beam from its top down onto "
	        "a point of its own surface, and the heart loses life; at none left, beams leap across it for fourteen "
	        "seconds, it glows, fades and explodes, and is gone in smoke 22 seconds on.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Field - glm::vec2 {40.0f}, k_Field + glm::vec2 {40.0f}},
	                .distance = k_WithinEarshot},
	    .objects = objects,
	    .throws = throws,
	    .fixtures = {.temples = {{.at = k_Field, .owner = PlayerNames::PLAYER_TWO}}},
	});
}

void AddFlyingReflections(std::vector<Scenario>& all)
{
	// A villager and a sheep thrown straight up over the lake's open water, in front of a camera low on its south shore
	const glm::vec2 lake = flat_land::k_LakeCentre - flat_land::k_MapMiddle;
	const glm::vec2 bank {lake.x, lake.y - flat_land::k_LakeHalfExtent.y - 40.0f};
	all.push_back({
	    .id = "physics.flying_reflections",
	    .name = "A thrown villager and sheep reflected in the sea",
	    .facet = Facet::Physics,
	    .description = "A villager and then a sheep are thrown straight up over the lake's open water, seen from low on "
	                   "its south shore, where the sea reflects what flies in the physics.",
	    .expected = "While each flies its mirror image shows in the water below it in the same pose as its body, its "
	                "limbs moving as they do, never standing stiff; the villager drowns and the sheep dies once they fall "
	                "in.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {lake.x, 4.0f, lake.y - 45.0f}, .look = {lake.x, 6.0f, lake.y - 10.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = bank - glm::vec2 {6.0f, 0.0f}},
	                {.type = AnimalInfo::Sheep, .offset = bank + glm::vec2 {6.0f, 0.0f}}},
	    .throws = {{.object = 0, .from = lake - glm::vec2 {4.0f, 15.0f}, .height = 6.0f, .velocity = {0.0f, 8.0f, 0.0f}},
	               {.object = 1,
	                .from = lake + glm::vec2 {4.0f, -15.0f},
	                .height = 6.0f,
	                .velocity = {0.0f, 8.0f, 0.0f},
	                .delaySeconds = 5.0f}},
	});
}

void AddTailOrder(std::vector<Scenario>& all)
{
	// A villager that dies before a low camera, its soul rising between the camera and, behind it, a chest falling from
	// the sky and a rock thrown up
	all.push_back({
	    .id = "physics.tail_order",
	    .name = "A soul in front of a falling chest and a thrown rock",
	    .facet = Facet::Physics,
	    .description = "A villager with no life left dies 12 metres in front of a low camera and its soul rises. Behind "
	                   "it a script drops a chest from the sky a second in, and two seconds in a rock is thrown straight "
	                   "up behind the soul.",
	    .expected = "The soul is drawn after the chest falling from the sky and after the rock in the physics, as the "
	                "original draws its frame: where they are behind the see-through soul, the soul shows the land and "
	                "the sky behind them instead of them. With the View menu's Object Tail Unplaced on, both show "
	                "through the soul, as before.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 3.0f, -12.0f}, .look = {0.0f, 6.0f, 20.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}, .life = 0.0f},
	                {.type = MobileObjectInfo::Ball, .offset = {8.0f, 20.0f}}},
	    .commands =
	        {{.kind = Command::Kind::ScriptReward, .delaySeconds = 1.0f, .point = {0.0f, 25.0f}, .value = 3, .fromSky = true}},
	    .throws = {{.object = 1,
	                .from = glm::vec2 {0.0f, 20.0f},
	                .height = 2.0f,
	                .velocity = {0.0f, 9.0f, 0.0f},
	                .delaySeconds = 2.0f}},
	});
}
void AddImmovableLetGo(std::vector<Scenario>& all)
{
	// Two rocks side by side on the field, the west one made immovable as a script makes it; each is lifted about 10 m
	// up and let go of with no speed, as the player's hand lets go of a thing
	constexpr float k_Apart = 4.0f;
	constexpr float k_Height = 10.0f;
	all.push_back({
	    .id = "physics.immovable_let_go",
	    .name = "An immovable rock let go in the air",
	    .facet = Facet::Physics,
	    .description = "Two rocks lie side by side on the field; a script has made the west one immovable. A second in, "
	                   "each is lifted about 10 m up and let go of with no speed, as the hand lets go of a thing.",
	    .expected = "The movable rock falls, lands and comes to rest. The immovable one hangs in the air where it was let "
	                "go, as in the original: the physics gives it no body, and nothing puts it down or back in the map. A "
	                "hand that tries to pick it up again takes nothing, and it stays up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Field - glm::vec2 {15.0f}, k_Field + glm::vec2 {15.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = k_Field - glm::vec2 {k_Apart, 0.0f}, .immovable = true},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_Field + glm::vec2 {k_Apart, 0.0f}}},
	    .throws = {{.object = 0, .height = k_Height, .delaySeconds = 1.0f},
	               {.object = 1, .height = k_Height, .delaySeconds = 1.0f}},
	});
}

void AddTreeReplanted(std::vector<Scenario>& all)
{
	// An oak alone on the field, far from any town or temple, lifted a little to a point beside it and let go of with
	// no speed and upright, as the player's hand puts a tree down gently; by a good player and by an evil one
	const glm::vec2 plantAt = k_Field + glm::vec2 {6.0f, 0.0f};
	struct Player
	{
		std::string_view id;
		std::string_view name;
		std::string_view description;
		float alignment;
	};
	constexpr std::array k_Players {
	    Player {.id = "physics.tree_replanted_good",
	            .name = "A tree replanted by a good player's hand",
	            .description = "An oak stands alone on open land, with no town or temple near. Two seconds in, the hand "
	                           "of a wholly good player lifts it a little to a point six metres east and lets go of it "
	                           "gently, upright and with no speed.",
	            .alignment = 1.0f},
	    Player {.id = "physics.tree_replanted_evil",
	            .name = "A tree replanted by an evil player's hand",
	            .description = "An oak stands alone on open land, with no town or temple near. Two seconds in, the hand "
	                           "of a wholly evil player lifts it a little to a point six metres east and lets go of it "
	                           "gently, upright and with no speed.",
	            .alignment = -1.0f},
	};
	for (const auto& player : k_Players)
	{
		all.push_back({
		    .id = player.id,
		    .name = player.name,
		    .facet = Facet::Physics,
		    .description = player.description,
		    .expected = "The oak is planted again at once where it was let go, standing on the land: a puff of smoke on "
		                "the ground under it, the planting sound, and the forest-created visual over it, green sparkles "
		                "rising and a pale swirl swelling and turning for about two seconds. It is the same visual for a "
		                "good and an evil player, as in the original: the effect takes no colour from its player (the "
		                "sky and the land's light differ, as they follow the player's alignment). The tree starts a "
		                "forest of its own, and the player's alignment moves a little towards good.",
		    .environment = {.dispenserGrid = false, .playerAlignment = player.alignment},
		    .framing = {.shot = Shot::Placed,
		                .eye = {plantAt.x, 14.0f, plantAt.y - 40.0f},
		                .look = {plantAt.x, 5.0f, plantAt.y}},
		    .objects = {{.type = TreeInfo::Oak, .offset = k_Field}},
		    .throws = {{.object = 0, .from = plantAt, .height = 2.0f, .delaySeconds = 2.0f}},
		});
	}
}

} // namespace

void testbed_scenarios::AddPhysicsScenarios(std::vector<Scenario>& all)
{
	AddIdleField(all);
	AddOneThrow(all);
	AddCrowdThrow(all);
	AddBuildingBreak(all);
	AddBuildingRepaired(all);
	AddRockSplit(all);
	AddBurningRockSplit(all);
	AddSinking(all);
	AddFlyingReflections(all);
	AddTailOrder(all);
	AddThrownVillagerLands(all);
	AddVillagersWatchThrow(all);
	AddThrownRockCaught(all);
	AddRockStrikesCreature(all);
	AddCreatureHit(all);
	AddCreatureCatch(all);
	AddTempleStruck(all);
	AddImmovableLetGo(all);
	AddTreeReplanted(all);
}
