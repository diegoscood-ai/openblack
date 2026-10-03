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
#include <functional>
#include <optional>
#include <string>

// The help system's tooltips: the one tooltip shown next to the hand (SubmitToolTips 0x5C9A70, ForceToolTips 0x5C9C60,
// its lifetime per game turn fn_005C9D00 / fn_005C9C80 inside HelpSystem::Process 0x5C8FE0, the text builder 0x5C9FC0)
// and its KMIcon (fn_004489D0: the fade in real time). What to submit each turn comes from the hand's state (the hand's
// tooltip functions, table 0xBF1C10), registered with SetStateSubmitter. Research: dev\documentacion\hand\tooltips\
// README.md and README_part2.md. Wiki: docs/bw1-notes/hand-and-interface.md, "Tooltips".
namespace openblack::help::tooltips
{

/// The texts this module accepts: 0xE73 (HELP_TEXT_TOOLTIP_01) .. 0xE73 + 0xA9
constexpr uint32_t k_First = 0xE73;
constexpr uint32_t k_Count = 0xAA;

/// SubmitToolTips 0x5C9A70(text, action, align, force): `action` is the BINDABLE_ACTION whose mouse button or key the icon
/// shows (-1: none); `value` fills the text's number when it has one (the builder 0x5C9FC0 reads it from the object; here
/// the caller passes it). Submitting the current text keeps it alive this turn.
void Submit(uint32_t text, int32_t action, uint32_t align, bool force, std::optional<float> value = std::nullopt);
/// ForceToolTips 0x5C9C60(text, value): Submit(text, -1, 0, 1), and the value [0xD17BBC] for the text's number
void Force(uint32_t text, float value);

/// The hand's part of the builder 0x5C9FC0 (fn_005D78D0 with the interface's hand state): called once per turn before
/// the lifetime, it submits what the hand shows
void SetStateSubmitter(std::function<void()> submitter);

/// TOOLTIP_LEVEL (HelpSystem +0x45FC, profile fn_005C6CF0, default 2): 0 none, 1 only priorities >= 0.9, 2 all with the
/// fade-out of the low ones, 3 all and no fade-in
void SetLevel(int32_t level);
[[nodiscard]] int32_t Level();

/// Once per game turn (HelpSystem::Process 0x5C8FE0 -> fn_005C9D00): the builder, then the lifetime and the icon
void ProcessTurn();
/// Every drawn frame with the real seconds (fn_00447850 -> fn_00448AC0(g_delta_time * 0.001)): the icon's fade
void Frame(float realSeconds);

/// The KMIcon on screen (fn_004489D0's fields), for CameraHelp::DrawKeyOrMouse 0x447EA0
struct Icon
{
	std::u16string text;  ///< +0x20, already formatted
	int32_t action {-1};  ///< the BINDABLE_ACTION (for the mouse cell; -1 = row 3, no icon)
	uint32_t align {0};   ///< +0x12C: the submitted align | 0x16
	float alpha {0.0f};   ///< +0xC, 0..1
};
[[nodiscard]] std::optional<Icon> Current();

/// GInterface::SetToZero / a new land (HelpSystem reset): no tooltip, timers and show counts kept
void Reset();

} // namespace openblack::help::tooltips
