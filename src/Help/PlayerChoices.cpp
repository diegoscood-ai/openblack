/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerChoices.h"

#include "Common/PlayerProfiles.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Help/ToolTips.h"

void openblack::help::ApplyPlayerChoices(const player_profiles::Settings& settings, HelpSystem* helpSystem,
                                         DialogueText* dialogue, ecs::systems::CreatureHelpSystemInterface* creatureHelp)
{
	// The help level, which the help system just made starts at 3
	if (helpSystem != nullptr)
	{
		helpSystem->SetHelpLevel(settings.helpLevel);
	}
	// The texts shown, where the newest goes and how fast the player reads them
	if (dialogue != nullptr)
	{
		dialogue->SetPlayerChoices(settings.storyText, settings.textFromBottom, settings.readSpeed);
	}
	// The tooltips: none, the important ones, the intelligent ones or all, the level taken as the profile keeps it (a
	// level out of the four is none of them, as in the original)
	tooltips::SetLevel(settings.toolTips);
	// The good advisor's messages about the player's creature, on or off as the profile keeps them
	if (creatureHelp != nullptr)
	{
		creatureHelp->SetHelpOn(settings.creatureHelp);
	}
}
