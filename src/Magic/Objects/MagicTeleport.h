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

#include "ECS/ObjectMetrics.h"
#include "Enums.h"

// The teleport stones: each TELEPORT cast leaves one invisible stone with a vortex PSys; a player's stones form a
// list. A living thing that uses a stone comes out of the stone of the same player that brings it nearest to where it
// is going. Wiki: docs/wiki/magic/teleport.md#teleport.
// A pool lives off the chants of the seed that cast it (the spell's timer is -1, info.dat effect 12, so only the chants
// end it): a dispenser orb gives 2000 and the spell spends 1 a turn, about 200 s, unless a player keeps paying. Each
// useful jump gives chants back (JumpCost: PayFor with a negative cost), so a pool that is being used
// lasts longer. That is the original's behaviour, not a defect.

namespace openblack::magic::teleport
{
/// 6.0, the stone's radius (also its 2D radius), which the teleport's cast check keeps free of MultiMapFixed objects
constexpr float k_Radius = ecs::object::k_MagicTeleportRadius;
/// ShouldLivingThingReact's detour factor
constexpr float k_DetourFactor = 1.2f;
/// The invisible hand-collision sphere drawn while the spell still has its seed
constexpr float k_HandCollisionRadius = 3.0f;
/// The vortex's particle type, made with the stone
constexpr int k_VortexParticleType = 73;
/// SPOT_VISUAL 14 VILLAGER_TELEPORT (the villager teleport effect), at both ends of a jump
constexpr auto k_SpotVisualVillagerTeleport = SpotVisualType::VillagerTeleport;

// ---- pure rules (no ECS: test_teleport) ----

/// The fast distance on MapCoords (fixed point, 6553.6 units a metre): max(|dx|, |dz|) + min / 2, integer
[[nodiscard]] int32_t FastDistance(const glm::vec3& a, const glm::vec3& b);

/// ShouldLivingThingReact's test for one other stone T: 1.2 x (|living - this| + |T - dest|) < |living - dest|
/// (FastDistance)
[[nodiscard]] bool IsWorthTheDetour(const glm::vec3& living, const glm::vec3& destination, const glm::vec3& stone,
                                    const glm::vec3& other);

/// DoTeleport's choice among the other stones: the largest saving s = |dest - living| - |dest - T| (metres), above 0
/// (above -1e6 when forced). -1 = none; `saving` gets the best s.
[[nodiscard]] int ChooseTarget(const glm::vec3& living, const glm::vec3& destination, const std::vector<glm::vec3>& others,
                               bool force, float* saving);

/// DoTeleport's cost: PayFor(-saving x costPerKilometer x 0.001, forced). Literal: a useful jump (saving > 0) gives the
/// spell chants; only a forced jump backwards costs (PayFor has no clamp).
[[nodiscard]] float JumpCost(float saving, float costPerKilometer);

/// A route through the player's stones (a villager going to the worship site): the stone nearest `from` (d1) and,
/// separately, the smallest distance d2 from any stone to `to`, both starting at maxDistance; that nearest stone if
/// d1 + d2 < maxDistance (strictly less). -1 = none. The distances are in metres, flat (x / z only).
[[nodiscard]] int FindRouteStone(const std::vector<glm::vec3>& stones, const glm::vec3& from, const glm::vec3& to,
                                 float maxDistance);

// ---- the stones ----

/// The stone at a MapCoords position (metres, y above the land) for a SpellTeleport; entt::null if it could not be
/// made
entt::entity Create(const glm::vec3& mapPosition, entt::entity spell);
/// A saved game's stone made again at a MapCoords position (metres, y above the land), as the game's load makes it: of
/// its saved player, the newest of that player's stones, in its cells, with its pool started again as its making starts
/// it; no reaction (the saved one is given back once the reactions are made), no spell yet and no travellers (they are
/// not saved). Its creation number and its record are left to the load
entt::entity Restore(const glm::vec3& mapPosition, std::optional<PlayerNames> player);
/// The stone goes, and so leaves its player's list
void ToBeDeleted(entt::entity stone);

/// The player's stones, newest first (by their serials)
[[nodiscard]] std::vector<entt::entity> StonesOf(PlayerNames player);
/// The stone's player, if it has one
[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity stone);
/// The stone's position as MapCoords (metres, y above the land)
[[nodiscard]] glm::vec3 MapPositionOf(entt::entity object);

/// The living moves and another stone of the stone's player is worth the detour to its final destination
[[nodiscard]] bool ShouldLivingThingReact(entt::entity stone, entt::entity living);
/// Where the living is going in the end, as a map position: a villager's or an animal's final destination, or where a
/// creature last set off for (where it stands when it never set off)
[[nodiscard]] glm::vec3 FinalDestinationOf(entt::entity living);
/// The living's old entry goes and {living, destination} is added (newest first)
void RegisterDestination(entt::entity stone, entt::entity living, const glm::vec3& destination);
/// 1 if the living jumped to another stone
int DoTeleport(entt::entity stone, entt::entity living, bool force);

/// A villager in the hand over a stone: the villager's player is the stone's and that player has more than one stone
/// (count != 1)
[[nodiscard]] bool ValidToApplyVillagerDirectly(entt::entity stone, entt::entity villager);
/// A villager dropped on a stone from the hand: FLYING, the interface lets it go at the stone, LANDED, it decides what
/// to do; then its final destination is registered and it jumps at once (forced), and decides again. 1 when it jumped,
/// else 0x17. The hand takes it out first (HandApplyToObject.cpp).
int ApplyVillagerDirectly(entt::entity stone, entt::entity villager);

/// G_SpellTeleportEnergiseGo (InGame 39) where it was, G_SpellTeleportEnergiseArrive
/// (InGame 38) where it goes, then MoveMapObject. A creature is not moved here: its mind takes it through the jump,
/// carried to the point and then walking on to `destination`
void MoveByTeleport(entt::entity living, const glm::vec3& mapPosition, const glm::vec3& destination);
/// A creature the stone carried comes out at the far stone, a map position: G_SpellTeleportEnergiseArrive (InGame 38)
/// there, and it is put there at once without walking
void CreatureArrives(entt::entity creature, const glm::vec3& mapPosition);

/// Is there a MultiMapFixed (buildings, fields, features, mobile statics, citadel parts, spell icons, other stones)
/// whose position is within `radius` of the point?
[[nodiscard]] bool AnyMultiCellStaticNear(const glm::vec3& mapPosition, float radius);

/// Each player's turn: for each available stone of each player, drops the travellers that are gone or no longer react
/// to the stone's reaction
void ProcessPlayers();
/// One player's turn: for each available stone of the player, drops the travellers that are gone or no longer react to
/// the stone's reaction
void ProcessPlayer(PlayerNames player);
/// Every frame: the vortex follows the stone and is stepped with the frame time
void UpdateFrame(float seconds);

/// The stones whose hand collision is on (the spell still has its seed: an invisible sphere of k_HandCollisionRadius),
/// for the hand's pick (HandPlacement.cpp PickObjectAlongRay)
[[nodiscard]] std::vector<entt::entity> HandCollisionStones();
/// The TELEPORT seed a stone gives the hand: picking the stone up forwards to the spell's seed; entt::null if the spell
/// has none
[[nodiscard]] entt::entity SeedOf(entt::entity stone);
/// The stone's own REACT_TO_TELEPORT, which a Living takes when it starts reacting to it (the worship activity check
/// reads it to start reacting); 0 when there is none
[[nodiscard]] uint32_t ReactionOf(entt::entity stone);

/// OPENBLACK_TEST_TELEPORT (TeleportDebugHooks.cpp)
void RunDebugHooks();
/// A land is loaded
void Clear();
[[nodiscard]] bool TraceEnabled();
} // namespace openblack::magic::teleport
