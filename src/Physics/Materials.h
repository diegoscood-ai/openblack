/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <span>

#include <entt/core/hashed_string.hpp>

#include "Physics/Body.h"

/// The physics materials of Data/PhysicsConstants.txt: one row per kind of thing that moves or is hit
namespace openblack::physics
{

inline constexpr std::size_t k_MaterialRows = 24;

using MaterialTable = std::array<PhysicsData, k_MaterialRows>;

/// The materials when the file can't be read: every one zero, as the game's table is until the file is read
inline constexpr MaterialTable k_NoMaterials {};

/// The materials' id in the resources
inline constexpr entt::hashed_string k_MaterialsId = entt::hashed_string("physics/materials");

/// What the file says, read over a table
struct MaterialsFile
{
	int version {0};
	/// How many rows the file declares
	int rows {0};
	MaterialTable table {};
};

/// PhysicsConstants.txt's bytes read over `base`: the version and the number of rows, then six numbers a row, each
/// clamped to its column's range; the rows the file does not have are copies of row 0
[[nodiscard]] MaterialsFile ParseMaterials(std::span<const uint8_t> bytes, const MaterialTable& base);

} // namespace openblack::physics
