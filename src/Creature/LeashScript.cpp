/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashScript.h"

#include <entt/entity/entity.hpp>

#include "Creature/LeashKeys.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Enums.h"
#include "Magic/Script/ScriptPlayer.h"

namespace openblack::creature_leash::script
{

std::optional<CreatureAndThing> Order(std::optional<entt::entity> creature, std::optional<entt::entity> thing,
                                      const IsCreature& isCreature)
{
	if (!creature.has_value() || !thing.has_value())
	{
		return std::nullopt;
	}
	if (!isCreature(*creature) && isCreature(*thing))
	{
		return CreatureAndThing {.creature = *thing, .thing = *creature};
	}
	if (!isCreature(*creature))
	{
		return std::nullopt;
	}
	return CreatureAndThing {.creature = *creature, .thing = *thing};
}

void AttachToThing(LeashSystemInterface& leash, std::optional<entt::entity> creature, std::optional<entt::entity> thing,
                   const IsCreature& isCreature)
{
	if (const auto pair = Order(creature, thing, isCreature))
	{
		leash.TieTo(pair->creature, pair->thing);
	}
}

void AttachToHand(LeashSystemInterface& leash, std::optional<entt::entity> creature)
{
	if (!creature.has_value())
	{
		return;
	}
	// Either way the leash ends up on and held: one held already stays as it is
	if (leash.TiedTo(*creature).has_value())
	{
		leash.UntieToHand(*creature);
	}
	else if (!leash.IsLeashed(*creature))
	{
		leash.Toggle(*creature);
	}
}

void Detach(LeashSystemInterface& leash, std::optional<entt::entity> creature)
{
	if (creature.has_value())
	{
		leash.TakeOff(*creature);
	}
}

bool IsLeashed(const LeashSystemInterface& leash, std::optional<entt::entity> creature)
{
	return creature.has_value() && leash.IsLeashed(*creature);
}

void SetWorks(LeashSystemInterface& leash, std::optional<entt::entity> creature, int32_t value)
{
	if (creature.has_value())
	{
		leash.SetWorks(*creature, value != 0);
	}
}

bool IsLeashedToThing(const LeashSystemInterface& leash, std::optional<entt::entity> creature,
                      std::optional<entt::entity> thing, const IsCreature& isCreature)
{
	const auto pair = Order(creature, thing, isCreature);
	return pair.has_value() && leash.TiedTo(pair->creature) == pair->thing;
}

bool Toggle(LeashSystemInterface& leash, int32_t scriptPlayer)
{
	PlayerNames player = PlayerNames::NEUTRAL;
	if (!magic::ScriptPlayerToGamePlayer(scriptPlayer, player))
	{
		return false;
	}
	return leash.PressKey(player, LeashKey::Leash);
}

} // namespace openblack::creature_leash::script
