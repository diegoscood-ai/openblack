/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Script.h"

#include <algorithm>
#include <array>
#include <ranges>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Town/TownVillagers.h"
#include "FeatureScriptCommands.h"
#include "LineReader.h"
#include "Locator.h"
#include "ScriptWriter.h"

using namespace openblack;
using namespace openblack::lhscriptx;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct ScriptRunState
{
	/// The 12 integer slots (reading a line stops at 12)
	std::array<int32_t, 12> intSlots {};
	/// The 12 fraction slots: slot i is written only for an 'F' argument or a fraction variable there
	std::array<float, 12> floatSlots {};
	/// The map features' position offset, last created object and tribe override (Script.h)
	std::optional<map_coords::MapCoords> positionOffset {};
	entt::entity lastCreated {entt::null};
	std::optional<Tribe> tribeOverride {};
};

ScriptRunState& RunState()
{
	return openblack::Locator::scriptState::value().Get<ScriptRunState>();
}
} // namespace

int32_t Script::IntSlot(size_t index)
{
	auto& state = RunState();
	return index < state.intSlots.size() ? state.intSlots.at(index) : 0;
}

void Script::BeginMapFeatures()
{
	auto& state = RunState();
	// (not ported) the player override: no command reads it yet
	state.positionOffset.reset();
	state.tribeOverride.reset();
	// The VERSION float (CREATE_FLOCK tests it against 2.1) back to 0, so a land without a VERSION line does not keep
	// the last land's
	if (Locator::mapScriptSystem::has_value())
	{
		Locator::mapScriptSystem::value().Globals().version = 0.0f;
	}
	state.lastCreated = entt::null;
	// (inferred) the vagrants start empty on each map: the original clears them when the game is made and each deleted
	// villager leaves them; openblack's Registry::Reset deletes no one one by one, so the list would keep dead ids
	// (CREATE_VILLAGER / _POS write it)
	ecs::town_villagers::ClearVagrants();
}

void Script::SetPositionOffset(std::optional<map_coords::MapCoords> offset)
{
	RunState().positionOffset = offset;
}

std::optional<map_coords::MapCoords> Script::PositionOffset()
{
	return RunState().positionOffset;
}

entt::entity Script::LastCreated()
{
	return RunState().lastCreated;
}

void Script::SetLastCreated(entt::entity entity)
{
	RunState().lastCreated = entity;
}

void Script::SetTribeOverride(std::optional<Tribe> tribe)
{
	RunState().tribeOverride = tribe;
}

std::optional<Tribe> Script::TribeOverride()
{
	return RunState().tribeOverride;
}

namespace
{
/// One of the reader's own words, which it looks up before the land's commands, with the letters of its arguments
struct ReaderWord
{
	std::string_view name;
	std::string_view types;
};

/// The reader's own words in the original's order: 23 for the front end, SLONG, FLOAT and CBYTE, IF, ENDIF, REM, and
/// ***VAR, which is read as a variable's name
constexpr std::array<ReaderWord, 30> k_ReaderWords = {{
    {.name = "OPEN_SCREEN", .types = "NNNN"},
    {.name = "BUILD_ALL_MNT_TEXTURE", .types = "N"},
    {.name = "D3D_SETUP", .types = "N"},
    {.name = "SWAP_TEXTURE_AGP_VS_LOCAL", .types = "N"},
    {.name = "MNT_NAME", .types = "A"},
    {.name = "USE_LOW_TEXTURE", .types = "N"},
    {.name = "SHOW_DEBUG_INFO", .types = "N"},
    {.name = "SET_AGP_MEM", .types = "N"},
    {.name = "SET_VRAM_MEM", .types = "N"},
    {.name = "USE_SMALL_BUMP", .types = "N"},
    {.name = "USE_WIREFRAME", .types = "N"},
    {.name = "SET_VANISH_OBJECT_DIST", .types = "N"},
    {.name = "ALLOC_MOUSE_SURFACE", .types = "N"},
    {.name = "OPEN_KEYBOARD", .types = "A"},
    {.name = "OPEN_MOUSE", .types = "A"},
    {.name = "OPEN_SOUND", .types = "A"},
    {.name = "LOAD_BMP", .types = "AN"},
    {.name = "LOAD_SPRITES_DEPTH", .types = "ANN"},
    {.name = "LOAD_SPRITES", .types = "AN"},
    {.name = "LOAD_DATA", .types = "AN"},
    {.name = "LOAD_FONT_DEPTH", .types = "AN"},
    {.name = "LOAD_FONT", .types = "AN"},
    {.name = "SET_TRANSPARENCY", .types = "NNN"},
    {.name = "SLONG", .types = "A"},
    {.name = "FLOAT", .types = "A"},
    {.name = "CBYTE", .types = "A"},
    {.name = "IF", .types = "AAA"},
    {.name = "ENDIF", .types = ""},
    {.name = "REM", .types = ""},
    {.name = "***VAR", .types = ""},
}};
constexpr size_t k_FirstDeclareWord = 23; // SLONG, then FLOAT and CBYTE
constexpr size_t k_IfWord = 26;
constexpr size_t k_EndIfWord = 27;
constexpr size_t k_RemWord = 28;
constexpr size_t k_VarWord = 29;

/// A position argument's text: its map units (line_reader::PositionUnits), plus the position offset when it is set, back
/// in metres, on the land there, with the map units themselves for the commands that keep them. (pending) the altitude
/// after a second comma, the original's height above the land, is only in those map units
/// (docs/wiki/scripts/map-script-reader.md#pending)
ScriptCommandParameter PositionParameter(std::string_view text)
{
	auto at = line_reader::PositionUnits(text);
	if (const auto offset = Script::PositionOffset(); offset.has_value())
	{
		at += *offset;
	}
	const auto x = map_coords::ToMetres(at.x);
	const auto z = map_coords::ToMetres(at.z);
	const auto& island = Locator::terrainSystem::value();
	ScriptCommandParameter parameter {x, island.GetHeightAt(glm::vec2(x, z)), z};
	parameter.SetUnits(at);
	return parameter;
}
} // namespace

Script::Script() = default;

void Script::Load(const std::string& source, const std::function<void()>& afterCommand)
{
	_variables.clear();
	line_reader::ForEachLine(source, [this, &afterCommand](std::string_view line, int number) {
		if (RunLine(line, afterCommand))
		{
			return true;
		}
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"),
		                   "LHScriptX: line {}: an argument is missing or a variable does not fit; the rest of the script "
		                   "is not read",
		                   number);
		return false;
	});
}

std::optional<size_t> Script::FindVariable(std::string_view name) const
{
	if (name.empty())
	{
		return std::nullopt;
	}
	for (size_t i = _variables.size(); i > 0; --i)
	{
		if (_variables.at(i - 1).name == name)
		{
			return i - 1;
		}
	}
	return std::nullopt;
}

Script::WordMeaning Script::Meaning(const line_reader::Word& word) const
{
	if (word.quoted)
	{
		return {};
	}
	// the reader's own words first, then the land's commands, then the variables
	for (size_t i = 0; i < k_ReaderWords.size(); ++i)
	{
		if (k_ReaderWords.at(i).name != word.text)
		{
			continue;
		}
		if (i < k_FirstDeclareWord)
		{
			return {.kind = WordKind::FrontEnd, .index = i};
		}
		if (i < k_IfWord)
		{
			return {.kind = WordKind::Declare, .index = i};
		}
		if (i == k_IfWord || i == k_EndIfWord)
		{
			return {.kind = WordKind::Condition, .index = i};
		}
		if (i == k_RemWord)
		{
			return {.kind = WordKind::Remark, .index = i};
		}
		return {.kind = WordKind::Variable, .index = k_VarWord};
	}
	if (const auto* format = FindCommandFormat(word.text); format != nullptr)
	{
		return {.kind = WordKind::Command, .index = static_cast<size_t>(format - k_CommandFormats.data())};
	}
	if (FindVariable(word.text).has_value())
	{
		return {.kind = WordKind::Variable, .index = k_VarWord};
	}
	return {};
}

bool Script::RunLine(std::string_view line, const std::function<void()>& afterCommand)
{
	line_reader::LineCursor cursor(line);
	while (true)
	{
		const auto word = cursor.Next();
		const auto meaning = Meaning(word);
		switch (meaning.kind)
		{
		case WordKind::None:
			// a word the reader does not know does nothing: the next word is read
			break;
		case WordKind::Remark:
			return true;
		case WordKind::Condition:
			// (not ported) IF runs the command after it and keeps its answer for the lines up to ENDIF
			SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "LHScriptX: {} is not ported; the rest of its line is not read",
			                   word.text);
			return true;
		case WordKind::Variable:
			if (!Assign(word.text, cursor))
			{
				return false;
			}
			break;
		case WordKind::FrontEnd:
		case WordKind::Declare:
		case WordKind::Command:
			if (!RunCommand(meaning, cursor, afterCommand))
			{
				return false;
			}
			break;
		}
		if (cursor.EndOfLine())
		{
			return true;
		}
	}
}

bool Script::Assign(std::string_view name, line_reader::LineCursor& cursor)
{
	// ***VAR is read as a variable's name, so it stops the script unless a variable has that name
	const auto found = FindVariable(name);
	if (!found.has_value())
	{
		return false;
	}
	// the "=" must not end the line
	const auto equals = cursor.Next();
	if (cursor.EndOfLine() || !equals.text.starts_with('='))
	{
		return false;
	}
	const auto value = cursor.Next();
	auto& variable = _variables.at(*found);
	switch (variable.type)
	{
	case Variable::Type::Whole:
		variable.whole = line_reader::AtoL(value.text);
		break;
	case Variable::Type::Fraction:
		variable.fraction = static_cast<float>(line_reader::AtoF(value.text));
		break;
	case Variable::Type::Text:
		variable.text = value.text;
		break;
	}
	return true;
}

bool Script::RunCommand(const WordMeaning& command, line_reader::LineCursor& cursor, const std::function<void()>& afterCommand)
{
	auto& state = RunState();
	const auto types =
	    command.kind == WordKind::Command ? k_CommandFormats.at(command.index).types : k_ReaderWords.at(command.index).types;

	// Each argument's word is read whatever it is: an 'N' goes to its integer slot (atol), an 'F' to its fraction slot
	// (atof), and every argument keeps its text. A variable's name gives its value instead: a whole number only for an
	// 'N', a fraction only for an 'F'; a text variable gives its text and leaves the slots as they were
	std::array<std::string, 12> texts;
	size_t i = 0;
	for (; i < types.size() && !cursor.EndOfLine(); ++i)
	{
		const auto word = cursor.Next();
		const char letter = types.at(i);
		texts.at(i) = word.text;
		if (Meaning(word).kind == WordKind::Variable)
		{
			const auto found = FindVariable(word.text);
			if (!found.has_value())
			{
				return false;
			}
			const auto& variable = _variables.at(*found);
			switch (variable.type)
			{
			case Variable::Type::Whole:
				if (letter != 'N')
				{
					return false;
				}
				state.intSlots.at(i) = variable.whole;
				break;
			case Variable::Type::Fraction:
				if (letter != 'F')
				{
					return false;
				}
				state.floatSlots.at(i) = variable.fraction;
				break;
			case Variable::Type::Text:
				if (!variable.text.has_value())
				{
					return false;
				}
				texts.at(i) = *variable.text;
				break;
			}
		}
		else if (letter == 'N')
		{
			state.intSlots.at(i) = line_reader::AtoL(word.text);
		}
		else if (letter == 'F')
		{
			state.floatSlots.at(i) = static_cast<float>(line_reader::AtoF(word.text));
		}
	}
	// The end of the line came before the last argument. In a file whose lines end in CRLF the carriage return is one
	// more word, so a missing last argument is read as an empty one
	if (i < types.size())
	{
		return false;
	}

	if (command.kind == WordKind::Declare)
	{
		const auto type = command.index == k_FirstDeclareWord       ? Variable::Type::Whole
		                  : command.index == k_FirstDeclareWord + 1 ? Variable::Type::Fraction
		                                                            : Variable::Type::Text;
		_variables.push_back({.name = texts.at(0), .type = type});
		return true;
	}
	if (command.kind == WordKind::FrontEnd)
	{
		// (not ported) the front end's own setup commands: their arguments are read, and nothing is done
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "LHScriptX: {} is not ported", k_ReaderWords.at(command.index).name);
		return true;
	}

	const auto& name = k_CommandFormats.at(command.index).name;
	const auto signature = std::ranges::find_if(FeatureScriptCommands::k_Signatures,
	                                            [&name](const auto& s) { return std::string_view(s.name.data()) == name; });
	if (signature == FeatureScriptCommands::k_Signatures.end())
	{
		return true;
	}
	ScriptCommandParameters parameters;
	for (size_t p = 0; p < signature->parameters.size() && signature->parameters.at(p) != ParameterType::None; ++p)
	{
		switch (signature->parameters.at(p))
		{
		case ParameterType::Number:
			parameters.emplace_back(state.intSlots.at(p));
			break;
		case ParameterType::Float:
			parameters.emplace_back(state.floatSlots.at(p));
			break;
		case ParameterType::String:
			parameters.emplace_back(texts.at(p));
			break;
		case ParameterType::Vector:
			parameters.push_back(PositionParameter(texts.at(p)));
			break;
		case ParameterType::None:
			break;
		}
	}
	signature->command(parameters);
	if (afterCommand)
	{
		afterCommand();
	}
	return true;
}
