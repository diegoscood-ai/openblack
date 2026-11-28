/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{
/// A street light a land script makes (CREATE_STREET_LIGHT): a place and nothing else. It is drawn by nothing, lights
/// nothing, plays no sound and is not on the map; only the Firestorm playground's script makes one
struct StreetLight
{
	/// The order the land's street lights were made in, from 1 (0 for one a saved game's load made again): the game's
	/// list of street lights takes each at its head as it is made
	uint32_t creationStamp {0};
};
} // namespace openblack::ecs::components
