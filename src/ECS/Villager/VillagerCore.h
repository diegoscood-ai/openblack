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

#include <functional>
#include <optional>
#include <string>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Villager/VillagerAge.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// The core of the villager's state machine, as runblack.exe W120 does it (docs/bw1-notes/villagers.md; research
// dev\tmp_dis\aldeanos\V1_spec.md): the turn (Villager::ProcessState 0x74FF70, CheckEveryTime 0x750410), the state
// changes (SetTopState 0x752010, SetCurrentAndDestinationState 0x7520E0, SetState 0x753690, the exit and entry calls)
// with their return codes, the creation (Villager::Create 0x74FBE0 and the constructor 0x74F950), CREATED (85) and
// PAUSE_FOR_A_SECOND (239). The rows of the state table are reached through LivingActionSystemInterface, so a test can
// put a fake one in the Locator.

namespace openblack::ecs::villager
{
using Index = components::LivingAction::Index;

/// The original's return codes
inline constexpr uint32_t k_Done = 1;             ///< done / accepted
inline constexpr uint32_t k_EntryNoSet = 0x23;    ///< an entry function accepted and set the states itself
inline constexpr uint32_t k_ExitRefused = 0x2E;   ///< an exit function refused the change: nothing changed
inline constexpr uint32_t k_EntryRefused = 0x2F;  ///< the entry function refused (Villager then enters 163)
/// k_TurnsPerYear (1500, 0xD01A04): VillagerAge.h

// ---- clock and random --------------------------------------------------------------------------------------------

/// GGame +0x205A40, the game turn (Game::GetTurn), or the test's
[[nodiscard]] uint32_t CurrentTurn();
void SetTurnForTests(std::optional<uint32_t> turn);
/// GRand::GameRand 0x6DE510: 0 .. n - 1 (0 for 0); forwards to game_random::GameRand
[[nodiscard]] uint32_t GameRand(uint32_t n);
/// GRand::GameFloatRand 0x6DE530: 0 for 0, signed like x; forwards to game_random::GameFloatRand
[[nodiscard]] float GameFloatRand(float x);
/// The tests' scripted draws (empty functions: back to the game's)
void SetRandForTests(std::function<uint32_t(uint32_t)> rand, std::function<float(float)> floatRand);

// ---- data --------------------------------------------------------------------------------------------------------

/// The villager's info.dat entry (Villager +0x28); villager[0] if openblack has none for it (never in the game)
[[nodiscard]] const GVillagerInfo& InfoOf(entt::entity villager);
[[nodiscard]] VillagerStates GetState(entt::entity villager, Index index);
/// Villager::GetFinalState 0x751DD0: TOP if it is a final state (table file 0x0C), else FINAL
[[nodiscard]] VillagerStates GetFinalState(entt::entity villager);
/// Living::GetAge 0x5ECAF0: (turn - birthTurn) / 1500, unsigned (AgeFromBirthTurn)
[[nodiscard]] uint32_t GetAge(entt::entity villager);
/// Living::SetAge 0x5ED2C0: birthTurn = turn - age * 1500 (BirthTurnForAge)
void SetAgeBirthTurn(entt::entity villager, uint32_t age, uint32_t turn);
/// Villager::IsChild 0x55C970 (vt +0xAF8): flags & 8
[[nodiscard]] bool IsChild(entt::entity villager);
/// Villager::IsWoman 0x752620: info sex (+0x1F8) FEMALE and not a child
[[nodiscard]] bool IsWoman(entt::entity villager);
/// Villager::IsHungry 0x752600: food <= info.hungryForFood (+0x2C0)
[[nodiscard]] bool IsHungry(entt::entity villager);
/// Villager::GetGameTurnsSinceLastChecked 0x750670 / SetGameTurnLastChecked 0x7506A0
[[nodiscard]] uint32_t GetGameTurnsSinceLastChecked(entt::entity villager, uint32_t turn);
void SetGameTurnLastChecked(entt::entity villager, uint32_t turn);
/// GameThingWithPos +0x24 & 0x400 (byte +0x25 & 4): controlled by a script (ecs::script_held)
[[nodiscard]] bool IsScriptControlled(entt::entity villager);
/// POWER 0x75BB60: 1 - min(x, 1)^3
[[nodiscard]] float Power(float x);
/// Villager::GetDesireForFood 0x75BB50: POWER(food)
[[nodiscard]] float GetDesireForFood(entt::entity villager);
/// g_DiscipleInfos 0x99A1F8 (.rdata, 13 rows of 0x1C) +0xC: the disciple type ignores the villager's needs
[[nodiscard]] bool DiscipleIgnoresNeeds(uint8_t discipleType);

// ---- creation ----------------------------------------------------------------------------------------------------

/// Villager::Create 0x74FBE0's first draw: GameRand(10) <= 1 tries SpecialVillager::Create 0x71F1A0. TODO(V14): no
/// special villagers yet, so a normal one is always made; returns whether the original would have tried.
bool RollSpecialVillager();
/// Villager::SetAge 0x7528C0: a child below grownUpAge (flags |= 8), else an adult of at least 18 (flags &= ~8);
/// lifeStage mirrors the bit; then the meshes and scale (`meshesAndScale`, DetailMeshes + InitialiseScale +
/// SetScaleForAge, which draw FloatRand) and Living::SetAge 0x5ED2C0. Returns the age set.
uint32_t SetAge(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn,
                const std::function<void(uint32_t age)>& meshesAndScale = {});
/// Villager::Villager 0x74F950 after Living::Living (the entity has its Villager, with life = info.life, and a
/// LivingAction in state 0): SetToZero 0x74FB20, SetAge (`meshesAndScale` is its DetailMeshes + InitialiseScale +
/// SetScaleForAge part, which draws FloatRand), food, lastCheckTurn, the state counter and the water rule:
/// SetState(TOP, inWater ? 16 DROWNING : 85 CREATED) without entry, clips or speed.
void Construct(entt::entity villager, const GVillagerInfo& info, uint32_t age, uint32_t turn, bool inWater,
               const std::function<void(uint32_t age)>& meshesAndScale);

// ---- the turn ----------------------------------------------------------------------------------------------------

/// Living::ProcessReaction 0x5F1270 for a villager. TODO(reactions, Milagros M-5): no-op; Milagros' handler
/// (VillagerReactions.cpp) applies the villagers' reactions meanwhile
void ProcessReaction(entt::entity villager);
/// Villager::ProcessState 0x74FF70
uint32_t ProcessState(entt::entity villager, uint32_t turn);
/// Villager::CheckEveryTime 0x750410
uint32_t CheckEveryTime(entt::entity villager, uint32_t turn);
/// CheckEveryTime's hurt rule (0x750557..0x7505C3) enters 36 GO_HOME only when enabled: on since V2 (36 has its state
/// function, VillagerHome.cpp); the tests may switch it off
void SetGoHomeEnabledForTests(bool enabled);
/// Villager::ProcessFoodSpeedup 0x753430
void ProcessFoodSpeedup(entt::entity villager, uint32_t turn);

// ---- state changes -----------------------------------------------------------------------------------------------

/// Villager::SetTopState 0x752010 (vt +0x8E8): the pause, then Living::SetTopState; 0x2F -> CallEntryStateFunction(163)
uint32_t SetTopState(entt::entity villager, VillagerStates state);
/// Living::SetTopState 0x5F28E0: exit, out-of clip, entry, speed and clips. 1, 0x2E or 0x2F
uint32_t LivingSetTopState(entt::entity villager, VillagerStates state);
/// Villager::SetCurrentAndDestinationState 0x7520E0 (vt +0x8DC): Living's, 0x2F -> CallEntryStateFunction(163)
uint32_t SetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination);
/// Living::SetCurrentAndDestinationState 0x5F2980: the exit (0x5F298B), the out-of clip (0x5F299C) and the into clip
/// (0x5F29EB) all get the destination `d` (ebx = arg 2, 0x5F2981); only the entry gets both (c, d) (0x5F29A5..0x5F29B1).
/// So: out = VillagerCallOutOfAnimation(e, d); ...; VillagerApplyStateClips(e, d, out), whose into function is the
/// TOP's (= c after the entry) with (1, d).
uint32_t LivingSetCurrentAndDestinationState(entt::entity villager, VillagerStates current, VillagerStates destination);
/// Villager::SetState 0x753690 (vt +0x938): one index, with the town's modifiers; setting TOP clears FINAL
void SetState(entt::entity villager, Index index, VillagerStates state);
/// Villager::AdjustTownModifier 0x753560: +-the state's served desire in the town
void AdjustTownModifier(entt::entity villager, VillagerStates state, bool entering);
/// Villager::CallExitStateFunction 0x752320: TOP's and (if different) the final state's exit, told `next`
uint32_t CallExitStateFunction(entt::entity villager, VillagerStates next);
/// Villager::CallEntryStateFunction 0x7523D0 (vt +0x90C): the entry of `state`; 1 -> SetState(TOP, state)
uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates state);
/// Villager::CallEntryStateFunction 0x752440 (vt +0x908): `current`'s, then `destination`'s -> SetState(FINAL, ...)
uint32_t CallEntryStateFunction(entt::entity villager, VillagerStates current, VillagerStates destination);
/// Villager::IsStateExitFunctionSameAs 0x752530 (vt +0x96C): GetFinalState's row and `next`'s row have the same exit
/// function (+0x20, 0xD091B8, the 16-byte member pointers compared whole: the original's addresses in
/// VillagerOriginalFns.h, 0 = none, two 0s are the same), or else `next` is not a final state (0xDB9E84)
[[nodiscard]] bool IsStateExitFunctionSameAs(entt::entity villager, VillagerStates next);
/// Villager::CanPauseForASecond 0x752120
[[nodiscard]] bool CanPauseForASecond(entt::entity villager, VillagerStates state);
/// Villager::SetupPauseForASecond 0x76B090: SetCurrentAndDestinationState(239, state) == 1
uint32_t SetupPauseForASecond(entt::entity villager, VillagerStates state);
/// Living::SetTopStateToFinal 0x5ECA80: SetTopState(FINAL) (the raw +0x8D)
void SetTopStateToFinal(entt::entity villager);
/// Villager::SetupWaitForCounter 0x76B060: SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final) == 1 and then the
/// state counter (+0x58, LivingAction::turnsUntilStateChange) = `turns`; 0 when the state change was refused
uint32_t SetupWaitForCounter(entt::entity villager, uint16_t turns, VillagerStates final);
/// Living::WaitForCounter 0x5EC310, the state function of 57: one turn off the counter and, at 0, the final state
uint32_t WaitForCounter(components::LivingAction& action);
/// Living::SetupMoveToWithHug 0x5F2890 (pos, final): SetCurrentAndDestinationState(GLivingInfo +0x124 moveState =
/// MOVE_TO_POS, final) first and, only if it returns 1, MobileWallHug::SetupMobileMoveToPos(pos, 0xC) (openblack: the
/// WallHug goal, a fresh step and a LINEAR move). Returns 1 if the walk was set up, else 0.
uint32_t SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final);

/// Living::LookAtPos 0x5EC550 (pos as MapCoords x / z, ecs::town_queries): one turning step towards pos, of at most
/// 0x40 / 0x80 / 0x100 (mode 0 / 1 / 2) or the mode itself (any other mode) 2048ths, the short way; 1 when it faces it.
/// Two arguments only (ret 8 at 0x5EC5BF / 0x5EC5E4). The angle (Living +0x5C) is WallHug::yAngle in openblack
/// (aproximado: kept in radians, rounded to 2048ths)
uint32_t LookAtPos(entt::entity villager, glm::ivec2 pos, uint32_t mode);

/// State 85 CREATED: Villager::VillagerCreated 0x753DD0
uint32_t VillagerCreated(components::LivingAction& action);
/// State 239 PAUSE_FOR_A_SECOND: Villager::PauseForASecond 0x76B0B0
uint32_t PauseForASecond(components::LivingAction& action);

// ---- checks that are neutral until their milestone ---------------------------------------------------------------

/// Villager::CheckHungry 0x75BCC0. TODO(V4): only its reset of lastCheckTurn (0x75BEDF; 0 turns -> 0, 0x75BCD0)
bool CheckHungry(entt::entity villager, uint32_t turn);
/// Villager::CheckChildGrownUp 0x751050. TODO(V4)
bool CheckChildGrownUp(entt::entity villager);
/// Villager::WomanSpecial 0x752240. TODO(V4)
bool WomanSpecial(entt::entity villager);
/// Villager::CheckDeathFromOldAge 0x760CA0. TODO(V4) (read in dev\tmp_dis\aldeanos\core\d_age.txt)
bool CheckDeathFromOldAge(entt::entity villager);

// ---- death (provisional until V12) -------------------------------------------------------------------------------

/// Villager::VillagerDead 0x7506C0. TODO(V12, villager death): marks it and kills it at the end of the turn
/// (FlushDeaths: ecs::life::Kill). The original keeps it alive (SetDying -> 13), which openblack has not ported.
void VillagerDead(entt::entity villager, DeathReason reason, PlayerNames player, float amount, int flag);
[[nodiscard]] bool IsDying(entt::entity villager);
/// The reason VillagerDead was given this turn, if any
[[nodiscard]] std::optional<DeathReason> PendingDeathReason(entt::entity villager);
void FlushDeaths();
/// The tests: forget the deaths of the turn without killing (FlushDeaths needs the physics and the animals)
void ForgetDeathsForTests();

// ---- test hooks (VillagerDebugHooks.cpp) -------------------------------------------------------------------------

/// OPENBLACK_VILLAGER_TRACE=1 (all) or =<n> (the villager with creation index n)
[[nodiscard]] bool TraceOn(entt::entity villager);
void Trace(entt::entity villager, const std::string& line);
/// Once a turn, before the villagers: OPENBLACK_TEST_VILLAGER_LIFE / _STATE / _BORN_IN_WATER / _POISONED and the
/// trace's summary every 100 turns
void RunDebugHooks(uint32_t turn);
} // namespace openblack::ecs::villager
