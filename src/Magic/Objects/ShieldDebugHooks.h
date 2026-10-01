/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// OPENBLACK_TEST_SHIELD_SHOT="<turns>,<path>[;<turns>,<path>...]": a screenshot that many game turns after the first
// MapShield was made (the frame count of --screenshot-frame drifts with the frame rate). Documented in
// docs/bw1-notes/openblack-internals.md.

namespace openblack::magic::shield_debug
{
/// MapShield::ProcessShields, each turn: `created` is the first shield's creation turn
void OnTurn(unsigned int created, unsigned int turn);
} // namespace openblack::magic::shield_debug
