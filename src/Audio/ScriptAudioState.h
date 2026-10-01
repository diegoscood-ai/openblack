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

#include <atomic>

// The audio switches of GScript (g_game+0x250090), dev\tmp_dis\audio\script.md §2.7. The fields are atomic: the music
// thread writes the music line and the beats (the marker callback 0x426BA0) while the scripts read them.

namespace openblack::audio
{

struct ScriptAudioState
{
	/// +0x84: the creatures' sounds. SET_CREATURE_SOUND 0x710020 (0x71003D), END_DIALOGUE (=1, 0x71080A) and
	/// START_CAMERA_CONTROL (=1, 0x6ECE74) write it; fn_00483290+0x16A reads it (with 0, only the local player's
	/// creature sounds). 1 after Reset (0x6EB2F4).
	std::atomic<int32_t> creatureSound {1};
	/// +0x90: only the dialogue banks sound (SET_GAME_SOUND 0x7100B0: false -> 1 and LHSampleStopAll, true -> 0); read
	/// by GAudio::PlaySoundEffect 0x429F88 and SamplePlayAnimEffect 0x42A56E. 0 after Reset (0x6EB403). (Kept here for
	/// the reset; SET_GAME_SOUND is milestone B6.)
	std::atomic<int32_t> gameSoundOff {0};
	/// +0x94: the alignment music (ENABLE_DISABLE_ALIGNMENT_MUSIC 0x710120, 0x71013D); ProcessAlignmentMusic plays
	/// nothing with 0 (0x427A20). 1 after Reset (0x6EB409).
	std::atomic<int32_t> alignmentMusic {1};
	/// +0x98: the last "L<n>" marker of the script music (0x426BD1); START_MUSIC sets 0 (0x70FB56); LAST_MUSIC_LINE
	/// compares it unsigned (0x710098). 0 after Reset (0x6EB306).
	std::atomic<uint32_t> musicLine {0};
	/// +0x9C: the beats: 1 at an "L" marker (0x426BE5), +1 at "P"/"W" (0x426BFC); START_MUSIC (0x70FB6B) and
	/// END_DIALOGUE (0x710820) set 0. Read by fn_005CB590+0x62B (inferred: the text of a song follows it). 0 after
	/// Reset (0x6EB30C).
	std::atomic<int32_t> musicBeat {0};

	/// The audio part of GScript::Reset 0x6EB2D0
	void Reset();
	/// The audio part of END_DIALOGUE 0x710780 (0x7107FE..0x710820): +0x84 = 1, +0x9C = 0. The original does it only
	/// when the calling task owns the dialogue (HelpSystem+0x45CC == ScriptDLL::TaskNumber, 0x71078C..0x71079F);
	/// openblack has no dialogue owner yet, so CHLApi calls it for any task (approximated).
	void EndDialogue();
};

/// GScript's state of the running game (one, as g_game+0x250090)
ScriptAudioState& GetScriptAudioState();

} // namespace openblack::audio
