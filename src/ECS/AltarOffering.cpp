/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AltarOffering.h"

#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Abodes.h"
#include "ECS/AltarSacrifice.h"
#include "ECS/Archetypes/FloatingNumberArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The floating number's least width
constexpr int k_NumberWidth = 3;

std::optional<altar_sacrifice::Offering> OfferingOf(const Registry& registry, entt::entity held)
{
	if (registry.AllOf<Villager>(held))
	{
		return altar_sacrifice::Offering::Villager;
	}
	if (registry.AllOf<Animal>(held))
	{
		return altar_sacrifice::Offering::Animal;
	}
	if (registry.AllOf<Tree>(held))
	{
		return altar_sacrifice::Offering::Tree;
	}
	if (registry.AllOf<DeadTree>(held))
	{
		return altar_sacrifice::Offering::DeadTree;
	}
	return std::nullopt;
}

/// The sacrifice's sounds at the hand's point, owned by the thing given and not following it
void PlaySounds(entt::entity held, altar_sacrifice::Offering kind, bool child, bool female, glm::vec3 handPoint)
{
	const auto randomSample = [](uint32_t first, uint32_t count) {
		return static_cast<uint32_t>(audio::tags::RandomSample(static_cast<int>(first), static_cast<int>(count)));
	};
	const auto play = [held, handPoint](const altar_sacrifice::Sound& sound) {
		audio::PlayOptions options;
		options.sample = {audio::Bank(sound.bank), static_cast<int>(sound.sample)};
		options.owner = audio::Owner::Thing(held);
		options.is3D = true;
		options.track = false;
		options.position = handPoint;
		audio::PlaySoundEffect(options);
	};
	altar_sacrifice::PlaySounds(kind, child, female, randomSample, play);
}

/// The light that rises from the top of the altar, its point and height, for the site's player
void RaiseLight(entt::entity altar, PlayerNames sitePlayer)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto top = object::MapCoordsOf(altar);
	top.altitude = top.altitude + object::GetHeight(altar);
	auto& particles = Locator::particleSystem::value();
	const auto visual =
	    particles.StartSpotVisual(SpotVisualType::ObjectAppear, map_coords::ToWorld(top), std::nullopt, entt::null, 1.0f);
	if (visual != entt::null)
	{
		particles.SetSpotVisualPlayer(visual, static_cast<int>(sitePlayer));
	}
}

/// The prayer power floats up in red, a whole number at least three wide, from the land under the pointer (as the
/// believers gained do: the map's corner when the pointer finds no land)
void FloatPrayerUp(float prayer)
{
	const auto hover = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHoverLandPoint() : std::nullopt;
	const auto point = hover.value_or(glm::vec3(0.0f));
	const auto at = map_coords::ToWorld(map_coords::MapCoords {map_coords::ToFixed(point.x), map_coords::ToFixed(point.z)});
	archetypes::FloatingNumberArchetype::Create(at, prayer, altar_sacrifice::k_NumberColour, k_NumberWidth);
}

/// The sacrifice value of a thing's kind in the game's tables
float TableValueOf(entt::entity held)
{
	const auto* info = fire::traits::InfoOf(held);
	return info != nullptr ? info->sacrificeValue : 0.0f;
}

} // namespace

bool altar_offering::IsAltar(entt::entity object)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return object != entt::null && registry.Valid(object) && registry.AllOf<WorshipTotem>(object);
}

bool altar_offering::Takes(entt::entity altar, entt::entity held)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return IsAltar(altar) && held != entt::null && registry.Valid(held) && OfferingOf(registry, held).has_value();
}

altar_offering::Answer altar_offering::Offer(entt::entity altar, entt::entity held, PlayerNames player,
                                             std::optional<glm::vec3> handPoint)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!IsAltar(altar) || held == entt::null || !registry.Valid(held))
	{
		return Answer::NotAnAltar;
	}
	const auto kind = OfferingOf(registry, held);
	const auto site = registry.Get<const WorshipTotem>(altar).site;
	auto* worship = site != entt::null && registry.Valid(site) ? registry.TryGet<WorshipSite>(site) : nullptr;
	if (!kind.has_value() || worship == nullptr)
	{
		return Answer::NotAnAltar;
	}

	const bool isVillager = *kind == altar_sacrifice::Offering::Villager;
	const bool child = isVillager && villager::IsChild(held);
	const float life = ecs::life::LifeOf(held);
	const auto sacrifice = altar_sacrifice::Judge({
	    .kind = *kind,
	    .life = life,
	    .tableValue = TableValueOf(held),
	    .child = child,
	    .indestructible = registry.AllOf<Indestructible>(held),
	    .siteBuilt = abodes::IsBuilt(site),
	});
	if (!sacrifice.has_value())
	{
		return Answer::Refused;
	}

	// its prayer power is stored at the site, however full, and counted among what was sacrificed there
	worship->battery += sacrifice->prayer;
	worship->sacrificed += sacrifice->prayer;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Sacrifice of kind {} at the altar: {:.1f} prayer power, the site stores {:.1f}",
	                    static_cast<int>(*kind), sacrifice->prayer, worship->battery);

	// the player's creature may see the deed, and learn to sacrifice
	if (sacrifice->deed)
	{
		creature_mimic::Consider(player, creature_watching::Deed::Sacrifice, held);
	}
	if (sacrifice->animalCounts)
	{
		effects::alignment::UpdateForDeath(player, DeathReason::Sacrifice, false, true);
	}
	if (sacrifice->villagerDies)
	{
		villager::VillagerDead(held, DeathReason::Sacrifice, player, life, 1);
	}
	if (sacrifice->light)
	{
		RaiseLight(altar, worship->player);
	}
	// a tree's ghost fades where the hand still draws it
	if (sacrifice->ghost)
	{
		object_ghosts::Add(held);
	}
	if (handPoint.has_value())
	{
		PlaySounds(held, *kind, child, isVillager && villager::IsWoman(held), *handPoint);
	}
	// only for the hand of the player at this machine, which is the one that gives
	if (sacrifice->number && handPoint.has_value())
	{
		FloatPrayerUp(sacrifice->prayer);
	}
	ToBeDeleted(held);
	return Answer::Sacrificed;
}
