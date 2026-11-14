/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LoadThingMusic.h"

#include <cstdint>

#include <bit>
#include <optional>

#include "3D/MapCoords.h"
#include "ECS/SavedWorldLoad.h"
#include "Sound/GameMusic.h"

using namespace openblack;
using namespace openblack::ecs;

std::vector<audio::ThingMusicInfo>
load_thing_music::InfosOf(std::span<const Planned> saved, const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	std::vector<audio::ThingMusicInfo> infos;
	for (const auto& [number, record] : saved)
	{
		const auto thing = record.object.number != 0 ? objectOf(record.object.number) : entt::entity {entt::null};
		if (thing == entt::null)
		{
			continue;
		}
		audio::ThingMusicInfo info {
		    .type = static_cast<int>(record.word),
		    .thing = static_cast<audio::ThingId>(entt::to_integral(thing)),
		    .enabled = static_cast<int>(record.words[0]),
		    .finished = static_cast<int>(record.words[1]),
		    .started = static_cast<int>(record.words[2]),
		    .hasPlayPosition = static_cast<int>(record.words[3]),
		    .key = number,
		};
		// the place is the game's map coordinates, which the record's point holds as they are: x and z in 16.16
		// units, the height above the land, which the music adds back as it plays
		info.playCoords = {.x = std::bit_cast<int32_t>(record.point[0]),
		                   .z = std::bit_cast<int32_t>(record.point[1]),
		                   .altitude = record.point[2]};
		if (info.hasPlayPosition != 0)
		{
			info.playPosition = map_coords::ToWorld(info.playCoords);
		}
		infos.push_back(info);
	}
	return infos;
}

void load_thing_music::Restore(std::vector<audio::ThingMusicInfo> infos)
{
	const auto lock = audio::game_music::Lock();
	if (auto* music = audio::game_music::Get(); music != nullptr)
	{
		music->RestoreThingMusic(std::move(infos));
	}
}

void load_thing_music::RestoreScriptMusic(const saved_world_load::PlannedScriptMusic& music, entt::entity town)
{
	const auto lock = audio::game_music::Lock();
	if (auto* game = audio::game_music::Get(); game != nullptr)
	{
		game->RestoreFromSave(static_cast<int>(music.type), static_cast<int>(music.playing),
		                      town != entt::null ? std::optional(static_cast<uint32_t>(entt::to_integral(town)))
		                                         : std::nullopt);
	}
}
