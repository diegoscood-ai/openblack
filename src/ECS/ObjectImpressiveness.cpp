/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectImpressiveness.h"

#include <cstddef>

#include <utility>

#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "Locator.h"

namespace
{
/// The land balance an object's impressiveness is multiplied by
constexpr size_t k_ImpressivenessBalance = 1;
} // namespace

float openblack::ecs::object::ImpressiveValue(entt::entity object)
{
	// a town desire flag's info is the prayer site's
	if (Locator::entitiesRegistry::has_value() && Locator::infoConstants::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		if (registry.Valid(object) && registry.AllOf<components::TownDesireFlag>(object))
		{
			return land_balance::Get(k_ImpressivenessBalance) * Locator::infoConstants::value().prayerSite.impressiveValue;
		}
	}
	const auto* info = fire::traits::InfoOf(object);
	return info != nullptr ? land_balance::Get(k_ImpressivenessBalance) * info->impressiveValue : 0.0f;
}
