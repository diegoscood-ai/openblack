/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of a creature that helps a town or plays with its people: food from a field given to a
// storage pit, a cow dropped in a storage pit, a villager kissed in play, and the thefts a copy of the player makes

#include <cstddef>

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

CreatureSetup Helper()
{
	return {.label = "helper", .species = CreatureType::Tiger, .offset = {0.0f, 0.0f}, .facingDegrees = 0.0f};
}

/// The game's table rows of the actions, as a script's CREATURE_DO_ACTION gives them
constexpr size_t k_GiveFoodFromFieldToStoragePit = 28;
constexpr size_t k_DropCowInStoragePit = 285;
constexpr size_t k_PlayfullyKissVillager = 301;
constexpr size_t k_StealObjectAndPutInTown = 264;
constexpr size_t k_StealFoodFromStoragePit = 307;
constexpr size_t k_StealWoodFromStoragePit = 308;

/// A script tells the creature to do the action, done to the scenario's first object, with its second when given
Command Do(size_t action, std::optional<size_t> with = std::nullopt)
{
	return {.kind = Kind::ScriptDoAction, .creature = 0, .delaySeconds = 1.0f, .value = action, .object = 0, .with = with};
}
} // namespace

void testbed_scenarios::AddCreatureTownHelpScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.field_food_to_store",
	    .name = "Creature gives food from a field to a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit in front of it and a ripe wheat field to one side. A script tells it to "
	                   "give food from the field to the pit, as a town's need for food makes it.",
	    .expected = "It walks to the field, faces it and pulls a handful of wheat from it. It goes to where it can throw "
	                "into the pit, faces the pit with a look of compassion and throws the wheat in. The handful holds no "
	                "food, as in the game (a field keeps no store of its own), so the pit's food stays as it was.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {65.0f, 45.0f}}},
	    .creatures = {Helper()},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 2000, .storedWood = 0},
	                {.type = FieldTypeInfo::Wheat, .offset = {50.0f, 35.0f}, .ripeCrop = true}},
	    .commands = {Do(k_GiveFoodFromFieldToStoragePit, 1)},
	});

	all.push_back({
	    .id = "objects.cow_to_store",
	    .name = "Creature drops a cow in a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit in front of it and a cow to one side. A script tells it to drop the cow "
	                   "in the pit.",
	    .expected = "It walks to the cow and picks it up, faces the pit, goes to where it can throw into the pit with a "
	                "look of compassion and throws the cow in. Whether the pit takes the cow as food is what this "
	                "scenario checks: the log's pit lines say.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -75.0f}, {40.0f, 10.0f}}},
	    .creatures = {Helper()},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 2000, .storedWood = 0},
	                {.type = AnimalInfo::Cow, .offset = {25.0f, -10.0f}}},
	    .commands = {Do(k_DropCowInStoragePit, 1)},
	});

	all.push_back({
	    .id = "objects.steal_food_from_store",
	    .name = "Creature steals food from a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with an empty storage pit in front of it and a second pit full of food to one side. A "
	                   "script tells it to steal food from the second pit for the first, as a copy of the player's theft "
	                   "makes it.",
	    .expected = "It goes right up to the full pit, looking at it, faces it and takes a handful of wheat. It goes to "
	                "where it can throw into the empty pit with a look of compassion, faces the pit and throws the wheat "
	                "in. The empty pit gets the food, and the full pit keeps all of its food, as in the game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {85.0f, 50.0f}}},
	    .creatures = {Helper()},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {70.0f, 30.0f}, .storedFood = 5000, .storedWood = 0}},
	    .commands = {Do(k_StealFoodFromStoragePit, 1)},
	});

	all.push_back({
	    .id = "objects.steal_wood_from_store",
	    .name = "Creature steals wood from a storage pit",
	    .facet = Facet::Objects,
	    .description = "A tiger with a storage pit with no wood in front of it and a second pit full of wood to one side. "
	                   "A script tells it to steal wood from the second pit for the first.",
	    .expected = "It goes right up to the full pit, faces it and takes a handful of logs. It goes to where it can throw "
	                "into the first pit with a look of compassion, faces the pit and throws the logs in. The first pit "
	                "gets the wood, and the full pit keeps all of its wood, as in the game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {85.0f, 50.0f}}},
	    .creatures = {Helper()},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {70.0f, 30.0f}, .storedFood = 0, .storedWood = 5000}},
	    .commands = {Do(k_StealWoodFromStoragePit, 1)},
	});

	all.push_back({
	    .id = "objects.steal_toy_to_town",
	    .name = "Creature steals a toy and takes it to a town",
	    .facet = Facet::Objects,
	    .description = "A tiger with a cuddly toy in front of it, and the storage pit of the player's town behind it. A "
	                   "script tells it to steal the toy and take it to the town.",
	    .expected = "It picks the toy up, looking at it. It goes near the storage pit of the town, to within three times its "
	                "height, looking at the pit, and puts the toy down there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .creature = 0, .distance = 1.5f},
	    .creatures = {Helper()},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -20.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 90.0f}, .storedFood = 0, .storedWood = 0}},
	    .commands = {Do(k_StealObjectAndPutInTown)},
	});

	all.push_back({
	    .id = "objects.kiss_villager",
	    .name = "Creature kisses a villager in play",
	    .facet = Facet::Objects,
	    .description = "A tiger with a villager in front of it. A script tells it to kiss the villager in play.",
	    .expected = "It walks to two times its height from the villager, faces it and plays the kiss.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -45.0f}, {15.0f, 10.0f}}},
	    .creatures = {Helper()},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -40.0f}}},
	    .commands = {Do(k_PlayfullyKissVillager)},
	});
}
