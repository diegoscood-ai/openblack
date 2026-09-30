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

#include "Enums.h"

// The player byte g_game +0x205A5B, read by the script, the magic objects and GameThing::GetPlayer. It is not the
// local player: GGame::SetupPlayers 0x550458 (and GGame::Load) write 7 there and nothing else writes it;
// GPlayer::IsNeutral 0x64AC00 is "this == g_game +0x18 + byte[0x205A5B] * 0xA60". So it is the neutral player's slot.

namespace openblack::magic
{
/// g_game +0x205A5B = 7 (GGame::SetupPlayers 0x550458): the neutral player, the owner of an ownerless object
inline constexpr PlayerNames k_NeutralPlayerSlot = PlayerNames::NEUTRAL;

/// GScript::ConvertScriptPlayerToGamePlayer 0x6EB9A0 + GGame::GetPlayer 0x5509B0: script player 0 is the player at
/// g_game +0x205A5B (the neutral one), n is game player n - 1; none out of 0..7 (GetPlayer returns nullptr from 8 on)
[[nodiscard]] inline bool ScriptPlayerToGamePlayer(int32_t script, PlayerNames& player)
{
	const int32_t game = script == 0 ? static_cast<int32_t>(k_NeutralPlayerSlot) : script - 1;
	if (game < 0 || game >= static_cast<int32_t>(PlayerNames::_COUNT))
	{
		return false;
	}
	player = static_cast<PlayerNames>(game);
	return true;
}
} // namespace openblack::magic
