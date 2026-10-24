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

namespace openblack::ecs::systems
{
/// The warnings the program logs only once, by a fixed key such as "graphics/missing-texture/0x1001"
/// (Locator::logOnce). It lives as long as the process, as the file system does: a new game in the same process
/// logs none of them again. Any thread may ask
class LogOnceInterface
{
public:
	virtual ~LogOnceInterface() = default;

	/// True the first time `key` is given in the process, false after that
	[[nodiscard]] virtual bool First(std::string_view key) = 0;
};
} // namespace openblack::ecs::systems
