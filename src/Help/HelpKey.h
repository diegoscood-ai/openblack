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
#include <string>

#include <entt/entity/entity.hpp>

#include "HelpQuery.h"
#include "ScriptControl.h"

namespace openblack::ecs::systems
{
class DialogueControlSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::help
{
class HelpSystem;
} // namespace openblack::help

/// The Help key: the advisors answer about what is in the hand, else what the hand is over, else the ground under it
namespace openblack::help::help_key
{

/// Says an answer: a message set (about the thing whose texts are given, for a set that says them), or texts through
/// their script. False when the help scripts running would not stop
bool Say(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, const help_query::Answer& answer,
         std::optional<help_query::Texts> thing, const script_control::Vm& vm, uint32_t turn);

/// The thing the Help key asks about: the first object in the hand, else the interface's thing under the hand (the
/// one its tooltips and actions take), else none (the ground)
[[nodiscard]] constexpr std::optional<entt::entity> ThingToAsk(std::optional<entt::entity> held,
                                                               std::optional<entt::entity> underHand) noexcept
{
	return held.has_value() ? held : underHand;
}

/// The Help key pressed: the answer about the hand's thing or the ground, said. False when nothing could be said
bool Ask(HelpSystem& help, const ecs::systems::DialogueControlSystemInterface& dialogue, const script_control::Vm& vm,
         uint32_t turn);

/// The last answer, for the debug view: what it was about and what was said
[[nodiscard]] const std::string& LastAnswer();

} // namespace openblack::help::help_key
