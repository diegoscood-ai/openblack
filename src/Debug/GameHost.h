/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <filesystem>

#include "ECS/SavedWorldLoad.h"
#include "Hand/HandTricons.h"
#include "TestbedHost.h"

namespace openblack::graphics
{
struct ObjectListFrame;
}

namespace openblack::debug::gui
{

/// What the debug windows need of the game around them, besides what the testbed's scenarios do: the islands the
/// "Load Island" menu loads, the new player's box, and the readouts of the last frame.
/// The game implements it; the debug windows only see this, so they never reach the game object itself. Its calls
/// keep the names Game gives them.
class GameHost: public testbed_scenarios::TestbedHost
{
public:
	using TestbedHost::ChangeLand;

	/// Loads a land from its map script, as a new game's first land. Whether it loaded
	virtual bool LoadMap(const std::filesystem::path& scriptPath) = 0;
	/// Changes to the land of that map script through the game's own land change, as a map script asks for one.
	/// Whether it loaded
	virtual bool ChangeLand(const std::filesystem::path& scriptPath) = 0;
	/// The new player's box over the game; Cancel shows only when it may and someone has a profile
	virtual void AskForAnotherPlayer(bool canCancel) = 0;
	/// Loads a saved game of the current profile from its slot (Game::LoadSavedGame). Whether it loaded
	virtual bool LoadSavedGame(uint32_t slot) = 0;
	/// Of every class of the last saved game loaded, how many records it held and how many the load made
	[[nodiscard]] virtual const ecs::saved_world_load::Census& GetSavedGameCensus() const = 0;
	/// The wall clock time between the last two turns
	[[nodiscard]] virtual std::chrono::duration<float, std::milli> GetDeltaTime() const = 0;
	/// The strengths of the camera's helper icons by the hand (pitch, rotate, zoom, cross)
	[[nodiscard]] virtual const hand_tricons::Fades& GetHandTriconFades() const = 0;
	/// What the object draw list left out of the last frame's draw; none outside the game's frames
	[[nodiscard]] virtual const graphics::ObjectListFrame* GetObjectListFrame() const = 0;
};

} // namespace openblack::debug::gui
