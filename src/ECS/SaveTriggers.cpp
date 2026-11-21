/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveTriggers.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>

#include <spdlog/spdlog.h>

#include "ECS/Events/Publish.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SavedSlot.h"
#include "ECS/SkirmishGame.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/SavePictureSystemInterface.h"
#include "GameClock.h"
#include "Gui/TextDatabase.h"
#include "Help/HelpSystem.h"
#include "Input/GamePackets.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// openblack plays no network game
constexpr bool k_NetworkGame = false;

bool ScriptWideScreenOn()
{
	const auto* help = help::Get();
	return help != nullptr && help->IsScriptWideScreen();
}

bool GameIsOver()
{
	return Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Context().gameOver;
}

uint32_t LandInPlay()
{
	return static_cast<uint32_t>(std::max(0, influence::LandNumber()));
}

/// One line of the log on how a save ended, and the game's message box when the player is to be told
saved_slot::SlotSave Told(saved_slot::SlotSave saved, uint32_t slot, const char* why, bool askedByPlayer)
{
	if (const auto log = spdlog::get("game"); log != nullptr)
	{
		if (saved.Written())
		{
			SPDLOG_LOGGER_INFO(log, "The game is saved into slot {} ({})", slot, why);
		}
		else if (saved.outcome != saved_slot::SlotSaveOutcome::NoSlot)
		{
			SPDLOG_LOGGER_WARN(log, "The game could not be saved into slot {} ({}): {}", slot, why, saved.failure);
			// (openblack, a readout) a save that could not be made: every reason, one line each, as the debug bar's
			// "List save refusals" gives them
			if (saved.outcome == saved_slot::SlotSaveOutcome::NotMade)
			{
				for (const auto& line : saved_slot::RefusalLines())
				{
					SPDLOG_LOGGER_INFO(log, "Save refusal: {}", line);
				}
			}
		}
	}
	if (save_triggers::ToldOnScreen(saved.outcome, askedByPlayer))
	{
		events::Publish(events::SaveProblem {});
	}
	return saved;
}

saved_slot::SlotSave SaveIntoSlot(uint32_t slot, const char* why, bool askedByPlayer)
{
	// A save by itself first takes a picture of the screen for the slot: its cells take it once the renderer gives it
	// back (with no profile kept, nothing is asked)
	if (Locator::savePictures::has_value())
	{
		Locator::savePictures::value().TakePicture(slot);
	}
	return Told(saved_slot::InstantSave(slot), slot, why, askedByPlayer);
}
} // namespace

bool save_triggers::AutoSaveDue(const AutoSaveConditions& conditions, bool force)
{
	if (conditions.scriptWideScreen || conditions.networkGame || conditions.land == k_PlaygroundLand)
	{
		return false;
	}
	return force || (conditions.turn - conditions.lastAutoSaveTurn > k_AutoSaveInterval && !conditions.paused);
}

bool save_triggers::QuickSaveAllowed(bool networkGame, bool scriptWideScreen, bool gameOver, uint32_t lastQuickSaveTurn,
                                     uint32_t turn)
{
	if (networkGame || scriptWideScreen || gameOver)
	{
		return false;
	}
	// the turns between them as a signed difference of the two counts, either way
	const auto gap = static_cast<int32_t>(lastQuickSaveTurn - turn);
	return std::abs(gap) > k_QuickSaveGap;
}

std::optional<save_triggers::QuickLoadRefusal> save_triggers::QuickLoadRefused(bool networkGame, bool gameOver,
                                                                               uint32_t lastLoadTurn, uint32_t turn)
{
	if (networkGame)
	{
		return QuickLoadRefusal::NetworkGame;
	}
	if (gameOver)
	{
		return QuickLoadRefusal::GameOver;
	}
	// the turns between them as a signed difference of the two counts, either way, as the quick save counts them
	const auto gap = static_cast<int32_t>(lastLoadTurn - turn);
	if (std::abs(gap) <= k_QuickSaveGap)
	{
		return QuickLoadRefusal::TooSoon;
	}
	return std::nullopt;
}

std::string_view save_triggers::QuickLoadRefusalText(QuickLoadRefusal refusal)
{
	switch (refusal)
	{
	case QuickLoadRefusal::NetworkGame:
		return "a network game";
	case QuickLoadRefusal::GameOver:
		return "the game is over";
	case QuickLoadRefusal::TooSoon:
		return "no more than 3 turns from the turn the last load was at";
	case QuickLoadRefusal::AlreadyAsked:
		return "a quick load already waits for its turn";
	}
	return "";
}

bool save_triggers::ExitSaves(bool gameOver, bool networkGame, bool skirmish, uint32_t land)
{
	return !gameOver && !networkGame && !skirmish && land != k_PlaygroundLand;
}

save_triggers::ExitSaveProfile save_triggers::ExitSaveProfileOf(std::u16string_view previous, std::u16string_view played,
                                                                bool restartForProfile, bool previousKept)
{
	if (previous.empty() || (!restartForProfile && player_profiles::SameName(played, previous)))
	{
		return ExitSaveProfile::Played;
	}
	if (previousKept && !player_profiles::SameName(played, previous))
	{
		return ExitSaveProfile::Previous;
	}
	return ExitSaveProfile::None;
}

std::optional<uint32_t> save_triggers::ScriptSlot(float popped)
{
	// a number past a whole one of 32 bits, or not a number, cuts to the lowest whole one, which is below 0
	constexpr float k_PastTheLargest = 2147483648.0f;
	if (!(popped > -1.0f) || !(popped < k_PastTheLargest))
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(std::trunc(popped)));
}

std::optional<saved_slot::SlotSave> save_triggers::AutoSave(bool force)
{
	if (!Locator::saveGame::has_value())
	{
		return std::nullopt;
	}
	auto& saves = Locator::saveGame::value();
	const auto land = LandInPlay();
	const AutoSaveConditions conditions {.scriptWideScreen = ScriptWideScreenOn(),
	                                     .networkGame = k_NetworkGame,
	                                     .land = land,
	                                     .turn = game_clock::Turn(),
	                                     .lastAutoSaveTurn = saves.LastAutoSaveTurn(),
	                                     .paused = game_clock::IsPaused()};
	if (!AutoSaveDue(conditions, force))
	{
		return std::nullopt;
	}
	saves.SetLastAutoSaveTurn(conditions.turn);
	// the land's count of autosaves picks its slot, and goes up after the save, whether it was written or not
	auto* profiles = Locator::playerProfileSystem::has_value() ? &Locator::playerProfileSystem::value() : nullptr;
	const auto count = profiles != nullptr ? profiles->CircleSlot(land) : 0U;
	auto saved = SaveIntoSlot(CircleSlot(count), "autosave", false);
	if (profiles != nullptr)
	{
		profiles->SetCircleSlot(land, count + 1);
	}
	return saved;
}

void save_triggers::EndTurn()
{
	const auto profile =
	    Locator::playerProfileSystem::has_value() ? Locator::playerProfileSystem::value().GetCurrentProfile() : std::nullopt;
	if (profile.has_value() && EndTurnAsks(profile->settings.autoSave, GameIsOver()))
	{
		AutoSave(false);
	}
}

void save_triggers::QuickSaveKey()
{
	if (!Locator::saveGame::has_value())
	{
		return;
	}
	auto& saves = Locator::saveGame::value();
	const auto turn = game_clock::Turn();
	const bool wideScreen = ScriptWideScreenOn();
	const bool gameOver = GameIsOver();
	if (!QuickSaveAllowed(k_NetworkGame, wideScreen, gameOver, saves.LastQuickSaveTurn(), turn))
	{
		// (debug readout) why the key did nothing; the original's key does nothing then either
		if (const auto log = spdlog::get("game"); log != nullptr)
		{
			log->debug("Quick save key: refused at turn {} (wide screen {}, game over {}, last quick save {})", turn,
			           wideScreen, gameOver, saves.LastQuickSaveTurn());
		}
		return;
	}
	saves.SetLastQuickSaveTurn(turn);
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	game_packets::Push({.type = game_packets::Type::QuickSave, .player = local});
}

std::optional<save_triggers::QuickLoadRefusal> save_triggers::QuickLoadKey()
{
	if (!Locator::saveGame::has_value())
	{
		return std::nullopt;
	}
	const auto& saves = Locator::saveGame::value();
	if (const auto refused = QuickLoadRefused(k_NetworkGame, GameIsOver(), saves.LastLoadTurn(), game_clock::Turn());
	    refused.has_value())
	{
		return refused;
	}
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	// the game keeps one packet for the player until its turn, which a second press only writes again: one load
	if (game_packets::IsPending(game_packets::Type::QuickLoad, local))
	{
		return QuickLoadRefusal::AlreadyAsked;
	}
	game_packets::Push({.type = game_packets::Type::QuickLoad, .player = local});
	return std::nullopt;
}

void save_triggers::RegisterPacketHandlers()
{
	game_packets::SetHandler(game_packets::Type::QuickSave,
	                         [](const game_packets::Packet& /*packet*/) { SaveIntoSlot(k_QuickSaveSlot, "quick save", true); });
	// the load itself waits for the frame's end: the turn and the frame go on with the world they started with
	game_packets::SetHandler(game_packets::Type::QuickLoad,
	                         [](const game_packets::Packet& /*packet*/) { events::Publish(events::QuickLoadAsked {}); });
}

void save_triggers::TempleBuilt()
{
	if (!ecs::IsSkirmishGame() && !k_NetworkGame)
	{
		SaveIntoSlot(k_AutoSaveSlot, "the temple is built", false);
	}
}

void save_triggers::Exiting(std::u16string_view previousProfile, bool restartForProfile, const std::function<void()>& takeUp)
{
	if (!ExitSaves(GameIsOver(), k_NetworkGame, ecs::IsSkirmishGame(), LandInPlay()))
	{
		return;
	}
	auto* profiles = Locator::playerProfileSystem::has_value() ? &Locator::playerProfileSystem::value() : nullptr;
	const auto played = profiles != nullptr ? profiles->GetCurrentProfile() : std::nullopt;
	const auto playedName = played.has_value() ? gui::ToUtf16(played->name) : std::u16string {};
	const bool previousKept = profiles != nullptr && player_profiles::Find(profiles->GetProfiles(), previousProfile) != nullptr;
	switch (ExitSaveProfileOf(previousProfile, playedName, restartForProfile, previousKept))
	{
	case ExitSaveProfile::Played:
		SaveIntoSlot(k_AutoSaveSlot, "leaving the game", false);
		break;
	case ExitSaveProfile::Previous:
		// the profile the Players page was opened with is played for the save, then the one played again
		profiles->SelectProfile(previousProfile);
		if (takeUp)
		{
			takeUp();
		}
		SaveIntoSlot(k_AutoSaveSlot, "leaving the game, into the profile played as the Players page was opened", false);
		profiles->SelectProfile(playedName);
		if (takeUp)
		{
			takeUp();
		}
		break;
	case ExitSaveProfile::None:
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Leaving the game saves nothing: the profile played as the Players page "
		                                        "was opened is gone, or the game starts again for it");
		break;
	}
}

void save_triggers::PlaygroundTrip()
{
	SaveIntoSlot(k_AutoSaveSlot, "going to the Gods' Playground", false);
}

void save_triggers::ScriptSave(float popped)
{
	if (const auto slot = ScriptSlot(popped); slot.has_value())
	{
		SaveIntoSlot(*slot, "a script", false);
	}
}

void save_triggers::ChallengeScrollTapped()
{
	SaveIntoSlot(k_ChallengeScrollSlot, "a challenge's scroll", false);
}

saved_slot::SlotSave save_triggers::SaveAsked(uint32_t slot, std::u16string_view roomName)
{
	if (slot < k_RoomSlots)
	{
		return RoomSaveAsked(slot, roomName);
	}
	return SaveIntoSlot(slot, "asked for", true);
}

saved_slot::SlotSave save_triggers::RoomSaveAsked(uint32_t slot, std::u16string_view name)
{
	return Told(saved_slot::RoomSave(slot, name), slot, "the save game room", true);
}
