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

namespace openblack
{

/// A mesh pack texture's 64 x 64 alpha mask, the one the hand's pick of a tree tests at a triangle's u, v (byte v x 64
/// + u, the ARGB 4444 alpha nibble in the top four bits). Made once as the pack's textures load (PickMaskLoader) and
/// kept in the resource system under the texture's id; nothing draws with it. Wiki: docs/bw1-notes/hand-and-interface.md,
/// "A tree under the cursor"
struct PickMask
{
	std::array<uint8_t, 64 * 64> cells {};
};

} // namespace openblack
