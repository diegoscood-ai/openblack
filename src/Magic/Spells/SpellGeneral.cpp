/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellClasses.h"

#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"

// The plain Spell (vtable 0x9805B0, GMagicInfo::AllocSpell 0x5FB450): MAGIC_TYPE 0-9. What a fireball, a lightning bolt
// or a beam explosion does lives in its PSys rules, which send the events.

void openblack::magic::RegisterGeneralSpell()
{
	SpellOps ops;
	ops.initWithPos = base::InitWithPos;       // 0x71FE50
	ops.initWithObject = base::InitWithObject; // 0x7200E0
	ops.process = base::Process;               // 0x720710
	ops.spellEvent = spell_event::SpellEvent;  // 0x720F40
	ops.costToMaintain = base::CalculateCostToMaintain; // 0x720810
	ops.closeDown = base::CloseDown;           // 0x55CE40
	ops.toBeDeleted = nullptr;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;  // 0x720130
	RegisterOps(SpellClass::General, ops);
}
