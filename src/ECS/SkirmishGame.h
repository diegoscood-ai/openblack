/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// Whether the game in play is a skirmish (RegistryContext::skirmish): only the skirmish box's Start Game starts one.
/// What a skirmish changes is listed on docs/wiki/interface/skirmish.md.
namespace openblack::ecs
{

/// The game in play is a skirmish; false without a registry
[[nodiscard]] bool IsSkirmishGame();

} // namespace openblack::ecs
