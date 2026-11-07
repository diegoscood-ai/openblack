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

#include "Common/PlayerProfiles.h"

namespace openblack::simulation
{

/// The parts of the start of the world's data that belong to the game's view and its sound: the timer of the start,
/// the game's menus, the audio, the films and the tips screen that follow it, and the help system
class WorldDataViewInterface
{
public:
	virtual ~WorldDataViewInterface() = default;

	/// A step of the start is complete
	virtual void StartupStep(std::string_view step) = 0;
	/// Makes the game's menus
	virtual void MakeInterface() = 0;
	/// Starts the audio: its banks, the voice table and the music player
	virtual void StartAudio() = 0;
	/// What follows once the audio has started: the pre-intro film and the tips screen
	virtual void ContinueStartupScreens() = 0;
	/// With the info tables read: the game music, the guidance's lists, the spooky voices, the advisors and the help
	/// system
	virtual void StartHelp() = 0;
};

/// Loads every resource of the game's data, in the order of the game: the temple's meshes, the mesh and animation
/// packs, the creatures' meshes, the one-off meshes, the lands, the info tables and the textures, with the view's own
/// parts between them. Gives false when a pack or the info tables do not load.
[[nodiscard]] bool RegisterWorldData(WorldDataViewInterface& view);

/// The hand's mesh from its file. The hand's good or evil shape is drawn into it as the player's alignment moves, so a
/// new game, whose hand starts neutral, reads it afresh as the launch did
void LoadHandMesh();

/// Loads what the tattoos and the marks of the creatures are painted with
void LoadCreatureSkinArt();

/// The options that the player keeps with the profile and that the world uses: the help, the dialogue, the creature's
/// help and the push scrolling
void ApplyPlayerSettings(const player_profiles::Settings& settings);

} // namespace openblack::simulation
