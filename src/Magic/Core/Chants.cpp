/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Chants.h"

#include <chrono>

#include "InfoConstants.h"
#include "Magic/SpellChants.h"

using namespace openblack;
using namespace openblack::magic;
using openblack::ecs::components::Spell;

// The chant rules' default turn length is the game's turn length
static_assert(k_ChantTurnDuration.count() == game_clock::k_MsPerTurn);

namespace
{
/// The chant rules a context describes. A context with no way to maintain the spell never asks for a refill.
SpellChantRules RulesOfContext(const chants::Context& context)
{
	const bool hasEffect = context.effect != nullptr;
	return {
	    .costPerEvent = hasEffect ? context.effect->costPerEvent : 0.0f,
	    .costToMaintain = context.costToMaintain,
	    .maintained = context.maintained,
	    .recharged = context.recharged && static_cast<bool>(context.maintain),
	    .divideCostsByTribalPower = hasEffect && context.effect->divideCostsByTribalPower == 1,
	    .tribalPower = context.tribalPower,
	    .seedPower = context.seedPower,
	    .turnDuration = std::chrono::milliseconds(context.turnMs),
	};
}

/// The spell's creator as the chant rules see it: the context's maintain and spell point callbacks
class ContextCaster final: public SpellCasterInterface
{
public:
	explicit ContextCaster(const chants::Context& context)
	    : _context(context)
	{
	}

	// Only reached when the rules say the spell is recharged, which needs a maintain callback
	float MaintainSpell(float amount) override { return _context.maintain(amount); }

	void OnChantsSpent(float chants, ChantCharge charge) override
	{
		if (_context.createSpellPoint)
		{
			_context.createSpellPoint(chants, charge == ChantCharge::PerTurn);
		}
	}

private:
	const chants::Context& _context;
};

/// The prayer power fields of the spell component
SpellChants ChantsOfSpell(const Spell& spell)
{
	return {
	    .chants = spell.chants,
	    .initialChants = spell.initialChants,
	    .strengthMultiplier = spell.strengthMultiplier,
	    .free = spell.free,
	};
}
} // namespace

void chants::SetChants(Spell& spell, float chants)
{
	auto state = ChantsOfSpell(spell);
	magic::SetChants(state, chants);
	spell.chants = state.chants;
	spell.initialChants = state.initialChants;
}

float chants::GetChantSafetyLevel(const Spell& spell, const Context& context)
{
	return magic::GetChantSafetyLevel(ChantsOfSpell(spell), RulesOfContext(context));
}

float chants::GetSpellStrength(const Spell& spell, const Context& context)
{
	const ContextCaster caster(context);
	return magic::GetSpellStrength(ChantsOfSpell(spell), RulesOfContext(context), context.hasCreator ? &caster : nullptr);
}

float chants::PayFor(Spell& spell, const Context& context, float cost, bool force)
{
	ContextCaster caster(context);
	auto state = ChantsOfSpell(spell);
	const float strength = magic::PayFor(state, RulesOfContext(context), context.hasCreator ? &caster : nullptr, cost,
	                                     force ? Refill::Whole : Refill::UpToCost);
	spell.chants = state.chants;
	return strength;
}

float chants::PayForOneTurn(Spell& spell, const Context& context)
{
	// A paid turn reports its spell point even when the spell has no creator; the chant rules only report it through
	// a creator, so that case is reported here
	if (!context.hasCreator && context.costToMaintain != 0.0f && context.createSpellPoint)
	{
		context.createSpellPoint(context.costToMaintain, true);
	}
	ContextCaster caster(context);
	auto state = ChantsOfSpell(spell);
	const float strength = magic::PayForOneTurn(state, RulesOfContext(context), context.hasCreator ? &caster : nullptr);
	spell.chants = state.chants;
	return strength;
}

float chants::PayForOneEvent(Spell& spell, const Context& context)
{
	ContextCaster caster(context);
	auto state = ChantsOfSpell(spell);
	const float strength = magic::PayForOneEvent(state, RulesOfContext(context), context.hasCreator ? &caster : nullptr);
	spell.chants = state.chants;
	return strength;
}

float chants::Recharge(Spell& spell, const Context& context)
{
	ContextCaster caster(context);
	auto state = ChantsOfSpell(spell);
	const float given = magic::Recharge(state, RulesOfContext(context), caster);
	spell.chants = state.chants;
	return given;
}
