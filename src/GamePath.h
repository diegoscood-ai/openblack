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
#include <optional>
#include <string>

namespace openblack::game_path
{
/// The install folder the registry gives (Black & White's GameDir), none where it gives none
using RegistryReader = std::function<std::optional<std::string>()>;

/// The game's folder: the command line's, which wins; else the registry's, which is only asked for without one; else
/// none, and the game cannot start
[[nodiscard]] std::optional<std::string> Choose(const std::optional<std::string>& commandLine, const RegistryReader& registry);
} // namespace openblack::game_path
