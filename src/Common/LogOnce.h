/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <set>
#include <string>
#include <string_view>

namespace openblack
{
/// Which keys (such as a script opcode's name) have already logged a message, so that only the first of each is
/// worth an error and the repeats a debug line
class LogOnce
{
public:
	/// True the first time `key` is given, false after that
	[[nodiscard]] bool First(std::string_view key)
	{
		if (_logged.contains(key))
		{
			return false;
		}
		_logged.emplace(key);
		return true;
	}

private:
	std::set<std::string, std::less<>> _logged;
};
} // namespace openblack
