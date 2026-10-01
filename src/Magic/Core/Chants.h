/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>

#include "ECS/Components/Spell.h"

namespace openblack
{
struct GMagicEffectInfo;
} // namespace openblack

// The prayer power ("chants") economy of a spell: its safety level, strength, upkeep and refill (Spell 0x720750 ..
// 0x720A90). Pure functions of the spell's fields and a context, so the unit test can drive them without a world.

namespace openblack::magic::chants
{

/// What the chant functions read besides the Spell's own fields (Spell.cpp builds it from the ECS)
struct Context
{
	const GMagicEffectInfo* effect {nullptr};
	bool maintained {false};       ///< GMagicInfo::IsMaintainedSpell 0x5FB810
	bool recharged {false};        ///< GMagicInfo.isSpellRecharged
	bool hasCreator {false};       ///< Spell +0xA0 != NULL
	float tribalPower {1.0f};      ///< Spell::GetTribalPower 0x7216F0
	float seedPower {1.0f};        ///< the seed's +0x8C, 1 without a seed
	float costToMaintain {0.0f};   ///< CalculateCostToMaintain (vt 0x53C) of this spell now
	unsigned int turnMs {100};     ///< *(u32*)0xD01A38
	/// creator->MaintainSpell(spell, amount) (GameThing vt 0x58): the chants the creator gives
	std::function<float(float amount)> maintain;
	/// CreateSpellPoint(chants, perTurn) 0x7213D0: the mana path sprites (worship-site creators only)
	std::function<void(float chants, bool perTurn)> createSpellPoint;
};

/// SetChants 0x720FA0: +0x38 = +0x3C = chants
void SetChants(ecs::components::Spell& spell, float chants);

/// Spell::GetChantSafetyLevel 0x720880: maintained spells -> initialChants; otherwise five seconds of upkeep
/// (cost x (1000 / turn ms) x 5), at most initialChants and at least one event (costPerEvent)
[[nodiscard]] float GetChantSafetyLevel(const ecs::components::Spell& spell, const Context& context);

/// Spell::GetSpellStrength 0x720750: chants / safety clamped to 0..1 (1 / 0 for a safety <= 0), x tribal power x the
/// seed's power x strengthMultiplier; 0 without a creator
[[nodiscard]] float GetSpellStrength(const ecs::components::Spell& spell, const Context& context);

/// Spell::PayFor 0x720990: takes the cost (divided by max(tribal power, 1) if divideCostsByTribalPower), then refills
/// the deficit under the safety level from the creator (all of it when forced, else at most the cost) if the spell is
/// recharged. Returns the new strength; 0 without a creator, 1 for a free spell.
float PayFor(ecs::components::Spell& spell, const Context& context, float cost, bool force);

/// fn_00720830: one turn of upkeep (CalculateCostToMaintain); 1 when the upkeep is 0
float PayForOneTurn(ecs::components::Spell& spell, const Context& context);

/// Spell::PayForOneEvent 0x720A90: costPerEvent
float PayForOneEvent(ecs::components::Spell& spell, const Context& context);

/// Spell::Recharge fn_00720910: the creator tops the chants up to the safety level (recharged spells); returns what it
/// gave
float Recharge(ecs::components::Spell& spell, const Context& context);

} // namespace openblack::magic::chants
