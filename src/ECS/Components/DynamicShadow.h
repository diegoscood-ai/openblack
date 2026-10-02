/*******************************************************************************
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

/// An object with its own ShadowInfo holder (fn_008745A0) updated every frame by fn_00874850, outside the physics list:
/// the launched boat (PetitNavire mode 0, 0x5E11AE / 0x5E0164). graphics::shadow_list makes its entry.
struct DynamicShadow
{
	/// ShadowInfo +0xC = 0: it falls on objects too (the boat: [holder]+0xC = 0, 0x5E11BE)
	bool onObjects {false};
	/// holder+4: lit by the fixed sun [0xEA1C88] instead of from 15000 above (fn_00874850 0x8748E0; the boat: 1,
	/// 0x5E118C / 0x5E11B6)
	bool useSun {false};
};

} // namespace openblack::ecs::components
