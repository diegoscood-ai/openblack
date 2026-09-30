/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimEffectBank.h"

#include <cstring>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"

using namespace openblack::audio;

void AnimEffectBank::Load(const std::filesystem::path& path)
{
	auto& fileSystem = Locator::filesystem::value();
	pack::PackFile file;
	if (file.ReadFile(*fileSystem.GetData(path)) != pack::PackResult::Success)
	{
		return;
	}
	Load(file);
}

void AnimEffectBank::Load(const pack::PackFile& file)
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

std::vector<int32_t> AnimEffectBank::FindList(const std::array<int32_t, 5>& key) const
{
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
	const auto list = static_cast<size_t>(rows[bestRow][5]);
	if (list >= waves.size() || waves[list] <= 0 || list + static_cast<size_t>(waves[list]) >= waves.size())
	{
		return {};
	}
	return {waves.begin() + static_cast<std::ptrdiff_t>(list + 1),
	        waves.begin() + static_cast<std::ptrdiff_t>(list + 1 + static_cast<size_t>(waves[list]))};
}

entt::id_type AnimEffectBank::SoundId(int32_t sample) const
{
	return entt::hashed_string(fmt::format("{}/{}", name, sample).c_str()).value();
}

const AnimEffectBank::Sample* AnimEffectBank::FindSample(int32_t sample) const
{
	const auto it = samples.find(sample);
	return it == samples.end() ? nullptr : &it->second;
}
