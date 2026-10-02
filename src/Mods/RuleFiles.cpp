/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RuleFiles.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace openblack::mods::rule_files
{
namespace
{
using Json = nlohmann::ordered_json; // the order of the rules matters (sections are read in order)

std::optional<std::string> ReadText(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return std::nullopt;
	}
	std::ostringstream text;
	text << file.rdbuf();
	return text.str();
}

/// A value as the .cfg wrote it: a list joined with ", ", true / false as on / off, numbers as JSON prints them
std::string CfgValue(const Json& value)
{
	if (value.is_string())
	{
		return value.get<std::string>();
	}
	if (value.is_boolean())
	{
		return value.get<bool>() ? "on" : "off";
	}
	if (value.is_array())
	{
		std::string text;
		for (const auto& item : value)
		{
			text += (text.empty() ? "" : ", ") + CfgValue(item);
		}
		return text;
	}
	return value.dump();
}
} // namespace

std::optional<std::string> JsonToCfg(std::string_view text, std::string& error)
{
	const auto json = Json::parse(text, nullptr, false, true); // no exceptions, // comments allowed
	if (json.is_discarded() || !json.is_object())
	{
		error = "not a valid JSON object";
		return std::nullopt;
	}
	std::string cfg;
	if (const auto rules = json.find("rules"); rules != json.end() && rules->is_array())
	{
		for (const auto& rule : *rules)
		{
			if (!rule.is_object() || !rule.contains("section") || !rule["section"].is_string())
			{
				error = "every rule needs a \"section\"";
				return std::nullopt;
			}
			cfg += "[" + rule["section"].get<std::string>() + "]\n";
			for (const auto& [key, value] : rule.items())
			{
				if (key != "section")
				{
					cfg += key + " = " + CfgValue(value) + "\n";
				}
			}
			cfg += "\n";
		}
		return cfg;
	}
	if (const auto textures = json.find("textures"); textures != json.end() && textures->is_object())
	{
		for (const auto& [key, value] : textures->items())
		{
			cfg += key + " = " + CfgValue(value) + "\n";
		}
		return cfg;
	}
	error = "neither \"rules\" nor \"textures\"";
	return std::nullopt;
}

std::optional<std::string> Read(const std::filesystem::path& folder, std::string_view stem, std::filesystem::path& used,
                                std::string& error)
{
	const auto jsonPath = folder / (std::string(stem) + ".json");
	if (const auto json = ReadText(jsonPath))
	{
		used = jsonPath;
		return JsonToCfg(*json, error);
	}
	const auto cfgPath = folder / (std::string(stem) + ".cfg");
	used = cfgPath;
	return ReadText(cfgPath);
}

} // namespace openblack::mods::rule_files
