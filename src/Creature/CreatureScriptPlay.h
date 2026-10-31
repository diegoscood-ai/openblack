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

#include <vector>

#include "Creature/CreatureIdleMind.h"

/// A script's "play" for a creature ("CreatureCow play 63 loop 1"): the script first gives the animation and how many
/// times, then tells the creature to start. The creature gives up what it was doing, plays it, waits for its body to be
/// free, and goes back to its own plans. Scripts also ask whether a creature has played what it was doing.
namespace openblack::creature_script_play
{

/// Numbers from this one on are the creature's individual actions, played once each time; those before it are its
/// static actions (a start, a loop and an end, as sitting), the number being the loop's, the start and the end the
/// numbers either side of it
constexpr uint32_t k_FirstIndividualAction = 52;
/// A static action's loop lasts this many seconds, in the game's whole turns counted as it starts
constexpr float k_StaticActionSeconds = 10.0f;
/// The game takes the number as signed to look its clip up, so a number past this one is a clip below 0
constexpr uint32_t k_LastClipNumber = 0x7FFF'FFFFu;

/// What a script last gave a creature to play
struct Request
{
	uint32_t animation {0};
	uint32_t plays {0};
};

/// The agenda for what the script gave: the action that many times, each played through (a static action's loop held
/// k_StaticActionSeconds before its end, mirrored half the time, drawn on every turn it tries to start), then a wait
/// until its body is free, which ends a turn after the body is seen free. The static action 0, which has no start
/// before it, and an individual action below 0 never start: the creature tries them every turn and never gets past
/// the first.
[[nodiscard]] std::vector<creature_mind::Step> Agenda(const Request& request);

/// Whether a creature has played what it was doing: its agenda is over, or it is only being idle. A creature busy with
/// any agenda, its own or a script's, hasn't.
[[nodiscard]] bool Played(const creature_mind::IdleMind& mind);

} // namespace openblack::creature_script_play
