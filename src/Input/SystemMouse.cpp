/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SystemMouse.h"

#ifdef _WIN32
// clang-format off
// windows.h without its min and max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// clang-format on
#endif

namespace openblack::input
{
SystemMouse QuerySystemMouse() noexcept
{
#ifdef _WIN32
	return {.hasWheel = GetSystemMetrics(SM_MOUSEWHEELPRESENT) != 0, .buttons = GetSystemMetrics(SM_CMOUSEBUTTONS)};
#else
	return {};
#endif
}
} // namespace openblack::input
