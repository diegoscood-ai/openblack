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

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// MapShield (SingleMapFixed; list g_game +0x205CA4, next +0x5C): the world object of a shield spell. MagicShield (0x68
/// bytes) is only a sphere for the tests; PhysicalShield (0x100 bytes) is the solid, growing MSH_S_SOLID_SHIELD dome.
/// Logic: Magic/Objects/MapShield.cpp.
struct MapShield
{
	enum class Kind : uint8_t
	{
		Magic,    ///< MAGIC_TYPE_SHIELD (19), vtable 0x9836EC
		Physical, ///< MAGIC_TYPE_PHYSICAL_SHIELD (20), vtable 0x983F64
	};
	Kind kind {Kind::Magic};
	entt::entity spell {entt::null};         ///< +0x60 the SpellShield (null once dying)
	MagicType magicType {MagicType::Shield}; ///< +0x64 the spell's GMagicShieldInfo
	glm::vec3 position {0.0f};               ///< +0x14 MapCoords: x, z and +0x1C the height above the land
	float objectScale {1.0f};                ///< Object::GetScale (SetScale 0x639200): the mesh's size for the tests

	// ---- PhysicalShield (fn_0072CB70 then the ctor 0x72C9F0) ----
	uint32_t creationTurn {0};  ///< +0x6C
	uint8_t alpha {0xFF};       ///< +0x70 DrawShield: max(40, min(1, strength) x 255)
	glm::mat3 rotation {1.0f};  ///< +0x74 the current matrix's rows (RotY(angle))
	glm::vec3 translation {0.0f};
	float scale {1.0f};                ///< +0xA4
	glm::mat3 previousRotation {1.0f}; ///< +0xA8 the last turn's, for the draw lerp
	glm::vec3 previousTranslation {0.0f};
	float previousScale {1.0f}; ///< +0xD8
	float startScale {1.0f};    ///< +0xDC finalScale x 0.01
	float finalScale {1.0f};    ///< +0xE0 0.017 x radius
	float startSpin {1.0f};     ///< +0xE4 clamp(the spell's curl, -3, 3) rad/s
	float endSpin {1.0f};       ///< +0xE8 +-0.15 with the start's sign
	float angle {0.0f};         ///< +0xEC
	float bob {0.0f};           ///< +0xF0
	uint32_t fx {0};            ///< +0xF4 SF_PhysicalShieldFX (psys::manager id)
	bool dying {false};         ///< +0xF8
	float dieTime {0.0f};       ///< +0xFC seconds
};

} // namespace openblack::ecs::components
