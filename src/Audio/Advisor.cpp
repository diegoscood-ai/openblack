/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Advisor.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "Audio.h"
#include "Sound.h"
#include "Voices.h"
#include "WaveBuffers.h"

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::advisor;

namespace
{
/// The sound part of a HelpDude
struct Dude
{
	BankId bank {k_NoBank};   ///< +0x2EFC
	int samples {0};          ///< +0x2F00
	uint32_t startTick {0};   ///< +0x35D4: GetTickCount() at which the delayed sentence starts (0 = none)
	int sample {0};           ///< +0x35D8 (and the options' +0x24)
	uint32_t lastTalkTick {0}; ///< +0x37EC: the last GetTickCount() IsTalking / PercentageDone saw it talking
	float hover {0.0f};       ///< +0x3514 (0 after the init, 0x5C1A61; the flight is not ported)
	float time {0.0f};        ///< +0x2F70
	AutoVoiceParams params;   ///< +0x2F10
	VoiceKey key;             ///< +0x2F60
	bool active {false};      ///< HelpDudeControl+0x74 + 4 dude
	bool interrupted {false}; ///< HelpDudeControl+0x7C + 4 dude
};

struct State
{
	std::array<Dude, k_Dudes> dudes;
	int speaker {-1};       ///< g_speaker 0xD15AA0
	int sentence {0};       ///< g_sentence 0xD15A9C
	uint32_t startTick {0}; ///< 0xD15A98: GetTickCount() of the last PlaySample
	SentencePcm pcm;        ///< 0xD15A94 / 0xD15A8C / 0xD15A88 / 0xD15A90
	/// 0xD14084: the spectrum buffer of ApplyLipSync / fn_005BCC40 (2 x 0x200 floats)
	std::array<float, 2 * k_LipSyncWindow> spectrum {};
};
State g_State;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace && spdlog::get("audio") != nullptr;
}

Dude* Get(int dude)
{
	return dude >= 0 && dude < k_Dudes ? &g_State.dudes[static_cast<size_t>(dude)] : nullptr;
}

/// GetTickCount() - since (the subtractions of the original are 32-bit)
uint32_t Since(uint32_t since)
{
	return TickCount() - since;
}

/// HelpDude::PlaySample 0x5BB530: +0x164 = 1, LHSamplePlay (no GAudio filter); on a start the tick 0xD15A98, the PCM the
/// DLL kept (info+0x80, +0x84 bytes, copied) and its frames = bytes / (2 * nChannels), rate and duration
Channel PlaySample(const Dude& dude)
{
	sample_play::Options options;
	options.sound = SampleId(dude.bank, dude.sample); // +0x3680 bank, +0x36A0 sample (0x5BB39C..0x5BB3C0)
	options.is3D = false;                             // +0x3684 = 0
	options.owner = Owner::Key(k_OwnerAdvisor);       // +0x369C = 0x270C
	options.keepPcm = true;                           // +0x37E0 = 1, +0x164 again in PlaySample (0x5BB53A)
	// +0x36AC..+0x36B4 = the dude's point +0x3374 (a 2D channel does not use it; the flight is not ported)
	const Channel channel = sample_play::Start(options);
	if (channel == k_NoChannel)
	{
		return k_NoChannel;
	}
	g_State.startTick = TickCount(); // 0x5BB55A
	g_State.pcm = {};                // 0x5BB568: the previous copy freed
	if (const auto* sound = sample_play::GetSound(options.sound); sound != nullptr)
	{
		// (the DLL converts the wave to PCM with ACM when +0x164 is set; openblack decodes it once more here, the only
		// reader of that PCM being this copy)
		wave_buffers::Pcm pcm;
		if (wave_buffers::Decode(*sound, pcm))
		{
			const int channels = pcm.layout == ChannelLayout::Stereo ? 2 : 1;
			g_State.pcm.samples = std::move(pcm.samples);
			// 0x5BB5CC..0x5BB5FE: frames = bytes / (2 * nChannels), duration = frames / nSamplesPerSec
			g_State.pcm.frames = static_cast<int>(g_State.pcm.samples.size()) / channels;
			g_State.pcm.rate = pcm.sampleRate > 0 ? pcm.sampleRate : sound->sampleRate;
			g_State.pcm.duration = g_State.pcm.rate > 0 ? static_cast<float>(g_State.pcm.frames) /
			                                                  static_cast<float>(g_State.pcm.rate)
			                                            : 0.0f;
		}
	}
	return channel;
}

/// HelpDude::UpdateSaySentence 0x5BB610
void UpdateSaySentence(Dude& dude)
{
	if (dude.startTick == 0) // 0x5BB61C
	{
		return;
	}
	if (static_cast<int32_t>(TickCount()) < static_cast<int32_t>(dude.startTick)) // 0x5BB62B: jl
	{
		return;
	}
	dude.startTick = 0; // 0x5BB645
	const Channel channel = PlaySample(dude);
	if (channel == k_NoChannel)
	{
		g_State.sentence = 0; // 0x5BB6D2
		return;
	}
	// 0x5BB659: g_sentence = the channel's sample (+0x1C)
	g_State.sentence = dude.sample;
	// 0x5BB669..0x5BB71F: +0x2F0C = the sample and its AudioTags (BuildAudioTags 0x42AE70 on the wave's tags, info+0x78 /
	// +0x7C): they drive the advisor's gestures (not ported)
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Advisor: dude {} says HelpSprites {} ({:.2f} s)",
		                   static_cast<int>(&dude - g_State.dudes.data()), dude.sample, g_State.pcm.duration);
	}
}

/// HelpDude::ApplyLipSync 0x5BCD00(dt, ...): the sound part
void ApplyLipSync(int index, float dt)
{
	auto& dude = g_State.dudes[static_cast<size_t>(index)];
	if (!IsTalking(index) || g_State.sentence == 0) // 0x5BCD11..0x5BCD24
	{
		return;
	}
	// 0x5BCD2A..0x5BCD4E: the time since the start
	dude.time = static_cast<float>(static_cast<double>(Since(g_State.startTick)) * static_cast<double>(0.001f));
	const int64_t position = sample_play::PlayPosition(dude.bank, Owner::Key(k_OwnerAdvisor)); // 0x5BCD66
	if (position < 0)
	{
		// 0x5BCDE2..0x5BCDF7: no longer playing and more than half a second in (0x8AA3B4)
		if (dude.time > 0.5f)
		{
			StopSentence(index);
		}
		return; // fn_005BCBC0(time, 1): the mouth (not ported)
	}
	const float t = static_cast<float>(static_cast<double>(position) * static_cast<double>(0.001f)); // 0x5BCD78
	dude.time = t;
	if (g_State.sentence != 0 && !g_State.pcm.samples.empty()) // 0x5BCD88..0x5BCD98
	{
		CalcKey(dude.params, dude.key, dt, t, g_State.pcm.samples.data(), g_State.pcm.frames,
		        static_cast<float>(g_State.pcm.rate), g_State.spectrum.data(), k_LipSyncWindow);
		// fn_005BF810(&key): the mouth pose (not ported)
	}
}
} // namespace

// ---- AudioAnalyse and AutoVoiceParams ------------------------------------------------------------------------------

void advisor::Four1(float* data, int nn, int isign)
{
	// 0x428D50: data is used 1-based (edx = data - 4, 0x428D63)
	float* d = data - 1;
	const int n = nn << 1;
	int j = 1;
	for (int i = 1; i < n; i += 2) // bit reversal 0x428D7B..0x428DB5
	{
		if (j > i)
		{
			std::swap(d[j], d[i]);
			std::swap(d[j + 1], d[i + 1]);
		}
		int m = nn; // n >> 1
		while (m >= 2 && j > m)
		{
			j -= m;
			m >>= 1;
		}
		j += m;
	}
	int mmax = 2;
	while (n > mmax) // Danielson-Lanczos 0x428DCA..0x428ED4
	{
		const int istep = mmax << 1;
		const double theta = 6.28318530717959 / static_cast<double>(mmax * isign); // 0x8C49F8
		const double wtemp = std::sin(0.5 * theta);                                  // 0x8AB260
		const double wpr = -2.0 * wtemp * wtemp;                                     // 0x8C49F0
		const double wpi = std::sin(theta);
		double wr = 1.0;
		double wi = 0.0;
		for (int m = 1; m < mmax; m += 2)
		{
			for (int i = m; i <= n; i += istep)
			{
				const int k = i + mmax;
				const double tempr = wr * d[k] - wi * d[k + 1];
				const auto tempi = static_cast<float>(wr * d[k + 1] + wi * d[k]); // fstp [esp+0x14] (0x428E53)
				d[k] = static_cast<float>(d[i] - tempr);
				d[k + 1] = d[i + 1] - tempi;
				d[i] = static_cast<float>(tempr + d[i]);
				d[i + 1] = tempi + d[i + 1];
			}
			const double wrOld = wr;
			wr = wr * wpr - wi * wpi + wr;          // 0x428E85..0x428EAC
			wi = wi * (wpr + 1.0) + wpi * wrOld;     // 0x428EAE..0x428EBC (wi * wpr + wtemp * wpi + wi)
		}
		mmax = istep;
	}
}

void advisor::Analyse(const int16_t* pcm, float* out, int n)
{
	// 0x428C61..0x428C93: the window's step 1 / (n * 32768) (0x8C49EC) as a float; the window itself in the x87
	const float step = 1.0f / (static_cast<float>(n) * 32768.0f);
	double w = 0.0;
	const int half = n / 2; // cdq, sar (0x428C82..0x428C8B)
	int i = 0;
	for (; i < half; ++i) // 0x428CA0
	{
		w += step;
		out[2 * i] = static_cast<float>(static_cast<double>(pcm[i]) * w);
		out[2 * i + 1] = 0.0f;
	}
	for (; i < n; ++i) // 0x428CD7
	{
		w -= step;
		out[2 * i] = static_cast<float>(static_cast<double>(pcm[i]) * w);
		out[2 * i + 1] = 0.0f;
	}
	Four1(out, n, 1); // 0x428D04
	const auto count = static_cast<float>(n); // fst [esp+0xC] (0x428C72)
	for (i = 0; i < n; ++i) // 0x428D16..0x428D3A: in place, the magnitudes packed at the front
	{
		const double re = out[2 * i];
		const double im = out[2 * i + 1];
		out[i] = static_cast<float>(std::sqrt((re * re + im * im) / count));
	}
}

float advisor::BandLevel(const float* spectrum, int n, float rate, float low, float high)
{
	// fn_00428A80: ftol of (low / rate) * n and (high / rate) * n, each capped at n (0x428AAB..0x428AB5)
	int from = static_cast<int>(static_cast<double>(low / rate) * n);
	int to = static_cast<int>(static_cast<double>(high / rate) * n);
	from = std::min(from, n);
	to = std::min(to, n);
	if (from >= to) // 0x428ABF
	{
		return 0.0f;
	}
	double sum = 0.0;
	for (int i = from; i < to; ++i)
	{
		sum += spectrum[i];
	}
	return static_cast<float>(sum / (to - from)); // fidiv (0x428ADC)
}

void advisor::CalcKey(AutoVoiceParams& params, VoiceKey& key, float dt, float t, const int16_t* pcm, int frames,
                      float rate, float* spectrum, int window)
{
	key.time = t; // 0x428859
	// 0x428860..0x428874: the centre sample
	const auto centre = static_cast<int>((static_cast<double>(params.offsetMs) * static_cast<double>(0.001f) + dt + t) *
	                                     static_cast<double>(rate));
	int start = centre - window / 2; // 0x428886
	start = std::max(start, 0);      // 0x428888
	start = std::min(start, frames - window); // 0x428892
	// (defensive, not in the original: a sentence shorter than the window would be read before its PCM)
	if (start < 0 || frames < window)
	{
		return;
	}
	Analyse(pcm + start, spectrum, window); // 0x4288A6
	float total = 0.0f;
	for (size_t k = 0; k < params.bands.size(); ++k) // 0x4288C4..0x4288FA
	{
		const auto& band = params.bands[k];
		params.bandLevel[k] = BandLevel(spectrum, window, rate, band.low, band.high) * band.gain;
		total += params.bandLevel[k];
	}
	if (total < params.threshold) // 0x428900
	{
		total = 0.0f;
	}
	params.total = total;                // +0x4C (0x42891E)
	const float squared = total * total; // 0x42891A
	if (squared != 0.0f)                 // 0x42892D
	{
		for (auto& level : params.bandLevel)
		{
			level = level * level / squared;
		}
	}
	// 0x42894F..0x42898C: the loudest band
	size_t loudest = 2;
	const auto& b = params.bandLevel;
	if (b[0] > b[1] && b[0] > b[2])
	{
		loudest = 0;
	}
	else if (b[1] > b[2] && b[1] > b[0])
	{
		loudest = 1;
	}
	float level = params.level * params.total; // 0x428991
	if (level > 1.0f)                          // 0x4289A6
	{
		level = 1.0f;
	}
	float sum = 0.0f;
	for (size_t k = 0; k < params.bands.size(); ++k) // 0x4289C2..0x428A3C
	{
		auto& weight = key.weights[static_cast<size_t>(params.index[k])];
		const double ratio = static_cast<double>(params.bandLevel[k]) / static_cast<double>(b[loudest]);
		double v = ratio * ratio * level;
		const auto limit = static_cast<float>(static_cast<double>(dt) * params.rate * static_cast<double>(0.18f));
		const float previous = weight;
		if (previous - v > limit) // 0x4289F4..0x428A0B
		{
			v = previous - limit;
		}
		const float limitUp = limit + limit; // 0x428A0F
		if (v - previous > limitUp)          // 0x428A19..0x428A30
		{
			v = limitUp + previous;
		}
		weight = static_cast<float>(v); // 0x428A37
		sum += weight;
	}
	if (sum > 1.0f) // 0x428A3E
	{
		const float scale = 1.0f / sum;
		for (const int index : params.index)
		{
			key.weights[static_cast<size_t>(index)] *= scale;
		}
	}
}

// ---- HelpDudeControl / HelpDude --------------------------------------------------------------------------------------

void advisor::Init(BankId bank)
{
	for (auto& dude : g_State.dudes)
	{
		dude.bank = bank;                    // fn_005BB060 +0x2EFC
		dude.samples = BankSampleCount(bank); // +0x2F00 (LHBankGetNumberOfSamples 0x5BB0C1)
	}
}

void advisor::Reset()
{
	for (int i = 0; i < k_Dudes; ++i)
	{
		Stop(i);
	}
	const auto bank = g_State.dudes[0].bank;
	g_State = {};
	Init(bank);
}

void advisor::Say(int dude, int sample, bool onlyIfSilent)
{
	auto* d = Get(dude);
	if (d == nullptr)
	{
		return;
	}
	// 0x5C36DD..0x5C3723
	double v = std::fabs(static_cast<double>(d->hover)) - 0.949999988079071; // double 0x915438
	if (v < 0.0)
	{
		v = 0.0;
	}
	else
	{
		v = (v + 1.0) * 250.0; // 0x8AA390, 0x8C7B2C
		if (v > 500.0)         // 0x8C78EC
		{
			v = 500.0;
		}
	}
	SaySentence(dude, sample, onlyIfSilent, static_cast<uint32_t>(static_cast<int32_t>(v)));
	d->active = true; // 0x5C373B
}

void advisor::SaySentence(int dude, int sample, bool onlyIfSilent, uint32_t delayMs)
{
	auto* d = Get(dude);
	if (d == nullptr)
	{
		return;
	}
	g_State.speaker = dude; // 0x5BB343
	if (d->bank == k_NoBank) // 0x5BB351
	{
		return;
	}
	if (g_State.sentence != 0) // 0x5BB357
	{
		if (onlyIfSilent)
		{
			if (IsTalking(dude))
			{
				return;
			}
		}
		else
		{
			StopSentence(dude);
		}
	}
	// 0x5BB389..0x5BB39A: 1..LHBankGetNumberOfSamples
	if (sample < 1 || sample > d->samples)
	{
		return;
	}
	d->sample = sample;                    // 0x5BB3F0
	d->startTick = TickCount() + delayMs;  // 0x5BB3F6..0x5BB402
	UpdateSaySentence(*d);                 // 0x5BB408
}

void advisor::Update(float dt)
{
	for (int i = 0; i < k_Dudes; ++i)
	{
		UpdateSaySentence(g_State.dudes[static_cast<size_t>(i)]);
		ApplyLipSync(i, dt);
	}
}

bool advisor::IsTalking(int dude)
{
	auto* d = Get(dude);
	if (d == nullptr || g_State.speaker != dude) // 0x5BB768
	{
		return false;
	}
	bool talking = d->startTick != 0; // 0x5BB774
	if (!talking)
	{
		if (d->bank == k_NoBank || g_State.sentence == 0) // 0x5BB77E, 0x5BB788
		{
			return false;
		}
		// 0x5BB797: LHSampleIsPlaying(bank, 0x270C, g_sentence)
		talking = sample_play::IsPlaying(SampleId(d->bank, g_State.sentence), Owner::Key(k_OwnerAdvisor));
		if (!talking)
		{
			StopSentence(dude); // 0x5BB7B6
			return false;
		}
	}
	d->lastTalkTick = TickCount(); // 0x5BB7A7
	return true;
}

bool advisor::TalkingOrJustStopped(int dude)
{
	const auto* d = Get(dude);
	if (d == nullptr)
	{
		return false;
	}
	if (IsTalking(dude))
	{
		return true;
	}
	return Since(d->lastTalkTick) < 200; // 0x5BB749..0x5BB756: cmp 0xC8, unsigned
}

float advisor::PercentageDone(int dude)
{
	auto* d = Get(dude);
	if (d == nullptr || g_State.speaker != dude) // 0x5BB7C9
	{
		return 1.0f;
	}
	if (d->startTick != 0) // 0x5BB7D5
	{
		d->lastTalkTick = TickCount();
		return 0.0f;
	}
	if (d->bank == k_NoBank || g_State.sentence == 0) // 0x5BB7F4, 0x5BB7FE
	{
		return 1.0f;
	}
	const float done = sample_play::PercentageDone(SampleId(d->bank, g_State.sentence), Owner::Key(k_OwnerAdvisor));
	if (done < 1.0f) // 0x5BB817..0x5BB822
	{
		d->lastTalkTick = TickCount();
		return done;
	}
	return 1.0f;
}

void advisor::StopSentence(int dude)
{
	auto* d = Get(dude);
	if (d == nullptr)
	{
		return;
	}
	d->startTick = 0; // 0x5BB84E
	if (d->bank == k_NoBank || g_State.sentence == 0 || g_State.speaker != dude) // 0x5BB854..0x5BB866
	{
		return;
	}
	// 0x5BB875: LHSampleStop(bank, 0x270C, g_sentence)
	sample_play::Stop(SampleId(d->bank, g_State.sentence), Owner::Key(k_OwnerAdvisor));
	// 0x5BB87B..0x5BB88E: fn_005BCBC0(duration + 100, 0), the mouth (not ported)
	g_State.sentence = 0; // 0x5BB893
	g_State.speaker = -1; // 0x5BB89F (and +0x2F08 = 0, the audio tags)
}

void advisor::Stop(int dude)
{
	auto* d = Get(dude);
	if (d == nullptr)
	{
		return;
	}
	StopSentence(dude);     // 0x5C375C
	d->interrupted = false; // 0x5C3763
	d->active = false;      // 0x5C3767
}

bool advisor::Active(int dude)
{
	const auto* d = Get(dude);
	return d != nullptr && d->active;
}

void advisor::Interrupt(int dude, int arg)
{
	auto* d = Get(dude);
	if (d == nullptr || !d->active || !IsTalking(dude) || d->interrupted) // 0x5C378A..0x5C37AD
	{
		return;
	}
	const int spirit = dude + 1; // fn_005C3770
	int text = 0;
	bool interrupted = true;
	if (arg == 0 && tags::RandomSample(0, 2) == 0) // 0x5C37C2..0x5C37D2: LocalRand(2)
	{
		// fn_005C5290: HELP_TEXT_INTERRUPTION_06..13 (good) / 19..26 (evil)
		text = (spirit == 1 ? 0xE44 : 0xE51) + tags::RandomSample(0, 8);
		interrupted = false;
	}
	else
	{
		// fn_005C52C0: HELP_TEXT_INTERRUPTION_01..05 (good) / 14..18 (evil)
		text = (spirit == 1 ? 0xE3F : 0xE4C) + tags::RandomSample(0, 5);
	}
	if (text >= 0x1B3E) // 0x5C37EF
	{
		text = 0;
	}
	const int sample = static_cast<int>(voices::Table().Get(static_cast<uint32_t>(text)).sample); // 0x900D48
	Stop(dude);                                                                                   // 0x5C3805
	if (IsInsideCitadel()) // 0x5C3810: g_game+0x205A28 == 1
	{
		return;
	}
	if (PercentageDone(dude) < 0.9) // 0x5C381D..0x5C382D (double 0x915440)
	{
		Say(dude, sample, false);
		d->interrupted = interrupted; // 0x5C383A
	}
}

bool advisor::AnyTalking()
{
	// 0x5C6372..0x5C63A0
	return (Active(k_GoodSpirit) && TalkingOrJustStopped(k_GoodSpirit)) ||
	       (Active(k_EvilSpirit) && TalkingOrJustStopped(k_EvilSpirit));
}

VoiceKey advisor::LipSyncKey(int dude)
{
	const auto* d = Get(dude);
	return d != nullptr ? d->key : VoiceKey {};
}

float advisor::SentenceTime(int dude)
{
	const auto* d = Get(dude);
	return d != nullptr ? d->time : 0.0f;
}

float advisor::Amplitude(int sample, int window)
{
	// fn_005BB420
	if (sample == 0)
	{
		return 0.0f;
	}
	const auto t = static_cast<float>(static_cast<double>(Since(g_State.startTick)) * static_cast<double>(0.001f));
	if (t < 0.0f || !(t < g_State.pcm.duration) || g_State.pcm.samples.empty()) // 0x5BB456..0x5BB483
	{
		return 0.0f;
	}
	// 0x5BB497: LHSampleIsPlaying(GAudio+0x3C0 = HelpSprites, 0x270C, sample)
	if (!sample_play::IsPlaying(SampleId(Bank(SfxBank::HelpSprites), sample), Owner::Key(k_OwnerAdvisor)))
	{
		return 0.0f;
	}
	const int frames = g_State.pcm.frames;
	int from = static_cast<int>(static_cast<double>(t / g_State.pcm.duration) * frames); // 0x5BB4A9..0x5BB4B9
	int to = from + window;
	from = from > 0 ? std::min(from, frames) : 0; // 0x5BB4CA..0x5BB4D6
	to = to > 0 ? std::min(to, frames) : 0;       // 0x5BB4D8..0x5BB4E4
	double sum = 0.0;
	for (int i = from; i < to; ++i)
	{
		const double s =
		    static_cast<double>(g_State.pcm.samples[static_cast<size_t>(i)]) * static_cast<double>(3.0517578125e-05f);
		sum += s * s;
	}
	return static_cast<float>(std::sqrt(sum)); // 0x5BB51A
}

int advisor::Sentence()
{
	return g_State.sentence;
}

int advisor::Speaker()
{
	return g_State.speaker;
}

const SentencePcm& advisor::Pcm()
{
	return g_State.pcm;
}
