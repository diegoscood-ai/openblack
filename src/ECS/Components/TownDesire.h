/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

namespace openblack::ecs::components
{

/// The number of things a town can want (TownDesireInfo)
inline constexpr size_t k_TownDesireCount = 17;

/// One entry of the town desire's two sorted orders (12 bytes as in the original)
struct DesireSort
{
	/// order 1: the scripts' boost + boost A (a float); order 2: boost A (copied as a dword)
	float boosts {0.0f};
	/// order 1 GetDesire, order 2 GetRawDesire; the comparator's only key
	float value {0.0f};
	/// the TownDesireInfo
	uint32_t index {0};
};

/// The town stats (added per villager and per abode in the original). Only the fields the desires read;
/// recomputed at the start of each town process by ecs::town_stats (the original keeps them incrementally)
struct TownStats
{
	uint32_t adults {0};
	uint32_t children {0};
	uint32_t abodesWithPlaces {0};             ///< Abodes with max villagers + max children != 0
	uint32_t civicBuildings {0};               ///< IsCivic
	uint32_t civicPlans {0};                   ///< The plans whose IsCivic (plans::IsCivic)
	uint32_t totalPlaces {0};                  ///< Sum of max villagers + max children of every abode
	uint32_t adultPlaces {0};                  ///< Sum of MaxVillagers of the abodes with places
	uint32_t childPlaces {0};                  ///< Sum of MaxChildren of the abodes with places
	int32_t freeAdultPlaces {0};               ///< MaxVillagers of the counted abodes - their adults
	uint32_t males {0};                        ///< Counted by sex for every villager, children too
	uint32_t females {0};                      ///< (ShuffleVillagersAroundAbodes reads them)
	std::array<uint8_t, 13> disciples {};      ///< NumDisciples[VillagerDisciple]
	float foodForDinner {0.0f};                ///< Sum of the villager infos' food required for dinner
	float foodCarried {0.0f};                  ///< Sum of the villagers' FOOD carried
	float woodCarried {0.0f};                  ///< Sum of the villagers' WOOD carried
	float woodAtSites {0.0f};                  ///< GetWoodForStats of the sites whose GetTown is the town
	std::array<uint8_t, 16> abodesByNumber {}; ///< Abodes per AbodeNumber
};

/// The town's desires. The town object is zero-filled at creation, so all start at 0. Four fields with no known use
/// are left out
struct TownDesire
{
	/// boost A. (inferred) nobody writes it in a new game (only the load)
	std::array<float, k_TownDesireCount> boostA {};
	/// the scripts' boost (SET_TOWN_DESIRE_BOOST, TOWN_DESIRE_BOOST)
	std::array<float, k_TownDesireCount> boost {};
	/// the desire, with the villagers' modification, in [-1, 1]
	std::array<float, k_TownDesireCount> desire {};
	/// adults + children - worshipping - on the way (unsigned), a float. No known reader
	float population {0.0f};
	/// the raw desire, function x TribeMultiplier, not clamped (CallDesireFunction)
	std::array<float, k_TownDesireCount> raw {};
	/// Amount / Desired (5, 6, 7 only; read only by the debug trace)
	std::array<float, k_TownDesireCount> amount {};
	std::array<float, k_TownDesireCount> desired {};
	/// order 1 (value GetDesire); GetSortedDesire = &sorted[k]
	std::array<DesireSort, k_TownDesireCount> sorted {};
	/// order 2 (value GetRawDesire), what audio reads
	std::array<DesireSort, k_TownDesireCount> sortedRaw {};
	/// turns above desireAffectsAlignmentAfter (the alignment by desires)
	std::array<int32_t, k_TownDesireCount> alignmentTurns {};
	/// doingNow / doingNowCount at the start of this turn's process
	std::array<float, k_TownDesireCount> doingNowAtStart {};
	std::array<float, k_TownDesireCount> doingNowCountAtStart {};
	/// the sum of +-amount (from the state table file) of the villagers' final states serving it
	/// (AdjustTownModifier); brought back to >= 0 by the desire process
	std::array<float, k_TownDesireCount> doingNow {};
	/// +-1 per state, a float as in the original
	std::array<float, k_TownDesireCount> doingNowCount {};
};

} // namespace openblack::ecs::components
