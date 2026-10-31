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

/// The worship part of a Citadel, on the entity of the player's temple (components::Temple)
struct CitadelWorship
{
	static constexpr size_t k_Sites = 6;

	/// the six worship sites, indexed by the site's slot
	std::array<entt::entity, k_Sites> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	/// the CitadelHeart's Y angle: the slot angles start there
	float heartYAngle {0.0f};
	/// "cannot create worship sites" (= !SET_CAN_BUILD_WORSHIPSITE on the citadel)
	bool cannotCreateSites {false};
	/// the worship-strain sound fraction, moved toward the sites' largest strain
	float strainSoundFraction {0.0f};
};

/// A worship site (a citadel part). The entity is drawn
/// with the citadel heart's B_WORSHIP mesh at the citadel's origin, turned to its slot at
/// creation; its special points (the mesh's extra metrics) place the dance, the totem and the icons.
struct WorshipSite
{
	entt::entity citadel {entt::null};         ///< the Citadel
	PlayerNames player {PlayerNames::NEUTRAL}; ///< the citadel's player
	uint8_t infoIndex {0};                     ///< GWorshipSiteInfo = worshipSiteInfo[tribe]
	Tribe tribe {Tribe::NORSE};                ///< the tribe
	uint8_t slot {0};                          ///< the citadel slot 0..5
	float yAngle {0.0f};                       ///< heart angle + slot x 2pi/7

	std::vector<entt::entity> towns;   ///< the towns (newest first)
	std::vector<entt::entity> icons;   ///< the WorshipSpellIcons (a new one goes at the head)
	entt::entity totem {entt::null};   ///< WorshipTotem (the tribe's altar at special point 8)
	/// the altar's three need signs (ShowNeedsVisuals: hunger, rest, people hiding), made with the totem
	std::array<entt::entity, 3> needsSigns {entt::null, entt::null, entt::null};
	entt::entity foodPot {entt::null}; ///< the food pot

	/// its dance (components::Dance), the citadel dance of its slot about its dance centre, made with its totem: the
	/// dance holds its dancers, its state and speed, and those on their way to it. None without a totem
	entt::entity dance {entt::null};
	/// the villagers at the site (dancing and hiding)
	int32_t villagersAtSite {0};
	/// the list of those villagers (newest first)
	std::vector<entt::entity> villagers;
	/// the villagers requesting to go home, sorted by their desire for life
	std::vector<entt::entity> goHomeRequests;
	/// the food on its way to the site in a villager's arms (the original's resources on the way, for food): only
	/// taken off, by the supply errands; nothing adds to it
	int32_t foodOnTheWay {0};

	float battery {0.0f};         ///< the battery ("mana": GET_MANA / GAME_SET_MANA)
	float sacrificed {0.0f};      ///< the prayer power of all sacrificed at its altar (GET_SACRIFICE_TOTAL)
	float available {0.0f};       ///< chants available this turn (battery + capacity, set at the end of the turn)
	float used {0.0f};            ///< chants used this turn
	float requested {0.0f};       ///< chants requested this turn
	float chantDamage {0.0f};     ///< chants produced per dancer this turn (taken from the dancers' life)
	bool infiniteChants {false};  ///< (cheat)
	bool freeMaintenance {false}; ///< (cheat)
	bool iconTookChants {false};  ///< an icon took chants this turn
	float strain {0.0f};          ///< (demand - capacity) / capacity
	float strainPhase {0.0f};     ///< per frame
	float strainPulse {0.0f};
	/// the mana path: the effect the motes of prayer power its miracles spend run in (particles::manager id), made with
	/// the first mote, 0 none
	uint32_t manaPathEffect {0};
};

/// A worship totem (a citadel part): the tribe's altar (GWorshipSiteInfo.meshType, e.g. 101
/// BuildingCitadelNorseAltar) at the site's special point 8, a spell seed return point
struct WorshipTotem
{
	entt::entity site {entt::null}; ///< the worship site
};

/// A villager's link to the worship site it goes to or worships at (the villager's site is its town's)
struct WorshipVillager
{
	bool onWay {false};           ///< counted among the dance's villagers on their way
	bool onWayInTown {false};     ///< counted in its town's villagers on their way to the site
	bool requestedGoHome {false}; ///< in the site's go-home queue
};

} // namespace openblack::ecs::components
