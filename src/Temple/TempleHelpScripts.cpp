/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHelpScripts.h"

#include <string>

#include <LHVM.h>

#include "CHLApi.h"
#include "ECS/Systems/DialogueControlSystemInterface.h"
#include "Help/HelpSystem.h"
#include "Help/ScriptControl.h"
#include "Locator.h"

namespace openblack::temple_help
{

void GameScripts::Start(std::string_view name)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	help::script_control::StartTempleScript(name, chlapi::ScriptVm());
}

void GameScripts::StopHelp()
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	help::script_control::StopHelp(chlapi::ScriptVm());
}

void GameDialogue::SendAdvisorsHome()
{
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->SpiritHome(2, 0);
		helpSystem->SpiritHome(1, 0);
	}
}

bool GameDialogue::IsControlled() const
{
	const auto* helpSystem = help::Get();
	return Locator::dialogueControlSystem::has_value() &&
	       Locator::dialogueControlSystem::value().IsControlled(helpSystem != nullptr ? helpSystem->GetWideScreenOwner() : 0u);
}

uint32_t GameDialogue::GetOwner() const
{
	return Locator::dialogueControlSystem::has_value() ? Locator::dialogueControlSystem::value().GetOwner() : 0u;
}

bool GameDialogue::IsTempleScript(uint32_t task) const
{
	// The temple's help and special scripts; a task that does not exist is a plain script
	constexpr uint32_t k_TempleScriptTypes = 0x18;
	if (!Locator::vm::has_value())
	{
		return false;
	}
	const auto vm = chlapi::ScriptVm();
	const uint32_t type = vm.taskType ? vm.taskType(task) : 1u;
	return (type & k_TempleScriptTypes) != 0;
}

void GameDialogue::GiveTo(uint32_t task)
{
	if (!Locator::dialogueControlSystem::has_value())
	{
		return;
	}
	const auto* helpSystem = help::Get();
	Locator::dialogueControlSystem::value().Request(task, helpSystem != nullptr ? helpSystem->GetWideScreenOwner() : 0u);
}

void GameDialogue::TakeAway()
{
	if (Locator::dialogueControlSystem::has_value())
	{
		Locator::dialogueControlSystem::value().Reset();
	}
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->GetDialogue().CloseBox();
	}
}

help::DialogueText::Held GameDialogue::HoldText() const
{
	const auto* helpSystem = help::Get();
	return helpSystem != nullptr ? helpSystem->GetDialogue().Hold() : help::DialogueText::Held {};
}

void GameDialogue::RestoreText(const help::DialogueText::Held& held)
{
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->GetDialogue().Restore(held);
	}
}

void GameDialogue::ShowTextAgain()
{
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->GetDialogue().ShowAgain();
	}
}

void GameScripts::Stop(std::string_view name)
{
	if (!Locator::vm::has_value())
	{
		return;
	}
	Locator::vm::value().StopScripts([name](const std::string& script, const std::string&) { return script == name; });
}

bool HelpSystemOn()
{
	const auto* helpSystem = help::Get();
	return helpSystem != nullptr && helpSystem->IsHelpSystemOn();
}

} // namespace openblack::temple_help
