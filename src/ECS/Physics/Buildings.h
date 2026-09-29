/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

namespace openblack::ecs::physics
{
struct PhysicsObject;

/// Buildings broken by thrown rocks (Abode::ReactToPhysicsImpact 0x406240, ApplyEffectsDueToPhysicalDestruction
/// 0x406640) and the fragments knocked off them (Fragment 0x76EB20..0x76F3F0).
class Buildings
{
public:
	/// Object::PhysicallyDestroysAbodes: rocks (physics row 3) and the toy with row 20.
	[[nodiscard]] static bool PhysicallyDestroysAbodes(entt::entity entity);
	/// Abode::ReactToPhysicsImpact on the building's proxy. Returns false when the building is gone.
	static bool ReactToPhysicsImpact(entt::entity building, PhysicsObject& po);
	/// Fragment::EndPhysics: a big piece (area > 9) whose building still stands becomes its rubble. Returns the entity
	/// that stays (entt::null when it merged).
	static entt::entity FragmentEndPhysics(entt::entity fragment, const PhysicsObject& po);
	/// Fragment::ProcessTimer, every game turn: a fragment vanishes when its time is up.
	static void ProcessTurn();
	/// The mesh the building's physics body is built from (its intact one while it is drawn broken).
	[[nodiscard]] static entt::id_type BodyMesh(entt::entity entity, entt::id_type drawn);
	static void DestroyFragment(entt::entity fragment);
	Buildings() = delete;
};
} // namespace openblack::ecs::physics
