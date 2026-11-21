/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StringUtils.h"

#include <cctype>
#include <cstring>

#include <algorithm>
#include <array>
#include <iterator>
#include <string_view>
#include <vector>

bool openblack::string_utils::EndsWith(const std::string& string, const std::string& ending)
{
	if (string.length() < ending.length())
	{
		return false;
	}
	return string.compare(string.length() - ending.length(), ending.length(), ending) == 0;
}

bool openblack::string_utils::BeginsWith(const std::string& string, const std::string& beginning)
{
	if (string.length() < beginning.length())
	{
		return false;
	}
	return string.compare(0, beginning.length(), beginning) == 0;
}

std::string openblack::string_utils::UpperCase(const std::string& string)
{
	auto uppercase = string;
	std::transform(uppercase.begin(), uppercase.end(), uppercase.begin(),
	               [](auto c) { return static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(c))); });
	return uppercase;
}

std::string openblack::string_utils::LowerCase(const std::string& string)
{
	auto lowercase = string;
	std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
	               [](auto c) { return static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c))); });
	return lowercase;
}

std::string openblack::string_utils::Capitalise(const std::string& string)
{
	auto capitalised = string;
	capitalised[0] = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(string[0])));
	return capitalised;
}

[[nodiscard]] std::vector<std::string> openblack::string_utils::Split(const std::string& string, const std::string& delimiter)
{
	auto parts = std::vector<std::string>();
	size_t start = 0;
	auto end = string.find(delimiter);

	while (end != std::string::npos)
	{
		parts.emplace_back(string.substr(start, end - start));
		start = end + delimiter.length();
		end = string.find(delimiter, start);
	}
	parts.emplace_back(string.substr(start));

	return parts;
}

std::string openblack::string_utils::ExtractQuote(std::string& string)
{
	size_t const first(string.find('\"'));
	size_t const second(string.find('\"', first + 1));
	return string.substr(first + 1, second - first - 1);
}

std::u16string openblack::string_utils::WidenWestern(std::string_view bytes)
{
	// The characters of 0x80..0x9F; the five bytes the page leaves unset keep their own value
	static constexpr std::array<char16_t, 32> k_Page1252 {
	    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
	    0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
	    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
	};
	// The conversion's buffer holds 2047 characters with the closing NUL
	constexpr size_t k_MaxBytes = 2046;
	std::u16string wide;
	if (bytes.size() > k_MaxBytes)
	{
		return wide;
	}
	wide.reserve(bytes.size());
	std::ranges::transform(bytes, std::back_inserter(wide), [](char c) {
		const auto byte = static_cast<unsigned char>(c);
		return byte >= 0x80 && byte <= 0x9F ? k_Page1252.at(byte - 0x80) : static_cast<char16_t>(byte);
	});
	return wide;
}

bool openblack::string_utils::EqualsIgnoringCase(std::string_view text, const std::array<char, 0x30>& name)
{
	const auto length = strnlen(name.data(), name.size());
	return text.size() == length && std::ranges::equal(text, std::string_view(name.data(), length), [](char a, char b) {
		       return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	       });
}
