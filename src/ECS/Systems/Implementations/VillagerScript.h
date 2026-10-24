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
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager in a script's hands, as the original does it
/// (docs/wiki/scripts/script-commands.md#script-commands-that-move-things-move_game_thing-033-and-friends): standing where the
/// script put it (IN_SCRIPT, 4), walking where it sends it and playing the clips it asks for (SCRIPT_PLAY_ANIM, 200, with
/// WAIT_FOR_ANIMATION, 23, between the plays). The walk and the state changes it uses are our villager's
/// (ECS/Villager/VillagerScript.h). Not ported: the script's reminder, with which a villager taken out of a script state
/// remembers its walk and resumes it when it comes back.
namespace openblack::ecs::villager_script
{

/// Whether a script can set the villager's state: it is available (not being deleted, not dying) and in the map. The
/// callers that also refuse a drowning villager test that themselves
[[nodiscard]] bool CanBeDirected(entt::entity villager);
/// A script puts the villager straight into a state: the state it works towards is kept as its previous one, the state
/// it leaves and the state it enters are told (their answers are ignored), its clip starts from the beginning and the
/// state's counter is cleared
void SetScriptState(entt::entity villager, VillagerStates state);
/// A script sends the villager to a point, where it stands waiting for the script; one already there waits at once.
/// Nothing for a villager out of the map or drowning
void MoveTo(entt::entity villager, glm::vec2 goal);
/// The clip the villager plays when a script puts it in the playing state, and how many times
void SetScriptAnimation(entt::entity villager, uint32_t clip, uint32_t plays);
/// The place a script gives the villager for its script states, on the ground
void SetScriptPlace(entt::entity villager, glm::vec2 place);
/// The number a script gives the villager for its script states
void SetScriptNumber(entt::entity villager, float number);
/// The x of that place, in whole map units, set by something else that keeps its own number in the same storage: the
/// hand's pick-up keeps there the disciple the villager had, so one picked up while it wanders round a script's place
/// and put back down wanders round a place with that x. The rest of the place and the number stay
void SetScriptPlaceX(entt::entity villager, int32_t x);
/// Whether the villager has finished the plays of the clip a script asked for: not while a clip plays out, nor in the
/// playing state while plays are left
[[nodiscard]] bool HasPlayedScriptAnimation(entt::entity villager);
/// The clip a script asked the villager to play, 0 when it asked for none
[[nodiscard]] int32_t ScriptClip(entt::entity villager);
/// A script lets go of the villager, after its state was set: it leaves its flock; in the physics or the hand one
/// without a town joins the vagrants; a dead one dies (and goes into the dead state unless already dying); one with a
/// town decides what to do; any other joins the vagrants and starts out as one
void ReleaseFromScript(entt::entity villager);

/// Waiting in a script's hands (IN_SCRIPT): nothing to do
uint32_t InScript(components::LivingAction& action);
/// Going into IN_SCRIPT: always accepted (1)
uint32_t EnterInScript(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Leaving IN_SCRIPT: 1 (it may leave) for another script state, one a script lets interrupt it, being picked up, a
/// state with the same exit as the one it works towards, or one that is not a final state; else 0
uint32_t ExitInScript(components::LivingAction& action, VillagerStates next);
/// Playing a script's clip (SCRIPT_PLAY_ANIM): one more play starts, and after it the villager plays again or stands
/// waiting; with no plays left it does nothing
uint32_t ScriptPlayAnim(components::LivingAction& action);
/// Wandering round a script's place (SCRIPT_WANDER_AROUND_POSITION): once it has stood its turns at a stop it sets off
/// for another at random within the script's distance of the place, going round what is in its way, and stands there a
/// random number of turns. It goes in and out as IN_SCRIPT does
uint32_t ScriptWanderAroundPosition(components::LivingAction& action);
/// Going into SCRIPT_PLAY_ANIM: always accepted (1)
uint32_t EnterPlayAnim(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Leaving SCRIPT_PLAY_ANIM: the same as leaving IN_SCRIPT
uint32_t ExitPlayAnim(components::LivingAction& action, VillagerStates next);

} // namespace openblack::ecs::villager_script
