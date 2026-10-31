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

#include <array>
#include <functional>
#include <optional>

// The help system's tooltips: the one tooltip shown next to the hand (submitted, forced, its lifetime per game turn
// inside help::Process, the text builder) and its InputPromptIcon (help::input_prompt). What to submit each turn comes from the
// hand's state (the hand's tooltip functions), registered with SetStateSubmitter. Wiki:
// docs/wiki/interface/tooltips.md#tooltips.
namespace openblack::help::tooltips
{

/// The texts this module accepts: 0xE73 (HELP_TEXT_TOOLTIP_01) .. 0xE73 + 0xA9
constexpr uint32_t k_First = 0xE73;
constexpr uint32_t k_Count = 0xAA;

/// What the builder reads from the object for a text (here the caller passes it): up to two numbers in the text's
/// order (0xEF9: the food, then the wood), `outOf` appended as "%s/%d" (0xEFC: the places), `percentAfter` written
/// after the text as "%s %3.0f%%" (a spell icon's charge), and `text`, the text the builder writes instead (0xECC ->
/// 0xEE0 when the believers are negative)
struct Numbers
{
	std::array<double, 2> values {};
	size_t count {0};
	std::optional<int32_t> outOf;
	std::optional<double> percentAfter;
	std::optional<uint32_t> text;
};

/// `action` is the BINDABLE_ACTION whose mouse button or key the icon shows (-1: none); `value` fills the text's
/// number when it has one (the original's builder reads it from the object; here the caller passes it). Submitting
/// the current text keeps it alive this turn.
void Submit(uint32_t text, int32_t action, uint32_t align, bool force, std::optional<float> value = std::nullopt);
/// The same with the builder's numbers (Numbers)
void Submit(uint32_t text, int32_t action, uint32_t align, bool force, const Numbers& numbers);
/// Submit(text, -1, 0, 1), and the value for the text's number
void Force(uint32_t text, float value);

/// The hand's part of the builder (from the interface's hand state): called once per turn before the lifetime, it
/// submits what the hand shows
void SetStateSubmitter(std::function<void()> submitter);

/// The texts whose numbers the builder reads again every turn from the object under the hand that turn, whoever
/// submitted them: the town's believers 0xECC, its population 0xEFC and a storage pit's stores 0xEF9
[[nodiscard]] constexpr bool ReadsObjectNumbers(uint32_t text)
{
	return text == 0xECC || text == 0xEFC || text == 0xEF9;
}
/// The texts the builder shows at once, as if forced, as it writes them: the temple's health 0xEE8, the altar's prayer
/// energy 0xEE9 and the need signs' 0xECE / 0xEFF always, and a desire flag's need 0xE88 and a workshop's production
/// 0xEF3 when it writes their number
[[nodiscard]] constexpr bool BuilderShowsAtOnce(uint32_t text, bool withNumber)
{
	return text == 0xEE8 || text == 0xEE9 || text == 0xECE || text == 0xEFF || ((text == 0xE88 || text == 0xEF3) && withNumber);
}
/// What the builder reads for those texts this turn: the numbers of the object under the hand, or none when there is
/// no such object (or it is not one the text is about), when the text shows as it is written
void SetObjectNumbers(std::function<std::optional<Numbers>(uint32_t text)> reader);

/// The size S of the tooltip's icon: a 25th of the screen's height, in whole pixels
[[nodiscard]] constexpr int32_t IconSize(int32_t screenHeight)
{
	return screenHeight / 25;
}

/// TOOLTIP_LEVEL (from the profile, default 2): 0 none, 1 only priorities >= 0.9, 2 all with the fade-out of the low
/// ones, 3 all and no fade-in
void SetLevel(int32_t level);
[[nodiscard]] int32_t Level();

/// Once per game turn, and per temple turn while the game is paused inside the citadel (from help::Process): the
/// builder, then the lifetime and the InputPromptIcon (created, kept or deleted; input_prompt::Frame fades it and
/// Renderer::DrawKeyOrMouse draws it). Under a script's widescreen, or paused outside the citadel, the icon is
/// deleted instead
void ProcessTurn();

/// A new land (the map's clear): no tooltip and every field at its start value; the show counts and the icon are kept
void Reset();
/// The middle step of HelpSystem::ResetIcons: only the tooltip's InputPromptIcon is deleted
void ResetIcon();
/// The text shown now (k_First and up), none when no tooltip is shown or before the game's script state exists. For the
/// debug readouts
[[nodiscard]] std::optional<uint32_t> ShownText();
/// Whether the text shown was forced (it shows at once and nothing not forced takes over). For the debug readouts
[[nodiscard]] bool IsShownForced();
/// Whether the tooltip's InputPromptIcon exists (it may outlive its text until the next turn). For the debug readouts
[[nodiscard]] bool HasIcon();

} // namespace openblack::help::tooltips
