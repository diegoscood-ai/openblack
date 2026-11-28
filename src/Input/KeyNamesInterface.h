/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <string>

namespace openblack::input
{
/// The names the game shows for its keys (Locator::keyNames), installed as the game starts
/// (system_key_name::SystemKeyNames); a test installs its own first
class KeyNamesInterface
{
public:
	virtual ~KeyNamesInterface() = default;

	/// The name shown for a DirectInput key code, or none when there is no name to give
	[[nodiscard]] virtual std::optional<std::u16string> Name(uint8_t code) = 0;
};
} // namespace openblack::input
