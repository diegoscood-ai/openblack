/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedQuestScript.h"

#include <charconv>
#include <cmath>

#include <algorithm>

using namespace openblack;
using namespace openblack::testbed_land_timeline;

namespace
{
/// The whole text as a float, none for anything else
std::optional<float> ParseFloat(std::string_view text)
{
	float value {};
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (text.empty() || error != std::errc {} || end != text.data() + text.size())
	{
		return std::nullopt;
	}
	return value;
}

/// Floats separated by semicolons, none when one is not
std::optional<std::vector<float>> ParseFloats(std::string_view text)
{
	std::vector<float> values;
	for (auto rest = text;;)
	{
		const auto semicolon = rest.find(';');
		const auto value = ParseFloat(rest.substr(0, semicolon));
		if (!value.has_value())
		{
			return std::nullopt;
		}
		values.push_back(*value);
		if (semicolon == std::string_view::npos)
		{
			return values;
		}
		rest = rest.substr(semicolon + 1);
	}
}
} // namespace

std::optional<ScriptArgument> testbed_land_timeline::ParseScriptArgument(std::string_view text)
{
	constexpr std::string_view k_Marker = "marker@";
	constexpr std::string_view k_Thing = "thing@";
	if (text.starts_with(k_Marker))
	{
		const auto values = ParseFloats(text.substr(k_Marker.size()));
		if (!values.has_value() || values->size() != 3)
		{
			return std::nullopt;
		}
		return ScriptArgument {.kind = ScriptArgument::Kind::Marker, .point = {(*values)[0], (*values)[1], (*values)[2]}};
	}
	if (text.starts_with(k_Thing))
	{
		const auto values = ParseFloats(text.substr(k_Thing.size()));
		if (!values.has_value() || (values->size() != 3 && values->size() != 4) || (*values)[0] < 0.0f ||
		    (*values)[0] > 255.0f || (*values)[0] != std::floor((*values)[0]))
		{
			return std::nullopt;
		}
		ScriptArgument thing {.kind = ScriptArgument::Kind::Thing,
		                      .thingType = static_cast<uint8_t>((*values)[0]),
		                      .thingPoint = {(*values)[1], (*values)[2]}};
		if (values->size() == 4)
		{
			thing.thingRadius = (*values)[3];
		}
		return thing;
	}
	const auto number = ParseFloat(text);
	if (!number.has_value())
	{
		return std::nullopt;
	}
	return ScriptArgument {.kind = ScriptArgument::Kind::Number, .number = *number};
}

std::optional<QuestScript> testbed_land_timeline::ParseQuestScript(std::string_view text)
{
	if (text.ends_with('&'))
	{
		auto script = ParseQuestScript(text.substr(0, text.size() - 1));
		if (!script.has_value() || script->globalValue.has_value() || script->background)
		{
			return std::nullopt;
		}
		script->background = true;
		return script;
	}
	if (const auto equals = text.find('='); equals != std::string_view::npos)
	{
		auto global = ParseQuestScript(text.substr(0, equals));
		const auto value = ParseScriptArgument(text.substr(equals + 1));
		if (!global.has_value() || !global->arguments.empty() || !value.has_value() ||
		    value->kind != ScriptArgument::Kind::Number)
		{
			return std::nullopt;
		}
		global->globalValue = value->number;
		return global;
	}
	const auto open = text.find('(');
	QuestScript script {.name = std::string(text.substr(0, open))};
	const bool plain = !script.name.empty() && std::ranges::all_of(script.name, [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
	});
	if (!plain)
	{
		return std::nullopt;
	}
	if (open == std::string_view::npos)
	{
		return script;
	}
	if (!text.ends_with(')'))
	{
		return std::nullopt;
	}
	for (auto rest = text.substr(open + 1, text.size() - open - 2);;)
	{
		const auto comma = rest.find(',');
		const auto argument = ParseScriptArgument(rest.substr(0, comma));
		if (!argument.has_value())
		{
			return std::nullopt;
		}
		script.arguments.push_back(*argument);
		if (comma == std::string_view::npos)
		{
			return script;
		}
		rest = rest.substr(comma + 1);
	}
}
