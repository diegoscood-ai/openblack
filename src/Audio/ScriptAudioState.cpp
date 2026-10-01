/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptAudioState.h"

namespace openblack::audio
{

void ScriptAudioState::Reset()
{
	creatureSound = 1;  // 0x6EB2F4 (edi = 1)
	musicLine = 0;      // 0x6EB306 (ebx = 0)
	musicBeat = 0;      // 0x6EB30C
	gameSoundOff = 0;   // 0x6EB403
	alignmentMusic = 1; // 0x6EB409
}

void ScriptAudioState::EndDialogue()
{
	creatureSound = 1; // 0x71080A
	musicBeat = 0;     // 0x710820
}

ScriptAudioState& GetScriptAudioState()
{
	static ScriptAudioState state;
	return state;
}

} // namespace openblack::audio
