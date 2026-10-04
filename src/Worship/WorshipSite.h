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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/WorshipSite.h"
#include "ECS/PotResource.h"
#include "Enums.h"

namespace openblack
{
struct GWorshipSiteInfo;
} // namespace openblack

// WorshipSite (Worship.cpp 0x77A960..0x77E870): the dance ground of one tribe at the player's citadel, its battery of
// prayer power and its spell icons. Research: dev\tmp_dis\miracles\sources.md §1.4, §3.3, §4.3; wiki magic.md (M7).

namespace openblack::worship::site
{
using ecs::components::WorshipSite;

/// The special points of the B_WORSHIP mesh the site uses
enum class Point : int
{
	Hide = 7,       ///< fn_0077CE70: where the villagers beyond maxDancersVisible wait
	DanceCentre = 8, ///< fn_0077CD90: the dance and the WorshipTotem (GetTotemPos 0x77CF30)
	Arrive = 9,     ///< GetArrivePos 0x77CED0 (and the heart's ring point, fn_00467890(heart, 9, angle), UNVERIFIED)
	FirstIcon = 10, ///< 10..15: the spell icon slots
	LastIcon = 15,
};

[[nodiscard]] const GWorshipSiteInfo& InfoOf(const WorshipSite& site);

/// WorshipSite::Create 0x77AC50 through the citadel (RequestANewWorshipSite 0x4633F0): the free slot nearest `near`
/// (Citadel::FindNearestWorshipSitePosAngleAndSlotToPos 0x463540), the site at the citadel's origin turned to it,
/// the towns of its tribe (AssignTownsToWorshipSite 0x77AF70), the WorshipTotem (0x780930), the dance and the food pot
/// (fn_0077AEE0). entt::null when all six slots are taken.
entt::entity Create(entt::entity citadel, Tribe tribe, const glm::vec3& near);

/// WorshipSite::GetSpecialPos 0x77CC90 (world point); nullopt without the mesh's point
[[nodiscard]] std::optional<glm::vec3> GetSpecialPos(entt::entity site, Point point);
/// with the point's rotation
[[nodiscard]] std::optional<glm::vec3> GetSpecialPos(entt::entity site, int point);

/// WorshipSite::AddTown 0x77C800: Town::SetWorshipSite, the town's spells as icons, the town in the list
void AddTown(entt::entity site, entt::entity town);
/// fn_0077C950: the town leaves (its icons go unless another town of the site holds the seed)
void RemoveTown(entt::entity site, entt::entity town);
/// WorshipSite::AddTownSpells 0x77C910: an icon for each TownSpellIcon of the town
void AddTownSpells(entt::entity site, entt::entity town);
/// WorshipSite::AddSpellIconIfNecessary 0x77C9E0
void AddSpellIconIfNecessary(entt::entity site, SpellSeedType seed);
/// fn_0077CAA0: the icon of the seed goes when no town of the site still has a spell icon of it
void RemoveSpellIconIfUnheld(entt::entity site, SpellSeedType seed);
/// WorshipSite::GetSpellIconFromSeedType 0x77B170 / GetSpellIconFromMagicType 0x77B1B0
[[nodiscard]] entt::entity GetSpellIconFromSeedType(entt::entity site, SpellSeedType seed);
[[nodiscard]] entt::entity GetSpellIconFromMagicType(entt::entity site, MagicType type);

/// fn_0077B960 -> 0x77CFB0: the dancers (Dance +0x90)
[[nodiscard]] int DancerCount(const WorshipSite& site);
/// the same by entity: 0 when it is not a site (a dance-less site has no dancers, 0x77CFB6)
[[nodiscard]] int DancerCount(entt::entity site);
/// Dance::CalculateFoodNeededByDancers 0x50BF20 (the site's dance +0xA0): the sum over the dancers of
/// (1 - food (+0xE8)) x foodReqiredForDinner (GVillagerInfo +0x2D8)
[[nodiscard]] float CalculateFoodNeededByDancers(entt::entity site);
/// WorshipSite::GetResource 0x77BD80 (vt +0x98) for FOOD: the food pot's (+0xB4) Pot::JustGetResource 0x66D390 (its
/// +0x70 amount when the pot holds food); 0 without a pot
[[nodiscard]] uint32_t GetFoodResource(entt::entity site);
/// WorshipSite::CalculateDesireForFood 0x77C310 (vt +0x420 of ??_7WorshipSite 0x8F2840): 1 - min((food + 1e-4) /
/// (needed + 1e-4), 1), food = GetFoodResource, needed = CalculateFoodNeededByDancers. 0 when it is not a site.
[[nodiscard]] float CalculateDesireForFood(entt::entity site);
/// fn_0077E060: the chants the dancers make each turn, N x chantsPerVillager x TribalPower[2] of the player (GPlayer +0x70)
[[nodiscard]] float Capacity(const WorshipSite& site);
/// fn_0077E780: chantsToFillBattery + N x eachVillagerAddToFillBattery
[[nodiscard]] float MaxBattery(const WorshipSite& site);
/// fn_0077CC10: 1e6 with the infinite cheat, else available - used
[[nodiscard]] float Available(const WorshipSite& site);
/// WorshipSite::GetTotalChantsAvailable 0x77CC30: 1e6, else +0xF8
[[nodiscard]] float TotalChantsAvailable(const WorshipSite& site);
/// fn_0077CBC0: Available, less chantsToReserveForMaintaining (~0, the info.dat bug) while an icon has seeds out; not
/// under 0
[[nodiscard]] float AvailableForIcons(const WorshipSite& site, bool seedsOut);

/// WorshipSite::UseChants 0x77BBB0: the request is booked, the chants taken (at most what is available); the player's
/// statistic. Returns what was taken.
float UseChants(entt::entity site, float amount);
/// fn_0077CC50: the amount without the infinite cheat's bookkeeping, else UseChants
float UseChantsIfNotInfinite(entt::entity site, float amount);
/// WorshipSite::MaintainSpell 0x77BC50: free with the cheat, else UseChants
float MaintainSpell(entt::entity site, float amount);

/// WorshipSite::ProcessSpellIcons 0x77B4D0 (once per turn from Citadel::ProcessSpellIcons): the strain, the charging
/// icons' share, every icon's Process, then the end of the turn fn_0077B6A0 (dance intensity, battery, chant damage)
void ProcessSpellIcons(entt::entity site);

/// GAME_SET_MANA 0x6FE800: fn_0077B060 empties every icon's store, then the battery is set
void SetMana(entt::entity site, float chants);

/// fn_0077B3B0 (per frame): the strain pulse (phase and pulse value)
void UpdateStrainVisual(entt::entity site, float milliseconds);

/// fn_0077B8D0: the dance starts (k > 0 while still) or stops (k <= 0 while dancing), its speed is k (fn_0050C340)
void SetDanceIntensity(entt::entity site, float intensity);

/// The site of a position (MapCoords::FindWorshipSite 0x602460, for seeds put down there): the first type 8 object of
/// the cell's fixed list (ecs::map_cells::FindType) if it is a site, or a worship icon's site; else null
[[nodiscard]] entt::entity FindAt(const glm::vec3& position);

/// A villager joins the dance (StartWorshippingAtWorshipSite 0x76C4C0: GroupBehaviour, Dance +0x90) or leaves it
void AddDancer(entt::entity site, entt::entity villager);
void RemoveDancer(entt::entity site, entt::entity villager);
/// The villager's place in the dance. (inf) The dance's shape comes from its .DAN file (GDanceInfo[19 + slot].fileName,
/// GroupBehaviour::CalculateDancePosition 0x597F20: groups of dancers on rings around their centre), which is not
/// ported: here the dancers stand on one ring of 6 m around the dance centre, evenly spaced (the ring part of 0x597F20,
/// 256 / N per dancer).
[[nodiscard]] glm::vec3 DancePosition(entt::entity site, entt::entity villager);

/// WorshipSite::GetNumVillagersRequestingToGoHome 0x77E260 (the queue's length)
[[nodiscard]] int VillagersRequestingToGoHome(const WorshipSite& site);

/// WorshipSite::DeleteObjectAndTakeResource 0x77E7B0 (vt +0x684; object, is): the supply help when the local hand threw
/// the object (0x77E7B6..0x77E7EC, ecs::take_resource::TriggerSupplyHelpIfThrownByMe), then
/// DoDeleteObjectAndTakeResource(object, is) 0x77E7F9 (the site's AddResource 0x77C5F0 through it). No reaction (unlike
/// the storage pit's). Returns 1 (0x77E7FF).
bool DeleteObjectAndTakeResource(entt::entity site, entt::entity object, const ecs::pot_resource::Dropper& is);
} // namespace openblack::worship::site
