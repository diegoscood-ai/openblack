/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MistArchetype.h"

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/FrameAnim.h"
#include "3D/LH3DRandom.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity MistArchetype::Create(const glm::vec3& position, float altitude, uint32_t colour, float size, float k)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();

	// CallVirtualFunctionsForCreation 0x606420: the LH3DObject at (x, GetAltitude + relY, z)
	glm::vec3 world = position;
	world.y = altitude;
	if (Locator::terrainSystem::has_value())
	{
		world.y += Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	registry.Assign<Transform>(entity, world, glm::mat3(1.0f), glm::vec3(size));

	Mist mist {};
	mist.size = size;
	mist.colour = colour;
	mist.edgeShrink = k != 1.0f;
	mist.k = mist.edgeShrink ? k : 3.0f;
	// LH3DMist ctor 0x7F9560 (0x7F95DC..0x7F95FB): the animation counter starts at ftol(Random(0, 16)) & 15, LH3D's own
	// random (graphics::lh3d::Random), not the game's
	mist.counter = graphics::frame_anim::MistStartCounter(graphics::lh3d::Random(0.0f, 16.0f));
	mist.counterRemainder = 0.0f;
	registry.Assign<Mist>(entity, mist);
	return entity;
}
