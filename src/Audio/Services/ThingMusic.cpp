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

#include "ECS/MapCoords.h"

namespace openblack::audio
{

ThingMusicInfo* ThingMusicList::Get(ThingId thing)
{
	// 0x42918E..0x42919A: from the head, info+0x18 == thing
	const auto it = std::find_if(_infos.begin(), _infos.end(), [thing](const auto& info) { return info.thing == thing; });
	return it != _infos.end() ? &*it : nullptr;
}

ThingMusicInfo& ThingMusicList::AddFront(int type, ThingId thing)
{
	ThingMusicInfo info;
	info.type = type;   // 0x429296
	info.thing = thing; // 0x429299
	info.enabled = 1;   // 0x42929C
	info.started = 0;   // 0x4292A3
	info.finished = 0;  // 0x4292A6
	info.hasPlayPosition = 0; // 0x4292A9 (and the MapCoords 0 at 0x429287..0x42928D)
	_infos.insert(_infos.begin(), info);
	return _infos.front();
}

void ThingMusicList::Remove(ThingId thing)
{
	// RemoveThingMusic 0x429349..0x4293A3: the info of GetThingMusicInfo, then every node pointing at it. Two infos of
	// one thing cannot exist (AddThingMusic reuses the first), so this is the first one.
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
		info->thing = to; // 0x429892
	}
}

void ThingMusicList::Enable(ThingId thing, int on)
{
	if (auto* info = Get(thing); info != nullptr)
	{
		info->enabled = on; // 0x4298B4
		info->finished = 0; // 0x4298B7
		info->started = 0;  // 0x4298BA
	}
}

void ThingMusicList::SetPlayPosition(ThingId thing, glm::vec3 point)
{
	if (auto* info = Get(thing); info != nullptr)
	{
		info->hasPlayPosition = 1; // 0x4298D2
		// 0x4298D9..0x4298E9: MapCoords::Set 0x603340 (ToFixed), read back by fn_00429500 (ToMetres, 0x429580..0x429587)
		info->playPosition = {ecs::map_coords::Quantise(point.x), point.y, ecs::map_coords::Quantise(point.z)};
	}
}

int ThingMusicList::IsFinished(ThingId thing)
{
	const auto* info = Get(thing);
	return info != nullptr ? info->finished : 1; // 0x4298FE / 0x429904
}

} // namespace openblack::audio
