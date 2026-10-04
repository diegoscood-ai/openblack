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

#include <entt/entity/entity.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
enum class MeshId : uint32_t;
}

// A villager's death, as runblack.exe W120 does it (docs/bw1-notes/villagers.md, section Death; spec
// dev\documentacion\aldeanos\V12_spec.md, dumps in its v12\): Villager::VillagerDead 0x7506C0 (the owner's alignment, the
// town's death counters, the help sprites, SetDying), the states 13 SET_DYING, 14 DYING and 15 DEAD (the dying clip, the
// smoke, the soul, the skeleton, the corpse's 600 / 120 turns), the deletion (Villager::ToBeDeleted 0x7521B0,
// DeleteDependancys 0x74FD60) and Villager::EndPhysics' dead branches' guard. The mourning is VillagerMourning.h, the soul
// VillagerSoul.h.

namespace openblack::ecs::villager
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// The help-sprite calls of VillagerDead (0x750770..0x7508EF), from the table 0x99A368 (12 bytes per reason: A +0, B +4,
/// C +8)
struct DeathHelp
{
	bool killingPeople {false};    ///< fn_0071CE70 HelpSpritesKillingPeople (A, the killer is the local player)
	bool deathInVillage {false};   ///< fn_0071C810 DeathInVillageSFX (C, else the owner is the local player)
	bool worshippersDying {false}; ///< fn_0071CFE0 HelpSpritesWorshippersDying (reason 4, the owner local)
	bool losingVillagers {false};  ///< 0x71C990 HelpSpritesLosingVillagers (B, adults above the threshold)
	bool lowOnPeople {false};      ///< 0x71CBE0 HelpSpritesLowOnPeople (adults at or below the threshold)
};
/// `adults` is the town's +0x618 read before SetDying (the dying one still counts), `threshold` GTownInfo +0x150
/// populationUnderWhichHelpSpritesWarn
[[nodiscard]] DeathHelp HelpFor(DeathReason reason, bool killerLocal, bool ownerLocal, bool killerIsOwner, bool hasTown,
                                uint32_t adults, uint32_t threshold);
/// Villager::SetDying 0x76A508..0x76A53A: a functional graveyard in the town -> DyingTimeWithGraveyard (+0x294, 120),
/// else DyingTimeWithoutGraveyard (+0x290, 600)
[[nodiscard]] uint16_t DyingTime(bool functionalGraveyard, const GVillagerInfo& info);
/// DyingAnimation 0x423770: in the water 283 P_INTO_DEAD_DROWNED, landType 2 -> 246 P_DEAD2, else 253 P_DYING
[[nodiscard]] int32_t DyingClip(bool water, uint8_t landType);
/// DeadAnimation 0x4237A0: in the water 249 P_DEAD_DROWNED, landType 2 -> 246 P_DEAD2, else 243 P_DEAD1
[[nodiscard]] int32_t DeadClip(bool water, uint8_t landType);

// ---- VillagerDead and the states ---------------------------------------------------------------------------------

/// Villager::VillagerDead 0x7506C0 (reason, killer, amount, drop): nothing while it flies (+0x24 & 0x40) or once dead;
/// the help sprites, the drops (`drop` only decides CreateDroppedResource; DropWood / DropFood always), the owner's
/// alignment, the town's counters and pulse, SetDying and the reason. `killer` none = the neutral player
void VillagerDead(entt::entity villager, DeathReason reason, std::optional<PlayerNames> killer, float amount, int drop);
/// Villager::DestroyedByEffect 0x7502D0 (vt +0x5F8, from Object::ApplyEffect when the life became 0): VillagerDead(2
/// SPELL, player, amount, 1); 1
uint32_t DestroyedByEffect(entt::entity villager, std::optional<PlayerNames> player, float amount);
/// Villager::SetDying 0x76A4C0 (vt +0x6A4; also the function of row 13 SET_DYING): life 0, TOP 14 DYING, out of its
/// abode and town (DeleteDependancys), landType 3, the corpse's counter, out of the world population. 1
uint32_t SetDying(entt::entity villager);
/// Row 13 SET_DYING: StateSetDying 0x5AFF40 = jmp [vt +0x6A4] = SetDying
uint32_t SetDyingState(components::LivingAction& action);
/// Row 14 DYING: StateDying 0x5AFE30 = jmp [vt +0x89C] = Villager::Dying 0x76A570
uint32_t Dying(components::LivingAction& action);
/// Row 15 DEAD: StateDead 0x5AFE90 = jmp [vt +0x8A0] = Villager::Dead 0x76A5E0, then Living::Dead 0x5EC400. 1, or 5
/// once the corpse went (the entity is gone then)
uint32_t Dead(components::LivingAction& action);
/// Row 15's exit, Living::CannotExitState 0x768640: 1 only for IN_HAND 24, FLYING 10 or a state with the same exit
/// function (IsStateExitFunctionSameAs vt +0x96C)
uint32_t CannotExitState(components::LivingAction& action, VillagerStates next);

// ---- queries -----------------------------------------------------------------------------------------------------

/// Living::IsDead 0x417270: status & 1, or TOP 15 DEAD, or not functional (vt +0xD4: (inferred) 1 for a villager)
[[nodiscard]] bool IsDead(entt::entity villager);
/// Villager::GetDeathReason 0x55CB10: +0x118
[[nodiscard]] DeathReason GetDeathReason(entt::entity villager);
/// +0xE0 & 0x40: out of the world population (SetDying)
[[nodiscard]] bool IsCountedOut(entt::entity villager);
/// Villager::GetPlayer 0x7502F0: town ? Town +0x2C : none
[[nodiscard]] std::optional<PlayerNames> GetPlayerOf(entt::entity villager);
/// Living::IsSkeleton 0x416FF0: status & 0x40
[[nodiscard]] bool IsSkeleton(entt::entity villager);
/// Villager::SetSkeleton 0x7562C0 (SET_SKELETON, the constructor's arg 4): the status bit 0x40, the mesh (0x75633D the
/// skeleton MeshPack[0x1FF], else the villager's own meshes) and 0x756436 SetScaleForAge. (pending) no caller sets it
/// yet: CHL SET_SKELETON is Intro's, and the constructor's arg 4
void SetSkeleton(entt::entity villager, bool on);

// ---- deletion ----------------------------------------------------------------------------------------------------

/// Villager::DeleteDependancys 0x74FD60: SET_DYING through the real exits unless already 13 / 14 / 15, the footpath (not
/// ported), a mother's orphans, out of its abode (Abode::RemoveDeletedVillagerFromAbode 0x404220), else its town
/// (Town::RemoveVillager 0x73E210), else the vagrants
void DeleteDependancys(entt::entity villager);
/// Villager::ToBeDeleted 0x7521B0's own part (ecs::ToBeDeleted's villager branch calls it when the villager is marked):
/// DeleteDependancys, then Living::ToBeDeleted 0x5EC0A0's StopReacting (vt +0x998) of the reaction it follows
void ToBeDeletedOverride(entt::entity villager);
/// Villager::ToBeDeleted 0x7521B0 (vt +0xC) = ecs::ToBeDeleted (ToBeDeletedOverride, then Object::ToBeDeleted's tail).
/// No death: no reason, no counters, no corpse
void Delete(entt::entity villager);

// ---- physics -----------------------------------------------------------------------------------------------------

/// Object::EndPhysics 0x6375A0 clears +0x24 & 0x40 before Villager::EndPhysics 0x5F0A60 reaches its dead branches; the
/// physics calls openblack's EndPhysics handler while the body is still listed, so the handler marks the villager out
/// of the physics for that call (VillagerDead's first test)
class EndingPhysicsScope
{
public:
	explicit EndingPhysicsScope(entt::entity villager);
	~EndingPhysicsScope();
	EndingPhysicsScope(const EndingPhysicsScope&) = delete;
	EndingPhysicsScope& operator=(const EndingPhysicsScope&) = delete;

private:
	entt::entity _previous;
};
/// VillagerDead's first test (+0x24 & 0x40, 0x7506C3): in the physics. (approximate) a flying body
/// (PhysicsObjects::IsFlying: openblack's resting proxies are not counted, as Living::SetDying's test in ECS/AnimalAI)
[[nodiscard]] bool IsInPhysics(entt::entity villager);

// ---- tests -------------------------------------------------------------------------------------------------------

/// The help sprites' calls go here instead of audio::guidance (empty: back to the game's)
void SetDeathHelpForTests(std::function<void(entt::entity villager, const DeathHelp& help)> sink);
/// Town +0x748 (graveyard::GetGraveyard) and its IsFunctional (abode_queries::IsFunctional): the tests may override it
/// (nullopt none, else whether it is functional; empty: back to the town's real graveyard)
void SetGraveyardForTests(std::function<std::optional<bool>(entt::entity town)> graveyard);
/// Whether the town has a functional graveyard (the adapter above; Villager::SetDying 0x76A50A..0x76A51E,
/// ReactToDeathPriority 0x766469..0x766486)
[[nodiscard]] bool HasFunctionalGraveyard(entt::entity town);
/// The smoke puffs and the soul Dead would make go here instead (empty: back to the game's): the first DEAD turn's
/// (smoke, soul, skeleton) and the vanish's (smoke only)
struct DeadEffects
{
	bool smoke {false};
	bool soul {false};
	MeshId soulMesh {};
	bool heavenForced {false};
	bool skeleton {false};
};
void SetDeadEffectsForTests(std::function<void(entt::entity villager, const DeadEffects& effects)> sink);
} // namespace openblack::ecs::villager

namespace openblack::ecs::living
{
struct DeadTickResult
{
	uint16_t counter {0};
	bool vanish {false};
};
/// Living::Dead 0x5EC400's counter (0x5EC41E..0x5EC443), shared by the villagers and the animals: not controlled by a
/// script (+0x25 & 4) -> c = counter, counter = c - 1, c == 0 -> vanish (`test ax` before the decrement); then reason 7
/// SACRIFICE -> vanish
[[nodiscard]] DeadTickResult DeadTick(uint16_t counter, bool scriptControlled, DeathReason reason);
} // namespace openblack::ecs::living
