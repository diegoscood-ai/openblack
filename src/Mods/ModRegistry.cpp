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
#include <climits>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include <fmt/format.h>

#include "Api.h"
#include "BuiltinManifests.h"
#include "FileSystem/FileSystemInterface.h"
#include "Manifest.h"
#include "ModLog.h"
#include "Switches.h"

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

std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

/// Mods/<old id> of a renamed built-in mod (an older exe, or a copied Mods folder, makes it again): what the new folder
/// lacks moves there, the rest goes, so it never shows up as a data mod. Returns true if the folder was one of those.
bool MigrateRenamedFolder(const std::filesystem::path& folder)
{
	static const std::map<std::string, std::string, std::less<>> k_Renamed = {
	    {"graphics.hd-people", "graphics.hd-tweaks"}, // 2026-09-30, it does more than villagers now
	};
	const auto renamed = k_Renamed.find(log::Utf8(folder.filename()));
	if (renamed == k_Renamed.end())
	{
		return false;
	}
	std::error_code error;
	const auto target = folder.parent_path() / renamed->second;
	std::filesystem::create_directories(target, error);
	for (const auto& entry : std::filesystem::directory_iterator(folder, error))
	{
		const auto destination = target / entry.path().filename();
		std::ifstream settings(entry.path());
		std::string header;
		// a settings.cfg the older exe wrote for it as a data mod carries no settings of the mod
		const bool dataModStub = entry.path().filename() == "settings.cfg" && std::getline(settings, header) &&
		                         header.find("(data.") != std::string::npos;
		settings.close();
		if (!dataModStub && !std::filesystem::exists(destination, error))
		{
			std::filesystem::rename(entry.path(), destination, error);
		}
	}
	std::filesystem::remove_all(folder, error);
	log::Info(renamed->second, fmt::format("Mods/{} is now Mods/{}: moved and removed", renamed->first, renamed->second));
	return true;
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

/// An old folder of replacement files with a mod.cfg (see ModRegistry)
class DataMod final: public Mod
{
public:
	DataMod(Info info, std::filesystem::path root)
	    : Mod(std::move(info), std::move(root))
	{
	}
};

/// An old folder of files for another mod to read (mod.cfg "module_of = <mod id>"), e.g. more plants for world.foliage.
/// Its mod.cfg may add options for the parent to read: "option.<id> = <label> | <choice>, <choice>... | <default>
/// [| slider]" (GetModules)
class ModuleMod final: public Mod
{
public:
	ModuleMod(Info info, std::filesystem::path root, const std::map<std::string, std::string>& manifest)
	    : Mod(std::move(info), std::move(root))
	{
		for (const auto& [key, value] : manifest)
		{
			if (!key.starts_with("option."))
			{
				continue;
			}
			std::vector<std::string> parts;
			for (size_t start = 0;;)
			{
				const auto bar = value.find('|', start);
				parts.push_back(Trim(std::string_view(value).substr(start, bar == std::string::npos ? bar : bar - start)));
				if (bar == std::string::npos)
				{
					break;
				}
				start = bar + 1;
			}
			ModOption option;
			option.id = key.substr(7);
			option.label = parts[0].empty() ? option.id : parts[0];
			if (parts.size() > 1)
			{
				for (size_t start = 0;;)
				{
					const auto comma = parts[1].find(',', start);
					auto choice = Trim(std::string_view(parts[1]).substr(start, comma == std::string::npos ? comma : comma - start));
					if (!choice.empty())
					{
						option.choices.push_back(std::move(choice));
					}
					if (comma == std::string::npos)
					{
						break;
					}
					start = comma + 1;
				}
			}
			if (option.choices.empty())
			{
				log::Warning(GetInfo().id, fmt::format("module option '{}' has no choices", key));
				continue;
			}
			if (parts.size() > 2)
			{
				const auto found = std::ranges::find(option.choices, parts[2]);
				option.value = found == option.choices.end() ? 0 : static_cast<size_t>(found - option.choices.begin());
			}
			option.slider = parts.size() > 3 && parts[3] == "slider";
			AddOption(std::move(option));
		}
	}
};

std::string DependencyText(const Dependency& dependency)
{
	const auto& range = dependency.range.GetText();
	return range.empty() || range == "*" ? dependency.id : fmt::format("{} {}", dependency.id, range);
}
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
		log::Error(mod->GetInfo().id, "registered twice: the second one is left out");
		return;
	}
	_mods.push_back(std::move(mod));
	SortLoadOrder();
}

void ModRegistry::Discover(const std::filesystem::path& modsDirectory)
{
	_modsDirectory = modsDirectory;

	// the mods that come with openblack, unless a mod.json on disk replaces one of them
	std::map<std::string, std::unique_ptr<Mod>, std::less<>> builtins;
	for (const auto& [folder, json] : builtin::Manifests())
	{
		auto result = ParseManifest(json, modsDirectory / folder, "");
		for (const auto& warning : result.warnings)
		{
			log::Warning(folder, fmt::format("built-in mod.json: {}", warning));
		}
		if (!result.mod)
		{
			for (const auto& error : result.errors)
			{
				log::Error(folder, fmt::format("built-in mod.json: {}", error));
			}
			continue;
		}
		auto id = result.mod->GetInfo().id;
		builtins[id] = std::move(result.mod);
	}

	std::error_code error;
	std::vector<std::filesystem::path> folders;
	if (std::filesystem::is_directory(modsDirectory, error))
	{
		for (const auto& entry : std::filesystem::directory_iterator(modsDirectory, error))
		{
			if (entry.is_directory())
			{
				folders.push_back(entry.path());
			}
		}
	}
	std::ranges::sort(folders);

	// mod.json and modpack.json folders first, so the old module folders find their parent whatever its name
	std::vector<std::pair<std::filesystem::path, std::map<std::string, std::string>>> modules;
	std::vector<std::filesystem::path> others;
	for (const auto& folder : folders)
	{
		if (MigrateRenamedFolder(folder))
		{
			continue;
		}
		if (std::filesystem::exists(folder / "modpack.json", error))
		{
			std::vector<std::string> errors;
			auto pack = ParseModpack(ReadText(folder / "modpack.json"), folder, errors);
			if (!pack)
			{
				_broken.push_back({folder, errors});
				for (const auto& text : errors)
				{
					log::Error(log::Utf8(folder.filename()), fmt::format("modpack.json: {}", text));
				}
				continue;
			}
			std::vector<std::filesystem::path> members;
			for (const auto& entry : std::filesystem::directory_iterator(folder, error))
			{
				if (entry.is_directory())
				{
					members.push_back(entry.path());
				}
			}
			std::ranges::sort(members);
			_packs.push_back(*pack);
			for (const auto& member : members)
			{
				DiscoverFolder(member, pack->id, modules);
			}
			log::Info(pack->id, fmt::format("modpack {} {} found", pack->name, pack->version.ToString()));
			continue;
		}
		others.push_back(folder);
	}
	for (const auto& folder : others)
	{
		const auto name = log::Utf8(folder.filename());
		if (!std::filesystem::exists(folder / "mod.json", error) && builtins.contains(name))
		{
			continue; // Mods/<built-in id>: that mod's own files and settings
		}
		DiscoverFolder(folder, "", modules);
	}

	// the built-in mods that no mod.json on disk replaced
	for (auto& [id, mod] : builtins)
	{
		if (Find(id) == nullptr)
		{
			mod->_root = modsDirectory / id;
			_mods.push_back(std::move(mod));
		}
	}

	// the old module folders, now that every parent is known
	for (const auto& [folder, manifest] : modules)
	{
		const auto folderName = log::Utf8(folder.filename());
		const auto& parent = manifest.at("module_of");
		const auto* parentMod = Find(parent);
		if (parentMod == nullptr)
		{
			log::Warning(folderName, fmt::format("module of '{}', which is not installed: left out", parent));
			continue;
		}
		Mod::Info info;
		info.id = folderName;
		info.name = manifest.contains("name") ? manifest.at("name") : folderName;
		info.description = manifest.contains("description") ? manifest.at("description") : "Module of " + parent;
		info.category = parentMod->GetInfo().category;
		info.parent = parent;
		info.pack = parentMod->GetInfo().pack;
		info.kind = Mod::Kind::Module;
		if (std::filesystem::exists(folder / "icon.png", error))
		{
			info.icon = folder / "icon.png";
		}
		_mods.push_back(std::make_unique<ModuleMod>(std::move(info), folder, manifest));
	}

	for (auto& pack : _packs)
	{
		for (const auto& mod : _mods)
		{
			if (mod->GetInfo().pack == pack.id)
			{
				pack.mods.push_back(mod->GetInfo().id);
			}
		}
	}
	SortLoadOrder();
	log::Info("", fmt::format("{} mods and {} modpacks in {}", _mods.size(), _packs.size(), log::Utf8(modsDirectory)));
}

void ModRegistry::DiscoverFolder(const std::filesystem::path& folder, std::string_view pack,
                                 std::vector<std::pair<std::filesystem::path, std::map<std::string, std::string>>>& modules)
{
	std::error_code error;
	const auto folderName = log::Utf8(folder.filename());
	if (std::filesystem::exists(folder / "mod.json", error))
	{
		auto result = ParseManifest(ReadText(folder / "mod.json"), folder, pack);
		for (const auto& warning : result.warnings)
		{
			log::Warning(folderName, fmt::format("mod.json: {}", warning));
		}
		if (!result.mod)
		{
			_broken.push_back({folder, result.errors});
			for (const auto& text : result.errors)
			{
				log::Error(folderName, fmt::format("mod.json: {}", text));
			}
			return;
		}
		const auto& id = result.mod->GetInfo().id;
		if (Find(id) != nullptr)
		{
			const auto text = fmt::format("there is already a mod '{}': this one is left out", id);
			_broken.push_back({folder, {text}});
			log::Error(folderName, text);
			return;
		}
		if (id != folderName)
		{
			log::Warning(id, fmt::format("its folder is called '{}': name it as its id", folderName));
		}
		log::Info(id, fmt::format("{} {} found", result.mod->GetInfo().name, result.mod->GetInfo().version.ToString()));
		_mods.push_back(std::move(result.mod));
		return;
	}

	const auto manifest = ReadKeyValues(folder / "mod.cfg");
	if (manifest.contains("module_of"))
	{
		modules.emplace_back(folder, manifest);
		return;
	}
	Mod::Info info;
	info.id = "data." + folderName;
	info.name = manifest.contains("name") ? manifest.at("name") : folderName;
	info.description = manifest.contains("description") ? manifest.at("description")
	                                                     : "Replacement files in Mods/" + folderName;
	info.category = "Data mods";
	info.restartRequired = true;
	info.pack = std::string(pack);
	info.kind = Mod::Kind::Data;
	if (std::filesystem::exists(folder / "icon.png", error))
	{
		info.icon = folder / "icon.png";
	}
	_mods.push_back(std::make_unique<DataMod>(std::move(info), folder));
}

std::filesystem::path ModRegistry::GetModDirectory(const Mod& mod) const
{
	return mod.GetRoot().empty() ? _modsDirectory / mod.GetInfo().id : mod.GetRoot();
}

std::filesystem::path ModRegistry::GetModFilesDirectory(std::string_view id) const
{
	const auto* mod = Find(id);
	return mod != nullptr ? GetModDirectory(*mod) : _modsDirectory / id;
}

bool ModRegistry::IsActive(const Mod& mod) const
{
	if (!mod.IsEnabled() || !mod.GetBlockedReason().empty())
	{
		return false;
	}
	if (mod.GetInfo().parent.empty())
	{
		return true;
	}
	const auto* parent = Find(mod.GetInfo().parent);
	return parent != nullptr && IsActive(*parent);
}

std::vector<std::filesystem::path> ModRegistry::GetModuleDirectories(std::string_view parentId) const
{
	std::vector<std::filesystem::path> directories;
	for (const auto& module : GetModules(parentId))
	{
		directories.push_back(module.directory);
	}
	return directories;
}

std::vector<ModRegistry::Module> ModRegistry::GetModules(std::string_view parentId) const
{
	std::vector<Module> modules;
	for (const auto* mod : _order)
	{
		if (mod->GetInfo().parent == parentId && IsActive(*mod))
		{
			Module module {GetModDirectory(*mod), {}};
			for (const auto& option : mod->GetOptions())
			{
				module.options[option.id] = option.choices.at(option.value);
			}
			modules.push_back(std::move(module));
		}
	}
	return modules;
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
	log::Info("", fmt::format("split {} into the mods' settings.cfg files", log::Utf8(legacyPath)));
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
				log::Warning(mod->GetInfo().id, fmt::format("{}: {}", log::Utf8(path), message));
			}
		}
	}

	_userOrder.clear();
	std::ifstream file(_modsDirectory / "load_order.cfg");
	for (std::string line; std::getline(file, line);)
	{
		line = Trim(line.substr(0, line.find('#')));
		if (!line.empty())
		{
			_userOrder.push_back(line);
		}
	}
	SortLoadOrder();
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

void ModRegistry::SaveLoadOrder() const
{
	if (_modsDirectory.empty())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(_modsDirectory, error);
	std::ofstream file(_modsDirectory / "load_order.cfg", std::ios::trunc);
	file << "# Load order of the mods (the Load order tab of the Mods window). A mod's dependencies always load first.\n";
	for (const auto* mod : _order)
	{
		file << mod->GetInfo().id << '\n';
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

void ModRegistry::Resolve()
{
	std::map<std::string, std::string, std::less<>> before;
	for (const auto& mod : _mods)
	{
		before[mod->GetInfo().id] = mod->_blockedReason;
		mod->_blockedReason.clear();
	}
	const auto apiOk = [](const Mod& mod) {
		if (mod.GetInfo().api.empty())
		{
			return true;
		}
		const auto range = VersionRange::Parse(mod.GetInfo().api);
		return range && range->Contains(k_ApiVersion);
	};
	const auto on = [](const Mod& mod) {
		return mod.IsEnabled() && mod.GetBlockedReason().empty();
	};

	// what a dependency needs of the other mod: its blocking reason, or empty when it is fine
	const auto requiredProblem = [this](const Dependency& dependency) -> std::string {
		const auto* other = Find(dependency.id);
		if (other == nullptr)
		{
			return fmt::format("needs {}, which is not installed", DependencyText(dependency));
		}
		if (!dependency.range.Contains(other->GetInfo().version))
		{
			return fmt::format("needs {}, found {}", DependencyText(dependency), other->GetInfo().version.ToString());
		}
		if (!other->IsEnabled())
		{
			return fmt::format("needs {}, which is off", other->GetInfo().name);
		}
		if (!other->GetBlockedReason().empty())
		{
			return fmt::format("needs {}, which is blocked", other->GetInfo().name);
		}
		if (!IsActive(*other))
		{
			return fmt::format("needs {}, which is not active", other->GetInfo().name);
		}
		return {};
	};

	// blocking spreads (a mod that needs a blocked mod is blocked too), so each phase goes on until nothing changes
	const auto dependencies = [&]() {
		for (bool changed = true; changed;)
		{
			changed = false;
			for (const auto& mod : _mods)
			{
				if (!on(*mod))
				{
					continue;
				}
				std::string reason;
				if (!apiOk(*mod))
				{
					reason = fmt::format("written for mod API {}, this openblack has {}", mod->GetInfo().api,
					                     k_ApiVersion.ToString());
				}
				for (const auto& dependency : mod->GetInfo().dependencies)
				{
					if (reason.empty() && dependency.kind == Dependency::Kind::Required)
					{
						reason = requiredProblem(dependency);
					}
				}
				if (!reason.empty())
				{
					mod->_blockedReason = reason;
					changed = true;
				}
			}
		}
	};
	dependencies();
	// incompatibilities only against mods that stay on after their own dependencies are checked
	bool blockedAny = false;
	for (const auto& mod : _mods)
	{
		if (!on(*mod))
		{
			continue;
		}
		for (const auto& dependency : mod->GetInfo().dependencies)
		{
			const auto* other = Find(dependency.id);
			if (dependency.kind == Dependency::Kind::Incompatible && other != nullptr && other != mod.get() && on(*other) &&
			    dependency.range.Contains(other->GetInfo().version))
			{
				mod->_blockedReason = fmt::format("does not work with {}, which is on", other->GetInfo().name);
				blockedAny = true;
				break;
			}
		}
	}
	if (blockedAny)
	{
		dependencies(); // what needed a mod blocked as incompatible
	}

	for (const auto& mod : _mods)
	{
		if (mod->_blockedReason != before[mod->GetInfo().id] && !mod->_blockedReason.empty())
		{
			log::Warning(mod->GetInfo().id, fmt::format("blocked: {}", mod->_blockedReason));
		}
	}
}

void ModRegistry::SortLoadOrder()
{
	// before[i] = mods that must load before mod i
	const size_t count = _mods.size();
	std::map<std::string_view, size_t> index;
	for (size_t i = 0; i < count; ++i)
	{
		index[_mods[i]->GetInfo().id] = i;
	}
	std::vector<std::set<size_t>> before(count);
	const auto edge = [&](std::string_view first, size_t then) {
		if (const auto it = index.find(first); it != index.end() && it->second != then)
		{
			before[then].insert(it->second);
		}
	};
	for (size_t i = 0; i < count; ++i)
	{
		const auto& info = _mods[i]->GetInfo();
		for (const auto& dependency : info.dependencies)
		{
			if (dependency.kind != Dependency::Kind::Incompatible)
			{
				edge(dependency.id, i);
			}
		}
		for (const auto& id : info.loadAfter)
		{
			edge(id, i);
		}
		for (const auto& id : info.loadBefore)
		{
			if (const auto it = index.find(id); it != index.end() && it->second != i)
			{
				before[it->second].insert(i);
			}
		}
		if (!info.parent.empty())
		{
			edge(info.parent, i);
		}
	}

	// the user's order first, then by id
	const auto priority = [&](size_t i) {
		const auto& id = _mods[i]->GetInfo().id;
		const auto it = std::ranges::find(_userOrder, id);
		return std::pair<size_t, std::string_view>(
		    it != _userOrder.end() ? static_cast<size_t>(it - _userOrder.begin()) : SIZE_MAX, id);
	};
	std::vector<bool> placed(count, false);
	_order.clear();
	for (size_t round = 0; round < count; ++round)
	{
		size_t best = SIZE_MAX;
		for (size_t i = 0; i < count; ++i)
		{
			if (placed[i] || !std::ranges::all_of(before[i], [&](size_t b) { return placed[b]; }))
			{
				continue;
			}
			if (best == SIZE_MAX || priority(i) < priority(best))
			{
				best = i;
			}
		}
		if (best == SIZE_MAX)
		{
			// a cycle: what is left goes in by id, and the log says which mods
			std::string cycle;
			for (size_t i = 0; i < count; ++i)
			{
				if (!placed[i])
				{
					cycle += (cycle.empty() ? "" : ", ") + _mods[i]->GetInfo().id;
					placed[i] = true;
					_order.push_back(_mods[i].get());
				}
			}
			log::Warning("", fmt::format("these mods depend on each other in a circle, loaded by id: {}", cycle));
			break;
		}
		placed[best] = true;
		_order.push_back(_mods[best].get());
	}
}

void ModRegistry::MoveInLoadOrder(const Mod& mod, int delta)
{
	// the user's order becomes the whole current order, with the mod moved
	_userOrder.clear();
	for (const auto* each : _order)
	{
		_userOrder.push_back(each->GetInfo().id);
	}
	const auto it = std::ranges::find(_userOrder, mod.GetInfo().id);
	if (it == _userOrder.end())
	{
		return;
	}
	const auto from = static_cast<int>(it - _userOrder.begin());
	const auto to = std::clamp(from + delta, 0, static_cast<int>(_userOrder.size()) - 1);
	auto id = *it;
	_userOrder.erase(it);
	_userOrder.insert(_userOrder.begin() + to, std::move(id));
	SortLoadOrder();
	SaveLoadOrder();
	ApplyAll();
}

void ModRegistry::ApplySwitches()
{
	// every switch at its default, then what each active mod sets, in load order; only real changes are set, so a
	// switch with an onChange (MSAA, grounding) runs it once
	std::map<std::string, double, std::less<>> target;
	for (const auto& value : switches::All())
	{
		target[value.name] = value.defaultValue;
	}
	for (const auto* mod : _order)
	{
		if (IsActive(*mod))
		{
			mod->CollectSwitches(target);
		}
	}
	for (const auto& [name, value] : target)
	{
		switches::Set(name, value); // it compares with the clamped value and runs onChange only on a real change
	}
}

void ModRegistry::ApplyAll()
{
	Resolve();
	ApplySwitches();
	// a mod that is not active any more falls silent (its sound owner, Api.h)
	for (const auto* mod : _order)
	{
		if (!IsActive(*mod))
		{
			api::StopSounds(*mod);
		}
	}
	for (auto* mod : _order)
	{
		mod->Apply();
	}
	for (const auto* mod : _order)
	{
		if (IsActive(*mod))
		{
			log::Info(mod->GetInfo().id, "on");
		}
	}
}

void ModRegistry::SetEnabled(Mod& mod, bool enabled)
{
	mod._enabled = enabled;
	_saved->values[mod.GetInfo().id]["enabled"] = enabled ? "on" : "off";
	ApplyAll();
	SaveSettings(mod);
}

void ModRegistry::SetOption(Mod& mod, size_t optionIndex, size_t choice)
{
	auto& option = mod._options.at(optionIndex);
	option.value = std::min(choice, option.choices.size() - 1);
	_saved->values[mod.GetInfo().id][option.id] = option.choices[option.value];
	ApplyAll();
	SaveSettings(mod);
}

std::string ModRegistry::RestartState(const Mod& mod) const
{
	std::string state = IsActive(mod) ? "on" : "off";
	for (const auto& option : mod.GetOptions())
	{
		state += "|" + option.choices.at(option.value);
	}
	return state;
}

void ModRegistry::MarkStarted()
{
	_started.clear();
	for (const auto& mod : _mods)
	{
		if (mod->GetInfo().restartRequired)
		{
			_started[mod->GetInfo().id] = RestartState(*mod);
		}
	}
}

std::vector<const Mod*> ModRegistry::PendingRestart() const
{
	std::vector<const Mod*> pending;
	for (const auto& mod : _mods)
	{
		const auto it = _started.find(mod->GetInfo().id);
		if (it != _started.end() && it->second != RestartState(*mod))
		{
			pending.push_back(mod.get());
		}
	}
	return pending;
}

bool ModRegistry::SetRuntimeSwitch(Mod& mod, std::string_view name, double value)
{
	if (switches::Find(name) == nullptr || !std::isfinite(value))
	{
		return false;
	}
	mod._runtimeSwitches[std::string(name)] = value;
	RefreshSwitches();
	return true;
}

void ModRegistry::RefreshSwitches()
{
	ApplySwitches();
}

const Modpack* ModRegistry::FindModpack(std::string_view id) const noexcept
{
	const auto it = std::ranges::find(_packs, id, &Modpack::id);
	return it != _packs.end() ? &*it : nullptr;
}

bool ModRegistry::IsModpackEnabled(const Modpack& pack) const
{
	return !pack.mods.empty() && std::ranges::all_of(pack.mods, [this](const std::string& id) {
		const auto* mod = Find(id);
		return mod != nullptr && mod->IsEnabled();
	});
}

void ModRegistry::SetModpackEnabled(const Modpack& pack, bool enabled)
{
	for (const auto& id : pack.mods)
	{
		if (auto* mod = Find(id); mod != nullptr)
		{
			mod->_enabled = enabled;
			_saved->values[id]["enabled"] = enabled ? "on" : "off";
		}
	}
	ApplyAll();
	for (const auto& id : pack.mods)
	{
		if (const auto* mod = Find(id); mod != nullptr)
		{
			SaveSettings(*mod);
		}
	}
}

void ModRegistry::MountDataMods(filesystem::FileSystemInterface& fileSystem) const
{
	// an old data mod is its whole folder; a mod.json mod, its replace/ folder
	std::error_code error;
	for (const auto* mod : _order)
	{
		if (!IsActive(*mod))
		{
			continue;
		}
		std::filesystem::path folder;
		if (mod->GetInfo().kind == Mod::Kind::Data)
		{
			folder = mod->GetRoot();
		}
		else if (mod->GetInfo().kind == Mod::Kind::Package && std::filesystem::is_directory(mod->GetRoot() / "replace", error))
		{
			folder = mod->GetRoot() / "replace";
		}
		if (!folder.empty())
		{
			fileSystem.AddOverridePath(folder);
			log::Info(mod->GetInfo().id, fmt::format("replacement files mounted from {}", log::Utf8(folder)));
		}
	}
}

Mod* ModRegistry::Find(std::string_view id) const noexcept
{
	const auto it = std::find_if(_mods.begin(), _mods.end(), [id](const auto& mod) { return mod->GetInfo().id == id; });
	return it != _mods.end() ? it->get() : nullptr;
}

} // namespace openblack::mods
