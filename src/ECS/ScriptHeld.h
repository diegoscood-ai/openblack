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

#include <entt/entity/fwd.hpp>

/// The things the scripts hold (GScript's ScriptManage slots, ScriptManage.cpp 0x70CEE0..0x70D8A0; research
/// dev\tmp_dis\animals\script_flags.md). openblack's CHL objects are the entities themselves, so a slot is the
/// components::ScriptHeld of the entity (ECS/Components/ScriptHeld.h).
namespace openblack::ecs::script_held
{

/// GScript::AddScriptGameThing (0x70D0F0): a command hands a thing to the script. `createdByScript` is the slot's +0xC
/// (1 for the CREATE family, 0 for the finders); a thing already in a script keeps its slot.
void AddScriptThing(entt::entity thing, bool createdByScript);
/// GScript::IncrementScriptReference (0x70CF90) -> ScriptManage::IncreaseReferenceCount (0x70D5F0): a script variable
/// takes it (LHVM's reference callback, ADD_REFERENCE). The first reference sets IsInScript and, for a thing the script
/// created (or one already controlled), ControlledByScript.
void IncrementReference(entt::entity thing);
/// GScript::DecrementScriptReference (0x70CFD0) -> fn_0070D670: one reference less (the release waits for Process)
void DecrementReference(entt::entity thing);
/// fn_0070D480, from GScript::Process (0x6EB6DB) right after the scripts' LookIn: every slot without a reference is
/// released (ReleaseControlFromScript 0x70D540, then SetInScript(0)) and freed (fn_0070D800).
void Process();

/// +0x24 & 0x200 (GameThingWithPos::IsInScript 0x402280)
[[nodiscard]] bool IsInScript(entt::entity thing);
/// +0x24 & 0x400 (byte +0x25 & 4)
[[nodiscard]] bool IsControlledByScript(entt::entity thing);
/// GameThingWithPos::SetControlledByScript (0x402240), also the vortex's (fn_005FE3B0 0x5FE474)
void SetControlledByScript(entt::entity thing, bool controlled);
/// +0x24 & 0x4000 (byte +0x25 & 0x40): cannot be eaten, survives an attack (components::CannotBeEaten)
[[nodiscard]] bool CannotBeEaten(entt::entity thing);
/// `or byte [thing+0x25], 0x40` (LandscapeVortex fn_005FE3B0 0x5FE5DD, the puzzle objects)
void SetCannotBeEaten(entt::entity thing);

/// The hunters' test (Animal::HuntingMoveToPos 0x418DD7, fn_00419340 0x419376, fn_004196D0 0x41973B): a target
/// controlled by a script is only for a hunter that is in a script itself
[[nodiscard]] bool MayTarget(entt::entity hunter, entt::entity target);

} // namespace openblack::ecs::script_held
