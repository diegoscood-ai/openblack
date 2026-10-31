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

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Audio/LipSync.h"
#include "Help/SpiritVoice.h"

// The voice of the two advisors (docs/wiki/audio/voices.md#advisors-guidance-confirmations-and-night-voices): the sound part of
// AdvisorSpiritController (its dudes: 0 the good spirit, 1 the evil one) and of AdvisorSpirit. The help texts slot holds it
// (ecs::systems::HelpTextSystemInterface:: GetVoices) and gives it the game's sound and clock through Audio
// (HelpTextSystem::VoiceAudio). The visual part (the models, their flight, the mouth poses and the tag walker, both fed by
// ApplyLipSync from each spirit's update) is Help/Spirits.h and Help/SpiritsRuntime.h; the lip-sync analysis is
// Audio/LipSync.h; the tags come from the waves' cue labels (Audio/WaveCueLabels.h, read by the help texts slot). The advisors'
// sound effect anims are not ported.
//
// A sentence plays on HelpSprites (6) with the advisor's owner directly on the sample player: none of PlaySoundEffect's
// filters applies. Only one advisor speaks at a time: the speaker is the dude that last called SaySentence and the
// sentence the sample it plays. The delays are in milliseconds of the computer's clock, once a frame, not in game turns.

namespace openblack::help
{

class AdvisorVoices
{
public:
	static constexpr int k_Advisors = 2;
	static constexpr int k_GoodSpirit = 0;
	static constexpr int k_EvilSpirit = 1;
	/// How long after its line an advisor still counts as talking
	static constexpr uint32_t k_JustStoppedMs = 200;

	/// A line's recording as the voices keep it while it plays
	struct Recording
	{
		/// 16-bit samples, the channels interleaved
		std::vector<int16_t> samples;
		int channels {1};
		int sampleRate {0};
		/// The cue labels the gesture tags are read from; none when the recording has no tag data at all, which leaves
		/// the line without tags
		std::optional<std::vector<spirits::TagLabel>> labels;
	};

	/// What the voices need from the rest of the game; unset reads as the value next to each
	struct Audio
	{
		/// The advisors' bank's number of lines, read by Init. Unset: 0, so nothing is said
		std::function<uint32_t()> lineCount;
		/// Starts an advisor's line (1 to the line count), false when it can't be played. Unset: false
		std::function<bool(uint32_t line)> start;
		/// Whether the line is still playing with the advisors' owner. Unset: false
		std::function<bool(uint32_t line)> isPlaying;
		/// How far through its line it is, 0 to 1. Unset: 1
		std::function<float(uint32_t line)> percentageDone;
		/// Stops the line. Unset: nothing
		std::function<void(uint32_t line)> stop;
		/// The computer's clock in milliseconds. Unset: 0
		std::function<uint32_t()> tickMs;
		/// A number below n from the game's local random numbers (0 for n 0). Unset: 0
		std::function<uint32_t(int32_t n)> localRand;
		/// The recording of a line that has just started. Unset: none, so the mouth does not move and there are no tags
		std::function<std::optional<Recording>(uint32_t line)> recording;
		/// How far into the advisors' line the mixer is playing, in milliseconds, -1 when it is not playing. Unset: -1
		std::function<int64_t(uint32_t line)> playPositionMs;
		/// The line of the voice table that says a help text, 0 for none (Interrupt's pick). Unset: 0
		std::function<uint32_t(uint32_t text)> textLine;
		/// The camera is inside the citadel (Interrupt). Unset: false
		std::function<bool()> insideCitadel;
	};

	/// What the voices tell the advisors' bodies; unset does nothing
	struct Hooks
	{
		/// An advisor's line started with these gesture tags
		std::function<void(int advisor, std::vector<spirits::AudioTag> tags)> setTags;
		/// An advisor's line stopped: its tags before `before` seconds fire on it, then are dropped
		std::function<void(int advisor, float before)> stopTags;
	};

	explicit AdvisorVoices(Audio audio);
	AdvisorVoices(Audio audio, Hooks hooks);

	/// AdvisorSpiritController's init on both dudes (HelpSprites, at HelpSystem's creation): their bank's number of lines,
	/// read once here
	void Init();
	/// For the tests only: every sentence stopped and the dudes' state as their init leaves it. Not in the original:
	/// AdvisorSpiritController lives as long as HelpSystem (its reset does not touch it; only its uninit deletes it), and
	/// at a map change the audio reset stops every sample, ending the advisor's channel, after which IsTalking clears the
	/// sentence by itself. No game code calls it.
	void Reset();

	/// v = |hover| - 0.95 (0.95f); the delay is 0 for v < 0, else min((v + 1) * 250, 500) ms
	[[nodiscard]] static uint32_t SayDelayMs(float hoverX);
	/// SaySentence after the delay for `hoverX`; the advisor is active
	void Say(int advisor, int sample, bool onlyIfSilent, float hoverX);
	/// Say with the advisor's hover x, given once a frame by help::spirits through SetHover (0 until then)
	void Say(int advisor, int sample, bool onlyIfSilent);
	/// The hover x of the advisor's flight (Help/Spirits.h), for Say's delay
	void SetHover(int advisor, float hover);
	/// The advisor becomes the speaker; with a sentence playing, nothing if onlyIfSilent and IsTalking, else
	/// StopSentence; nothing for a sample outside 1..count; the sample starts at the clock + delay, then
	/// UpdateSaySentence
	void SaySentence(int advisor, int sample, bool onlyIfSilent, uint32_t delayMs);
	/// The advisor's line starts if its delay is over (the line played, the start tick and the line's recording kept);
	/// the advisors' update asks this once a frame for each advisor, at its point in the frame. Nothing for an index that
	/// is not an advisor
	void UpdateSaySentence(int advisor);
	/// Only the speaker; a start still waiting counts (and the last talk tick = now); else whether the sentence plays with
	/// the advisor's owner (and the last talk tick = now); a sentence that ended is stopped (StopSentence)
	[[nodiscard]] bool IsTalking(int advisor);
	/// IsTalking, or less than 200 ms since the last talk tick
	[[nodiscard]] bool TalkingOrJustStopped(int advisor);
	/// 0 while the start waits, the sentence's percentage done while < 1 (both refresh the last talk tick), else 1 (also
	/// for an advisor that is not the speaker)
	[[nodiscard]] float PercentageDone(int advisor);
	/// The delayed start cancelled; for the speaker with a sentence, the sample stopped (the 20 ms ramp), the line's tags
	/// up to 100 ms past its end let go, no sentence and no speaker
	void StopSentence(int advisor);
	/// StopSentence and the advisor's interrupted and active flags cleared
	void Stop(int advisor);
	/// The advisor was given a sentence by Say (until Stop)
	[[nodiscard]] bool IsActive(int advisor) const;
	/// On a click of the help interface (arg 1 from some callers): an active advisor that talks and was not interrupted
	/// yet picks an "interruption" text (arg 0 and a random 1 in 2: HELP_TEXT 0xE44 + rand 8 for the good one / 0xE51 +
	/// rand 8; else 0xE3F + rand 5 / 0xE4C + rand 5), its sample from the voice table, Stop(advisor), and outside the
	/// citadel, if PercentageDone < 0.9, Say(advisor, sample, 0) and marks it interrupted. (As the original:
	/// PercentageDone is read after the stop, which leaves no speaker, so it is 1 and the interruption is never said.)
	void Interrupt(int advisor, int arg);
	/// The advisors part of the dialogue's IsTextRead: either advisor active and TalkingOrJustStopped
	[[nodiscard]] bool AnyTalking();

	/// The sound part of the advisor's lip sync: nullopt unless it is the speaker (IsTalking) and a line has started.
	/// The line's time is the clock since it started until the mixer reports a play position (>= 0), then that position;
	/// a line whose position is still < 0 more than half a second in is stopped. While the position is >= 0, the mouth's
	/// key moves towards the shape of the recording at that time (lip_sync::UpdateKey with `dt`). The advisors' update
	/// asks this from each spirit's update that runs its animations, with the frame's real seconds.
	[[nodiscard]] std::optional<spirits::LipSyncFrame> ApplyLipSync(int advisor, float dt);
	/// The advisor's key after the last ApplyLipSync
	[[nodiscard]] audio::lip_sync::Key GetLipSyncKey(int advisor) const;
	/// The sentence's time in seconds as ApplyLipSync last set it
	[[nodiscard]] float SentenceTime(int advisor) const;
	/// sqrt of the sum of (s / 32768)^2 over `window` samples of the sentence from trunc(t / duration * frames),
	/// t = (clock - start) * 0.001; 0 for sample 0, t < 0 or past the duration, no recording or the sample not playing
	/// with the advisor's owner
	[[nodiscard]] float Amplitude(int sample, int window) const;
	/// The sample the speaker plays, 0 for none
	[[nodiscard]] int GetSentence() const { return _sentence; }
	/// The speaking dude, -1 for none
	[[nodiscard]] int GetSpeaker() const { return _speaker; }

private:
	/// The sound part of an AdvisorSpirit
	struct Voice
	{
		int samples {0};           ///< The bank's number of samples, read by Init
		uint32_t startTick {0};    ///< Tick at which the delayed sentence starts (0 = none)
		int sample {0};            ///< The sentence's sample
		uint32_t lastTalkTick {0}; ///< The last tick IsTalking / PercentageDone saw it talking
		float hover {0.0f};        ///< help::spirits' hover x (SetHover); 0 until then
		float time {0.0f};         ///< The sentence's time in seconds
		audio::lip_sync::Key key;  ///< The mouth's weights, which carry over from line to line
		bool active {false};       ///< Given a sentence by Say (until Stop)
		bool interrupted {false};  ///< Already interrupted once
	};

	/// The sentence's recording and its frames / rate / duration, kept until the next one starts
	struct Sentence
	{
		std::vector<int16_t> samples;
		int frames {0};
		int rate {0};
		float duration {0.0f};
	};

	[[nodiscard]] Voice* Get(int advisor);
	[[nodiscard]] const Voice* Get(int advisor) const;
	[[nodiscard]] uint32_t Now() const { return _audio.tickMs ? _audio.tickMs() : 0; }
	/// The time since `since` on the clock (32-bit wrapping subtraction, as the original)
	[[nodiscard]] uint32_t Since(uint32_t since) const { return Now() - since; }
	/// Starts the delayed sentence once its tick is reached
	void UpdateSaySentence(Voice& advisor);
	/// The start tick, a copy of the line's recording, its frames, rate and duration, and the speaker's tags
	void KeepRecording(int advisor, int sample);

	Audio _audio;
	Hooks _hooks;
	std::array<Voice, k_Advisors> _dudes {};
	int _speaker {-1};       ///< The dude that last called SaySentence
	int _sentence {0};       ///< The sample the speaker plays
	uint32_t _startTick {0}; ///< Tick of the last line's start
	Sentence _pcm;
	/// The tag parser's last word, which carries over from one label to the next
	std::string _tagWord;
	/// The spectrum buffer of ApplyLipSync (2 x 512 floats)
	std::array<float, 2 * audio::lip_sync::k_Window> _spectrum {};
};

} // namespace openblack::help
