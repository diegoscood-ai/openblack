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

#include <limits>

#include <glm/geometric.hpp>

#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerShield.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "Magic/Spells/SpellShield.h"
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

/// The open spell of a magic type, or any shield's, nearest a point of the land (x, z), or none
entt::entity NearestSpell(glm::vec2 point, bool shield, MagicType type)
{
	const auto& registry = Locator::entitiesRegistry::value();
	entt::entity nearest = entt::null;
	float distance = std::numeric_limits<float>::max();
	for (const auto spell : magic::Spells())
	{
		const auto* component = registry.Valid(spell) ? registry.TryGet<const ecs::components::Spell>(spell) : nullptr;
		if (component == nullptr || component->closedDown)
		{
			continue;
		}
		if (shield ? !ecs::villager_shield::IsShieldSpellAvailable(spell) : component->magicType != type)
		{
			continue;
		}
		const float apart = glm::distance(glm::vec2(component->position.x, component->position.z), point);
		if (apart < distance)
		{
			distance = apart;
			nearest = spell;
		}
	}
	return nearest;
}

/// A fireball cast as the player's at a map position, from high above it and with its first strength, as a seed of it
/// in the hand casts it
entt::entity CastStriker(const glm::vec3& mapPosition)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& info = Locator::infoConstants::value();
	constexpr float k_FireSeedMagnitude = 1.0f;
	constexpr float k_NoTimeLimit = -1.0f;
	constexpr float k_HandHeight = 100.0f;
	const float timer = magic::GetTimerWhenPlayerCasting(info, MagicType::Fireball);
	const magic::SpellCastData cast {
	    .magnitude = k_FireSeedMagnitude,
	    .chants = magic::GetMagicEffectInfo(info, MagicType::Fireball).initialChants,
	    .duration = timer < 0.0f ? k_NoTimeLimit : timer,
	    .maxObjectsToCreate = -1,
	};
	const auto point = magic::ToWorld(mapPosition);
	const particles::ProcessInfo process {
	    .handPos = point + glm::vec3(0.0f, k_HandHeight, 0.0f),
	    .cameraForward = glm::vec3(0.0f, -1.0f, 0.0f),
	};
	return TheMagicSystem().CastAtPoint(MagicType::Fireball, PlayerNames::PLAYER_ONE, point, cast, process);
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
	const auto dispenser = worship::dispenser::Create(point, abode, entt::null, 0.0f, 1.0f);
	if (dispenser != entt::null)
	{
		// its miracle, an orb at once, and the period (0 leaves it inactive)
		worship::dispenser::SetMagicAndPeriod(dispenser, magic, periodTurns);
	}
	return dispenser;
}

bool openblack::debug::miracles::BreakShield(entt::entity spell)
{
	if (!Locator::entitiesRegistry::has_value() || !ecs::villager_shield::IsShieldSpellAvailable(spell) ||
	    !Locator::magicSystem::has_value())
	{
		return false;
	}
	magic::spell_shield::SetUpDestroyedReaction(spell);
	Locator::magicSystem::value().CloseDown(spell);
	return true;
}

entt::entity openblack::debug::miracles::NearestShield(glm::vec2 point)
{
	return Locator::entitiesRegistry::has_value() ? NearestSpell(point, true, MagicType::None) : entt::entity {entt::null};
}

std::optional<ShieldStrike> openblack::debug::miracles::StrikeShield(entt::entity spell)
{
	if (!Locator::entitiesRegistry::has_value() || !ecs::villager_shield::IsShieldSpellAvailable(spell) ||
	    !Locator::magicSystem::has_value())
	{
		return std::nullopt;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& shield = registry.Get<const ecs::components::Spell>(spell);
	if (shield.closedDown)
	{
		return std::nullopt;
	}
	// the live fireball nearest the shield, else one made for the blow, which goes once it has struck
	const glm::vec2 middle {shield.position.x, shield.position.z};
	auto striker = NearestSpell(middle, false, MagicType::Fireball);
	const bool made = striker == entt::null;
	if (made)
	{
		striker = CastStriker(shield.castPos);
	}
	if (striker == entt::null || !registry.Valid(striker))
	{
		return std::nullopt;
	}
	const auto* before = registry.TryGet<const magic::SpellShieldData>(spell);
	const bool hadReaction = before != nullptr && before->struckReaction != 0;
	// an empty shield is not struck: the hit does nothing to it
	const bool hadStrength = magic::GetSpellStrength(spell) != 0.0f;
	const bool destroyed = magic::spell_event::SpellHitSpell(striker, spell) && hadStrength;
	if (made)
	{
		magic::DeleteSpell(striker);
	}
	const auto* after = registry.TryGet<const magic::SpellShieldData>(spell);
	return ShieldStrike {
	    .strength = magic::GetSpellStrength(spell),
	    .destroyed = destroyed,
	    .reactionMade = !hadReaction && after != nullptr && after->struckReaction != 0,
	};
}
