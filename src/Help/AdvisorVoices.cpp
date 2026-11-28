/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AdvisorVoices.h"

#include <cmath>

#include <algorithm>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "Debug/DebugEnv.h"

using namespace openblack;
using namespace openblack::help;

namespace
{
namespace lip_sync = openblack::audio::lip_sync;

bool Trace()
{
	static const bool k_Trace = debug_env::AudioTrace();
	return k_Trace && spdlog::get("audio") != nullptr;
}

/// A line whose play position is still below 0 this long after it started is stopped
constexpr float k_NotPlayingStop = 0.5f;
/// A stopping line's tags up to this long past its end are made
constexpr float k_FlushPastEnd = 100.0f;
} // namespace

AdvisorVoices::AdvisorVoices(Audio audio)
    : AdvisorVoices(std::move(audio), Hooks())
{
}

AdvisorVoices::AdvisorVoices(Audio audio, Hooks hooks)
    : _audio(std::move(audio))
    , _hooks(std::move(hooks))
{
}

AdvisorVoices::Voice* AdvisorVoices::Get(int advisor)
{
	return advisor >= 0 && advisor < k_Advisors ? &_dudes[static_cast<size_t>(advisor)] : nullptr;
}

const AdvisorVoices::Voice* AdvisorVoices::Get(int advisor) const
{
	return advisor >= 0 && advisor < k_Advisors ? &_dudes[static_cast<size_t>(advisor)] : nullptr;
}

void AdvisorVoices::KeepRecording(int advisor, int sample)
{
	_startTick = Now();
	_pcm = {}; // The previous copy freed
	// (the original's audio library keeps the converted PCM): the recording the channel's buffer was just made from
	std::optional<Recording> recording = _audio.recording ? _audio.recording(static_cast<uint32_t>(sample)) : std::nullopt;
	if (recording && recording->channels > 0)
	{
		_pcm.samples = std::move(recording->samples);
		// frames = bytes / (2 * nChannels), duration = frames / sample rate
		_pcm.frames = static_cast<int>(_pcm.samples.size()) / recording->channels;
		_pcm.rate = recording->sampleRate;
		_pcm.duration = _pcm.rate > 0 ? static_cast<float>(_pcm.frames) / static_cast<float>(_pcm.rate) : 0.0f;
	}
	// The wave's audio tags drive the advisor's gestures; a recording without tag data, or a label with a mistake in it,
	// leaves the line without them
	std::vector<spirits::AudioTag> tags;
	if (recording && recording->labels)
	{
		tags = spirits::BuildSentenceTags(*recording->labels, _tagWord);
	}
	if (_hooks.setTags)
	{
		_hooks.setTags(advisor, std::move(tags));
	}
}

void AdvisorVoices::UpdateSaySentence(Voice& advisor)
{
	if (advisor.startTick == 0)
	{
		return;
	}
	if (static_cast<int32_t>(Now()) < static_cast<int32_t>(advisor.startTick)) // Signed comparison
	{
		return;
	}
	advisor.startTick = 0;
	// Played directly (no sound effect filter), keeping its recording
	if (!(_audio.start && _audio.start(static_cast<uint32_t>(advisor.sample))))
	{
		_sentence = 0;
		return;
	}
	// The sentence is the channel's sample
	_sentence = advisor.sample;
	const int index = static_cast<int>(&advisor - _dudes.data());
	KeepRecording(index, advisor.sample);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Advisor: dude {} says HelpSprites {} ({:.2f} s)", index, advisor.sample,
		                   _pcm.duration);
	}
}

std::optional<spirits::LipSyncFrame> AdvisorVoices::ApplyLipSync(int index, float dt)
{
	auto* d = Get(index);
	if (d == nullptr || !IsTalking(index) || _sentence == 0)
	{
		return std::nullopt;
	}
	auto& dude = *d;
	spirits::LipSyncFrame frame;
	// The time since the start
	dude.time = static_cast<float>(static_cast<double>(Since(_startTick)) * static_cast<double>(0.001f));
	const int64_t position = _audio.playPositionMs ? _audio.playPositionMs(static_cast<uint32_t>(_sentence)) : -1;
	if (position < 0)
	{
		// No longer playing and more than half a second in
		if (dude.time > k_NotPlayingStop)
		{
			StopSentence(index);
		}
		// The tag walker runs on the clock's time (help::spirits)
		frame.time = dude.time;
		frame.weights = dude.key.weights;
		return frame;
	}
	const float t = static_cast<float>(static_cast<double>(position) * static_cast<double>(0.001f));
	dude.time = t;
	// The mouth poses and the tag walker run on t (help::spirits)
	frame.time = t;
	frame.playing = true;
	if (_sentence != 0 && !_pcm.samples.empty())
	{
		static_cast<void>(lip_sync::UpdateKey(lip_sync::Settings {}, dude.key, dt, t, _pcm.samples, _pcm.frames, _pcm.rate,
		                                      _spectrum, lip_sync::k_Window));
	}
	frame.weights = dude.key.weights;
	return frame;
}

// ---- AdvisorSpiritController / AdvisorSpirit ---------------------------------------------------------------------

void AdvisorVoices::Init()
{
	const auto count = static_cast<int>(_audio.lineCount ? _audio.lineCount() : 0);
	for (auto& dude : _dudes)
	{
		dude.samples = count;
	}
}

void AdvisorVoices::Reset()
{
	for (int i = 0; i < k_Advisors; ++i)
	{
		Stop(i);
	}
	_dudes = {};
	_speaker = -1;
	_sentence = 0;
	_startTick = 0;
	_pcm = {};
	_spectrum = {};
	Init();
}

void AdvisorVoices::SetHover(int advisor, float hover)
{
	if (auto* d = Get(advisor); d != nullptr)
	{
		d->hover = hover;
	}
}

uint32_t AdvisorVoices::SayDelayMs(float hoverX)
{
	// |hover| minus 0.95f (a float subtraction at the original's single precision), (v + 1) * 250 and the cap 500,
	// all floats
	float v = std::fabs(hoverX) - 0.95f;
	if (v < 0.0f)
	{
		v = 0.0f;
	}
	else
	{
		v = (v + 1.0f) * 250.0f;
		if (v > 500.0f)
		{
			v = 500.0f;
		}
	}
	return static_cast<uint32_t>(static_cast<int32_t>(v));
}

void AdvisorVoices::Say(int advisor, int sample, bool onlyIfSilent, float hoverX)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	SaySentence(advisor, sample, onlyIfSilent, SayDelayMs(hoverX));
	d->active = true;
}

void AdvisorVoices::Say(int advisor, int sample, bool onlyIfSilent)
{
	const auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	Say(advisor, sample, onlyIfSilent, d->hover);
}

void AdvisorVoices::SaySentence(int advisor, int sample, bool onlyIfSilent, uint32_t delayMs)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	_speaker = advisor;
	if (_sentence != 0)
	{
		if (onlyIfSilent)
		{
			if (IsTalking(advisor))
			{
				return;
			}
		}
		else
		{
			StopSentence(advisor);
		}
	}
	// 1..the bank's number of samples
	if (sample < 1 || sample > d->samples)
	{
		return;
	}
	d->sample = sample;
	d->startTick = Now() + delayMs;
	UpdateSaySentence(*d);
}

void AdvisorVoices::UpdateSaySentence(int advisor)
{
	if (auto* d = Get(advisor); d != nullptr)
	{
		UpdateSaySentence(*d);
	}
}

bool AdvisorVoices::IsTalking(int advisor)
{
	auto* d = Get(advisor);
	if (d == nullptr || _speaker != advisor)
	{
		return false;
	}
	bool talking = d->startTick != 0; // A start still waiting counts
	if (!talking)
	{
		if (_sentence == 0)
		{
			return false;
		}
		// Is the sentence still playing with the advisor's owner
		talking = _audio.isPlaying && _audio.isPlaying(static_cast<uint32_t>(_sentence));
		if (!talking)
		{
			StopSentence(advisor); // A sentence that ended is stopped
			return false;
		}
	}
	d->lastTalkTick = Now();
	return true;
}

bool AdvisorVoices::TalkingOrJustStopped(int advisor)
{
	const auto* d = Get(advisor);
	if (d == nullptr)
	{
		return false;
	}
	if (IsTalking(advisor))
	{
		return true;
	}
	return Since(d->lastTalkTick) < k_JustStoppedMs; // Unsigned, in ms
}

float AdvisorVoices::PercentageDone(int advisor)
{
	auto* d = Get(advisor);
	if (d == nullptr || _speaker != advisor)
	{
		return 1.0f;
	}
	if (d->startTick != 0)
	{
		d->lastTalkTick = Now();
		return 0.0f;
	}
	if (_sentence == 0)
	{
		return 1.0f;
	}
	const float done = _audio.percentageDone ? _audio.percentageDone(static_cast<uint32_t>(_sentence)) : 1.0f;
	if (done < 1.0f)
	{
		d->lastTalkTick = Now();
		return done;
	}
	return 1.0f;
}

void AdvisorVoices::StopSentence(int advisor)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	d->startTick = 0;
	if (_sentence == 0 || _speaker != advisor)
	{
		return;
	}
	// Stops the sentence playing with the advisor's owner
	if (_audio.stop)
	{
		_audio.stop(static_cast<uint32_t>(_sentence));
	}
	// The tag walker runs past the end to close the mouth
	if (_hooks.stopTags)
	{
		_hooks.stopTags(advisor, _pcm.duration + k_FlushPastEnd);
	}
	_sentence = 0;
	_speaker = -1; // (and the audio tags cleared)
}

void AdvisorVoices::Stop(int advisor)
{
	auto* d = Get(advisor);
	if (d == nullptr)
	{
		return;
	}
	StopSentence(advisor);
	d->interrupted = false;
	d->active = false;
}

bool AdvisorVoices::IsActive(int advisor) const
{
	const auto* d = Get(advisor);
	return d != nullptr && d->active;
}

void AdvisorVoices::Interrupt(int advisor, int arg)
{
	auto* d = Get(advisor);
	if (d == nullptr || !d->active || !IsTalking(advisor) || d->interrupted)
	{
		return;
	}
	const auto draw = [this](int32_t n) { return static_cast<int>(_audio.localRand ? _audio.localRand(n) : 0); };
	const int spirit = advisor + 1; // 1 good, 2 evil
	int text = 0;
	bool interrupted = true;
	if (arg == 0 && draw(2) == 0)
	{
		// HELP_TEXT_INTERRUPTION_06..13 (good) / 19..26 (evil)
		text = (spirit == 1 ? 0xE44 : 0xE51) + draw(8);
		interrupted = false;
	}
	else
	{
		// HELP_TEXT_INTERRUPTION_01..05 (good) / 14..18 (evil)
		text = (spirit == 1 ? 0xE3F : 0xE4C) + draw(5);
	}
	if (text >= 0x1B3E) // Past the last help text
	{
		text = 0;
	}
	// From the voice table
	const int sample = static_cast<int>(_audio.textLine ? _audio.textLine(static_cast<uint32_t>(text)) : 0);
	Stop(advisor);
	if (_audio.insideCitadel && _audio.insideCitadel())
	{
		return;
	}
	if (PercentageDone(advisor) < 0.9)
	{
		Say(advisor, sample, false);
		d->interrupted = interrupted;
	}
}

bool AdvisorVoices::AnyTalking()
{
	return (IsActive(k_GoodSpirit) && TalkingOrJustStopped(k_GoodSpirit)) ||
	       (IsActive(k_EvilSpirit) && TalkingOrJustStopped(k_EvilSpirit));
}

audio::lip_sync::Key AdvisorVoices::GetLipSyncKey(int advisor) const
{
	const auto* d = Get(advisor);
	return d != nullptr ? d->key : audio::lip_sync::Key {};
}

float AdvisorVoices::SentenceTime(int advisor) const
{
	const auto* d = Get(advisor);
	return d != nullptr ? d->time : 0.0f;
}

float AdvisorVoices::Amplitude(int sample, int window) const
{
	if (sample == 0)
	{
		return 0.0f;
	}
	const auto t = static_cast<float>(static_cast<double>(Since(_startTick)) * static_cast<double>(0.001f));
	if (t < 0.0f || !(t < _pcm.duration) || _pcm.samples.empty())
	{
		return 0.0f;
	}
	// The sample must be playing on the advisors' bank (HelpSprites) with the advisor's owner
	if (!(_audio.isPlaying && _audio.isPlaying(static_cast<uint32_t>(sample))))
	{
		return 0.0f;
	}
	const int frames = _pcm.frames;
	// Divide, multiply (rounded to a float), truncate
	int from = static_cast<int>(static_cast<float>(static_cast<double>(t / _pcm.duration) * frames));
	int to = from + window;
	from = from > 0 ? std::min(from, frames) : 0;
	to = to > 0 ? std::min(to, frames) : 0;
	float sum = 0.0f; // In float steps
	for (int i = from; i < to; ++i)
	{
		const float s = static_cast<float>(_pcm.samples[static_cast<size_t>(i)]) * 3.0517578125e-05f; // 1 / 32768
		sum += s * s;
	}
	return std::sqrt(sum);
}
