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

#include <algorithm>
#include <array>

#include <entt/entity/fwd.hpp>

/// The things the scripts hold. openblack's CHL objects are the entities themselves, so a script's slot is the
/// components::ScriptHeld of the entity (ECS/Components/ScriptHeld.h).
namespace openblack::ecs::script_held
{

/// A command hands a thing to the script. `createdByScript` is true for the CREATE family, false for the finders; a
/// thing already in a script keeps its slot.
void AddScriptThing(entt::entity thing, bool createdByScript);
/// A script variable takes it (LHVM's reference callback, ADD_REFERENCE). The first reference sets IsInScript and, for
/// a thing the script created (or one already controlled), ControlledByScript.
void IncrementReference(entt::entity thing);
/// One reference less (the release waits for Process)
void DecrementReference(entt::entity thing);
/// Right after the scripts run each turn: every slot without a reference is released (ReleaseControlFromScript, then
/// out of the script) and freed.
void Process();

/// RELEASE_FROM_SCRIPT 159: a controlled thing is released: a container's controlled members first, then the thing
/// itself by its type (a villager DECIDE_WHAT_TO_DO and villager_script::ReleaseFromScript, an animal WANDER and
/// animal_ai::ReleaseFromScript); never deleted. The slot and its references stay
void ReleaseFromScript(entt::entity thing);

/// Whether a script holds a reference to it
[[nodiscard]] bool IsInScript(entt::entity thing);
/// Whether a script controls it
[[nodiscard]] bool IsControlledByScript(entt::entity thing);
/// Sets script control, also used by the vortex
void SetControlledByScript(entt::entity thing, bool controlled);

/// The natives that take control of the things they are given: SET_SCRIPT_STATE, SET_SCRIPT_STATE_POS,
/// SET_SCRIPT_FLOAT, SET_SCRIPT_ULONG, MOVE_GAME_THING, FLOCK_ATTACH, POPULATE_CONTAINER, WALK_PATH, SWAP_CREATURE,
/// ATTACH_TO_GAME, DETACH_FROM_GAME, DETACH_UNDEFINED_FROM_GAME, SET_ONLY_FOR_SCRIPTS and START_MATCH_WITH_REFEREE
inline constexpr std::array<uint32_t, 14> k_ControllingNatives = {17,  18,  19,  20,  33,  37,  109,
                                                                  177, 210, 214, 215, 216, 217, 218};

/// Whether the native takes control of the things it is given
[[nodiscard]] constexpr bool TakesControl(uint32_t native)
{
	return std::ranges::find(k_ControllingNatives, native) != k_ControllingNatives.end();
}

/// A thing given to a native that takes control: it is controlled by the script from now on. A thing the scripts hold
/// no slot for takes one first, as a thing handed to a script without being kept, so that the release at the end of
/// the turn gives it back to the game
void TakeControl(entt::entity thing);
/// Cannot be eaten, survives an attack: what cannot be destroyed (components::Indestructible) is never prey either
[[nodiscard]] bool CannotBeEaten(entt::entity thing);
/// Marks it as not to be eaten, which makes it indestructible too (components::Indestructible)
void SetCannotBeEaten(entt::entity thing);

/// Sets a living thing's script state: a creature its own (TODO(creature)); a villager villager_script::SetScriptState, an
/// animal animal_ai::SetScriptState (available, in the map, the previous state stored, the state). FLOCK_ATTACH (27),
/// FLOCK_DISBAND (4) and the container loop of SET_SCRIPT_STATE call it; the single-object SET_SCRIPT_STATE tests
/// drowning first, this does not
void SetLivingScriptState(entt::entity living, uint32_t state);

/// The hunters' test (an animal hunting, moving to its prey): a target controlled by a script is only for a hunter
/// that is in a script itself
[[nodiscard]] bool MayTarget(entt::entity hunter, entt::entity target);

} // namespace openblack::ecs::script_held
