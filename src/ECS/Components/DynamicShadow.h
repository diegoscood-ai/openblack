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

/// An object with its own ShadowInfo (fn_008745A0) updated every frame by fn_00874850, outside the physics list: the
/// launched boat (PetitNavire mode 0, 0x5E11AE / 0x5E0164). graphics::PhysicsShadows draws it like a physics object's.
struct DynamicShadow
{
	/// ShadowInfo +0xC = 0: it falls on objects too (the boat sets it; PhysicsShadows only draws on the land)
	bool onObjects {false};
};

} // namespace openblack::ecs::components
