/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>

namespace openblack::ecs::components
{

/// A thing that shows only as a print of its model on the land, such as the holder of a singing stone. Its model is not
/// drawn (components::NotDrawn) and casts no shadow on the land. The land's own list of prints takes its model's print
/// where the thing was made, with no turn and no scale, and keeps it there; the newest print is painted first
/// (ecs::land_prints).
/// docs/wiki/world/terrain.md#the-prints-the-16th-step
struct GroundPrint
{
	/// Where the print lies, as the thing was made (land_footprints::GroundPrintPlacement)
	std::array<float, 12> placement {};
	/// The order in which the prints were made
	uint32_t made {0};
	/// Whether the land took a print for it: not when an older print of the same model was already at the same place
	bool printed {true};
};

} // namespace openblack::ecs::components
