/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InterfaceActive.h"

namespace openblack::interface_active
{
namespace
{
/// GInterface+0x28 (0 from the GInterface constructor: inferred, not read)
uint8_t s_flags = 0;
} // namespace

void SetActive(bool active)
{
	// 0x5CEDC0: and/or of bit 0 with (active == 0)
	s_flags = static_cast<uint8_t>((s_flags & ~1u) | (active ? 0u : 1u));
}

bool IsActive()
{
	return (s_flags & 1u) == 0; // 0x5CE2E0
}

uint8_t GetFlags()
{
	return s_flags;
}

void SetFlags(uint8_t flags)
{
	s_flags = flags;
}

} // namespace openblack::interface_active
