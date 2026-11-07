/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the teleport miracle: two stones cast far apart by hand, villagers walking from one to the
// other that take the shortcut, and a villager put down on a stone that jumps at once; and the two stones cast straight
// at their points, with a villager of the scenario's town routed through them

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The two stones, south-west and north-east of the middle
constexpr glm::vec2 k_StoneA {-40.0f, 20.0f};
constexpr glm::vec2 k_StoneB {40.0f, 70.0f};
/// The villagers start beside the first and walk to beside the second, and back
constexpr glm::vec2 k_Start {-34.0f, 24.0f};
constexpr glm::vec2 k_Goal {46.0f, 74.0f};
/// They set off once both stones are down, and turn back after this long
constexpr float k_WalkAfter = 5.0f;
constexpr float k_WalkRepeat = 30.0f;
/// Two stones too far apart for a villager to walk between to worship
constexpr glm::vec2 k_WorshipStoneA {-300.0f, 0.0f};
constexpr glm::vec2 k_WorshipStoneB {300.0f, 0.0f};
/// The straight casts' walker starts beyond the first stone and walks to past the second, on the line through both.
/// A stone's reaction reaches the walkers only as the stone is made, so the second stone is cast first and the first
/// once the villager is on its way, within the reaction's reach of 35 (half the sum of the distances along x and z:
/// (20 + 12) / 2 = 16 from the start). The game's rule for taking the detour, on its fast distance (the larger of the
/// distances along x and z plus half the smaller), from the start S through the first stone A and out of the second B
/// to the goal G: 1.2 x (|S - A| + |B - G|) = 1.2 x ((20 + 12 / 2) + (20 + 12 / 2)) = 62.4 < |S - G| = 120 + 74 / 2 =
/// 157, and at A itself 1.2 x 26 = 31.2 < |A - G| = 100 + 62 / 2 = 131, so it holds all the way to the stone
constexpr glm::vec2 k_StraightStart {-60.0f, 8.0f};
constexpr glm::vec2 k_StraightGoal {60.0f, 82.0f};
/// The second stone is down first, the villager sets off, and the first stone is cast while it walks
constexpr float k_StraightSecondStoneAt = 1.0f;
constexpr float k_StraightWalkAfter = 4.0f;
constexpr float k_StraightFirstStoneAt = 6.0f;
} // namespace

void testbed_scenarios::AddTeleportScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.teleport_two_points",
	    .name = "Teleport between two points",
	    .facet = Facet::Miracles,
	    .description = "Two teleport miracles cast by hand some 95 apart, then three villagers beside the first stone "
	                   "set off to walk home to beside the second, and back every 30 seconds. A fourth villager is put down "
	                   "on the first stone after 15 seconds, as the hand drops one.",
	    .expected = "Each cast leaves a swirling pool of land on the ground, a disc in its owner's colour in the middle "
	                "and fading at the rim, with a low hum. The walkers turn aside into the near stone, walking or running "
	                "as they were going, vanish from it once within a step of it in a burst of pale yellow sparks with a "
	                "whoosh and come out of the other in another burst, then walk the last few steps; the way back goes "
	                "through the stones too. The dropped villager decides what to do as it is put down and jumps at once "
	                "to the other stone. The debug window's prayer power of each teleport goes up a little with every "
	                "useful jump, and is topped up from the player's store after every jump.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_Start, k_Goal}, .distance = 0.8f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Start,
	                 .walkTo = k_Goal,
	                 .walkAfterSeconds = k_WalkAfter,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_Start + glm::vec2(2.0f, -2.0f),
	                 .walkTo = k_Goal + glm::vec2(2.0f, -2.0f),
	                 .walkAfterSeconds = k_WalkAfter + 1.0f,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticLeaderMale,
	                 .offset = k_Start + glm::vec2(-2.0f, 2.0f),
	                 .walkTo = k_Goal + glm::vec2(-2.0f, 2.0f),
	                 .walkAfterSeconds = k_WalkAfter + 2.0f,
	                 .walkRepeatSeconds = k_WalkRepeat,
	                 .walkFinal = VillagerStates::ArrivesHome},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StoneA + glm::vec2(-8.0f, -4.0f),
	                 .dropOnStoneSeconds = 15.0f}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	});

	all.push_back({
	    .id = "miracles.teleport_creature",
	    .name = "A creature takes the teleport",
	    .facet = Facet::Miracles,
	    .description = "The same two stones cast by hand, then a creature beside the first told to walk to beside the "
	                   "second, its mind paused so nothing else takes it off its way.",
	    .expected = "The creature turns aside into the near stone and, within 5 m of it, bursts into yellow sparks there "
	                "and at the far stone while it fizzes out of sight over two seconds; it comes out of the far stone with "
	                "the sound of arriving (and no sound of going), fizzes back into sight over two seconds, then walks on "
	                "to within 5 m of where it was told and stops there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_Start, k_Goal}, .distance = 0.8f},
	    .creatures = {{.label = "traveller",
	                   .species = CreatureType::Tiger,
	                   .offset = k_Start + glm::vec2(-6.0f, 2.0f),
	                   .facingDegrees = 180.0f,
	                   .pauseMind = true}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_StoneA,
	                  .handOffset = k_StoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_StoneB,
	                  .handOffset = k_StoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Command::Kind::WalkTo, .creature = 0, .delaySeconds = k_WalkAfter, .point = k_Goal}},
	});

	all.push_back({
	    .id = "miracles.teleport_worship",
	    .name = "A worshipper goes through the stones",
	    .facet = Facet::Miracles,
	    .description = "Two teleport miracles cast 600 apart, then a villager beside the first sets off to worship at a "
	                   "point beside the second, further than villagers walk to worship.",
	    .expected = "The villager turns aside into the near stone at once, vanishes in a burst of pale yellow sparks and "
	                "comes out of the far one, near the worship point, and takes up its way to worship again (which is "
	                "as far as openblack's villagers go: worshipping itself is not there yet).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_WorshipStoneB + glm::vec2(-20.0f, -20.0f), k_WorshipStoneB + glm::vec2(20.0f, 20.0f)}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_WorshipStoneA + glm::vec2(4.0f, 3.0f),
	                 .walkAfterSeconds = 6.0f,
	                 .worshipAt = k_WorshipStoneB + glm::vec2(6.0f, 4.0f)}},
	    .miracles = {{.type = MagicType::Teleport,
	                  .point = k_WorshipStoneA,
	                  .handOffset = k_WorshipStoneA,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .byHand = true},
	                 {.type = MagicType::Teleport,
	                  .point = k_WorshipStoneB,
	                  .handOffset = k_WorshipStoneB,
	                  .handHeight = 12.0f,
	                  .delaySeconds = 3.0f,
	                  .byHand = true}},
	});

	all.push_back({
	    .id = "miracles.teleport_straight_two_points",
	    .name = "A villager routed through two stones cast straight",
	    .facet = Facet::Miracles,
	    .description = "Two teleport miracles cast straight at their points by the first player, without the hand: the "
	                   "far stone first, then a villager of the scenario's town beyond the near stone sets off to walk to "
	                   "past the far one, and the near stone, 16 from where it set off, is cast two seconds into its walk. "
	                   "Run with OPENBLACK_TELEPORT_TRACE=1 to log the stones, the reaction and the jump.",
	    .expected = "Each cast leaves a swirling pool of land in the first player's colour. The villager turns aside into "
	                "the near stone, walking or running as it was going, vanishes from it in a burst of pale yellow sparks "
	                "and comes out of the far stone in another; it never walks the ground between the stones. What shows "
	                "it used them is the teleport trace, in this order: the far stone made for player 0 with 1 stone, the "
	                "near one with 2; the near stone's REACT_TO_TELEPORT offered to the villager and the villager reacting "
	                "to the near stone, state 201 (or 251 when it runs); the near stone's spell paying a negative cost, "
	                "its prayer power going up; then the villager going from the near stone to the far stone for its goal, "
	                "saving about 94 m. Without the trace, the runner's log has both casts and the villager's walk, and "
	                "the villager is seen at the far stone a moment after it reached the near one.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_StoneA, k_StoneB, k_StraightStart, k_StraightGoal}, .distance = 0.8f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_StraightStart,
	                 .walkTo = k_StraightGoal,
	                 .walkAfterSeconds = k_StraightWalkAfter,
	                 .walkFinal = VillagerStates::ArrivesHome,
	                 .joinTown = true}},
	    .miracles = {{.type = MagicType::Teleport, .point = k_StoneB, .delaySeconds = k_StraightSecondStoneAt},
	                 {.type = MagicType::Teleport, .point = k_StoneA, .delaySeconds = k_StraightFirstStoneAt}},
	});
}
