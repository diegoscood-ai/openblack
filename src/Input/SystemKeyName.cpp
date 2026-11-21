/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SystemKeyName.h"

#include <algorithm>
#include <array>

#include "EnglishUsKeyNames.h"
#include "Locator.h"
#include "SystemKeyNames.h"

#if defined(_WIN32)
// (NOMINMAX and WIN32_LEAN_AND_MEAN come from src/CMakeLists.txt)
#include <windows.h>
#endif

namespace openblack::input::system_key_name
{
namespace
{
#if defined(_WIN32)
/// The system's name in its wide form, else its narrow one made wide in the system's code page, as the game asks
std::u16string WindowsName(int32_t param, int32_t room)
{
	std::array<wchar_t, k_NameRoom> wide {};
	const int32_t size = std::min<int32_t>(room, static_cast<int32_t>(wide.size()));
	GetKeyNameTextW(param, wide.data(), size);
	if (wide[0] == L'\0')
	{
		std::array<char, k_NameRoom> narrow {};
		GetKeyNameTextA(param, narrow.data(), size);
		std::array<wchar_t, k_NameRoom> converted {};
		const int count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, narrow.data(), -1, converted.data(),
		                                      static_cast<int>(converted.size()));
		if (count > 0)
		{
			wide = converted;
		}
	}
	std::u16string name;
	for (const wchar_t c : wide)
	{
		if (c == L'\0')
		{
			break;
		}
		name.push_back(static_cast<char16_t>(c));
	}
	return name;
}
#endif
} // namespace

SystemKeyNames::SystemKeyNames(RealInputQuery realInput) noexcept
    : _realInput(realInput)
{
}

SystemKeyNames::SystemKeyNames(Source source) noexcept
    : _source(source)
{
}

std::optional<std::u16string> SystemKeyNames::Name(uint8_t code)
{
	if (_source != nullptr)
	{
		return _source(NameParam(code), k_NameRoom);
	}
	if (_realInput == nullptr)
	{
		return std::nullopt;
	}
	if (!_realInput())
	{
		return std::u16string(EnglishUsName(code));
	}
#if defined(_WIN32)
	return WindowsName(NameParam(code), k_NameRoom);
#else
	return std::nullopt;
#endif
}

std::optional<std::u16string> Name(uint8_t code)
{
	if (!Locator::keyNames::has_value())
	{
		return std::nullopt;
	}
	return Locator::keyNames::value().Name(code);
}

std::u16string_view EnglishUsName(uint8_t code) noexcept
{
	const auto found = std::ranges::find(k_EnglishUsNames, code, &KeyNameOf::code);
	return found != k_EnglishUsNames.end() ? found->name : std::u16string_view();
}

} // namespace openblack::input::system_key_name
