/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StreetLanternArchetype.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "3D/AllMeshes.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/GUtilsDistance.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/ObjectCreationIndex.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

namespace
{
/// The map cell (MapCoords::ToMap 0x603430: the high words of the 1/6553.6 fixed point, 10 units a cell)
glm::ivec2 MapCell(const glm::vec3& position)
{
	return ecs::map_coords::CellOf(position);
}

/// MapCoords::FindType(OBJECT_TYPE_MOBILE_STATIC) 0x6045C0 walked with GUtils::GetDistanceInMetres 0x74CD70: whether
/// an object of type 0x1C in the position's map cell is less than 0.5 m away (in x and z). Every GMobileStaticInfo has
/// that type (info.dat), so it is anything made from one: rocks and mobile statics, bonfires, lanterns, dead trees.
bool MobileStaticWithinHalfMetre(const glm::vec3& position)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto cell = MapCell(position);
	bool found = false;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		if (found || !registry.AnyOf<MobileStatic, StreetLantern, DeadTree>(entity) || MapCell(transform.position) != cell)
		{
			return;
		}
		// GUtils::GetDistanceInMetres 0x74CD70 (the table hypotenuse 0x74F680 on the two MapCoords) against 0.5 m
		found = gutils::GetDistanceInMetres(transform.position, position) < 0.5f;
	});
	return found;
}
} // namespace

entt::entity StreetLanternArchetype::Create(const glm::vec3& position, MobileStaticInfo info)
{
	// GStreetLantern::Create 0x7346E0: nothing if another MobileStatic is within 0.5 m
	if (MobileStaticWithinHalfMetre(position))
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	// +0x58 = (info != GMobileStaticInfo[7]); CallVirtualFunctionsForCreation 0x734810: mesh 148 (MSH_B_CAMPFIRE) for a
	// country lantern, else 398 (MSH_O_TOWNLIGHT), LH3DObject::SetPosition((x, GetAltitude + y, z), angle 0, scale 1)
	// and the light fn_00823240(that point, +0x58). It also creates the sound tag of sample 0x93 (fn_0071E8C0), for
	// either kind of lantern: the looping G_Lantern_01 that audio::lantern_sounds::ProcessTurn starts at night.
	const bool country = info != MobileStaticInfo::StreetLantern;
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	const auto resourceId = resources::HashIdentifier(country ? MeshId::BuildingCampfire : MeshId::ObjectTownLight);
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(1));
	registry.Assign<StreetLantern>(entity, country);
	registry.Assign<LanternLight>(entity, static_cast<uint8_t>(country ? 1 : 0));
	return entity;
}
