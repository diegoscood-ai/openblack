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
#include <string_view>
#include <vector>

namespace openblack::helptext
{

/// HELP_TEXT_LAST of W120 (Scripts\InfoScript2.txt): the size of the voice table 0x915D40 and the bound GScript::RunText
/// 0x6F7D60 (0x6F7DC5) and fn_005C5F90 (0x5C5FA0) check text ids against (cmp 0x1B3E)
constexpr uint32_t k_TextCount = 0x1B3E;

/// HELP_TEXT_NARRATOR_* as Scripts\InfoScript2.txt W120 declares them ("NAME = value" lines); fn_005C5F90 compares the
/// narrator with these two (0x5C602A, 0x5C6061)
constexpr int32_t k_NarratorGoodSpirit = 2;
constexpr int32_t k_NarratorEvilSpirit = 3;
/// A narrator name ADD_TEXT uses without declaring it (GUIDE and MONK in W120). (inferred) The value the original's
/// script reader (LHScriptX::Load 0x7E7960, not read) gives such a name is unknown; it is neither 2 nor 3 in any case
/// that matters to fn_005C5F90 or IsTextRead (the voices of those texts are in villagers.sad).
constexpr int32_t k_NarratorUndeclared = -1;

/// One ADD_TEXT(arg0, NARRATOR, "NAME", "text") of Scripts\InfoScript2.txt. The callback 0x7192E0 hands it to
/// fn_005CCF80 -> HelpTextData fn_005CAD00, which keeps {+0 narrator, +4 arg0, +8 copy of the text} (12 bytes).
struct Entry
{
	/// HelpTextData +4: the 1st argument (0 or 1; meaning not read)
	int32_t arg0 {0};
	/// HelpTextData +0: the 2nd argument, a HELP_TEXT_NARRATOR_* value
	int32_t narrator {0};
	/// The 3rd argument, "HELP_TEXT_...". The original does not keep it (0x5CAD00 stores only the narrator, arg0 and the
	/// text); openblack needs it to rebuild the voice table 0x915D40 from the wave names (audio::VoiceTable).
	std::string name;
	/// HelpTextData +8: the 4th argument after the reader's conversion fn_007191F0
	std::u16string text;
};

/// fn_007191F0, applied by the ADD_TEXT callback 0x7192E0 to the name and the text: " ~", "~ " and "~" become 0xF8FE,
/// the two characters "\n" become a line feed, the rest is copied
[[nodiscard]] std::u16string ConvertScriptText(std::u16string_view text);

/// Every ADD_TEXT of an InfoScript2.txt (UTF-16 little endian, with or without the BOM) in file order: a text's id is its
/// 0-based position. The narrator names are resolved with the file's own "NAME = value" lines.
[[nodiscard]] std::vector<Entry> Parse(const std::vector<uint8_t>& utf16);

/// The help text database (GSetup::LoadTextScripts 0x719280): Scripts\InfoScript2.txt, loaded on first use.
/// GetHelpText: entry 0 for an id out of range (0 < id < count, as 0x5C5FAD..0x5C5FCA).
[[nodiscard]] const std::u16string& Get(uint32_t id);
/// The whole entry of a text, with the same rule for ids out of range (an empty entry when nothing was loaded)
[[nodiscard]] const Entry& GetEntry(uint32_t id);
/// Number of texts loaded (HelpTextDatabase +4, [0xD17CAC])
[[nodiscard]] size_t Count();

/// UNICODE_sprintf of the text with one number (e.g. 0xEEA "Cantidad: %3.0f"): the first printf conversion is
/// replaced with the value formatted by it
[[nodiscard]] std::u16string Format(uint32_t id, double value);

/// Tooltip ids (HELP_TEXT_TOOLTIP_*, the tooltip table starts at 0xE73)
constexpr uint32_t k_ToolTipPickUp = 0xE73;       ///< "Recoger" / "Pick up"
constexpr uint32_t k_ToolTipAmountInHand = 0xEEA; ///< "Cantidad: %3.0f"
constexpr uint32_t k_ToolTipContinue = 0xE79;     ///< HELP_TEXT_TOOLTIP_07 "Continuar", the click cue (Draw3D 0x5C59D0)

} // namespace openblack::helptext
