/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Manifest.h"

#include <algorithm>
#include <cctype>
#include <charconv>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "Switches.h"

namespace openblack::mods
{
namespace
{
using Json = nlohmann::json;
using SwitchValues = std::map<std::string, double, std::less<>>;

std::string& Language()
{
	static std::string language = "en";
	return language;
}

/// "text" or {"en": "text", "es": "texto"}: the current language, else English, else the first one
std::string Localise(const Json& value)
{
	if (value.is_string())
	{
		return value.get<std::string>();
	}
	if (value.is_object() && !value.empty())
	{
		for (const auto& language : {Language(), std::string("en")})
		{
			if (const auto it = value.find(language); it != value.end() && it->is_string())
			{
				return it->get<std::string>();
			}
		}
		if (value.begin()->is_string())
		{
			return value.begin()->get<std::string>();
		}
	}
	return {};
}

bool ValidId(std::string_view id)
{
	if (id.size() < 2 || id.size() > 64)
	{
		return false;
	}
	return std::ranges::all_of(id, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
	});
}

/// The first number in a choice's text: "x10" -> 10, "4x" -> 4, "10s" -> 10, "0.5" -> 0.5
std::optional<double> NumberIn(std::string_view text)
{
	const auto first = text.find_first_of("-0123456789.");
	if (first == std::string_view::npos)
	{
		return std::nullopt;
	}
	double value = 0.0;
	const auto [end, error] = std::from_chars(text.data() + first, text.data() + text.size(), value);
	if (error != std::errc())
	{
		return std::nullopt;
	}
	return value;
}

std::optional<double> JsonNumber(const Json& value)
{
	if (value.is_boolean())
	{
		return value.get<bool>() ? 1.0 : 0.0;
	}
	if (value.is_number())
	{
		return value.get<double>();
	}
	return std::nullopt;
}

std::string ChoiceText(const Json& value)
{
	if (value.is_string())
	{
		return value.get<std::string>();
	}
	if (value.is_boolean())
	{
		return value.get<bool>() ? "on" : "off";
	}
	return value.dump();
}

std::vector<std::string> StringList(const Json& object, const char* key)
{
	std::vector<std::string> list;
	if (const auto it = object.find(key); it != object.end())
	{
		if (it->is_string())
		{
			list.push_back(it->get<std::string>());
		}
		else if (it->is_array())
		{
			for (const auto& item : *it)
			{
				if (item.is_string())
				{
					list.push_back(item.get<std::string>());
				}
			}
		}
	}
	return list;
}

std::filesystem::path IconOf(const Json& object, const std::filesystem::path& root)
{
	std::error_code error;
	if (const auto it = object.find("icon"); it != object.end() && it->is_string() && !root.empty())
	{
		const auto path = root / std::filesystem::path(reinterpret_cast<const char8_t*>(it->get<std::string>().c_str()));
		return std::filesystem::exists(path, error) ? path : std::filesystem::path {};
	}
	if (!root.empty() && std::filesystem::exists(root / "icon.png", error))
	{
		return root / "icon.png";
	}
	return {};
}

/// A mod made from a mod.json: its options and switches bound to engine switches (Switches.h)
class PackageMod final: public Mod
{
public:
	explicit PackageMod(Info info, std::filesystem::path root)
	    : Mod(std::move(info), std::move(root))
	{
	}

	void AddBoundOption(ModOption option, std::vector<SwitchValues> perChoice)
	{
		_perChoice.push_back(std::move(perChoice));
		AddOption(std::move(option));
	}

	void SetConstantSwitches(SwitchValues switches) { _switches = std::move(switches); }

	void CollectSwitches(SwitchValues& switches) const override
	{
		for (const auto& [name, value] : _switches)
		{
			switches[name] = value;
		}
		const auto& options = GetOptions();
		for (size_t i = 0; i < options.size(); ++i)
		{
			const auto& perChoice = _perChoice.at(i);
			if (options[i].value < perChoice.size())
			{
				for (const auto& [name, value] : perChoice[options[i].value])
				{
					switches[name] = value;
				}
			}
		}
		Mod::CollectSwitches(switches); // what its script or library set, last
	}

private:
	SwitchValues _switches;
	std::vector<std::vector<SwitchValues>> _perChoice; ///< per option, per choice
};

/// One "bind" entry of an option: switch name -> (choice -> value) or a value, or a bare switch name
void ReadBinding(const std::string& switchName, const Json* values, const ModOption& option, bool isBool,
                 std::vector<SwitchValues>& perChoice, std::vector<std::string>& warnings)
{
	if (switches::Find(switchName) == nullptr)
	{
		warnings.push_back(fmt::format("option '{}': there is no engine switch '{}'", option.id, switchName));
		return;
	}
	for (size_t c = 0; c < option.choices.size(); ++c)
	{
		const auto& choice = option.choices[c];
		std::optional<double> value;
		if (values == nullptr)
		{
			// a bare switch name: on/off for a bool option, else the number in the choice ("x10" -> 10)
			value = isBool ? (choice == "on" ? std::optional<double>(1.0) : std::nullopt) : NumberIn(choice);
			if (!isBool && !value)
			{
				warnings.push_back(
				    fmt::format("option '{}': choice '{}' has no number for switch '{}'", option.id, choice, switchName));
			}
		}
		else if (values->is_object())
		{
			if (const auto it = values->find(choice); it != values->end())
			{
				value = JsonNumber(*it);
			}
		}
		else if (isBool && choice == "on")
		{
			value = JsonNumber(*values); // "bind": {"switch": -1.0}: that value while on
		}
		if (value)
		{
			perChoice[c][switchName] = *value;
		}
	}
}

ModOption ReadOption(const Json& entry, std::vector<SwitchValues>& perChoice, std::vector<std::string>& warnings,
                     bool& ok)
{
	ok = false;
	ModOption option;
	if (!entry.is_object() || !entry.contains("id") || !entry["id"].is_string())
	{
		warnings.push_back("an option without an \"id\" was left out");
		return option;
	}
	option.id = entry["id"].get<std::string>();
	option.label = entry.contains("label") ? Localise(entry["label"]) : option.id;
	if (option.label.empty())
	{
		option.label = option.id;
	}
	option.description = entry.contains("description") ? Localise(entry["description"]) : "";
	const std::string type = entry.value("type", std::string("choice"));
	const bool isBool = type == "bool";
	option.slider = type == "slider";
	if (isBool)
	{
		option.choices = {"on", "off"};
	}
	else if (const auto it = entry.find("values"); it != entry.end() && it->is_array())
	{
		for (const auto& value : *it)
		{
			option.choices.push_back(ChoiceText(value));
		}
	}
	if (option.choices.empty())
	{
		warnings.push_back(fmt::format("option '{}' has no \"values\" and was left out", option.id));
		return option;
	}
	if (const auto it = entry.find("default"); it != entry.end())
	{
		if (it->is_number_integer() && !isBool)
		{
			option.value = std::min(static_cast<size_t>(std::max<int64_t>(0, it->get<int64_t>())), option.choices.size() - 1);
		}
		else
		{
			const auto text = ChoiceText(*it);
			const auto found = std::ranges::find(option.choices, text);
			if (found != option.choices.end())
			{
				option.value = static_cast<size_t>(found - option.choices.begin());
			}
			else
			{
				warnings.push_back(fmt::format("option '{}': default '{}' is not one of its values", option.id, text));
			}
		}
	}

	perChoice.assign(option.choices.size(), {});
	if (const auto it = entry.find("bind"); it != entry.end())
	{
		if (it->is_string())
		{
			ReadBinding(it->get<std::string>(), nullptr, option, isBool, perChoice, warnings);
		}
		else if (it->is_object())
		{
			for (const auto& [name, values] : it->items())
			{
				ReadBinding(name, values.is_null() ? nullptr : &values, option, isBool, perChoice, warnings);
			}
		}
		else
		{
			warnings.push_back(fmt::format("option '{}': \"bind\" must be a switch name or an object", option.id));
		}
	}
	ok = true;
	return option;
}

void ReadDependencies(const Json& object, const char* key, Dependency::Kind kind, Mod::Info& info,
                      std::vector<std::string>& errors)
{
	const auto it = object.find(key);
	if (it == object.end())
	{
		return;
	}
	if (!it->is_object())
	{
		errors.push_back(fmt::format("\"{}\" must be an object of id: version range", key));
		return;
	}
	for (const auto& [id, rangeText] : it->items())
	{
		const auto text = rangeText.is_string() ? rangeText.get<std::string>() : std::string("*");
		auto range = VersionRange::Parse(text);
		if (!range)
		{
			errors.push_back(fmt::format("\"{}\": '{}' is not a version range for '{}'", key, text, id));
			continue;
		}
		info.dependencies.push_back({id, std::move(*range), kind});
	}
}
} // namespace

void SetLanguage(std::string language)
{
	Language() = std::move(language);
}

const std::string& GetLanguage()
{
	return Language();
}

static ManifestResult ParseManifestUnchecked(std::string_view text, const std::filesystem::path& root, std::string_view pack)
{
	ManifestResult result;
	Json json = Json::parse(text, nullptr, false, true); // no exceptions, comments allowed
	if (json.is_discarded() || !json.is_object())
	{
		result.errors.push_back("mod.json is not a valid JSON object");
		return result;
	}
	if (json.value("schema", 1) > k_ManifestSchema)
	{
		result.errors.push_back(fmt::format("schema {} is newer than this openblack reads ({})", json.value("schema", 1),
		                                    k_ManifestSchema));
		return result;
	}

	Mod::Info info;
	info.kind = Mod::Kind::Package;
	info.pack = std::string(pack);
	if (!json.contains("id") || !json["id"].is_string() || !ValidId(json["id"].get<std::string>()))
	{
		result.errors.push_back("\"id\" is missing or not 2-64 of a-z 0-9 . - _");
		return result;
	}
	info.id = json["id"].get<std::string>();
	info.name = json.contains("name") ? Localise(json["name"]) : info.id;
	if (info.name.empty())
	{
		info.name = info.id;
	}
	info.description = json.contains("description") ? Localise(json["description"]) : "";
	info.category = json.contains("category") ? Localise(json["category"]) : "Other";
	if (const auto it = json.find("version"); it != json.end())
	{
		const auto version = it->is_string() ? Version::Parse(it->get<std::string>()) : std::nullopt;
		if (!version)
		{
			result.errors.push_back("\"version\" is not a version like \"1.2.0\"");
			return result;
		}
		info.version = *version;
	}
	info.authors = StringList(json, "authors");
	info.url = json.value("url", std::string());
	info.icon = IconOf(json, root);
	info.api = json.value("api", std::string());
	if (!info.api.empty())
	{
		if (!VersionRange::Parse(info.api))
		{
			result.errors.push_back(fmt::format("\"api\": '{}' is not a version range", info.api));
			return result;
		}
	}
	info.restartRequired = json.value("restart_required", false);
	info.enabledByDefault = json.value("enabled_by_default", false);
	info.parent = json.value("parent", std::string());
	info.loadAfter = StringList(json, "load_after");
	info.loadBefore = StringList(json, "load_before");
	info.provides = StringList(json, "provides");
	if (const auto it = json.find("entry"); it != json.end() && it->is_object())
	{
		const auto entryPath = [&root](const std::string& relative) {
			return root.empty() ? std::filesystem::path {}
			                    : root / std::filesystem::path(reinterpret_cast<const char8_t*>(relative.c_str()));
		};
		if (const auto lua = it->find("lua"); lua != it->end() && lua->is_string())
		{
			info.luaEntry = entryPath(lua->get<std::string>());
		}
		if (const auto native = it->find("native"); native != it->end())
		{
#if defined(_WIN32)
			constexpr const char* k_Platform = "windows";
#elif defined(__APPLE__)
			constexpr const char* k_Platform = "macos";
#else
			constexpr const char* k_Platform = "linux";
#endif
			if (native->is_string())
			{
				info.nativeEntry = entryPath(native->get<std::string>());
			}
			else if (const auto file = native->find(k_Platform); native->is_object() && file != native->end() && file->is_string())
			{
				info.nativeEntry = entryPath(file->get<std::string>());
			}
		}
	}
	if (const auto it = json.find("replace"); it != json.end())
	{
		if (it->is_object())
		{
			info.replaceJson = it->dump();
		}
		else
		{
			result.warnings.push_back("\"replace\" must be an object (meshes, textures, objects)");
		}
	}
	ReadDependencies(json, "dependencies", Dependency::Kind::Required, info, result.errors);
	ReadDependencies(json, "optional", Dependency::Kind::Optional, info, result.errors);
	ReadDependencies(json, "incompatible", Dependency::Kind::Incompatible, info, result.errors);
	if (!result.errors.empty())
	{
		return result;
	}

	// a script, a library or replacements are read once, at start-up
	info.restartRequired |= json.contains("entry") || !info.replaceJson.empty();
	auto mod = std::make_unique<PackageMod>(std::move(info), root);

	if (const auto it = json.find("switches"); it != json.end())
	{
		SwitchValues switches;
		if (it->is_object())
		{
			for (const auto& [name, value] : it->items())
			{
				const auto number = JsonNumber(value);
				if (switches::Find(name) == nullptr)
				{
					result.warnings.push_back(fmt::format("\"switches\": there is no engine switch '{}'", name));
				}
				else if (!number)
				{
					result.warnings.push_back(fmt::format("\"switches\": '{}' needs a number or true/false", name));
				}
				else
				{
					switches[name] = *number;
				}
			}
		}
		else
		{
			result.warnings.push_back("\"switches\" must be an object of switch: value");
		}
		mod->SetConstantSwitches(std::move(switches));
	}

	if (const auto it = json.find("options"); it != json.end() && it->is_array())
	{
		for (const auto& entry : *it)
		{
			std::vector<SwitchValues> perChoice;
			bool ok = false;
			auto option = ReadOption(entry, perChoice, result.warnings, ok);
			if (ok)
			{
				mod->AddBoundOption(std::move(option), std::move(perChoice));
			}
		}
	}

	result.mod = std::move(mod);
	return result;
}

static std::optional<Modpack> ParseModpackUnchecked(std::string_view text, const std::filesystem::path& root,
                                    std::vector<std::string>& errors)
{
	Json json = Json::parse(text, nullptr, false, true);
	if (json.is_discarded() || !json.is_object())
	{
		errors.push_back("modpack.json is not a valid JSON object");
		return std::nullopt;
	}
	if (!json.contains("id") || !json["id"].is_string() || !ValidId(json["id"].get<std::string>()))
	{
		errors.push_back("\"id\" is missing or not 2-64 of a-z 0-9 . - _");
		return std::nullopt;
	}
	Modpack pack;
	pack.id = json["id"].get<std::string>();
	pack.name = json.contains("name") ? Localise(json["name"]) : pack.id;
	pack.description = json.contains("description") ? Localise(json["description"]) : "";
	pack.category = json.contains("category") ? Localise(json["category"]) : "Other";
	if (const auto it = json.find("version"); it != json.end())
	{
		const auto version = it->is_string() ? Version::Parse(it->get<std::string>()) : std::nullopt;
		if (!version)
		{
			errors.push_back("\"version\" is not a version like \"1.2.0\"");
			return std::nullopt;
		}
		pack.version = *version;
	}
	pack.authors = StringList(json, "authors");
	pack.icon = IconOf(json, root);
	pack.root = root;
	return pack;
}

} // namespace openblack::mods

namespace openblack::mods
{

// nlohmann's value() and get() throw on a wrong type ("restart_required": "yes"); a broken manifest must never stop
// openblack, so whatever is thrown becomes an error of that mod
ManifestResult ParseManifest(std::string_view text, const std::filesystem::path& root, std::string_view pack)
{
	try
	{
		return ParseManifestUnchecked(text, root, pack);
	}
	catch (const std::exception& error)
	{
		ManifestResult result;
		result.errors.push_back(fmt::format("mod.json: {}", error.what()));
		return result;
	}
}

std::optional<Modpack> ParseModpack(std::string_view text, const std::filesystem::path& root, std::vector<std::string>& errors)
{
	try
	{
		return ParseModpackUnchecked(text, root, errors);
	}
	catch (const std::exception& error)
	{
		errors.push_back(fmt::format("modpack.json: {}", error.what()));
		return std::nullopt;
	}
}

} // namespace openblack::mods
