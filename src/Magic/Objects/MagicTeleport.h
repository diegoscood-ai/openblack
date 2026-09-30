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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The teleport stones (MagicTeleport.cpp 0x5FBF50..0x5FCE00): each TELEPORT cast leaves one invisible stone with a
// vortex PSys; a player's stones form a list (GPlayer +0xA58 head, +0xA5C count). A living thing that uses a stone
// comes out of the stone of the same player that brings it nearest to where it is going. Wiki: docs/bw1-notes/magic.md,
// "Teletransporte".

namespace openblack::magic::teleport
{
/// fn_005FCCA0: 6.0 (also MagicTeleport::Get2DRadius 0x5FCCB0), the radius GMagicTeleportInfo's CanCast keeps free of
/// MultiMapFixed objects
constexpr float k_Radius = 6.0f;
/// 0x8C6C98: ShouldLivingThingReact's detour factor
constexpr float k_DetourFactor = 1.2f;
/// MagicTeleport::Draw 0x5FCCC0: the invisible hand-collision sphere while the spell still has its seed
constexpr float k_HandCollisionRadius = 3.0f;
/// PSysInterface::Create's particle type in CallVirtualFunctionsForCreation 0x5FC260 (0x49 = SF_TeleportVortex)
constexpr int k_VortexParticleType = 73;
/// SPOT_VISUAL 14 VILLAGER_TELEPORT (SF_TeleportVillager), at both ends of a jump
constexpr int k_SpotVisualVillagerTeleport = 14;

// ---- pure rules (no ECS: test_teleport) ----

/// GUtils::FastDistance 0x74CE10 on MapCoords (fixed point, 6553.6 units a metre): max(|dx|, |dz|) + min / 2, integer
[[nodiscard]] int32_t FastDistance(const glm::vec3& a, const glm::vec3& b);

/// ShouldLivingThingReact's test for one other stone T 0x5FC590: 1.2 x (|living - this| + |T - dest|) < |living - dest|
/// (FastDistance)
[[nodiscard]] bool IsWorthTheDetour(const glm::vec3& living, const glm::vec3& destination, const glm::vec3& stone,
                                    const glm::vec3& other);

/// DoTeleport's choice 0x5FC790 among the other stones: the largest saving s = |dest - living| - |dest - T| (metres,
/// GetDistance 0x74CD50), above 0 (above -1e6 when forced). -1 = none; `saving` gets the best s.
[[nodiscard]] int ChooseTarget(const glm::vec3& living, const glm::vec3& destination, const std::vector<glm::vec3>& others,
                               bool force, float* saving);

/// DoTeleport's cost: PayFor(-saving x costPerKilometer x 0.001, forced) (fn_005FBF10). Literal: a useful jump
/// (saving > 0) gives the spell chants; only a forced jump backwards costs (research R13, PayFor 0x720990 has no clamp).
[[nodiscard]] float JumpCost(float saving, float costPerKilometer);

/// GPlayer fn_0064D6B0 (Villager::CanIGetToTheWorshipSite 0x76BC20): the stone nearest `from` (d1) and, separately,
/// the smallest distance d2 from any stone to `to`, both starting at maxDistance; the first stone if d1 + d2 <
/// maxDistance. -1 = none.
[[nodiscard]] int FindRouteStone(const std::vector<glm::vec3>& stones, const glm::vec3& from, const glm::vec3& to,
                                 float maxDistance);

// ---- the stones ----

/// MagicTeleport::Create 0x5FC1F0 (ctor 0x5FC130, CallVirtualFunctionsForCreation 0x5FC260): the stone at a MapCoords
/// position (metres, y above the land) for a SpellTeleport; entt::null if it could not be made
entt::entity Create(const glm::vec3& mapPosition, entt::entity spell);
/// MagicTeleport::ToBeDeleted 0x5FC310 (and the entity goes)
void ToBeDeleted(entt::entity stone);

/// The player's stones, newest first (GPlayer +0xA58 list)
[[nodiscard]] const std::vector<entt::entity>& StonesOf(PlayerNames player);
/// The stone's player (MagicTeleport::GetPlayer 0x5FC430), if it has one
[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity stone);
/// The stone's position as MapCoords (metres, y above the land)
[[nodiscard]] glm::vec3 MapPositionOf(entt::entity object);

/// MagicTeleport::ShouldLivingThingReact 0x5FC590: the living moves (IsMoving vt 0x174) and another stone of the
/// stone's player is worth the detour to its final destination (GetFinalDestPos vt 0x884)
[[nodiscard]] bool ShouldLivingThingReact(entt::entity stone, entt::entity living);
/// fn_005FC6A0: the living's old entry goes and {living, destination} is added (newest first)
void RegisterDestination(entt::entity stone, entt::entity living, const glm::vec3& destination);
/// MagicTeleport::DoTeleport 0x5FC790: 1 if the living jumped to another stone
int DoTeleport(entt::entity stone, entt::entity living, bool force);

/// MagicTeleport::ValidToApplyVillagerDirectlyToTeleport fn_005FC4B0 (a villager in the hand over a stone): the
/// villager's player is the stone's and that player has more than one stone (count != 1)
[[nodiscard]] bool ValidToApplyVillagerDirectly(entt::entity stone, entt::entity villager);
/// fn_005FC4F0 (Villager::ApplyThisToObject 0x752D40 on a stone): FLYING, the interface lets it go at the stone
/// (fn_005DA0C0), LANDED, DecideWhatToDo (vt 0x8C8); then its final destination is registered and it jumps at once
/// (forced), and decides again. 1 when it jumped, else 0x17.
int ApplyVillagerDirectly(entt::entity stone, entt::entity villager);

/// Living::MoveByTeleport 0x5EC340: G_SpellTeleportEnergiseGo (InGame 39) where it was, G_SpellTeleportEnergiseArrive
/// (InGame 38) where it goes, then MoveMapObject
void MoveByTeleport(entt::entity living, const glm::vec3& mapPosition);

/// Is there a MultiMapFixed (AsMultiMapFixed vt 0x678: buildings, fields, features, mobile statics, citadel parts, spell
/// icons, other stones) whose position is within `radius` of the point (fn_00604C30 with that predicate)?
[[nodiscard]] bool AnyMultiMapFixedNear(const glm::vec3& mapPosition, float radius);

/// GPlayer::Process 0x6496BC -> fn_005FCC70: for each available stone of each player, fn_005FCBA0 drops the travellers
/// that are gone or no longer react to the stone's reaction
void ProcessPlayers();
/// MagicTeleport::Draw 0x5FCCC0, every frame: the vortex follows the stone and is stepped with the frame time
void UpdateFrame(float seconds);

/// The stones whose hand collision is on (the spell still has its seed: Draw's SendInvisibleDrawCollision), for the hand
[[nodiscard]] std::vector<entt::entity> HandCollisionStones();
/// The TELEPORT seed a stone gives the hand: MagicTeleport::ValidForPlaceInHand 0x5FC440 / InterfaceSetInMagicHand
/// 0x5FC470 forward to the spell's seed (Spell +0xAC); entt::null if the spell has none
[[nodiscard]] entt::entity SeedOf(entt::entity stone);

/// OPENBLACK_TEST_TELEPORT (TeleportDebugHooks.cpp)
void RunDebugHooks();
/// A land is loaded
void Clear();
[[nodiscard]] bool TraceEnabled();
} // namespace openblack::magic::teleport
