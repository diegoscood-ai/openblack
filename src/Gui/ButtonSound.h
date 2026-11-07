/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Audio/Game/BankTables.h"

// The sound a dialog box's control makes as it acts. Wiki: docs/wiki/audio/creature-and-interface-sounds.md
namespace openblack::gui
{

/// What audio::PlaySoundEffect is asked to play, with no owner
struct ButtonSound
{
	int sample {0};
	int mode {0};
	int loops {0};
	bool extra3DFlag {false};
	bool is3D {false};
	audio::SfxBank bank {audio::SfxBank::None};
};

/// The menu button's click: sample 159 of the InGame bank, mode 3, once, in 2D
constexpr ButtonSound k_ButtonSound {
    .sample = 159,
    .mode = 3,
    .loops = 0,
    .extra3DFlag = false,
    .is3D = false,
    .bank = audio::SfxBank::InGame,
};

} // namespace openblack::gui
