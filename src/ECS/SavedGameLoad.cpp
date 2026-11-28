/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedGameLoad.h"

#include <algorithm>
#include <bit>
#include <utility>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/Game/AudioSystem.h"
#include "Camera/BookmarkSaveReadout.h"
#include "Camera/CameraHelp.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/PlayerCreature.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedScriptObjects.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/LandBalanceSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "InfoConstants.h"
#include "LHScriptX/LineReader.h"
#include "Locator.h"
#include "Temple/TempleHelpScripts.h"

using namespace openblack::ecs;

std::vector<uint32_t> saved_game_load::InterfaceStatusPlayers(const gamesave::WorldRead& world)
{
	std::vector<uint32_t> players;
	for (const auto number : world.loadOrder)
	{
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			continue;
		}
		if (const auto* status = std::get_if<gamesave::InterfaceStatusRecord>(&*world.objects[number - 1]); status != nullptr)
		{
			players.push_back(status->playerNumber);
		}
	}
	return players;
}

openblack::camera_zones::Zones saved_game_load::CameraZonesOf(const gamesave::GameTail& tail)
{
	const auto& values = tail.exclusionValues;
	camera_zones::Zones zones {
	    .exclusionsOn = values[k_TailExclusionsOn] != 0,
	    .fenceOn = values[k_TailFenceOn] != 0,
	    .useMaxAltitude = values[k_TailUseMaxAltitude] != 0,
	    .useHeightAboveLand = values[k_TailUseHeightAboveLand] != 0,
	    .maxAltitude = std::bit_cast<float>(values[k_TailMaxAltitude]),
	    .heightAboveLand = std::bit_cast<float>(values[k_TailHeightAboveLand]),
	};
	for (const auto& point : tail.forceFieldPoints.values)
	{
		zones.fence.emplace_back(point[0], point[1], point[2]);
	}
	if (tail.exclusionNameSize != camera_zones::k_ExclusionRecordSize)
	{
		return zones;
	}
	for (const auto& record : tail.exclusions)
	{
		if (record.values.size() == camera_zones::k_ExclusionRecordSize)
		{
			zones.exclusions.push_back(camera_zones::ExclusionOf(
			    std::span<const uint8_t, camera_zones::k_ExclusionRecordSize>(record.values.data(), record.values.size())));
		}
	}
	return zones;
}

namespace
{
// A counted list read into its place, as far as both go
template <size_t N>
void ReadHelpList(const openblack::gamesave::Counted<uint32_t>& list, std::array<uint32_t, N>& into)
{
	const auto count = std::min(list.values.size(), into.size());
	std::copy_n(list.values.begin(), count, into.begin());
}
} // namespace

openblack::help::SavedHelp saved_game_load::HelpOf(const gamesave::GameTail& tail)
{
	help::SavedHelp saved;
	ReadHelpList(tail.helpLists[k_TailHelpConditions], saved.conditions);
	ReadHelpList(tail.helpLists[k_TailHelpSentTurns], saved.sentTurn);
	ReadHelpList(tail.helpLists[k_TailHelpCategoryTurns], saved.categoryTurns);
	ReadHelpList(tail.helpLists[k_TailHelpSentCounts], saved.sentCount);
	saved.lastSet = tail.helpWords[k_TailHelpLastSet];
	saved.messageTurn = tail.helpWords[k_TailHelpMessageTurn];
	// the banter sets sent, the half after the dialogue's task and its last control
	saved.banterCount = tail.helpThirdHalf;
	saved.helpLevel = static_cast<int32_t>(tail.helpLastWords[k_TailHelpLevel]);
	saved.helpOn = tail.helpLastWords[k_TailHelpOn];
	// the dialogue: its task, its texts shown and its click waits; then the one the temple keeps, and whether it does
	saved.dialogue.owner = tail.helpFourthWords[k_TailHelpDialogueOwner];
	ReadHelpList(tail.helpValues, saved.dialogue.texts);
	saved.dialogue.clickToContinue = tail.helpSecondWords[k_TailHelpClickToContinue] != 0;
	saved.dialogue.noSkip = tail.helpSecondWords[k_TailHelpNoSkip] != 0;
	saved.kept.owner = tail.helpThirdWord;
	ReadHelpList(tail.helpArray, saved.kept.texts);
	saved.kept.clickToContinue = tail.helpInts[k_TailHelpKeptClickToContinue] != 0;
	saved.kept.noSkip = tail.helpInts[k_TailHelpKeptNoSkip] != 0;
	saved.textKept = tail.helpInts[k_TailHelpTextKept] != 0;
	return saved;
}

std::optional<saved_game_load::LoadedGameState> saved_game_load::StateOf(const gamesave::WorldRead& world)
{
	if (world.objects.empty() || !world.objects.front().has_value())
	{
		return std::nullopt;
	}
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.front());
	if (game == nullptr)
	{
		return std::nullopt;
	}
	LoadedGameState state {
	    .landscape = world.framing.landscape,
	    .landNumber = game->landNumber,
	    .gameFlags = game->gameFlags,
	    .gameTurn = game->data[k_GameTurn],
	    .realGameTurn = game->data[k_RealGameTurn],
	    .createdObjects = game->data[k_CreatedObjects],
	    .frameCount = game->data[k_FrameCount],
	    .seeds = {.synced = game->data[k_SyncedSeed], .local = game->data[k_LocalSeed]},
	    .playerIndex = game->bytes[k_PlayerIndex],
	    .renderTime = game->words[k_RenderTime],
	    .renderTimeIncrement = game->words[k_RenderTimeIncrement],
	    .playerInfluenceMultiplier = std::bit_cast<float>(game->words[k_PlayerInfluence]),
	    .townInfluenceMultiplier = std::bit_cast<float>(game->words[k_TownInfluence]),
	    .visualTime = std::bit_cast<float>(game->words[k_VisualTime]),
	    .visualTimeCycle = {std::bit_cast<float>(game->words[k_FirstCycleValue]),
	                        std::bit_cast<float>(game->words[k_FirstCycleValue + 1]),
	                        std::bit_cast<float>(game->words[k_FirstCycleValue + 2])},
	    .visualTimeScale = std::bit_cast<float>(game->words[k_VisualTimeScale]),
	};
	// the snow lying: its grid, its blocks' changes, then the melting clock and band, as the game writes them
	const auto floats = [](const std::vector<uint32_t>& words) {
		std::vector<float> out;
		out.reserve(words.size());
		for (const auto word : words)
		{
			out.push_back(std::bit_cast<float>(word));
		}
		return out;
	};
	// the camera's zones, read after the camera and the climate
	state.cameraZones = CameraZonesOf(game->tail);
	// the camera help's features and auto-pitch angle and height, then whether the temple's creature room shows the
	// creature, read just before the snow
	state.cameraHelpFeatures = static_cast<int32_t>(game->tail.cameraWords[k_TailCameraHelpFeatures]);
	state.autoPitchAngle = std::bit_cast<float>(game->tail.cameraWords[k_TailCameraHelpAutoPitchAngle]);
	state.autoPitchHeight = std::bit_cast<float>(game->tail.cameraWords[k_TailCameraHelpAutoPitchHeight]);
	state.creatureInTemple = game->tail.cameraWords[k_TailCreatureInTemple];
	state.creatureCurse = CreatureCurseOf(game->tail.creatureCurse);
	state.snowDepths = floats(game->tail.snow.values);
	state.snowBlockChanges = floats(game->tail.snowSecond.values);
	state.snowMeltClock = std::bit_cast<float>(game->tail.snowWords[0]);
	state.snowMeltBand = static_cast<int32_t>(game->tail.snowWords[1]);
	// the did-you-know texts read: each category's texts, then the count its list holds
	for (size_t category = 0; category < state.didYouKnowTexts.size(); ++category)
	{
		state.didYouKnowTexts.at(category) = game->tail.didYouKnow.at(category).values;
		state.didYouKnowCounts.at(category) = game->tail.didYouKnowWords.at(category);
	}
	// the load reads the last autosave's turn and sets it to the loaded turn, and the clock starts again at the turn's
	// time; the last quick save's turn is the saved one
	state.lastAutoSaveTurn = state.gameTurn;
	state.lastQuickSaveTurn = game->tail.words[k_TailLastQuickSaveTurn];
	// whether the game was a skirmish: any other value than 0 is one
	state.skirmish = game->tail.words[k_TailSkirmish] != 0;
	// whether the game was over for the local player, read next: any other value than 0 is over
	state.gameOver = game->tail.words[k_TailGameOver] != 0;
	// the script's camera owner and the help's wide screen, read back as they are
	state.cameraOwner = game->tail.scriptSecondWords[k_TailScriptCameraOwner];
	state.drawLeash = game->tail.scriptSecondWords[k_TailScriptDrawLeash];
	state.unusedCameraFlag = game->tail.scriptSecondWords[k_TailScriptUnusedCameraFlag];
	state.drawHighlight = game->tail.scriptSecondWords[k_TailScriptDrawHighlight];
	state.interfaceLevel = static_cast<int32_t>(game->tail.scriptLastWords[k_TailScriptInterfaceLevel]);
	state.wideScreen = game->tail.helpLastWords[k_TailHelpWideScreen];
	state.wideScreenOwner = game->tail.helpLastWords[k_TailHelpWideScreenOwner];
	state.wideScreenTimer = std::bit_cast<float>(game->tail.helpLastWords[k_TailHelpWideScreenTimer]);
	// and the rest of the help system's own state openblack models
	state.helpState = HelpOf(game->tail);
	state.creatureHelp = game->tail.helpLastWords[k_TailHelpCreatureHelp];
	// the bookmarks and their three words
	state.bookmarks = game->tail.bookmarks;
	state.bookmarkWords = game->tail.bookmarkWords;
	// the land's balance after the bookmarks: its multipliers' bytes as they were in play
	state.landBalance = std::bit_cast<decltype(state.landBalance)>(game->tail.landBalance);
	// after the audio, whether the temple's rooms are open, then the main room's map buttons
	state.roomsAvailable = game->tail.roomsAvailable;
	state.mapButtons = game->tail.buttonConfig;
	// then the challenge room's records, in the order they started
	for (const auto& part : game->tail.challenges.elements)
	{
		state.challenges.push_back(ChallengeRecordOf(part, static_cast<uint32_t>(state.challenges.size())));
	}
	state.clockMs = state.gameTurn * game_clock::k_MsPerTurn;
	return state;
}

void saved_game_load::PutBackSkirmish(Registry& registry, const LoadedGameState& state)
{
	registry.Context().skirmish = state.skirmish;
}

void saved_game_load::PutBackGameOver(Registry& registry, const LoadedGameState& state)
{
	registry.Context().gameOver = state.gameOver;
}

void saved_game_load::PutBackLandBalance(systems::LandBalanceSystemInterface& balance, const LoadedGameState& state)
{
	for (size_t index = 0; index < state.landBalance.size(); ++index)
	{
		balance.Set(static_cast<int>(index), state.landBalance.at(index));
	}
}

bool saved_game_load::LandBalanceUnwritten(std::span<const uint8_t> saved)
{
	return std::ranges::all_of(saved, [](uint8_t byte) { return byte == 0; });
}

std::array<float, openblack::land_balance::k_Count> saved_game_load::LandBalanceOfScript(std::string_view script)
{
	namespace reader = lhscriptx::line_reader;
	std::array<float, land_balance::k_Count> values {};
	values.fill(1.0f);
	reader::ForEachLine(script, [&values](std::string_view line, int /*number*/) {
		reader::LineCursor cursor(line);
		const auto command = cursor.Next();
		if (command.quoted || command.text != "SET_GLOBAL_LAND_BALANCE")
		{
			return true;
		}
		const auto index = cursor.Next();
		const auto value = cursor.Next();
		// a line with a missing argument sets nothing
		if (value.text.empty())
		{
			return true;
		}
		const auto number = reader::AtoL(index.text);
		if (number >= 0 && static_cast<size_t>(number) < values.size())
		{
			values.at(static_cast<size_t>(number)) = static_cast<float>(reader::AtoF(value.text));
		}
		return true;
	});
	return values;
}

std::string saved_game_load::LandScriptNameOf(std::string_view landscape)
{
	const auto slash = landscape.find_last_of(R"(\/)");
	auto name = landscape.substr(slash == std::string_view::npos ? 0 : slash + 1);
	name = name.substr(0, name.find_last_of('.'));
	return std::string(name) + ".txt";
}

openblack::ecs::components::ChallengeRecord saved_game_load::ChallengeRecordOf(const gamesave::ChallengePart& part,
                                                                               uint32_t order)
{
	const auto pointOf = [](const gamesave::Point& point) { return glm::vec3(point[0], point[1], point[2]); };
	components::ChallengeRecord record {
	    .challenge = part.half,
	    .order = order,
	    .title = part.words[0],
	    .success = std::bit_cast<float>(part.words[1]),
	    .alignment = std::bit_cast<float>(part.words[2]),
	    .position = pointOf(part.points[0]),
	    .focus = pointOf(part.points[1]),
	    .quest = part.lastWord != 0,
	    // the turn it started is read twice into one place, so the second is kept
	    .startedTurn = part.secondWords[1],
	};
	const auto& name = part.name.values;
	record.reminderScript.assign(name.begin(), std::find(name.begin(), name.end(), uint8_t {0}));
	if (challenge_records::IsDone(record))
	{
		record.doneTurn = part.secondWords[2];
	}
	const size_t count = std::min<size_t>(part.secondWords[3], part.first.values.size());
	for (size_t i = 0; i < count; ++i)
	{
		openblack::script::challenge_snapshots::Value value;
		value.value.uintVal = part.first.values[i];
		value.type = i < part.second.values.size() ? static_cast<openblack::lhvm::DataType>(part.second.values[i])
		                                           : openblack::lhvm::DataType::None;
		record.arguments.push_back(value);
	}
	return record;
}

void saved_game_load::PutBackChallengeRecords(Registry& registry, const LoadedGameState& state,
                                              std::span<const uint32_t> places)
{
	uint32_t order = 0;
	registry.Each<const components::ChallengeRecord>(
	    [&order](entt::entity, const components::ChallengeRecord& record) { order = std::max(order, record.order + 1); });
	for (const auto& saved : state.challenges)
	{
		auto record = saved;
		record.order = order++;
		for (auto& argument : record.arguments)
		{
			if (argument.type == openblack::lhvm::DataType::Object)
			{
				argument.value.uintVal = saved_script_objects::ValueOf(places, argument.value.uintVal);
			}
		}
		const auto entity = registry.Create();
		registry.Assign<components::ChallengeRecord>(entity, std::move(record));
	}
}

saved_game_load::CreatureCurse saved_game_load::CreatureCurseOf(const std::array<uint8_t, 12>& bytes)
{
	const auto word = [&bytes](size_t at) {
		return std::bit_cast<float>(static_cast<uint32_t>(bytes.at(at)) | (static_cast<uint32_t>(bytes.at(at + 1)) << 8u) |
		                            (static_cast<uint32_t>(bytes.at(at + 2)) << 16u) |
		                            (static_cast<uint32_t>(bytes.at(at + 3)) << 24u));
	};
	return {.height = word(0), .strength = word(4), .alignment = word(8)};
}

std::array<uint8_t, 12> saved_game_load::CreatureCurseBytes(const CreatureCurse& curse)
{
	std::array<uint8_t, 12> bytes {};
	const auto put = [&bytes](size_t at, float value) {
		const auto word = std::bit_cast<uint32_t>(value);
		for (size_t i = 0; i < 4; ++i)
		{
			bytes.at(at + i) = static_cast<uint8_t>(word >> (8u * i));
		}
	};
	put(0, curse.height);
	put(4, curse.strength);
	put(8, curse.alignment);
	return bytes;
}

saved_game_load::CreatureCurse saved_game_load::CreatureCurseToSave(const Registry& registry,
                                                                    std::optional<entt::entity> creature)
{
	if (!creature.has_value() || !registry.Valid(*creature))
	{
		return {};
	}
	const auto* body = registry.TryGet<const components::Creature>(*creature);
	const auto height = player_creature::CreatureHeightProperty(registry, *creature);
	if (body == nullptr || !height.has_value())
	{
		return {};
	}
	return {.height = *height, .strength = body->strength, .alignment = body->alignment};
}

bool saved_game_load::PutBackCreatureCurse(Registry& registry, std::optional<entt::entity> creature, const CreatureCurse& curse,
                                           bool curseRunning)
{
	if (!creature.has_value() || !registry.Valid(*creature) || !curseRunning || !(curse.height > 0.0f))
	{
		return false;
	}
	auto* body = registry.TryGet<components::Creature>(*creature);
	if (body == nullptr || !player_creature::SetCreatureHeight(registry, *creature, curse.height))
	{
		return false;
	}
	body->strength = curse.strength;
	body->alignment = std::clamp(curse.alignment, -1.0f, 1.0f);
	return true;
}

void saved_game_load::PutBackTemple(TempleInteriorInterface& temple, const LoadedGameState& state)
{
	temple.SetRoomsOpened(state.roomsAvailable != 0);
	std::array<bool, TempleInteriorInterface::k_MapButtonCount> shown {};
	for (size_t button = 0; button < shown.size(); ++button)
	{
		shown.at(button) = state.mapButtons.at(button) != 0;
	}
	temple.SetMapButtons(shown);
}

bool saved_game_load::CameraHelpUnwritten(const std::array<uint32_t, 4>& cameraWords)
{
	return cameraWords[k_TailCameraHelpFeatures] == 0 && cameraWords[k_TailCameraHelpAutoPitchAngle] == 0 &&
	       cameraWords[k_TailCameraHelpAutoPitchHeight] == 0;
}

void saved_game_load::PutStartCameraHelp(std::array<uint32_t, 4>& cameraWords)
{
	cameraWords[k_TailCameraHelpFeatures] = static_cast<uint32_t>(camera_help::k_NormalFeatures);
	cameraWords[k_TailCameraHelpAutoPitchAngle] = std::bit_cast<uint32_t>(camera_help::k_DefaultAutoPitchAngle);
	cameraWords[k_TailCameraHelpAutoPitchHeight] = std::bit_cast<uint32_t>(camera_help::k_DefaultAutoPitchDistance);
}

saved_game_load::BookmarkThings
saved_game_load::BookmarkThingsOf(const LoadedGameState& state,
                                  const std::function<entt::entity(gamesave::ObjectNumber)>& objectOf)
{
	BookmarkThings things {};
	for (size_t index = 0; index < things.size(); ++index)
	{
		const auto number = state.bookmarks.at(index).object.number;
		if (const auto thing = number != 0 && objectOf ? objectOf(number) : entt::entity {entt::null}; thing != entt::null)
		{
			things.at(index) = thing;
		}
	}
	return things;
}

openblack::ecs::systems::SavedBookmarks saved_game_load::SavedBookmarksOf(const LoadedGameState& state,
                                                                          const BookmarkThings& things)
{
	systems::SavedBookmarks saved;
	for (size_t index = 0; index < saved.bookmarks.size(); ++index)
	{
		const auto& part = state.bookmarks.at(index);
		saved.bookmarks.at(index) = {.flags = part.byte,
		                             .target = things.at(index),
		                             .position = {.x = std::bit_cast<int32_t>(part.points[0][0]),
		                                          .z = std::bit_cast<int32_t>(part.points[0][1]),
		                                          .altitude = part.points[0][2]},
		                             .cameraOffset = {part.points[1][0], part.points[1][1], part.points[1][2]}};
	}
	const auto followed = static_cast<int32_t>(state.bookmarkWords[0]);
	saved.followed = followed >= 0 && followed < static_cast<int32_t>(systems::k_SavedBookmarkCount)
	                     ? std::optional(static_cast<uint8_t>(followed))
	                     : std::nullopt;
	saved.current = static_cast<uint8_t>(state.bookmarkWords[1]);
	saved.enabled = state.bookmarkWords[2] != 0;
	return saved;
}

void saved_game_load::Apply(const LoadedGameState& state)
{
	// the game's data: both seeds, and the objects' creation counter (the loaded objects keep their own numbers)
	game_random::Load(state.seeds);
	if (Locator::objectCreationIndexSystem::has_value())
	{
		object_index::Restore(state.createdObjects);
	}
	// and the game loop's passes since the saved land's start
	if (Locator::time::has_value())
	{
		game_clock::SetFrameCount(state.frameCount);
	}
	// the land's number and the influence multipliers, which the land's script keeps
	if (Locator::mapScriptSystem::has_value())
	{
		auto& globals = Locator::mapScriptSystem::value().Globals();
		globals.landNumber = static_cast<int32_t>(state.landNumber);
		globals.playerInfluenceMultiplier = state.playerInfluenceMultiplier;
		globals.townInfluenceMultiplier = state.townInfluenceMultiplier;
	}
	// the time of day forced to the saved one, then its speed and its cycle (duration, night, change)
	if (Locator::dayNightClock::has_value())
	{
		auto& clock = Locator::dayNightClock::value().Clock();
		clock.ForceVisualTime(state.visualTime);
		clock.SetScale(state.visualTimeScale);
		clock.SetCycle(state.visualTimeCycle[0], state.visualTimeCycle[2], state.visualTimeCycle[1]);
		if (Locator::skySystem::has_value())
		{
			Locator::skySystem::value().SetTime(clock.GetScriptTime());
		}
	}
	// the script's interface interaction level set again as the script's load ends, with all it sets (the interface's
	// limits, the camera features, the hand's reach and the control map's switches); the camera help read next wins
	help::interface_interaction::Set(state.interfaceLevel);
	// the camera's zones as saved, in place of every one there was: its switches, limits and fence, and the exclusions
	// saved with the game
	if (Locator::cameraZoneSystem::has_value())
	{
		Locator::cameraZoneSystem::value().Restore(state.cameraZones);
	}
	// the camera help as the scripts last set it: every feature as saved, and the auto-pitch angle and height
	if (Locator::cameraHelpSystem::has_value())
	{
		auto& help = Locator::cameraHelpSystem::value().Get();
		help.features = state.cameraHelpFeatures;
		help.autoPitch = state.autoPitchAngle;
		help.autoPitchHeight = state.autoPitchHeight;
		// A readout for the log (the game channel at debug): the camera help the saved game put back
		if (const auto logger = spdlog::get("game"); logger != nullptr && logger->should_log(spdlog::level::debug))
		{
			logger->debug("Camera help loaded: {}", bookmark_save_readout::DescribeCameraHelp(
			                                            state.cameraHelpFeatures, state.autoPitchAngle, state.autoPitchHeight));
		}
	}
	// whether the temple's creature room shows the creature, as a script last said
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetCreatureShown(state.creatureInTemple != 0);
	}
	// the snow lying
	if (Locator::snowSystem::has_value())
	{
		Locator::snowSystem::value().Restore(state.snowDepths, state.snowBlockChanges, state.snowMeltClock, state.snowMeltBand);
	}
	// whether the temple's rooms are open and the main room's map buttons, read just before the did-you-know texts
	if (Locator::temple::has_value())
	{
		PutBackTemple(Locator::temple::value(), state);
	}
	// the did-you-know texts read, category by category
	if (Locator::scriptState::has_value())
	{
		for (size_t category = 0; category < state.didYouKnowTexts.size(); ++category)
		{
			script_highlight::did_you_know_read::Load(static_cast<DykCategory>(category), state.didYouKnowTexts.at(category),
			                                          state.didYouKnowCounts.at(category));
		}
	}
	// (openblack, a readout) the temple as the load puts it back: whether its rooms are open, its map buttons, the
	// did-you-know texts read in each category and the challenge room's records
	if (auto log = spdlog::get("game"); log != nullptr)
	{
		const auto counted = [](const auto& lists) {
			std::string text;
			for (const auto& list : lists)
			{
				text += fmt::format("{}{}", text.empty() ? "" : " ", list.size());
			}
			return text;
		};
		log->info("Load: the temple put back: rooms open {}, map buttons {}, did-you-know read {}, {} challenge records",
		          state.roomsAvailable, fmt::join(state.mapButtons, " "), counted(state.didYouKnowTexts),
		          state.challenges.size());
	}
	// the turns of the last autosave and quick save, which the saves keep
	if (Locator::saveGame::has_value())
	{
		Locator::saveGame::value().SetLastAutoSaveTurn(state.lastAutoSaveTurn);
		Locator::saveGame::value().SetLastQuickSaveTurn(state.lastQuickSaveTurn);
	}
	// a skirmish carries on as one, and a game that was over stays over
	if (Locator::entitiesRegistry::has_value())
	{
		PutBackSkirmish(Locator::entitiesRegistry::value(), state);
		PutBackGameOver(Locator::entitiesRegistry::value(), state);
	}
	// the task that had the camera has it again, the camera left as the load made it, so its own end of the camera
	// control gives it back
	if (Locator::scriptControlSystem::has_value())
	{
		Locator::scriptControlSystem::value().RestoreCameraOwner(state.cameraOwner);
	}
	// the script's camera switches as saved: whether the leashes and the highlights are drawn, and the unused flag
	{
		auto& camera = help::script_control::GetCameraControl();
		camera.drawLeash = static_cast<int32_t>(state.drawLeash);
		camera.field7C = static_cast<int32_t>(state.unusedCameraFlag);
		camera.drawHighlight = static_cast<int32_t>(state.drawHighlight);
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Load: the script's camera switches: leashes {}, highlights {}, owner {}",
		                   state.drawLeash, state.drawHighlight, state.cameraOwner);
	}
	// the help system's message sets, categories' turns, help level and switch as saved; then the wide screen and its
	// owner as saved, without what a change of the wide screen does (the dialogs put away, the interface switched): the
	// bars at their saved timer, and the sounds that the script's wide screen keeps quiet
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->Restore(state.helpState);
		helpSystem->RestoreWideScreen(static_cast<int32_t>(state.wideScreen), state.wideScreenOwner);
		audio::SetScriptWideScreen(helpSystem->IsScriptWideScreen());
		// the dialogue as saved: its task has it, without what a request does (the texts cleared), with its texts shown
		// and click waits; the dialogue the temple last kept as saved; then, as the game's load ends, the kept dialogue
		// goes back to its task when the save was made with one kept and nobody has the dialogue, and the box shows its
		// texts again
		const auto& dialogue = state.helpState.dialogue;
		if (Locator::dialogueControlSystem::has_value())
		{
			Locator::dialogueControlSystem::value().Restore(dialogue.owner);
		}
		helpSystem->GetDialogue().Restore(
		    {.texts = dialogue.texts, .waitClick = dialogue.clickToContinue, .noClick = dialogue.noSkip});
		if (Locator::temple::has_value())
		{
			Locator::temple::value().SetLastKeptDialogue(state.helpState.kept);
		}
		temple_help::GameDialogue gameDialogue;
		temple_help::BringBackAtLoad(gameDialogue, state.helpState.kept, state.helpState.textKept);
		// A readout for the log (the game channel at debug): the dialogue the saved game put back
		if (const auto logger = spdlog::get("game"); logger != nullptr && logger->should_log(spdlog::level::debug))
		{
			logger->debug("Load: the dialogue's task {}, newest text {}; the temple's kept dialogue of task {}, {}",
			              dialogue.owner, dialogue.texts[0], state.helpState.kept.owner,
			              state.helpState.textKept ? "kept, given back if nobody has the dialogue" : "not kept");
		}
	}
	// the creature help switch as saved, over the option's: a script's SET_CREATURE_HELP stays set after a load
	if (Locator::creatureHelpSystem::has_value())
	{
		Locator::creatureHelpSystem::value().SetHelpOn(state.creatureHelp != 0);
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const float time =
		    Locator::infoConstants::has_value() ? Locator::infoConstants::value().helpSystem.wideScreenTime : 2.0f;
		Locator::cinematicDirectorSystem::value().RestoreWideScreen(state.wideScreen != 0, state.wideScreenTimer, time);
	}
	// the land's balance as saved, over the map clear's
	if (Locator::landBalanceSystem::has_value() && Locator::mapScriptSystem::has_value())
	{
		PutBackLandBalance(Locator::landBalanceSystem::value(), state);
		// (debug readout) the balance put back
		if (auto log = spdlog::get("game"); log != nullptr)
		{
			log->info("Load: the land's balance put back: {}", fmt::join(state.landBalance, ", "));
		}
	}
	// the load's end: the turn, and the clock stopped, set to the turn's time and started again; then the frame clock from
	// the loaded turn, its frame time 0, as the game's resolve of a load does, so the first frame after the load takes no
	// game time for the turns the load moved the clock by (the land's start set it from turn 0)
	game_clock::SetTurn(state.gameTurn);
	game_clock::ResetLocalTimer();
	game_clock::OnLoad();
}
