/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <vector>

#include <glm/vec3.hpp>

#include "Audio/GAudio/BankTables.h"
#include "Audio/GameQueries.h"

// The music attached to script objects (ATTACH_MUSIC): ThingMusicInfo (AudioMusicThing.cpp, 0x38 bytes, vtable
// 0x8C4A14) in the list GAudio+0x184 (nodes {next, info}, count +0x188). This is the list and its plain operations;
// what plays (fn_00429500, fn_00429790...) is in GameMusic. Sources: dev\tmp_dis\audio\script.md §2.5, music.md
// §2.5.6 and the disassembly in script_dis_thingmusic.txt (cited at each step).

namespace openblack::audio
{

struct ThingMusicInfo
{
	/// +0x14 MUSIC_TYPE (ATTACH_MUSIC only reports a type outside 1..84 and attaches it anyway, 0x70FC31..0x70FC53)
	int type {0};
	ThingId thing {0};                  ///< +0x18
	int enabled {1};                    ///< +0x1C (1 in AddThingMusic, 0x42929C)
	int finished {0};                   ///< +0x20 (nothing in 0x429180..0x429950 sets it to 1)
	int started {0};                    ///< +0x24
	int hasPlayPosition {0};            ///< +0x28 (SET_MUSIC_PLAY_POSITION)
	glm::vec3 playPosition {0.0f};      ///< +0x2C..+0x34 MapCoords, as a world point (see SetPlayPosition)
};

class ThingMusicList
{
public:
	/// GetThingMusicInfo 0x429180: the first node whose info has that thing (nullptr if none)
	[[nodiscard]] ThingMusicInfo* Get(ThingId thing);
	/// The new info of AddThingMusic 0x429265..0x4292AC (enabled 1, the rest 0), put at the head of the list
	/// (0x4292D7..0x4292E8); the caller has checked that the thing has no info yet
	ThingMusicInfo& AddFront(int type, ThingId thing);
	/// RemoveThingMusic 0x429340 (and the purges 0x4297FE / fn_00429700): every node of that thing's info goes and the
	/// info is deleted. Nothing is stopped.
	void Remove(ThingId thing);
	/// ReleaseAllThingMusicInfo 0x4291B0
	void Clear() { _infos.clear(); }

	/// fn_00429880 (MOVE_MUSIC): the info of from now belongs to to
	void Move(ThingId from, ThingId to);
	/// fn_004298A0 (ENABLE_DISABLE_MUSIC): enabled = on, finished = started = 0 (0x4298B4..0x4298BA)
	void Enable(ThingId thing, int on);
	/// SetPlayPosition 0x4298C0 (SET_MUSIC_PLAY_POSITION): +0x28 = 1, +0x2C = MapCoords(point). MapCoords::Set 0x603340
	/// keeps x and z as ftol(v * 6553.6) and the height above the land; fn_00429500 adds GetAltitude back
	/// (0x42954C..0x42955D) and multiplies x and z by 1 / 6553.6 (0x8AA3A4), so the point comes back with x and z
	/// truncated to the 16.16 grid and the same y (ecs::map_coords::Quantise: 6553.6f at 0x8AC400, 0x39200000 = 10 / 65536
	/// exactly at 0x8AA3A4).
	void SetPlayPosition(ThingId thing, glm::vec3 point);
	/// IsMusicThingFinished 0x4298F0: +0x20, or 1 without an info
	[[nodiscard]] int IsFinished(ThingId thing);

	[[nodiscard]] std::vector<ThingMusicInfo>& GetInfos() { return _infos; }
	[[nodiscard]] const std::vector<ThingMusicInfo>& GetInfos() const { return _infos; }
	[[nodiscard]] size_t GetCount() const { return _infos.size(); } ///< +0x188

private:
	/// In the order of the linked list (head first)
	std::vector<ThingMusicInfo> _infos;
};

} // namespace openblack::audio
