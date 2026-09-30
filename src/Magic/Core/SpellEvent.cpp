/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellEvent.h"

#include <spdlog/spdlog.h>

#include "ECS/Components/Poisoned.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "PSys/Rules/Shield.h"
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
	if (event.type == psys::SpellEventInfo::Started || event.type == psys::SpellEventInfo::InitWithoutPSys)
	{
		return 1;
	}
	return ApplyDefaultSpellEffect(spell, event);
}

bool spell_event::GetPaidEffectValues(entt::entity spell, EffectValues& values)
{
	// fn_00720AE0: EffectValues(effect info, applied by the creator, the creator's player)
	auto& component = SpellOf(spell);
	values = EffectValues::FromEffectInfo(EffectInfoOf(spell));
	values.appliedBy = component.creator.entity;
	values.appliedByCreature = component.creator.kind == SpellCreator::Kind::Creature;
	values.hasPlayer = component.hasPlayer;
	values.player = component.player;
	const float strength = chants::PayForOneEvent(component, ChantContextOf(spell));
	if (strength <= 0.0f)
	{
		return false;
	}
	values.Scale(strength);
	return true;
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
			otherOps.setUpDestroyedReaction(other); // vt 0x520 (SpellShield 0x72B7C0)
		}
		return true;
	}
	if (otherOps.updateStruckReaction != nullptr)
	{
		otherOps.updateStruckReaction(other); // vt 0x51C (SpellShield 0x72B780)
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
	// the spell moves to the event (ftol x, z; altitude 0)
	component.position = glm::vec3(event.position.x, 0.0f, event.position.z);
	EffectValues values;
	if (!GetPaidEffectValues(spell, values))
	{
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} event {} not paid (strength 0)",
			                   static_cast<uint32_t>(spell), event.type);
		}
		return 0;
	}
	values.Scale(GetTribalPower(spell) * event.strength);
	entt::entity hit = entt::null;
	if (event.type == psys::SpellEventInfo::HitSpell)
	{
		if (event.target != entt::null && registry.Valid(event.target) && registry.AllOf<Spell>(event.target) &&
		    !SpellHitSpell(spell, event.target))
		{
			return 0;
		}
	}
	else
	{
		// event +0x20 (0x720D19): a DefensiveSphere holding the point, with the effect's radius as the margin
		// (fn_006D0BC0), gets a SpellEvent 4 at the point sent to this spell with itself as the target (0x720D84; the
		// intent is UNVERIFIED, it is what the code does); 0 stops the event, else the shield sparks there (fn_006D0AF0)
		if (event.checkShields)
		{
			if (const auto* sphere = psys::shields::FindShieldContainingPoint(event.position, values.radius); sphere != nullptr)
			{
				psys::SpellEventInfo hitShield;
				hitShield.type = psys::SpellEventInfo::HitSpell;
				hitShield.position = event.position;
				hitShield.velocity = glm::vec3(0.0f);
				hitShield.strength = 1.0f;
				hitShield.checkShields = false;
				hitShield.target = spell;
				if (OpsOf(component.spellClass).spellEvent(spell, hitShield) == 0)
				{
					return 0;
				}
				psys::shields::AddImpactTarget(*sphere, event.position);
			}
		}
		if (event.type == psys::SpellEventInfo::CanDestroy)
		{
			if (event.target != entt::null)
			{
				// vt 0x778 CanBeDestroyedBySpell (Object 0x639960) == 1, with no reaction and no direction. TODO(M5/M6):
				// the per-class answers; 1 (inf)
				return 1;
			}
			// no target (0x720DB4): nothing applied, straight to the reaction and the direction below, and 1
		}
		else if (event.type == psys::SpellEventInfo::Object && event.target != entt::null)
		{
			if (registry.Valid(event.target) && ecs::effects::IsEffectReceiver(event.target, values))
			{
				const float life0 = ecs::life::LifeOf(event.target);
				ecs::effects::ApplyEffect(event.target, values);
				// Heal / HealPU cures poison: Living::IsPoisoned 0x416F90 (vt 0x4A4) -> SetPoisoned(0) 0x416FA0 (vt 0x69C)
				const bool cured = (component.magicType == MagicType::Heal || component.magicType == MagicType::HealPowerUpOne) &&
				                   registry.AllOf<Poisoned>(event.target);
				if (cured)
				{
					registry.Remove<Poisoned>(event.target);
				}
				if (TraceEnabled())
				{
					SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} event 5 on entity {}: life {:.3f} -> {:.3f}{}",
					                   static_cast<uint32_t>(spell), static_cast<uint32_t>(event.target), life0,
					                   ecs::life::LifeOf(event.target), cured ? ", poison cured" : "");
				}
				hit = event.target;
			}
		}
		else
		{
			hit = values.ApplyEffectToMapPos(component.position);
		}
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Spell trace: spell {} event {} at ({:.1f}, {:.1f}) burn {:.4f} crush {:.4f} hit {:.4f} heal "
			                   "{:.4f} radius {:.1f} -> hit entity {}",
			                   static_cast<uint32_t>(spell), event.type, event.position.x, event.position.z,
			                   values.numbers[EffectValues::Burn], values.numbers[EffectValues::Crush],
			                   values.numbers[EffectValues::Hit], values.numbers[EffectValues::Heal], values.radius,
			                   hit == entt::null ? -1 : static_cast<int>(static_cast<uint32_t>(hit)));
		}
		// hit && player && !IsCreatureCasting -> GPlayer::ConsiderMakingCreatureMimicPlayer 0x4EA900 with
		// fn_004E9DF0's action. TODO(M8): the creature's learning
	}
	const auto& effect = EffectInfoOf(spell);
	if (effect.createReactionOnEvent != 0 && component.reaction == 0 && effect.reactionType != Reaction::None)
	{
		component.reaction = ecs::effects::reactions::CreateReaction(spell, effect.reactionType, component.player, false);
	}
	component.movementDirection = event.velocity;
	return 1;
}
