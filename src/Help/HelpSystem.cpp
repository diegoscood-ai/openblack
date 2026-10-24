/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpSystem.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <utility>

#include <spdlog/spdlog.h>

#include "Camera/FieldOfView.h"
#include "Common/ProcessLogOnce.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "HelpMessageSets.h"
#include "Locator.h"
#include "SpiritsRuntime.h"
#include "ToolTips.h"

namespace openblack::help
{

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct HelpSystemState
{
	std::unique_ptr<HelpSystem> helpSystem {};
	/// The help texts slot that holds its dialogue, from Start to Shutdown
	ecs::systems::HelpTextSystemInterface* texts {nullptr};
};

HelpSystemState& HelpSystemData()
{
	return openblack::Locator::scriptState::value().Get<HelpSystemState>();
}

/// The did-you-know bubble (Locator::tipBubbleSystem)
ecs::systems::TipBubbleSystemInterface& TipBubbles()
{
	if (!Locator::tipBubbleSystem::has_value())
	{
		std::fputs("help: no tip bubble in the locator (Locator::tipBubbleSystem)\n", stderr);
		std::abort();
	}
	return Locator::tipBubbleSystem::value();
}
} // namespace

HelpSystem::HelpSystem(Info info, Queries queries, Hooks hooks, std::unique_ptr<DialogueText>& dialogue)
    : _taskScriptType(std::move(queries.taskScriptType))
    , _spiritHome(std::move(hooks.spiritHome))
    , _wideScreenHook(std::move(hooks.wideScreen))
    , _snapWideScreen(std::move(hooks.snapWideScreen))
    // made at this point of the construction, into its owner
    , _dialogue(*(dialogue = std::make_unique<DialogueText>(info, DialogueQueries(queries), DialogueHooks(hooks))))
{
}

DialogueText::Queries HelpSystem::DialogueQueries(Queries& queries) const
{
	return {
	    .textEntry = std::move(queries.textEntry),
	    .textVoice = std::move(queries.textVoice),
	    .turn = std::move(queries.turn),
	    .nowMs = std::move(queries.nowMs),
	    .citadelClock = std::move(queries.citadelClock),
	    .skipKey = std::move(queries.skipKey),
	    .playBack = std::move(queries.playBack),
	    .filmPlaying = std::move(queries.filmPlaying),
	    .voiceBankLoaded = std::move(queries.voiceBankLoaded),
	    .advisorsTalking = std::move(queries.advisorsTalking),
	    .isPlaying = std::move(queries.isPlaying),
	    .screenHeight = std::move(queries.screenHeight),
	    .scriptWideScreen = [this]() { return IsScriptWideScreen(); },
	};
}

DialogueText::Hooks HelpSystem::DialogueHooks(Hooks& hooks)
{
	return {
	    .showText = std::move(hooks.showText),
	    .textStarted = std::move(hooks.textStarted),
	    .sayVoice = std::move(hooks.sayVoice),
	    .stopVoicesOnClick = std::move(hooks.stopVoicesOnClick),
	    .interruptAdvisors = std::move(hooks.interruptAdvisors),
	};
}

void HelpSystem::Reset()
{
	_dialogue.ClearAllText();
	_messageSets.sentTurn.fill(0);
	_messageSets.sentCount.fill(0);
	_categoryTurns.fill(0);
	_messageSets.banterCount = 0;
	SetWideScreen(0, 0);
	// the bars go at once, not in a slide
	if (_snapWideScreen)
	{
		_snapWideScreen();
	}
	_dialogue.ResetIcons();
	_dialogue.ResetHistory();
	_helpOn = 1;
	_dialogue.SetSpeakerOverride(SpeakerOverride::None);
}

void HelpSystem::ResetForScriptsRestart()
{
	Reset();
	SpiritHome(2, 0);
	SpiritHome(1, 0);
}

void HelpSystem::TriggerCategory(int32_t category)
{
	// The category's turn is the game turn (unchecked in the original)
	if (category >= 0 && static_cast<size_t>(category) < _categoryTurns.size())
	{
		_categoryTurns.at(static_cast<size_t>(category)) = _dialogue.Turn();
	}
}

uint32_t HelpSystem::GetCategoryTurn(int32_t category) const
{
	return category >= 0 && static_cast<size_t>(category) < _categoryTurns.size()
	           ? _categoryTurns.at(static_cast<size_t>(category))
	           : 0;
}

void HelpSystem::OnDialogueReleased(bool helpScript)
{
	_dialogue.CloseBox();
	// Both spirits sent home first; then, at once (arg 1) when the task is a Help script
	SpiritHome(1, 0);
	SpiritHome(2, 0);
	SetWideScreen(0, 0);
	_dialogue.InterruptAdvisors();
	SpiritHome(1, helpScript ? 1 : 0);
	SpiritHome(2, helpScript ? 1 : 0);
}

void HelpSystem::ReleaseWideScreenOf(uint32_t task)
{
	if (_wideScreen != 0 && task == _wideScreenOwner)
	{
		SetWideScreen(0, 0);
	}
}

void HelpSystem::SpiritHome(int32_t spirit, int32_t arg)
{
	if (_spiritHome)
	{
		_spiritHome(spirit, arg);
	}
}

void HelpSystem::SetWideScreen(int32_t on, uint32_t owner)
{
	if (_wideScreen == on)
	{
		return;
	}
	_wideScreen = on;
	_wideScreenOwner = on != 0 ? owner : 0;
	if (_wideScreenHook)
	{
		_wideScreenHook(on != 0);
	}
	if (TextTracing())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Text: wide screen {} (task {})", on, _wideScreenOwner);
	}
}

HelpSystem* Get()
{
	// nothing without the script state (the unit tests that make no services)
	return openblack::Locator::scriptState::has_value() ? HelpSystemData().helpSystem.get() : nullptr;
}

void Process(const script_control::Vm& vm, uint32_t turn)
{
	// The interface's hand state. (pending) the help system's field-of-view object is not reset: its reader is not ported
	if (Locator::handSystem::has_value())
	{
		Locator::handSystem::value().UpdateInterfaceHandState();
	}
	if (auto* helpSystem = Get(); helpSystem != nullptr)
	{
		helpSystem->ProcessIcons();
	}
	// Both spirits' turn, the evil one first
	if (Locator::advisorSystem::has_value() && Locator::advisorSystem::value().IsLoaded())
	{
		Locator::advisorSystem::value().ProcessTurn();
	}
	tooltips::ProcessTurn();
	// The did-you-know bubble's sign gone or off the screen closes the bubble; else the bubble's life is reset to 3.0
	if (Get() != nullptr)
	{
		TipBubbles().ProcessTurn();
	}
	// With the help on: the conditions and the banter
	if (auto* helpSystem = Get(); helpSystem != nullptr && helpSystem->IsHelpSystemOn())
	{
		message_sets::ProcessConditions(*helpSystem);
		if (Locator::dialogueControlSystem::has_value())
		{
			message_sets::ProcessBanter(*helpSystem, Locator::dialogueControlSystem::value(), vm, turn);
		}
		else
		{
			if (log_once::First("game/no-dialogue-control"))
			{
				if (auto logger = spdlog::get("game"); logger != nullptr)
				{
					logger->error("No dialogue control: the advisors' banter is skipped");
				}
			}
		}
	}
}

void Start(ecs::systems::HelpTextSystemInterface& texts, HelpSystem::Info info, HelpSystem::Queries queries,
           HelpSystem::Hooks hooks)
{
	std::unique_ptr<DialogueText> dialogue;
	auto helpSystem = std::make_unique<HelpSystem>(info, std::move(queries), std::move(hooks), dialogue);
	texts.Start(std::move(dialogue));
	auto& state = HelpSystemData();
	state.texts = &texts;
	state.helpSystem = std::move(helpSystem);
}

void Shutdown()
{
	auto& state = HelpSystemData();
	state.helpSystem.reset();
	if (state.texts != nullptr)
	{
		state.texts->Shutdown();
		state.texts = nullptr;
	}
}

} // namespace openblack::help
