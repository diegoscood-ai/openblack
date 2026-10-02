/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Banks.h"

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <fstream>
#include <memory>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "AnimEffects.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "MusicBank.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Sound.h"
#include "Voices.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
struct BankEntry
{
	std::string path; ///< lower case, '/' separators
	std::string group;
	int samples {0}; ///< LHBankGetNumberOfSamples: the .sad's sample table size
	std::vector<entt::id_type> sounds; ///< the samples LoadAll loaded (the debug panel, the atmos banks)
};

struct State
{
	std::vector<BankEntry> banks; ///< BankId - 1
	/// GAudio+0x3A8 + 4 * type
	std::array<BankId, static_cast<size_t>(SfxBank::_COUNT)> types {};
	/// GAudio+0x2C + 4 * type (0x9C9748), and whether a type was tried already
	std::array<std::unique_ptr<MusicBank>, static_cast<size_t>(MusicType::_COUNT)> music;
	std::array<bool, static_cast<size_t>(MusicType::_COUNT)> musicTried {};
};
State g_State;

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

std::string Normalised(std::string_view path)
{
	std::string text(path);
	std::replace(text.begin(), text.end(), '\\', '/');
	std::transform(text.begin(), text.end(), text.begin(),
	               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool EndsWith(const std::string& text, const std::string& tail)
{
	return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

/// The path is the one of a type of 0x9CB3F8 (any case, any separator)
bool IsBankOfType(const std::filesystem::path& path, SfxBank type)
{
	return EndsWith(Normalised(path.generic_string()), Normalised(SfxBankPath(type)));
}

/// LHBankRegister(path, 0) 0x10002240 of one sample bank, as Game read the .sad: its headers (and, but for the dialogue
/// banks, its waves), its anim effect tables, its samples as resources
void LoadBank(const std::filesystem::path& f)
{
	auto& fileSystem = Locator::filesystem::value();
	auto& soundManager = Locator::resources::value().GetSounds();

	pack::PackFile soundPack;
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Opening sound pack {}", f.filename().string());
	// The dialogue banks of 0x9CB3F8 (types 6..10, Audio\Dialogue) are registered as LHBankRegister(path, 0)
	// 0x10002240 does: only the headers are read, and each wave is read from the file at its first play
	// (0x10011420 -> fn_100032D0; Sound::waveFile). The other banks keep their bytes in memory (approximated: the
	// original reads every bank that way, 0x426EEE).
	bool onDemand = false;
	for (const auto bank : {SfxBank::HelpSprites, SfxBank::Villagers, SfxBank::VillagersBanter, SfxBank::SpellDialogue,
	                        SfxBank::Guidance})
	{
		onDemand = onDemand || IsBankOfType(f, bank);
	}
	const auto result =
	    onDemand ? soundPack.ReadAudioHeaders(*fileSystem.GetData(f)) : soundPack.ReadFile(*fileSystem.GetData(f));
	if (result != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to load sound pack {}: {}", f.filename().string(),
		                    pack::ResultToStr(result));
		return;
	}
	const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
	const auto& audioData = soundPack.GetAudioSamplesData();
	if (audioHeaders.empty())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound pack found for {}. Skipping", f.filename().string());
		return;
	}
	auto soundName = std::filesystem::path(audioHeaders[0].name.data());

	auto groupName = f.filename().string();

	// The wave names of the dialogue banks of the voice table 0x915D40 (k_SfxBankPaths 6, 7, 10)
	for (const auto bank : {SfxBank::Villagers, SfxBank::HelpSprites, SfxBank::Guidance})
	{
		if (IsBankOfType(f, bank))
		{
			std::vector<std::string> names;
			names.reserve(audioHeaders.size());
			for (const auto& header : audioHeaders)
			{
				names.emplace_back(header.name.begin(), std::find(header.name.begin(), header.name.end(), '\0'));
			}
			voices::SetBankSampleNames(bank, std::move(names));
		}
	}

	// A music bank (its waves are ".mpg"): LHMusic registers it by MUSIC_TYPE (MusicBankOf, k_MusicBanks 0x9C9748)
	if (soundName.extension() == ".mpg")
	{
		return;
	}
	// LHBankRegister 0x10002240: the bank of its samples (the 11 types of 0x9CB3F8 by path, any case)
	const auto bankId = RegisterBank(f, groupName);
	SetBankSampleCount(bankId, static_cast<int>(audioHeaders.size()));
	// 0x10002778..0x100029AB: its anim effect tables, read once here (audio::anim_effects)
	anim_effects::RegisterTables(bankId, soundPack);
	auto& sounds = g_State.banks[bankId - 1].sounds;
	sounds.clear();
	for (size_t i = 0; i < audioHeaders.size(); i++)
	{
		soundName = std::filesystem::path(audioHeaders[i].name.data());
		if (onDemand ? audioHeaders[i].size == 0 : audioData[i].empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound buffer found for {}. Skipping", soundName.string());
			continue; // the next ones still load (spells.sad has an empty entry 31 before 32..88)
		}

		const auto stringId = fmt::format("{}/{}", groupName, audioHeaders[i].id);
		const entt::id_type id = entt::hashed_string(stringId.c_str());
		const std::vector<std::vector<uint8_t>> buffer =
		    onDemand ? std::vector<std::vector<uint8_t>> {} : std::vector<std::vector<uint8_t>> {audioData[i]};
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Loading sound {}: {}", stringId, audioHeaders[i].name.data());
		soundManager.Load(id, resources::SoundLoader::FromBufferTag {}, audioHeaders[i], buffer);
		soundManager.Handle(id)->bank = bankId;
		if (onDemand)
		{
			soundManager.Handle(id)->waveFile = f;
			soundManager.Handle(id)->waveOffset = soundPack.GetAudioWaveDataOffset() + audioHeaders[i].offset;
			soundManager.Handle(id)->waveSize = audioHeaders[i].size;
		}
		sounds.emplace_back(id);
	}
}
} // namespace

// ---- the registry ---------------------------------------------------------------------------------------------------

BankId audio::RegisterBank(const std::filesystem::path& path, std::string_view group)
{
	const auto normalised = Normalised(path.generic_string());
	for (size_t i = 0; i < g_State.banks.size(); ++i)
	{
		if (g_State.banks[i].path == normalised)
		{
			return static_cast<BankId>(i + 1);
		}
	}
	g_State.banks.push_back({normalised, std::string(group)});
	const auto id = static_cast<BankId>(g_State.banks.size());
	// fn_0042A390: the slot of a type is filled once, with the bank of its path
	for (size_t type = 1; type < k_SfxBankPaths.size(); ++type)
	{
		if (g_State.types[type] == k_NoBank && EndsWith(normalised, Normalised(k_SfxBankPaths[type])))
		{
			g_State.types[type] = id;
		}
	}
	return id;
}

void audio::SetBankSampleCount(BankId bank, int samples)
{
	if (bank != k_NoBank && bank <= g_State.banks.size())
	{
		g_State.banks[bank - 1].samples = samples;
	}
}

int audio::BankSampleCount(BankId bank)
{
	return bank != k_NoBank && bank <= g_State.banks.size() ? g_State.banks[bank - 1].samples : 0;
}

BankId audio::Bank(SfxBank type)
{
	const auto index = static_cast<size_t>(type);
	return index < g_State.types.size() ? g_State.types[index] : k_NoBank;
}

BankId audio::FindBank(std::string_view path)
{
	const auto wanted = Normalised(path);
	for (size_t i = 0; i < g_State.banks.size(); ++i)
	{
		if (EndsWith(g_State.banks[i].path, wanted))
		{
			return static_cast<BankId>(i + 1);
		}
	}
	return k_NoBank;
}

std::string audio::BankGroup(BankId bank)
{
	return bank != k_NoBank && bank <= g_State.banks.size() ? g_State.banks[bank - 1].group : std::string {};
}

entt::id_type audio::SampleId(BankId bank, int number)
{
	const auto group = BankGroup(bank);
	if (group.empty())
	{
		return 0;
	}
	const auto key = fmt::format("{}/{}", group, number);
	return entt::hashed_string(key.c_str()).value();
}

// ---- loading --------------------------------------------------------------------------------------------------------

void banks::LoadAll()
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	// every sound pack in the Audio directory, in the file system's order
	fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Audio>(), true, [](const std::filesystem::path& f) {
		if (f.extension() == ".sad")
		{
			LoadBank(f);
		}
	});
}

size_t banks::Count()
{
	return g_State.banks.size();
}

std::string banks::Path(BankId bank)
{
	return bank != k_NoBank && bank <= g_State.banks.size() ? g_State.banks[bank - 1].path : std::string {};
}

const std::vector<entt::id_type>& banks::Samples(BankId bank)
{
	static const std::vector<entt::id_type> k_None;
	return bank != k_NoBank && bank <= g_State.banks.size() ? g_State.banks[bank - 1].sounds : k_None;
}

bool banks::ReadWave(const Sound& sound, std::vector<uint8_t>& out)
{
	out.clear();
	if (sound.waveFile.empty() || sound.waveSize == 0)
	{
		return false;
	}
	// fn_100032D0 (from 0x10011420): the wave is read from the bank's open file when the sample first plays
	std::unique_ptr<std::istream> stream;
	if (Locator::filesystem::has_value())
	{
		stream = Locator::filesystem::value().GetData(sound.waveFile);
	}
	else
	{
		stream = std::make_unique<std::ifstream>(sound.waveFile, std::ios::binary);
	}
	if (!stream || !*stream)
	{
		return false;
	}
	out.resize(sound.waveSize);
	stream->seekg(static_cast<std::streamoff>(sound.waveOffset));
	stream->read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
	if (stream->gcount() != static_cast<std::streamsize>(out.size()))
	{
		out.clear();
		return false;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Wave of {} read from {} ({} bytes at {})", sound.name,
		                   sound.waveFile.filename().string(), sound.waveSize, sound.waveOffset);
	}
	return true;
}

// ---- the music banks ------------------------------------------------------------------------------------------------

MusicBank* banks::MusicBankOf(MusicType type, bool& registeredNow)
{
	registeredNow = false;
	const auto index = static_cast<size_t>(type);
	if (index >= g_State.music.size())
	{
		return nullptr;
	}
	if (!g_State.musicTried[index])
	{
		g_State.musicTried[index] = true;
		const auto& entry = k_MusicBanks[index];
		if (!entry.path.empty() && Locator::filesystem::has_value())
		{
			try
			{
				const auto path = Locator::filesystem::value().FindPath(std::filesystem::path(entry.path));
				g_State.music[index] = MusicBank::Register(path);
			}
			catch (const std::exception& e)
			{
				if (auto logger = spdlog::get("audio"))
				{
					SPDLOG_LOGGER_WARN(logger, "music: {} ({}): {}", entry.name, entry.path, e.what());
				}
			}
		}
		registeredNow = g_State.music[index] != nullptr;
	}
	return g_State.music[index].get();
}

void banks::ReleaseMusicBanks()
{
	for (auto& bank : g_State.music)
	{
		bank.reset();
	}
	g_State.musicTried.fill(false);
}
