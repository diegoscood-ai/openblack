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

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// What the CHL scripts do to a villager, as runblack.exe W120 does it (docs/bw1-notes/map-loading.md, "Órdenes de
// guion"): MOVE_GAME_THING's walk (Living::SetupMoveToPos 0x5F2830 -> MobileWallHug::SetupMobileMoveToPos 0x60AAD0),
// the script states IN_SCRIPT (4) and SCRIPT_PLAY_ANIM (200) with WAIT_FOR_ANIMATION (23) between the clips,
// GScript::SetScriptState 0x6F82E0 and Villager::IsScriptAnimationComplete 0x7689D0. Not ported: the original's
// DataForScriptRemind (Living +0xB0, Create 0x5EF190 / KeepThatInMind 0x5EF1D0 / fn_005EF2A0), with which a villager
// taken out of a script state remembers its walk and resumes it when it comes back.

namespace openblack::ecs::components
{
struct Transform;
struct WallHug;
} // namespace openblack::ecs::components

namespace openblack::ecs::villager
{
/// MobileWallHug::SetYAngle 0x60DAC0 (vt +0x524, which calls Object::SetYAngle 0x639260) as openblack keeps it:
/// WallHug::yAngle (the 3D angle, VillagerCore's convention) and the drawn rotation the pathfinding gives the transform, AngleY(angle + 90 degrees), the "Scawen" angle
/// of Object::GetWorldMatrix 0x638200. (aproximado) no u16 game angle (+0x5C) is stored: VillagerCore derives it from
/// yAngle when it reads it (rounded to 2048ths). Shared by PathfindingSystem's InitializeStep, InitStepsXZ and
/// ECS/SuperVillager; VillagerCore's SetGameAngle (the game angle in, then FaceAngle) is a second 0x639260 site, kept
void SetYAngle(components::Transform& transform, components::WallHug& wallHug, float angle);
/// Villager::IsStateEntryFunctionSameAs 0x7524D0 (a, b): the entry functions (+0x10 of the state rows, 0xD091A8,
/// VillagerOriginalFns.h) of both states are the same (EnterInScript, EnterPlayAnim, EnterBuilding)
[[nodiscard]] bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b);
/// MobileWallHug::AreWeThere(pos, r) 0x60AD60 (vt +0x85C): the x / z distance from the villager to `pos` is less than
/// its speed (+0x5A, u16 MapCoords a turn: openblack's WallHug::speed in metres) + r. (aproximado: floats in metres
/// instead of the 16.16 MapCoords integers)
[[nodiscard]] bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r);
/// MobileWallHug::GetDestPos 0x416F70 (vt +0x860): the walk's goal (+0x80; openblack's WallHug::goal)
[[nodiscard]] std::optional<glm::vec2> GetDestPos(entt::entity villager);
/// MobileWallHug::AreWeThere(r) 0x60AD40: AreWeThere(GetDestPos(), r)
[[nodiscard]] bool AreWeThereAtDestination(entt::entity villager, float r);
/// Living::SetupMoveToPos 0x5F2830 (pos, final): SetCurrentAndDestinationState(GLivingInfo +0x124 moveState, final) and,
/// only if it returns 1, MobileWallHug::SetupMobileMoveToPos 0x60AAD0: a STEP_THROUGH walk (no obstacle hugging), or
/// ARRIVED if it is there already. Returns 1 if the walk was set up, else 0.
uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final);
/// Villager::StorePreviousState 0x763470 (vt +0x8EC): the final state goes to PREVIOUS, unless it is a passing one
/// (table file +0x10 or +0xB8), which keeps what was stored
void StorePreviousState(entt::entity villager);
/// Villager::IsAvailable 0x751D50 (vt +0x2C): not being deleted (+0xA & 1) and the final state is not 14 DYING
[[nodiscard]] bool IsAvailable(entt::entity villager);
/// Object::IsObjectInMap 0x6392B0 (vt +0x178): +0x24 & 1. (aproximado: openblack keeps no such flag; a villager in the
/// hand is the one taken out of the map)
[[nodiscard]] bool IsObjectInMap(entt::entity villager);
/// GScript::SetScriptState 0x6F82E0 for a villager (the Living that is not a creature, 0x6F831E..0x6F836F)
void SetScriptState(entt::entity villager, VillagerStates state);
/// GScript::SetScriptUlong 0x6F8770 on a villager (0x6F884D..0x6F8855): +0x11C = the clip, +0x120 = the times
void SetScriptAnimation(entt::entity villager, uint32_t anim, uint32_t loops);
/// Villager::ScriptAnimation 0x768A00 (SCRIPT_PLAY_ANIM's clip function, slot 0x60): +0x11C
[[nodiscard]] int32_t ScriptAnimation(entt::entity villager);
/// Villager::IsScriptAnimationComplete 0x7689D0 (PLAYED 064): TOP 23 -> 0; TOP 200 -> the times left (+0x120) == 0;
/// else 1
[[nodiscard]] bool IsScriptAnimationComplete(entt::entity villager);
/// Living::PlayAnimThenSetState 0x5ECAC0 (state, unused): CallExitStateFunction(state) (vt +0x904) and, if 1,
/// CallEntryStateFunction(23 WAIT_FOR_ANIMATION, state) (vt +0x908); the clip is left as it is
void PlayAnimThenSetState(entt::entity villager, VillagerStates state);

/// State 4 IN_SCRIPT: Living::StateInScript 0x5ED9A0
uint32_t StateInScript(components::LivingAction& action);
/// Living::EnterInScript 0x5ED7E0
uint32_t EnterInScript(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Living::ExitInScript 0x5ED9C0
uint32_t ExitInScript(components::LivingAction& action, VillagerStates next);
/// State 200 SCRIPT_PLAY_ANIM: Villager::ScriptPlayAnim 0x768970
uint32_t ScriptPlayAnim(components::LivingAction& action);
/// Living::EnterPlayAnim 0x768840
uint32_t EnterPlayAnim(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// Living::ExitPlayAnim 0x7689C0: vt +0x914 = ExitInScript 0x5ED9C0
uint32_t ExitPlayAnim(components::LivingAction& action, VillagerStates next);
/// State 23 WAIT_FOR_ANIMATION: Living::WaitForAnimation 0x5EC990
uint32_t WaitForAnimation(components::LivingAction& action);
} // namespace openblack::ecs::villager
