/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// OneOffSpellSeed (a MobileObject, 0x7C bytes, vtable 0x8F3774; Create 0x72A2F0): the one-shot miracle orb on the
/// land, mesh .\data\spells\meshes\O_Bibble_up.l3d with a 4x4 animated texture. Tapping it puts a fully charged seed
/// in the hand.
struct OneOffSpellSeed
{
	SpellSeedType seedType {SpellSeedType::None}; ///< +0x68
	float scale {1.0f};                           ///< +0x6C (the multiplier passed to the seed)
	entt::entity graphic {entt::null};            ///< +0x70 SpellSeedGraphic (the seed inside; M7)
	float phase {0.0f};                           ///< +0x74 UpdateFrame 0x72A570: 0..16, 18 frames a second
	int powerUp {-1};                             ///< +0x78
	/// Draw 0x518E90 -> fn_00518720 turns the drawn orb every frame: the mesh's +Y (the dome) points at the camera,
	/// about the centre of the mesh's box. Only the drawing uses it (the object's position stays; RenderingSystem):
	/// drawn = facing * vertex + Transform::position + facingOffset
	glm::mat3 facing {1.0f};
	glm::vec3 facingOffset {0.0f};
};

} // namespace openblack::ecs::components
