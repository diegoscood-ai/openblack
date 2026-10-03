/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::map_coords
{
struct MapCoords;
} // namespace openblack::ecs::map_coords

// The citadel as the container of the worship sites (Citadel.cpp 0x463130..0x465200): up to 6 sites, one per tribe,
// around the heart at heart angle + slot x 2 pi / 7. The citadel entity is the temple (components::Temple).

namespace openblack::worship::citadel
{
/// GPlayer +0xA48: the player's citadel (its temple entity with a CitadelWorship), or entt::null
[[nodiscard]] entt::entity Of(PlayerNames player);

/// GPlayer +0xA48 -> Citadel +0x34..+0x48 (as GGuidance::CheckWorshipSiteDesiresSFX 0x71B2A5..0x71B301 walks them):
/// the six site slots in slot order, entt::null for an empty slot; all null when the player has no citadel
[[nodiscard]] std::array<entt::entity, 6> WorshipSitesOf(PlayerNames player);
/// Citadel +0x70, the worship-strain sound fraction: written only by SetWorshipStrainSoundFrac 0x463850 (from
/// ProcessSpellIcons 0x46396C, the local player's citadel), saved / loaded as 4 bytes (Save 0x463D6A, Load 0x463FB9).
/// 0 when it is not a citadel.
[[nodiscard]] float StrainSoundFraction(entt::entity citadel);
/// +0x70 as GGuidance::CheckWorshipSiteDesiresSFX reads it (0x71B319..0x71B332): below 1 (fcomp 0x8AA390) it is
/// kept (a NaN too: C0 set when unordered), else 1
[[nodiscard]] float StrainSoundFractionAtMostOne(entt::entity citadel);

/// fn_004639A0 (GAudio::ProcessChantMusic 0x4277DF: the camera's MapCoords, 100): of the six slots in order, the site
/// with dancers (fn_0077B960) whose dance centre (fn_0077CD90, special point 8) is nearest, GetDistanceInMetres
/// 0x74CD70 < best (strictly; best starts at maxDistance); entt::null for none or when it is not a citadel
[[nodiscard]] entt::entity FindNearestWorshipSite(entt::entity citadel, const ecs::map_coords::MapCoords& coords,
                                                  float maxDistance);

/// Citadel +0x30 (the CitadelHeart) with IsBuilt (vt +0x890, CitadelPart::IsBuilt) and GetLife (vt +0x11C) > 0, as
/// GGuidance's fn_0071C460 tests it (0x71C574..0x71C5A6) before the heart beat plays. (inferido) openblack's heart is
/// the temple itself, made built with it: a citadel with life (ecs::life::LifeOf of the temple)
[[nodiscard]] bool HasLivingHeart(entt::entity citadel);

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
