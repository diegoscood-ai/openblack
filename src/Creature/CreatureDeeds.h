/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>

#include "Enums.h"

namespace openblack::creature_watching
{
/// The deeds of a player that the player's creature watches and may copy. Each is the row of the same name in the
/// game's table of mimicry rules (InfoConstants::creatureMimic), in the same order.
enum class Deed : uint8_t
{
	PutFoodInWorshipSite = 0,
	CastMagicFoodInWorshipSite = 1,
	PutFoodInStoragePit = 2,
	CastMagicFoodInStoragePit = 3,
	PutWoodInStoragePit = 4,
	CastMagicWoodInStoragePit = 5,
	BuildHouse = 6,
	PutWoodInBuildingSite = 7,
	CastMagicWoodByBuildingSite = 8,
	PutWoodInWorkshop = 9,
	CastMagicWoodByWorkshop = 10,
	PlantTree = 11,
	GiveTownProtectionWithShield = 12,
	BringPeopleToWorship = 13,
	MakeArtefact = 14,
	DamageByThrowing = 15,
	DamageByThrowingAt = 16,
	DamageWithFire = 17,
	DamageWithMagic = 18,
	ImpressByThrowing = 19,
	ImpressWithMagic = 20,
	ThrowInTheSea = 21,
	MakeDiscipleFarmer = 22,
	MakeDiscipleForester = 23,
	MakeDiscipleFisherman = 24,
	MakeDiscipleBuilder = 25,
	MakeDiscipleBreeder = 26,
	MakeDiscipleProtection = 27,
	MakeDiscipleMissionary = 28,
	MakeDiscipleCraftsman = 29,
	MakeDiscipleChangeHouse = 30,
	MakeDiscipleWorship = 31,
	TakeObjectHome = 32,
	CastWaterOnCrops = 33,
	CastWaterToPutOutFire = 34,
	StealObjectAndPutInTown = 35,
	StealObjectAndPutByCitadel = 36,
	BreakRocks = 37,
	ThrowFootballInGoal = 38,
	CatchFootball = 39,
	Sacrifice = 40,
	PlayWithToy = 41,
	Heal = 42,
	StealFoodFromFarm = 43,
	StealFoodFromStoragePit = 44,
	StealWoodFromStoragePit = 45,
};

/// How many deeds there are, one per row of the mimicry table
constexpr size_t k_DeedCount = 46;
static_assert(static_cast<size_t>(Deed::StealWoodFromStoragePit) + 1 == k_DeedCount);

/// The kind of thing that a player gave a resource to, other than a storage pit (whose deed the pit works out)
enum class Receiver : uint8_t
{
	WorshipSite,
	Workshop,
	/// Anything else, such as a house
	Other,
};

/// What the creature sees when its player gives a resource to a thing that is not a storage pit
struct Gift
{
	Receiver receiver {Receiver::Other};
	/// The thing is a building that goes up, which takes wood onto its site
	bool goingUp {false};
	ResourceType resource {ResourceType::None};
};

/// The deed of a gift, none when the gift shows no deed. Wood to a building that goes up is always the building site
/// deed, whatever the building; then wood to a workshop and food to a worship site are their deeds.
[[nodiscard]] std::optional<Deed> GiftDeed(const Gift& gift);

/// A thing that a player's hand dropped comes to rest within this many metres of the player's temple
inline constexpr float k_TempleReach = 80.0f;

/// What the creature sees when a thing that its player's hand dropped comes to rest
struct Landing
{
	PlayerNames player {PlayerNames::NEUTRAL};
	/// The thing came to rest nearer than k_TempleReach to the player's temple
	bool nearOwnTemple {false};
	/// The player of the town that the thing belongs to, none when it belongs to no town
	std::optional<PlayerNames> thingsTown;
	/// The player of the town nearest to the thing, none when there is no town
	std::optional<PlayerNames> nearestTown;
};
/// The deed of a landing, none when it shows no deed. Near the player's temple, a thing of another player's town is a
/// theft put by the temple, and anything else is a thing taken home. Farther away, a thing of another player's town that
/// comes to rest with the player's own town nearest is a theft put in the town.
[[nodiscard]] std::optional<Deed> LandingDeed(const Landing& landing);

/// The deed of a disciple that a player puts down: the deeds of the disciples follow the sea deed in the order of the
/// disciple types, so a villager with no disciple type is the sea deed itself. The game adds the type as it is, so from
/// the trader on each type shows the deed of the next type (vanilla bug). None for a type past the last deed.
[[nodiscard]] std::optional<Deed> DiscipleDeed(VillagerDisciple type);

} // namespace openblack::creature_watching
