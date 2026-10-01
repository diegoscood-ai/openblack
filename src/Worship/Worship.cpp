/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Worship.h"

#include <vector>

#include "Citadel.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "FireFlyReward.h"
#include "GestureIconProvider.h"
#include "InterfaceStatus.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/SpellSeed.h"
#include "PlayerSpellIcons.h"
#include "SpellDispenser.h"
#include "SpellSeedGraphic.h"
#include "WorshipPercentage.h"
#include "WorshipSite.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
bool g_PostLoadDone = false;

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

void worship::OnLoadMap()
{
	g_PostLoadDone = false;
	player::Reset();
	fire_fly::Reset(); // FireFly::OnClearMap 0x52A1E0
	gesture_icons::Register();
	ResetDebugHooks();
}

void worship::ProcessSpellIcons()
{
	seed_graphic::ProcessTurn(); // fn_00727350
	player::ProcessSpellIcons(); // GPlayer::ProcessSpellIcons 0x64AEE0
}

void worship::ProcessTurn(uint32_t turn)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	if (!g_PostLoadDone)
	{
		// GSetup::LoadMapFeatures 0x7180B0 -> GPlayer::PostLoadCleanup 0x64AB90, once the land's script has run
		g_PostLoadDone = true;
		citadel::PostLoadCleanup();
	}
	RunDebugHooks(turn);
	dispenser::ProcessTurn();
}

void worship::Update(float seconds)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const float milliseconds = seconds * 1000.0f;
	seed_graphic::UpdatePhase(milliseconds);
	auto& registry = Registry();
	std::vector<entt::entity> icons;
	registry.Each<const WorshipSpellIcon>([&](entt::entity icon, const WorshipSpellIcon&) { icons.push_back(icon); });
	for (const auto icon : icons)
	{
		icon::UpdateChargingVisual(icon, seed_graphic::Phase());
	}
	std::vector<entt::entity> sites;
	registry.Each<const WorshipSite>([&](entt::entity site, const WorshipSite&) { sites.push_back(site); });
	for (const auto site : sites)
	{
		site::UpdateStrainVisual(site, milliseconds); // fn_00463980 -> fn_0077B3B0
	}
	percentage::UpdateTotems(seconds);
}

bool worship::InterfaceValidToTap(entt::entity object, PlayerNames player)
{
	auto& registry = Registry();
	if (object == entt::null || !registry.Valid(object))
	{
		return false;
	}
	if (registry.AllOf<SpellIcon>(object))
	{
		return icon::InterfaceValidToTap(object, player);
	}
	return registry.AllOf<OneOffSpellSeed>(object); // OneOffSpellSeed::InterfaceValidToTap 0x72A630 = 1
}

int worship::InterfaceTap(entt::entity object, PlayerNames player)
{
	if (!InterfaceValidToTap(object, player))
	{
		return 0;
	}
	if (Registry().AllOf<SpellIcon>(object))
	{
		return icon::InterfaceTap(object, player);
	}
	return magic::one_off::InterfaceTap(object, player);
}

void worship::OnPlacedInMagicHand(entt::entity object)
{
	fire_fly::OnPlacedInMagicHand(object);
}

bool worship::IsSeedReturnPoint(entt::entity object, PlayerNames player)
{
	auto& registry = Registry();
	if (object == entt::null || !registry.Valid(object))
	{
		return false;
	}
	if (dispenser::IsDispenser(object))
	{
		return true; // SpellDispenser::IsSpellSeedReturnPoint (no player check: fn_00728C50 takes any seed)
	}
	if (const auto* totem = registry.TryGet<const WorshipTotem>(object); totem != nullptr && totem->site != entt::null)
	{
		return registry.Get<const WorshipSite>(totem->site).player == player;
	}
	if (const auto* spellIcon = registry.TryGet<const SpellIcon>(object); spellIcon != nullptr)
	{
		return spellIcon->player == player;
	}
	return false;
}

int worship::ApplySeedToObject(entt::entity seedEntity, entt::entity object)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity) || object == entt::null ||
	    !registry.Valid(object))
	{
		return 0;
	}
	if (dispenser::IsDispenser(object))
	{
		return dispenser::ApplySeed(object, seedEntity) ? 3 : 0; // fn_00728C50 / fn_00728C80
	}
	// SpellSeed::ApplyToWorshipSite 0x728B30: a WorshipTotem -> its site; a SpellIcon -> its site's battery, the seed
	// goes, and an icon of another seed of the same player gives its fully charged seed instead (the swap)
	if (const auto* totem = registry.TryGet<const WorshipTotem>(object); totem != nullptr && totem->site != entt::null)
	{
		auto& seed = registry.Get<SpellSeed>(seedEntity);
		registry.Get<WorshipSite>(totem->site).battery += seed.chantStore; // SpellSeed::ApplyToWorshipSite 0x7289C0
		magic::seed::SetChantStore(seed, 0.0f);
		magic::seed::ToBeDeleted(seedEntity);
		return 3;
	}
	const auto* spellIcon = registry.TryGet<const SpellIcon>(object);
	if (spellIcon == nullptr)
	{
		return 0;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	const bool otherSeed = spellIcon->seedType != seed.seedType;
	const auto player = seed.creator.player;
	// vt 0x914 GetWorshipSpellIcon of the icon given to, and vt 0x30C its site
	const auto target = icon::WorshipIconOf(object);
	const entt::entity site = target != entt::null ? registry.Get<const WorshipSpellIcon>(target).site : entt::null;
	if (site != entt::null && registry.Valid(site))
	{
		registry.Get<WorshipSite>(site).battery += seed.chantStore;
	}
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	if (otherSeed && spellIcon->player == player && target != entt::null &&
	    icon::ValidForPutFullyChargedSeedInHand(target, player))
	{
		icon::PutFullyChargedSeedInHand(target, player);
	}
	return 3;
}

int worship::ReturnSeedToItsSite(entt::entity seedEntity)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return 0;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		const auto site = registry.Get<const WorshipSpellIcon>(seed.icon).site;
		if (site != entt::null && registry.Valid(site))
		{
			registry.Get<WorshipSite>(site).battery += seed.chantStore; // 0x7289C0
			magic::seed::SetChantStore(seed, 0.0f);
		}
	}
	magic::seed::ToBeDeleted(seedEntity);
	return 3;
}

void worship::OnSeedOutOfHand(entt::entity seedEntity, PlayerNames player)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return;
	}
	const auto& seed = registry.Get<const SpellSeed>(seedEntity);
	player::SetLastSeedType(player, seed.seedType);
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		icon::CancelCharge(seed.icon, player);
	}
}

int worship::ApplySeedToPosition(entt::entity seedEntity, const glm::vec3& position)
{
	// SpellSeed::RemoveFromHand 0x728F00: MapCoords::FindWorshipSite -> ApplyToWorshipSite 0x7289C0 (3), else 0x17
	const auto site = site::FindAt(position);
	auto& registry = Registry();
	if (site == entt::null)
	{
		return 0x17; // 0x728F1C
	}
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return 0; // openblack's guard
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	registry.Get<WorshipSite>(site).battery += seed.chantStore;
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	return 3;
}
