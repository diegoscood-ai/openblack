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

#include <optional>
#include <string_view>

#include "ScriptControl.h"

// The help system's message sets (HELP_SYSTEM_MESSAGE_SET): the table of sets, the conditions of the turn and the
// advisors' banter.
namespace openblack::help
{
class HelpSystem;
} // namespace openblack::help

namespace openblack::help::message_sets
{

/// The sets of the table (Reset clears the sent turn and count of each)
constexpr uint32_t k_Sets = 0x98;
/// The banter sets are 1..25
constexpr uint32_t k_BanterSets = 25;
/// The conditions evaluated each turn
constexpr uint32_t k_Conditions = 18;
/// The idle time for condition 2, in seconds
constexpr float k_IdleSeconds = 120.0f;

/// One set of the table
struct Set
{
	uint32_t first;          ///< the first text (HELP_TEXT)
	uint32_t last;           ///< the last one
	uint32_t mode;           ///< 0 the texts, 1 one of them at random, 2 the thing's query texts, 3 no texts
	uint32_t category;       ///< HELP_SET_CATEGORY (TriggerCategory)
	std::string_view script; ///< the CHL help script RunMessage starts
};

/// A range of help texts, said from the first to the last
struct Texts
{
	uint32_t first;
	uint32_t last;
};

/// The table of sets
[[nodiscard]] const Set& Get(uint32_t set);
/// This turn's conditions
void ProcessConditions(HelpSystem& help);
/// A banter set not sent yet, on the local random stream (not the synced one)
uint32_t GetRandomBanterSet(HelpSystem& help);
/// Record a set as sent this turn
void MarkMessageSetSent(HelpSystem& help, uint32_t set, uint32_t turn);
/// Run a set, about a thing when `thing` has its help texts: false when the running help scripts refuse to stop. A set
/// of mode 2 says the thing's texts, 0 and 0 without a thing
bool RunMessageSet(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, uint32_t set,
                   const script_control::Vm& vm, uint32_t turn, std::optional<Texts> thing = std::nullopt);
/// The advisors' banter when the player has been idle
void ProcessBanter(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, const script_control::Vm& vm,
                   uint32_t turn);

} // namespace openblack::help::message_sets
