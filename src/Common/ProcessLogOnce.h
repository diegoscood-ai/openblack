/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

/// The program's own warnings that are logged once per process (Locator::logOnce), by a fixed key
namespace openblack::log_once
{
/// True the first time `key` is given in the process. Without the service (a unit test that made none) always true:
/// every warning is logged, nothing is dereferenced
[[nodiscard]] bool First(std::string_view key);
} // namespace openblack::log_once
