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

#include <glm/vec4.hpp>

#include "Enums.h"

namespace openblack::debug::gui::villager_overlay
{

/// The colour of a villager's label in the villagers' overlay: its town's owner's colour as the land gives it, white
/// for a villager of no town or of a neutral one (whose colour is black); a female's is lighter
[[nodiscard]] glm::vec4 Colour(std::optional<PlayerNames> owner, bool female, int32_t landNumber);

} // namespace openblack::debug::gui::villager_overlay
