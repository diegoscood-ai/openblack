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
#include <string>

namespace openblack::ecs::components
{
/// Values of the game's global state that the map script sets or reads (only data so far)
struct MapScriptGlobals
{
	static constexpr size_t k_MagicCount = 42;

	/// VERSION; CREATE_FLOCK reads its town from N5 from 2.1 on, else from N4
	float version {0.0f};
	/// SET_LAND_NUMBER: 0 until a script sets it. A land's load keeps the last land's number until the new land's
	/// script sets its own
	int32_t landNumber {0};
	/// SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER: 1 at the game's start, and kept from the last
	/// land until the new land's script sets them (read by the towns' and the citadels' influence)
	float townInfluenceMultiplier {1.0f};
	float playerInfluenceMultiplier {1.0f};
	/// FIRE_FLY_SPELL_REWARD_PROB, by magic type (the first magic effect whose name matches without case; an unknown
	/// name gives 42 and is dropped). Not reset between lands.
	std::array<float, k_MagicCount> fireFlySpellRewardProbability {};
	/// The running sums of the table above, remade on every change
	std::array<float, k_MagicCount> fireFlySpellRewardCumulative {};
	/// The land's balances, all 1 unless its script sets them (SET_GLOBAL_LAND_BALANCE, by index): 4 the villagers'
	/// speed, 5 the wood value of trees, 7 the belief speed. Read, written and reset through the land balance service
	/// (land_balance::), back to 1 when a land loads
	std::array<float, 8> landBalance {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// How much of a town's boredom with each kind of reaction wears off at each of its turns, 1 unless the land's script
	/// sets it (SET_LOST_TOWN_SCALE). Through the land balance service too, and reset with landBalance
	float lostTownScale {1.0f};
	/// SET_INTERACT_DESIRE: added to a villager's interest in food and in its town's wants when it looks for something to
	/// take up; 0 until a feature script sets it, and back to 0 when a land loads
	float interactDesire {0.0f};
	/// The land's file as the script opened it (LOAD_LANDSCAPE), which a save writes at the start of its world file
	std::string landscapeFile;
};

/// What a land's load puts back of the map script's globals before the land's script runs, as the game does: only the
/// interact desire. The land number, the influence multipliers and the firefly rewards stay as the last land left them;
/// VERSION goes back to 0 when the land's script starts (LHScriptX's Script) and the land balances through
/// land_balance::Reset
constexpr void ResetForLandLoad(MapScriptGlobals& globals) noexcept
{
	globals.interactDesire = 0.0f;
}
} // namespace openblack::ecs::components
