/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// GAudio::PlaySoundEffect's variants 0x429D60..0x42A100, the stop / loop / query wrappers 0x42A210..0x42A330 and
// fn_00428740, and the global cyclic counters of their callers (layer 3 of dev\tmp_dis\audio\PLAN.md §2.1: GameSfx).

#include <array>

#include "Audio.h"
#include "Sound.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// GAudio's working options +0x240 (LH_SamplePlayOptions::ctor at 0x426D63) as the variants fill them: the fields they
/// do not write keep the ctor's defaults (inferred: no other GAudio function writes +0x240 between two calls)
sample_play::Options Variant(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool flag10, bool is3D,
                             BankId bank)
{
	sample_play::Options options;
	options.sound = SampleId(bank, sample); // +0x04 bank, +0x24 sample
	options.owner = owner;                  // +0x20
	options.flag10 = flag10;                // +0x10
	options.is3D = is3D;                    // +0x08
	options.track = is3D;                   // +0x0C = is3D (0x42A087)
	options.position = position;            // +0x30
	options.offset = glm::vec3(0.0f);       // +0x3C (0x42A0B5..0x42A0C1)
	options.loops = loops;                  // +0x4C
	options.mode = mode;                    // +0x50
	return options;
}

/// Get3DSoundPos of the owner of 0x429DA0 (vtable +0x10): a thing or a registered object; nothing for the others (a key
/// or a tag passed as a GameThingWithPos would be a bad pointer in the original)
std::optional<glm::vec3> SoundPosition(const Owner& owner)
{
	switch (owner.kind)
	{
	case Owner::Kind::Thing:
	case Owner::Kind::Object:
		return OwnerSoundPosition(owner);
	default:
		return std::nullopt;
	}
}

struct CounterRule
{
	int count;
	bool maskAfter; ///< the tree mulch: c = (c + 1) & 3, then base + c (0x63AA39)
};
/// Counter -> its count (sfx_inventory.md, "contador cíclico")
constexpr std::array<CounterRule, static_cast<size_t>(Counter::_Count)> k_CounterRules = {{
    {9, false},  // KnockRoof 0xC4CC7C
    {5, false},  // CitadelSparkEffect 0xC5E3E4
    {5, false},  // CitadelSparkDamage 0xC5E3E8
    {4, false},  // CreatureRockTap 0xC6421C
    {3, false},  // CreatureSquash 0xC64220
    {10, false}, // HandInWater 0xD18228
    {4, true},   // TreeMulch 0xD4437C
    {4, false},  // RockTap 0xD559AC
    {4, false},  // ScaffoldCombine 0xD95AF8
    {4, false},  // ScaffoldTap 0xD95AFC
}};
std::array<int, static_cast<size_t>(Counter::_Count)> g_Counters {};
} // namespace

Channel audio::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool flag10, bool is3D, SfxBank bank)
{
	// 0x429D60: GAudio+0x3A8 + 4 * type
	return PlaySoundEffect(owner, sample, mode, loops, flag10, is3D, Bank(bank));
}

Channel audio::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool flag10, bool is3D, BankId bank)
{
	// 0x429DA9: nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	glm::vec3 position(0.0f);
	if (is3D)
	{
		// 0x429DBD..0x429DD7: an unavailable owner or none plays nothing; else its Get3DSoundPos
		if (OwnerUnavailable(owner) || owner.kind == Owner::Kind::None)
		{
			return k_NoChannel;
		}
		if (const auto at = SoundPosition(owner))
		{
			position = *at;
		}
	}
	return PlaySoundEffectAt(owner, position, sample, mode, loops, flag10, is3D, bank);
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool flag10, bool is3D,
                                 SfxBank bank)
{
	// 0x42A000: GAudio+0x3A8 + 4 * type
	return PlaySoundEffectAt(owner, position, sample, mode, loops, flag10, is3D, Bank(bank));
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool flag10, bool is3D,
                                 BankId bank)
{
	// 0x42A04C: nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	const auto options = Variant(owner, position, sample, mode, loops, flag10, is3D, bank);
	// 0x42A0CE..0x42A0DB: a 3D one of an unavailable owner plays nothing
	if (is3D && OwnerUnavailable(owner))
	{
		return k_NoChannel;
	}
	return PlaySoundEffectOptions(options);
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode,
                                 int loops, bool flag10, bool is3D, BankId bank)
{
	// 0x42A109: nothing for sample 0
	if (sample == 0)
	{
		return k_NoChannel;
	}
	auto options = Variant(owner, position, sample, mode, loops, flag10, is3D, bank);
	options.offset = offset; // +0x3C (0x42A15C..0x42A174)
	options.track = track;   // +0x0C, the low byte of the 5th argument (0x42A18E..0x42A19C)
	// 0x42A19A..0x42A1B4: is3D and track and an unavailable owner
	if (is3D && track && OwnerUnavailable(owner))
	{
		return k_NoChannel;
	}
	return PlaySoundEffectOptions(options);
}

void audio::StopSoundEffect(int sample, Owner owner, SfxBank bank)
{
	StopSoundEffect(sample, owner, Bank(bank));
}

void audio::StopSoundEffect(int sample, Owner owner, BankId bank)
{
	// 0x42A210 -> LHSampleStop(bank, owner, sample): sample 0 = any (0x10012C76)
	if (sample == 0)
	{
		sample_play::StopOwner(bank, owner);
		return;
	}
	sample_play::Stop(SampleId(bank, sample), owner);
}

void audio::StopAllSoundEffects()
{
	sample_play::StopAll();
}

void audio::ReleaseLoop(Owner owner, int sample, SfxBank bank)
{
	ReleaseLoop(owner, sample, Bank(bank));
}

void audio::ReleaseLoop(Owner owner, int sample, BankId bank)
{
	sample_play::ReleaseLoop(SampleId(bank, sample), owner);
}

bool audio::IsPlaying(Owner owner, int sample, SfxBank bank)
{
	return IsPlaying(owner, sample, Bank(bank));
}

bool audio::IsPlaying(Owner owner, int sample, BankId bank)
{
	return sample_play::IsPlaying(SampleId(bank, sample), owner);
}

bool audio::IsPlaying(Owner owner, SfxBank bank)
{
	return sample_play::IsOwnerPlaying(Bank(bank), owner);
}

void audio::SetPitch(BankId bank, Owner owner, int sample, int percent)
{
	sample_play::SetPitch(SampleId(bank, sample), owner, percent);
}

void audio::SetVolume(Channel channel, int volume)
{
	sample_play::SetVolume(channel, volume);
}

bool audio::IsPlaying(Channel channel)
{
	return sample_play::IsPlaying(channel);
}

int audio::NextCounter(Counter counter)
{
	const auto index = static_cast<size_t>(counter);
	if (index >= g_Counters.size())
	{
		return 0;
	}
	auto& value = g_Counters[index];
	const auto& rule = k_CounterRules[index];
	if (rule.maskAfter)
	{
		// 0x63AA39: g = (g + 1) & 3, then the sample is 155 + g
		value = (value + 1) & (rule.count - 1);
		return value;
	}
	// 0x4068F4..0x40690F: the sample is base + g, then ++g and back to 0 at the count
	const int current = value;
	value = value + 1 == rule.count ? 0 : value + 1;
	return current;
}
