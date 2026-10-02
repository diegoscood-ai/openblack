/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GObjectInfo;
} // namespace openblack

// What the fire asks of the burning object: the Object virtuals of the fire (vt 0x5C4..0x5F8, 0x6BC/0x6C0, 0x7CC) and
// the info.dat values they read (GObjectInfo +0x90 defenceMultiplierBurn, +0xB0 heatCapacity, +0xB4
// combustionTemperature, +0xB8 burningPriority; runtime = file + 0x10). One function per virtual; the classes that
// override it are handled inside (vt_fire_overrides2.txt). Wiki: docs/bw1-notes/magic.md, "Fuego".

namespace openblack::ecs::fire::traits
{
/// The object's GObjectInfo (Object +0x28): trees, dead trees, abodes, fields, features, villagers, animals, mobile
/// statics and objects, pots, magic fireballs. nullptr: no info, the object never burns.
[[nodiscard]] const GObjectInfo* InfoOf(entt::entity object);

/// Object::GetCombustionTemperature 0x639A30 (info +0xB4); 0 without an info (FireEffect::Create refuses it)
[[nodiscard]] float CombustionTemperature(entt::entity object);
/// Object::GetHeatCapacity 0x637CE0 (info +0xB0, vt 0x5E4); MagicFireBall::GetHeatCapacity 0x682D40
[[nodiscard]] float HeatCapacity(entt::entity object);
/// info +0x90 (fn_00730230)
[[nodiscard]] float DefenceMultiplierBurn(entt::entity object);
/// info +0xB8 (fn_007302E0 reads it for the fire group)
[[nodiscard]] float BurningPriority(entt::entity object);

/// GetDefaultFireCentrePos (vt 0x5F0): the object's position (0x639AA0); DeadTree 0x510CE0: its mesh centre in x, z
/// (with its own height above the land). World x, z and the height above the land (MapCoords y) in y.
[[nodiscard]] glm::vec3 FireCentre(entt::entity object);
/// GetDefaultFireRadius (vt 0x5F4): Get2DRadius (0x639AC0); DeadTree 0x510E10: 0.35 x its height; WorshipSite 0x77DE10:
/// 14 (ecs::object::GetDefaultFireRadius)
[[nodiscard]] float DefaultFireRadius(entt::entity object);
/// GetHeight (vt 0x42C, Object 0x638120; ecs::object::GetHeight)
[[nodiscard]] float Height(entt::entity object);
/// GetRadius (vt 0x60, Object 0x638110 = Get2DRadius; ecs::object::GetRadius)
[[nodiscard]] float Radius(entt::entity object);
/// GetRainCoolingMultiplier (vt 0x5EC): 0.01; MagicFireBall 0x682DB0: 0 when the script cast it
[[nodiscard]] float RainCoolingMultiplier(entt::entity object);

/// GameThing::IsAvailable (vt 0x2C): still in the world
[[nodiscard]] bool IsAvailable(entt::entity object);
/// Object::IsObjectInMap (vt 0x178): a MagicFireBall is not (InsertMapObject is empty)
[[nodiscard]] bool IsObjectInMap(entt::entity object);
[[nodiscard]] bool IsVillager(entt::entity object);
[[nodiscard]] bool IsCreature(entt::entity object);
/// Object +0x24 bit 2: in the hand (GetPlayerHoldingThis 0x63A190 gives the holder)
[[nodiscard]] bool InHand(entt::entity object);
/// Object::IsEffectReceiver (vt 0x774) for a burn: Object 1; Villager 0x751D70; Pot 0x66D650 (+0x70 != 0); Field
/// 0x528900
[[nodiscard]] bool IsBurnReceiver(entt::entity object, float burn);

/// Object +0x0A bit 3 (SET_SET_ON_FIRE false): it never catches fire
[[nodiscard]] bool CannotBeSetOnFire(entt::entity object);
void SetCannotBeSetOnFire(entt::entity object, bool value);
/// Object +0x0A bit 2 (SET_HURT_BY_FIRE false): burning does it no harm
[[nodiscard]] bool NotHurtByFire(entt::entity object);
void SetNotHurtByFire(entt::entity object, bool value);

/// ReduceLifeDueToBurning (vt 0x5C4): Object 0x637C20 (ReduceLife unless +0x0A bit 2; the town's aggressor is not
/// ported), Field 0x52A050 (RemoveFood(damage x info +0x130), returns 1). Returns the life after it.
float ReduceLifeDueToBurning(entt::entity object, float damage, bool hasPlayer, PlayerNames player);
/// DestroyedByEffect (vt 0x5F8): Object 0x6378E0 = ToBeDeleted (features too); Villager 0x7502D0 dies; Field 0x52A010
/// empties and deletes its fire;
/// Abode 0x403F80 (building site, ghost) is not ported yet (the abode stays at life 0)
void DestroyedByEffect(entt::entity object);
/// StartOnFire (vt 0x6BC): MultiMapFixed 0x52EC60 / DeadTree 0x510E20 drop their reactions; Pot 0x66D6C0 its own
void StartOnFire(entt::entity object);
/// EndOnFire (vt 0x6C0): DeadTree 0x510E60 creates REACT_TO_WOOD; Pot 0x66D6D0 re-creates its reaction (not ported)
void EndOnFire(entt::entity object);

/// A land is loaded
void Clear();
} // namespace openblack::ecs::fire::traits
