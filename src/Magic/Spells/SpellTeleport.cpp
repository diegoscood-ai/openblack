/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellClasses.h"

#include <vector>

#include <spdlog/spdlog.h>

#include "ECS/Components/MagicTeleport.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/Core/SpellWithObjects.h"
#include "Magic/Objects/MagicTeleport.h"

// SpellTeleport (a SpellWithObjects, 0xF4 bytes, vtable 0x8F3FD4; GMagicTeleportInfo::AllocSpell 0x5FBDF0): MAGIC_TYPE
// 12. Its one object is the MagicTeleport stone; the spell has no particle type (the vortex belongs to the stone), so
// Spell::InitWithPos sends it SpellEvent 11. Its other virtuals are SpellWithObjects' and Spell's: Process 0x721290,
// CloseDown 0x721300, ToBeDeleted 0x720FD0, ProcessSpellSeed 0x7212F0, SpellEvent 0x720F40.

namespace
{
using namespace openblack;
using namespace openblack::magic;

/// SpellTeleport::InitWithPos 0x5FBEB0: MagicTeleport::Create(pos, this) joins the object list (+0xEC / +0xF0), then
/// Spell::InitWithPos
int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	if (const auto stone = teleport::Create(position, spell); stone != entt::null)
	{
		spell_objects::Add(spell, stone);
	}
	return base::InitWithPos(spell, position, castData, info);
}

/// SpellWithObjects::CloseDown 0x721300: Spell::CoreCloseDown, then (GetSetObjectsDyingOnCloseDown 0x55CF50 = 1) every
/// object not already going gets SetDying (vt 0x6A4; Object::SetDying 0x4027A0 = ToBeDeleted(0) at once). Not named
/// CloseDown: inside magic::RegisterTeleportSpell that name is magic::CloseDown (the vt 0x530 dispatch) -> recursion.
void TeleportCloseDown(entt::entity spell)
{
	base::CloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		teleport::ToBeDeleted(object);
	}
	spell_objects::ProcessObjectsAndRemoveDeleted(spell);
}

/// SpellWithObjects::ToBeDeleted 0x720FD0 (the class part): CloseDown (vt 0x530), the object list emptied (the objects
/// themselves are not deleted here), then Spell::ToBeDeleted (DeleteSpell runs it next)
void ToBeDeleted(entt::entity spell)
{
	TeleportCloseDown(spell);
	for (const auto object : std::vector<entt::entity>(spell_objects::Objects(spell)))
	{
		spell_objects::Remove(spell, object);
	}
}
} // namespace

void openblack::magic::RegisterTeleportSpell()
{
	SpellOps ops;
	ops.initWithPos = InitWithPos;
	ops.initWithObject = base::InitWithObject;
	ops.process = spell_objects::Process; // SpellWithObjects::Process 0x721290
	ops.spellEvent = spell_event::SpellEvent;
	ops.costToMaintain = base::CalculateCostToMaintain;
	ops.closeDown = TeleportCloseDown;
	ops.toBeDeleted = ToBeDeleted;
	ops.hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast;
	ops.particleType = base::GetParticleType;
	RegisterOps(SpellClass::Teleport, ops);
}
