/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "Physics/DamageMesh.h"

namespace openblack::graphics::world_triangles
{
struct Frame;
}

namespace openblack
{
enum class PlayerNames : uint8_t;
}

namespace openblack::ecs
{
struct PhysicsEntry;
}

namespace openblack::ecs::systems
{
/// One damage mesh draw of a frame (openblack, for the draw snapshot): taken with the game data (SnapshotDamageMeshes),
/// drawn without it (AppendDamageMeshes). The damage mesh is shared, not copied: once in a component it is never changed
/// (an impact or a merge makes a new one), so the draw may hold it while the game goes on.
struct DamageMeshDraw
{
	std::shared_ptr<const openblack::physics::damage::Mesh> mesh;
	/// a fragment's world matrix (its draw pose while it flies); none for a broken building's (its triangles are in the
	/// world)
	std::optional<glm::mat4> world;
	/// damage::Mesh::ObjectLight(position, tint, tintSpecular, frame) of the snapshot's frame
	openblack::physics::damage::Mesh::DrawLight light;
	/// what that light was made from, for a light measured again with another FrameLight
	glm::vec3 position {0.0f};
	uint32_t tint {0xFFFFFFFFu};
	uint32_t tintSpecular {0};
};

/// Buildings broken by thrown rocks and the fragments knocked off them (Locator::buildingDamageSystem).
class BuildingDamageSystemInterface
{
public:
	virtual ~BuildingDamageSystemInterface() = default;

	/// A hit on the building's proxy. Returns false when the building is gone.
	virtual bool ReactToImpact(entt::entity building, PhysicsEntry& entry) = 0;
	/// A blow on a building that no body gives (a creature's kick): it strikes along its velocity through its point
	struct BodilessBlow
	{
		glm::vec3 point;
		glm::vec3 velocity;
	};
	/// Blows that break pieces off a built building with no body hitting it, as a creature's kick does, each as wide as
	/// `radius`: they strike its broken model, made from the intact model when it has none, else a copy of it (no test
	/// of how far it has been repaired, as a body's blow makes); every piece flies as a body's blow sends one. Nothing
	/// lost: the broken model goes and the building is drawn whole. Something lost: the building takes the damage of a
	/// physical impact by `hitter` for `player`, then is drawn broken. Returns false when the building is gone
	virtual bool StrikeWithoutBody(entt::entity /*building*/, entt::entity /*hitter*/, std::optional<PlayerNames> /*player*/,
	                               float /*radius*/, std::span<const BodilessBlow> /*blows*/)
	{
		return true;
	}
	/// A fragment's own end of physics, once its flying-object reactions are gone: first the end of any object (back in
	/// the map cells, or deleted outside them), then a big piece (area > 9) whose building is still available and broken
	/// becomes that building's rubble, its corners through the matrix given (the body's rows and centre;
	/// damage::Mesh::Merge), and goes; otherwise it forgets its building. Returns the entity that stays (entt::null when
	/// it merged or was deleted).
	virtual entt::entity PieceAtRest(entt::entity fragment, const glm::mat3& rows, glm::vec3 centre) = 0;
	/// Every game turn: a fragment vanishes when its time is up.
	virtual void ProcessTurn() = 0;
	virtual void DestroyFragment(entt::entity fragment) = 0;
	/// entt's on_destroy<BuildingDamage> (Registry::Destroy, Remove, Reset): the damage mesh's generated model out of the
	/// mesh cache at the real destruction of a deleted building (dead list: the zombie's body leaves at the start of the
	/// next turn), and on_destroy<Fragment>: a fragment's model too, as a land change clears the registry with fragments
	/// still about. Connected once per registry, from abodes::ConnectDrawMeshListener (idempotent).
	virtual void ConnectDamageListener() = 0;
	/// The draw of a damaged building: its damage mesh plus the intact model partly built over it as its DrawMesh; for
	/// the abodes' redraws (repair, life, built) while it has a damage mesh.
	/// Nothing for a building with none.
	virtual void Redraw(entt::entity building) = 0;
	/// Once repaired: the damage mesh goes, the whole model is drawn again (the construction draw if it has a site).
	/// Nothing for a building with none.
	virtual void RemoveDamage(entt::entity building) = 0;
	/// Buildings forget a hitter (the pass-through pair ends); with a building given, only that building does, and with
	/// no hitter it forgets whatever hit it last.
	virtual void ForgetHitter(entt::entity hitter, entt::entity building = entt::null) = 0;
	/// This frame's draws of every damage mesh (first pass): a broken building's (no matrix, its position, the fire's
	/// charring grey and glow or 0xFFFFFFFF / 0) and a fragment's (its world matrix and translation, the damage mesh's
	/// default 0xFFFFFFFF / 0)
	/// = SnapshotDamageMeshes + AppendDamageMeshes(out, draws), in one thread
	/// `turnTime`: the turn and its fraction the fire glow is computed at (SnapshotDamageMeshes)
	virtual void AppendDamageMeshes(graphics::world_triangles::Frame& out,
	                                const openblack::physics::damage::Mesh::FrameLight& frame, float turnTime) = 0;
	/// The game side of AppendDamageMeshes: `out` is cleared and gets this frame's draws, in AppendDamageMeshes' order (the
	/// broken buildings, then the fragments), with their matrix and light (the fire's tint, the land light, the haze)
	virtual void SnapshotDamageMeshes(std::vector<DamageMeshDraw>& out,
	                                  const openblack::physics::damage::Mesh::FrameLight& frame, float turnTime) = 0;
	/// The draw side: damage::Mesh::AppendDraw of every draw, in order. Reads nothing but `draws` and their damage meshes
	virtual void AppendDamageMeshes(graphics::world_triangles::Frame& out, const std::vector<DamageMeshDraw>& draws) = 0;
};
} // namespace openblack::ecs::systems
