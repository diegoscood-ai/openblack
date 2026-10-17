/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellCreator.h"

#include <optional>
#include <utility>

#include "3D/CreatureBody.h"
#include "Camera/Camera.h"
#include "Creature/CreatureHands.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureSpellCasting.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Players.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::magic;
using Kind = openblack::ecs::components::SpellCreator::Kind;

namespace
{
/// A creature's body as it pays for miracles, at the size it is drawn at (the game's user size), and its species'
/// casting values; none for a creature with no body (openblack: the original's always has one)
std::optional<std::pair<creature_spell_casting::Body, creature_spell_casting::Rates>> CasterBodyOf(entt::entity creature)
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(creature) || !lookup.AllOf<ecs::components::Creature, ecs::components::CreatureNeeds>(creature) ||
	    !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& body = lookup.Get<const ecs::components::Creature>(creature);
	const auto row = creature::InfoRow(body.species);
	if (row >= info.creature.size())
	{
		return std::nullopt;
	}
	const auto& species = info.creature.at(row);
	const auto& needs = lookup.Get<const ecs::components::CreatureNeeds>(creature).needs;
	return std::pair {creature_spell_casting::Body {.size = ecs::object::GetScale(creature),
	                                                .strength = body.strength,
	                                                .energy = needs.energy,
	                                                .exhaustion = needs.exhaustion},
	                  creature_spell_casting::Rates {.chantsPerEnergy = species.chantsPerEnergy,
	                                                 .energyFloor = species.spellEnergyFloor,
	                                                 .sizeFactor = species.spellSizeFactor}};
}

/// The creature's body pays what the spell asks (creature_spell_casting::MaintainSpell), its
/// energy by the share its species' magic action table gives the spell's magic type. A creature with no body pays
/// nothing (openblack: the original's always has one)
float CreaturePays(entt::entity creature, entt::entity spell, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	auto caster = CasterBodyOf(creature);
	if (!caster.has_value() || !lookup.Valid(spell) || !lookup.AllOf<ecs::components::Spell>(spell))
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	const auto magicType = static_cast<size_t>(lookup.Get<const ecs::components::Spell>(spell).magicType);
	if (magicType >= info.creatureMagicActionKnownAboutEntry.size())
	{
		return 0.0f;
	}
	auto& [body, rates] = *caster;
	const float share = info.creatureMagicActionKnownAboutEntry.at(magicType).field0x5c;
	const float paid = creature_spell_casting::MaintainSpell(body, rates, amount, share);
	auto& needs = registry.Get<ecs::components::CreatureNeeds>(creature).needs;
	needs.energy = body.energy;
	needs.exhaustion = body.exhaustion;
	return paid;
}

/// A creature's hands and the way from them to the info's point (the target): from its body as posed for the turn,
/// unless the spell is cast from above, where the point it is cast from stays and the way is from there down to the
/// target. Before its body is posed its hands stay where they were
void CreatureSpellInfo(entt::entity creature, entt::entity spell, psys::ProcessInfo& info)
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	if (!lookup.Valid(creature) || !lookup.AllOf<ecs::components::Creature>(creature) || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto* component = lookup.Valid(spell) ? lookup.TryGet<const ecs::components::Spell>(spell) : nullptr;
	const bool fromAbove =
	    component != nullptr && IsCreatureCastFromAbove(Locator::infoConstants::value(), component->magicType);
	const auto rigId = creature::GetRigId(lookup.Get<const ecs::components::Creature>(creature).species);
	if (!fromAbove && Locator::resources::has_value() && Locator::resources::value().GetCreatureRigs().Contains(rigId))
	{
		const auto& rigs = Locator::resources::value().GetCreatureRigs();
		if (const auto hands = creature_hands::HandsOf(lookup, creature, *rigs.Handle(rigId)))
		{
			info.handPos = *hands;
		}
	}
	info.cameraForward = creature_hands::Towards(info.interfacePos, info.handPos);
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

creator::SpellCreator creator::OfCreature(entt::entity creature)
{
	const auto& lookup = std::as_const(Locator::entitiesRegistry::value());
	const auto* body = lookup.Valid(creature) ? lookup.TryGet<const ecs::components::Creature>(creature) : nullptr;
	return {Kind::Creature, body != nullptr ? body->owner : PlayerNames::NEUTRAL, creature};
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

bool creator::CreatureCanPayFor(entt::entity creature, float costToCreate)
{
	const auto caster = CasterBodyOf(creature);
	return caster.has_value() && creature_spell_casting::CanCast(caster->first, caster->second, costToCreate);
}

void creator::UpdateSpellInfo(const SpellCreator& creator, entt::entity spell, psys::ProcessInfo& info)
{
	// a creature: its hands and the way from them
	if (creator.kind == Kind::Creature)
	{
		CreatureSpellInfo(creator.entity, spell, info);
		return;
	}
	// a player: not the neutral one; only a spell cast from the interface (a hand cast) takes the hand's info
	// a worship icon: its player's, when that player is human
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
