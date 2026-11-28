/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <vector>

namespace openblack
{
/// A map the land makes again at each change of its altitudes (the island's height map), with every map it replaced
/// kept by version: version n is the map before the n+1th change, and the current one is the version after the last
template <typename Map>
class VersionedMaps
{
public:
	/// The current map is `map`; the one it replaces, if any, is kept as the next version
	void Replace(std::unique_ptr<Map> map)
	{
		if (_current != nullptr)
		{
			_before.push_back(std::move(_current));
		}
		_current = std::move(map);
	}
	/// The first map, with nothing kept before it
	void Reset(std::unique_ptr<Map> map)
	{
		_before.clear();
		_current = std::move(map);
	}
	/// How many maps were replaced: the current map's version
	[[nodiscard]] uint32_t Version() const { return static_cast<uint32_t>(_before.size()); }
	[[nodiscard]] const Map& Current() const { return *_current; }
	[[nodiscard]] Map& Current() { return *_current; }
	/// The map of a version: a replaced one, or the current one for its own version or a later one
	[[nodiscard]] const Map& At(uint32_t version) const { return version < _before.size() ? *_before.at(version) : *_current; }

private:
	std::unique_ptr<Map> _current;
	std::vector<std::unique_ptr<Map>> _before;
};
} // namespace openblack
