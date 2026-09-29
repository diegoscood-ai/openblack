/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "Lexer.h"

namespace openblack::lhscriptx
{

/// A line the script reader cannot understand (syntax, unknown command, wrong arguments): the line is skipped
class ScriptError: public std::runtime_error
{
public:
	explicit ScriptError(const std::string& msg)
	    : std::runtime_error(msg)
	{
	}
};

class Script
{
public:
	Script();

	void Load(const std::string&);

private:
	[[nodiscard]] bool IsCommand(const std::string& identifier) const;
	void RunCommand(const std::string& identifier, const std::vector<Token>& args);

	const Token* PeekToken(Lexer&);
	const Token* AdvanceToken(Lexer&);

	// The current token.
	Token _token {Token::MakeInvalidToken()};
};

} // namespace openblack::lhscriptx
