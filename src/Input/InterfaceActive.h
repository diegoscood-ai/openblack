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

// GInterface's active switch (runblack.exe W120, bit 0 of GInterface+0x28). The script's wide screen makes the interface
// inactive (HelpSystem::SetWideScreen 0x5C6AD0 -> SetActive(!(on && owner)), 0x5C6AF4 / 0x5C6B01), a hand demo makes it
// active again so its recorded messages drive the real hand (StartPlayBack 0x5DAD60) and inactive when it ends with a
// script still holding the bars (EndPlayBack 0x5DB3F0). Research: dev\documentacion\intro\spec_demo_mode.md §1.2.
namespace openblack::interface_active
{

/// GInterface::SetActive 0x5CEDC0: +0x28 bit 0 = (active == 0). (pending) its other effects: HelpSystem+0x460C = bit 0 of
/// GInterface+0x39 (0x5CEDDF), +0x40 &= ~4, fn_005D1260, ResetActionState 0x5D29C0 and fn_005D81C0 (StopAllImmersion)
void SetActive(bool active);
/// GInterface::IsActive 0x5CE2E0: !(+0x28 & 1). While inactive the interface's hand state is 25 (fn_005D7E40)
[[nodiscard]] bool IsActive();
/// GInterface+0x28 as a whole (bit 0 inactive; bits 1 and 2 are SET_INTERFACE_INTERACTION's limits, 0x70B7A8)
[[nodiscard]] uint8_t GetFlags();
void SetFlags(uint8_t flags);

} // namespace openblack::interface_active
