/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// The names the help texts' spoken lines are reached by. Which sample says each text is our voice table
// (Audio/Services/Voices.h, audio::VoiceTable).

namespace openblack::audio
{

/// The sound banks the game's spoken lines are in
enum class SpeechBank : uint8_t
{
	/// The people of the story: the family of the opening, the trainer, Khazar, Lethys...
	Villagers,
	/// The advisors
	HelpSprites,
	/// The villagers' voices of guidance, awe and the like
	Guidance,
};

/// Who a script's spoken line is said by. The game keeps two, so that two people can speak at once and a script can
/// wait for either to finish.
enum class SpeechVoice : uint8_t
{
	First,
	Second,
};

} // namespace openblack::audio
