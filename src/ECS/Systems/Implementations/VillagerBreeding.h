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

#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "Enums.h"

// Love out of doors (docs/wiki/beings/villager-life.md#love-outside-the-home-and-the-breeder): two villagers of an age
// and of opposite sexes meet (START_HAVING_SEX 122), kiss (HAVING_SEX 123), and the woman may fall pregnant as the kiss
// ends; then they part (STOP_HAVING_SEX 124). Each keeps the other as its target thing (Villager::targetThing) while
// they are together. The states at home (125-127) do nothing. The pregnancy and the birth are VillagerBirth's.
//
// The breeder disciple brings it about: a villager the hand leaves by a home becomes one, and first seeks the nearest
// villager of an age and of the other sex about it (BREEDER_JUST_LANDED 254). Its job (villager::SetupBreederDisciple,
// declared in VillagerDisciple.h) sends it to a spot by a home of its town or by its food drop-off, about which it
// wanders (BREEDER_DISCIPLE 225), calling the villagers of the other sex to it (reaction 30 REACT_TO_BREEDER, taken up
// in state 227) until it is pregnant, its town wants sleep most, or the town is too far over full.

namespace openblack::ecs::villager
{

/// The villager and its mate meet to make love (MakeVillagesMeet into 122, 0.562 m apart); each takes the other as its
/// target. Whether they were touching already
bool GoAndHaveSexWith(entt::entity villager, entt::entity mate);
/// The two go into `state` together, the mate first: each takes the other as its target and the kiss's turns
/// (villager_breeding::LoveTurns). 0, and nothing changed, when the mate is not available; else 1
uint32_t StartHavingSexWith(entt::entity villager, entt::entity mate, VillagerStates state);

/// State 122 START_HAVING_SEX: the mate about and touching (edges within 0.57 m) -> both face each other and kiss (123);
/// about but not touching -> they meet again; else DECIDE_WHAT_TO_DO. 1
uint32_t StartHavingSex(components::LivingAction& action);
/// State 123 HAVING_SEX: once the clip has played through, a woman of an age falls pregnant on a draw below her
/// pregnancy chance (even if pregnant already); then 124. 1
uint32_t HavingSex(components::LivingAction& action);
/// State 124 STOP_HAVING_SEX: inside its home -> it goes home; else DECIDE_WHAT_TO_DO. 1
uint32_t StopHavingSex(components::LivingAction& action);
/// States 125-127 (at home): nothing. 1
uint32_t SexAtHome(components::LivingAction& action);
/// The exit of 122-124. Into another of them: nothing. Else it lets go of its partner and, when the partner is still in
/// the same state as itself, the partner lets go too and stops (124). Always 1 (it may leave)
uint32_t ExitSex(components::LivingAction& action, VillagerStates next);
/// The validate of 122 (and of 225): a partner no longer available is let go. The result is not used: true
bool SexValidate(components::LivingAction& action);
/// When the hand takes the villager: if its final state as stored is HAVING_SEX and it has a partner, the partner is
/// let go and decides what to do, and the villager lets go of it too
void LetPartnerGo(entt::entity villager);

// ---- the breeder disciple ----------------------------------------------------------------------------------------

/// A walk to a point up to 8 m from the wander area (two draws, the angle first), into `state` with obstacle hugging,
/// then the turns it waits there (a draw of its own). 1
uint32_t SetupWander(entt::entity villager, const map_coords::MapCoords& area, VillagerStates state);
/// State 225 BREEDER_DISCIPLE: pregnant, or Sleep its town's first desire -> GO_HOME. Else the counter goes down and,
/// once it has run out, a new wander about its spot. 1
uint32_t BreederDisciple(components::LivingAction& action);
/// The entry of 225: the breeder calls the villagers of the other sex to it (reaction 30, its turn stamped). 1
uint32_t EnterBreeder(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// The exit of 225: its call ends. 1
uint32_t ExitBreeder(components::LivingAction& action, VillagerStates next);
/// State 254 BREEDER_JUST_LANDED: the first villager of each of the 9 spiral cells about it, of an age and of the other
/// sex, the nearest -> they meet to make love; none -> DECIDE_WHAT_TO_DO. 1
uint32_t BreederJustLanded(components::LivingAction& action);
/// State 227 REACT_TO_BREEDER: the breeder it follows still a villager -> they meet to make love, 1; else 0
uint32_t ReactToBreeder(components::LivingAction& action);
/// Reaction 30's priority for a villager: the breeder a villager and not itself, the villager of an age and of the
/// other sex, the call followed by nobody yet, and the villager under no script -> the table's priority; else 0
[[nodiscard]] uint8_t ReactToBreederPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// Reaction 30's set-up: it follows the call (227) with the breeder as its object
void SetupReactToBreeder(entt::entity villager, entt::entity breeder, uint32_t reaction);
/// The villagers that follow a reaction now (its follower list)
[[nodiscard]] uint32_t FollowersOf(uint32_t reaction);

} // namespace openblack::ecs::villager
