/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdio>
#include <cstdlib>

#include "3D/DayNightClock.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Systems/AdvisorSystemInterface.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/CreatureModeSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TipBubbleSystemInterface.h"
#include "ECS/Systems/TutorialSkipSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

// The game's services in the locator, each by its interface. Those a missing service would turn into a null
// dereference stop the game with a line that names the service. Then what a jump to a camera bookmark is made from
// and the flight to one
namespace openblack
{

/// The day / night clock (Locator::dayNightClock)
inline DayNightClock& TheDayNightClock()
{
	return Locator::dayNightClock::value().Clock();
}

/// The script fade and the cinema bars (Locator::cinematicDirectorSystem)
inline ecs::systems::CinematicDirectorSystemInterface& CinematicDirector()
{
	return Locator::cinematicDirectorSystem::value();
}

/// The miracles (Locator::magicSystem)
inline ecs::systems::MagicSystemInterface& TheMagicSystem()
{
	if (!Locator::magicSystem::has_value())
	{
		std::fputs("game: no miracles service in the locator (Locator::magicSystem)\n", stderr);
		std::abort();
	}
	return Locator::magicSystem::value();
}

/// The objects the scripts hold (Locator::scriptObjects)
inline ecs::systems::ScriptObjectsSystemInterface& TheScriptObjects()
{
	if (!Locator::scriptObjects::has_value())
	{
		std::fputs("game: no script objects service in the locator (Locator::scriptObjects)\n", stderr);
		std::abort();
	}
	return Locator::scriptObjects::value();
}

/// The answer to the start-of-game question (Locator::tutorialSkipSystem)
inline ecs::systems::TutorialSkipSystemInterface& TheTutorialSkipSystem()
{
	if (!Locator::tutorialSkipSystem::has_value())
	{
		std::fputs("game: no start-of-game answer in the locator (Locator::tutorialSkipSystem)\n", stderr);
		std::abort();
	}
	return Locator::tutorialSkipSystem::value();
}

/// The help texts (Locator::helpTextSystem)
inline ecs::systems::HelpTextSystemInterface& TheHelpTextSystem()
{
	if (!Locator::helpTextSystem::has_value())
	{
		std::fputs("game: no help texts in the locator (Locator::helpTextSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpTextSystem::value();
}

/// The dialogue the help system made, which the help texts hold: nullptr before the help system's start
inline help::DialogueText* TheDialogue()
{
	return Locator::helpTextSystem::has_value() ? Locator::helpTextSystem::value().GetDialogue() : nullptr;
}

/// The opening's specials (Locator::introSystem)
inline ecs::systems::IntroSystemInterface& TheIntroSystem()
{
	if (!Locator::introSystem::has_value())
	{
		std::fputs("game: no opening specials in the locator (Locator::introSystem)\n", stderr);
		std::abort();
	}
	return Locator::introSystem::value();
}

/// The villagers drawn in high detail (Locator::highDetailSystem)
inline ecs::systems::HighDetailSystemInterface& TheHighDetailSystem()
{
	if (!Locator::highDetailSystem::has_value())
	{
		std::fputs("game: no high-detail villagers in the locator (Locator::highDetailSystem)\n", stderr);
		std::abort();
	}
	return Locator::highDetailSystem::value();
}

/// The creatures fizzing out of sight and back in (Locator::creatureFizzSystem)
inline ecs::systems::CreatureFizzSystemInterface& TheCreatureFizzSystem()
{
	if (!Locator::creatureFizzSystem::has_value())
	{
		std::fputs("game: no creature fizz in the locator (Locator::creatureFizzSystem)\n", stderr);
		std::abort();
	}
	return Locator::creatureFizzSystem::value();
}

/// The help profile (Locator::helpProfileSystem)
inline ecs::systems::HelpProfileSystemInterface& TheHelpProfileSystem()
{
	if (!Locator::helpProfileSystem::has_value())
	{
		std::fputs("game: no help profile in the locator (Locator::helpProfileSystem)\n", stderr);
		std::abort();
	}
	return Locator::helpProfileSystem::value();
}

/// The player's two advisors (Locator::advisorSystem)
inline ecs::systems::AdvisorSystemInterface& TheAdvisorSystem()
{
	if (!Locator::advisorSystem::has_value())
	{
		std::fputs("game: no advisors in the locator (Locator::advisorSystem)\n", stderr);
		std::abort();
	}
	return Locator::advisorSystem::value();
}

/// The did-you-know bubble over the sign last tapped (Locator::tipBubbleSystem)
inline ecs::systems::TipBubbleSystemInterface& TheTipBubbleSystem()
{
	if (!Locator::tipBubbleSystem::has_value())
	{
		std::fputs("game: no tip bubble in the locator (Locator::tipBubbleSystem)\n", stderr);
		std::abort();
	}
	return Locator::tipBubbleSystem::value();
}

/// The owners of the camera and of the game's speed (Locator::scriptControlSystem)
inline ecs::systems::ScriptControlSystemInterface& TheScriptControlSystem()
{
	if (!Locator::scriptControlSystem::has_value())
	{
		std::fputs("game: no script control in the locator (Locator::scriptControlSystem)\n", stderr);
		std::abort();
	}
	return Locator::scriptControlSystem::value();
}

/// The sky and its clock of day and night (Locator::skySystem)
inline ecs::systems::SkySystemInterface& TheSkySystem()
{
	if (!Locator::skySystem::has_value())
	{
		std::fputs("game: no sky in the locator (Locator::skySystem)\n", stderr);
		std::abort();
	}
	return Locator::skySystem::value();
}

/// What a jump to a camera bookmark is made from: the interface status's place of the last turn, the camera's origin,
/// and whether the game is inside the citadel
inline ecs::systems::BookmarkJumpFrom JumpFromCamera(const Camera& camera)
{
	return {
	    .statusPlace =
	        Locator::handSystem::has_value() ? Locator::handSystem::value().GetSyncedPlace() : map_coords::MapCoords {},
	    .cameraOrigin = camera.GetOrigin(),
	    .insideCitadel = game_clock::IsInsideCitadel(),
	};
}

/// A flight to a camera bookmark is the player's camera's: Creature Mode hands it back first
inline void FlyToBookmark(Camera& camera, const ecs::systems::BookmarkFlight& flight)
{
	if (Locator::creatureModeSystem::has_value())
	{
		Locator::creatureModeSystem::value().Leave();
	}
	camera.GetModel().SetFlight(flight.origin, flight.focus);
}

} // namespace openblack
