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

/// The back buffer's reset flags with four-sample multisampling on or off, every other flag kept as it was. The
/// original draws with none; the debug bar's switch (off) is the only one to turn it on
[[nodiscard]] constexpr uint32_t ResetFlagsWithMultisampling(uint32_t flags, bool multisampling) noexcept
{
	const uint32_t others = flags & ~static_cast<uint32_t>(BGFX_RESET_MSAA_MASK);
	return multisampling ? (others | BGFX_RESET_MSAA_X4) : others;
}

} // namespace openblack::graphics
