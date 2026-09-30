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

#include <array>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The worship part of a Citadel (sources.md §1.3), on the entity of the player's temple (components::Temple)
struct CitadelWorship
{
	static constexpr size_t k_Sites = 6;

	/// +0x34..+0x48 `WorshipSite* sites[6]`, indexed by the site's slot (WorshipSite +0x110)
	std::array<entt::entity, k_Sites> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	/// the CitadelHeart's Y angle (heart vt 0x508): the slot angles start there (Citadel::GetWorshipSiteAngle 0x463610)
	float heartYAngle {0.0f};
	/// +0x74 "cannot create worship sites" (= !SET_CAN_BUILD_WORSHIPSITE on the citadel)
	bool cannotCreateSites {false};
	/// +0x70 the worship-strain sound fraction, moved toward the sites' largest strain (SetWorshipStrainSoundFrac 0x463850)
	float strainSoundFraction {0.0f};
};

/// WorshipSite (a CitadelPart, 0x128 bytes; ctor fn_0077A960, chant fields zeroed by fn_0077AE60). The entity is drawn
/// with the citadel heart's B_WORSHIP mesh at the citadel's origin, turned to its slot (CallVirtualFunctionsForCreation
/// 0x77B9D0); its special points (the mesh's extra metrics) place the dance, the totem and the icons.
struct WorshipSite
{
	entt::entity citadel {entt::null};         ///< the Citadel (CitadelPart +0x..., vt 0x114 GetCitadel)
	PlayerNames player {PlayerNames::NEUTRAL}; ///< the citadel's player (vt 0x1C)
	uint8_t infoIndex {0};                     ///< +0x28 GWorshipSiteInfo = worshipSiteInfo[tribe]
	Tribe tribe {Tribe::NORSE};                ///< +0x8C the tribe
	uint8_t slot {0};                          ///< +0x110 the citadel slot 0..5
	float yAngle {0.0f};                       ///< heart angle + slot x 2pi/7

	std::vector<entt::entity> towns;  ///< +0xA4 / +0xA8 the towns (newest first)
	std::vector<entt::entity> icons;  ///< +0xE0 / +0xE4 the WorshipSpellIcons (WorshipSite::AddSpellIcon 0x77C430: at the head)
	entt::entity totem {entt::null};  ///< +0xDC WorshipTotem (the tribe's altar at special point 8)
	entt::entity foodPot {entt::null}; ///< +0xB4 the food pot (CreateFoodPot 0x77AD90)

	/// the dance (+0xA0, Dance of GDanceInfo[19 + slot], "CitadelDance_<slot+1>"): its members (Dance +0x90 = N) and
	/// state (+0x100: 0 still, 1 dancing), intensity (+0xF8 = k, fn_0050C340)
	std::vector<entt::entity> dancers;
	uint8_t danceState {0};
	float danceSpeed {0.0f};
	/// Dance +0x114: villagers on their way to the dance
	int32_t dancersOnWay {0};
	/// +0xC8 the villagers at the site (dancing and hiding)
	int32_t villagersAtSite {0};
	/// +0x120 / +0x124 the villagers requesting to go home, sorted by their desire for life (fn_0077E0C0)
	std::vector<entt::entity> goHomeRequests;

	float battery {0.0f};        ///< +0xF0 the battery ("mana": GET_MANA / GAME_SET_MANA)
	float available {0.0f};      ///< +0xF8 chants available this turn (battery + capacity, set at the end of the turn)
	float used {0.0f};           ///< +0xFC chants used this turn
	float requested {0.0f};      ///< +0x100 chants requested this turn
	float chantDamage {0.0f};    ///< +0x104 chants produced per dancer this turn (ReduceVillagerLifeByChant)
	bool infiniteChants {false}; ///< +0x108 (cheat fn_0077BC70)
	bool freeMaintenance {false}; ///< +0x10C (cheat)
	bool iconTookChants {false}; ///< +0x111 an icon took chants this turn
	float strain {0.0f};         ///< +0x114 (demand - capacity) / capacity
	float strainPhase {0.0f};    ///< +0x118 fn_0077B3B0 (per frame)
	float strainPulse {0.0f};    ///< +0x11C
};

/// WorshipTotem (a CitadelPart, 0x104 bytes; Create 0x780930): the tribe's altar (GWorshipSiteInfo.meshType, e.g. 101
/// BuildingCitadelNorseAltar) at the site's special point 8, a spell seed return point (IsSpellSeedReturnPoint)
struct WorshipTotem
{
	entt::entity site {entt::null}; ///< +0x100
};

/// A villager's link to the worship site it goes to or worships at (Villager::GetWorshipSite 0x76C340 is its town's
/// site; the flags are Villager +0xE0)
struct WorshipVillager
{
	bool atSite {false};          ///< +0xE0 & 0x2: counted at the site (AddVillagerToWorshipSite 0x76C3F0)
	bool onWay {false};           ///< +0xE0 & 0x10: counted in Dance +0x114 (GotoWorshipSiteForWorship 0x76BCC0)
	bool onWayInTown {false};     ///< in Town +0x5CC (AddVillagerOnWayToWorshipSite 0x73E300)
	bool dancing {false};         ///< in the dance group (GroupBehaviour::FindDanceGroup), else hiding
	bool requestedGoHome {false}; ///< +0x118: in the site's go-home queue (fn_0077E0C0)
	bool walking {false};         ///< (openblack) the WallHug walk to the state's point is under way
};

} // namespace openblack::ecs::components
