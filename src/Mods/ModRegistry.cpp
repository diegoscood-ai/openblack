/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModRegistry.h"

#include <algorithm>
#include <fstream>
#include <map>

#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"

namespace openblack::mods
{
namespace
{
std::string Trim(std::string_view text)
{
	const auto first = text.find_first_not_of(" \t\r\n");
	if (first == std::string_view::npos)
	{
		return {};
	}
	const auto last = text.find_last_not_of(" \t\r\n");
	return std::string(text.substr(first, last - first + 1));
}

/// "key = value" lines; '#' starts a comment
std::map<std::string, std::string> ReadKeyValues(const std::filesystem::path& path)
{
	std::map<std::string, std::string> values;
	std::ifstream file(path);
	std::string line;
	while (std::getline(file, line))
	{
		const auto hash = line.find('#');
		if (hash != std::string::npos)
		{
			line.resize(hash);
		}
		const auto equals = line.find('=');
		if (equals == std::string::npos)
		{
			continue;
		}
		auto key = Trim(std::string_view(line).substr(0, equals));
		if (!key.empty())
		{
			values[key] = Trim(std::string_view(line).substr(equals + 1));
		}
	}
	return values;
}

bool ParseOnOff(std::string_view value, bool& result)
{
	if (value == "on" || value == "1" || value == "true" || value == "yes")
	{
		result = true;
		return true;
	}
	if (value == "off" || value == "0" || value == "false" || value == "no")
	{
		result = false;
		return true;
	}
	return false;
}

/// A folder of replacement files (see ModRegistry)
class DataMod final: public Mod
{
public:
	DataMod(Info info, std::filesystem::path root)
	    : Mod(std::move(info))
	    , _root(std::move(root))
	{
	}

	void Apply() override {} // files are mounted at start-up (MountDataMods)

	[[nodiscard]] const std::filesystem::path& GetRoot() const noexcept { return _root; }

private:
	std::filesystem::path _root;
};
} // namespace

struct ModRegistry::SavedState
{
	std::map<std::string, std::string> values;
};

ModRegistry::ModRegistry()
    : _saved(std::make_unique<SavedState>())
{
}

ModRegistry::~ModRegistry() = default;

void ModRegistry::Register(std::unique_ptr<Mod> mod)
{
	if (Find(mod->GetInfo().id) != nullptr)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Mod '{}' registered twice", mod->GetInfo().id);
		return;
	}
	_mods.push_back(std::move(mod));
}

void ModRegistry::DiscoverDataMods(const std::filesystem::path& modsDirectory)
{
	_modsDirectory = modsDirectory;
	std::error_code error;
	if (!std::filesystem::is_directory(modsDirectory, error))
	{
		return;
	}
	std::vector<std::filesystem::path> folders;
	for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, error))
	{
		if (entry.is_directory())
		{
			folders.push_back(entry.path());
		}
	}
	std::sort(folders.begin(), folders.end());
	for (const auto& folder : folders)
	{
		const auto manifest = ReadKeyValues(folder / "mod.cfg");
		const auto folderName = folder.filename().string();
		Mod::Info info;
		info.id = "data." + folderName;
		info.name = manifest.contains("name") ? manifest.at("name") : folderName;
		info.description = manifest.contains("description") ? manifest.at("description") : "Replacement files in Mods/" + folderName;
		info.category = "Data mods";
		info.restartRequired = true;
		Register(std::make_unique<DataMod>(std::move(info), folder));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Found data mod '{}' in {}", folderName, folder.generic_string());
	}
}

void ModRegistry::LoadSettings(const std::filesystem::path& settingsPath)
{
	_settingsPath = settingsPath;
	_saved->values = ReadKeyValues(settingsPath);
	for (const auto& [key, value] : _saved->values)
	{
		const auto error = ApplyArgument(key + "=" + value);
		if (!error.empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "{}: {}", settingsPath.generic_string(), error);
		}
	}
}

void ModRegistry::SaveSettings() const
{
	if (_settingsPath.empty())
	{
		return;
	}
	std::ofstream file(_settingsPath, std::ios::trunc);
	file << "# openblack mods: <mod> = on|off, <mod>.<option> = <choice>. Also: --mod <mod>[=off], --mod <mod>.<option>=<choice>\n";
	for (const auto& [key, value] : _saved->values)
	{
		file << key << " = " << value << '\n';
	}
}

std::string ModRegistry::ApplyArgument(std::string_view argument)
{
	const auto equals = argument.find('=');
	const auto key = Trim(argument.substr(0, equals));
	const auto value = equals == std::string_view::npos ? std::string("on") : Trim(argument.substr(equals + 1));

	if (auto* mod = Find(key); mod != nullptr)
	{
		bool enabled = false;
		if (!ParseOnOff(value, enabled))
		{
			return "'" + value + "' is not on or off for mod '" + key + "'";
		}
		mod->_enabled = enabled;
		return {};
	}
	// <mod>.<option>: the mod id itself may contain dots, so try the longest mod id first
	for (auto dot = key.rfind('.'); dot != std::string::npos && dot > 0; dot = key.rfind('.', dot - 1))
	{
		auto* mod = Find(std::string_view(key).substr(0, dot));
		if (mod == nullptr)
		{
			continue;
		}
		const auto optionId = key.substr(dot + 1);
		for (auto& option : mod->_options)
		{
			if (option.id != optionId)
			{
				continue;
			}
			const auto choice = std::find(option.choices.begin(), option.choices.end(), value);
			if (choice == option.choices.end())
			{
				return "'" + value + "' is not a choice of " + key;
			}
			option.value = static_cast<size_t>(choice - option.choices.begin());
			return {};
		}
		return "mod '" + mod->GetInfo().id + "' has no option '" + optionId + "'";
	}
	return "unknown mod '" + key + "'";
}

void ModRegistry::ApplyAll()
{
	for (const auto& mod : _mods)
	{
		mod->Apply();
		if (mod->IsEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Mod on: {}", mod->GetInfo().id);
		}
	}
}

void ModRegistry::SetEnabled(Mod& mod, bool enabled)
{
	mod._enabled = enabled;
	_saved->values[mod.GetInfo().id] = enabled ? "on" : "off";
	mod.Apply();
	SaveSettings();
}

void ModRegistry::SetOption(Mod& mod, size_t optionIndex, size_t choice)
{
	auto& option = mod._options.at(optionIndex);
	option.value = std::min(choice, option.choices.size() - 1);
	_saved->values[mod.GetInfo().id + "." + option.id] = option.choices[option.value];
	mod.Apply();
	SaveSettings();
}

void ModRegistry::MountDataMods(filesystem::FileSystemInterface& fileSystem) const
{
	for (const auto& mod : _mods)
	{
		const auto* dataMod = dynamic_cast<const DataMod*>(mod.get());
		if (dataMod != nullptr && dataMod->IsEnabled())
		{
			fileSystem.AddOverridePath(dataMod->GetRoot());
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Data mod mounted: {}", dataMod->GetRoot().generic_string());
		}
	}
}

Mod* ModRegistry::Find(std::string_view id) const noexcept
{
	const auto it = std::find_if(_mods.begin(), _mods.end(), [id](const auto& mod) { return mod->GetInfo().id == id; });
	return it != _mods.end() ? it->get() : nullptr;
}

} // namespace openblack::mods
