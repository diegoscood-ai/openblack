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
#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <SavedRecords.h>

#include "Audio/Services/ThingMusic.h"
#include "ECS/Components/ComputerHand.h"
#include "ECS/FireFlies.h"
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
/// The ways into another land a script makes and their crossing files' saves, a source keyed by each way in
/// (WriteVortices.cpp)
void RegisterVortexWriters(ClassWriters& writers);
/// The crossing files the world's crossing files' saves name, each once: the files a save copies beside its world file
/// as it writes it (WriteVortices.cpp)
[[nodiscard]] std::vector<std::filesystem::path> CrossingFiles(const ClassWriters& writers, const WorldSource& source);
/// The fires', a source keyed by each fire's handle (WriteFires.cpp)
void RegisterFireWriters(ClassWriters& writers);
/// The fireballs' balls (WriteFireBalls.cpp)
void RegisterFireBallWriters(ClassWriters& writers);
/// The villagers' and the special villagers' (WriteVillagers.cpp)
void RegisterVillagerWriters(ClassWriters& writers);
/// The arenas', the land's and the creature fights' (WriteArenas.cpp)
void RegisterArenaWriters(ClassWriters& writers);
/// The data paths', each the walk of a living thing along a script's track (WriteDataPaths.cpp)
void RegisterDataPathWriters(ClassWriters& writers);
/// The dances', their groups', key frames' and actions' (WriteDances.cpp)
void RegisterDanceWriters(ClassWriters& writers);
/// The flocks' and the game's list of them (WriteFlocks.cpp)
void RegisterFlockWriters(ClassWriters& writers);
/// The animals' and birds', of every class (WriteAnimals.cpp)
void RegisterAnimalWriters(ClassWriters& writers);
/// The players' interfaces, their statuses and their leashes' statuses (WritePlayerInterfaces.cpp)
void RegisterPlayerInterfaceWriters(ClassWriters& writers);
/// Of a loaded world, each player's interface, its status and its leash's status, by the player's number
void KeepPlayerInterfaces(const gamesave::WorldRead& world, LoadedObjects& loaded);

/// The game camera's, a source of one object (WriteCamera.cpp)
void RegisterCameraWriters(ClassWriters& writers);
/// Of a loaded world, the camera the game object names, by its one id (saved_camera::k_CameraId)
void KeepCamera(const gamesave::WorldRead& world, LoadedObjects& loaded);

/// The players' computer players, their hands' states and their attitudes (WriteComputerPlayers.cpp)
void RegisterComputerPlayerWriters(ClassWriters& writers);
/// Of a loaded world, each player's computer player, its hand's state and its eight attitudes, by the player's number
/// (and the other player's); and what the computer players' service keeps of them put back (its Restore)
void LoadComputerPlayers(const gamesave::WorldRead& world, LoadedObjects& loaded);
/// The hand of a computer player as its saved state keeps it, when it has steps (a new hand is made as a script first
/// gives it some): its place, the point its step goes to, the steps' and the player's speed, whether the first step is
/// to be looked at for a move in front and whether one was put there, the first step's stage, a wait's turns left, a
/// gesture's origin and progress, whether a script holds it, and its steps with their arguments, the things the load
/// made again by `loaded`
[[nodiscard]] std::optional<components::ComputerHand> HandOfRecord(const gamesave::WorldRead& world,
                                                                   const LoadedObjects& loaded,
                                                                   const gamesave::ActionStateRecord& state,
                                                                   const gamesave::ComputerPlayerRecord& computer);
/// The number by which a record names the player of the number: the players are a source's objects, by their numbers
/// (`SourceKey` of the players' class); 0 for a number that is no player's (WriteComputerPlayers.cpp)
[[nodiscard]] gamesave::ObjectNumber PlayerObject(WriteContext& context, int player);

/// The spot visuals' containers, with their effects' particle interfaces and managers, and the game's list of them
/// (WriteParticles.cpp)
void RegisterParticleWriters(ClassWriters& writers);
/// The container of the spot visual whose object is given (the object StartSpotVisual gives), by its effect's key; 0
/// once the spot visual has gone
[[nodiscard]] gamesave::ObjectNumber SpotVisualObject(WriteContext& context, entt::entity object);
/// The number a record names an object of the scripts' table by: a spot visual's object (one a script holds) by its
/// container, as the game's table holds the container; a player's entity with a computer player by that computer
/// player's object, as the game's table holds the computer player; any other entity by its own class
/// (WriteParticles.cpp)
[[nodiscard]] gamesave::ObjectNumber ScriptTableObject(WriteContext& context, entt::entity object);
/// The key of the effect of the spot visual whose object is given, the id its container is a source's object by; none
/// once it has gone (WriteParticles.cpp)
[[nodiscard]] std::optional<uint64_t> SpotVisualEffect(entt::entity object);

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
/// A puzzle game (WritePuzzleGames.cpp)
void RegisterPuzzleGameWriters(ClassWriters& writers);
/// The influence rings and the game's list of them (WriteInfluenceRings.cpp)
void RegisterInfluenceRingWriters(ClassWriters& writers);
/// The reactions', a source of no entity, and the game's list of them (WriteReactions.cpp)
void RegisterReactionWriters(ClassWriters& writers);
/// The creatures' (WriteCreatures.cpp)
void RegisterCreatureWriters(ClassWriters& writers);
/// The footpaths, their nodes and the footpath links (WriteFootpaths.cpp)
void RegisterFootpathWriters(ClassWriters& writers);
/// The abodes', the creches', the graveyards', the storage pits' and their wood and food piles' (WriteAbodes.cpp)
void RegisterAbodeWriters(ClassWriters& writers);
/// The towns' desire flags' (WriteTownDesireFlags.cpp)
void RegisterTownDesireFlagWriters(ClassWriters& writers);
/// The game's lists whose objects are entities of one kind, newest first (WriteGameLists.cpp)
void RegisterGameListWriters(ClassWriters& writers);
/// The citadels' hearts' (WriteCitadelHearts.cpp)
void RegisterCitadelHeartWriters(ClassWriters& writers);
/// The worship sites', their altars', what the altars show of the sites' needs and the signs' (WriteWorshipSites.cpp)
void RegisterWorshipSiteWriters(ClassWriters& writers);
/// The towns' (WriteTowns.cpp)
void RegisterTownWriters(ClassWriters& writers);
/// A scaffold's own planned abode, its plan (0) or its candidate (1), by the id `PartId(scaffold, index)` names it with:
/// the planned abodes' source writes a town's plans and these (WritePots.cpp)
[[nodiscard]] std::optional<gamesave::SavedRecord> WriteScaffoldPlan(WriteContext& context, uint64_t id);
/// The drinking waypoints' and the scripts' markers', with their lists (WriteMarkers.cpp)
void RegisterMarkerWriters(ClassWriters& writers);
/// The standard and the workshops' building sites' (WriteBuildingSites.cpp)
void RegisterBuildingSiteWriters(ClassWriters& writers);
/// The towns' artifacts' (WriteTownArtifacts.cpp)
void RegisterTownArtifactWriters(ClassWriters& writers);
/// A town's artifacts, the newest to join first, each by its number
[[nodiscard]] std::vector<gamesave::ObjectNumber> TownArtifactsOf(WriteContext& context, entt::entity town);
/// The pots' and the piles': the plain pot, the food and wood piles, the magic food and the magic wood (WritePots.cpp)
void RegisterPotWriters(ClassWriters& writers);
/// The fireflies' list, from how many there are (WriteGameLists.cpp): empty, or as the load kept it, while there are
/// none; their records have no writer yet, so a save with any refuses
/// What the fireflies' writer writes: the fireflies, the list's head first, the reward table's running sums and
/// weights, and whether the next nightfall tops them up
struct FireFlyWorld
{
	std::vector<SavedFireFly> fireFlies;
	std::vector<float> sums;
	std::vector<float> probabilities;
	bool maySpawn {true};
};
/// Each firefly as the original's record of one, keyed by its creation number, and the game's list of them, head first
void RegisterFireFlyWriters(ClassWriters& writers, std::function<FireFlyWorld()> world);
/// A player's statistics after its player, in the game's order, by the player's number
using StatsBytes = std::function<std::vector<uint8_t>(size_t player)>;
/// Every player's game statistics, from its statistics (WriteGameStats.cpp); a loaded player's as the load kept them
void RegisterGameStatsWriters(ClassWriters& writers, StatsBytes statsOf);
/// The land's features, animated statics and scripts' highlights (WriteLandFeatures.cpp)
void RegisterLandFeatureWriters(ClassWriters& writers);
/// The fish farms' (WriteFishFarms.cpp)
void RegisterFishFarmWriters(ClassWriters& writers);
/// The street lanterns', with whether it is dark enough for them to be heard (WriteStreetLanterns.cpp)
void RegisterStreetLanternWriters(ClassWriters& writers, std::function<bool()> soundsOn);
/// The music attached to things (WriteThingMusic.cpp): a source of the thing's music class, the infos `infos` gives
/// (the audio's list, head first) by their keys, each with its type, its thing, whether it is enabled, finished and
/// started, whether it plays at a place of its own and that place's map coordinates; the game object's writer lists
/// them in the audio's part of its tail
void RegisterThingMusicWriters(ClassWriters& writers, std::function<std::vector<audio::ThingMusicInfo>()> infos);

/// What a load does with the loaded objects that no entity stands for and no maker of saved_world_load::Apply makes:
/// each area keeps them for their classes' writers and puts back what openblack keeps of them, one line each
void KeepLoadedSources(const gamesave::WorldRead& world, LoadedObjects& loaded);

/// Every area's class writers
[[nodiscard]] ClassWriters GameWriters();

/// The game object's writer (WriteGame.cpp): the record its load kept, or a new one, with the values `values` gives
/// (saved_game_save::CurrentValues in the game) put where the save writes them; a new game's players from their
/// entities; each global list by its list writer, else as the load kept it (a new game needs every list's writer)
void RegisterGameObjectWriter(ClassWriters& writers, std::function<saved_game_save::GameValues()> values);

/// The scripts' record of the last thing hit and of what hit it that a loaded world's game object names in its script
/// part, each the entity the load made of its object: entt::null for none, or for an object the load did not make
[[nodiscard]] std::array<entt::entity, 2> LoadedScriptHits(const Registry& registry, const gamesave::WorldRead& world,
                                                           const LoadedObjects& loaded);

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
