/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpookyVoices.h"

#include <cstdlib>

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/GAudio/AudioSystem.h"
#include "Audio/GameQueries.h"
#include "Audio/Services/Guidance.h"
#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

// Every function cites its original in SpookyVoices.h. Disassembly: dev\tmp_dis\audio\spooky_72e130.txt.

using namespace openblack;
using namespace openblack::audio;
using namespace openblack::audio::spooky;

namespace
{
State g_Spooky;
ClockFn g_Clock;
TextFn g_Texts;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_GUIDANCE_TRACE") != nullptr;
	return k_Trace;
}

/// _isalpha in the "C" locale (inferred): the ASCII letters
bool IsAlpha(char16_t c)
{
	return (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z');
}
} // namespace

void spooky::SetNameTexts(TextFn texts)
{
	g_Texts = std::move(texts);
}

void spooky::SetClock(ClockFn clock)
{
	g_Clock = std::move(clock);
}

int64_t spooky::UnixTime()
{
	return g_Clock ? g_Clock() : static_cast<int64_t>(std::time(nullptr));
}

std::tm spooky::LocalTime()
{
	const auto now = static_cast<std::time_t>(UnixTime());
	std::tm local {};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return local;
}

bool spooky::NightNow(const std::tm& time)
{
	// 0x72E3C5..0x72E3E9: tm_hour >= 23 (0x17) or <= 5 -> 1; < 20 (0x14) -> 0; tm_min < 45 (0x2D) -> 0; < 21 -> 1
	const int hour = time.tm_hour;
	if (hour >= 23 || hour <= 5)
	{
		return true;
	}
	if (hour < 20 || time.tm_min < 45)
	{
		return false;
	}
	return hour < 21;
}

int spooky::SoundExCode(char16_t c)
{
	if (!IsAlpha(c))
	{
		return 0; // 0x72E4F5
	}
	const auto index = static_cast<uint32_t>((c | 0x60) - 0x61);
	// 0x72E54C: a b c d e f g h i j k l m n o p q r s t u v w x y z ('h', 'w', 'y': 0x72E548, the character)
	constexpr std::array<int, 26> k_Codes {0, 1, 2, 3, 0, 1, 2, -1, 0, 2, 2, 4, 5, 5, 0, 1, 2, 6, 2, 3, 0, 1, -1, 2, -1, 2};
	if (index > 25)
	{
		return static_cast<int>(c); // 0x72E507: ja 0x72E548
	}
	const int code = k_Codes.at(index);
	return code < 0 ? static_cast<int>(c) : code;
}

int spooky::GetNextSoundexCode(const char16_t*& p)
{
	int code = 0;
	// 0x72E5CB..0x72E5F4
	for (;;)
	{
		const char16_t c = *p;
		if (c == 0 || c == u' ')
		{
			return 0;
		}
		const char16_t* at = p;
		++p;
		code = SoundExCode(*at);
		if (code != 0)
		{
			break;
		}
	}
	// 0x72E5FE..0x72E619: the next character (not passed)
	if (SoundExCode(*p) != code)
	{
		return code;
	}
	do
	{
		++code;
	} while (SoundExCode(*p) == code);
	return code;
}

bool spooky::PerformSoundexComparison(const char16_t* text, const char16_t* word)
{
	if (text[0] != word[0] || text[0] == 0) // 0x72E63E..0x72E64E
	{
		return false;
	}
	const char16_t* a = text + 1; // 0x72E657..0x72E670
	const char16_t* b = word + 1;
	for (int i = 0; i < 3; ++i)
	{
		const int ca = GetNextSoundexCode(a);
		const int cb = GetNextSoundexCode(b);
		if (ca != cb)
		{
			return false;
		}
	}
	return true;
}

bool spooky::SoundexOverlap(const char16_t* text, const char16_t* name)
{
	if (*name == 0)
	{
		return false; // 0x72E6E6
	}
	for (;;)
	{
		if (PerformSoundexComparison(text, name))
		{
			return true;
		}
		// 0x72E700..0x72E727: to the next space (or the end), past it
		while (*name != 0 && *name != u' ')
		{
			++name;
		}
		if (*name == 0)
		{
			return false;
		}
		++name;
		if (*name == 0)
		{
			return false;
		}
	}
}

uint32_t spooky::TrySoundex(std::u16string_view name)
{
	if (name.empty() || name.front() == 0)
	{
		return 0; // 0x72E7E5..0x72E7F2
	}
	const std::u16string nameText(name); // the original reads a 0-terminated wide string
	for (size_t k = 0; k < k_Names; ++k)
	{
		const auto id = static_cast<uint32_t>(k_FirstName + k);
		// 0x72E7FB..0x72E81C: HelpTextDatabase[id] (entry 0 for an id out of 1..count-1: helptext::GetEntry's rule)
		const std::u16string text = g_Texts ? g_Texts(id) : helptext::GetEntry(id).text;
		if (SoundexOverlap(text.c_str(), nameText.c_str()))
		{
			return voices::Table().Get(id).sample; // 0x72E841..0x72E850: [0x984D48 + 12 id]
		}
	}
	return 0;
}

uint32_t spooky::GetName()
{
	const auto& queries = Queries();
	const auto profile = queries.profileName ? queries.profileName() : std::u16string();
	if (const uint32_t sample = TrySoundex(profile); sample != 0)
	{
		return sample; // 0x72E759
	}
	// 0x72E762..0x72E7C8: [0xD204D4] + 0x70 (the network login) and the registry's DefName: not ported
	return 0;
}

void spooky::Init()
{
	// 0x72E2A0..0x72E306
	g_Spooky.bank = Bank(SfxBank::Guidance);
	g_Spooky.options = sample_play::Options {};
	g_Spooky.field2C = 90;
	g_Spooky.sample = GetName();
	g_Spooky.counter = 0;
	g_Spooky.countdown = 100; // 0x64
	g_Spooky.initialised = true;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SpookyVoices: Init, name sample {}", g_Spooky.sample);
	}
}

void spooky::UpdatePlayerName()
{
	g_Spooky.sample = GetName();
}

void spooky::Shutdown()
{
	g_Spooky.initialised = false;
}

const State& spooky::GetState()
{
	return g_Spooky;
}

void spooky::SetForTests(uint32_t sample, uint32_t counter, uint32_t countdown)
{
	g_Spooky.bank = Bank(SfxBank::Guidance);
	g_Spooky.options = sample_play::Options {};
	g_Spooky.field2C = 90;
	g_Spooky.sample = sample;
	g_Spooky.counter = counter;
	g_Spooky.countdown = countdown;
	g_Spooky.initialised = true;
}

void spooky::Process()
{
	if (!g_Spooky.initialised)
	{
		return;
	}
	const auto& queries = Queries();
	const int land = queries.landNumber ? queries.landNumber() : 0;
	if (land == 1 || land == 2 || g_Spooky.sample == 0) // 0x72E315..0x72E334
	{
		return;
	}
	if (g_Spooky.countdown != 0) // 0x72E33B
	{
		--g_Spooky.countdown;
		return;
	}
	if (NightNow(LocalTime())) // 0x72E344
	{
		// 0x72E34D..0x72E379: r = LocalFloatRand(1), ftol((1 - r^3) x 1000) compared unsigned with the counter
		// (float steps: the game's FPU is at 24 bits, fn_007DEE00 `and cw, 0xFCFF` at 0x7DEE0D)
		const float r = guidance::LocalFloatRand(1.0f);
		const auto chance = static_cast<uint32_t>(static_cast<int32_t>((1.0f - r * r * r) * 1000.0f));
		if (chance < g_Spooky.counter)
		{
			PlaySpooky();
		}
		else
		{
			guidance::OneOff(1); // 0x72E397
		}
		++g_Spooky.counter; // 0x72E39C
	}
	g_Spooky.countdown = 99; // 0x72E3A2..0x72E3A8: 100, then the decrement
}

void spooky::PlaySpooky()
{
	// 0x72E3F6..0x72E42C in float steps (the FPU at 24 bits, fn_007DEE00)
	const float a = guidance::LocalFloatRand(0.65f); // 0x3F266666
	float p = a * a * a + 1.0f;
	if (guidance::LocalRand(2) == 0)
	{
		p = 1.0f / p;
	}
	const float pitchFactor = p;
	const auto pan = static_cast<int>(guidance::LocalRand(180)); // 0xB4
	const float b = guidance::LocalFloatRand(0.8f); // 0x3F4CCCCD
	float q = b * b * b + 1.0f;
	if (guidance::LocalRand(2) == 0)
	{
		q = 1.0f / q;
	}
	// 0x72E470..0x72E4CD
	auto& options = g_Spooky.options;
	options.pitch = static_cast<int>(pitchFactor * 100.0f);
	options.volume = static_cast<int>(static_cast<float>(options.volume) * q); // fild (exact), fmul at 24 bits
	g_Spooky.field2C = pan;
	options.is3D = false;
	PlayOptions play;
	static_cast<sample_play::Options&>(play) = options;
	play.sound = 0;
	play.sample = {g_Spooky.bank, static_cast<int>(g_Spooky.sample)};
	const auto channel = PlaySoundEffect(play);
	g_Spooky.counter = 0; // 0x72E4D3: +0x14 = 0
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SpookyVoices: sample {} pitch {} volume {} (channel {})", g_Spooky.sample,
		                   options.pitch, options.volume, channel);
	}
}
