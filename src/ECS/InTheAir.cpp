/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "InTheAir.h"

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"
#include "Physics/LivingRules.h"

using namespace openblack;

namespace living_rules = openblack::physics::living;

bool ecs::ActuallyInTheAir(entt::entity object)
{
	if (!Locator::dynamicsSystem::has_value() || !Locator::dynamicsSystem::value().IsFlying(object))
	{
		return false;
	}
	const auto* entry = Locator::dynamicsSystem::value().Find(object);
	if (entry == nullptr)
	{
		return false;
	}
	const auto centre = entry->body.Centre();
	const glm::vec2 under(map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(centre.x)),
	                      map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(centre.z)));
	const float land = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(under) : 0.0f;
	return living_rules::IsActuallyInTheAir(living_rules::FlyingSpeed(entry->body.velocity), centre.y, land,
	                                        entry->body.Radius());
}
