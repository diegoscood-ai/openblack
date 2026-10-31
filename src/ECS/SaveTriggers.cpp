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
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "Input/GamePackets.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// openblack plays no network game and keeps no skirmish of its own
constexpr bool k_NetworkGame = false;
constexpr bool k_Skirmish = false;

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

bool save_triggers::ExitSaves(bool gameOver, bool networkGame, bool skirmish, uint32_t land)
{
	return !gameOver && !networkGame && !skirmish && land != k_PlaygroundLand;
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
	if (!QuickSaveAllowed(k_NetworkGame, ScriptWideScreenOn(), GameIsOver(), saves.LastQuickSaveTurn(), turn))
	{
		return;
	}
	saves.SetLastQuickSaveTurn(turn);
	const auto local =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
	game_packets::Push({.type = game_packets::Type::QuickSave, .player = local});
}

void save_triggers::RegisterPacketHandlers()
{
	game_packets::SetHandler(game_packets::Type::QuickSave,
	                         [](const game_packets::Packet& /*packet*/) { SaveIntoSlot(k_QuickSaveSlot, "quick save", true); });
}

void save_triggers::TempleBuilt()
{
	if (!k_Skirmish && !k_NetworkGame)
	{
		SaveIntoSlot(k_AutoSaveSlot, "the temple is built", false);
	}
}

void save_triggers::Exiting()
{
	if (ExitSaves(GameIsOver(), k_NetworkGame, k_Skirmish, LandInPlay()))
	{
		SaveIntoSlot(k_AutoSaveSlot, "leaving the game", false);
	}
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
		return Told(saved_slot::RoomSave(slot, roomName), slot, "the save game room", true);
	}
	return SaveIntoSlot(slot, "asked for", true);
}
