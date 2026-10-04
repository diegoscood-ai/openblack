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

#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/MapCoords.h"

// Field.cpp of runblack.exe W120 (Field : Abode, 0x124 bytes; spec dev\documentacion\edificios\fields_features_spec.md
// §2): the field's farmers (+0xD4 / +0xD8), its food and growth, and what the villager side (Personas, V8_V9_spec §4)
// calls.

namespace openblack
{
struct GFieldTypeInfo;
}

namespace openblack::ecs
{

/// Field::Process 0x529020, once per game turn (GlobalGameLists::Process 0x591379, the first loop): every 10th turn
/// (plus the field's +0x11C) a fully sown field that is not on fire grows until it is ripe, by 2 (0.5 alignment + 1) x
/// the sun / rain multiplier of its GFieldTypeInfo, and its food with it
void ProcessFieldsTurn(uint32_t turn);

/// Field::RemoveFood 0x5295A0 (fields::RemoveFood): the wrapper the hand and the fire call (they drop the result)
int32_t RemoveFieldFood(entt::entity field, float amount);

/// Field::IsUnripe 0x5298D0 (misnamed in the symbols: true when ripe, growth >= ageRecolt)
[[nodiscard]] bool IsFieldRipe(entt::entity field);

/// Field::ApplyWaterSpell 0x528F30, the field's own part (the water miracle's drop, Magic/Spells/SpellWater.cpp, calls
/// it after the Object part, only when the field is not on fire): crops <= timesToSow -> crops = ftol(timesToSow + 1)
/// = 31, sown at once; else, while growth <= ageRecolt, growth += effectOfWaterSpell (info.dat 2.0) and food +=
/// effectOfWaterSpell x totalFoodInField / ageRecolt. Returns false when it is not a field.
bool ApplyWaterSpellToField(entt::entity field);

/// Field::Draw 0x528570: shown only with growth >= 0.25 x ageGrowth and food >= 25; sinks with its food over 1 s and
/// fades out below 20 % (PileSink / Alpha); sown again at once when empty with the mod world.crops.without-farmers
void UpdateFields(float seconds);

namespace components
{
struct Field;
}

/// Field::Draw 0x528570: the object colour the field's land light is multiplied by (fn_0080BF10), from
/// BlendColor 0x5284C0: growing, olive (full food) to light green; ripening, olive to white; ripe, white
[[nodiscard]] glm::u8vec3 FieldDrawColour(const components::Field& field);

/// The wind lean of the 16 sway slots (Tree::PreDraw 0x74A7C0: T0 = -0.03 cos(phase), the wind angle being always 0,
/// so the lean is along world z only); ripe fields shear their up axis by 1.75 x scale x this, trees by 1 x
[[nodiscard]] float WindSway(uint32_t slot);

} // namespace openblack::ecs

namespace openblack::ecs::fields
{
/// Field::DeleteDependancys 0x52813D / FishFarm::DeleteDependancys 0x52C6C9: the worker's SetTopState(163
/// DECIDE_WHAT_TO_DO) (vt +0x8E8, villager::SetTopState); its exit function ExitFarming 0x75A2A0 / ExitFishing 0x75B880
/// unlinks it (RemoveFarmer / RemoveFisherman). Nothing for a villager no longer valid
void ReleaseWorker(entt::entity villager);
/// (openblack) Game::LoadMap calls it right before Registry::Reset: entt's clear publishes on_destroy pool by pool
/// ((not verified) whether GGame::ClearMap 0x552BB0 sends any worker to 163); without it the deletion listeners
/// (on_destroy<Field> / on_destroy<FishFarm>, connected again by the next AddFarmer / AddFisherman) would run
/// villager::SetTopState and its random draws on a half-cleared registry after the new map's RNG reset
void DisconnectDeletionListeners();

/// The field's GFieldTypeInfo (+0x120, the ctor 0x527E23): InfoConstants::fieldType[type]
[[nodiscard]] const GFieldTypeInfo& InfoOf(const components::Field& field);

/// The growth of one Field::Process step (0x5290A1..0x529110): a = 2 (alignment x 0.5 + 1); x the rain multipliers
/// (+0x140 growing / +0x144 ripening) when it rains, else the sun ones (+0x138 / +0x13C); growing while growth <
/// ageGrowth (+0x120), read before the step
[[nodiscard]] float GrowthStep(float growth, const GFieldTypeInfo& info, float alignment, bool raining);

/// Field::GetPercentFull 0x529500: (float)crops / timesToSow
[[nodiscard]] float GetPercentFull(entt::entity field);
/// Field::GetFieldActivity 0x529350 (its argument is never read, `ret 4`): GetPercentFull < 1 -> 1 (to sow);
/// growth >= ageGrowth -> 2 (to harvest); else 0
[[nodiscard]] int GetFieldActivity(entt::entity field);
/// Field::GetDesireToBeFarmed 0x5293A0: a fire effect (+0x44) or not functional (vt +0xD4) -> 0; a = 1 - min(1, farmers
/// / maxFarmerInFarm); activity 2: growth >= ageRecolt ? a : 0; activity 1: (1 - min(GetPercentFull, 1)) a^3; else 0
[[nodiscard]] float GetDesireToBeFarmed(entt::entity field);
/// Field::PlantCrop 0x5291A0 (its position is not read): (float)crops < timesToSow -> crops++ and true; else false
bool PlantCrop(entt::entity field);
/// Field::GetPlantCropPos 0x529210 (misnamed: "still sowing"): (float)crops < timesToSow
[[nodiscard]] bool IsStillSowing(entt::entity field);
/// fn_00528970 RandomFarmPoint: r1 = 5 - GameFloatRand(10) ("Field.cpp" 0x164) for x, r2 = 5 - GameFloatRand(10)
/// (0x165) for z; each axis ftol((pos x 10 / 65536 + r) x 65536 / 10); the altitude copied
[[nodiscard]] map_coords::MapCoords RandomFarmPoint(entt::entity field);
/// fn_00529240 RipeFarmPoint: growth < ageRecolt -> false (no draw); else out = RandomFarmPoint (2 draws), true
bool RipeFarmPoint(entt::entity field, map_coords::MapCoords& out);
/// Field::GetArrivePos 0x529330 (vt +0x104): the position +0x14
[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity field);
/// Field::IsTouching 0x529290 (vt +0x6B4): fx - 5 <= px < fx + 5 and the same in z, in metres
[[nodiscard]] bool IsTouching(entt::entity field, const map_coords::MapCoords& pos);
/// Field::RemoveFood 0x5295A0 (EAX; the symbol's float return is not used): 0 when it has no food or is not fully
/// sown; k = ftol(amount); cost = unripe ? ftol(amount x ratioBeforeRipe + k) : k; (u32)cost < food -> food -= cost,
/// k. Otherwise it runs out: SetTemperature(0, null), the town's pulse (+0x5E8 = 1, +0x5EC = 0), and unripe: food = 0,
/// ftol(amount x ratioBeforeRipe); ripe: ftol(food) and the field cleared (food, crops, growth 0)
int32_t RemoveFood(entt::entity field, float amount);
/// Field::GetFoodValue 0x529700: growth < ageRecolt ? 0 : food
[[nodiscard]] float GetFoodValue(entt::entity field);
/// Field::AddFarmer 0x5283E0: already in the list or null -> nothing; else at the head (+0xD8++). No maximum
/// (maxFarmerInFarm is only in GetDesireToBeFarmed); TargetThing is not written
void AddFarmer(entt::entity field, entt::entity villager);
/// fn_00528340 RemoveFarmer: every node of the villager out (+0xD8-- each); the villager's TargetThing = null ALWAYS,
/// also when it was not in the list (0x528362 / 0x5283B8 / 0x5283C8; TODO(Personas): villager::SetTargetThing)
void RemoveFarmer(entt::entity field, entt::entity villager);
/// Is the villager in the list (AddFarmer's test 0x5283F2..0x5283FB)
[[nodiscard]] bool HasFarmer(entt::entity field, entt::entity villager);
/// +0xD8
[[nodiscard]] uint32_t FarmerCount(entt::entity field);
/// Field::GetTown 0x528960: +0x118 (Field::town, a Town::id), entt::null without one
[[nodiscard]] entt::entity TownOf(entt::entity field);
/// Town +0x780: the town's fields, newest first (head insertion in the ctor 0x527E64..0x527E75): the Field entities
/// with that town, by creation index from high to low. (inferred) the same order: fields never change town
[[nodiscard]] std::vector<entt::entity> TownFields(entt::entity town);
/// "in g_game +0x205C04" (ExitFarming 0x75A2A0's test): a valid entity with a Field
[[nodiscard]] bool IsField(entt::entity thing);
/// Field::DeleteDependancys 0x528100 (Abode::ToBeDeleted 0x402C6F, vt +0x910; run by an on_destroy<Field> listener that
/// AddFarmer connects, so ecs::ToBeDeleted / Registry::Destroy reach it): every farmer, head first, the next
/// taken before the call: ReleaseWorker (SetTopState(163) 0x528146) and TargetThing = null (0x52814E,
/// TODO(Personas)). The town list +0x780, the global list +0x205C04 and SetTownArea 0x5281C3 need nothing in openblack
/// (the components are the lists; town_placement recomputes the rectangle when it is read); RemoveMapObject is
/// ecs::ToBeDeleted's
void DeleteDependancys(entt::entity field);
} // namespace openblack::ecs::fields
