/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellEvent.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Poisoned.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Particles/Rules/Shield.h"
#include "Spell.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;
using openblack::ecs::effects::EffectValues;

namespace
{
Spell& SpellOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().Get<Spell>(spell);
}
} // namespace

int spell_event::SpellEvent(entt::entity spell, const psys::SpellEventInfo& event)
{
	if (event.type == psys::SpellEventInfo::Type::Started || event.type == psys::SpellEventInfo::Type::InitWithoutPSys)
	{
		return 1;
	}
	return ApplyDefaultSpellEffect(spell, event);
}

bool spell_event::GetPaidEffectValues(entt::entity spell, EffectValues& values)
{
	// EffectValues(effect info, applied by the creator, the creator's player)
	auto& component = SpellOf(spell);
	values = EffectValues::FromEffectInfo(EffectInfoOf(spell));
	values.appliedBy = component.creator.entity;
	values.appliedByCreature = component.creator.kind == SpellCreator::Kind::Creature;
	values.appliedByPlayer = component.creator.kind == SpellCreator::Kind::Player;
	values.player = component.hasPlayer ? std::optional(component.player) : std::nullopt;
	const float strength = chants::PayForOneEvent(component, ChantContextOf(spell));
	if (strength <= 0.0f)
	{
		return false;
	}
	values.Scale(strength);
	return true;
}

EffectSource spell_event::SourceOf(entt::entity spell)
{
	// the spell's player (the neutral one without a player, and then no player at all), and the creature that cast it
	const auto& component = SpellOf(spell);
	return {.player = component.player,
	        .casterCreature = component.creator.kind == SpellCreator::Kind::Creature ? component.creator.entity : entt::null,
	        .playerless = !component.hasPlayer};
}

bool spell_event::SpellHitSpell(entt::entity spell, entt::entity other)
{
	const float cost = GetSpellStrength(spell) * EffectInfoOf(spell).costPerShieldCollide;
	if (GetSpellStrength(other) == 0.0f)
	{
		return true;
	}
	chants::PayFor(SpellOf(other), ChantContextOf(other), cost, true);
	chants::PayForOneEvent(SpellOf(spell), ChantContextOf(spell));
	const float otherStrength = GetSpellStrength(other);
	const float strength = GetSpellStrength(spell);
	const auto& otherOps = OpsOf(SpellOf(other).spellClass);
	if (otherStrength <= 0.0f && strength > 0.0f)
	{
		if (otherOps.setUpDestroyedReaction != nullptr)
		{
			otherOps.setUpDestroyedReaction(other);
		}
		return true;
	}
	if (otherOps.updateStruckReaction != nullptr)
	{
		otherOps.updateStruckReaction(other);
	}
	return false;
}

int spell_event::ApplyDefaultSpellEffect(entt::entity spell, const psys::SpellEventInfo& event)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& component = SpellOf(spell);
	if (component.closedDown)
	{
		return 0;
	}
	// the spell moves to the event (x, z truncated; altitude 0)
	component.position = glm::vec3(event.position.x, 0.0f, event.position.z);
	EffectValues values;
	if (!GetPaidEffectValues(spell, values))
	{
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} event {} not paid (strength 0)",
			                   static_cast<uint32_t>(spell), static_cast<int>(event.type));
		}
		return 0;
	}
	values.Scale(GetTribalPower(spell) * event.strength);
	// who applies it is in the values; the source says it as the world's hooks read it
	auto& world = TheWorld();
	const auto source = SourceOf(spell);
	entt::entity hit = entt::null;
	if (event.type == psys::SpellEventInfo::Type::HitSpell)
	{
		if (event.target != entt::null && registry.Valid(event.target) && registry.AllOf<Spell>(event.target) &&
		    !SpellHitSpell(spell, event.target))
		{
			return 0;
		}
	}
	else
	{
		// when the event checks shields: a defensive sphere holding the point, with the effect's radius as the margin,
		// gets a spell event 4 at the point sent to this spell with itself as the target (the intent is UNVERIFIED, it
		// is what the code does); 0 stops the event, else the shield sparks there
		if (event.checkShields)
		{
			if (const auto* sphere = psys::shields::FindShieldContainingPoint(event.position, values.radius); sphere != nullptr)
			{
				const psys::SpellEventInfo hitShield {.type = psys::SpellEventInfo::Type::HitSpell,
				                                      .position = event.position,
				                                      .velocity = glm::vec3(0.0f),
				                                      .strength = 1.0f,
				                                      .checkShields = false,
				                                      .target = spell};
				if (OpsOf(component.spellClass).spellEvent(spell, hitShield) == 0)
				{
					return 0;
				}
				psys::shields::AddImpactTarget(*sphere, event.position);
			}
		}
		if (event.type == psys::SpellEventInfo::Type::CanDestroy)
		{
			if (event.target != entt::null)
			{
				// whether a miracle may destroy it, with no reaction and no direction
				return world.CanBeDestroyedBySpell(event.target) ? 1 : 0;
			}
			// no target: nothing applied, straight to the reaction and the direction below, and 1
		}
		else if (event.type == psys::SpellEventInfo::Type::Object && event.target != entt::null)
		{
			if (ecs::IsAvailable(event.target) && ecs::effects::IsEffectReceiver(event.target, values))
			{
				const float life0 = ecs::life::LifeOf(event.target);
				world.ApplyEffect(event.target, values, source);
				// Heal / HealPU cures poison
				const bool cured =
				    (component.magicType == MagicType::Heal || component.magicType == MagicType::HealPowerUpOne) &&
				    registry.AllOf<Poisoned>(event.target);
				if (cured)
				{
					world.CurePoison(event.target);
				}
				if (TraceEnabled())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"),
					                   "Spell trace: spell {} event 5 on entity {}: life {:.3f} -> {:.3f}{}",
					                   static_cast<uint32_t>(spell), static_cast<uint32_t>(event.target), life0,
					                   ecs::life::LifeOf(event.target), cured ? ", poison cured" : "");
				}
				hit = event.target;
			}
		}
		else
		{
			// everything about the event's point on the land takes it; the last one reached is the hit
			const glm::vec3 point(component.position.x, world.LandHeight(glm::vec2(component.position.x, component.position.z)),
			                      component.position.z);
			const auto reached = world.ApplyEffectAt(point, values, source);
			hit = reached.empty() ? entt::null : reached.back();
		}
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Spell trace: spell {} event {} at ({:.1f}, {:.1f}) burn {:.4f} crush {:.4f} hit {:.4f} heal "
			                   "{:.4f} radius {:.1f} -> hit entity {}",
			                   static_cast<uint32_t>(spell), static_cast<int>(event.type), event.position.x, event.position.z,
			                   values.numbers[static_cast<size_t>(EffectValues::Number::Burn)],
			                   values.numbers[static_cast<size_t>(EffectValues::Number::Crush)],
			                   values.numbers[static_cast<size_t>(EffectValues::Number::Hit)],
			                   values.numbers[static_cast<size_t>(EffectValues::Number::Heal)], values.radius,
			                   hit == entt::null ? -1 : static_cast<int>(static_cast<uint32_t>(hit)));
		}
		// what a player's miracle (not a creature's) did to the last thing it reached, the player's creature may copy
		if (hit != entt::null && component.hasPlayer && component.creator.kind != SpellCreator::Kind::Creature)
		{
			world.PlayerAffected(hit, component.magicType, source.player);
		}
	}
	const auto& effect = EffectInfoOf(spell);
	if (effect.createReactionOnEvent != 0 && component.reaction == 0 && effect.reactionType != Reaction::None)
	{
		component.reaction = ecs::effects::reactions::CreateReaction(spell, effect.reactionType, component.player, false);
	}
	component.movementDirection = event.velocity;
	return 1;
}
