/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedGameSave.h"

#include <algorithm>
#include <bit>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/CameraHelp.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/LandBalanceSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Help/ScriptControl.h"
#include "Locator.h"
#include "Sound/GameMusic.h"

using namespace openblack::ecs;
using namespace openblack::ecs::saved_game_load;

namespace
{
constexpr size_t k_DidYouKnowCategories = 5;
/// The bytes of a challenge's reminder script the challenge room's save writes
constexpr size_t k_ChallengeScriptBytes = 0x40;

std::vector<uint32_t> WordsOf(std::span<const float> values)
{
	std::vector<uint32_t> words;
	words.reserve(values.size());
	for (const auto value : values)
	{
		words.push_back(std::bit_cast<uint32_t>(value));
	}
	return words;
}
} // namespace

std::array<float, openblack::land_balance::k_Count>
saved_game_save::LandBalanceOf(const systems::LandBalanceSystemInterface& balance)
{
	std::array<float, land_balance::k_Count> values {};
	for (size_t index = 0; index < values.size(); ++index)
	{
		values.at(index) = balance.Get(index);
	}
	return values;
}

void saved_game_save::PutCameraZones(gamesave::GameTail& tail, const camera_zones::Zones& zones)
{
	// A readout for the log (the game channel at debug): the zones a saved game takes
	if (const auto logger = spdlog::get("game"); logger != nullptr && logger->should_log(spdlog::level::debug))
	{
		logger->debug("Camera zones saved: {}", camera_zones::Describe(zones));
	}
	tail.exclusionVersion = k_TailExclusionVersion;
	auto& values = tail.exclusionValues;
	values[k_TailExclusionsOn] = zones.exclusionsOn ? 1U : 0U;
	values[k_TailFenceOn] = zones.fenceOn ? 1U : 0U;
	values[k_TailUseMaxAltitude] = zones.useMaxAltitude ? 1U : 0U;
	values[k_TailUseHeightAboveLand] = zones.useHeightAboveLand ? 1U : 0U;
	values[k_TailMaxAltitude] = std::bit_cast<uint32_t>(zones.maxAltitude);
	values[k_TailHeightAboveLand] = std::bit_cast<uint32_t>(zones.heightAboveLand);
	tail.forceFieldPoints.values.clear();
	for (const auto& point : zones.fence)
	{
		tail.forceFieldPoints.values.push_back({point.x, point.y, point.z});
	}
	tail.exclusionNameSize = camera_zones::k_ExclusionRecordSize;
	tail.exclusions.clear();
	for (const auto& exclusion : zones.exclusions)
	{
		if (exclusion.saved)
		{
			const auto record = camera_zones::RecordOf(exclusion);
			tail.exclusions.push_back({.values = {record.begin(), record.end()}});
		}
	}
}

saved_game_save::GameValues saved_game_save::CurrentValues()
{
	GameValues values;
	if (Locator::gameRandom::has_value())
	{
		values.seeds = game_random::Current();
	}
	if (Locator::time::has_value())
	{
		values.gameTurn = game_clock::Turn();
		values.frameCount = game_clock::FrameCount();
	}
	if (Locator::objectCreationIndexSystem::has_value())
	{
		values.createdObjects = object_index::Peek();
	}
	if (Locator::mapScriptSystem::has_value())
	{
		const auto& globals = Locator::mapScriptSystem::value().Globals();
		values.landNumber = static_cast<uint32_t>(globals.landNumber);
		values.influenceMultipliers = std::array {globals.playerInfluenceMultiplier, globals.townInfluenceMultiplier};
	}
	if (Locator::dayNightClock::has_value())
	{
		const auto& clock = Locator::dayNightClock::value().Clock();
		values.clock = GameValues::Clock {.visualTime = clock.GetVisualTime(),
		                                  .cycle = {clock.GetDuration(), clock.GetChange(), clock.GetNight()},
		                                  .scale = clock.GetScale()};
	}
	if (Locator::creatureCaveSystem::has_value())
	{
		values.creatureInTemple = Locator::creatureCaveSystem::value().IsCreatureShown();
	}
	// the local player's creature's known leashes, none known with no creature
	if (Locator::leashSystem::has_value())
	{
		const auto& leash = Locator::leashSystem::value();
		const auto creature = leash.PlayersCreature(PlayerNames::PLAYER_ONE);
		values.leashes = creature.has_value()
		                     ? LeashByte(leash.Knows(*creature, LeashType::Evil), leash.Knows(*creature, LeashType::Rope),
		                                 leash.Knows(*creature, LeashType::Good))
		                     : uint8_t {0};
	}
	values.interfaceLevel = help::interface_interaction::GetLevel();
	if (Locator::cameraZoneSystem::has_value())
	{
		values.cameraZones = Locator::cameraZoneSystem::value().GetZones();
	}
	if (Locator::cameraHelpSystem::has_value())
	{
		values.cameraHelp = GameValues::CameraHelp {.features = camera_help::GetEnabledFeatures(),
		                                            .autoPitchAngle = camera_help::GetAutoPitchAngle(),
		                                            .autoPitchHeight = camera_help::GetAutoPitchDistance()};
	}
	if (Locator::snowSystem::has_value())
	{
		values.snow = Locator::snowSystem::value().Saved();
	}
	if (Locator::cameraBookmarkSystem::has_value())
	{
		values.bookmarks = Locator::cameraBookmarkSystem::value().Saved();
	}
	if (Locator::scriptState::has_value())
	{
		std::array<std::vector<uint32_t>, k_DidYouKnowCategories> texts;
		for (size_t category = 0; category < texts.size(); ++category)
		{
			const auto read = script_highlight::did_you_know_read::In(static_cast<DykCategory>(category));
			texts.at(category).assign(read.begin(), read.end());
		}
		values.didYouKnow = std::move(texts);
	}
	if (Locator::saveGame::has_value())
	{
		values.lastAutoSaveTurn = Locator::saveGame::value().LastAutoSaveTurn();
		values.lastQuickSaveTurn = Locator::saveGame::value().LastQuickSaveTurn();
	}
	if (Locator::entitiesRegistry::has_value())
	{
		values.skirmish = Locator::entitiesRegistry::value().Context().skirmish;
		values.gameOver = Locator::entitiesRegistry::value().Context().gameOver;
	}
	if (Locator::scriptControlSystem::has_value())
	{
		values.cameraOwner = Locator::scriptControlSystem::value().GetCameraOwner();
	}
	// the script's camera switches, as the script set them
	{
		const auto& camera = help::script_control::GetCameraControl();
		values.scriptSwitches = GameValues::ScriptSwitches {
		    .drawLeash = camera.drawLeash, .unusedCameraFlag = camera.field7C, .drawHighlight = camera.drawHighlight};
	}
	if (const auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		values.wideScreen =
		    GameValues::WideScreen {.on = helpSystem->GetWideScreen(), .owner = helpSystem->GetWideScreenOwner()};
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		values.wideScreenTimer = Locator::cinematicDirectorSystem::value().GetWideScreenTimer();
	}
	if (const auto lock = audio::game_music::Lock(); const auto* music = audio::game_music::Get())
	{
		values.music = GameValues::Music {.scriptType = static_cast<uint32_t>(music->GetScriptType()),
		                                  .scriptPlaying = static_cast<uint32_t>(music->GetScriptStarted()),
		                                  .town = music->GetCurrentTown(),
		                                  .alignment = music->GetCameraAlignment()};
	}
	if (Locator::dynamicsSystem::has_value())
	{
		const auto& dynamics = Locator::dynamicsSystem::value();
		const auto idOf = [](entt::entity entity) -> std::optional<uint32_t> {
			return entity != entt::null ? std::optional {entt::to_integral(entity)} : std::nullopt;
		};
		values.hits = GameValues::Hits {.hit = idOf(dynamics.GetHitObject()), .hitter = idOf(dynamics.GetObjectWhichHit())};
	}
	if (Locator::landBalanceSystem::has_value() && Locator::mapScriptSystem::has_value())
	{
		values.landBalance = LandBalanceOf(Locator::landBalanceSystem::value());
	}
	if (Locator::temple::has_value())
	{
		values.temple = TempleOf(Locator::temple::value());
	}
	return values;
}

openblack::gamesave::BookmarkPart saved_game_save::BookmarkPartOf(const systems::SavedBookmark& bookmark,
                                                                  gamesave::ObjectNumber target)
{
	const gamesave::Point offset {bookmark.cameraOffset.x, bookmark.cameraOffset.y, bookmark.cameraOffset.z};
	return {.byte = bookmark.flags,
	        .point = offset,
	        .object = {.number = target},
	        .points = {gamesave::Point {std::bit_cast<float>(bookmark.position.x), std::bit_cast<float>(bookmark.position.z),
	                                    bookmark.position.altitude},
	                   offset}};
}

std::array<uint32_t, 3> saved_game_save::BookmarkWordsOf(const systems::SavedBookmarks& bookmarks)
{
	constexpr uint32_t k_NoneFollowed = 0xFFFFFFFF;
	return {bookmarks.followed.has_value() ? uint32_t {*bookmarks.followed} : k_NoneFollowed, uint32_t {bookmarks.current},
	        bookmarks.enabled ? 1U : 0U};
}

saved_game_save::GameValues::Temple saved_game_save::TempleOf(const TempleInteriorInterface& temple)
{
	return {.roomsOpened = temple.RoomsOpened(), .mapButtons = temple.GetMapButtons()};
}

openblack::gamesave::ChallengePart saved_game_save::ChallengePartOf(const components::ChallengeRecord& record,
                                                                    const PlaceOfThing& placeOf)
{
	const auto pointOf = [](const std::optional<glm::vec3>& point) {
		return point.has_value() ? gamesave::Point {point->x, point->y, point->z} : gamesave::Point {};
	};
	gamesave::ChallengePart part {
	    .half = static_cast<uint16_t>(record.challenge),
	    .words = {record.title, std::bit_cast<uint32_t>(record.success), std::bit_cast<uint32_t>(record.alignment)},
	    .points = {pointOf(record.position), pointOf(record.focus)},
	    // the turn it started is written twice, then the turn it was done on and the count of its values
	    .secondWords = {record.startedTurn, record.startedTurn, record.doneTurn.value_or(0),
	                    static_cast<uint32_t>(record.arguments.size())},
	    .lastWord = record.quest ? 1U : 0U,
	};
	part.name.values.assign(k_ChallengeScriptBytes, 0);
	std::copy_n(record.reminderScript.begin(), std::min(record.reminderScript.size(), k_ChallengeScriptBytes),
	            part.name.values.begin());
	for (const auto& argument : record.arguments)
	{
		const bool thing = argument.type == openblack::lhvm::DataType::Object;
		part.first.values.push_back(thing && placeOf ? placeOf(argument.value.uintVal) : argument.value.uintVal);
		part.second.values.push_back(static_cast<uint32_t>(argument.type));
	}
	return part;
}

std::vector<openblack::gamesave::ChallengePart> saved_game_save::ChallengePartsOf(const Registry& registry,
                                                                                  const PlaceOfThing& placeOf)
{
	std::vector<gamesave::ChallengePart> parts;
	for (const auto entity : challenge_records::InOrder(registry))
	{
		parts.push_back(ChallengePartOf(registry.Get<components::ChallengeRecord>(entity), placeOf));
	}
	return parts;
}

saved_game_save::GameValues saved_game_save::ValuesOf(const LoadedGameState& state)
{
	return {
	    .landNumber = state.landNumber,
	    .seeds = state.seeds,
	    .gameTurn = state.gameTurn,
	    .createdObjects = state.createdObjects,
	    .frameCount = state.frameCount,
	    .influenceMultipliers = std::array {state.playerInfluenceMultiplier, state.townInfluenceMultiplier},
	    .clock =
	        GameValues::Clock {.visualTime = state.visualTime, .cycle = state.visualTimeCycle, .scale = state.visualTimeScale},
	    .interfaceLevel = state.interfaceLevel,
	    .cameraZones = state.cameraZones,
	    .cameraHelp = GameValues::CameraHelp {.features = state.cameraHelpFeatures,
	                                          .autoPitchAngle = state.autoPitchAngle,
	                                          .autoPitchHeight = state.autoPitchHeight},
	    .creatureInTemple = state.creatureInTemple != 0,
	    .snow = systems::SavedSnow {.depths = state.snowDepths,
	                                .blockChanges = state.snowBlockChanges,
	                                .meltClock = state.snowMeltClock,
	                                .meltBand = state.snowMeltBand},
	    .didYouKnow = state.didYouKnowTexts,
	    .lastAutoSaveTurn = std::nullopt,
	    .lastQuickSaveTurn = state.lastQuickSaveTurn,
	    .skirmish = state.skirmish,
	    .gameOver = state.gameOver,
	    .cameraOwner = state.cameraOwner,
	    .scriptSwitches = GameValues::ScriptSwitches {.drawLeash = static_cast<int32_t>(state.drawLeash),
	                                                  .unusedCameraFlag = static_cast<int32_t>(state.unusedCameraFlag),
	                                                  .drawHighlight = static_cast<int32_t>(state.drawHighlight)},
	    .wideScreen = GameValues::WideScreen {.on = static_cast<int32_t>(state.wideScreen), .owner = state.wideScreenOwner},
	    .wideScreenTimer = state.wideScreenTimer,
	    .landBalance = state.landBalance,
	    .temple =
	        GameValues::Temple {.roomsOpened = state.roomsAvailable != 0,
	                            .mapButtons = {state.mapButtons[0] != 0, state.mapButtons[1] != 0, state.mapButtons[2] != 0,
	                                           state.mapButtons[3] != 0, state.mapButtons[4] != 0}},
	};
}

openblack::gamesave::GameHead saved_game_save::HeadOf(const GameValues& values, gamesave::GameHead base)
{
	if (values.landNumber.has_value())
	{
		base.landNumber = *values.landNumber;
	}
	// after the local player, the leashes its creature knows
	if (values.leashes.has_value())
	{
		base.bytes[saved_game_load::k_LeashesByte] = *values.leashes;
	}
	if (values.seeds.has_value())
	{
		base.data[k_SyncedSeed] = values.seeds->synced;
		base.data[k_LocalSeed] = values.seeds->local;
	}
	if (values.gameTurn.has_value())
	{
		base.data[k_GameTurn] = *values.gameTurn;
	}
	if (values.createdObjects.has_value())
	{
		base.data[k_CreatedObjects] = *values.createdObjects;
	}
	if (values.frameCount.has_value())
	{
		base.data[k_FrameCount] = *values.frameCount;
	}
	if (values.influenceMultipliers.has_value())
	{
		base.words[k_PlayerInfluence] = std::bit_cast<uint32_t>((*values.influenceMultipliers)[0]);
		base.words[k_TownInfluence] = std::bit_cast<uint32_t>((*values.influenceMultipliers)[1]);
	}
	if (values.clock.has_value())
	{
		base.words[k_VisualTime] = std::bit_cast<uint32_t>(values.clock->visualTime);
		for (size_t value = 0; value < values.clock->cycle.size(); ++value)
		{
			base.words.at(k_FirstCycleValue + value) = std::bit_cast<uint32_t>(values.clock->cycle.at(value));
		}
		base.words[k_VisualTimeScale] = std::bit_cast<uint32_t>(values.clock->scale);
	}
	// the script's interface interaction level, the fifth of the script's last words
	if (values.interfaceLevel.has_value())
	{
		base.tail.scriptLastWords[k_TailScriptInterfaceLevel] = static_cast<uint32_t>(*values.interfaceLevel);
	}
	// the camera's zones, after the camera and the climate
	if (values.cameraZones.has_value())
	{
		PutCameraZones(base.tail, *values.cameraZones);
	}
	// the camera help's features and auto-pitch angle and height, the three words before the creature in the temple
	if (values.cameraHelp.has_value())
	{
		base.tail.cameraWords[k_TailCameraHelpFeatures] = static_cast<uint32_t>(values.cameraHelp->features);
		base.tail.cameraWords[k_TailCameraHelpAutoPitchAngle] = std::bit_cast<uint32_t>(values.cameraHelp->autoPitchAngle);
		base.tail.cameraWords[k_TailCameraHelpAutoPitchHeight] = std::bit_cast<uint32_t>(values.cameraHelp->autoPitchHeight);
	}
	// whether the temple's creature room shows the creature, the word just before the snow
	if (values.creatureInTemple.has_value())
	{
		base.tail.cameraWords[k_TailCreatureInTemple] = *values.creatureInTemple ? 1U : 0U;
	}
	// the snow lying: a grid of the record's size (a new record: any), its blocks' changes, the melting clock and band
	if (values.snow.has_value())
	{
		const auto& snow = *values.snow;
		auto& tail = base.tail;
		if (tail.snow.values.empty() || tail.snow.values.size() == snow.depths.size())
		{
			tail.snow.values = WordsOf(snow.depths);
		}
		if (tail.snowSecond.values.empty() || tail.snowSecond.values.size() == snow.blockChanges.size())
		{
			tail.snowSecond.values = WordsOf(snow.blockChanges);
		}
		tail.snowWords[0] = std::bit_cast<uint32_t>(snow.meltClock);
		tail.snowWords[1] = static_cast<uint32_t>(snow.meltBand);
	}
	// each category's texts read, then the same count again, as the game writes them
	if (values.didYouKnow.has_value())
	{
		for (size_t category = 0; category < k_DidYouKnowCategories; ++category)
		{
			const auto& texts = values.didYouKnow->at(category);
			base.tail.didYouKnow.at(category).values = texts;
			base.tail.didYouKnowWords.at(category) = static_cast<uint32_t>(texts.size());
		}
	}
	if (values.lastAutoSaveTurn.has_value())
	{
		base.tail.words[k_TailLastAutoSaveTurn] = *values.lastAutoSaveTurn;
	}
	if (values.lastQuickSaveTurn.has_value())
	{
		base.tail.words[k_TailLastQuickSaveTurn] = *values.lastQuickSaveTurn;
	}
	if (values.skirmish.has_value())
	{
		base.tail.words[k_TailSkirmish] = *values.skirmish ? 1U : 0U;
	}
	// then whether the game is over for the local player
	if (values.gameOver.has_value())
	{
		base.tail.words[k_TailGameOver] = *values.gameOver ? 1U : 0U;
	}
	// the script's camera owner, then the help's wide screen, its owner and the bars' timer
	if (values.cameraOwner.has_value())
	{
		base.tail.scriptSecondWords[k_TailScriptCameraOwner] = *values.cameraOwner;
	}
	// the leashes drawn, the unused camera flag and the highlights drawn, the first three of the script's words
	if (values.scriptSwitches.has_value())
	{
		base.tail.scriptSecondWords[k_TailScriptDrawLeash] = static_cast<uint32_t>(values.scriptSwitches->drawLeash);
		base.tail.scriptSecondWords[k_TailScriptUnusedCameraFlag] =
		    static_cast<uint32_t>(values.scriptSwitches->unusedCameraFlag);
		base.tail.scriptSecondWords[k_TailScriptDrawHighlight] = static_cast<uint32_t>(values.scriptSwitches->drawHighlight);
	}
	if (values.wideScreen.has_value())
	{
		base.tail.helpLastWords[k_TailHelpWideScreen] = static_cast<uint32_t>(values.wideScreen->on);
		base.tail.helpLastWords[k_TailHelpWideScreenOwner] = values.wideScreen->owner;
	}
	if (values.wideScreenTimer.has_value())
	{
		base.tail.helpLastWords[k_TailHelpWideScreenTimer] = std::bit_cast<uint32_t>(*values.wideScreenTimer);
	}
	if (values.music.has_value())
	{
		base.tail.audioWords = {values.music->scriptType, values.music->scriptPlaying};
		base.tail.audioWord = std::bit_cast<uint32_t>(values.music->alignment);
	}
	// the land's balance, its multipliers' bytes as they are in play
	if (values.landBalance.has_value())
	{
		base.tail.landBalance = std::bit_cast<decltype(base.tail.landBalance)>(*values.landBalance);
	}
	// after the audio, whether the temple's rooms are open, then the main room's five map buttons, each 1 for on
	if (values.temple.has_value())
	{
		base.tail.roomsAvailable = values.temple->roomsOpened ? 1U : 0U;
		for (size_t button = 0; button < base.tail.buttonConfig.size(); ++button)
		{
			base.tail.buttonConfig.at(button) = values.temple->mapButtons.at(button) ? 1U : 0U;
		}
	}
	return base;
}

openblack::gamesave::GameHead saved_game_save::NewGameHead()
{
	gamesave::GameHead head;
	head.tail.scriptSecondWords[k_TailScriptDrawLeash] = 1;
	head.tail.scriptSecondWords[k_TailScriptDrawHighlight] = 1;
	head.tail.scriptSecondWords[k_TailScriptPlayCreatureSounds] = 1;
	head.tail.scriptLastWords[k_TailScriptCitadelInteract] = 1;
	// and the camera help as a game starts with it, and the camera's zones (the exclusions kept to, no fence, no limits
	// and both limits 500)
	saved_game_load::PutStartCameraHelp(head.tail.cameraWords);
	PutCameraZones(head.tail, {});
	return head;
}
