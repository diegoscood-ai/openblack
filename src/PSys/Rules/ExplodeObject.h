/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <memory>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The meshes thrown to pieces (PSysExplosion.cpp of the original): the queues fn_00681260 / fn_006812B0 fill, the
// always-on EXPLODE_OBJECT effect (SF_ExplodeObject, PARTICLE_TYPE 23) of PSysUtilityPSys that empties them once a turn
// (fn_006717F0 from PSysGlobal::GameLoopEnd 0x68F5B0 -> fn_006721B0), its rule UR_ExplodeObject
// (ModifyAtomCollection 0x6814E0 -> ExplodeMesh 0x6807B0) and the pieces' draw (RenderParticleGJMesh::DrawAt 0x67C150,
// here mesh atoms of PSys/Creators/Mesh.cpp). Wiki: docs/bw1-notes/miracles.md, "Explosión de rayo".

namespace openblack::psys
{
struct Atom;

namespace explode_object
{

/// What ExplodeMesh reads of an LH3DMesh: the sub-meshes (LH3DMesh +0xC count, +0x10 table) with their flags (+0) and
/// primitives (+4 count, +8 table); of each primitive its vertices (+0x14, 32 bytes: position, uv, normal), its
/// triangles (+0x18 count, +0x1C uint16 x 3) and its material (+0). Read from the mesh's own L3D data.
struct SourcePrimitive
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	std::vector<std::array<uint16_t, 3>> triangles;
};
struct SourceSubMesh
{
	uint32_t flags {0}; ///< the L3D sub-mesh flags as the original keeps them (0x20000000: LOD 0; 0x3F0: the status)
	std::vector<SourcePrimitive> primitives;
};
struct SourceMesh
{
	entt::id_type meshId {0}; ///< the resource the pieces take their materials from (the same sub-mesh / primitive)
	std::vector<SourceSubMesh> subMeshes;
};

/// The LH3DMesh of MeshPack[index] (LH3DMesh::MeshPack 0xE9FE34: AllMeshes.g3d), read once; nullptr if it is not there
[[nodiscard]] std::shared_ptr<const SourceMesh> PackMesh(uint32_t index);

/// fn_006812B0's 0x48 bytes: {mesh, world matrix (LHMatrix, 0x30), origin, speed, spread}
struct QueuedMesh
{
	std::shared_ptr<const SourceMesh> mesh;
	glm::mat3 axes {1.0f};     ///< the LHMatrix rows as columns: world = position + axes x local
	glm::vec3 position {0.0f}; ///< the LHMatrix translation (+0x24)
	glm::vec3 origin {0.0f};   ///< the point the pieces fly away from
	float speed {0.0f};
	float spread {0.0f}; ///< +0x44: stored, read by nobody (ExplodeMesh does not get it)
};

/// fn_006812B0(LH3DObject, matrix, origin, speed, spread, second): an object without a mesh (vt 0xF8) is not queued.
/// `second` != 0 queues in 0xD4E308 (UR_ExplodeObject2's), else in 0xD4E320 (UR_ExplodeObject's)
void QueueMesh(std::shared_ptr<const SourceMesh> mesh, const glm::mat3& axes, const glm::vec3& position, const glm::vec3& origin,
               float speed, float spread, bool second = false);
/// fn_00681260(object, origin, speed, spread, second): the object's 3D object (+0x40) with its GetWorldMatrix (vt 0x63C).
/// False when the object has none or its mesh is not one of the pack's.
bool QueueObject(entt::entity object, const glm::vec3& origin, float speed, float spread, bool second = false);
/// The entries waiting in a queue (for the tests and the traces)
[[nodiscard]] size_t QueuedCount(bool second = false);

/// fn_006717F0 (from PSysGlobal::GameLoopEnd 0x68F5B0 -> fn_006721B0, once a turn): the EXPLODE_OBJECT effect made if
/// there is none (fn_006718E0: PSysInterface::Create(NULL, 23, (0, 0, 0), (0, 0, 0), 1.0, 0)) and processed (vt 0x100);
/// once it says 5 (finished) it is deleted (fn_006718C0) and made again on the next turn
void GameLoopEnd();
/// PSysGlobal::OnClearMap 0x68F820: fn_006811D0 / fn_00681200 empty the two queues (the effect goes with the PSys)
void Clear();

/// The ExplodeMesh 0x6807B0 pieces of one primitive: lists of triangle indices, in the order they are made
[[nodiscard]] std::vector<std::vector<uint32_t>> SplitPrimitive(const SourcePrimitive& primitive, int maxTrigsPerFrag);
/// ExplodeMesh 0x680F34..0x6810AC: the velocity of a piece whose centroid is `centre`: d = centre - origin set to
/// `speed` long (if not 0), r (PSysRandR3) set to |d| x RandomFactor long (if not 0), then (d.x + r.x, d.y + r.y / 2,
/// d.z + r.z)
[[nodiscard]] glm::vec3 PieceVelocity(const glm::vec3& centre, const glm::vec3& origin, float speed, float randomFactor,
                                      glm::vec3 random);

/// A piece atom (its RenderParticleGJMesh +0x128): the generated mesh it is drawn with (0 without the renderer)
struct Piece
{
	Piece() = default;
	Piece(const Piece&) = delete;
	Piece(Piece&&) = delete;
	Piece& operator=(const Piece&) = delete;
	Piece& operator=(Piece&&) = delete;
	~Piece(); ///< the generated mesh goes with the atom
	entt::id_type meshId {0};
	uint32_t triangles {0};
};
/// The piece an atom carries, nullptr for the other atoms
[[nodiscard]] const Piece* PieceOf(const Atom& atom);

/// RenderParticleGJMesh::DrawAt 0x67C150 0x67C175..0x67C1F6 (its +0x21, set by ExplodeMesh 0x680B9B): the DrawData
/// colour (0xAARRGGBB) times the land light under the drawn position (fn_00801C90), byte by byte, alpha included
[[nodiscard]] uint32_t LitColour(uint32_t argb, const glm::vec3& position);
/// fn_00801C90 0x801CB8..0x8020F7: the land light under a point (0xAARRGGBB, alpha 0xFF)
[[nodiscard]] uint32_t LandLight(const glm::vec3& position);

/// RegisterExplosionRules (Rules/Explosion.cpp) calls it: UR_ExplodeObject, UR_ExplodeObject2
void RegisterRules();

} // namespace explode_object
} // namespace openblack::psys
