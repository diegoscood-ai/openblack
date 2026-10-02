/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipSite.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownInfluence.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/Transform.h"
#include "ECS/GUtilsAngle.h"
#include "ECS/MapCoords.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "SpecialPoints.h"
#include "WorshipSpellIcon.h"
#include "WorshipTrace.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
/// The citadel heart's worship-site mesh (fn_008829C0 loads Data\Citadel\OutsideMeshes\B_WORSHIP.l3d into the heart's
/// 3D object +0xAC; the site's CallVirtualFunctionsForCreation 0x77B9D0 gives it to the site: vt 0xF4)
constexpr auto k_SiteMesh = entt::hashed_string("temple/B_WORSHIP_l3d");
/// Citadel::GetWorshipSiteAngle 0x463610: slot x 2 pi / 7 (0x8C836C)
constexpr float k_SlotAngle = 0.8975979f;
/// WorshipSite::GetSpellIconPos 0x77B080: the rings (7.5 m apart, 0x8C2C40) up to 30 m (0x8BF51C)
constexpr float k_IconRingStep = 7.5f;
constexpr float k_IconRingMax = 30.0f;
/// the dance ring (see DancePosition); (inferido): 6 m has no source, the .DAN rings are not ported
constexpr float k_DanceRadius = 6.0f;

WorshipSite& SiteOf(entt::entity site)
{
	return Locator::entitiesRegistry::value().Get<WorshipSite>(site);
}

bool IsSite(entt::entity site)
{
	auto& registry = Locator::entitiesRegistry::value();
	return site != entt::null && registry.Valid(site) && registry.AllOf<WorshipSite>(site);
}

/// Town::GetTribe 0x73C840
Tribe TribeOfTown(entt::entity town)
{
	const auto* tribe = Locator::entitiesRegistry::value().TryGet<const Tribe>(town);
	return tribe != nullptr ? *tribe : Tribe::NONE;
}

PlayerNames OwnerOfTown(entt::entity town)
{
	const auto* data = Locator::entitiesRegistry::value().TryGet<const Town>(town); // Town +0x2C
	return data != nullptr ? data->owner : PlayerNames::NEUTRAL;
}

/// fn_0073D2E0: the town has a TownSpellIcon of that seed
bool TownHasSpellIcon(entt::entity town, SpellSeedType seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* magic = registry.TryGet<const TownMagic>(town);
	if (magic == nullptr)
	{
		return false;
	}
	return std::ranges::any_of(magic->spellIcons, [&](entt::entity icon) {
		const auto* component = registry.TryGet<const SpellIcon>(icon);
		return component != nullptr && component->seedType == seed;
	});
}

/// Citadel::GetWorshipSiteAngle 0x463610
float SlotAngle(const CitadelWorship& citadel, int slot)
{
	return citadel.heartYAngle + static_cast<float>(slot) * k_SlotAngle;
}

/// The site's matrix at a slot: the citadel's origin (fn_0077A960), turned to the slot's angle, scale 1.
/// (inferido): the snap to the land's height is openblack's
Transform SiteTransform(const glm::vec3& citadelPosition, float yAngle)
{
	glm::vec3 origin = citadelPosition;
	origin.y = GroundAt(origin);
	return Transform {origin, glm::mat3(glm::eulerAngleY(-yAngle)), glm::vec3(1.0f)};
}

/// fn_00467890(heart, 9, angle) (called at 0x463587): the heart's B_WORSHIP special point 9 turned to the angle.
/// UNVERIFIED (sources.md §10.3): fn_00467890 is not read; its radius may be info.radiusFromCitadel (37.5)
std::optional<glm::vec3> HeartRingPoint(const glm::vec3& citadelPosition, float yAngle)
{
	if (!Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(k_SiteMesh))
	{
		return std::nullopt;
	}
	const auto& metrics = Locator::resources::value().GetMeshes().Handle(k_SiteMesh)->GetExtraMetrics();
	if (metrics.size() <= static_cast<size_t>(site::Point::Arrive))
	{
		return std::nullopt;
	}
	const auto transform = SiteTransform(citadelPosition, yAngle);
	return transform.position + transform.rotation * glm::vec3(metrics[static_cast<size_t>(site::Point::Arrive)][3]);
}

/// Citadel::FindNearestWorshipSitePosAngleAndSlotToPos 0x463540: of the free slots, the one whose ring point is nearest
/// (GUtils::GetDistanceInMetres, x and z) to `near`; -1 when all are taken
int FindNearestFreeSlot(const CitadelWorship& citadel, const glm::vec3& citadelPosition, const glm::vec3& near)
{
	int best = -1;
	float bestDistance = 1e6f; // 0x497423F0
	for (int slot = 0; slot < static_cast<int>(CitadelWorship::k_Sites); ++slot)
	{
		if (citadel.sites[static_cast<size_t>(slot)] != entt::null)
		{
			continue;
		}
		const auto point = HeartRingPoint(citadelPosition, SlotAngle(citadel, slot));
		if (!point)
		{
			continue;
		}
		const float distance = glm::distance(glm::vec2(point->x, point->z), glm::vec2(near.x, near.z));
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = slot;
		}
	}
	return best;
}

/// WorshipSite::GetSpellIconPosFromSlot 0x77AFC0: the slot's point pushed `ring` metres outward along the ray from the
/// site's origin through it. With ring > 0 (`fcomp 0; test ah, 0x41`, 0x77AFEC) the point's altitude is set to 0
/// (0x77B002) and point += GetPosFromAngle(Get3DAngleFromXZ(site +0x14, point), ring) (0x77B00A..0x77B02F): it ends
/// on the ground. With ring 0 the special point stays as it is
std::optional<glm::vec3> IconPositionFromSlot(entt::entity site, int slot, float ring)
{
	const auto point = site::GetSpecialPos(site, slot);
	if (!point)
	{
		return std::nullopt;
	}
	auto position = *point;
	if (ring > 0.0f)
	{
		const auto& origin = Locator::entitiesRegistry::value().Get<const Transform>(site).position;
		auto coords = ecs::map_coords::FromMetres(glm::vec2(position.x, position.z)); // altitude 0
		const float angle = gutils::Get3DAngleFromXZ(ecs::map_coords::FromMetres(glm::vec2(origin.x, origin.z)), coords);
		coords += gutils::GetPosFromAngle(angle, ring);
		const auto xz = ecs::map_coords::ToMetres(coords);
		position = glm::vec3(xz.x, 0.0f, xz.y);
		position.y = GroundAt(position); // GetLHPoint: the ground + the altitude 0
	}
	return position;
}

/// WorshipSite::GetSpellIconPos 0x77B080: for the rings 0, 7.5 .. 30, the first slot 10..15 whose candidate is not
/// within 1.0 of an icon of the site (MapCoords::IsCloseToEqual, inf: per axis); slot -1 when there is no room
glm::vec3 FindIconPosition(entt::entity site, int16_t& slot)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& icons = SiteOf(site).icons;
	glm::vec3 candidate(0.0f);
	for (float ring = 0.0f; ring <= k_IconRingMax; ring += k_IconRingStep)
	{
		for (int s = static_cast<int>(site::Point::FirstIcon); s <= static_cast<int>(site::Point::LastIcon); ++s)
		{
			// (inferido): without the mesh's point the candidate is the site's origin (the original has no fallback)
			candidate = IconPositionFromSlot(site, s, ring).value_or(registry.Get<const Transform>(site).position);
			const bool taken = std::ranges::any_of(icons, [&](entt::entity icon) {
				const auto& at = registry.Get<const Transform>(icon).position;
				return std::abs(at.x - candidate.x) <= 1.0f && std::abs(at.z - candidate.z) <= 1.0f;
			});
			if (!taken)
			{
				slot = static_cast<int16_t>(s);
				return candidate;
			}
		}
	}
	slot = -1;
	return candidate;
}

/// WorshipTotem::Create 0x780930 (ctor 0x780840): the tribe's altar at the site's special point 8, at the site's
/// angle and scale
entt::entity CreateTotem(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& site = SiteOf(siteEntity);
	const auto point = site::GetSpecialPos(siteEntity, site::Point::DanceCentre);
	if (!point)
	{
		return entt::null;
	}
	const auto totem = registry.Create();
	ecs::object_index::Assign(totem);
	registry.Assign<Transform>(totem, *point, glm::mat3(glm::eulerAngleY(-site.yAngle)), glm::vec3(1.0f));
	// WorshipTotem::GetMesh 0x780A70: GWorshipSiteInfo +0x124 (file +0x114 = meshType)
	registry.Assign<Mesh>(totem, resources::HashIdentifier(site::InfoOf(site).meshType), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	registry.Assign<WorshipTotem>(totem, siteEntity);
	return totem;
}

/// WorshipSite::CreateFoodPot 0x77AD90: pot info 2 (StoragePitFoodPile, 0xD4C8E8) at the resource position
/// (GetFoodPosAndYAngle 0x77CDB0: the site's local point (9, 0, -38)), its angle + 1.5 (0x99C45C), scale 0.7 (0x99C460)
entt::entity CreateFoodPot(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(siteEntity);
	glm::vec3 position = transform.position + transform.rotation * glm::vec3(9.0f, 0.0f, -38.0f);
	position.y = GroundAt(position);
	const float yAngle = SiteOf(siteEntity).yAngle + 1.5f;
	const auto pot = ecs::archetypes::PotArchetype::Create(position, yAngle, PotInfo::StoragePitFoodPile, 0, true);
	if (pot != entt::null)
	{
		if (auto* potTransform = registry.TryGet<Transform>(pot); potTransform != nullptr)
		{
			potTransform->scale = glm::vec3(0.7f);
		}
	}
	return pot;
}
} // namespace

const GWorshipSiteInfo& site::InfoOf(const WorshipSite& site)
{
	return Locator::infoConstants::value().worshipSite.at(site.infoIndex);
}

entt::entity site::Create(entt::entity citadelEntity, Tribe tribe, const glm::vec3& near)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (citadelEntity == entt::null || !registry.Valid(citadelEntity) || tribe == Tribe::NONE)
	{
		return entt::null;
	}
	auto& citadel = registry.Get<CitadelWorship>(citadelEntity);
	const auto& citadelTransform = registry.Get<const Transform>(citadelEntity);
	const int slot = FindNearestFreeSlot(citadel, citadelTransform.position, near);
	if (slot < 0)
	{
		return entt::null;
	}
	const float yAngle = SlotAngle(citadel, slot);

	// fn_0077A960: CitadelPart(pos = the citadel's origin, info, citadel, slot, tribe, angle, scale 1)
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	const auto transform = SiteTransform(citadelTransform.position, yAngle);
	registry.Assign<Transform>(entity, transform);
	registry.Assign<Mesh>(entity, k_SiteMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
	auto& site = registry.Assign<WorshipSite>(entity);
	site.citadel = citadelEntity;
	site.player = registry.Get<const Temple>(citadelEntity).owner;
	site.infoIndex = static_cast<uint8_t>(tribe); // GTribeInfo.worshipSiteInfo = the tribe's index
	site.tribe = tribe;
	site.slot = static_cast<uint8_t>(slot);
	site.yAngle = yAngle;
	citadel.sites[static_cast<size_t>(slot)] = entity; // fn_00463770

	// WorshipSite::AssignTownsToWorshipSite 0x77AF70: the player's towns (GPlayer +0xA50) of the site's tribe
	std::vector<entt::entity> towns;
	registry.Each<const Town, const Tribe>([&](entt::entity town, const Town& data, const Tribe& t) {
		if (data.owner == site.player && t == tribe)
		{
			towns.push_back(town);
		}
	});
	for (const auto town : towns)
	{
		AddTown(entity, town);
	}
	auto& created = SiteOf(entity);
	created.totem = CreateTotem(entity);
	if (created.totem != entt::null)
	{
		// only with a totem (0x77AD5D); fn_0077AEE0: the Dance of GDanceInfo[19 + slot] (0xCC4B80 + (19 + slot) x 0xB0)
		// at point 8; fn_0077B8D0(0.5) 0x77AF5B; CreateFoodPot 0x77AF62 (AddResource 0x77C638 remakes it when gone)
		SetDanceIntensity(entity, 0.5f);
		SiteOf(entity).foodPot = CreateFoodPot(entity);
	}
	const auto& placed = Locator::entitiesRegistry::value().Get<const Transform>(entity).position;
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Worship: site {} of player {} ({}) in slot {} at ({:.1f}, {:.1f}) angle {:.3f}, {} towns",
	                   static_cast<uint32_t>(entity), static_cast<int>(site.player), static_cast<int>(tribe), slot, placed.x,
	                   placed.z, yAngle, SiteOf(entity).towns.size());
	return entity;
}

std::optional<glm::vec3> site::GetSpecialPos(entt::entity site, Point point)
{
	return GetSpecialPos(site, static_cast<int>(point));
}

std::optional<glm::vec3> site::GetSpecialPos(entt::entity site, int point)
{
	const auto special = GetSpecialPoint(site, point);
	if (!special)
	{
		return std::nullopt;
	}
	return special->position;
}

void site::AddTown(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* magic = registry.TryGet<TownMagic>(town); magic != nullptr)
	{
		magic->worshipSite = siteEntity; // Town::SetWorshipSite 0x73D030
	}
	// the footpath link and GFootpathFinder from the town (fn_007412A0, GFootpathLink::GetNearestPathToQuick): the
	// villagers walk there with openblack's pathfinding instead
	AddTownSpells(siteEntity, town);
	auto& towns = SiteOf(siteEntity).towns;
	towns.insert(towns.begin(), town);
}

void site::RemoveTown(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& towns = SiteOf(siteEntity).towns;
	towns.erase(std::remove(towns.begin(), towns.end(), town), towns.end());
	if (auto* magic = registry.TryGet<TownMagic>(town); magic != nullptr)
	{
		for (const auto icon : std::vector<entt::entity>(magic->spellIcons))
		{
			if (const auto* component = registry.TryGet<const SpellIcon>(icon); component != nullptr)
			{
				RemoveSpellIconIfUnheld(siteEntity, component->seedType);
			}
		}
		magic->worshipSite = entt::null;
	}
}

void site::AddTownSpells(entt::entity siteEntity, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* magic = registry.TryGet<const TownMagic>(town);
	if (magic == nullptr)
	{
		return;
	}
	// Town::GetNextSpellIcon 0x73D360 from the head
	for (const auto icon : std::vector<entt::entity>(magic->spellIcons))
	{
		if (const auto* component = registry.TryGet<const SpellIcon>(icon); component != nullptr)
		{
			AddSpellIconIfNecessary(siteEntity, component->seedType);
		}
	}
}

void site::AddSpellIconIfNecessary(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (const auto existing = GetSpellIconFromSeedType(siteEntity, seed); existing != entt::null)
	{
		// WorshipSpellIcon::StopRemoveFromPlayer 0x77FF40 when it was fading
		auto& icon = registry.Get<WorshipSpellIcon>(existing);
		if (icon.removeTimer != 0)
		{
			registry.Get<Transform>(existing).scale = glm::vec3(icon.savedScale);
			icon.removeTimer = 0;
		}
		return;
	}
	int16_t slot = -1;
	const auto position = FindIconPosition(siteEntity, slot);
	icon::Create(position, seed, siteEntity, slot);
}

void site::RemoveSpellIconIfUnheld(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	const auto& site = SiteOf(siteEntity);
	if (std::ranges::any_of(site.towns, [&](entt::entity town) { return TownHasSpellIcon(town, seed); }))
	{
		return;
	}
	for (const auto icon : std::vector<entt::entity>(site.icons))
	{
		if (icon::SeedTypeOf(icon) == seed)
		{
			icon::ToBeDeleted(icon);
		}
	}
}

entt::entity site::GetSpellIconFromSeedType(entt::entity siteEntity, SpellSeedType seed)
{
	if (!IsSite(siteEntity))
	{
		return entt::null;
	}
	for (const auto icon : SiteOf(siteEntity).icons)
	{
		if (icon::SeedTypeOf(icon) == seed) // SpellIcon::IsSpellSeed 0x726310
		{
			return icon;
		}
	}
	return entt::null;
}

entt::entity site::GetSpellIconFromMagicType(entt::entity siteEntity, MagicType type)
{
	return GetSpellIconFromSeedType(siteEntity, magic::GetFirstSpellSeedForMagicType(Locator::infoConstants::value(), type));
}

int site::DancerCount(const WorshipSite& site)
{
	return static_cast<int>(site.dancers.size());
}

float site::Capacity(const WorshipSite& site)
{
	const float tribalPower = magic::players::MagicOf(site.player).tribalPower[2]; // GPlayer +0x70
	return static_cast<float>(DancerCount(site)) * InfoOf(site).chantsPerVillager * tribalPower;
}

float site::MaxBattery(const WorshipSite& site)
{
	const auto& info = InfoOf(site);
	return static_cast<float>(DancerCount(site)) * info.eachVillagerAddToFillBattery + info.chantsToFillBattery;
}

float site::Available(const WorshipSite& site)
{
	return site.infiniteChants ? 1e6f : site.available - site.used;
}

float site::TotalChantsAvailable(const WorshipSite& site)
{
	return site.infiniteChants ? 1e6f : site.available;
}

float site::AvailableForIcons(const WorshipSite& site, bool seedsOut)
{
	const float available = Available(site);
	if (!seedsOut)
	{
		return available;
	}
	// fn_0077A950 reads the int 500 as a float (~7e-43)
	const float reserved = available - InfoOf(site).chantsToReserveForMaintaining;
	return 0.0f < reserved ? reserved : 0.0f;
}

float site::UseChants(entt::entity siteEntity, float amount)
{
	if (amount < 0.0f)
	{
		return 0.0f;
	}
	auto& site = SiteOf(siteEntity);
	site.requested += amount;
	float used = amount;
	if (Available(site) < amount)
	{
		used = Available(site);
		site.used = site.available;
	}
	else
	{
		site.used += amount;
	}
	magic::players::MagicOf(site.player).chantsUsed += used; // GPlayer +0xA44 -> +0x1120
	return used;
}

float site::UseChantsIfNotInfinite(entt::entity siteEntity, float amount)
{
	return SiteOf(siteEntity).infiniteChants ? amount : UseChants(siteEntity, amount);
}

float site::MaintainSpell(entt::entity siteEntity, float amount)
{
	return SiteOf(siteEntity).freeMaintenance ? amount : UseChants(siteEntity, amount);
}

void site::ProcessSpellIcons(entt::entity siteEntity)
{
	auto& registry = Locator::entitiesRegistry::value();
	{
		auto& site = SiteOf(siteEntity);
		const float capacity = Capacity(site);
		if (capacity != 0.0f)
		{
			site.strain = (site.requested - capacity) / capacity;
		}
		else
		{
			site.strain = site.requested != 0.0f ? 1.0f : 0.0f;
		}
		if (site.strain <= 0.0f)
		{
			// the spells did not ask for more than the dancers make: the charging icons share what is left
			float count = 0.0f;
			float needed = 0.0f;
			bool seedsOut = false;
			site.iconTookChants = false;
			for (const auto icon : site.icons)
			{
				if (icon::IsCharging(icon, PlayerNames::NEUTRAL, true) && icon::GetChantNeeded(icon) > 0.0f)
				{
					count += 1.0f;
					needed += icon::GetChantNeeded(icon);
				}
				if (!registry.Get<const WorshipSpellIcon>(icon).seeds.empty())
				{
					seedsOut = true;
				}
			}
			if (count != 0.0f)
			{
				const float available = AvailableForIcons(site, seedsOut);
				const float share = (available < needed ? available : needed) / count;
				if (share != 0.0f) // 0x77B602..0x77B60D (count != 0: 0x77B5C0)
				{
					for (const auto icon : std::vector<entt::entity>(site.icons))
					{
						if (icon::IsCharging(icon, PlayerNames::NEUTRAL, true) && icon::GetChantNeeded(icon) > 0.0f)
						{
							const float taken = icon::AddToChantStore(icon, share);
							UseChantsIfNotInfinite(siteEntity, taken);
							SiteOf(siteEntity).iconTookChants = true;
						}
					}
				}
			}
		}
	}
	for (const auto icon : std::vector<entt::entity>(SiteOf(siteEntity).icons))
	{
		if (registry.Valid(icon))
		{
			icon::Process(icon); // vt 0x5FC
		}
	}

	// fn_0077B6A0: the end of the turn
	auto& site = SiteOf(siteEntity);
	const float capacity = Capacity(site);
	const float ratio = capacity != 0.0f ? site.used / capacity : 1.0f;
	float boost = 0.5f - site.battery / MaxBattery(site) * 0.5f;
	boost = boost <= 0.0f ? 0.0f : std::max(boost, 0.2f);
	float intensity = ratio + boost;
	if (!(intensity < 1.0f))
	{
		intensity = 1.0f;
	}
	SetDanceIntensity(siteEntity, intensity);
	const float produced = capacity * intensity;
	const int dancers = DancerCount(site);
	// every 1000 turns the artifacts on the site (+0xAC) get [0xD1A298] x artifactPowerupMultiplier x N / the player's
	// fn_0064D0E0 (fn_00426A80): openblack has no artifacts (research R12)
	site.chantDamage = dancers != 0 ? produced / static_cast<float>(dancers) : 0.0f;
	float battery = site.battery - (site.used - produced);
	if (battery <= 0.0f)
	{
		battery = 0.0f;
	}
	site.battery = battery;
	site.used = 0.0f;
	site.requested = 0.0f;
	site.available = battery + capacity;
	if (trace::Enabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Worship trace: site {} icons {} N {} C {:.1f} k {:.3f} strain {:.3f} battery {:.1f} / {:.0f} "
		                   "available {:.1f} damage {:.2f}",
		                   static_cast<uint32_t>(siteEntity), site.icons.size(), dancers, capacity, intensity, site.strain,
		                   site.battery, MaxBattery(site), site.available, site.chantDamage);
	}
}

void site::SetMana(entt::entity siteEntity, float chants)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto icon : SiteOf(siteEntity).icons)
	{
		registry.Get<WorshipSpellIcon>(icon).chantStore = 0.0f; // fn_0077B060
	}
	SiteOf(siteEntity).battery = chants;
}

void site::UpdateStrainVisual(entt::entity siteEntity, float milliseconds)
{
	auto& site = SiteOf(siteEntity);
	if (site.strain > 0.0f)
	{
		const float strain = std::clamp(site.strain, 0.0f, 1.0f);
		site.strainPhase = std::fmod(site.strainPhase + (5.0f + 5.0f * strain) * milliseconds * 0.001f,
		                             glm::two_pi<float>());
		site.strainPulse = (std::cos(site.strainPhase) + 1.0f) * 0.5f;
	}
	// the mana path's alpha: strain > 0 ? 255 x pulse : 255 (the mana path PSys is the casting side, Spell::CreateSpellPoint)
}

void site::SetDanceIntensity(entt::entity siteEntity, float intensity)
{
	auto& site = SiteOf(siteEntity);
	if (intensity > 0.0f)
	{
		if (site.danceState == 0)
		{
			site.danceState = 1; // Dance::SetState 0x50BAF0
		}
	}
	else if (site.danceState == 1)
	{
		site.danceState = 0;
	}
	site.danceSpeed = intensity; // fn_0050C340
}

entt::entity site::FindAt(const glm::vec3& position)
{
	// MapCoords::FindWorshipSite 0x602460: the first object of type 8 in the cell that is a WorshipSite, or a
	// WorshipSpellIcon's site. The site's cells are its collide footprint (CreateCollideData 0x77E490, not ported):
	// (inf) the B_WORSHIP mesh's box in the site's frame
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	const auto& meshes = Locator::resources::value().GetMeshes();
	registry.Each<const WorshipSite, const Transform>([&](entt::entity entity, const WorshipSite&, const Transform& transform) {
		if (found != entt::null || !meshes.Contains(k_SiteMesh))
		{
			return;
		}
		const auto box = meshes.Handle(k_SiteMesh)->GetBoundingBox();
		const auto local = glm::transpose(transform.rotation) * (position - transform.position);
		if (local.x >= box.minima.x && local.x <= box.maxima.x && local.z >= box.minima.z && local.z <= box.maxima.z)
		{
			found = entity;
		}
	});
	if (found != entt::null)
	{
		return found;
	}
	// the MapCoords cell: 10 units (16.16 coordinate x 0.1, see LandIsland.cpp / SoundMap.cpp); (inferido): an icon is
	// only in the cell of its position (its collide footprint is not ported)
	registry.Each<const WorshipSpellIcon, const Transform>(
	    [&](entt::entity, const WorshipSpellIcon& icon, const Transform& transform) {
		    if (found == entt::null && ecs::map_coords::CellOf(transform.position) == ecs::map_coords::CellOf(position))
		    {
			    found = icon.site;
		    }
	    });
	return found;
}

void site::AddDancer(entt::entity siteEntity, entt::entity villager)
{
	auto& dancers = SiteOf(siteEntity).dancers;
	if (std::ranges::find(dancers, villager) == dancers.end())
	{
		dancers.push_back(villager);
	}
}

void site::RemoveDancer(entt::entity siteEntity, entt::entity villager)
{
	if (!IsSite(siteEntity))
	{
		return;
	}
	auto& dancers = SiteOf(siteEntity).dancers;
	dancers.erase(std::remove(dancers.begin(), dancers.end(), villager), dancers.end());
}

glm::vec3 site::DancePosition(entt::entity siteEntity, entt::entity villager)
{
	const auto& site = SiteOf(siteEntity);
	const auto centre = GetSpecialPos(siteEntity, Point::DanceCentre)
	                        .value_or(Locator::entitiesRegistry::value().Get<const Transform>(siteEntity).position);
	const auto it = std::ranges::find(site.dancers, villager);
	const auto index = static_cast<float>(it - site.dancers.begin());
	const auto count = static_cast<float>(std::max<size_t>(site.dancers.size(), 1));
	// (inferido): the ring starting at the site's yAngle and the cos/sin order are not from 0x597F20
	const float angle = index / count * glm::two_pi<float>() + site.yAngle;
	glm::vec3 position = centre + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * k_DanceRadius;
	position.y = GroundAt(position);
	return position;
}

int site::VillagersRequestingToGoHome(const WorshipSite& site)
{
	return static_cast<int>(site.goHomeRequests.size());
}
