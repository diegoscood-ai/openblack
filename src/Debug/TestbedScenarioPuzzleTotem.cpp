/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the puzzle totems a script makes: the fourth land's bell towers, which the player clicks,
// and the undead village's totem, which the hand drags up out of the ground

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// Where the middle tower stands; the others stand 10 m apart in a row across the camera's view
constexpr glm::vec2 k_Middle {0.0f, 40.0f};
/// Where the town whose influence the hand works in stands, east of the totem
constexpr glm::vec2 k_Town {18.0f, 48.0f};
} // namespace

void testbed_scenarios::AddPuzzleTotemScenarios(std::vector<Scenario>& all)
{
	// The bell towers as the fourth land's Totem Puzzle makes them: CREATE_WITH_ANGLE_AND_SCALE of a totem of the bell
	// tower's info at the script's five angles, scale 1, each raised to its full height and made unpickupable. They
	// stand in a row here, not in the land's arc
	const std::vector<float> angles {340.0f, 10.0f, 50.0f, 80.0f, 100.0f};
	std::vector<Command> towers;
	for (size_t i = 0; i < angles.size(); ++i)
	{
		towers.push_back({.kind = Kind::ScriptPuzzleTotem,
		                  .delaySeconds = i == 0 ? 1.0f : 0.0f,
		                  .point = k_Middle + glm::vec2(10.0f * (static_cast<float>(i) - 2.0f), 0.0f),
		                  .value = static_cast<size_t>(AbodeInfo::Belltower),
		                  .amount = 1.0f,
		                  .built = 1.0f,
		                  .angleDegrees = angles.at(i),
		                  .unpickupable = true});
	}
	towers.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 1.0f});
	towers.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.45f}});
	towers.push_back({.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3});
	towers.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3});
	towers.push_back({.kind = Kind::PuzzleTotemHeights, .delaySeconds = 1.0f});
	all.push_back({
	    .id = "story.bell_towers",
	    .name = "The fourth land's bell towers, made as its script makes them",
	    .facet = Facet::Objects,
	    .description = "Five bell towers made by a script's CREATE of a totem (the bell tower's abode info, the Totem "
	                   "Puzzle's five angles, scale 1), each set to its full HEIGHT and made unpickupable, in a row 10 m "
	                   "apart north of the camera. The pointer then clicks the middle one with the Action (right) "
	                   "button.",
	    .expected = "The log gives each tower as made, with HEIGHT 100 of MAX_HEIGHT 100, and again after the click. "
	                "The five towers stand on the land at their full height, each turned its own way, and none sunk. "
	                "The click takes no hold of the middle tower and knocks on nothing: the hand stays as it was, and "
	                "the tower is remembered as the thing clicked, as the Totem Puzzle's script reads it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 15.0f, 10.0f}, .look = {0.0f, 6.0f, 40.0f}},
	    .commands = towers,
	});

	// The undead village's totem as its script makes it: CREATE_WITH_ANGLE_AND_SCALE of a totem of the African totem's
	// info, turned 36 degrees at scale 2.233, sunk to HEIGHT 0. A town of the player's stands beside it, so that the
	// hand is in the player's influence there
	all.push_back({
	    .id = "story.undead_totem_drag",
	    .name = "The hand drags the undead village's totem up out of the ground",
	    .facet = Facet::Objects,
	    .description = "An African totem made by a script's CREATE of a totem (scale 2.233, turned 36 degrees) with its "
	                   "HEIGHT set to 0, beside a Celtic town centre of the player's town, which has one villager. The "
	                   "pointer goes to the totem's top, the Action (right) button goes down on it, the mouse moves up "
	                   "30% of the screen over two seconds, and the button is let go half a second later.",
	    .expected = "Made, the totem stands sunk into the ground by 0.65 of its height, HEIGHT 0 of MAX_HEIGHT 100. The "
	                "press takes hold of it (the log says it is held by the hand), and sliding the mouse up raises it "
	                "with the hand in whole steps, never above where it was made. Let go, it keeps the height it was "
	                "let go at (the log's HEIGHT after the release), at times one step below the step the hand showed, "
	                "as in the original.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, 20.0f}, .look = {0.0f, 3.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Town},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Town + glm::vec2(8.0f, -8.0f), .joinTown = true}},
	    .commands = {{.kind = Kind::ScriptPuzzleTotem,
	                  .delaySeconds = 1.0f,
	                  .point = k_Middle,
	                  .value = static_cast<size_t>(AbodeInfo::AfricanTotem),
	                  .amount = 2.233f,
	                  .built = 0.0f,
	                  .angleDegrees = 36.0f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.55f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PuzzleTotemHeights, .delaySeconds = 0.4f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.3f}, .amount = 2.0f},
	                 {.kind = Kind::PuzzleTotemHeights, .delaySeconds = 2.2f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PuzzleTotemHeights, .delaySeconds = 1.0f}},
	    .townBelief = 10.0f,
	});
}
