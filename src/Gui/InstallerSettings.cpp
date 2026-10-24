/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstallerSettings.h"

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
#endif

namespace openblack::gui::installer_settings
{

std::optional<uint32_t> Number([[maybe_unused]] const char* name)
{
#ifdef _WIN32
	DWORD value = 0;
	DWORD size = sizeof(value);
	if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Lionhead Studios Ltd\\Black & White", name, RRF_RT_REG_DWORD, nullptr,
	                 &value, &size) == ERROR_SUCCESS)
	{
		return static_cast<uint32_t>(value);
	}
#endif
	return std::nullopt;
}

} // namespace openblack::gui::installer_settings
