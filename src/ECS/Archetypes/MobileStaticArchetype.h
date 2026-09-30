/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>
#include <glm/mat3x3.hpp>

#include "Enums.h"

namespace openblack
{
struct GMobileStaticInfo;
}

namespace openblack::ecs::archetypes
{
class MobileStaticArchetype
{
public:
	static entt::entity Create(const glm::vec3& position, MobileStaticInfo type, float altitude, float xAngleRadians,
	                           float yAngleRadians, float zAngleRadians, float scale);
	/// fn_00608770(pos, info, 0, 0, yAngle, scale) (CREATE_MOBILESTATIC, CHL CREATE): info 8 -> Bonfire::Create with
	/// temperature 100, info 6 -> nothing, otherwise a Rock (info +0x128 == 2) or a MobileStatic
	/// @return entt::null when nothing is created
	static entt::entity CreateFromInfo(const glm::vec3& position, MobileStaticInfo type, float altitude, float yAngleRadians,
	                                   float scale);
	/// fn_00608840 (CREATE_MOBILE_STATIC): info 6 -> GBaseOnly fn_00609340, info 7 -> nothing, otherwise fn_00608770 with
	/// the Y angle and scale; then SetXYZAnglesAndScale(x, y, z, scale) on what was made
	/// @return entt::null when nothing is created
	static entt::entity CreateWithXYZAngles(const glm::vec3& position, MobileStaticInfo type, float altitude,
	                                        float xAngleRadians, float yAngleRadians, float zAngleRadians, float scale);
	/// The rotation of MobileStatic::GetWorldMatrix 0x608DE0: LHMatrix::SetYXZMatrixOnly(yAngle, xAngle, zAngle)
	/// (0x7FAC10), whose rows are openblack's rotation columns
	static glm::mat3 XYZRotation(float xAngleRadians, float yAngleRadians, float zAngleRadians);
	MobileStaticArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
