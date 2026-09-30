/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::ecs::components
{

/// A game thing the scripts know about: the original's ScriptManage slot (0x14 bytes, 511 of them from 0xD967F8, taken
/// by GScript::AddScriptGameThing 0x70D0F0) and the script bits of GameThingWithPos's word +0x24. The logic is in
/// ECS/ScriptHeld.h (dev\tmp_dis\animals\script_flags.md).
struct ScriptHeld
{
	/// ScriptManage +0x10: how many script variables hold it (IncreaseReferenceCount 0x70D5F0 stops at 0xFF)
	uint8_t references {0};
	/// ScriptManage +0xC (GScript::IsCreatedByScript 0x70D440): the slot was taken by a CREATE-type command
	/// (AddScriptGameThing(thing, 1)); the finders (CALL, GET_...) take it with 0
	bool createdByScript {false};
	/// +0x24 & 0x200, GameThingWithPos::IsInScript 0x402280 (Object::SetInScript 0x639B20): a script holds it
	bool inScript {false};
	/// +0x24 & 0x400 (byte +0x25 & 4), SetControlledByScript 0x402240: set with the first reference of a thing the
	/// script created. Not prey unless the hunter is in a script, never reacts, its corpse never times out, a flock
	/// with it is a script flock.
	bool controlledByScript {false};
	/// false: only the +0x24 bits, no ScriptManage slot (SetControlledByScript on a thing no script took): Process
	/// leaves it alone
	bool hasSlot {true};
};

/// +0x24 & 0x4000 (byte +0x25 & 0x40): cannot be eaten and survives an attack. Set by LandscapeVortex's thing-out
/// (fn_005FE3B0 0x5FE5DD: everything that comes out of the land-to-land vortex), the puzzle objects and a save that had
/// it. Read by fn_004196D0 (not prey), Living::BeingEaten 0x5EC4E0 and Villager::BeingEaten 0x76B380 (LANDED
/// instead of dead).
struct CannotBeEaten
{
};

} // namespace openblack::ecs::components
