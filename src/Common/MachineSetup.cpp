/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MachineSetup.h"

#include <charconv>

#include <exception>
#include <span>
#include <system_error>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Resources/CreatureFileWriter.h"

namespace openblack::machine_setup
{
namespace
{
/// The names the original's setup key gives the two volumes
constexpr std::string_view k_SampleMainVolumeKey = "AudioSampleMasterVolume";
constexpr std::string_view k_MusicMainVolumeKey = "AudioMusicMasterVolume";
/// The largest effects' volume the audio takes
constexpr uint32_t k_MaxSampleVolume = 127;

std::optional<uint32_t> ReadNumber(std::string_view text)
{
	uint32_t value = 0;
	const auto* end = text.data() + text.size();
	const auto [last, error] = std::from_chars(text.data(), end, value);
	if (error != std::errc {} || last != end || text.empty())
	{
		return std::nullopt;
	}
	return value;
}
} // namespace

std::string Write(const Setup& setup)
{
	std::string text;
	const auto line = [&text](std::string_view key, const std::optional<uint32_t>& value) {
		if (value.has_value())
		{
			text.append(key).append("\t").append(std::to_string(*value)).append("\n");
		}
	};
	line(k_SampleMainVolumeKey, setup.sampleMainVolume);
	line(k_MusicMainVolumeKey, setup.musicMainVolume);
	return text;
}

Setup Read(std::string_view text)
{
	Setup setup;
	while (!text.empty())
	{
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!line.empty() && line.back() == '\r')
		{
			line.remove_suffix(1);
		}
		const auto tab = line.find('\t');
		if (tab == std::string_view::npos)
		{
			continue;
		}
		const auto key = line.substr(0, tab);
		const auto value = ReadNumber(line.substr(tab + 1));
		if (!value.has_value())
		{
			continue;
		}
		if (key == k_SampleMainVolumeKey)
		{
			setup.sampleMainVolume = value;
		}
		else if (key == k_MusicMainVolumeKey)
		{
			setup.musicMainVolume = value;
		}
	}
	return setup;
}

Setup FromVolumes(uint32_t sampleMainVolume, uint32_t musicMainVolume)
{
	return {.sampleMainVolume = sampleMainVolume, .musicMainVolume = musicMainVolume};
}

void ApplyVolumes(const Setup& setup, uint32_t& sampleMainVolume, uint32_t& musicMainVolume)
{
	if (setup.sampleMainVolume.has_value() && *setup.sampleMainVolume <= k_MaxSampleVolume)
	{
		sampleMainVolume = *setup.sampleMainVolume;
	}
	if (setup.musicMainVolume.has_value())
	{
		musicMainVolume = *setup.musicMainVolume;
	}
}

Setup Load(filesystem::FileSystemInterface& fileSystem, const std::optional<std::filesystem::path>& folder)
{
	if (!folder.has_value())
	{
		return {};
	}
	const auto path = *folder / k_SetupFile;
	try
	{
		// the player's own file: a data mod never replaces it
		const auto bytes = fileSystem.ReadUserFile(path);
		if (!bytes.has_value())
		{
			return {};
		}
		return Read(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
	}
	catch (const std::exception& e)
	{
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The setup could not be read from {}: {}", path.generic_string(), e.what());
		}
		return {};
	}
}

bool Save(filesystem::FileSystemInterface& fileSystem, const std::optional<std::filesystem::path>& folder, const Setup& setup)
{
	if (!folder.has_value())
	{
		return false;
	}
	const auto path = *folder / k_SetupFile;
	const auto text = Write(setup);
	const std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(text.data()), text.size());
	if (!fileSystem.MakeFolder(*folder) || !resources::WriteFile(fileSystem, path, bytes))
	{
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			SPDLOG_LOGGER_WARN(log, "The setup could not be written to {}", path.generic_string());
		}
		return false;
	}
	return true;
}

} // namespace openblack::machine_setup
