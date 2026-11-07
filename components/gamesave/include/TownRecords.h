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

#include <array>
#include <tuple>

#include "SaveFields.h"

// The records of a player's citadel and town that a save meets right after the players: the citadel and its heart,
// the footpaths, the storage pit with its piles, and the town. Each lists its values in the game's order; the values
// whose meaning is not read yet keep their bytes. See docs/wiki/engine/saved-games.md#the-citadel-the-footpaths-and-the-town.
namespace openblack::gamesave
{

/// The citadel: the temple's whole building
struct CitadelRecord
{
	static constexpr uint32_t k_Type = 0x35;
	ThingPart thing;
	ContainerFields container;
	Ref heart;
	/// Its six parts, as an array of references
	RefList parts;
	/// Four lists of objects; the first two keep their counts as raw words, which the stream counts the same way
	RefList firstList;
	RefList secondList;
	RefList thirdList;
	RefList fourthList;
	uint32_t firstWord {0};
	uint32_t secondWord {0};
	int32_t thirdWord {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.container, record.heart, record.parts, record.firstList, record.secondList, record.thirdList,
		                record.fourthList, record.firstWord, record.secondWord, record.thirdWord);
	}
	bool operator==(const CitadelRecord&) const = default;
};

/// The citadel's heart, the temple, with the parts every piece of a citadel writes
struct CitadelHeartRecord
{
	static constexpr uint32_t k_Type = 0x3A;
	ThingPart thing;
	MultiMapFixedFields base;
	/// What every piece of a citadel adds: its influence, its citadel and one more object
	float influence {0.0f};
	Ref citadel;
	Ref other;
	uint32_t lastImpactTurn {0};
	Ref town;
	Ref alignmentFlock;
	uint32_t specularColour {0};
	int32_t drawColour {0};
	uint8_t destructionStarted {0};
	float destructionTime {0.0f};
	Ref destructionPlasma;
	Ref destructionMagic;
	float nextPlasmaTime {0.0f};
	Ref transferContainer;
	Ref transferObject;
	uint32_t lastTransferTurn {0};
	Ref entrance;

	static auto FieldsOf(auto& record)
	{
		return std::tie(record.base, record.influence, record.citadel, record.other, record.lastImpactTurn, record.town,
		                record.alignmentFlock, record.specularColour, record.drawColour, record.destructionStarted,
		                record.destructionTime, record.destructionPlasma, record.destructionMagic, record.nextPlasmaTime,
		                record.transferContainer, record.transferObject, record.lastTransferTurn, record.entrance);
	}
	bool operator==(const CitadelHeartRecord&) const = default;
};

/// The footpaths that start at a building
struct FootpathLinkRecord
{
	static constexpr uint32_t k_Type = 0x2;
	ThingPart thing;
	RefList footpaths;

	static auto FieldsOf(auto& record) { return std::tie(record.footpaths); }
	bool operator==(const FootpathLinkRecord&) const = default;
};

/// A footpath: its nodes and whether it is in use
struct FootpathRecord
{
	static constexpr uint32_t k_Type = 0x1;
	ThingPart thing;
	RefList nodes;
	uint32_t active {0};

	static auto FieldsOf(auto& record) { return std::tie(record.nodes, record.active); }
	bool operator==(const FootpathRecord&) const = default;
};

/// A node of a footpath: where it is, its flags and the living things that follow it
struct FootpathNodeRecord
{
	static constexpr uint32_t k_Type = 0x3;
	ThingPart thing;
	MapCoords coords {};
	uint8_t flags {0};
	RefList followers;

	static auto FieldsOf(auto& record) { return std::tie(record.coords, record.flags, record.followers); }
	bool operator==(const FootpathNodeRecord&) const = default;
};

/// A footpath link kept with the place of its building
struct FootpathLinkSaveRecord
{
	static constexpr uint32_t k_Type = 0x4;
	ThingPart thing;
	MapCoords coords {};
	Ref link;

	static auto FieldsOf(auto& record) { return std::tie(record.coords, record.link); }
	bool operator==(const FootpathLinkSaveRecord&) const = default;
};

/// What an abode adds to the parts of a building of several cells
struct AbodeFields
{
	uint8_t abodeFlags {0};
	MapCoords drinkingWater {};
	uint32_t unusedWord {0};
	Ref town;
	RefList villagers;
	uint8_t adults {0};
	uint8_t presentAtHome {0};
	uint8_t children {0};
	uint8_t index {0};
	/// Food and wood
	Counted<uint32_t> resources;
	DestructionMeshFlag destructionMesh;

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.abodeFlags, part.drinkingWater, part.unusedWord, part.town, part.villagers, part.adults,
		                part.presentAtHome, part.children, part.index, part.resources, part.destructionMesh);
	}
	bool operator==(const AbodeFields&) const = default;
};

/// A town's storage pit: an abode with its food pile and its other piles
struct StoragePitRecord
{
	static constexpr uint32_t k_Type = 0x8;
	ThingPart thing;
	MultiMapFixedFields base;
	AbodeFields abode;
	/// Its food pile, then its five other piles, each as an array of references
	RefList foodPile;
	RefList otherPiles;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.abode, record.foodPile, record.otherPiles); }
	bool operator==(const StoragePitRecord&) const = default;
};

/// What a pot adds to a moving object, what a pot of a building adds after it, and the pile's height
struct PotFields
{
	uint32_t resourceType {0};
	Ref parentTown;
	uint32_t resourceAmount {0};
	/// Poisoned, from a building site, sped up
	uint8_t flags {0};
	Ref building;
	Ref player;
	uint8_t buildingByte {0};
	/// The pile's height as it changes, one value of the game's interpolator
	std::array<uint8_t, 48> altitudeZoomer {};

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.resourceType, part.parentTown, part.resourceAmount, part.flags, part.building, part.player,
		                part.buildingByte, part.altitudeZoomer);
	}
	bool operator==(const PotFields&) const = default;
};

/// A pile of wood
struct WoodPileRecord
{
	static constexpr uint32_t k_Type = 0x5A;
	ThingPart thing;
	MobileObjectFields base;
	PotFields pile;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.pile); }
	bool operator==(const WoodPileRecord&) const = default;
};

/// A pile of food, with one more reference after the pile's values
struct FoodPileRecord
{
	static constexpr uint32_t k_Type = 0x59;
	ThingPart thing;
	MobileObjectFields base;
	PotFields pile;
	Ref store;

	static auto FieldsOf(auto& record) { return std::tie(record.base, record.pile, record.store); }
	bool operator==(const FoodPileRecord&) const = default;
};

/// One of a town's desires, sorted
struct DesireSort
{
	uint32_t first {0};
	float value {0.0f};
	uint32_t desire {0};

	bool operator==(const DesireSort&) const = default;
};
static_assert(sizeof(DesireSort) == 12);

/// A town's desires, each array with one value for each of the town's 17 desires
struct TownDesireFields
{
	Counted<float> base;
	Counted<float> modifier;
	Counted<float> cheat;
	Counted<float> boost;
	Counted<float> desire;
	uint32_t lastProcessTurn {0};
	Ref town;
	float overall {0.0f};
	Counted<float> raw;
	Counted<float> functionMinimum;
	Counted<float> functionMaximum;
	Counted<float> change;
	Counted<DesireSort> sorts;
	Counted<DesireSort> sortsAgain;
	Counted<int32_t> alignmentChanges;
	Counted<float> previousVillagerStateAmount;
	Counted<float> previousVillagerStateCount;
	Counted<float> villagerStateAmount;
	Counted<float> villagerStateCount;

	static auto FieldsOf(auto& part)
	{
		return std::tie(part.base, part.modifier, part.cheat, part.boost, part.desire, part.lastProcessTurn, part.town,
		                part.overall, part.raw, part.functionMinimum, part.functionMaximum, part.change, part.sorts,
		                part.sortsAgain, part.alignmentChanges, part.previousVillagerStateAmount,
		                part.previousVillagerStateCount, part.villagerStateAmount, part.villagerStateCount);
	}
	bool operator==(const TownDesireFields&) const = default;
};

/// A town
struct TownRecord
{
	static constexpr uint32_t k_Type = 0x28;
	ThingPart thing;
	ContainerFields container;
	Ref mainStoragePit;
	TownDesireFields desires;
	/// Where the camera looks at the town from, and at
	std::array<float, 6> cameraView {};
	uint32_t id {0};
	uint32_t tribe {0};
	uint8_t playerNumber {0};
	float worshipPercentage {0.0f};
	int32_t worshipCount {0};
	float influence {0.0f};
	uint32_t villagersOnWayToWorship {0};
	float averageTurnsToWorshipSite {0.0f};
	float promiscuity {0.0f};
	float beliefInNeutralPlayer {0.0f};
	uint32_t raining {0};
	uint32_t buildingRequested {0};
	uint32_t desireDirty {0};
	uint32_t desireDirtyTimer {0};
	uint32_t cannotBuildWorshipSite {0};
	uint32_t uninhabitable {0};
	/// The pots it keeps its food and wood in for a while, as an array of references
	RefList temporaryStores;
	RefList forests;
	/// Its figures, one value of the game's own (280 bytes, with what the game leaves from its memory)
	std::array<uint8_t, 280> stats {};
	MapCoords areaMin {};
	MapCoords areaMax {};
	Ref creche;
	Ref graveyard;
	RefList workshops;
	RefList abodes;
	RefList homeless;
	RefList extraVillagers;
	RefList spellIcons;
	RefList fields;
	RefList fishFarms;
	RefList buildingSites;
	RefList artifacts;
	/// Its belief in each player, one value of the game's own (464 bytes)
	std::array<uint8_t, 464> belief {};
	RefList villagers;
	Ref nearestTown;
	RefList playthings;
	RefList animals;
	Ref worshipSite;
	Ref townCentre;
	RefList planned;
	/// What it knows of each player, one value of the game's own (eight of 128 bytes)
	std::array<uint8_t, 1024> playerInteract {};
	RefList villagersOnWayToWorshipSite;
	/// Which miracles it holds, one value of 42 words
	std::array<int32_t, 42> magicTypesHeld {};
	Ref footballPitch;
	Ref lastAttackingPlayer;
	uint32_t lastAttackedTurn {0};
	float otherPlayersAggressorScale {0.0f};
	float ownerAggressorScale {0.0f};
	float ownerInteractionTotal {0.0f};
	float otherInteractionTotal {0.0f};
	float worshipPercentageBeforeEmergency {0.0f};
	/// The turn each player last took food and wood from it
	Table<uint32_t> resourceLastRemovedTurn;
	RefList flocks;
	/// Its desire flags, one for each desire, as an array of references
	RefList desireFlags;
	RefList missionaries;
	float balanceBeliefScale {0.0f};
	uint32_t emptyTownTimer {0};

	static auto FieldsOf(auto& record)
	{
		return std::tie(
		    record.container, record.mainStoragePit, record.desires, record.cameraView, record.id, record.tribe,
		    record.playerNumber, record.worshipPercentage, record.worshipCount, record.influence,
		    record.villagersOnWayToWorship, record.averageTurnsToWorshipSite, record.promiscuity, record.beliefInNeutralPlayer,
		    record.raining, record.buildingRequested, record.desireDirty, record.desireDirtyTimer,
		    record.cannotBuildWorshipSite, record.uninhabitable, record.temporaryStores, record.forests, record.stats,
		    record.areaMin, record.areaMax, record.creche, record.graveyard, record.workshops, record.abodes, record.homeless,
		    record.extraVillagers, record.spellIcons, record.fields, record.fishFarms, record.buildingSites, record.artifacts,
		    record.belief, record.villagers, record.nearestTown, record.playthings, record.animals, record.worshipSite,
		    record.townCentre, record.planned, record.playerInteract, record.villagersOnWayToWorshipSite, record.magicTypesHeld,
		    record.footballPitch, record.lastAttackingPlayer, record.lastAttackedTurn, record.otherPlayersAggressorScale,
		    record.ownerAggressorScale, record.ownerInteractionTotal, record.otherInteractionTotal,
		    record.worshipPercentageBeforeEmergency, record.resourceLastRemovedTurn, record.flocks, record.desireFlags,
		    record.missionaries, record.balanceBeliefScale, record.emptyTownTimer);
	}
	bool operator==(const TownRecord&) const = default;
};

} // namespace openblack::gamesave
