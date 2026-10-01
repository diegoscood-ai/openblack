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

#include "Advisor.h"
#include "Audio.h"
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

void voices::SetTable(VoiceTable table)
{
	s_table = std::move(table);
}

bool voices::BankRegistered(SfxBank bank)
{
	return Bank(bank) != k_NoBank;
}

Channel voices::RunTextVoice(int32_t narrator, TextVoice voice)
{
	if (voice.bank == SfxBank::HelpSprites) // 0x5C6025
	{
		const int dude = narrator == helptext::k_NarratorGoodSpirit   ? advisor::k_GoodSpirit // 0x5C602A
		                 : narrator == helptext::k_NarratorEvilSpirit ? advisor::k_EvilSpirit // 0x5C6061
		                                                              : -1;
		if (dude >= 0)
		{
			advisor::Stop(advisor::k_EvilSpirit);                           // fn_005C3750(1) 0x5C6034 / 0x5C606B
			advisor::Stop(advisor::k_GoodSpirit);                           // fn_005C3750(0) 0x5C603E / 0x5C6075
			advisor::Say(dude, static_cast<int>(voice.sample), false);      // fn_005C36D0(dude, sample, 0)
			return k_NoChannel;
		}
	}
	if (voice.sample == 0 || voice.bank == SfxBank::None) // 0x5C609C..0x5C60A2
	{
		return k_NoChannel;
	}
	PlayOptions options;                                            // ctor 0x5C60A8
	options.sample = {Bank(voice.bank), static_cast<int>(voice.sample)}; // +0x24 (0x5C60B4), +0x04 (0x5C60C4)
	options.owner = Owner::Key(k_OwnerVoice);                       // +0x20 = 0x270F (0x5C60C8)
	options.keepPcm = true;                                         // +0x164 = 1 (0x5C60D0)
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	return PlaySoundEffect(options); // 0x5C60DB
}

Channel voices::Say(uint32_t textId, bool withPosition, bool alt, glm::vec3 position)
{
	if (textId >= helptext::k_TextCount) // 0x70F8EA
	{
		textId = 0;
	}
	// 0x70F8FB / 0x70F904: the say table 0x942B38 (+8 sample, +4 bank); W120's entries all have id == index, so the
	// id test of 0x70F910 never zeroes the bank (VoiceTable keeps no id)
	const auto voice = Table().Get(textId);
	if (voice.sample == 0) // 0x70F90A
	{
		return k_NoChannel;
	}
	PlayOptions options;                                                 // ctor 0x70F91E
	options.sample = {Bank(voice.bank), static_cast<int>(voice.sample)}; // +0x04 (0x70F956), +0x24 (0x70F93E)
	options.owner = Owner::Key(alt ? k_OwnerVoiceAlt : k_OwnerVoice);    // +0x20 (0x70F931..0x70F949)
	options.is3D = withPosition;                                         // +0x08 (0x70F96C)
	options.keepPcm = true;                                              // +0x164 = 1 (0x70F961)
	if (withPosition)                                                    // 0x70F970..0x70F986
	{
		options.position = position;
	}
	options.track = false; // +0x0C = 0 (0x70F98F)
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: SAY({}, {}, ({:.1f}, {:.1f}, {:.1f}), alt {}) -> bank {} sample {}", textId,
		                   withPosition ? 1 : 0, position.x, position.y, position.z, alt ? 1 : 0, static_cast<int>(voice.bank),
		                   voice.sample);
	}
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	return PlaySoundEffect(options); // 0x70F997
}

bool voices::IsSaying(bool alt, uint32_t textId)
{
	if (textId >= helptext::k_TextCount) // 0x7102A7
	{
		textId = 0;
	}
	const auto voice = Table().Get(textId);
	if (voice.sample == 0) // 0x7102C3
	{
		return false;
	}
	// 0x7102C7..0x7102DC: fn_0042A280(alt ? 0x270D : 0x270F, sample, bank)
	return IsPlaying(Owner::Key(alt ? k_OwnerVoiceAlt : k_OwnerVoice), static_cast<int>(voice.sample), voice.bank);
}

void voices::CutByClick()
{
	// 0x5C6AA4..0x5C6AAD: StopPlayingSoundEffect(0, 0x270F, 7)
	StopSoundEffect(0, Owner::Key(k_OwnerVoice), SfxBank::Villagers);
}

} // namespace openblack::audio
