/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerPlayerMindTree.h"

#include <algorithm>

namespace openblack::computer_player
{

namespace
{

/// The name a script gives begins with the node's name, as it is now, up to its ':'
bool NameMatchesNow(uint16_t node, std::string_view name, const MindRowNames& names)
{
	const auto prefix = NamePrefix(names.Name(MindNodes().at(node).row));
	return name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix;
}

} // namespace

MindRowNames::MindRowNames()
{
	for (size_t row = 0; row < k_MindRows.size(); ++row)
	{
		// the name and its nul: the room a file read writes in
		_rooms.at(row) = std::string(k_MindRows.at(row).name) + '\0';
	}
}

std::string_view MindRowNames::Name(size_t row) const
{
	// up to the first nul; a room whose own nul was written over ends where the room ends
	return {_rooms.at(row).c_str()};
}

std::span<char> MindRowNames::Room(size_t row)
{
	auto& room = _rooms.at(row);
	return {room.data(), room.size()};
}

const MindRowNames& OwnRowNames()
{
	static const MindRowNames own;
	return own;
}

std::vector<uint16_t> FindNodes(std::string_view name, const MindRowNames& names)
{
	std::vector<uint16_t> found;
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		if (NameMatchesNow(node, name, names))
		{
			found.push_back(node);
		}
	}
	return found;
}

std::optional<uint16_t> FindNode(std::string_view name, const MindRowNames& names)
{
	for (uint16_t node = 0; node < k_MindNodeCount; ++node)
	{
		if (NameMatchesNow(node, name, names))
		{
			return node;
		}
	}
	return std::nullopt;
}

std::span<const std::string_view> MindNodeNames()
{
	static const std::vector<std::string_view> names = [] {
		std::vector<std::string_view> firstMet;
		for (uint16_t node = 0; node < k_MindNodeCount; ++node)
		{
			const auto prefix = NamePrefix(RowOf(node).name);
			if (std::ranges::find(firstMet, prefix) == firstMet.end())
			{
				firstMet.push_back(prefix);
			}
		}
		return firstMet;
	}();
	return names;
}

} // namespace openblack::computer_player
