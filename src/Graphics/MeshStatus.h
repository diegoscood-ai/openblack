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

#include <optional>

// The parts of a model shown by an object's status. Each submesh of a model has a status number; an object draws every
// submesh whose status is no more than its own. Only a graveyard has a status above 0: its graves stage, so its graves
// appear one by one with its burials. Pure: values only.
namespace openblack::graphics::mesh_status
{

/// Whether an object with this status draws a submesh with that status
[[nodiscard]] constexpr bool ShownBy(uint32_t subMeshStatus, uint32_t objectStatus)
{
	return subMeshStatus <= objectStatus;
}

/// Whether a draw takes a submesh with this status. Without `statusParts` it is the common draw of every object, status
/// 0 only; with it, the draw over an object of that status of the parts its status adds, 1 to `statusParts`. Together
/// the two draws take what ShownBy takes.
[[nodiscard]] constexpr bool Drawn(uint32_t subMeshStatus, std::optional<uint8_t> statusParts)
{
	if (!statusParts.has_value())
	{
		return subMeshStatus == 0;
	}
	return subMeshStatus >= 1 && ShownBy(subMeshStatus, *statusParts);
}

} // namespace openblack::graphics::mesh_status
