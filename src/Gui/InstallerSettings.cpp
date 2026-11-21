/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InstallerSettings.h"

#include <array>

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
// clang-format on
// (NOMINMAX and WIN32_LEAN_AND_MEAN come from src/CMakeLists.txt)
#include <windows.h>
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

std::u16string SetupUserName()
{
	std::u16string name;
#ifdef _WIN32
	// the game reads the text into a buffer of 512 bytes, then widens it in the system's code page (2047 at most)
	constexpr size_t k_TextRoom = 0x200;
	constexpr int k_WideRoom = 2047;
	std::array<char, k_TextRoom> text {};
	DWORD size = static_cast<DWORD>(text.size());
	if (RegGetValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\MS Setup (ACME)\\User Info", "DefName", RRF_RT_REG_SZ, nullptr,
	                 text.data(), &size) != ERROR_SUCCESS)
	{
		return name;
	}
	std::array<wchar_t, k_WideRoom + 1> wide {};
	const int count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, text.data(), -1, wide.data(), k_WideRoom);
	for (int i = 0; i < count && wide[static_cast<size_t>(i)] != L'\0'; ++i)
	{
		name.push_back(static_cast<char16_t>(wide[static_cast<size_t>(i)]));
	}
#endif
	return name;
}

} // namespace openblack::gui::installer_settings
