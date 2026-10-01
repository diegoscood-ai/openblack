/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Voices.h"

#include <cctype>

#include <array>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include "Common/HelpText.h"

namespace openblack::audio
{

namespace
{
std::string Upper(std::string_view s)
{
	std::string out(s);
	for (auto& c : out)
	{
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return out;
}

std::array<VoiceTable::SampleNames, static_cast<size_t>(SfxBank::_COUNT)> s_bankNames;
VoiceTable s_table;
} // namespace

std::string VoiceSampleKey(std::string_view waveName)
{
	const auto slash = waveName.find_last_of("\\/");
	if (slash != std::string_view::npos)
	{
		waveName.remove_prefix(slash + 1);
	}
	auto key = Upper(waveName);
	if (key.size() >= 4 && key.compare(key.size() - 4, 4, ".WAV") == 0)
	{
		key.resize(key.size() - 4);
	}
	return key;
}

VoiceTable VoiceTable::Build(const std::vector<std::string>& textNames, const SampleNames& villagers,
                             const SampleNames& helpSprites, const SampleNames& guidance)
{
	// the first sample of each name, in the search order of voices.md §3.2
	std::unordered_map<std::string, TextVoice> byName;
	const std::array<std::pair<SfxBank, const SampleNames*>, 3> order = {{
	    {SfxBank::Villagers, &villagers},
	    {SfxBank::HelpSprites, &helpSprites},
	    {SfxBank::Guidance, &guidance},
	}};
	for (const auto& [bank, names] : order)
	{
		for (size_t i = 0; i < names->size(); ++i)
		{
			byName.try_emplace(VoiceSampleKey((*names)[i]), TextVoice {bank, static_cast<uint32_t>(i + 1)});
		}
	}

	VoiceTable table;
	table._voices.reserve(textNames.size());
	for (const auto& name : textNames)
	{
		const auto found = byName.find(Upper(name));
		table._voices.push_back(found != byName.end() ? found->second : TextVoice {});
	}
	return table;
}

TextVoice VoiceTable::Get(uint32_t textId) const
{
	return textId < _voices.size() ? _voices[textId] : TextVoice {};
}

bool voices::IsTableBank(SfxBank bank)
{
	return bank == SfxBank::Villagers || bank == SfxBank::HelpSprites || bank == SfxBank::Guidance;
}

void voices::SetBankSampleNames(SfxBank bank, VoiceTable::SampleNames names)
{
	s_bankNames[static_cast<size_t>(bank)] = std::move(names);
}

void voices::BuildTable()
{
	std::vector<std::string> textNames;
	textNames.reserve(helptext::Count());
	for (uint32_t i = 0; i < helptext::Count(); ++i)
	{
		textNames.push_back(helptext::GetEntry(i).name);
	}
	s_table = VoiceTable::Build(textNames, s_bankNames[static_cast<size_t>(SfxBank::Villagers)],
	                            s_bankNames[static_cast<size_t>(SfxBank::HelpSprites)],
	                            s_bankNames[static_cast<size_t>(SfxBank::Guidance)]);
	std::array<size_t, static_cast<size_t>(SfxBank::_COUNT)> counts {};
	for (uint32_t i = 0; i < s_table.Size(); ++i)
	{
		if (const auto voice = s_table.Get(i); voice.HasVoice())
		{
			++counts[static_cast<size_t>(voice.bank)];
		}
	}
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Voice table: {} texts, HelpSprites {}, villagers {}, Guidance {}", s_table.Size(),
		                   counts[static_cast<size_t>(SfxBank::HelpSprites)], counts[static_cast<size_t>(SfxBank::Villagers)],
		                   counts[static_cast<size_t>(SfxBank::Guidance)]);
	}
}

const VoiceTable& voices::Table()
{
	return s_table;
}

} // namespace openblack::audio
