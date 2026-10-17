/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the creatures' gate on the first land: the Norse gate and the gate stone plinth beside it,
// placed as the land's script places them, the three carved stones laid in the plinth as the hand gives them, and the
// plinth and the gate opened and closed as the choosing script does, and a creature walking past the gate shut and open

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The gate stands in the middle; the plinth is where the first land puts it from the gate, and both are turned as
/// there (5.953 radians)
constexpr glm::vec2 k_Plinth {-6.53f, -17.33f};
constexpr float k_Turn = 341.08f;
/// The objects by their place in the scenario
constexpr size_t k_GateObject = 0;
constexpr size_t k_PlinthObject = 1;
constexpr size_t k_TigerStone = 2;
constexpr size_t k_ApeStone = 3;
constexpr size_t k_CowStone = 4;

std::vector<ObjectSetup> GateAndStones()
{
	return {
	    {.type = AnimatedStaticInfo::NorseGate, .yawDegrees = k_Turn},
	    {.type = AnimatedStaticInfo::GateStonePlinth, .offset = k_Plinth, .yawDegrees = k_Turn},
	    {.type = MobileStaticInfo::GateTotemTiger, .offset = k_Plinth + glm::vec2(-8.0f, -4.0f)},
	    {.type = MobileStaticInfo::GateTotemApe, .offset = k_Plinth + glm::vec2(-4.0f, -8.0f)},
	    {.type = MobileStaticInfo::GateTotemCow, .offset = k_Plinth + glm::vec2(4.0f, -8.0f)},
	};
}

Framing GateView()
{
	return {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, k_Plinth}, .distance = 1.5f};
}

Command Lay(size_t stone, float delay)
{
	return {.kind = Kind::LayGateStone, .delaySeconds = delay, .value = k_PlinthObject, .object = stone};
}

Command OpenClose(size_t object, bool open, float delay)
{
	return {.kind = Kind::SetOpenClose, .delaySeconds = delay, .value = open ? size_t {1} : size_t {0}, .object = object};
}

/// The creature walking to a point from the middle of the map, once it has arrived where it went before
Command Walk(glm::vec2 point, float delay)
{
	return {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = delay, .waitUntilFree = true, .point = point};
}
} // namespace

void testbed_scenarios::AddGateScenarios(std::vector<Scenario>& all)
{
	// The hand, not a script, lays a stone: the pointer takes the tiger stone beside the plinth with the right button
	// and gives it to the plinth with another press, then moves away. The camera looks steeply down on both, the plinth
	// left of the middle and the stone right of it
	all.push_back({
	    .id = "objects.gate_stone_by_hand",
	    .name = "The hand lays a gate stone in the plinth",
	    .facet = Facet::Objects,
	    .description = "The gate stone plinth with the tiger stone a little to its east. The hand picks the stone up with "
	                   "the right button, carries it over the plinth and presses again, then moves away.",
	    .expected = "Pressed over the plinth, the stone leaves the hand: it vanishes, leaving a fading ghost, as the "
	                "plinth takes it (worth 2 to the scripts).",
	    .framing = {.shot = Shot::Placed, .eye = {3.0f, 30.0f, -12.0f}, .look = {3.0f, 0.0f, 0.0f}},
	    .objects = {{.type = AnimatedStaticInfo::GateStonePlinth},
	                {.type = MobileStaticInfo::GateTotemTiger, .offset = {6.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.573f, 0.466f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.427f, 0.466f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.15f, 0.85f}}},
	});
	all.push_back({
	    .id = "objects.creature_gate_open",
	    .name = "The creatures' gate opens",
	    .facet = Facet::Objects,
	    .description = "The Norse gate and the gate stone plinth stand as on the first land, with the tiger, ape and cow "
	                   "gate stones beside the plinth. The stones are laid in the plinth one by one, then the plinth is "
	                   "opened and, two seconds later, the gate, as the choosing script does once all three are in.",
	    .expected = "Each stone vanishes from the ground, leaving a fading ghost, and appears on the plinth's top, stacked "
	                "one above the other. Opened, the plinth sinks over about ten seconds and the stones sink down into it "
	                "until they are gone. The gate's two doors swing open over about ten seconds and stay open.",
	    .framing = GateView(),
	    .objects = GateAndStones(),
	    .commands = {Lay(k_TigerStone, 2.0f), Lay(k_ApeStone, 2.0f), Lay(k_CowStone, 2.0f),
	                 OpenClose(k_PlinthObject, true, 3.0f), OpenClose(k_GateObject, true, 2.0f)},
	});

	all.push_back({
	    .id = "objects.creature_gate_close",
	    .name = "The creatures' gate opens and closes again",
	    .facet = Facet::Objects,
	    .description = "The Norse gate and the gate stone plinth with one stone laid in it are opened, and closed again "
	                   "twelve seconds later, as the scripts may.",
	    .expected = "The gate swings open and the plinth sinks with its stone; once closed, each plays back to where it "
	                "began: the gate shut and the plinth risen, the stone showing on its top again, drawn both stacked "
	                "and sinking while the plinth comes back up.",
	    .framing = GateView(),
	    .objects = GateAndStones(),
	    .commands = {Lay(k_TigerStone, 1.0f), OpenClose(k_PlinthObject, true, 1.0f), OpenClose(k_GateObject, true, 0.0f),
	                 OpenClose(k_PlinthObject, false, 12.0f), OpenClose(k_GateObject, false, 0.0f)},
	});

	// A creature's route past the gate: round it while it is shut, through its middle once it stands open. The gate
	// lies across the walk, east to west, with the creature 50 units south of it
	constexpr glm::vec2 k_South {0.0f, -50.0f};
	constexpr glm::vec2 k_North {0.0f, 50.0f};
	Command open = OpenClose(k_GateObject, true, 1.0f);
	open.waitUntilFree = true;
	all.push_back({
	    .id = "creature.walks_past_creature_gate",
	    .name = "A creature walking past the creatures' gate",
	    .facet = Facet::Movement,
	    .description = "A small tiger 50 units south of a shut Norse gate, which lies across its way east to west, walks "
	                   "to 50 units north of it. Once there the gate is opened, as the choosing script opens it, and "
	                   "twelve seconds later the tiger walks back south.",
	    .expected = "Shut, the gate is a wall of small circles to the route planner, so the tiger's route bends round one "
	                "end of it rather than through it. Once the gate stands open and still, its middle five circles are "
	                "left out and the walk back goes straight through the middle of the gate. Until the gate's clip "
	                "plays, an opened gate counts as still opening, so the walk back goes round it too.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -60.0f}, {40.0f, 60.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = k_South,
	        .facingDegrees = 180.0f,
	        .size = 0.6f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f},
	        .hold = true,
	        .pauseMind = true}},
	    .objects = {{.type = AnimatedStaticInfo::NorseGate}},
	    .commands = {Walk(k_North, 1.0f), open, Walk(k_South, 12.0f)},
	});
}
