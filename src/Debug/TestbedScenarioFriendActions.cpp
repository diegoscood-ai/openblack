/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of two creatures that do things together: side by side with a friend (being happy, having a
// poo, sitting, sleeping, drinking) and with a friend at a town (helping it, attacking it)

#include <cstddef>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;
using creature_desires::Desire;

/// The game's table rows of the actions, as a script's CREATURE_DO_ACTION gives them
constexpr size_t k_SmileAtFriend = 60;
constexpr size_t k_DrinkWithFriend = 199;
constexpr size_t k_PooWithFriend = 200;
constexpr size_t k_SitWithFriend = 201;
constexpr size_t k_HappyWithFriend = 202;
constexpr size_t k_SleepWithFriend = 203;
constexpr size_t k_AttackWithFriend = 273;
constexpr size_t k_HelpTownWithFriend = 274;

/// A creature kept content, held there, so that its needs stay out of what the scenario shows
CreatureSetup Content(CreatureType species, glm::vec2 offset, float facing, std::string_view label)
{
	return {.label = label,
	        .species = species,
	        .offset = offset,
	        .facingDegrees = facing,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f, .warmth = 0.0f},
	        .hold = true};
}

/// Every desire gone but one, which is as strong as it gets
std::vector<DesireOverride> OnlyDesire(Desire wanted)
{
	std::vector<DesireOverride> overrides;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		const auto desire = static_cast<Desire>(i);
		overrides.push_back({.desire = desire, .fraction = desire == wanted ? 1.0f : 0.0f});
	}
	return overrides;
}

/// A lion of no player, as the guide is on the first land
CreatureSetup GuideLion(glm::vec2 offset, float facing)
{
	auto lion = Content(CreatureType::Lion, offset, facing, "guide");
	lion.owner = PlayerNames::NEUTRAL;
	return lion;
}
} // namespace

void testbed_scenarios::AddFriendActionScenarios(std::vector<Scenario>& all)
{
	auto befriender = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "befriends");
	befriender.desires = OnlyDesire(Desire::BeFriends);
	auto befriended = GuideLion({25.0f, 0.0f}, 270.0f);
	befriended.desires = OnlyDesire(Desire::BeFriends);
	all.push_back({
	    .id = "friends.side_by_side",
	    .name = "Do things side by side with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. Both want only to be friends, "
	                   "held there.",
	    .expected = "Some of the time the tiger asks the lion to be its friend and does a joint action with it. To be happy "
	                "together, both go to places side by side, the tiger waits until the lion stops, both turn to face "
	                "each other and both play the happy action. To sit together, both go to the two sides of a clear "
	                "place, turn to face each other and sit for 10 to 15 seconds; then the tiger plays the happy action. "
	                "The Mind panel's Attitudes show the partners while they do it, and the step readout the wait for the "
	                "lion to stand still.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, befriended},
	});
	// One joint action with the lion, which a script tells the tiger to do. The lion first smiles at the tiger by a
	// script too, so that a script controls both and the lion agrees.
	struct JointScene
	{
		std::string_view id;
		std::string_view name;
		size_t action {0};
		bool town {false};
		std::optional<Desire> desire;
		std::string_view expected;
	};
	for (const auto& scene : {
	         JointScene {.id = "friends.happy_together",
	                    .name = "Be happy with a friend",
	                    .action = k_HappyWithFriend,
	                    .expected = "Both go to places side by side and the tiger waits until the lion stops. Both turn "
	                                "to face each other and play the happy action."},
	         JointScene {.id = "friends.sit_together",
	                    .name = "Sit with a friend",
	                    .action = k_SitWithFriend,
	                    .expected = "Both go to the two sides of a clear place, turn to face each other and sit for 10 "
	                                "to 15 seconds. Then the tiger plays the happy action."},
	         JointScene {.id = "friends.sleep_together",
	                    .name = "Sleep with a friend",
	                    .action = k_SleepWithFriend,
	                    .desire = Desire::Tiredness,
	                    .expected = "Both go to the two sides of a clear place, turn to face each other and sleep for 17 "
	                                "seconds. Then the tiger wakes with a confused look."},
	         JointScene {.id = "friends.drink_together",
	                    .name = "Drink with a friend",
	                    .action = k_DrinkWithFriend,
	                    .desire = Desire::Water,
	                    .expected = "Both go to the nearest coast, side by side, turn to face the sea and drink. Then "
	                                "the tiger turns to the lion with a smile."},
	         JointScene {.id = "friends.poo_together",
	                    .name = "Have a poo with a friend",
	                    .action = k_PooWithFriend,
	                    .desire = Desire::Poo,
	                    .expected = "Both go to places side by side, squat for four seconds and have a poo. Then the "
	                                "tiger turns to the lion with a smile and plays the happy action."},
	         JointScene {.id = "friends.help_town_together",
	                    .name = "Help a town with a friend",
	                    .action = k_HelpTownWithFriend,
	                    .town = true,
	                    .expected = "The tiger turns to the lion, plays the happy action and points at the town. Both go "
	                                "near the storage pit and turn to face each other. Then the first of the two to take "
	                                "up the wish to help the town does so, and the other one stops. The step readout "
	                                "shows the desire taken up about the town."},
	         JointScene {.id = "friends.attack_town_together",
	                    .name = "Attack a town with a friend",
	                    .action = k_AttackWithFriend,
	                    .town = true,
	                    .desire = Desire::Anger,
	                    .expected = "The script's action fails: a creature that has learnt nothing finds the town more "
	                                "useful to its compassion than to its anger, so it cannot attack the town. A creature "
	                                "taught to be angry with towns attacks it as helping a town with a friend."},
	     })
	{
		auto tiger = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "asks");
		if (scene.desire.has_value())
		{
			tiger.desires = {{.desire = *scene.desire, .fraction = 0.6f}};
		}
		Scenario scenario {
		    .id = scene.id,
		    .name = scene.name,
		    .facet = Facet::Mind,
		    .description = "A tiger and a lion of no player stand 50 apart and face each other. A script tells the lion "
		                   "to smile at the tiger, then tells the tiger to do a joint action with the lion.",
		    .expected = scene.expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview},
		    .creatures = {tiger, GuideLion({25.0f, 0.0f}, 270.0f)},
		    .commands = {{.kind = Kind::ScriptDoAction,
		                  .creature = 1,
		                  .delaySeconds = 0.5f,
		                  .value = k_SmileAtFriend,
		                  .atCreature = size_t {0}},
		                 {.kind = Kind::ScriptDoAction,
		                  .creature = 0,
		                  .delaySeconds = 4.0f,
		                  .value = scene.action,
		                  .atCreature = size_t {1}}},
		};
		if (scene.town)
		{
			scenario.objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -70.0f}, .storedFood = 2000}};
		}
		all.push_back(std::move(scenario));
	}
}
