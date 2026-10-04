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

#include "FragMesh.h"

namespace openblack::graphics::world_triangles
{
struct Frame;
}

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
	static void DestroyFragment(entt::entity fragment);
	/// The physics' part of a building that is deleted: its generated broken mesh goes and it leaves the physics
	/// (call it before the entity is destroyed).
	static void OnBuildingDeleted(entt::entity building);
	/// Abode::Draw 0x515F70 for a building with a DestructionMesh: its FragMesh plus the intact model partly built over it
	/// (DrawBuilding) as its DrawMesh; for session Edificios' redraws (repair, life, built) while it has a FragMesh.
	/// Nothing for a building with none.
	static void Redraw(entt::entity building);
	/// MultiMapFixed::RemoveDamage (vt +0x8B8, from Repaired 0x52EC70): the DestructionMesh goes, the whole model is
	/// drawn again (V6's construction draw if it has a site). Nothing for a building with none.
	static void RemoveDamage(entt::entity building);
	/// fn_646D60 / Abode::SetUpPhysOb: buildings forget a hitter (the pass-through pair ends); with a building given,
	/// that building forgets its last hitter.
	static void ForgetHitter(entt::entity hitter, entt::entity building = entt::null);
	/// This frame's draws of every FragMesh (FragMesh::Draw fn_007F7960, pass 0): a broken building's from Abode::Draw
	/// (0x5160A6..0x5160E9: no matrix, its position, the fire's charring grey fn_00730570 and glow 0x730480 or
	/// 0xFFFFFFFF / 0) and a fragment's from Fragment::Draw 0x76EC00 / PhysicsObject::DrawAll 0x646E77 (its world
	/// matrix and translation, the pair of the FragMesh ctor fn_007F6EE0: 0xFFFFFFFF / 0)
	static void AppendFragMeshes(graphics::world_triangles::Frame& out, const FragMesh::FrameLight& frame);
	Buildings() = delete;
};
} // namespace openblack::ecs::physics
