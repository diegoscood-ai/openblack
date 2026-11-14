/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a town's belief in the player: the believers it gained float up from the land under the
// cursor; a town won, with or without a town centre; a scaffold put down by the hand, which impresses the town and
// which its villagers look at; and a neutral town won by the player's food miracles alone

#include <array>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddBeliefScenarios(std::vector<Scenario>& all)
{
	using Kind = Command::Kind;
	constexpr glm::vec2 k_Centre {0.0f, 40.0f};
	// The scenario's town is a Celtic town of the player's, believing a little in the player as a land's script makes
	// its player's town believe, well under the cap of 10 so that what it gains shows. The camera looks north at the
	// town centre from 25 m south of it; the pointer stands on the land in front of the centre, then to its left
	all.push_back({
	    .id = "objects.believers_gained",
	    .name = "The believers a town gained float up from the land",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre of the player's town, which has one villager and believes 1 in the player. The "
	                   "pointer goes to the land in front of the centre and the town gains 0.025 of belief in the player, "
	                   "as a gift to its store gives it; six seconds later the pointer goes to the land left of the centre "
	                   "and the town gains 0.25.",
	    .expected = "Within a second of each gain the tooltip says how many believers were gained (25, then 250), and the "
	                "same count, in yellow over a black shadow one pixel left and down, floats up from the land under the "
	                "pointer: its text ends at the point, it rises 10 m a second, fades in its last second and is gone "
	                "after 5 seconds. The second number rises from the second point, not following the pointer.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 7.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.8f}},
	                 {.kind = Kind::TownGainsBelief, .delaySeconds = 1.0f, .amount = 0.025f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.25f, 0.75f}},
	                 {.kind = Kind::TownGainsBelief, .delaySeconds = 0.5f, .amount = 0.25f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.75f, 0.85f}}},
	    .townBelief = 1.0f,
	});
	// The scenario's town is left believing in no one, so its first fold hands it to the neutral player; then it gains
	// belief in the player, and its next fold hands it back
	all.push_back({
	    .id = "objects.town_won",
	    .name = "A town won by the player celebrates with fireworks and the player's symbol",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre with one villager, its town believing in no one, so that it goes to the neutral "
	                   "player at its first turn. Three seconds in, the town gains 2 of belief in the player, as a gift to "
	                   "its store gives it.",
	    .expected = "At the town's next turn after the gain it changes hands to the player: the take-over sound, the town "
	                "fireworks and the fountain of the player's symbol over the town centre, and its totem turns to the "
	                "player. No fireworks when it went to the neutral player at the start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, 15.0f}, .look = {0.0f, 7.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_Centre + glm::vec2(10.0f, -10.0f),
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::TownGainsBelief, .delaySeconds = 3.0f, .amount = 2.0f}},
	});
	// The same take-over of a town with no centre: its take-over reaction is made from the town itself, so its villagers
	// take it up
	all.push_back({
	    .id = "objects.town_won_without_a_centre",
	    .name = "The villagers of a town won without a town centre stop to watch their town",
	    .facet = Facet::Objects,
	    .description = "Two villagers of a Celtic town that has no town centre, 5 m either side of the town's middle, the "
	                   "town believing in no one, so that it goes to the neutral player at its first turn. Three seconds "
	                   "in, the town gains 2 of belief in the player, as a gift to its store gives it.",
	    .expected = "At its first turn the town goes to the neutral player, and both villagers turn to face the town's "
	                "middle and stand watching it. At the town's next turn after the gain it changes hands to the player, "
	                "with the take-over sound, the fireworks and the fountain of the player's symbol over the town's "
	                "middle. With a town centre (objects.town_won) nobody turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-5.0f, 0.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {5.0f, 0.0f}, .joinTown = true}},
	    .commands = {{.kind = Kind::TownGainsBelief, .delaySeconds = 3.0f, .amount = 2.0f}},
	});
	// A scaffold the player's hand puts down gently by three villagers of the town: its reaction reaches them
	all.push_back({
	    .id = "objects.scaffold_put_down_watched",
	    .name = "Villagers stop to look at a scaffold the hand puts down",
	    .facet = Facet::Objects,
	    .description = "Three villagers of a Celtic town of the player's, believing 1 in the player so that it stays the "
	                   "player's, 8 to 10 m from a point of open land. Two "
	                   "seconds in, the player's hand puts a scaffold worth one down gently on that point, as its "
	                   "put-down of a held scaffold ends.",
	    .expected = "The scaffold builds the town's building that fits there: a building site with the building's "
	                "outline. The three villagers stop what they are doing and stand looking at the scaffold until the "
	                "reaction's time is up or they are needed elsewhere.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -22.0f}, .look = {0.0f, 1.0f, 10.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-8.0f, 4.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {8.0f, 4.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 0.0f}, .joinTown = true}},
	    .commands = {{.kind = Kind::HandPlantScaffold, .delaySeconds = 2.0f, .point = {0.0f, 10.0f}, .value = 1}},
	    .townBelief = 1.0f,
	});
	// A town won only by what the player does, with no belief handed to it: a town believing in no one goes to the
	// neutral player at its first turn, and the player's food miracles poured into its empty store, in sight of its
	// villagers, are all that can win it back. Its belief in each player shows each turn in the town belief part of
	// the state hash's entity trace
	std::vector<ObjectSetup> wonByFood {
	    {.type = AbodeInfo::CelticTownCentre, .offset = k_Centre, .storedFood = 0},
	    {.type = AbodeInfo::CelticStoragePit, .offset = k_Centre + glm::vec2(14.0f, 0.0f), .storedFood = 0},
	};
	constexpr std::array k_Villagers {VillagerInfo::CelticFarmerMale, VillagerInfo::CelticForesterMale,
	                                  VillagerInfo::CelticHousewifeFemale};
	for (int i = 0; i < 9; ++i)
	{
		wonByFood.push_back(
		    {.type = k_Villagers.at(static_cast<size_t>(i % 3)),
		     .offset = k_Centre + glm::vec2(-12.0f + 3.0f * static_cast<float>(i), -12.0f + 2.0f * static_cast<float>(i % 3)),
		     .joinTown = true});
	}
	all.push_back({
	    .id = "objects.town_won_by_food",
	    .name = "A neutral town won by the player's food miracles alone",
	    .facet = Facet::Objects,
	    .description = "A Celtic town centre and an empty storage pit 14 m east of it, with nine villagers of the town "
	                   "in front of them, the town believing in no one, so that it goes to the neutral player at its first "
	                   "turn. From two seconds in, every ten seconds, the player's hand pours a food miracle over the pit "
	                   "for four seconds, as a player casts it. No belief is given to the town by the testbed.",
	    .expected = "The food lands in the pit's food pile. The villagers in reach stop and face each pour, a belief "
	                "symbol rising from each in the player's colour. At each of the town's turns what it was impressed "
	                "by is added to its belief in the player, which never falls while the town is not the player's; once "
	                "that belief passes the neutral player's (0.5), the town changes hands to the player at that turn, "
	                "with the take-over sound, the fireworks and the fountain of the player's symbol over the town "
	                "centre.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {20.0f, 50.0f}}, .distance = 0.8f},
	    .objects = wonByFood,
	    .miracles = {{.type = MagicType::Food,
	                  .point = k_Centre + glm::vec2(14.0f, 0.0f),
	                  .handOffset = k_Centre + glm::vec2(14.0f, 0.0f),
	                  .handHeight = 12.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 4.0f,
	                  .repeatSeconds = 10.0f,
	                  .byHand = true}},
	});
}
