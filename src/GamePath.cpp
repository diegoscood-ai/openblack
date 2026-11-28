/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePath.h"

std::optional<std::string> openblack::game_path::Choose(const std::optional<std::string>& commandLine,
                                                        const RegistryReader& registry)
{
	if (commandLine.has_value())
	{
		return commandLine;
	}
	return registry ? registry() : std::nullopt;
}
