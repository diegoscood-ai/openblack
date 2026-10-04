/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// BuildingSite (0x644 bytes) / StandardBuildingSite (0x648, vtable 0x8C6DF4) of runblack.exe W120, one per entity
/// (spec dev\documentacion\edificios\V6_spec.md §2.1). Made by BuildingSite(MultiMapFixed*) 0x43B7E0 through the
/// building's CreateBuildingSite (MultiMapFixed 0x52F590); kept by the town's list Town +0x790 (Town::buildingSites,
/// head first) and by the building's +0x74 (Abode::buildingSite). ecs::building_sites is its only writer.
/// Not kept: +0x00..+0x13 GameThing (only the +0xA bit 0, `beingDeleted`), the global list g_game +0x205CAC (head
/// insert 0x43B809..0x43B82D; openblack walks the components), +0x20..+0x30 the scaffolds (not ported).
/// The citadel heart's site also carries components::CitadelBuildingSite
struct BuildingSite
{
	/// the ring of builder positions: 128 entries (PosBuilder::Process `mov edi, 0x80` 0x43AEF0, GetNearestEdge `&
	/// 0x7F`, ArrivesAtBuildingSite `cmp eax, 0x80` 0x758B20; the bw1-decomp header's 0x7F is wrong)
	static constexpr size_t k_RingSize = 128;

	/// +0x14 RootBuilding (GetRootBuilding 0x43BCA0); null after ToBeDeleted 0x43B960 step 8
	entt::entity root {entt::null};
	/// +0x18 / +0x1C LHLinkedList<Villager*> {head, count}: the builders, the head first (AddBuilder 0x43BE40 pushes at
	/// the head; RemoveBuilder 0x43BE90 takes every node of the villager). The count is builders.size()
	std::vector<entt::entity> builders;
	/// +0x34..+0x633: LHPoint (x, y, z floats, world metres), 12 bytes each (PosBuilder::Process 0x43AE10)
	std::array<glm::vec3, k_RingSize> ring {};
	/// +0x634: the number of builders, ++ in AddBuilder and -- once in RemoveBuilder (also when the villager is not in
	/// the list: it can go negative, literal); NOT reset by ToBeDeleted
	int32_t builderCount {0};
	/// +0x638: a repair site (the ctor: building +0x58 bit 2, 0x43B7E0)
	bool isRepairSite {false};
	/// +0x63C: the desire boost (BUILD_BUILDING's desire x 5, ForceBuildingOfPlannedAtPos 0x73E560)
	float desireBoost {0.0f};
	/// +0x640: the repair base (the ctor: GetLife, or 1.1 x life - 0.1 for a built building without a DestructionMesh;
	/// Abode::ReduceLife 0x405EA7 writes 1.1 x life - 0.1); read by GetPercentRepairedFromWhenDamaged 0x52F010
	float repairBase {0.0f};
	/// +0x644 StandardBuildingSite: Pot* WoodPile (GetPileWood 0x43D6E0 / SetPileWood 0x43D6F0)
	entt::entity woodPile {entt::null};
	/// GameThing +0xA bit 0 (IsAvailable 0x401810 = !bit): set by ToBeDeleted 0x43B960. (approximate) the original
	/// frees the object in the game's deletion pass; openblack destroys the entity in building_sites::FlushDeleted at
	/// the start of the next town turn, so IsAvailable stays callable (and false) for the rest of the turn
	bool beingDeleted {false};
};

/// CitadelBuildingSite (0x65C bytes, vtable 0x8C6CC0): the citadel heart's site (CitadelHeart::CreateBuildingSite
/// 0x468DC0, ctor 0x43D1E0), a BuildingSite whose own pile (+0x644 of a StandardBuildingSite, `woodPile`) is not used:
/// six wood piles 22 m out instead (CreatePilesOfWood 0x43D2A0)
struct CitadelBuildingSite
{
	static constexpr size_t k_Piles = 6;
	/// +0x644..+0x658 Pot* [6] (slot i at GetResourcePosAndYAngle(WOOD, i), 0x43D470); emptied only by Process 0x43D660
	/// and RemovePotFromStructure 0x43D5B0 (ToBeDeleted keeps them, SetPileWood 0x43D180 is a no-op)
	std::array<entt::entity, k_Piles> piles {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	/// (openblack) ToBeDeleted 0x43D220 called each pile's SetMultiMapFixed(0) (vt +0x868): their PotStructure part no
	/// longer reaches this site (building_sites::SiteOfPile)
	bool pilesUnlinked {false};
};

} // namespace openblack::ecs::components
