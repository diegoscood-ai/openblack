/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include "ECS/BuildingDamageWorld.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::components
{
struct BuildingDamage;
}

namespace openblack::ecs::systems
{
/// The game's broken buildings and their fragments. It keeps no state of its own: the damage is in the buildings'
/// BuildingDamage and the fragments' Fragment components
class BuildingDamageSystem final: public BuildingDamageSystemInterface
{
public:
	/// The broken buildings in the game's own world
	BuildingDamageSystem();
	/// The same in the world given
	explicit BuildingDamageSystem(std::unique_ptr<building_world::World> world);

	bool ReactToImpact(entt::entity building, PhysicsEntry& entry) override;
	bool StrikeWithoutBody(entt::entity building, entt::entity hitter, std::optional<PlayerNames> player, float radius,
	                       std::span<const BodilessBlow> blows) override;
	entt::entity PieceAtRest(entt::entity fragment, const glm::mat3& rows, glm::vec3 centre) override;
	void ProcessTurn() override;
	void DestroyFragment(entt::entity fragment) override;
	void ConnectDamageListener() override;
	void Redraw(entt::entity building) override;
	void RemoveDamage(entt::entity building) override;
	void ForgetHitter(entt::entity hitter, entt::entity building = entt::null) override;
	void AppendDamageMeshes(graphics::world_triangles::Frame& out, const openblack::physics::damage::Mesh::FrameLight& frame,
	                        float turnTime) override;
	void SnapshotDamageMeshes(std::vector<DamageMeshDraw>& out, const openblack::physics::damage::Mesh::FrameLight& frame,
	                          float turnTime) override;
	void AppendDamageMeshes(graphics::world_triangles::Frame& out, const std::vector<DamageMeshDraw>& draws) override;

private:
	void EraseMesh(entt::id_type id);
	void RedrawBuilding(entt::entity building, components::BuildingDamage& damage);
	void CreateFragment(const openblack::physics::damage::Mesh::Piece& piece, entt::entity parent);

	std::unique_ptr<building_world::World> _world;
};
} // namespace openblack::ecs::systems
