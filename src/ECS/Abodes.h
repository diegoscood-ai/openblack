/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack
{
struct GAbodeInfo;
struct GMultiMapFixedInfo;
}

/// The Abode class of the original (Abode.cpp 0x401350..0x409000): every village building, the fields, the totem, the
/// storage pit, the town centre and the spell dispenser included. Only the interface tap lives here for now (milestone
/// B8 of dev\tmp_dis\audio\PLAN.md): knocking on a roof.
namespace openblack::ecs::abodes
{

/// Abode::GetAbodeType 0x4061F0: the ABODE_TYPE of the abode's GAbodeInfo (+0x28 -> +0x120). nullopt when no info record
/// matches (openblack keeps the abode number and the mesh, not the info pointer)
[[nodiscard]] std::optional<AbodeType> TypeOf(entt::entity abode);

/// Abode::InterfaceValidToTap 0x406820: always 1, every abode can be tapped
[[nodiscard]] bool InterfaceValidToTap(entt::entity abode);

/// Abode::InterfaceTap 0x406830, the packet 0x20 of a tap (GInterface::SendTap 0x5D38A0). `handPosition` is the tapping
/// interface status' +0xC8 (the hand's point). The original also remembers the abode's town in [0xC4CC6C] (0x40683F),
/// counts the knock in HowManyPeople::KnockKnock 0x829690, wakes the abode's villagers (Villager::
/// SetStateWhenTappedOnAbode 0x752B80 for the whole +0xA0 list, any abode: TODO(Personas HEAD), V11_spec §5) and, for
/// the local player, plays the hand's knocking animation 0x39 (CHand::StartFixedPosAnimation 0x46C050): the town, the
/// count and the animation are not ported.
void InterfaceTap(entt::entity abode, const glm::vec3& handPosition);
/// Abode::InterfaceValidToTap 0x406820 / InterfaceTap 0x406830 in ecs::hand_tap (Register<Abode>). Once: the later
/// calls do nothing (hand_tap::Register appends without checking). AbodeArchetype::Create calls it
void RegisterTapHandler();
/// (openblack) the generated DrawMesh models (construction and FragMesh) are erased when their DrawMesh goes: the
/// listener, connected at every abode's creation (idempotent) so the physics can rely on it
void ConnectDrawMeshListener();

// ---- life and damage -----------------------------------------------------------------------------------------------

/// What the physics passes (Abode::ReactToPhysicsImpact 0x406240 -> ApplyEffectsDueToPhysicalDestruction 0x406640,
/// (Object* hitter, GPlayer* player))
struct PhysicalDamage
{
	/// the DestructionMesh's remaining part (+0x90 -> +0x18, FragMesh::GetRemaining) after the impact; nullopt without one
	/// (a building not built yet: 0x4062E8 skips the FragMesh)
	std::optional<float> remaining;
	entt::entity hitter {entt::null};
	std::optional<PlayerNames> player;
	/// the thrower is a creature (0x4064BA..: the Abode +0x7C bit 0x20 is set around the call)
	bool byCreature {false};
};
/// Abode::ApplyEffectsDueToPhysicalDestruction 0x406640: the crash (SamplePlayAnimEffect {1, 0, 0x16, 9, 75}), then
/// EffectValues(3) (info.dat effect[3]: crush 1, alignment 1) applied by the hitter: GetPlayer is the hitter's (a
/// rock's none, another mobile static's the neutral player), +0x3C the hand's (the town's aggressor); with a
/// DestructionMesh scaled by max(life - remaining, 0) and divided by GetDefenseMultiplier 0x637930; applied by
/// Object::ApplyEffect
/// 0x637980 (effects::ApplyEffect: the damage is ReduceLife below, the alignment moves) and, at life 0 from above,
/// DestroyedByEffect. Without one (not built yet) the preset as it is: its crush x the crush defence off the percent
/// built. False when the building is gone
bool OnPhysicalDamage(entt::entity building, const PhysicalDamage& hit);
/// StopBeingFunctional vt +0x918: Abode 0x4073C0 (a player and +0xB9 >= 200: the player's statistics, not ported;
/// nothing else), StoragePit 0x733960 (then Pot::SetupReaction on its piles that hold something), TownCentre 0x744A00
/// (then the town's worship percentage 0). The villagers stay and no site is made here (ReduceLife makes it)
void StopBeingFunctional(entt::entity building, std::optional<PlayerNames> player);
/// Abode::DestroyedByEffect 0x403F80 -> Abode::ToBeDeleted 0x402C60: the villagers become homeless
/// (RemoveAllVillagersFromAbode 0x404560), a store loses its piles, the graveyard hands on, the town keeps a plan of
/// it (MoveAbodeToPlannedAbodes), the site goes, the physics forgets it (physics::Buildings::OnBuildingDeleted) and the
/// building goes. (not ported) GoolooGooloo 0x5E6540, the script abode's keep path (IsInScript vt +0x448) and the
/// scaffolds' restart
void DestroyedByEffect(entt::entity building);
/// Abode::MoveAbodeToPlannedAbodes 0x404520 (vt +0x90C, from ToBeDeleted 0x402C8A): no town -> false; not
/// GetShouldNotBeAddedToPlanned (+0x7C bit 2: only the scaffolds set it, not ported, clear) and PlannedAbode::Create
/// (Abode*) 0x405660 (plans::CreateFromBuilding: a rebuild plan, +0x30 = 1, when it was built) -> true; else the
/// town's RemoveBuildingSite(this) 0x73BA20 and false
bool MoveAbodeToPlannedAbodes(entt::entity building);
/// MultiMapFixed::GetPercentForDrawBuilding 0x52EFD0 (vt +0x898) = GetPercentBuilt (vt +0x880) <=
/// GetPercentRepairedFromWhenDamaged (vt +0x888) ? GetPercentBuilt : GetPercentRepairedFromWhenDamaged
[[nodiscard]] float GetPercentForDrawBuilding(entt::entity building);
// ---- construction (V6: MultiMapFixed +0x58 / +0x5C / +0x74, spec dev\documentacion\edificios\V6_spec.md §3) ---------

/// IsBuilt vt +0x890: Abode 0x4016C0 = !(+0x58 & 2) && GetPercentBuilt (+0x5C) >= 1; a Feature 0x422110 (the same on
/// its percentBuilt; openblack's Feature keeps no +0x58); CitadelPart 0x464AD0 (the citadel heart) and WorshipSite
/// 0x77BDD0 the same as Abode's on components::CitadelPartBuild; any other MultiMapFixed 0x438D80 = 1
[[nodiscard]] bool IsBuilt(entt::entity building);
/// IsRepaired vt +0x88C: Abode 0x4016A0 and CitadelPart 0x464AB0 = GetPercentRepaired (GetLife) >= 1; any other
/// MultiMapFixed 0x438D70 = 1
[[nodiscard]] bool IsRepaired(entt::entity building);
/// GetPercentBuilt vt +0x880 0x4014F0 = +0x5C (an Abode's, a Feature's); 1 for anything else
[[nodiscard]] float GetPercentBuilt(entt::entity building);
/// GetPercentRepaired vt +0x884 0x401500 = GetLife (vt +0x11C, ecs::life)
[[nodiscard]] float GetPercentRepaired(entt::entity building);
/// GetPercentRepairedForNonFunctional vt +0x894: Abode 0x407290 = info +0x1B8 thresholdForStopBeingFunctional;
/// MultiMapFixed 0x52EFC0 = 0.75 (also an abode without an info record)
[[nodiscard]] float GetPercentRepairedForNonFunctional(entt::entity building);
/// GetDestructionMesh vt +0x8B4 (Abode 0x401700 = +0x90 FragMesh): the physics' BuildingDamage with its FragMesh
[[nodiscard]] bool HasDestructionMesh(entt::entity building);
/// +0x74: the building site (components::BuildingSite's entity) or null
[[nodiscard]] entt::entity GetBuildingSite(entt::entity building);
/// IsDrawBuilding vt +0x8A4 0x52F0C0 = +0x74 != 0 (MultiMapFixed::Draw 0x518090 then takes DrawBuilding 0x517F90)
[[nodiscard]] bool IsDrawBuilding(entt::entity building);
/// GetPercentRepairedFromWhenDamaged vt +0x888 0x52F010: not built -> 1; a DestructionMesh (+0x90, the physics'
/// BuildingDamage) and a site: a = 1 - site +0x640, b = GetPercentRepaired - site +0x640, (a == 0 || b == 0) ? 0 : b /
/// a; else GetPercentRepaired x 0.98 ([0x8CF3FC])
[[nodiscard]] float GetPercentRepairedFromWhenDamaged(entt::entity building);
/// The abode's GAbodeInfo (+0x28): the record AbodeArchetype made it with, else its number and mesh's
/// (town_stats::AbodeInfoOf with its town's tribe); null when none
[[nodiscard]] const GAbodeInfo* InfoOf(entt::entity building);
/// The building's info (+0x28) as a GMultiMapFixedInfo: an abode's GAbodeInfo (InfoOf), the citadel heart's
/// GCitadelHeartInfo, a worship site's GWorshipSiteInfo; null otherwise (building_sites reads +0x6C, +0x110, +0x118)
[[nodiscard]] const GMultiMapFixedInfo* MultiMapFixedInfoOf(entt::entity building);
/// SetShadowOnTexture (LH3DObject +4 bit 0x1000, vt +0x80 fn_7F9880) of a building: false for an abode without the
/// built bit (+0x58 bit 8): CallVirtualFunctionsForCreation 0x52EA1E..0x52EA40 turns it off for an unbuilt one,
/// MultiMapFixed::Built 0x52EC2C on again; true otherwise, drawn or not (RenderingSystem's CastsStaticShadow calls it;
/// Fisicas' CastsPhysicsShadow may)
[[nodiscard]] bool CastsShadowOnTexture(entt::entity building);

/// MultiMapFixed::BuildBy(x) 0x52ED40 (vt +0x900; the site's fn_43D080): built and not repaired -> IncreaseLife(x) (vt
/// +0x5BC, Abode 0x405ED0) and, at life >= 1, Repaired (vt +0x8AC); not built -> +0x5C += x (0 when negative), >= 1 ->
/// Built (vt +0x8A8). Then RedrawConstruction
void BuildBy(entt::entity building, float amount);
/// fn_52EDD0 0x52EDD0 (SetPercentBuilt): +0x5C = p, 0 when p < 0; +0x5C >= 1 -> Built. Then RedrawConstruction
void SetPercentBuilt(entt::entity building, float percent);
/// MultiMapFixed::Built 0x52EBB0 + Abode::Built 0x404720 (vt +0x8A8), in order: the site's ToBeDeleted; the "new
/// building" reaction 15 of a civic one (not ported); the shadow-on-texture bake (not ported, V6_pending §4); +0x58 =
/// (& ~2) | 8, +0x5C = 1; the player's GameStats (not ported) and FUN_0064da80 (multiplayer only); MakeFunctional with
/// a town. True
bool Built(entt::entity building);
/// Abode::MakeFunctional 0x4047E0 (vt +0x914) and the class parts (StoragePit 0x732F30 Town::SetStoragePit 0x73EA60,
/// Creche 0x50AB50 town +0x744, TownCentre 0x743E80 the totem and the spell icons; the graveyard's +0x748 (inferred)).
/// The order in the .cpp
void MakeFunctional(entt::entity building);
/// MultiMapFixed::Repaired 0x52EC70 + Abode::Repaired 0x4047B0 (vt +0x8AC): the site's ToBeDeleted, RemoveDamage (vt
/// +0x8B8, Abode 0x403F40: physics::Buildings::RemoveDamage), +0x58 &= ~4, MakeFunctional with a town. True
bool Repaired(entt::entity building);
/// Abode::IncreaseLife(x) 0x405ED0 (vt +0x5BC): wasAbove = vt +0x894 < life; Object::IncreaseLife 0x637870 (cap 1);
/// !wasAbove && vt +0x894 < the new life -> RestartBeingFunctional (vt +0x91C 0x401680). Returns the new life
float IncreaseLife(entt::entity building, float amount);
/// RestartBeingFunctional vt +0x91C: Abode 0x401680 = `ret`; StoragePit 0x7339D0: Pot::RemoveReaction on its available
/// piles (the food pile +0xC4, the five wood piles +0xC8)
void RestartBeingFunctional(entt::entity building);
/// CausesTownEmergencyIfDamaged vt +0x920: Abode 0x4016F0 = 0, StoragePit 0x55CCE0 = 1, TownCentre 0x55DB30 = 1
[[nodiscard]] bool CausesTownEmergencyIfDamaged(entt::entity building);
/// Abode::ReduceLife(amount, player) 0x405D90 around MultiMapFixed::ReduceLife 0x52F5E0 (repair_spec.md §2.1, §2.2):
/// built -> Object::ReduceLife; not built -> +0x5C - amount (>= 0) through SetPercentBuilt, at 0 the life too; then the
/// stop-being-functional part with the town's emergency (town_emergency::SetInStateOfEmergency for a storage pit or a
/// town centre, also an unbuilt one at 0 %) and the building site (+0x638 = 0 here: an ordinary site, repaired by the
/// builders of GetBestBuildingSite, repair_spec §5.4). A field's vt +0x5B8 (Field::ReduceLife 0x52A0A0) changes
/// nothing. The inhabitants' SetStateWhenTappedOnAbode is TODO(Personas HEAD). Returns the new life. The physics',
/// the effects', the fire's and the beam's damage comes here
float ReduceLife(entt::entity building, float amount, std::optional<PlayerNames> player);
/// Abode::GetDesireToBeRepaired 0x406970 (vt +0x8D8, with MultiMapFixed's 0x52ECE0):
/// town_desire::AbodeDesireToBeRepaired on this abode; 0 for anything else
[[nodiscard]] float GetDesireToBeRepaired(entt::entity building);

/// The partly built model of an abode with a building site and no DestructionMesh: MultiMapFixed::Draw 0x518090 ->
/// DrawBuilding 0x517F90 draws fn_816AD0 at GetPercentForDrawBuilding (physics::PartialBuild::BuildMesh) into
/// components::DrawMesh, and nothing at 0 (0x517FE0: components::NotDrawn; the footprint stays, SetFootPrintOnTexture
/// 0x52EA33). The Mesh component stays the whole model (the 3D object's mesh: sizes, map cells, type). Rebuilt only
/// when the percent changed; both go when IsDrawBuilding no longer holds. With a FragMesh the physics' RedrawBuilding
/// draws it
void RedrawConstruction(entt::entity building);

/// GScript::GetProperty 0x70E1A9 (CHL property 22 BUILT_PERCENTAGE) of an abode: GetPercentBuilt; nullopt for anything
/// else (feature_build answers for the Features)
[[nodiscard]] std::optional<float> GetBuiltPercentage(entt::entity entity);
/// GScript::SetProperty 0x70EC69 on an abode: fn_52EDD0 (SetPercentBuilt). (pending) the town's building list part
/// 0x70EC9B..0x70ECD4 is not read. False when it is not an abode
bool SetBuiltPercentage(entt::entity entity, float value);

} // namespace openblack::ecs::abodes
