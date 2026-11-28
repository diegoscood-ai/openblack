/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FileSystemInterface.h"

#include <algorithm>
#include <array>
#include <exception>
#include <span>

using namespace openblack::filesystem;

FileSystemInterface::~FileSystemInterface() = default;

std::filesystem::path FileSystemInterface::FixPath(const std::filesystem::path& path)
{
	std::string result = path.generic_string();

	constexpr std::array<std::string_view, 3> caseFixTable = {
	    "\\Data\\",
	    "\\Landscape\\",
	    "\\Multi_Player\\",
	};
	for (const auto& pattern : caseFixTable)
	{
		auto foundIter = std::search(result.cbegin(), result.cend(), pattern.cbegin(), pattern.cend(),
		                             [](char left, char right) { return std::toupper(left) == std::toupper(right); });
		if (foundIter != result.cend())
		{
			result.replace(foundIter, foundIter + pattern.size(), pattern.data());
		}
	}

	for (auto pos = result.find('\\'); pos != std::string::npos; pos = result.find('\\', pos + 1))
	{
		result[pos] = '/';
	}

	return result;
}

FileSystemInterface::WriteResult FileSystemInterface::WriteFile(const std::filesystem::path& path,
                                                                std::span<const uint8_t> bytes)
{
	std::unique_ptr<Stream> stream;
	try
	{
		stream = Open(path, Stream::Mode::Write);
	}
	catch (const std::exception&)
	{
		return WriteResult::NotOpened;
	}
	if (stream == nullptr)
	{
		return WriteResult::NotOpened;
	}
	try
	{
		stream->Write(bytes.data(), bytes.size());
	}
	catch (const std::exception&)
	{
		return WriteResult::Incomplete;
	}
	return WriteResult::Written;
}
