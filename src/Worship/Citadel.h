/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

// The citadel as the container of the worship sites (Citadel.cpp 0x463130..0x465200): up to 6 sites, one per tribe,
// around the heart at heart angle + slot x 2 pi / 7. The citadel entity is the temple (components::Temple).

namespace openblack::worship::citadel
{
/// GPlayer +0xA48: the player's citadel (its temple entity with a CitadelWorship), or entt::null
[[nodiscard]] entt::entity Of(PlayerNames player);

/// The citadel's worship part, from CitadelArchetype (the heart's Y angle from the script's rotation)
void Initialise(entt::entity temple, float heartYAngle);

/// Citadel::AddTown 0x463130: FindOrCreateWorshipSite(town), and WorshipSite::AddTown if it is not there yet
entt::entity AddTown(entt::entity citadel, entt::entity town);
/// Citadel::FindOrCreateWorshipSite(Town*) 0x4631D0: nothing when the citadel may not make sites (+0x74) or the town
/// may not have one (Town::IsAllowedToCreateWorshipSite); else the tribe's site
entt::entity FindOrCreateWorshipSite(entt::entity citadel, entt::entity town);
/// Citadel::FindOrCreateWorshipSite(GTribeInfo*) 0x463220: FindTribeWorshipSite 0x463190, else RequestANewWorshipSite
/// 0x4633F0 (the free slot nearest the nearest town of that tribe, else nearest the citadel)
entt::entity FindOrCreateWorshipSite(entt::entity citadel, Tribe tribe);
/// Citadel::FindTribeWorshipSite 0x463190
[[nodiscard]] entt::entity FindTribeWorshipSite(entt::entity citadel, Tribe tribe);

/// Citadel::ProcessSpellIcons 0x463920: every site's ProcessSpellIcons, then the strain sound fraction of the local
/// player's citadel toward the largest strain (SetWorshipStrainSoundFrac 0x463850: 0.001 x ms per turn)
void ProcessSpellIcons(entt::entity citadel);

/// CitadelHeart::CreateBuiltWorshipSite 0x465110 (CREATE_WORSHIP_SITE): the tribe's site (with no town check), and
/// the player's towns of that tribe added (their building site of it removed); else it is built (vt 0x900)
entt::entity CreateBuiltWorshipSite(entt::entity citadel, Tribe tribe);

/// fn_00464F50 (SET_CAN_BUILD_WORSHIPSITE on a citadel): for each town of the player, FindOrCreateWorshipSite(town)
/// and AddTown; an unbuilt site becomes a building site of the town (openblack's sites are built at once)
void OpenWorshipSites(entt::entity citadel);

/// fn_00463AD0 (GET_SPELL_ICON_IN_TEMPLE): the site icon of that magic in any of the sites
[[nodiscard]] entt::entity GetSpellIcon(entt::entity citadel, MagicType type);

/// GPlayer::PostLoadCleanup 0x64AB90 (GSetup::LoadMapFeatures, after the land's script): for every player with a
/// citadel, every town of the player without a worship site -> Citadel::AddTown
void PostLoadCleanup();
} // namespace openblack::worship::citadel
