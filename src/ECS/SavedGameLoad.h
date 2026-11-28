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
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <GameTailRecords.h>
#include <SaveParts.h>
#include <entt/entity/entity.hpp>

#include "Camera/CameraZones.h"
#include "Common/GameRandom.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/CreatureCurse.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "Help/SavedHelp.h"
#include "LandBalance.h"

namespace openblack::gamesave
{
struct WorldRead;
struct ChallengePart;
}

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{
class LandBalanceSystemInterface;
}

namespace openblack
{
class TempleInteriorInterface;
}

// What a load of a saved game sets from the game object's record, before any object of the world: the land's file,
// the turn, the seeds, the render clock, the influence multipliers and the time of day. A pure reading of the world
// file, so it can be checked on the game's own saves without the game. See
// docs/wiki/engine/saved-games.md#what-a-load-does.
namespace openblack::ecs::saved_game_load
{

/// Where the game object's record keeps each value: in the game's data (`GameHead::data`), the bytes after it
/// (`GameHead::bytes`) and the ten words after the global lists (`GameHead::words`)
inline constexpr size_t k_SyncedSeed = 0;
inline constexpr size_t k_LocalSeed = 1;
inline constexpr size_t k_GameTurn = 2;
inline constexpr size_t k_RealGameTurn = 3;
inline constexpr size_t k_CreatedObjects = 4;
inline constexpr size_t k_FrameCount = 5;
inline constexpr size_t k_PlayerIndex = 0;
/// The local player's creature's known leashes, after the local player (saved_game_save::LeashByte)
inline constexpr size_t k_LeashesByte = 1;
inline constexpr size_t k_RenderTime = 1;
inline constexpr size_t k_RenderTimeIncrement = 2;
inline constexpr size_t k_PlayerInfluence = 3;
inline constexpr size_t k_TownInfluence = 4;
inline constexpr size_t k_VisualTime = 5;
/// The cycle's three values: the day's length, the change and the night, as the game keeps them
inline constexpr size_t k_FirstCycleValue = 6;
inline constexpr size_t k_VisualTimeScale = 9;
/// In the five words of the game object's tail after the scripts' reboot (`GameTail::words`): the skirmish, the game
/// over for the local player, a word, the turn of the last autosave and the turn the last quick save was asked for
inline constexpr size_t k_TailSkirmish = 0;
inline constexpr size_t k_TailGameOver = 1;
inline constexpr size_t k_TailLastAutoSaveTurn = 3;
inline constexpr size_t k_TailLastQuickSaveTurn = 4;
/// In the script's eight words of the tail (`GameTail::scriptSecondWords`): the task that has the camera
/// In the script's eight words of the tail: whether the leashes are drawn, the unused camera flag, whether the
/// highlights are drawn and whether the creature sounds play, the first four
inline constexpr size_t k_TailScriptDrawLeash = 0;
inline constexpr size_t k_TailScriptUnusedCameraFlag = 1;
inline constexpr size_t k_TailScriptDrawHighlight = 2;
inline constexpr size_t k_TailScriptPlayCreatureSounds = 3;
inline constexpr size_t k_TailScriptCameraOwner = 6;
/// In the script's seven last words of the tail (`GameTail::scriptLastWords`): whether the citadel interacts, the third,
/// and the interface interaction level, the fifth
inline constexpr size_t k_TailScriptCitadelInteract = 2;
inline constexpr size_t k_TailScriptInterfaceLevel = 4;
/// In the help system's last seventeen words (`GameTail::helpLastWords`): the wide screen, the task that owns it and the
/// bars' slide timer (a float, in milliseconds)
inline constexpr size_t k_TailHelpWideScreen = 5;
inline constexpr size_t k_TailHelpWideScreenOwner = 6;
inline constexpr size_t k_TailHelpWideScreenTimer = 7;
/// Then the help level, how the text is drawn, the creature's help, the tooltips' level, the immersion level and the
/// help switch, the ninth to fourteenth
inline constexpr size_t k_TailHelpLevel = 8;
inline constexpr size_t k_TailHelpTextDraw = 9;
inline constexpr size_t k_TailHelpCreatureHelp = 10;
inline constexpr size_t k_TailHelpTooltipLevel = 11;
inline constexpr size_t k_TailHelpImmersionLevel = 12;
inline constexpr size_t k_TailHelpOn = 13;
/// The help system's four counted lists (`GameTail::helpLists`): the message sets' conditions, the turn each set was
/// last sent, the categories' turns and the count each set was sent, of these sizes
inline constexpr size_t k_TailHelpConditions = 0;
inline constexpr size_t k_TailHelpSentTurns = 1;
inline constexpr size_t k_TailHelpCategoryTurns = 2;
inline constexpr size_t k_TailHelpSentCounts = 3;
inline constexpr std::array<size_t, 4> k_TailHelpListSizes {18, 0x98, 9, 0x98};
/// In the help system's two words after its lists (`GameTail::helpWords`): the last message set and its turn
inline constexpr size_t k_TailHelpLastSet = 0;
inline constexpr size_t k_TailHelpMessageTurn = 1;
/// In the help's three words after its field of view (`GameTail::helpSecondWords`): whether the field of view is locked,
/// then whether the newest text waits for a click and whether it takes none
inline constexpr size_t k_TailHelpClickToContinue = 1;
inline constexpr size_t k_TailHelpNoSkip = 2;
/// In the three words after the saved texts shown (`GameTail::helpInts`): the click waits of the dialogue the temple
/// keeps, then whether it keeps one
inline constexpr size_t k_TailHelpKeptClickToContinue = 0;
inline constexpr size_t k_TailHelpKeptNoSkip = 1;
inline constexpr size_t k_TailHelpTextKept = 2;
/// In the help's two words before its banter count (`GameTail::helpFourthWords`): the task that has the dialogue, then
/// the last control's number
inline constexpr size_t k_TailHelpDialogueOwner = 0;
/// The texts shown, the saved texts shown and the history's entries, each counted, of these sizes
inline constexpr size_t k_TailHelpTextsShown = 6;
inline constexpr size_t k_TailHelpHistoryEntries = 1024;
/// In the four words of the tail before the snow (`GameTail::cameraWords`): the camera help's three (its features, the
/// auto-pitch angle and height), then whether the temple's creature room shows the player's creature
inline constexpr size_t k_TailCameraHelpFeatures = 0;
inline constexpr size_t k_TailCameraHelpAutoPitchAngle = 1;
inline constexpr size_t k_TailCameraHelpAutoPitchHeight = 2;
inline constexpr size_t k_TailCreatureInTemple = 3;
/// The camera's exclusions part of the tail, as the game's exclusion save writes it: its version (1), then in
/// `GameTail::exclusionValues` whether the exclusions are kept to, whether the fence is, whether the camera's height and
/// its height above the land are limited, and those two limits (floats); then the fence's points and the exclusions
/// saved with the game, each a record of `camera_zones::k_ExclusionRecordSize` bytes
inline constexpr uint32_t k_TailExclusionVersion = 1;
inline constexpr size_t k_TailExclusionsOn = 0;
inline constexpr size_t k_TailFenceOn = 1;
inline constexpr size_t k_TailUseMaxAltitude = 2;
inline constexpr size_t k_TailUseHeightAboveLand = 3;
inline constexpr size_t k_TailMaxAltitude = 4;
inline constexpr size_t k_TailHeightAboveLand = 5;

/// The record's 12 bytes as the three values, each a little-endian float in that order
[[nodiscard]] CreatureCurse CreatureCurseOf(const std::array<uint8_t, 12>& bytes);
/// The three values as the record's 12 bytes
[[nodiscard]] std::array<uint8_t, 12> CreatureCurseBytes(const CreatureCurse& curse);

/// The game object's values a load puts back first
struct LoadedGameState
{
	/// The land's file as the game opened it
	std::string landscape;
	uint32_t landNumber {0};
	uint32_t gameFlags {0};
	/// The game's turn and the turn of the real clock
	uint32_t gameTurn {0};
	uint32_t realGameTurn {0};
	/// The objects made so far in the game: the creation number the next object takes
	uint32_t createdObjects {0};
	/// The passes of the game's loop since the map's last clear
	uint32_t frameCount {0};
	/// The synchronised seed and the local one
	GameRandomSeeds seeds;
	/// The player of this machine
	uint8_t playerIndex {0};
	uint32_t renderTime {0};
	uint32_t renderTimeIncrement {0};
	float playerInfluenceMultiplier {0.0f};
	float townInfluenceMultiplier {0.0f};
	/// The time of day, its cycle's three values and its speed
	float visualTime {0.0f};
	std::array<float, 3> visualTimeCycle {};
	float visualTimeScale {0.0f};
	/// The turn of the last autosave: the loaded turn
	uint32_t lastAutoSaveTurn {0};
	/// The turn the last quick save was asked for, as saved
	uint32_t lastQuickSaveTurn {0};
	/// The clock starts again at the turn times the turn's length
	uint32_t clockMs {0};
	/// Whether the temple's creature room shows the player's creature, as a script last said (any value but 0: it does)
	uint32_t creatureInTemple {1};
	/// The snow lying: the depth of every cell of its grid, the change over every land block, the melting clock and
	/// band
	std::vector<float> snowDepths;
	std::vector<float> snowBlockChanges;
	float snowMeltClock {0.0f};
	int32_t snowMeltBand {0};
	/// The did-you-know texts read, by category, and the count each list holds
	std::array<std::vector<uint32_t>, 5> didYouKnowTexts {};
	std::array<uint32_t, 5> didYouKnowCounts {};
	/// The game is a skirmish, so a loaded one carries on as one
	bool skirmish {false};
	/// The game is over for the local player: its script had started, so a loaded one stays over
	bool gameOver {false};
	/// The script task that has the camera (0: none)
	uint32_t cameraOwner {0};
	/// The script's camera switches: the leashes drawn, the unused camera flag and the highlights drawn
	uint32_t drawLeash {1};
	uint32_t unusedCameraFlag {0};
	uint32_t drawHighlight {1};
	/// The help system's wide screen, the script task that owns it (0: none, or the game's) and the bars' timer
	uint32_t wideScreen {0};
	uint32_t wideScreenOwner {0};
	float wideScreenTimer {0.0f};
	/// The help system's own state openblack models: the message sets, the categories' turns, the help level and switch
	help::SavedHelp helpState;
	/// The creature help switch as saved (0: off, any other value on; on without a profile)
	uint32_t creatureHelp {1};
	/// The land's balance: the multipliers the land's script set, as saved
	std::array<float, land_balance::k_Count> landBalance {};
	/// Whether the temple's rooms were open (any value but 0: they were), and the main room's five map buttons (any
	/// value but 0: on), in the order the game saves them
	uint32_t roomsAvailable {0};
	std::array<uint32_t, 5> mapButtons {};
	/// The challenge room's records, the first started first, each thing they hold as its place in the scripts' object
	/// table
	std::vector<components::ChallengeRecord> challenges;
	/// The script's interface interaction level (SET_INTERFACE_INTERACTION's)
	int32_t interfaceLevel {0};
	/// The camera's zones as the scripts last set them: the switches, the limits, the fence and the exclusions saved
	camera_zones::Zones cameraZones;
	/// The camera help's features and auto-pitch angle and height
	int32_t cameraHelpFeatures {0};
	float autoPitchAngle {0.0f};
	float autoPitchHeight {0.0f};
	/// The bookmarks as the game's bookmark save wrote them, and its three words
	std::array<gamesave::BookmarkPart, gamesave::k_BookmarkCount> bookmarks {};
	std::array<uint32_t, 3> bookmarkWords {};
	/// The script's curse of the creature, the record's last values
	CreatureCurse creatureCurse;
};

/// What the save keeps of the curse from the local player's creature: its height (its drawn size as a height), its
/// strength and its alignment; three 0s with no creature
[[nodiscard]] CreatureCurse CreatureCurseToSave(const Registry& registry, std::optional<entt::entity> creature);

/// The load's resolve of the curse, once the creature's mind file and the scripts are read: while the curse's script
/// runs and the kept height is above 0, the local player's creature takes the kept height (as SET_PROPERTY HEIGHT sets
/// it), strength and alignment (within -1..1). False, and nothing changed, otherwise or with no creature
bool PutBackCreatureCurse(Registry& registry, std::optional<entt::entity> creature, const CreatureCurse& curse,
                          bool curseRunning);

/// The camera's zones a tail holds: its switches and limits, its fence, and its exclusions when their records are of the
/// game's size (of another size the game reads and drops them all)
[[nodiscard]] camera_zones::Zones CameraZonesOf(const gamesave::GameTail& tail);

/// The help system's state a tail holds, of the fields openblack models; a list shorter than the game's is read as far
/// as it goes, the rest left as a new help system has it
[[nodiscard]] help::SavedHelp HelpOf(const gamesave::GameTail& tail);

/// The values, or none when the game object's record was not read
[[nodiscard]] std::optional<LoadedGameState> StateOf(const gamesave::WorldRead& world);

/// Whether a saved game's camera help words are the three zeros openblack wrote before its saves wrote them (no feature,
/// no self-tilt angle and no height), which no game of the original's holds: its start is every feature but the
/// self-tilt, and the scripts' levels all leave some
[[nodiscard]] bool CameraHelpUnwritten(const std::array<uint32_t, 4>& cameraWords);
/// The camera help words of a game's start (camera_help's start values), for a repaired save
void PutStartCameraHelp(std::array<uint32_t, 4>& cameraWords);

/// The player of each interface status the file holds, in the order their reads ended (the order the game resolves
/// them in, each starting its guidance again)
[[nodiscard]] std::vector<uint32_t> InterfaceStatusPlayers(const gamesave::WorldRead& world);

/// Puts the values back as the game object's load does, in its order: the seeds and the objects' creation counter, the
/// land's number, the influence multipliers, the time of day forced (with the sky), its speed and its cycle, whether the
/// temple's creature room shows the creature, the snow lying, the did-you-know texts read, the turns of the last autosave and
/// quick save, whether the game is a skirmish and whether it is over, the script task that has the camera, the help
/// system's message sets, categories' turns, help level and switch, and the wide screen with its owner and the bars' timer as
/// they were, with nothing else of a wide screen's change, the land's balance, whether the temple's rooms are open and its map
/// buttons; then, as the load's end does, the turn and the clock started again at the turn's time. Through the Locator's
/// services
void Apply(const LoadedGameState& state);

/// The temple's rooms open or closed, and the main room's map buttons each on or off, as the saved game had them
void PutBackTemple(TempleInteriorInterface& temple, const LoadedGameState& state);

/// The things the saved bookmarks follow, each the entity the load made of its object (none for none, or one not made)
using BookmarkThings = std::array<std::optional<entt::entity>, gamesave::k_BookmarkCount>;
[[nodiscard]] BookmarkThings BookmarkThingsOf(const LoadedGameState& state,
                                              const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf);
/// The bookmarks as the game's bookmark load reads them: each bookmark's flags, its thing, its map position and its camera
/// offset (the second the load reads), then the one followed (none for -1 or any other not a bookmark), the current one and
/// whether they are on
[[nodiscard]] systems::SavedBookmarks SavedBookmarksOf(const LoadedGameState& state, const BookmarkThings& things);

/// A challenge's record as the challenge room's load reads it, its place among the records given: its challenge (the
/// low 16 bits the save keeps), its reminder script (to its first null), its title, success and alignment, where the
/// camera was and looked, the turn it started (read twice, the second read kept), the turn it was done on (only for a
/// record whose success is exactly 1), its reminder's values with their types (as many as its count, each thing as its
/// place in the scripts' object table) and whether it is a quest
[[nodiscard]] components::ChallengeRecord ChallengeRecordOf(const gamesave::ChallengePart& part, uint32_t order);

/// The challenge room's records back, after any the game holds, in their saved order, each thing they hold the loaded
/// object of its place (`places`: the scripts' value of each place, place 1 first). They make no new script reference:
/// the loaded table's places already count the records' own
void PutBackChallengeRecords(Registry& registry, const LoadedGameState& state, std::span<const uint32_t> places);

/// Makes the game a skirmish if the saved one was, and none if it wasn't, so a loaded skirmish carries on as one
void PutBackSkirmish(Registry& registry, const LoadedGameState& state);

/// Makes the game over if the saved one was, and not over if it wasn't: a game saved after its game over started stays
/// over, so its saves, quick loads and the save room's slots read it again
void PutBackGameOver(Registry& registry, const LoadedGameState& state);

/// Sets every land balance multiplier to its saved value, whatever the land's script set: the lost-town scale is not
/// saved, and stays as the map's clear left it
void PutBackLandBalance(systems::LandBalanceSystemInterface& balance, const LoadedGameState& state);

/// (openblack, for the debug bar's land balance repair) Whether a save holds its land balance as never written: all of
/// its 32 bytes 0, as openblack wrote it before its saves wrote the land balance. A save of the original game always
/// holds the eight multipliers, each 1 unless the land's script set it
[[nodiscard]] bool LandBalanceUnwritten(std::span<const uint8_t> saved);

/// (openblack, for the debug bar's land balance repair) The land balance a land's script leaves, as the land's start
/// sets it: every multiplier 1 from the map's clear, then each SET_GLOBAL_LAND_BALANCE line of the script in turn sets
/// the multiplier of its number (0 to 7; any other number sets nothing) to its fraction. The numbers are read as the
/// land script's reader reads them, as written on the line (the shipped land scripts write them so)
[[nodiscard]] std::array<float, land_balance::k_Count> LandBalanceOfScript(std::string_view script);

/// (openblack, for the debug bar's land balance repair) The name of the land's script in the scripts folder, from the
/// saved land's file: its name with ".txt" for its extension (".\DATA\LANDSCAPE\Land3.lnd" is "Land3.txt")
[[nodiscard]] std::string LandScriptNameOf(std::string_view landscape);

} // namespace openblack::ecs::saved_game_load
