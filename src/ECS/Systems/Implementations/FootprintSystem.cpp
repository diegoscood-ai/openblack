/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "FootprintSystem.h"

#include <numbers>
#include <optional>
#include <span>
#include <utility>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "3D/CreatureBody.h"
#include "3D/LandIslandInterface.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/FishShoals.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

FootprintSystem::FootprintSystem(DateProvider today)
    : _today(std::move(today))
{
}

void FootprintSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	// the land's draw shows them as they are now, and they fade after it
	_drawn = _trail.prints;
	creature_footprints::Fade(_trail, gameTime.count());
}

void FootprintSystem::Step(entt::entity creature)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	if (!registry.Valid(creature) || !registry.AllOf<CreatureAnimation, Transform>(creature) ||
	    !Locator::terrainSystem::has_value())
	{
		return;
	}
	// a creature, or the Creature Cave's copy of one
	const auto* component = registry.TryGet<const Creature>(creature);
	const auto* copy = registry.TryGet<const CreatureCopy>(creature);
	if (component == nullptr && copy == nullptr)
	{
		return;
	}
	const auto species = component != nullptr ? component->species : copy->species;
	const auto& animation = registry.Get<const CreatureAnimation>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(species);
	if (!rigs.Contains(rigId))
	{
		return;
	}
	// the print is laid under the foot as the body was last drawn, in the world: between its turn poses, smaller in its
	// pen, from before this frame blends it again. A body not drawn from its turn poses (in the hand, in flight, before
	// its first turn) lays it under the foot as its animations pose it, placed by its own drawn matrix
	const bool fromTurnPoses = !animation.drawnInWorld.empty();
	const std::span<const glm::mat4> bones = fromTurnPoses ? animation.drawnInWorld : animation.boneMatrices;
	const auto& points = rigs.Handle(rigId)->actionPoints;
	if (!points.has_value() || points->rightFoot >= bones.size())
	{
		return;
	}
	const std::optional<uint32_t> rightBone = points->rightFoot;
	const auto leftBone = *rightBone < animation.mirror.size() ? animation.mirror[*rightBone] : *rightBone;

	const auto placement = fromTurnPoses ? glm::mat4(1.0f) : ecs::DrawnBodyModel(registry, creature);
	const auto right = creature_footprints::FootOf(creature::PosedBone(*rightBone, bones, placement));
	const auto left = creature_footprints::FootOf(creature::PosedBone(leftBone, bones, placement));
	const bool onLeft = creature_footprints::LeftIsLower(right, left);
	const auto& foot = onLeft ? left : right;

	// the copy belongs to no creature, so its print is the first species' row's, the Giant Ape's, as the original's
	// type for a body with none
	const auto print =
	    creature_footprints::PrintOf(component != nullptr ? component->species : CreatureType::Unknown, IsAprilFools());
	const auto& land = Locator::terrainSystem::value();
	// as big as the size it is drawn at: smaller in its pen; the copy's own size
	const auto size = creature_footprints::PrintSize(
	    component != nullptr ? ecs::creature_pose::DrawnSize(registry, creature) : copy->size, print);
	const auto laid =
	    creature_footprints::MakeFootprint(foot.position, foot.yaw + std::numbers::pi_v<float>, size, print.cell, onLeft,
	                                       [&land](float x, float z) { return land.GetHeightAt(glm::vec2(x, z)); });
	if (!creature_footprints::Add(_trail, laid))
	{
		++_dropped;
	}
	// A foot that goes into the water scares the fish there, print laid or not
	if (creature_footprints::Splashes(foot))
	{
		ecs::SplashWater(foot.position);
	}
}

void FootprintSystem::Reset()
{
	// the prints already laid stay: nothing in the original takes them away, so the last land's fade out on the new one
}

bool FootprintSystem::IsAprilFools() const
{
	if (_aprilFools.has_value())
	{
		return *_aprilFools;
	}
	// the local date is read again each time, so the prints laid change at midnight
	if (!_today)
	{
		return false;
	}
	const auto date = _today();
	return creature_footprints::IsAprilFools(date.month, date.day);
}
