/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::player_profiles
{
struct Settings;
} // namespace openblack::player_profiles

namespace openblack::ecs::systems
{
class CreatureHelpSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::help
{

class DialogueText;
class HelpSystem;

/// The player's options as the help takes them, from their profile: the help level, the texts shown, where the newest
/// goes and how fast the player reads them, the tooltips' level and the creature help. A part not made yet (nullptr)
/// is passed over; the tooltips' level is always taken
void ApplyPlayerChoices(const player_profiles::Settings& settings, HelpSystem* helpSystem, DialogueText* dialogue,
                        ecs::systems::CreatureHelpSystemInterface* creatureHelp);

} // namespace openblack::help
