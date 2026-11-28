/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandPrintMarks.h"

#include <span>

#include <glm/vec2.hpp>

#include "3D/BlockTextureQueue.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

bool ecs::land_print_marks::IsShadowClass(const Registry& registry, entt::entity object)
{
	const bool shadowClass =
	    registry.AnyOf<Tree, Abode, Feature, BigForest>(object) || registry.AnyOf<Fixed, MobileStatic, MobileObject>(object);
	const bool noShadowClass = registry.AnyOf<Pot, DeadTree, Field, Villager, Creature, Hand, Alpha>(object) ||
	                           registry.AnyOf<AnimatedStatic, Flowers, TempleInteriorPart>(object);
	return shadowClass && !noShadowClass;
}

bool ecs::land_print_marks::HasFootprintBit(const Registry& registry, entt::entity object)
{
	if (registry.AnyOf<Field, AnimatedStatic, WorshipSite, CitadelHeart>(object))
	{
		return true;
	}
	const auto* abode = registry.TryGet<const Abode>(object);
	return abode != nullptr && abode->footprintOnTexture;
}

ecs::land_print_marks::TextureBits ecs::land_print_marks::TextureBitsOf(const Registry& registry, entt::entity object)
{
	const auto* abode = registry.TryGet<const Abode>(object);
	const bool built = abode == nullptr || (abode->buildFlags & Abode::k_Built) != 0;
	// a building made not built keeps every fixed object's shadow until its first insert is done
	const bool beingMade = !built && !abode->footprintOnTexture;
	return {
	    .shadow = IsShadowClass(registry, object) && (built || beingMade),
	    .footprint = HasFootprintBit(registry, object),
	};
}

void ecs::land_print_marks::MarkBlocksUnder(entt::entity object)
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	const auto* transform = registry.TryGet<const Transform>(object);
	const auto* mesh = registry.TryGet<const Mesh>(object);
	if (transform == nullptr || mesh == nullptr)
	{
		return;
	}
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(mesh->id))
	{
		return;
	}
	const auto l3d = meshes.Handle(mesh->id);
	const auto box = l3d->GetBoundingBox();
	// the footprint the land's footprint pass prints for the mesh (its first one), as the mesh's own
	std::span<const glm::vec2> corners;
	if (l3d->ContainsLandscapeFeature() && !l3d->GetFootprints().empty())
	{
		corners = l3d->GetFootprints().front().corners;
	}
	const auto bits = TextureBitsOf(registry, object);
	const block_texture::MarkingObject marking {
	    .position = {transform->position.x, transform->position.z},
	    .yAngle = map_cells::detail::YAngleOf(transform->rotation),
	    .scale = object::GetScale(object),
	    .shadow = bits.shadow,
	    .footprint = bits.footprint,
	    .boxCentre = box.Center(),
	    .boxHalfSize = object::HalfExtents(box),
	    .footprintCorners = corners,
	};
	if (const auto range = block_texture::BlocksMarkedBy(marking))
	{
		Locator::terrainSystem::value().MarkBlocksChanged(range->min, range->max);
	}
}
