/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellCreator.h"

#include <utility>

#include "3D/CreatureBody.h"
#include "Camera/Camera.h"
#include "Creature/CreatureSpellCasting.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Players.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using Kind = openblack::ecs::components::SpellCreator::Kind;

namespace
{
/// The creature's body pays what the spell asks (creature_spell_casting::MaintainSpell), its
/// energy by the share its species' magic action table gives the spell's magic type. A creature with no body pays
/// nothing (openblack: the original's always has one)
float CreaturePays(entt::entity creature, entt::entity spell, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	if (!lookup.Valid(creature) || !lookup.AllOf<ecs::components::Creature, ecs::components::CreatureNeeds>(creature) ||
	    !lookup.Valid(spell) || !lookup.AllOf<ecs::components::Spell>(spell) || !Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& body = lookup.Get<const ecs::components::Creature>(creature);
	const auto row = creature::InfoRow(body.species);
	const auto magicType = static_cast<size_t>(lookup.Get<const ecs::components::Spell>(spell).magicType);
	if (row >= info.creature.size() || magicType >= info.creatureMagicActionKnownAboutEntry.size())
	{
		return 0.0f;
	}
	const auto& species = info.creature.at(row);
	auto& needs = registry.Get<ecs::components::CreatureNeeds>(creature).needs;
	creature_spell_casting::Body caster {
	    .size = body.size,
	    .strength = body.strength,
	    .energy = needs.energy,
	    .exhaustion = needs.exhaustion,
	};
	const creature_spell_casting::Rates rates {
	    .chantsPerEnergy = species.chantsPerEnergy,
	    .energyFloor = species.spellEnergyFloor,
	    .sizeFactor = species.spellSizeFactor,
	};
	const float share = info.creatureMagicActionKnownAboutEntry.at(magicType).field0x5c;
	const float paid = creature_spell_casting::MaintainSpell(caster, rates, amount, share);
	needs.energy = caster.energy;
	needs.exhaustion = caster.exhaustion;
	return paid;
}
} // namespace

creator::SpellCreator creator::NeutralPlayer()
{
	return {Kind::Player, PlayerNames::NEUTRAL, entt::null};
}

creator::SpellCreator creator::OfPlayer(PlayerNames player)
{
	return {Kind::Player, player, entt::null};
}

float creator::MaintainSpell(const SpellCreator& creator, entt::entity spell, float amount)
{
	switch (creator.kind)
	{
	case Kind::Player:
		// the neutral player ? amount : 0
		return creator.player == PlayerNames::NEUTRAL ? amount : 0.0f;
	case Kind::Thing:
		return amount;
	case Kind::WorshipSpellIcon:
		// the icon's worship site pays with its chants
		if (creator.entity != entt::null && Locator::entitiesRegistry::value().Valid(creator.entity) &&
		    Locator::entitiesRegistry::value().AllOf<ecs::components::WorshipSpellIcon>(creator.entity))
		{
			return worship::icon::MaintainSpell(creator.entity, amount);
		}
		return 0.0f;
	case Kind::Creature:
		// the creature's body pays with its energy and tires
		return CreaturePays(creator.entity, spell, amount);
	case Kind::None:
		break;
	}
	return 0.0f;
}

void creator::UpdateSpellInfo(const SpellCreator& creator, entt::entity spell, psys::ProcessInfo& info)
{
	// a player: not the neutral one; only a spell cast from the interface (a hand cast) takes the hand's info
	// a worship icon: its player's, when that player is human
	// TODO: a creature's (Kind::Creature returns here)
	const bool icon = creator.kind == Kind::WorshipSpellIcon && players::IsHuman(creator.player);
	if ((creator.kind != Kind::Player && !icon) || creator.player == PlayerNames::NEUTRAL)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* component = registry.TryGet<const ecs::components::Spell>(spell);
	if (component == nullptr || !component->castFromInterface || !Locator::handSystem::has_value())
	{
		return;
	}
	// the interface's position, the hand, the camera's forward and the hand's velocity
	Locator::handSystem::value().GetSpellInfo(info.interfacePos, info.handPos, info.cameraForward, info.direction);
}

bool creator::IsFunctional(const SpellCreator& creator)
{
	switch (creator.kind)
	{
	case Kind::None:
		return false;
	case Kind::Player:
		return true;
	default:
		// IsAvailable of the object: a creator with no object is not functional
		return creator.entity != entt::null && ecs::IsAvailable(creator.entity);
	}
}

bool creator::IsCreature(const SpellCreator& creator)
{
	return creator.kind == Kind::Creature;
}

bool creator::IsHumanPlayerCasting(const SpellCreator& creator)
{
	if (creator.kind != Kind::Player && creator.kind != Kind::WorshipSpellIcon)
	{
		return false;
	}
	return players::IsHuman(creator.player);
}
