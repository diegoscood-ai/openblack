/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"
#include "LineReader.h"

namespace openblack::lhscriptx
{

class Script
{
public:
	Script();

	/// The reader's integer slots (one set for every script): reading a line writes slot i only for an 'N' argument
	/// (atol) or an int variable there; any other argument leaves the value of an earlier command. CREATE_VILLAGER "AAL"
	/// reads slot 2 as its age: what the last command with an 'N' third argument left
	[[nodiscard]] static int32_t IntSlot(size_t index);

	/// At the start of a map's features: the position offset none, the player and tribe overrides -1, the VERSION 0, the
	/// last created object none. Not per line. Also (inferred) the vagrants list (town_villagers::ClearVagrants)
	static void BeginMapFeatures();
	/// Added (as MapCoords) to every script position; none: 0. (pending) its writer is the vortex reader
	static void SetPositionOffset(std::optional<map_coords::MapCoords> offset);
	[[nodiscard]] static std::optional<map_coords::MapCoords> PositionOffset();
	/// What the last creation command made, written right after creating, null too; the vortex reader returns it
	[[nodiscard]] static entt::entity LastCreated();
	static void SetLastCreated(entt::entity entity);
	/// (-1 none): CREATE_VILLAGER / _POS take this tribe's villager of the same number when there is one. (pending) its
	/// writer is the vortex reader
	static void SetTribeOverride(std::optional<Tribe> tribe);
	[[nodiscard]] static std::optional<Tribe> TribeOverride();

	/// Carries out the commands of a land script as the original reads it: line by line, every command on a line in turn,
	/// calling `afterCommand`, when given, after each land command. A line with a missing argument (or another error of
	/// the reader) stops the script there (docs/wiki/scripts/map-script-reader.md#how-a-line-is-read). The script's
	/// variables are made again for each call
	void Load(const std::string& source, const std::function<void()>& afterCommand = {});

private:
	/// A variable a script made with SLONG (a whole number), FLOAT (a fraction) or CBYTE (a text, none until one is
	/// given)
	struct Variable
	{
		enum class Type
		{
			Whole,
			Fraction,
			Text,
		};
		std::string name;
		Type type {Type::Whole};
		int32_t whole {0};
		float fraction {0.0f};
		std::optional<std::string> text;
	};

	/// What a word of a line is to the reader
	enum class WordKind
	{
		None,      ///< a word it does not know, or a quoted text: nothing
		FrontEnd,  ///< one of the reader's own commands for the front end (OPEN_SCREEN, LOAD_BMP...): not ported
		Declare,   ///< SLONG, FLOAT or CBYTE: makes a variable
		Condition, ///< IF or ENDIF: not ported
		Remark,    ///< REM: the rest of the line is a remark
		Variable,  ///< a variable's name, or ***VAR
		Command,   ///< a land command
	};
	struct WordMeaning
	{
		WordKind kind {WordKind::None};
		/// The index among the reader's own words, or in the land's command table
		size_t index {0};
	};

	[[nodiscard]] WordMeaning Meaning(const line_reader::Word& word) const;
	/// The newest variable of the name; none for an empty name
	[[nodiscard]] std::optional<size_t> FindVariable(std::string_view name) const;
	/// Runs one line; false when the reader cannot go on, which stops the script
	bool RunLine(std::string_view line, const std::function<void()>& afterCommand);
	/// NAME = value, NAME already read; false on an error
	bool Assign(std::string_view name, line_reader::LineCursor& cursor);
	/// Reads the arguments of a command, already read, then carries it out; false when an argument is missing or a
	/// variable does not fit its argument
	bool RunCommand(const WordMeaning& command, line_reader::LineCursor& cursor, const std::function<void()>& afterCommand);

	/// The variables of this script, the newest last
	std::vector<Variable> _variables;
};

} // namespace openblack::lhscriptx
