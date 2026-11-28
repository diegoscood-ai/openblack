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

#include "KeyNamesInterface.h"
#include "SystemKeyName.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::input::system_key_name
{
/// The names the game shows, installed as it starts (Locator::keyNames): while the run reads the real input, the
/// system's own (on systems that have them, Windows; elsewhere none); in a run that reads none (a test run: the real
/// input ignored, a fixed or pinned mouse, a recording playing), the English (United States) layout's, so that a test
/// run names the keys the same on every machine, as its mouse is the same (MouseOfRun). Asked at each name
class SystemKeyNames final: public KeyNamesInterface
{
public:
	/// The names as the run's input decides, asked at each name
	explicit SystemKeyNames(RealInputQuery realInput) noexcept;
	/// Tests: the names come from here, asked as the game asks the system
	explicit SystemKeyNames(Source source) noexcept;

	[[nodiscard]] std::optional<std::u16string> Name(uint8_t code) override;

private:
	Source _source {nullptr};
	RealInputQuery _realInput {nullptr};
};
} // namespace openblack::input::system_key_name
