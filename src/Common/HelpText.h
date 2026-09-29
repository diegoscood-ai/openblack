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
#include <string>

namespace openblack::helptext
{

/// The help text database (GSetup::LoadTextScripts 0x719280): Scripts\InfoScript2.txt, UTF-16 with a BOM, lines
/// ADD_TEXT(n, NARRATOR, "NAME", "text"); a text's id is its 0-based order. Loaded on first use.
/// GetHelpText: entry 0 for an id out of range.
[[nodiscard]] const std::u16string& Get(uint32_t id);

/// UNICODE_sprintf of the text with one number (e.g. 0xEEA "Cantidad: %3.0f"): the first printf conversion is
/// replaced with the value formatted by it
[[nodiscard]] std::u16string Format(uint32_t id, double value);

/// Tooltip ids (HELP_TEXT_TOOLTIP_*, the tooltip table starts at 0xE73)
constexpr uint32_t k_ToolTipPickUp = 0xE73;       ///< "Recoger" / "Pick up"
constexpr uint32_t k_ToolTipAmountInHand = 0xEEA; ///< "Cantidad: %3.0f"

} // namespace openblack::helptext
