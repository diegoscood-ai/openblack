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
/// counts the knock in HowManyPeople::KnockKnock 0x829690, wakes the abode's villagers
/// (Villager::SetStateWhenTappedOnAbode 0x752B80 for the whole +0xA0 list) and, for the local player, plays the hand's
/// knocking animation 0x39 (CHand::StartFixedPosAnimation 0x46C050): none of those exist in openblack yet, so only the
/// sound is here.
void InterfaceTap(entt::entity abode, const glm::vec3& handPosition);

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
/// Abode::ApplyEffectsDueToPhysicalDestruction 0x406640: the crash (SamplePlayAnimEffect {1, 0, 0x16, 9, 75}) and the
/// life. (approximate) Still openblack's first version: life = min(life, remaining), the repair base 1.1 x life - 0.1,
/// StopBeingFunctional below 0.75 and DestroyedByEffect at 0; the original's EffectValues(3) / GetDefenseMultiplier /
/// ApplyEffect path is (pending). False when the building is gone
bool OnPhysicalDamage(entt::entity building, const PhysicalDamage& hit);
/// Abode::StopBeingFunctional 0x4073C0. (pending) only logs: the villagers leaving, the store's piles, the town's
/// emergency and the repair site are not ported
void StopBeingFunctional(entt::entity building);
/// Abode::DestroyedByEffect 0x403F80: the villagers become homeless (RemoveAllVillagersFromAbode 0x404560), a store
/// loses its piles, the physics forgets it (physics::Buildings::OnBuildingDeleted) and the building goes
void DestroyedByEffect(entt::entity building);
/// MultiMapFixed::GetPercentForDrawBuilding 0x52EFD0 (vt +0x898) = min(GetPercentBuilt vt +0x880,
/// GetPercentRepairedFromWhenDamaged vt +0x888 0x52F010). The latter: not built -> 1; a DestructionMesh and a building
/// site (+0x74) -> (life - site +0x640) / (1 - site +0x640), 0 when either is 0; else life x 0.98 ([0x8CF3FC]).
/// (approximate until V6 / V11) GetPercentBuilt is 1 (abode_queries::IsBuilt) and the repair site's +0x640 is the
/// physics' BuildingDamage::repairBase, set with the damage (OnPhysicalDamage)
[[nodiscard]] float GetPercentForDrawBuilding(entt::entity building);

} // namespace openblack::ecs::abodes
