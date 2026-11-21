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

#include "Audio/Game/BankTables.h"

namespace openblack::audio
{

/// The sound effect banks a script names by number when it plays a sound: the game's bank types, numbered alike
using ScriptSoundBank = SfxBank;

/// What a bank's sample says about when it may be heard (the sample's user parameter in its bank header, of which only
/// the low 16 bits count, compared whole)
enum class SoundEffectUse : uint16_t
{
	Any = 0,
	/// Not heard while a script holds the widescreen bars
	QuietInCutscenes = 1,
	/// The only sounds heard while the camera is inside the temple
	HeardInTemple = 2,
	/// Not heard while the player controls a creature fight
	QuietInFights = 4,
};

/// The game state that decides whether a sound effect is heard
struct SoundEffectConditions
{
	/// The widescreen bars are up and a script owns them
	bool scriptWideScreen {false};
	/// The camera is inside the temple
	bool insideTemple {false};
	/// The scripts haven't turned the game's sound effects off
	bool gameSoundOn {true};
	/// The interface is in one of the creature fight's action states: the creature fight itself, applying a fight
	/// move or selecting one. Nothing in openblack sets those states yet, so this stays false in the game.
	bool creatureFightControl {false};
};

/// Whether a sound effect of this bank, with this user parameter, is heard in these conditions. With the game's sound
/// turned off by a script, only the advisors' and the villagers' speech still play.
[[nodiscard]] bool SoundEffectHeard(const SoundEffectConditions& conditions, ScriptSoundBank bank, uint16_t userParam);

} // namespace openblack::audio
