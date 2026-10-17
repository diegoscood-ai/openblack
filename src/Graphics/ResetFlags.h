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

#include <bgfx/defines.h>

namespace openblack::graphics
{

/// The back buffer's reset flags with the vertical sync switched on or off, every other flag kept as it was
[[nodiscard]] constexpr uint32_t ResetFlagsWithVsync(uint32_t flags, bool vsync) noexcept
{
	return vsync ? (flags | BGFX_RESET_VSYNC) : (flags & ~static_cast<uint32_t>(BGFX_RESET_VSYNC));
}

} // namespace openblack::graphics
