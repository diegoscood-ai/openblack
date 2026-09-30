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

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// Where miracles come from (M7; research dev\tmp_dis\miracles\sources.md, wiki docs/bw1-notes/magic.md): towns hold
// magic types, the player's citadel has a worship site per tribe with an icon per spell seed, villagers dance there and
// fill its battery with prayer power, the icons charge from it and put the seed into the hand. Plus the one-shot
// orbs' dispensers and the fireflies. This header is what Magic/MagicLoop.cpp and the hand call.
//
//   Worship/Citadel            the six sites around the heart, PostLoadCleanup
//   Worship/WorshipSite        the site, its battery and chant accounting, its icons' places
//   Worship/WorshipSpellIcon   charging, tapping, the seed into the hand, refunds
//   Worship/TownCentreSpellIcon the town centre's icons
//   Worship/TownMagic          the magic a town holds
//   Worship/WorshipPercentage  the totem drag, who goes to worship
//   Worship/SpellSeedGraphic   the floating seed over an icon or in an orb
//   Worship/PlayerSpellIcons   the player's icons (the gesture selection's queries)
//   Worship/SpellDispenser     the miracle dispensers
//   Worship/FireFlyReward      the fireflies' one-shots
//   ECS/Systems/Implementations/VillagerWorship  the villagers' worship states

namespace openblack::worship
{
/// A land is loaded (before its script)
void OnLoadMap();

/// Spell::ProcessSpells 0x720300's part: the SpellSeedGraphic list (fn_00727350) and GPlayer::ProcessSpellIcons
/// 0x64AEE0, before the maintain requests
void ProcessSpellIcons();

/// Once per game turn in GGame::ProcessTurn's players/dances slot: on the first turn GPlayer::PostLoadCleanup 0x64AB90
/// (the original runs it right after the land's script), the test hooks, then the spell dispensers (SpellDispenser::
/// Process 0x722A70, inf: the abodes' own process is the towns')
void ProcessTurn(uint32_t turn);

/// Per frame (game time): the PSys global phase, the icons' charge rings, the sites' strain pulse, the totems' rise
void Update(float seconds);

/// The hand's tap on an object (SpellIcon::InterfaceValidToTap / InterfaceTap, OneOffSpellSeed::InterfaceTap): worship
/// icons, town centre icons and one-shot orbs. 0 when the object is none of them or refused the tap.
[[nodiscard]] bool InterfaceValidToTap(entt::entity object, PlayerNames player);
int InterfaceTap(entt::entity object, PlayerNames player);

/// GInterface::PlaceObjectInMagicHand 0x5DA6F0's tail for any object the hand takes (the firefly on it, fn_0052B600)
void OnPlacedInMagicHand(entt::entity object);

/// SpellSeed::CanCast(Object, GPlayer) 0x729190's first branch: the object is a spell-seed return point of the player
/// (IsSpellSeedReturnPoint: a spell dispenser, a WorshipTotem or a spell icon), so a seed may always be given to it
[[nodiscard]] bool IsSeedReturnPoint(entt::entity object, PlayerNames player);

/// A spell seed put down, thrown or given (SpellSeed::RemoveFromHand 0x728F00 on a site's ground, ApplyToWorshipSite
/// 0x728B30 on a totem or an icon, fn_00728C80 on a dispenser): 3 when the seed went (its chants back in the battery),
/// 0 when the point / object is none of those
int ApplySeedToObject(entt::entity seed, entt::entity object);
int ApplySeedToPosition(entt::entity seed, const glm::vec3& position);
/// SpellSeed::ApplyToWorshipSite 0x729A80 (a forced throw, SpellSeed::ThrowObjectFromHand 0x72ACD0): the charge back to
/// its icon's site; the seed goes either way (3)
int ReturnSeedToItsSite(entt::entity seed);
/// SpellSeed::InterfaceSetOutMagicHand 0x728940's worship part: the seed's icon stops charging for that hand
/// (WorshipSpellIcon::CancelCharge 0x77F9A0) and the interface remembers the seed type (fn_005DCA20)
void OnSeedOutOfHand(entt::entity seed, PlayerNames player);

/// OPENBLACK_TEST_MANA / _WORSHIP / _TOWN_SPELL / _TAP_ICON / _DISPENSER / _FIREFLY_REWARD (WorshipDebugHooks.cpp)
void RunDebugHooks(uint32_t turn);
void ResetDebugHooks();
} // namespace openblack::worship
