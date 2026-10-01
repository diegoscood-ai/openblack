/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimEffects.h"

#include <cstdlib>
#include <cstring>

#include <map>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "SamplePlay.h"
#include "Sound.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
/// The tables of each registered bank (LH_AudioBank +0x124 / +0x12C / +0x130)
std::map<BankId, AnimEffectTable> g_Tables;

/// The animation sounds' trace (AnimationSounds): only its own lines, as before milestone B2
bool AnimTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
	return k_Trace;
}

/// The core's trace (OPENBLACK_AUDIO_TRACE): every cull of an anim effect
bool AudioTrace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}
} // namespace

// ---- the tables (the miracles' AnimEffectBank, unchanged) -----------------------------------------------------------

void AnimEffectTable::Load(const std::filesystem::path& path)
{
	// A bank the game has registered already has its tables in the core (read once, LHBankRegister 0x10002778): they
	// are copied, not read again (the miracles' spells.sad of SpellSounds). Its last three parts name it
	// ("Sfx/Game/spells.sad"); a bank not registered (tools, tests) is read from its file.
	const auto tail = path.parent_path().parent_path().filename() / path.parent_path().filename() / path.filename();
	if (const auto* registered = anim_effects::Tables(FindBank(tail.generic_string())); registered != nullptr)
	{
		rows = registered->rows;
		waves = registered->waves;
		samples = registered->samples;
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	pack::PackFile file;
	if (file.ReadFile(*fileSystem.GetData(path)) != pack::PackResult::Success)
	{
		return;
	}
	Load(file);
}

void AnimEffectTable::Load(const pack::PackFile& file)
{
	for (const auto& header : file.GetAudioSampleHeaders())
	{
		// LH_BankSample +0x244: the override flags (the copy in the DLL's start 0x10011420 @0x100119D4)
		const uint32_t overrides = static_cast<uint32_t>(header.unknown10) | (static_cast<uint32_t>(header.unknown11) << 16);
		Sample sample;
		if ((overrides & 0x40u) != 0)
		{
			sample.loops = header.loop;
		}
		if ((overrides & 0x400u) != 0)
		{
			sample.playMode = static_cast<int32_t>(header.loopType);
		}
		sample.maxDistance = header.maxDist;
		samples.insert_or_assign(header.id, sample);
	}
	const auto& blocks = file.GetBlocks();
	const auto table = blocks.find("LHAudioAnimArrayTable");
	const auto lists = blocks.find("LHAudioWaveNumTable");
	if (table == blocks.end() || lists == blocks.end() || table->second.size() < 8)
	{
		return;
	}
	// 0x100027DD / 0x100027F8: u32 rows (+0x130), u32 width (+0x12C), then rows x width s32 (+0x124). Every bank of the
	// game has width 6 (5 attributes and the list); another width is not read (openblack).
	int32_t count = 0;
	int32_t width = 0;
	std::memcpy(&count, table->second.data(), 4);
	std::memcpy(&width, table->second.data() + 4, 4);
	for (int32_t r = 0; width == 6 && r < count && 8 + (r + 1) * 24 <= static_cast<int32_t>(table->second.size()); ++r)
	{
		std::array<int32_t, 6> row {};
		std::memcpy(row.data(), table->second.data() + 8 + r * 24, 24);
		rows.push_back(row);
	}
	waves.resize(lists->second.size() / 4);
	std::memcpy(waves.data(), lists->second.data(), waves.size() * 4);
}

std::vector<int32_t> AnimEffectTable::FindList(const std::array<int32_t, 5>& key) const
{
	// LHFindAttribRow 0x10014420: a row matches when each of its first width - 1 columns is the wildcard or the key's
	// (0x100145A1..0x100145B6); between two matching rows fn_10014610 keeps the earlier one only when it has more
	// columns equal to the key (0x10014662), so the later wins a tie
	int best = -1;
	size_t bestRow = 0;
	for (size_t r = 0; r < rows.size(); ++r)
	{
		int exact = 0;
		bool match = true;
		for (size_t c = 0; c < 5 && match; ++c)
		{
			const auto value = rows[r][c];
			if (value == k_Wildcard)
			{
				continue;
			}
			match = value == key.at(c);
			++exact;
		}
		if (match && exact >= best)
		{
			best = exact;
			bestRow = r;
		}
	}
	if (best < 0)
	{
		return {};
	}
	// the last column: a dword index of LHAudioWaveNumTable, {count, sample numbers...}
	const auto list = static_cast<size_t>(rows[bestRow][5]);
	if (list >= waves.size() || waves[list] <= 0 || list + static_cast<size_t>(waves[list]) >= waves.size())
	{
		return {};
	}
	return {waves.begin() + static_cast<std::ptrdiff_t>(list + 1),
	        waves.begin() + static_cast<std::ptrdiff_t>(list + 1 + static_cast<size_t>(waves[list]))};
}

entt::id_type AnimEffectTable::SoundId(int32_t sample) const
{
	return entt::hashed_string(fmt::format("{}/{}", name, sample).c_str()).value();
}

const AnimEffectTable::Sample* AnimEffectTable::FindSample(int32_t sample) const
{
	const auto it = samples.find(sample);
	return it == samples.end() ? nullptr : &it->second;
}

// ---- the banks' tables -----------------------------------------------------------------------------------------------

void anim_effects::RegisterTables(BankId bank, const pack::PackFile& file)
{
	if (bank == k_NoBank || g_Tables.contains(bank))
	{
		return;
	}
	AnimEffectTable table;
	table.name = BankGroup(bank);
	table.Load(file);
	if (table.rows.empty())
	{
		return;
	}
	if (auto logger = spdlog::get("audio"))
	{
		SPDLOG_LOGGER_DEBUG(logger, "Anim effects: {} rows in {}", table.rows.size(), table.name);
	}
	g_Tables.emplace(bank, std::move(table));
}

const AnimEffectTable* anim_effects::Tables(BankId bank)
{
	const auto found = g_Tables.find(bank);
	return found != g_Tables.end() ? &found->second : nullptr;
}

void anim_effects::Clear()
{
	g_Tables.clear();
}

// ---- LHSampleGetAnimEffectNumber / LHSamplePlayAnimEffect -----------------------------------------------------------

int anim_effects::Number(const AnimKey& key, BankId bank)
{
	// 0x10014674..0x1001468F: the audio system's +8 / +0x14 / +4 and a bank
	if (bank == k_NoBank || !sample_play::IsActive())
	{
		return 0;
	}
	const auto* table = Tables(bank);
	if (table == nullptr)
	{
		return 0;
	}
	const auto list = table->FindList(key);
	if (list.empty())
	{
		return 0;
	}
	if (list.size() == 1)
	{
		return list[0];
	}
	// 0x100146CF: LH_AudioSystem::Rand(count)
	return list[static_cast<size_t>(sample_play::Random(static_cast<int>(list.size())))];
}

Channel anim_effects::Play(Owner owner, float distance, int sample, bool track, BankId bank, float minDistance,
                           float maxDistance)
{
	// 0x10014A48..0x10014A7D: +8, +0x14 (active), +4 and a bank
	if (bank == k_NoBank || !sample_play::IsActive())
	{
		return k_NoChannel;
	}
	const auto id = SampleId(bank, sample);
	const auto* sound = sample_play::GetSound(id);
	if (sound == nullptr)
	{
		return k_NoChannel;
	}
	// 0x10014A83 (the global 800) and 0x10014AB0 (the sample's .sad +0x26C): the caller's distance must not be more
	if (!(distance <= k_MaxDistance) || !(distance <= sound->maxDistance))
	{
		const float limit = distance > k_MaxDistance ? k_MaxDistance : sound->maxDistance;
		if (AnimTrace() && bank == Bank(SfxBank::VillagersBanter))
		{
			// the line of the animation sounds' trace before milestone B2 (AnimationSounds: banter only)
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Animation sound: banter {} too far ({:.1f} > {})", sample, distance,
			                   limit);
		}
		else if (AudioTrace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Anim effect: {}/{} ({}) too far ({:.1f} > {})", BankGroup(bank), sample,
			                   sound->name, distance, limit);
		}
		return k_NoChannel;
	}
	sample_play::Options options;
	options.sound = id;           // +0x04 bank, +0x24 sample
	options.owner = owner;        // +0x20
	options.is3D = true;          // +0x08 = 1 (0x10014AF3)
	options.flag10 = false;       // +0x10 = 0 (0x10014AFD)
	options.track = track;        // +0x0C (0x10014B05)
	options.callerMask = 0;       // +0x1C (0x10014B0C)
	if (minDistance > 0.0f)       // 0x10014ACC: fcomp 0
	{
		options.callerMask |= 0x80u;
		options.minDistance = minDistance; // +0x54
	}
	if (maxDistance > 0.0f)
	{
		options.callerMask |= 0x100u;
		options.maxDistance = maxDistance; // +0x58
	}
	// 0x10014B91: the game's 3D function (fn_00427200) for the owner; 0 = nothing plays
	const auto position = Get3DSoundPos(owner);
	if (!position)
	{
		return k_NoChannel;
	}
	options.position = *position; // +0x30 (0x10014B9F..0x10014BBA)
	return sample_play::Start(options);
}

Channel anim_effects::PlayKey(Owner owner, float distance, const AnimKey& key, AnimAction action, bool track,
                              BankId bank, float minDistance, float maxDistance)
{
	// 0x1001471E..0x10014762: +8, +0x14, +4, a bank, and the distance within the global 800
	if (bank == k_NoBank || !sample_play::IsActive() || !(distance <= k_MaxDistance))
	{
		return k_NoChannel;
	}
	const auto* table = Tables(bank);
	if (table == nullptr)
	{
		return k_NoChannel;
	}
	const auto list = table->FindList(key);
	if (list.empty())
	{
		return k_NoChannel;
	}
	switch (action)
	{
	case AnimAction::Play:
	{
		// 0x100147D0: one sample of the list, then the same as 0x10014A20
		const int sample = list.size() == 1 ? list[0]
		                                    : list[static_cast<size_t>(sample_play::Random(static_cast<int>(list.size())))];
		return Play(owner, distance, sample, track, bank, minDistance, maxDistance);
	}
	case AnimAction::Stop:
		// 0x1001491C..0x10014989: for each sample of the list the first channel of (bank, owner, sample) stops
		for (const auto sample : list)
		{
			sample_play::Stop(SampleId(bank, sample), owner);
		}
		return k_NoChannel;
	default:
		// 0x10014990..0x100149F5: the same with LHSampleReleaseLoop
		for (const auto sample : list)
		{
			sample_play::ReleaseLoop(SampleId(bank, sample), owner);
		}
		return k_NoChannel;
	}
}
