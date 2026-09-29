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
	/// mod id -> "enabled" / option id -> value
	std::map<std::string, std::map<std::string, std::string>> values;
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
		const auto folderName = folder.filename().string();
		if (Find(folderName) != nullptr)
		{
			continue; // Mods/<built-in mod id>: that mod's own files (e.g. Mods/world.foliage)
		}
		const auto manifest = ReadKeyValues(folder / "mod.cfg");
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

std::filesystem::path ModRegistry::GetModDirectory(const Mod& mod) const
{
	if (const auto* dataMod = dynamic_cast<const DataMod*>(&mod); dataMod != nullptr)
	{
		return dataMod->GetRoot();
	}
	return _modsDirectory / mod.GetInfo().id;
}

void ModRegistry::ImportLegacySettings(const std::filesystem::path& legacyPath)
{
	std::error_code error;
	if (!std::filesystem::exists(legacyPath, error))
	{
		return;
	}
	for (const auto& [key, value] : ReadKeyValues(legacyPath))
	{
		// "<mod>" or "<mod>.<option>"; the id itself may contain dots, so try the whole key first
		if (Find(key) != nullptr)
		{
			_saved->values[key]["enabled"] = value;
			continue;
		}
		for (auto dot = key.rfind('.'); dot != std::string::npos && dot > 0; dot = key.rfind('.', dot - 1))
		{
			if (Find(std::string_view(key).substr(0, dot)) != nullptr)
			{
				_saved->values[key.substr(0, dot)][key.substr(dot + 1)] = value;
				break;
			}
		}
	}
	for (const auto& [id, values] : _saved->values)
	{
		if (const auto* mod = Find(id); mod != nullptr && !std::filesystem::exists(GetModDirectory(*mod) / "settings.cfg", error))
		{
			SaveSettings(*mod);
		}
	}
	std::filesystem::remove(legacyPath, error);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Split {} into the mods' settings.cfg files", legacyPath.generic_string());
}

void ModRegistry::LoadSettings()
{
	for (const auto& mod : _mods)
	{
		const auto path = GetModDirectory(*mod) / "settings.cfg";
		std::error_code error;
		if (!std::filesystem::exists(path, error))
		{
			SaveSettings(*mod); // every mod gets its folder, with its current (default) state
			continue;
		}
		auto& saved = _saved->values[mod->GetInfo().id];
		saved = ReadKeyValues(path);
		for (const auto& [key, value] : saved)
		{
			const auto argument = key == "enabled" ? mod->GetInfo().id + "=" + value : mod->GetInfo().id + "." + key + "=" + value;
			if (const auto message = ApplyArgument(argument); !message.empty())
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "{}: {}", path.generic_string(), message);
			}
		}
	}
}

void ModRegistry::SaveSettings(const Mod& mod) const
{
	if (_modsDirectory.empty())
	{
		return;
	}
	const auto directory = GetModDirectory(mod);
	std::error_code error;
	std::filesystem::create_directories(directory, error);
	const auto found = _saved->values.find(mod.GetInfo().id);
	const auto saved = [&](const std::string& key, const std::string& fallback) {
		if (found != _saved->values.end())
		{
			if (const auto value = found->second.find(key); value != found->second.end())
			{
				return value->second;
			}
		}
		return fallback;
	};
	std::ofstream file(directory / "settings.cfg", std::ios::trunc);
	file << "# " << mod.GetInfo().name << " (" << mod.GetInfo().id << "). For one session only: --mod " << mod.GetInfo().id
	     << "[=off]";
	if (!mod.GetOptions().empty())
	{
		file << ", --mod " << mod.GetInfo().id << ".<option>=<choice>";
	}
	file << '\n';
	file << "enabled = " << saved("enabled", mod.IsEnabled() ? "on" : "off") << '\n';
	for (const auto& option : mod.GetOptions())
	{
		std::string choices;
		for (const auto& choice : option.choices)
		{
			choices += (choices.empty() ? "" : ", ") + choice;
		}
		file << option.id << " = " << saved(option.id, option.choices.at(option.value)) << "  # " << option.label << ": "
		     << choices << '\n';
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
	_saved->values[mod.GetInfo().id]["enabled"] = enabled ? "on" : "off";
	mod.Apply();
	SaveSettings(mod);
}

void ModRegistry::SetOption(Mod& mod, size_t optionIndex, size_t choice)
{
	auto& option = mod._options.at(optionIndex);
	option.value = std::min(choice, option.choices.size() - 1);
	_saved->values[mod.GetInfo().id][option.id] = option.choices[option.value];
	mod.Apply();
	SaveSettings(mod);
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
