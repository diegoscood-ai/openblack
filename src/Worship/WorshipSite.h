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

// WorshipSite: the dance ground of one tribe at the player's citadel, its battery of prayer power and its spell
// icons. See docs/wiki/magic/worship.md#how-worship-fits-together.

namespace openblack::worship::site
{
using ecs::components::WorshipSite;

/// The special points of the B_WORSHIP mesh the site uses
enum class Point : int
{
	Rest = 7,        ///< The altar's rest point (the villagers beyond maxDancersVisible wait at the site's door)
	DanceCentre = 8, ///< The dance and the WorshipTotem
	Arrive = 9,      ///< Where the villagers arrive (and the heart's ring point, UNVERIFIED)
	FirstIcon = 10,  ///< 10..15: the spell icon slots
	LastIcon = 15,
};

[[nodiscard]] const GWorshipSiteInfo& InfoOf(const WorshipSite& site);

/// A new site through the citadel: the free slot nearest `near`, the site at the citadel's origin turned to it, the
/// towns of its tribe, the WorshipTotem and its three need signs, the dance and the food pot. entt::null when all six
/// slots are taken.
entt::entity Create(entt::entity citadel, Tribe tribe, const glm::vec3& near);
/// The site's own part of the deletion (abodes::OnToBeDeleted runs it from ecs::ToBeDeleted, then the citadel part
/// and fixed-map parts): the spell icons go, the totem is unlinked and goes, the towns and the citadel forget the
/// site, its town list is emptied, the food pot goes; the altar's need signs go before the totem. (not ported) the
/// dance and two objects not identified in openblack. `now` goes to the signs, the totem and the food pot
void ToBeDeleted(entt::entity site, bool now = false);

/// The world point of a special point; nullopt without the mesh's point
[[nodiscard]] std::optional<glm::vec3> GetSpecialPos(entt::entity site, Point point);
/// with the point's rotation
[[nodiscard]] std::optional<glm::vec3> GetSpecialPos(entt::entity site, int point);

/// The town's worship site is set, the town's spells become icons, the town joins the list
void AddTown(entt::entity site, entt::entity town);
/// The town leaves (its icons go unless another town of the site holds the seed)
void RemoveTown(entt::entity site, entt::entity town);
/// An icon for each TownSpellIcon of the town
void AddTownSpells(entt::entity site, entt::entity town);
/// An icon for the seed unless the site has one (a fading one is kept)
void AddSpellIconIfNecessary(entt::entity site, SpellSeedType seed);
/// The icon of the seed goes when no town of the site still has a spell icon of it
void RemoveSpellIconIfUnheld(entt::entity site, SpellSeedType seed);
/// The site's icon of a seed / of a magic type, or entt::null
[[nodiscard]] entt::entity GetSpellIconFromSeedType(entt::entity site, SpellSeedType seed);
[[nodiscard]] entt::entity GetSpellIconFromMagicType(entt::entity site, MagicType type);

/// The dancers
[[nodiscard]] int DancerCount(const WorshipSite& site);
/// the same by entity: 0 when it is not a site (a dance-less site has no dancers)
[[nodiscard]] int DancerCount(entt::entity site);
/// The sum over the dancers of (1 - food) x foodReqiredForDinner
[[nodiscard]] float CalculateFoodNeededByDancers(entt::entity site);
/// The food resource: the food pot's amount when the pot holds food; 0 without a pot
[[nodiscard]] uint32_t GetFoodResource(entt::entity site);
/// The food the site wants its towns to bring: CalculateFoodNeededByDancers (cut to a whole number) less its pot's food
/// and the food on its way, at least none (worshipper_rules::FoodNeededFromTown); 0 when it is not a site
[[nodiscard]] int32_t CalculateFoodNeededFromTown(entt::entity site);
/// The food on its way goes down by what a villager carried (it may go below none)
void RemoveFromFoodOnTheWay(entt::entity site, uint32_t amount);
/// 1 - min((food + 1e-4) / (needed + 1e-4), 1), food = GetFoodResource, needed = CalculateFoodNeededByDancers.
/// 0 when it is not a site.
[[nodiscard]] float CalculateDesireForFood(entt::entity site);
/// The dancers' want of rest (altar_needs::DesireForRest of their lives); 0 when it is not a site
[[nodiscard]] float CalculateDesireForRest(entt::entity site);
/// The people at the site who are not dancing, a hundredth each, at most 1 (altar_needs::PeopleHidingIndicator); 0 when
/// it is not a site
[[nodiscard]] float CalculatePeopleHidingIndicator(entt::entity site);
/// Every turn, in its citadel's turn: the altar's three need signs take the site's desire for food, for rest, and its
/// people hiding. Once the altar is no longer available its signs are deleted instead
void ProcessNeedsSigns(entt::entity site);
/// The chants the dancers make each turn, N x chantsPerVillager x TribalPower[2] of the player
[[nodiscard]] float Capacity(const WorshipSite& site);
/// chantsToFillBattery + N x eachVillagerAddToFillBattery
[[nodiscard]] float MaxBattery(const WorshipSite& site);
/// 1e6 with the infinite cheat, else available - used
[[nodiscard]] float Available(const WorshipSite& site);
/// 1e6 with the infinite cheat, else available
[[nodiscard]] float TotalChantsAvailable(const WorshipSite& site);
/// Available, less chantsToReserveForMaintaining (~0, the info.dat bug) while an icon has seeds out; not
/// under 0
[[nodiscard]] float AvailableForIcons(const WorshipSite& site, bool seedsOut);

/// The request is booked, the chants taken (at most what is available); the player's statistic. Returns what was
/// taken.
float UseChants(entt::entity site, float amount);
/// The amount without the infinite cheat's bookkeeping, else UseChants
float UseChantsIfNotInfinite(entt::entity site, float amount);
/// Free with the cheat, else UseChants
float MaintainSpell(entt::entity site, float amount);

/// Once per turn from the citadel: the strain, the charging icons' share, every icon's Process, then the end of the
/// turn (dance intensity, battery, chant damage)
void ProcessSpellIcons(entt::entity site);

/// GAME_SET_MANA: every icon's store is emptied, then the battery is set
void SetMana(entt::entity site, float chants);

/// Per frame: the strain pulse (phase and pulse value)
void UpdateStrainVisual(entt::entity site, float milliseconds);

/// The dance starts (k > 0 while still) or stops (k <= 0 while dancing), its speed is k
void SetDanceIntensity(entt::entity site, float intensity);

/// The site of a position (for seeds put down there): the first type 8 object of the cell's fixed list
/// (ecs::map_cells::FindType) if it is a site, or a worship icon's site; else null
[[nodiscard]] entt::entity FindAt(const glm::vec3& position);

/// A villager joins the dance or leaves it
void AddDancer(entt::entity site, entt::entity villager);
void RemoveDancer(entt::entity site, entt::entity villager);
/// The villager out of the dance it is in, whichever site's that is (its own town's site may be gone by then)
void LeaveDance(entt::entity villager);
/// The villager's place in the dance. (inferred) The dance's shape comes from its .DAN file
/// (GDanceInfo[19 + slot].fileName: groups of dancers on rings around their centre), which is not ported: here the
/// dancers stand on one ring of 6 m around the dance centre, evenly spaced (as in the original's ring, 256 / N per
/// dancer).
[[nodiscard]] glm::vec3 DancePosition(entt::entity site, entt::entity villager);

/// The length of the go-home request queue
[[nodiscard]] int VillagersRequestingToGoHome(const WorshipSite& site);

/// Where a resource given to a worship site goes, or comes from when it is taken: WOOD or ANY to (from) its building
/// site while it has one, FOOD into (out of) its food pot, anything else nowhere
enum class AddResourceRoute : uint8_t
{
	BuildingSite,
	FoodPot,
	Nothing,
};
[[nodiscard]] AddResourceRoute AddResourceRouteOf(ResourceType type, bool hasBuildingSite);
/// The site takes a resource by AddResourceRouteOf: its building site's AddResource (the poison passed on), or the
/// food pot's own add (made first when the site has none: the pile sound, the cap, the poison, the size). Returns
/// what was taken; 0 for what is not a site
uint32_t AddResource(entt::entity site, ResourceType type, uint32_t amount, bool poisoned = false);
/// The site gives a resource by AddResourceRouteOf: its building site's RemoveResource, or the food pot's own take
/// (ecs::object_resources::RemoveFromPotDirect; no pot is made, and none gives nothing). Returns what was taken; 0 for
/// what is not a site
uint32_t RemoveResource(entt::entity site, ResourceType type, uint32_t amount);
/// Where its food is kept: its local point (9, 0, -38) through its placement, at the height of its origin; nullopt for
/// what is not a site
[[nodiscard]] std::optional<glm::vec3> GetFoodPos(entt::entity site);

/// The supply help when the local hand threw the object (ecs::take_resource::TriggerSupplyHelpIfThrownByMe), then
/// DoDeleteObjectAndTakeResource(object, is) (the site's resource is added through it). No reaction (unlike the
/// storage pit's). Returns true.
bool DeleteObjectAndTakeResource(entt::entity site, entt::entity object, const ecs::pot_resource::Dropper& is);
} // namespace openblack::worship::site
