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

#include <array>
#include <functional>

#include "ECS/SavedGameSave.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "Enums.h"

namespace openblack::particles
{
struct ProcessInfo;
}

// The game's class writers. Each area registers its classes' writers from its own file,
// src/ECS/SavedWorld/Write<Area>.cpp, through a function declared here and called from GameWriters. A class with no
// writer is written from the records its load kept. See docs/wiki/engine/saved-games.md#openblack.
namespace openblack::ecs::saved_world_write
{

/// The forests', the trees' and the big forests' (WriteForests.cpp)
void RegisterForestWriters(ClassWriters& writers);
/// The climates', with their natural storms (WriteWeather.cpp)
void RegisterWeatherWriters(ClassWriters& writers);
/// The rivers' and the banks of mist's (WriteRiversAndMists.cpp)
void RegisterRiverAndMistWriters(ClassWriters& writers);
/// The rocks' and the other mobile statics' (WriteMobileStatics.cpp)
void RegisterMobileStaticWriters(ClassWriters& writers);
/// The fires', a source keyed by each fire's handle (WriteFires.cpp)
void RegisterFireWriters(ClassWriters& writers);
/// The villagers' and the special villagers' (WriteVillagers.cpp)
void RegisterVillagerWriters(ClassWriters& writers);
/// The arenas', the land's and the creature fights' (WriteArenas.cpp)
void RegisterArenaWriters(ClassWriters& writers);
/// The dances', their groups', key frames' and actions' (WriteDances.cpp)
void RegisterDanceWriters(ClassWriters& writers);
/// The flocks' and the game's list of them (WriteFlocks.cpp)
void RegisterFlockWriters(ClassWriters& writers);
/// The players' interfaces, their statuses and their leashes' statuses (WritePlayerInterfaces.cpp)
void RegisterPlayerInterfaceWriters(ClassWriters& writers);
/// Of a loaded world, each player's interface, its status and its leash's status, by the player's number
void KeepPlayerInterfaces(const gamesave::WorldRead& world, LoadedObjects& loaded);

/// The players' computer players, their hands' states and their attitudes (WriteComputerPlayers.cpp)
void RegisterComputerPlayerWriters(ClassWriters& writers);
/// Of a loaded world, each player's computer player, its hand's state and its eight attitudes, by the player's number
/// (and the other player's); and what the computer players' service keeps of them put back (its Restore)
void LoadComputerPlayers(const gamesave::WorldRead& world, LoadedObjects& loaded);
/// The number by which a record names the player of the number: the players are a source's objects, by their numbers
/// (`SourceKey` of the players' class); 0 for a number that is no player's (WriteComputerPlayers.cpp)
[[nodiscard]] gamesave::ObjectNumber PlayerObject(WriteContext& context, int player);

/// The spot visuals' containers, with their effects' particle interfaces and managers, and the game's list of them
/// (WriteParticles.cpp)
void RegisterParticleWriters(ClassWriters& writers);

/// The town centres' and the worship sites' spell icons (WriteSpellIcons.cpp)
void RegisterSpellIconWriters(ClassWriters& writers);
/// The number a save names a spell seed's table row by: the rows run from the storm's, 2578, down
[[nodiscard]] uint32_t SpellSeedInfoRow(SpellSeedType type);

/// The 60 bytes of a particle effect's or a spell's process info as the game keeps it: the interface's place, the
/// gesture's, the camera's forward, the direction, the strength, the angular speed and whether its creator still casts
/// it; the three bytes after that are not read (0) (WriteParticles.cpp)
[[nodiscard]] std::array<uint8_t, 60> ProcessInfoBytes(const particles::ProcessInfo& info);
/// The spells of the classes whose records are known: the water spell, and the game's list of spells
/// (WriteSpells.cpp)
void RegisterSpellWriters(ClassWriters& writers);
/// A spell's seed (WriteSpellSeeds.cpp)
void RegisterSpellSeedWriters(ClassWriters& writers);
/// The influence rings and the game's list of them (WriteInfluenceRings.cpp)
void RegisterInfluenceRingWriters(ClassWriters& writers);
/// The reactions', a source of no entity, and the game's list of them (WriteReactions.cpp)
void RegisterReactionWriters(ClassWriters& writers);

/// What a load does with the loaded objects that no entity stands for and no maker of saved_world_load::Apply makes:
/// each area keeps them for their classes' writers and puts back what openblack keeps of them, one line each
void KeepLoadedSources(const gamesave::WorldRead& world, LoadedObjects& loaded);

/// Every area's class writers
[[nodiscard]] ClassWriters GameWriters();

/// The game object's writer (WriteGame.cpp): the record its load kept, or a new one, with the values `values` gives
/// (saved_game_save::CurrentValues in the game) put where the save writes them; a new game's players from their
/// entities; each global list by its list writer, else as the load kept it (a new game needs every list's writer)
void RegisterGameObjectWriter(ClassWriters& writers, std::function<saved_game_save::GameValues()> values);

/// What the players' writer takes from the game
struct PlayerSources
{
	/// A player's values (saved_players::CurrentValues in the game); none leaves its record's
	std::function<std::optional<saved_players::PlayerValues>(PlayerNames)> values;
	/// The objects a player's record names that openblack makes (saved_players::ObjectsOf in the game)
	std::function<saved_players::PlayerObjects(const Registry&, PlayerNames)> objects;
	/// A new game's record of a player (saved_players::NewGameRecord in the game)
	std::function<gamesave::PlayerRecord(PlayerNames)> newRecord;
};

/// The players' writer (WritePlayers.cpp): the game's eight players, a source of the players' class by their numbers.
/// Each from the record its load kept, or a new game's record, with the values and objects `sources` gives put where
/// the save writes them; a new game's computer player, statistics and interface named as their classes' objects of
/// the player's number
void RegisterPlayerWriters(ClassWriters& writers, PlayerSources sources);

} // namespace openblack::ecs::saved_world_write
