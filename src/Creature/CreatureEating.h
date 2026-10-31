/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <glm/vec2.hpp>

#include "CreaturePhysiology.h"
#include "Enums.h"

/// The rules of a creature's eating: what it may eat, the portion it makes of a pile of food, how a meal ends and
/// what it does to its hunger, and which piles and storage pits hold food for it. All the arithmetic is in float, step
/// by step, as the game does it
namespace openblack::creature_eating
{

/// A portion holds a thousand times the creature's drawn size, at most what its source holds
inline constexpr float k_PortionAmountPerSize = 1000.0f;
/// A portion is drawn at this many times the creature's drawn size
inline constexpr float k_PortionScalePerSize = 3.75f;
/// A portion is made this share of the creature's pick-up range away from it, towards its source
inline constexpr float k_PortionRangeShare = 0.95f;
/// A thing worth less food than this is not food, unless it is a mushroom or pile food
inline constexpr double k_LeastFoodValue = 5.0;
/// A food pile within this many times its town's storage pit's radius of the pit belongs to the pit
inline constexpr float k_PitReachPerRadius = 1.1f;
/// A creature goes to within this many times its own radius and a pile's of the pile to eat from it
inline constexpr float k_PileApproachShare = 1.2f;
/// A creature goes to within this many times its own height of a storage pit's food pile to eat from it
inline constexpr float k_PitApproachHeights = 1.4f;
/// Before it makes a portion from a storage pit, a field or a tree, it turns to face it and holds still this long
inline constexpr float k_FaceSourceSeconds = 0.1f;
/// A storage pit a creature eats from is no wider than this many times the creature's drawn size
inline constexpr float k_PitRadiusPerSize = 15.0f;
/// A tree a creature eats from is at least this many times the creature's drawn size tall
inline constexpr float k_TreeHeightPerSize = 15.0f;

/// What a thing is to the game's test of whether a creature can eat it
enum class Eaten : uint8_t
{
	/// An object (a villager, an animal, a mobile object, a pot): eaten when the creature can pick it up
	AnObject,
	/// A mobile static: eaten unless it is a toy
	AMobileStatic,
	/// Anything else (a creature, a thing that is only a thing with a place): never eaten
	Never,
};

/// What the test of whether a creature can eat a thing reads
struct EatCheck
{
	Eaten as {Eaten::Never};
	/// The creature can pick the thing up (creature_pick_up::CreatureMayPickUp)
	bool mayPickUp {false};
	/// The thing is a toy to the creature
	bool isToy {false};
};

/// How a meal ends, once the creature has swallowed it
enum class MealEnd
{
	/// It is eaten: the hunger goes down
	Eaten,
	/// It is not food: the creature spits it out and learns that its kind is not food
	NotFood,
	/// It is poisoned: the creature spits it out, and learns nothing
	Poisoned,
	/// It may not be eaten: the creature spits it out whole, learns nothing, and it is let go of (RefusesMeal)
	Refused,
};

/// Whether a creature can eat a thing, as the game's eating plans ask it: an object when the creature can pick it up, a
/// mobile static when it is not a toy, nothing else
[[nodiscard]] bool CanBeEaten(const EatCheck& check);

/// The food in a portion: the creature's drawn size times a thousand, cut towards zero as the game's float to integer
/// conversion does, and no more than the source holds
[[nodiscard]] uint32_t PortionAmount(float drawnSize, uint32_t sourceAmount);
/// What a portion holds of its source: the resource its own row of the pot table gives (food for wheat and fruit, wood
/// for branches and logs), food when there is no row
[[nodiscard]] ResourceType PortionResource(std::optional<ResourceType> rowResource);
/// The portion's scale
[[nodiscard]] float PortionScale(float drawnSize);
/// Where the portion is made, across the land: at a share of the pick-up range from the creature towards the source,
/// whatever the source's distance. Where the two are at one place (or a difference is not a number) the difference is
/// left as it is
[[nodiscard]] glm::vec2 PortionPoint(glm::vec2 creature, glm::vec2 source, float pickUpRange);
/// How near a creature goes to a pile before it makes a portion of it: the two radii added, then the share of that
[[nodiscard]] float PileApproach(float pileRadius, float creatureRadius);

/// Whether a meal may not be eaten at all, before anything else is asked of it: a thing marked indestructible, or one
/// in a script while a script owns the widescreen bars
[[nodiscard]] constexpr bool RefusesMeal(bool indestructible, bool inScript, bool scriptWideScreen)
{
	return indestructible || (inScript && scriptWideScreen);
}

/// The death of a person a creature has eaten, before it is gone: no reason given, put down to the creature's player
/// (none for a neutral creature), the amount the life it had, and no resource dropped
struct EatenPersonDeath
{
	DeathReason reason;
	std::optional<PlayerNames> killer;
	float amount;
	int drop;
};
[[nodiscard]] constexpr EatenPersonDeath EatenPersonDeathOf(PlayerNames creatureOwner, float life)
{
	return {.reason = DeathReason::None,
	        .killer = creatureOwner != PlayerNames::NEUTRAL ? std::optional(creatureOwner) : std::nullopt,
	        .amount = life,
	        .drop = 0};
}

/// How a meal of a food value ends. A mushroom or pile food is always eaten; otherwise a poisoned thing, or one worth
/// less than the least food value (or not a number), is spat out
[[nodiscard]] MealEnd JudgeMeal(float foodValue, bool mushroom, bool pileFood, bool poisoned);
/// The hunger left after a meal is eaten: the food value over the creature's real size (at most
/// creature_physiology::k_MaxMealSize) times its species' food to energy, taken off and kept within 0..1. Not a number
/// gives 0
[[nodiscard]] float HungerAfterMeal(float hunger, float foodValue, float realSize, float foodToEnergy);

/// Whether a pile of food is one a creature may eat from: a magic food pile whose nearest town has no storage pit
/// (pitRadius absent), or that is at least the pit's reach from it. A distance or radius not a number counts as outside
[[nodiscard]] bool IsFoodPileOutsideStoragePit(bool magicFood, std::optional<float> pitRadius, float distance);
/// Whether a storage pit has food for a creature: not wider than its share of the creature's drawn size (not a number
/// passes), with a food pile that is not marked as gone and holds some food
[[nodiscard]] bool StoragePitHasFood(float pitRadius, float drawnSize, bool hasPile, bool pileFlagged, uint32_t pileAmount);
/// Whether a field has food for a creature: its crop has grown some and holds some food. Nothing, less, or not a number
/// has none
[[nodiscard]] bool FieldHasFood(float growth, float food);
/// Whether a tree is tall enough for a creature to eat from: at least its share of the creature's drawn size. A height
/// or size not a number is not
[[nodiscard]] bool TreeTallEnough(float height, float drawnSize);

/// The living things a creature killed by eating them, for its player's statistics and the temple's creature scroll
struct Tallies
{
	uint32_t peopleKilled {0};
	uint32_t animalsKilled {0};
	uint32_t creaturesKilled {0};
};
/// The object types of the living things whose meal counts as a kill
inline constexpr uint32_t k_CreatureSort = 1;
inline constexpr uint32_t k_VillagerSort = 2;
inline constexpr uint32_t k_AnimalSort = 4;
/// A meal of a thing of an object type: a person, an animal or a creature counts as killed; anything else counts nothing
void CountMeal(Tallies& tallies, uint32_t type);

} // namespace openblack::creature_eating
