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
#include <cstdint>

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
/// GGuidance's fn_0071C460 tests it (0x71C574..0x71C5A6) before the heart beat plays
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

/// CitadelHeart::CreateBuiltWorshipSite 0x465110 (CREATE_WORSHIP_SITE): the tribe's site (FindOrCreateWorshipSite
/// 0x463220, no town check; none -> null); the first town of the player of that tribe (Town +0x5B8): AddTown if
/// missing, and when the town has a building site of it BuildBy(1.0) (vt +0x900) and RemoveBuildingSite; the site.
/// No such town: BuildBy(1.0) and null (0x465163..0x465174)
entt::entity CreateBuiltWorshipSite(entt::entity citadel, Tribe tribe);

/// fn_00464F50(citadel, boost) (CitadelHeart::Create at life >= 1, CitadelHeart::Built, SET_CAN_BUILD_WORSHIPSITE): for
/// each town of the player, FindOrCreateWorshipSite(town) 0x4631D0 and WorshipSite::AddTown when missing; a site not
/// built and repaired gets the town's building site (Town::AddBuildingSite 0x73B8E0 when it has none), +0x63C = boost
void OpenWorshipSites(entt::entity citadel, float boost = 0.0f);

/// fn_00463AD0 (GET_SPELL_ICON_IN_TEMPLE): the site icon of that magic in any of the sites
[[nodiscard]] entt::entity GetSpellIcon(entt::entity citadel, MagicType type);

/// Citadel +0x30: the citadel's heart (openblack: the citadel's own entity when it is a CitadelHeart), or entt::null
[[nodiscard]] entt::entity HeartOf(entt::entity citadel);
/// Citadel::Process 0x462D70 (GPlayer::Process 0x649525, every turn before the player's towns): the heart's 3D object
/// takes its GetPercentBuilt (LH3DCitadel::SetPercent 0x883120); reaching 1 it leaves and enters the map cells again.
/// (pending) the alignment colour and flock part (0x462D8F..0x462DE4, 0x462E55..0x463068)
void Process(entt::entity citadel);
/// LH3DCitadel::SetPercent 0x883120 (the clamp to 0..1, +0x9C) and its draw 0x882A40: below 1 the partly built temple
/// (fn_816AD0 with the inner walls 1.0 m in: physics::PartialBuild into components::DrawMesh), else the whole model
void SetHeartDrawPercent(entt::entity heart, float percent);
/// CitadelHeart::Built 0x465000 after MultiMapFixed::Built (abodes::Built): SetLife(1.0), the heart's building site out
/// of every town of the player, fn_464F50(citadel, 0), and for the local player outside a script's wide screen the
/// music 0x3D (no script music playing, not land 1) and SaveGameRoom::InstantSaveGame(0x14) (not ported)
void HeartBuilt(entt::entity heart);
/// WorshipSite::Built 0x77AC10 after MultiMapFixed::Built: the site's building site out of every town of its player
void WorshipSiteBuilt(entt::entity site);

/// SET_INTERFACE_CITADEL 414 (0x70B9A0): GScript +0xA0 = the popped value (raw). GScript::Reset 0x6EB312 sets it to 1
void SetInterfaceCitadel(uint32_t value);
[[nodiscard]] uint32_t InterfaceCitadel();
/// GScript::Reset 0x6EB2D0's +0xA0 = 1. (pending) openblack's caller of GScript::Reset (a new land's script)
void ResetInterfaceCitadel();
/// CitadelEntrance::InterfaceValidToTap 0x468F50: IsMultiplayerGame ? 1 : GScript +0xA0 != 0
[[nodiscard]] bool EntranceValidToTap(entt::entity entrance);
/// CitadelEntrance::InterfaceTap 0x468EF0: its heart (+0x54) of the local player and the tapping interface the local
/// one -> GGame::GoInsideCitadel(0, 0) 0x553E10 (openblack: the temple interior's Activate). Returns 1
uint32_t EntranceTap(entt::entity entrance, bool myInterface);

/// GPlayer::PostLoadCleanup 0x64AB90 (GSetup::LoadMapFeatures, after the land's script): for every player with a
/// citadel, every town of the player without a worship site -> Citadel::AddTown
void PostLoadCleanup();
} // namespace openblack::worship::citadel
