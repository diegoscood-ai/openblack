/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <SavedRecords.h>
#include <entt/entity/entity.hpp>

#include "ECS/Components/Alignment.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "Enums.h"

namespace openblack::gamesave
{
struct WorldRead;
}

namespace openblack::ecs
{
class Registry;
}

// The players' records of a saved game: what a load puts back of each player that openblack models, and what a save
// writes of it, in the game's order of the record. The pure parts (a record's values, a record made from values, a new
// game's record) are apart from the parts that read and set the game's services. See
// docs/wiki/engine/saved-games.md#the-players-records.
namespace openblack::ecs::saved_players
{

/// The row of the players' table every player's record names: the same in every save
inline constexpr uint32_t k_PlayerInfo = 1743;
/// The first word of a player's alignment as the game writes it: a value of the game's code, the same in every save
inline constexpr uint32_t k_AlignmentClassWord = 0x8CC7F0;
/// The text the neutral player is named with
inline constexpr uint32_t k_NeutralNameText = 0x1A6C;
/// The players' types as the game keeps them: none, the human, a player the computer plays, the neutral player
inline constexpr uint32_t k_NeutralPlayerType = 3;
/// The characters of a player's name, its zeros included
inline constexpr size_t k_NameLength = 32;
/// The interfaces a player's record lists
inline constexpr size_t k_Interfaces = 18;
/// The players the damage and the alliances are kept for
inline constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

/// What openblack models of a player's record
struct PlayerValues
{
	/// The alignment and the change gathered this turn
	components::Alignment alignment;
	std::array<float, components::PlayerMagic::k_Tribes> tribalPower {};
	std::array<float, components::PlayerMagic::k_Tribes> maxTribalPower {};
	uint32_t type {0};
	/// The "all magic" cheat
	bool allMagic {false};
	uint32_t totalPopulation {0};
	/// How many holders enable each magic type, and whether the player ever had it
	std::array<int32_t, components::PlayerMagic::k_MagicTypes> magicEnabled {};
	std::array<bool, components::PlayerMagic::k_MagicTypes> everEnabled {};
	float influencePower {0.0f};
	float averageInfluencePower {0.0f};
	/// The harm each player's effects did this player's things, and the strength of its alliance with each player
	std::array<float, k_Players> damageFromPlayer {};
	std::array<float, k_Players> allies {};
};

/// The values of a player's record that openblack models (a list shorter than openblack's leaves the rest 0)
[[nodiscard]] PlayerValues ValuesOf(const gamesave::PlayerRecord& record);
/// The record with the values put where the game's save writes them, over `base` (the record a load kept, or a new
/// one); a list keeps the base's count, and every other value stays as the base has it
[[nodiscard]] gamesave::PlayerRecord RecordOf(const PlayerValues& values, gamesave::PlayerRecord base);
/// A new game's record of the player, as the game sets its players up before any land: its number, type and name,
/// the start values of its alignment, tribal powers and magic, and the counts the game writes for each list. Its
/// references are none; the writer names them
[[nodiscard]] gamesave::PlayerRecord NewRecord(PlayerNames player, uint32_t type, std::u16string_view name);
/// The name a new game gives the player: the local player's profile name, the neutral player's text, and
/// "Player[n]" for the others, n from 1
[[nodiscard]] std::u16string NewName(PlayerNames player, std::u16string_view profileName, std::u16string_view neutralName);
/// The type a new game gives the player before any land: the human for the local player, the neutral player's own,
/// none for the others
[[nodiscard]] uint32_t NewType(PlayerNames player, PlayerNames localPlayer);

/// A new game's record of the player, from the game: NewRecord with NewType of the local player and NewName of the
/// current profile and the neutral player's text
[[nodiscard]] gamesave::PlayerRecord NewGameRecord(PlayerNames player);

/// The objects a player's record names that openblack makes as entities
struct PlayerObjects
{
	/// The player's citadel (the citadel's first heart, which stands for it)
	entt::entity citadel {entt::null};
	/// The player's creature
	entt::entity creature {entt::null};
	/// The player's towns, in the order its list holds them
	std::vector<entt::entity> towns;
};
/// The game's: the citadel whose temple is the player's and not marked for deletion, the creature the player's entity
/// keeps as theirs, and the towns the player owns in the order they joined its list
[[nodiscard]] PlayerObjects ObjectsOf(const Registry& registry, PlayerNames player);

/// The player's values as the game holds them now, from the Locator's services; none without the player service
[[nodiscard]] std::optional<PlayerValues> CurrentValues(PlayerNames player);
/// Puts the values back into the game, where it keeps them: on the player's entity, else in the player service
void Apply(PlayerNames player, const PlayerValues& values);

/// A player record of a world file: its number in the file and the record
struct ListedPlayer
{
	gamesave::ObjectNumber number {0};
	const gamesave::PlayerRecord* record {nullptr};
};
/// The player records the game object names, in its order, each of a player openblack has
[[nodiscard]] std::vector<ListedPlayer> ListPlayers(const gamesave::WorldRead& world);

/// A load's players: each player record the game object names put back (Apply), and listed in `loaded` by its number
/// in the file, as the players' source object of its number, so that the next save writes it from the game. How many
/// were put back
size_t Load(const gamesave::WorldRead& world, saved_world_write::LoadedObjects* loaded);

} // namespace openblack::ecs::saved_players
