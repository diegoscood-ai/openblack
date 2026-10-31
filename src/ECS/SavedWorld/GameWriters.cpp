/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameWriters.h"

using namespace openblack::ecs::saved_world_write;

ClassWriters openblack::ecs::saved_world_write::GameWriters()
{
	ClassWriters writers;
	// Each area's registration, one line each; a class with none is written from what its load kept
	RegisterGameObjectWriter(writers, saved_game_save::CurrentValues);
	RegisterForestWriters(writers);
	RegisterRiverAndMistWriters(writers);
	RegisterWeatherWriters(writers);
	RegisterReactionWriters(writers);
	RegisterPlayerWriters(writers, {.values = saved_players::CurrentValues,
	                                .objects = saved_players::ObjectsOf,
	                                .newRecord = saved_players::NewGameRecord});
	RegisterMobileStaticWriters(writers);
	RegisterFireWriters(writers);
	RegisterVillagerWriters(writers);
	RegisterArenaWriters(writers);
	RegisterDanceWriters(writers);
	RegisterFlockWriters(writers);
	RegisterPlayerInterfaceWriters(writers);
	RegisterComputerPlayerWriters(writers);
	RegisterParticleWriters(writers);
	RegisterSpellIconWriters(writers);
	RegisterSpellSeedWriters(writers);
	RegisterSpellWriters(writers);
	RegisterInfluenceRingWriters(writers);
	return writers;
}

void openblack::ecs::saved_world_write::KeepLoadedSources(const gamesave::WorldRead& world, LoadedObjects& loaded)
{
	KeepPlayerInterfaces(world, loaded);
	LoadComputerPlayers(world, loaded);
}
