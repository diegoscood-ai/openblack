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

#include <functional>

#include <entt/entity/entity.hpp>

#include "ECS/PotResource.h"
#include "Enums.h"

// Object::GetResource / RemoveResource / AddResource / IsPoisoned of the original (vt +0x98, +0xA0, +0x9C, +0x4A4) for
// the objects that hold resources: the abodes (Abode 0x404D30, 0x404D90, 0x404F10), the storage pit (StoragePitStore),
// the pots and piles (Pot / PotStructure 0x66D290..0x66EF00) and the villagers. Spec: dev\documentacion\edificios\
// V5_resources_spec.md. `dropper` is the original's GInterfaceStatus*: none for villagers and scripts, the hand's for
// the hand (its branch: the desire before and after, the alignment and the town's belief).

namespace openblack::ecs::object_resources
{
/// Object::GetResource (vt +0x98): an abode or a storage pit its mirror +0xBC[type] (Abode 0x404D30; a pit's is the
/// total of its piles, StoragePitStore); a pile of a storage pit the pit's total (PotStructure::GetResource 0x66EF00:
/// IsPartOfStructure vt +0x860, the building-site test before it); any other pot its own amount when the resource is
/// its own (Pot::JustGetResource 0x66D390); a building site's pile its own amount (0x66EF12..0x66EF47, V6)
[[nodiscard]] uint32_t GetResource(entt::entity object, ResourceType type);
/// Pot::JustGetResource 0x66D390 (vt +0x94 of every pot and pile, a PotStructure's too): +0x70 when the type is the
/// pot's own (GetResourceType vt +0x690), else 0; 0 for what is not a pot. The out poison flag is not returned
[[nodiscard]] uint32_t JustGetResource(entt::entity pot, ResourceType type);
/// Object::RemoveResource (vt +0xA0): StoragePit 0x7332A0 (StoragePitStore); Abode 0x404F10 -> DoResourceRemoving
/// 0x404F60 (JustRemoveResource 0x404D60: min(amount, what it has)); a pile of a storage pit
/// PotStructure::RemoveResource 0x66EE10 (the touched pile gives n - min(over, n), over = the pit's amount above its
/// maximum, then the pit's own order, pile 5 -> 1, the rest); a building site's pile the site's RemoveResource
/// (0x66EE1E); any other pot JustRemoveFromPot (Pot 0x66D3F0, PotStructure 0x66D9B0). An abode with a building site
/// (+0x74) and WOOD or -2: the site's RemoveResource (0x404F1A). (pending) the out poison flag (0x66EE60: *out =
/// IsPoisoned) is not returned: the callers ask IsPoisoned. Returns what was removed
uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper = {});
/// Object::AddResource (vt +0x9C): the storage pit first (it has an Abode too): StoragePit::AddResource 0x732F60
/// (StoragePitStore; with a building site, WOOD or -2 go to the site, 0x732F67); Abode 0x404D90 (the same +0x74
/// redirect) -> DoResourceAdding 0x404DF0 -> JustAddResource 0x404D40 (+0xBC[type] += amount, no cap); a building
/// site's pile the site's AddResource (0x66EDB9); a pot or a pile PotStructure::AddResource 0x66ED70 / Pot 0x66D290
/// (pot_resource::PotStructureAddResource). Returns what was taken
uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper = {},
                     bool poisoned = false);
/// The object's IsPoisoned (vt +0x4A4) as GetResourceFrom 0x7533EC reads it: StoragePit 0x7336B0 =
/// IsPoisonedResource(FOOD) || (WOOD) 0x733550 (an available pile of +0xC4 / +0xC8 whose Pot::IsPoisoned is set);
/// an abode 0 (GameThingWithPos 0x402400); a pot its own flag
[[nodiscard]] bool IsPoisoned(entt::entity object);

/// Pot::JustRemoveResource 0x66D410 (the type is not tested) and PotStructure::JustRemoveResource 0x66D9B0: amount >=
/// what it has -> all of it, and the emptied pot loses its reaction (RemoveReaction 0x66D6A0), its poison (+0x74 bit 0)
/// and its fire (+0x44, ToBeDeleted); SetSize. A pile without a structure that empties is deleted (0x66D9E0,
/// ToBeDeleted vt +0xC); a storage pit's or a building site's empty pile stays (0x66D9D3). Returns what was removed
uint32_t JustRemoveFromPot(entt::entity pot, uint32_t amount);

/// Abode::DoResourceAdding 0x404DF0 (vt +0x8E4) of an abode or a storage pit, around `justAdd` (the change itself:
/// JustAddResource 0x404D40 for an abode, the piles for a pit). Without an interface or a town (0x404E01..0x404E09)
/// only justAdd. With one: CallDesireFunction(type != FOOD) before and after (0x404E22 / 0x404E48), the difference x
/// Town::GetGameTurnResourceLastRemovedModifier 0x740030 (0x404E70), GAlignment::Update(this, type, n, delta) 0x414520
/// of the interface's player (0x404E87), the town's GBelief::AddToBelief 0x437EB0 with delta x (not the owner ?
/// multiplierForNonOwnerAddingResource : 1) x multiplierForAddingResourceToTown (0x404E8C..0x404EC3), and
/// DoCreatureMimicAfterAddingResource (vt +0x68C, TODO(creature)). Returns justAdd's value
uint32_t DoResourceAdding(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                          const std::function<uint32_t()>& justAdd);
/// Abode::DoResourceRemoving 0x404F60 (vt +0x8E8) around `justRemove`: CallDesireFunction(type != FOOD) of the town
/// before (0x404FA0); with an interface and a town Town::SetGameTurnResourceLastRemoved 0x7400D0 of its player, the
/// desire after, and GAlignment::Update(this, type, -n, before - after) of the TOWN OWNER (0x404FEF..0x405005; -n the
/// amount asked for). Returns justRemove's value
uint32_t DoResourceRemoving(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                            const std::function<uint32_t()>& justRemove);
} // namespace openblack::ecs::object_resources
