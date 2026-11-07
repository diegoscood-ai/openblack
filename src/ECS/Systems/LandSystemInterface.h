/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "LandLoad.h"

namespace openblack::ecs::systems
{

/// The parts of a change of land that belong to the game in front of the player rather than to the world: the camera
/// help icons by the hand, the scripts' restart and the game loop's turn clock
class LandViewInterface
{
public:
	virtual ~LandViewInterface() = default;

	/// The map's clear: the camera help icons by the hand faded out, the rotate arrow unturned and the red cross no
	/// longer held back
	virtual void RestartHandIcons() = 0;
	/// The scripts' restart, at a new game and a playground's start, never at a land change
	virtual void ResetScripts() = 0;
	/// The game loop's clock of real time between turns starts again with the new land
	virtual void RestartTurnClock() = 0;
};

/// A view that does nothing, for a world with no player in front of it
class NoLandView final: public LandViewInterface
{
public:
	void RestartHandIcons() override {}
	void ResetScripts() override {}
	void RestartTurnClock() override {}
};

/// The land of the world: a land load in three steps, around the map script's read and the script itself. The resets of
/// the last land, the registry's reset with the player's hand and the camera's projection, then the start of the new
/// land's clock and game loop. The view of the player does its own parts.
class LandSystemInterface
{
public:
	virtual ~LandSystemInterface() = default;

	/// The view that does the parts of the player. With none, the land does no part of a view.
	virtual void SetView(LandViewInterface* view) = 0;

	/// The resets of the last land. playgroundStart: every land but the campaign's first, whose random seeds go to 0.
	/// resets: what the load resets besides the map (land_load::ResetsFor): at a new game (a playground's start is one)
	/// the scripts' state (the view's ResetScripts), the camera's lens and a film; a land change keeps them
	virtual void PrepareNewLand(bool playgroundStart, land_load::Resets resets) = 0;
	/// Every entity goes, but for what outlives the land; the player's hand is made again and the camera's projection set
	virtual void ResetRegistryForNewLand() = 0;
	/// The new land's clock and game loop; startsAfresh: the creatures' map makes its regions again; newGame: a new game
	/// (land_load::Resets::newGame) starts the guidance again
	virtual void StartNewLand(std::string_view land, bool startsAfresh, bool newGame) = 0;
};

} // namespace openblack::ecs::systems
