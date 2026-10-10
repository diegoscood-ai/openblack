/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiraclesCaster.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Systems/MagicSystemInterface.h"
#include "Locator.h"
#include "Worship/SpellDispenser.h"

using namespace openblack;
using namespace openblack::debug::miracles;

namespace
{
/// The miracles (Locator::magicSystem)
ecs::systems::MagicSystemInterface& TheMagicSystem()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("miracles window: no miracles service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value();
}
} // namespace

entt::entity GameSpellCaster::CastAtPoint(const CastPlan& plan, PlayerNames player, glm::vec3 point)
{
	return TheMagicSystem().CastAtPoint(plan.type, player, point, plan.cast, plan.process);
}

entt::entity GameSpellCaster::CastOnObject(const CastPlan& plan, PlayerNames player, entt::entity target)
{
	return TheMagicSystem().CastOnObject(plan.type, player, target, plan.cast, plan.process);
}

bool GameSpellCaster::CanCastAt(MagicType type, PlayerNames player, glm::vec3 point) const
{
	return TheMagicSystem().CanCastAt(type, player, point);
}

entt::entity GameDispenserCreator::CreateOneShot(SpellSeedType seed, int powerUpLevel, glm::vec3 point)
{
	return TheMagicSystem().CreateOneOffSeed(point, seed, powerUpLevel, 1.0f);
}

entt::entity GameDispenserCreator::CreateOneShotInHand(SpellSeedType seed, int powerUpLevel, PlayerNames player)
{
	return TheMagicSystem().GiveSeedToHand(player, seed, powerUpLevel, 1.0f);
}

entt::entity GameDispenserCreator::CreatePermanent(AbodeInfo abode, MagicType magic, uint32_t periodTurns, glm::vec3 point)
{
	constexpr int k_NearestTown = -1;
	const auto dispenser = worship::dispenser::Create(point, abode, k_NearestTown, 0.0f, 1.0f);
	if (dispenser != entt::null)
	{
		// its miracle, an orb at once, and the period (0 leaves it inactive)
		worship::dispenser::SetMagicAndPeriod(dispenser, magic, periodTurns);
	}
	return dispenser;
}
