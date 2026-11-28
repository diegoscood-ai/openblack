/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AppleBarrelSystem.h"

#include <utility>
#include <vector>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/GameDistance.h"
#include "ECS/Animations.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/AppleBarrelRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
const entt::id_type k_BarrelModel = resources::HashIdentifier(MeshId::Z_AppleBarrel);
const entt::id_type k_WormModel = resources::HashIdentifier(MeshId::AnimalWorm);
} // namespace

std::optional<entt::entity> AppleBarrelSystem::CarryingStore()
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto player =
	    Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;

	// The pits are measured from the player's temple
	std::optional<map_coords::MapCoords> temple;
	registry.Each<const components::Temple>([&temple, &registry, player](entt::entity entity, const components::Temple& found) {
		if (!temple.has_value() && found.owner == player && !registry.AnyOf<components::Unavailable>(entity))
		{
			temple = ecs::object::MapCoordsOf(entity);
		}
	});
	const auto from = apple_barrel::MeasuredFrom(temple);

	// The storage pit of each of the player's towns, the towns in the order the player got them
	std::vector<entt::entity> stores;
	std::vector<float> distances;
	for (const auto town : ecs::map_cells::TownsOf(player))
	{
		if (const auto store = ecs::town_queries::GetStoragePit(town); store != entt::null)
		{
			stores.push_back(store);
			distances.push_back(game_math::GetDistanceInMetres(from, ecs::object::MapCoordsOf(store)));
		}
	}
	const auto nearest = apple_barrel::NearestStore(distances);
	return nearest.has_value() ? std::optional(stores[*nearest]) : std::nullopt;
}

void AppleBarrelSystem::Draw(uint32_t frameGameMs)
{
	_drawn.reset();
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto store = CarryingStore();
	if (!store.has_value())
	{
		return;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_BarrelModel) || !meshes.Contains(k_WormModel))
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* transform = registry.TryGet<const components::Transform>(*store);
	if (transform == nullptr)
	{
		return;
	}

	// The barrel stands at the pit's edge on the land, unturned and at its own size
	const auto place =
	    apple_barrel::BarrelPlace(ecs::object::MapCoordsOf(*store), ecs::map_cells::detail::YAngleOf(transform->rotation),
	                              ecs::object::Get2DRadius(*store));
	const auto ground = apple_barrel::GroundCoords(place);
	const glm::vec3 barrel {map_coords::ToMetres(place.x),
	                        Locator::terrainSystem::value().GetHeightAt(map_coords::ToMetres(ground)),
	                        map_coords::ToMetres(place.z)};
	const auto& point = meshes.Handle(k_BarrelModel)->GetChimneyPos();
	const auto worm = apple_barrel::WormSpot(barrel, point.value_or(glm::vec3(0.0f)));

	// The worm wriggles on by the frame's game milliseconds
	const auto& animations = Locator::resources::value().GetAnimations();
	if (const auto clip = ecs::ClipId(static_cast<uint32_t>(AnimId::AWorm)); animations.Contains(clip))
	{
		const auto handle = animations.Handle(clip);
		_wormPlace = apple_barrel::StepWorm(_wormPlace, frameGameMs, handle->GetPlayTime(), handle->IsLooping());
	}
	_drawn = Drawn {.store = *store, .barrel = barrel, .worm = worm, .wormPlace = _wormPlace};
}
