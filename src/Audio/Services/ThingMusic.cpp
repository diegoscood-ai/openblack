/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingMusic.h"

#include <algorithm>

#include "3D/MapCoords.h"

namespace openblack::audio
{

ThingMusicInfo* ThingMusicList::Get(ThingId thing)
{
	// From the head, the first info of that thing
	const auto it = std::find_if(_infos.begin(), _infos.end(), [thing](const auto& info) { return info.thing == thing; });
	return it != _infos.end() ? &*it : nullptr;
}

ThingMusicInfo& ThingMusicList::AddFront(int type, ThingId thing)
{
	ThingMusicInfo info {
	    .type = type,
	    .thing = thing,
	    .enabled = 1,
	    .finished = 0,
	    .started = 0,
	    .hasPlayPosition = 0, // (and the play position 0)
	    .key = ++_lastKey,
	};
	_infos.insert(_infos.begin(), info);
	return _infos.front();
}

void ThingMusicList::Restore(std::vector<ThingMusicInfo> infos)
{
	_infos = std::move(infos);
	for (const auto& info : _infos)
	{
		_lastKey = std::max(_lastKey, info.key);
	}
}

void ThingMusicList::Remove(ThingId thing)
{
	// The thing's info, then every node pointing at it. Two infos of one thing cannot exist (attaching reuses the
	// first), so this is the first one.
	const auto it = std::find_if(_infos.begin(), _infos.end(), [thing](const auto& info) { return info.thing == thing; });
	if (it != _infos.end())
	{
		_infos.erase(it);
	}
}

void ThingMusicList::Move(ThingId from, ThingId to)
{
	if (auto* info = Get(from); info != nullptr)
	{
		info->thing = to;
	}
}

void ThingMusicList::Enable(ThingId thing, int on)
{
	if (auto* info = Get(thing); info != nullptr)
	{
		info->enabled = on;
		info->finished = 0;
		info->started = 0;
	}
}

void ThingMusicList::SetPlayPosition(ThingId thing, glm::vec3 point, const map_coords::MapCoords& coords)
{
	if (auto* info = Get(thing); info != nullptr)
	{
		info->hasPlayPosition = 1;
		info->playCoords = coords;
		// Stored as fixed-point map coordinates and read back in metres when the music plays
		info->playPosition = {map_coords::Quantise(point.x), point.y, map_coords::Quantise(point.z)};
	}
}

int ThingMusicList::IsFinished(ThingId thing)
{
	const auto* info = Get(thing);
	return info != nullptr ? info->finished : 1;
}

} // namespace openblack::audio
