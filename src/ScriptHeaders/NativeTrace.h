/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <span>
#include <string>
#include <string_view>

#include <LHVMTypes.h>

namespace openblack::script
{

/// The native trace's text: one line per native the scripts call, with the values it took from the stack, the values
/// it gave back, the game turn and the script of the task that called it, so that a run's log can be read against what
/// a quest does (tools/checks/story/quest_check.py)

/// Stack values as the trace writes them, comma separated: an int as its number, a float as its shortest exact text,
/// an object as obj:<id>, a boolean as true or false, any other type as <type name>:<raw bits>
[[nodiscard]] std::string FormatNativeValues(std::span<const lhvm::VMValue> values, std::span<const lhvm::DataType> types);

/// A native call's line: "native <NAME>(<arguments>) -> <results>, turn <turn>, task <script>"
[[nodiscard]] std::string FormatNativeCall(std::string_view name, std::string_view arguments, std::string_view results,
                                           uint32_t turn, std::string_view task);

} // namespace openblack::script
