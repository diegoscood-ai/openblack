/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MobileStaticArchetype.h"

#include <cmath>

#include <glm/gtx/euler_angles.hpp>

#include "AbodeArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MobileStaticArchetype::Create(const glm::vec3& position, MobileStaticInfo type, float altitude,
                                           float xAngleRadians, float yAngleRadians, float zAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(type));

	glm::vec3 offset(0.0f, altitude, 0.0f);

	// MobileStatic::GetWorldMatrix (0x608DE0): position (x, GetAltitude + altitude, z) and
	// LHMatrix::SetYXZMatrixOnly(yAngle, xAngle, zAngle) (0x7FAC10), whose rows are openblack's rotation columns.
	const float ca = std::cos(yAngleRadians);
	const float sa = std::sin(yAngleRadians);
	const float cb = std::cos(xAngleRadians);
	const float sb = std::sin(xAngleRadians);
	const float cc = std::cos(zAngleRadians);
	const float sc = std::sin(zAngleRadians);
	const glm::mat3 rotation(glm::vec3(ca * cc - sa * sb * sc, -cb * sc, sa * cc + ca * sb * sc),
	                         glm::vec3(sa * sb * cc + ca * sc, cb * cc, sa * sc - ca * sb * cc),
	                         glm::vec3(-sa * cb, sb, ca * cb));
	registry.Assign<Transform>(entity, position + offset, rotation, glm::vec3(scale));
	registry.Assign<Mobile>(entity);
	registry.Assign<MobileStatic>(entity, type);
	const auto resourceId = resources::HashIdentifier(info.meshId);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));

	return entity;
}
