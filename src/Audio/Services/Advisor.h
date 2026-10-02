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

#include "Audio/GAudio/AudioSystem.h"

// The voice of the two advisors (milestone B7 of dev\tmp_dis\audio\PLAN.md, dev\tmp_dis\audio\voices.md §2.5):
// HelpDudeControl (HelpSystem+0x10, its dudes at +4 / +8: 0 the good spirit, 1 the evil one) and the sound part of
// HelpDude (helpdude.cpp, 0x5BB060..0x5BB8A7, 0x5BCD00, 0x5C36D0..0x5C3842). The visual part (the models, their flight,
// the mouth poses fn_005BF810 / fn_005BCBC0, the anim effects of HelpDude::PlaySoundFX 0x5C2800) is not ported.
//
// A sentence plays on HelpSprites (6) with the owner 0x270C through LHSamplePlay directly (HelpDude::PlaySample
// 0x5BB530): none of GAudio::PlaySoundEffect's filters applies. Only one advisor speaks at a time: g_speaker 0xD15AA0
// is the dude that last called SaySentence and g_sentence 0xD15A9C the sample it plays. The delays are in milliseconds
// of GetTickCount (audio::TickCount), once a frame, not in game turns (UpdateSaySentence from HelpDude::Update1
// 0x5BDE41 and from the control's loop 0x5C3CB3).

namespace openblack::audio::advisor
{

/// HelpDudeControl's dudes (+4 good, +8 evil)
inline constexpr int k_Dudes = 2;
inline constexpr int k_GoodSpirit = 0;
inline constexpr int k_EvilSpirit = 1;
/// The window of the lip-sync analysis: 0x200 samples (ApplyLipSync 0x5BCDA6)
inline constexpr int k_LipSyncWindow = 0x200;

/// VoiceKey, HelpDude+0x2F60 (ctor fn_005C2D50): the time CalcKey was asked for and the three mouth weights it gives
struct VoiceKey
{
	float time {0.0f};                  ///< +0
	std::array<float, 3> weights {};    ///< +4..+0xC, by AutoVoiceParams::index
};

/// AutoVoiceParams, HelpDude+0x2F10, with the values HelpDude's init writes (0x5C1EA1..0x5C1F24)
struct AutoVoiceParams
{
	float offsetMs {0.0f};    ///< +0x00 (0x5C1EC8: 0): added to the time, in ms
	float threshold {0.05f};  ///< +0x04 (0x3D4CCCCD): a total band level below it is silence
	float level {2.5f};       ///< +0x08 (0x40200000): the total level's gain (capped at 1)
	float rate {40.0f};       ///< +0x0C (0x42200000): how fast a weight may move, x dt x 0.18 (0x8C49E8)
	struct Band
	{
		float low;  ///< Hz
		float high; ///< Hz
		float gain;
	};
	/// +0x10: 200..400 Hz x 0.85, 400..700 Hz x 1.1, 700..10000 Hz x 10 (0x43480000, 0x43C80000, 0x3F59999A,
	/// 0x43C80000, 0x442F0000, 0x3F8CCCCD, 0x442F0000, 0x461C4000, 0x41200000)
	std::array<Band, 3> bands {{{200.0f, 400.0f, 0.85f}, {400.0f, 700.0f, 1.1f}, {700.0f, 10000.0f, 10.0f}}};
	/// +0x34: which weight of the key each band drives (0, 1, 2)
	std::array<int, 3> index {0, 1, 2};
	/// +0x40: the bands' levels of the last CalcKey (normalised); +0x4C the total
	std::array<float, 3> bandLevel {};
	float total {0.0f};
};

/// AudioAnalyse::four1 0x428D50: Numerical Recipes' complex FFT in place on `data` (re, im pairs; nn points, a power of
/// two), isign 1 or -1. The game's FPU is at 24 bits (fn_007DEE00, `and cw, 0xFCFF` at 0x7DEE0D): every arithmetic step
/// rounds to a float (the doubles 2 pi 0x8C49F8, 0.5, -2, 1, 0 are applied exactly), only fsin keeps its full precision
/// (sin(theta) stays unrounded on the FPU). (approximated) openblack's sin is the double one, not the x87's 64-bit.
void Four1(float* data, int nn, int isign);
/// AudioAnalyse::Analyse 0x428C60(pcm, out, n): the n samples under a triangle window that rises by 1 / (n * 32768) a
/// sample to the middle and falls back (int16 -> -1..1 x the window), as complex numbers; four1(out, n, 1); then
/// out[i] = sqrt((re^2 + im^2) / n) for i < n. `out` holds 2n floats.
void Analyse(const int16_t* pcm, float* out, int n);
/// fn_00428A80(spectrum, n, rate, low, high): the mean of spectrum[ftol(low / rate * n) .. ftol(high / rate * n)), both
/// capped at n; 0 when empty
[[nodiscard]] float BandLevel(const float* spectrum, int n, float rate, float low, float high);
/// AutoVoiceParams::CalcKey 0x428850(key, dt, t, pcm, frames, rate, spectrum, window): the window of PCM centred at
/// (offsetMs * 0.001 + dt + t) * rate, its three bands, and the key's weights moved towards the loudest band's shape at
/// most dt * rate * 0.18 down / twice that up a call, normalised when they add up past 1. (As the original: a window of
/// pure silence divides 0 by 0, the weights go NaN.)
void CalcKey(AutoVoiceParams& params, VoiceKey& key, float dt, float t, const int16_t* pcm, int frames, float rate,
             float* spectrum, int window);

/// HelpDudeControl::Init (HelpSystem::CallVirtualFunctionsForCreation 0x5C5860 -> fn_005C3660(HelpSprites, HelpSprites)
/// -> fn_005BB060 on both dudes): their bank (+0x2EFC) and its number of samples (+0x2F00)
void Init(BankId bank);
/// For the tests only: every sentence stopped and the dudes' state as their init leaves it. Not in the original:
/// HelpDudeControl lives as long as HelpSystem (HelpSystem::Reset 0x5C5580 does not touch it; only Uninit 0x5C5680
/// deletes it), and at a map change GAudio::Reset's LHSampleStopAll ends the advisor's channel, after which IsTalking
/// clears the sentence by itself (0x5BB7B6). No game code calls it.
void Reset();

/// HelpDudeControl::Say fn_005C36D0(dude, sample, onlyIfSilent): v = |dude+0x3514| - 0.95 (double 0x915438 = 0.95f); the delay
/// is 0 for v < 0, else min((v + 1) * 250, 500) ms (0x8C7B2C, 0x8C78EC); HelpDude::SaySentence; +0x74 + 4 dude = 1
/// (approximated: +0x3514 belongs to the advisor's flight, which is not ported; it stays at its init value 0,
/// 0x5C1A61, so the delay is 0)
void Say(int dude, int sample, bool onlyIfSilent);
/// HelpDude::SaySentence fn_005BB340(sample, onlyIfSilent, delayMs): g_speaker = the dude; with a sentence playing,
/// nothing if onlyIfSilent and IsTalking, else StopSentence; nothing for a sample outside 1..count; the options +0x367C
/// (bank, 2D, owner 0x270C, the sample, +0x164 = 1), +0x35D8 = sample, +0x35D4 = GetTickCount() + delay, then
/// UpdateSaySentence
void SaySentence(int dude, int sample, bool onlyIfSilent, uint32_t delayMs);
/// Once a frame (HelpDudeControl's loop 0x5C3B05..0x5C3CBC -> HelpDude::Update1 0x5BDDA0): UpdateSaySentence 0x5BB610
/// (the delayed start: HelpDude::PlaySample 0x5BB530 = LHSamplePlay with +0x164 = 1, the start tick 0xD15A98 and the
/// sentence's PCM 0xD15A94 / frames 0xD15A8C / rate 0xD15A88 / duration 0xD15A90; g_sentence = the channel's sample)
/// and ApplyLipSync 0x5BCD00(dt) of each dude. `dt`: the frame's seconds.
void Update(float dt);
/// HelpDude::IsTalking 0x5BB760: only the speaker; a start still waiting counts (and +0x37EC = now); else
/// LHSampleIsPlaying(bank, 0x270C, g_sentence) (and +0x37EC = now); a sentence that ended is stopped (StopSentence)
[[nodiscard]] bool IsTalking(int dude);
/// fn_005BB730: IsTalking, or GetTickCount() - +0x37EC < 200 ms (0x5BB74F)
[[nodiscard]] bool TalkingOrJustStopped(int dude);
/// fn_005BB7C0: 0 while the start waits, LHSampleGetPercentageDone(bank, 0x270C, g_sentence) while < 1 (both refresh
/// +0x37EC), else 1 (also for a dude that is not the speaker)
[[nodiscard]] float PercentageDone(int dude);
/// HelpDude::StopSentence 0x5BB840: +0x35D4 = 0; for the speaker with a sentence, LHSampleStop(bank, 0x270C,
/// g_sentence) (the 20 ms ramp), g_sentence = 0, no speaker
void StopSentence(int dude);
/// fn_005C3750(dude): StopSentence and the control's +0x7C / +0x74 of the dude cleared
void Stop(int dude);
/// HelpDudeControl+0x74 + 4 dude: the dude was given a sentence by Say (until Stop)
[[nodiscard]] bool Active(int dude);
/// HelpDudeControl fn_005C3780(dude, arg) (from fn_005C6720 -> fn_005C4C20: the click of HelpSystem::ProcessInterface
/// 0x5C6A88 / 0x5C6A93 and fn_005C6800 0x5C6856 / 0x5C6861 with arg 1): an active dude that talks and was not
/// interrupted yet (+0x7C) picks an "interruption" text (arg 0 and LocalRand(2) == 0: fn_005C5290, HELP_TEXT 0xE44 +
/// rand 8 for the good one / 0xE51 + rand 8; else fn_005C52C0, 0xE3F + rand 5 / 0xE4C + rand 5), its sample from the
/// voice table (0x900D48), Stop(dude), and outside the citadel, if PercentageDone < 0.9 (double 0x915440), Say(dude,
/// sample, 0) and +0x7C. (As in W120: PercentageDone is read after the stop, which leaves no speaker, so it is 1 and the
/// interruption is never said.)
void Interrupt(int dude, int arg);
/// The advisors part of HelpSystem::IsTextRead (0x5C6372..0x5C63A0): +0x74 && fn_005BB730(dude 0) || +0x78 &&
/// fn_005BB730(dude 1)
[[nodiscard]] bool AnyTalking();
/// HelpDude+0x2F60 after the last ApplyLipSync (CalcKey 0x428850 on the PCM kept by PlaySample)
[[nodiscard]] VoiceKey LipSyncKey(int dude);
/// HelpDude+0x2F70: the sentence's time in seconds as ApplyLipSync last set it
[[nodiscard]] float SentenceTime(int dude);
/// fn_005BB420(audio, sample, window): sqrt of the sum of (s / 32768)^2 (3.0517578e-5, 0x8C5848) over `window` PCM
/// samples of the sentence from ftol(t / duration * frames), t = (GetTickCount() - start) * 0.001 (0x8AC418); 0 for
/// sample 0, t < 0 or past the duration, no PCM or the sample not playing on HelpSprites with 0x270C
[[nodiscard]] float Amplitude(int sample, int window);
/// The sample the speaker plays (g_sentence 0xD15A9C), 0 for none
[[nodiscard]] int Sentence();
/// The speaking dude (g_speaker 0xD15AA0), -1 for none
[[nodiscard]] int Speaker();

/// The PCM of the sentence (0xD15A94) and its frames / rate (0xD15A8C / 0xD15A88), for the tests
struct SentencePcm
{
	std::vector<int16_t> samples;
	int frames {0};
	int rate {0};
	float duration {0.0f};
};
[[nodiscard]] const SentencePcm& Pcm();

} // namespace openblack::audio::advisor
