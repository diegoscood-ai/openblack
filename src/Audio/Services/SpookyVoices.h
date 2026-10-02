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
#include <ctime>
#include <functional>
#include <string>
#include <string_view>

#include "Audio/Audio.h"

// GSpookyVoices (SpookyVoices.cpp, 0x72E130..0x72E8B0, milestone B10 of dev\tmp_dis\audio\PLAN.md): at night by the
// computer's clock, now and then, a voice of Guidance.sad whispers the player's name, picked once among the 100
// HELP_TEXT_SPOOKY_NAMES_* by a Soundex comparison with the profile's name. Disassembly:
// dev\tmp_dis\audio\spooky_72e130.txt; notes: docs/bw1-notes/audio.md (B10).
//
// The object is the static 0xDA0830: +0x8 the bank [0xDA0838], +0xC the options [0xDA083C], +0x10 the sample
// [0xDA0840], +0x14 a counter [0xDA0844], +0x18 a countdown [0xDA0848]. The GSpookyVoiceInfo of info.dat (5 entries,
// 0xDA0850) is never read by this code.

namespace openblack::audio::spooky
{

/// 0x999434..0x9995C4: the HELP_TEXT of the 100 names (HELP_TEXT_SPOOKY_NAMES_01..100 = 4586..4685)
inline constexpr uint32_t k_FirstName = 4586;
inline constexpr size_t k_Names = 100;

/// The computer's clock, time() in seconds (tests); unset: the real one
using ClockFn = std::function<int64_t()>;
void SetClock(ClockFn clock);
/// time() 0x7C79FD
[[nodiscard]] int64_t UnixTime();
/// localtime 0x7C789D of UnixTime
[[nodiscard]] std::tm LocalTime();
/// fn_0072E3B0: the local hour >= 23 or <= 5, or 20:45..20:59
[[nodiscard]] bool NightNow(const std::tm& time);

/// GSpookyVoices::SoundExCode 0x72E4E0(wchar_t*): not a letter (_isalpha 0x7C686F) 0; else by (c | 0x60) - 0x61 in
/// the jump table 0x72E54C: a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3, l 4, m n 5, r 6, and h w y the character
/// itself (the table's 0x72E548 returns eax, which holds it). (inferred) _isalpha in the "C" locale: ASCII letters.
[[nodiscard]] int SoundExCode(char16_t c);
/// GetNextSoundexCode 0x72E5C0(wchar_t*&): skips characters of code 0 up to the end or a space (0); after a coded
/// character, when the next one has the same code, the code + 1 is returned, again while the next one equals that
/// (the pointer is not moved past it: the W120 code)
[[nodiscard]] int GetNextSoundexCode(const char16_t*& p);
/// PerformSoundexComparison 0x72E630(text, word): the same first character (exact), not the end, then three
/// GetNextSoundexCode of each from the second character equal
[[nodiscard]] bool PerformSoundexComparison(const char16_t* text, const char16_t* word);
/// SoundexOverlap 0x72E6E0(text, name): one of the name's words (split at spaces) passes PerformSoundexComparison
[[nodiscard]] bool SoundexOverlap(const char16_t* text, const char16_t* name);
/// TrySoundex 0x72E7E0(name, &sample): for the 100 names in order, the first whose text (HelpTextDatabase[id], entry
/// 0 for an id out of 1..count-1) overlaps the name -> the sample of the voice table 0x984D48 (another copy of 0x915D40);
/// 0 when none or for an empty name
[[nodiscard]] uint32_t TrySoundex(std::u16string_view name);
/// The text of a HELP_TEXT for TrySoundex (tests); unset: helptext::GetEntry
using TextFn = std::function<std::u16string(uint32_t textId)>;
void SetNameTexts(TextFn texts);
/// GSpookyVoices::GetName 0x72E740: the profile's name ([0xD4BF38], GameQueries::profileName), then the network name
/// ([0xD204D4] + 0x70) and the registry's "Software\Microsoft\MS Setup (ACME)\User Info" DefName (LHLogR
/// RegistryRetrieveString): those two are not ported (openblack has no network login; the old Office setup key)
[[nodiscard]] uint32_t GetName();

/// GSpookyVoices::Init 0x72E2A0 (GGame::InitOneTimeOnly 0x54F024): new options (ctor defaults) with the bank Guidance
/// (GAudio+0x3D0), the sample = GetName, the counter 0, the countdown 100
void Init();
/// GSpookyVoices::GetPlayerName 0x72E870 (GGame::Init 0x54FD10): the sample = GetName again
void UpdatePlayerName();
/// fn_0072E280 (fn_0054EB40 0x54EC24): the options freed
void Shutdown();
/// GSpookyVoices::Process 0x72E310 (GGame::ProcessTurn 0x54E711): nothing on lands 1 and 2 (g_game+0x205A08) or without
/// a sample; the countdown runs down and at 0 it is 100 again (then 99); then, at night (NightNow), r = LocalFloatRand(1):
/// ftol((1 - r^3) 1000) (0x8AB228) < the counter -> PlaySpooky, else GGuidance fn_0071D0B0(1); the counter + 1
void Process();
/// fn_0072E3F0: a = LocalFloatRand(0.65), LocalRand(2) ? p = 1 + a^3 : 1 / (1 + a^3); +0x2C = LocalRand(180);
/// b = LocalFloatRand(0.8), the same for q; the options' sample, pitch ftol(100 p), volume ftol(volume x q) (the options
/// keep it: it builds up), 2D; GAudio::PlaySoundEffect 0x429E30; the counter = 0
void PlaySpooky();

struct State
{
	uint32_t sample {0};     ///< [0xDA0840]
	uint32_t counter {0};    ///< [0xDA0844]
	uint32_t countdown {0};  ///< [0xDA0848]
	bool initialised {false};
	sample_play::Options options; ///< [0xDA083C]
	BankId bank {k_NoBank};       ///< [0xDA0838]
	int field2C {90};             ///< its +0x2C: recorded only (not modelled by sample_play::Options)
};
[[nodiscard]] const State& GetState();
/// For the tests: the sample and the countdown set
void SetForTests(uint32_t sample, uint32_t counter, uint32_t countdown);

} // namespace openblack::audio::spooky
