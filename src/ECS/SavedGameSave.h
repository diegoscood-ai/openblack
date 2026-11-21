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
#include <optional>
#include <vector>

#include <SavedRecords.h>

#include "Camera/CameraZones.h"
#include "Common/GameRandom.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "LandBalance.h"

namespace openblack
{
class TempleInteriorInterface;
}

// What a save writes of the game object's own values: the inverse of saved_game_load. The values openblack keeps
// are taken from its services and put into the game object's record where the game's save writes them; every other
// value stays as the record has it. See docs/wiki/engine/saved-games.md#writing-the-world-file.
namespace openblack::ecs::systems
{
class LandBalanceSystemInterface;
}

namespace openblack::ecs::saved_game_save
{

/// The camera's exclusions part of a tail, as the game's exclusion save writes it: its version, the zones' switches and
/// limits, the fence's points, then only the exclusions saved with the game, with their records' size
void PutCameraZones(gamesave::GameTail& tail, const camera_zones::Zones& zones);

/// The game object's values openblack keeps, each group none when its service is not there (the record's then stays)
struct GameValues
{
	std::optional<uint32_t> landNumber;
	std::optional<GameRandomSeeds> seeds;
	std::optional<uint32_t> gameTurn;
	/// The creation number the next object takes
	std::optional<uint32_t> createdObjects;
	/// The passes of the game's loop since the map's last clear
	std::optional<uint32_t> frameCount;
	/// The player's and the towns' influence multipliers
	std::optional<std::array<float, 2>> influenceMultipliers;
	/// The time of day, its cycle as the game keeps it (the day's length, the change, the night) and its speed
	struct Clock
	{
		float visualTime {0.0f};
		std::array<float, 3> cycle {};
		float scale {0.0f};
	};
	std::optional<Clock> clock;
	/// The script's interface interaction level
	std::optional<int32_t> interfaceLevel;
	/// The camera's zones as the scripts last set them: the switches, the limits, the fence and the exclusions
	std::optional<camera_zones::Zones> cameraZones;
	/// The camera help: the features the player may use and the auto-pitch angle and height
	struct CameraHelp
	{
		int32_t features {0};
		float autoPitchAngle {0.0f};
		float autoPitchHeight {0.0f};
	};
	std::optional<CameraHelp> cameraHelp;
	/// Whether the temple's creature room shows the player's creature
	std::optional<bool> creatureInTemple;
	/// The leashes the local player's creature knows, as the game's leash byte packs them (LeashByte); 0 with no creature
	std::optional<uint8_t> leashes;
	std::optional<systems::SavedSnow> snow;
	/// The bookmarks, the one followed, the current one and whether they are on
	std::optional<systems::SavedBookmarks> bookmarks;
	/// The did-you-know texts read, by category
	std::optional<std::array<std::vector<uint32_t>, 5>> didYouKnow;
	/// The turns of the last autosave and of the last quick save asked for
	std::optional<uint32_t> lastAutoSaveTurn;
	std::optional<uint32_t> lastQuickSaveTurn;
	/// Whether the game is a skirmish
	std::optional<bool> skirmish;
	/// Whether the game is over for the local player (its script has started)
	std::optional<bool> gameOver;
	/// The script task that has the camera (0: none)
	std::optional<uint32_t> cameraOwner;
	/// The script's camera switches: whether the leashes are drawn, the unused camera flag and whether the highlights
	/// are drawn, in the script's words of the tail
	struct ScriptSwitches
	{
		int32_t drawLeash {1};
		int32_t unusedCameraFlag {0};
		int32_t drawHighlight {1};
	};
	std::optional<ScriptSwitches> scriptSwitches;
	/// The help system's wide screen and the script task that owns it
	struct WideScreen
	{
		int32_t on {0};
		uint32_t owner {0};
	};
	std::optional<WideScreen> wideScreen;
	/// The cinema bars' slide timer, in milliseconds
	std::optional<float> wideScreenTimer;
	/// The audio's script music, its type and whether it plays, the town the music last played for (its entity's id;
	/// none for none) and the alignment at the camera the music reads
	struct Music
	{
		uint32_t scriptType {0};
		uint32_t scriptPlaying {0};
		std::optional<uint32_t> town;
		float alignment {0.0f};
	};
	std::optional<Music> music;
	/// The scripts' record of the last thing hit in the physics and of what hit it (their entities' ids; none for none)
	struct Hits
	{
		std::optional<uint32_t> hit;
		std::optional<uint32_t> hitter;
	};
	std::optional<Hits> hits;
	/// The land's balance: the multipliers the land's script set
	std::optional<std::array<float, land_balance::k_Count>> landBalance;
	/// The temple: whether its rooms are open (the main room once drawn opens the others for the rest of the game), and
	/// the main room's map buttons, each on or off, in the order the game saves them
	struct Temple
	{
		bool roomsOpened {false};
		std::array<bool, 5> mapButtons {};
	};
	std::optional<Temple> temple;
};

/// Every land balance multiplier, as the balance holds them now
[[nodiscard]] std::array<float, land_balance::k_Count> LandBalanceOf(const systems::LandBalanceSystemInterface& balance);

/// The values, from the Locator's services
[[nodiscard]] GameValues CurrentValues();
/// A bookmark as the game's bookmark save writes it: its flags, its camera offset, the thing it follows (`target`, its
/// object's number), its map position (its 12 bytes) and its camera offset again
[[nodiscard]] gamesave::BookmarkPart BookmarkPartOf(const systems::SavedBookmark& bookmark, gamesave::ObjectNumber target);
/// The bookmark save's three words: the bookmark followed (-1 for none), the current one and whether they are on
[[nodiscard]] std::array<uint32_t, 3> BookmarkWordsOf(const systems::SavedBookmarks& bookmarks);
/// The temple's values as it holds them now
[[nodiscard]] GameValues::Temple TempleOf(const TempleInteriorInterface& temple);

/// A thing's place in the scripts' object table, from the scripts' value naming it (0 for none)
using PlaceOfThing = std::function<uint32_t(uint32_t value)>;
/// A challenge's record as the challenge room's save writes it: its challenge's low 16 bits, its reminder script in 64
/// bytes (nulls after its end), its title, success and alignment, where the camera was and looked (0 for a record
/// never given them), the turn it started twice, the turn it was done on (0 before), its count of values, the values
/// (each thing by its place) and their types, each with the count again, and whether it is a quest
[[nodiscard]] gamesave::ChallengePart ChallengePartOf(const components::ChallengeRecord& record, const PlaceOfThing& placeOf);
/// The challenge room's records, the first started first
[[nodiscard]] std::vector<gamesave::ChallengePart> ChallengePartsOf(const Registry& registry, const PlaceOfThing& placeOf);
/// The values a load puts back as it read them, so that a save right after the load writes the record it read: the
/// turn of the last autosave is left out, as the load sets it to the loaded turn rather than putting back the saved one
[[nodiscard]] GameValues ValuesOf(const saved_game_load::LoadedGameState& state);

/// The game object's record with the values put where the game's save writes them, over `base` (the record a load
/// kept, or a new one): the land's number; in the game's data both seeds, the turn, the creation counter and the frame
/// count; in the words after the lists the influence multipliers, the visual time, its cycle and its speed; in the tail
/// whether the temple's creature room shows the creature, the snow lying (a grid of another size than the record's leaves it),
/// each category's texts read with their count, the turns of the last autosave and quick save, whether the game is a skirmish
/// and whether it is over, the script task that has the camera, and the wide screen, its owner and the bars' timer; and in
/// the audio's part of the tail the script music's type and whether it plays, and the alignment (the town it names is the
/// game object's writer's, by its number); then whether the temple's rooms are open and the main room's map buttons;
/// and the land's balance after the bookmarks
[[nodiscard]] gamesave::GameHead HeadOf(const GameValues& values, gamesave::GameHead base);

/// The head a game openblack started writes from where no loaded game's record is kept: zeros, but for the script's
/// values its state starts with in the original (the leashes and the highlights drawn, the creature sounds played and
/// the citadel interacting), which a save of such a game otherwise wrote as off
[[nodiscard]] gamesave::GameHead NewGameHead();

/// The game's leash byte: the evil leash known 1, the rope 2, the good 4, as the original packs the local player's
/// creature's three leashes, and a load reads them back
[[nodiscard]] constexpr uint8_t LeashByte(bool evil, bool rope, bool good)
{
	return static_cast<uint8_t>((((good ? 1U : 0U) << 1U) + (rope ? 1U : 0U)) << 1U) + (evil ? 1U : 0U);
}

} // namespace openblack::ecs::saved_game_save
