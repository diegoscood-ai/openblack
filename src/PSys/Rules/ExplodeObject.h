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
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "PSys/PSys.h"

// The meshes thrown to pieces (PSysExplosion.cpp of the original): the queues fn_00681260 / fn_006812B0 fill, the
// always-on EXPLODE_OBJECT effect (SF_ExplodeObject, PARTICLE_TYPE 23) of PSysUtilityPSys that empties them once a turn
// (fn_006717F0 from PSysGlobal::GameLoopEnd 0x68F5B0 -> fn_006721B0), its rule UR_ExplodeObject
// (ModifyAtomCollection 0x6814E0 -> ExplodeMesh 0x6807B0) and the pieces' draw (RenderParticleGJMesh::DrawAt 0x67C150,
// here world triangles: gj_mesh below and Graphics/WorldTriangles.h). Wiki: docs/bw1-notes/miracles.md, "Explosión de
// rayo".

namespace openblack::graphics::world_triangles
{
struct Frame;
}

namespace openblack::psys
{
struct Atom;

namespace explode_object
{

/// The pieces drawn as the original does (RenderParticleGJMesh::DrawAt 0x67C150 -> Draw3DWorldTriangle 0x81C090): one
/// CPU-made triangle list per frame (gj_mesh::Build, graphics::world_triangles), Creator::Kind::GJMesh, no bgfx buffer
/// per piece and no handle limit. false keeps the old way (a generated L3D mesh per piece drawn as a mesh atom, behind
/// the GpuBuffersLeft guard) until session sistemas accepts the Renderer.cpp hunk (pieces_shadows_PLAN.md §1.3 d,
/// §3); that code goes in the commit that makes this final
inline constexpr bool k_PiecesAsWorldTriangles = true;

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

/// A piece atom (its RenderParticleGJMesh +0x128) and its GJMesh (ctor 0x67FF20, filled by fn_0057D630): three vertices
/// per triangle (+8 positions about the centroid, 0x680E5A..0x680E89; +0x44 uvs; +0x80 normals, the source's as they
/// are, not turned by the matrix), the triangles b, b + 1, b + 2 (+0x6C), no colours of its own (+0x24 != the vertex
/// count) and the source primitive as its material (GJMesh +0, 0x680C49)
struct Piece
{
	Piece() = default;
	Piece(const Piece&) = delete;
	Piece(Piece&&) = delete;
	Piece& operator=(const Piece&) = delete;
	Piece& operator=(Piece&&) = delete;
	~Piece(); ///< (old way) the generated mesh goes with the atom
	std::shared_ptr<const SourceMesh> source; ///< the mesh of its material
	uint16_t subMesh {0};
	uint16_t primitive {0};
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	uint32_t triangles {0};
	/// (old way, !k_PiecesAsWorldTriangles) the generated mesh it is drawn with (0 without the renderer)
	entt::id_type meshId {0};
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

/// RenderParticleGJMesh::DrawAt 0x67C150 of the exploded pieces, to graphics::world_triangles. Called WHERE THEY ARE
/// DRAWN (after model_light::UpdateFrameLight of the frame): the original draws them at once, no Z object (0x67C150 does
/// not read [0xC0215D]); EXPLODE_OBJECT is Sorted, drawn by fn_006718A0's Draw_(1) (0x6718A9..0x6718AB) from
/// PSysGlobal::DrawLoop 0x68F60C (fn_00672210 0x672228), after GGame::Draw 0x54E00A and before the queue's drain
/// (FinishFrame 0x82F460).
namespace gj_mesh
{
/// The DrawData colour of a drawn atom, 0xAARRGGBB: its colour and its alpha byte (0..255, truncated)
[[nodiscard]] uint32_t DrawDataColour(const Effect::DrawAtom& atom);
/// fn_00679920's PSR matrix of a drawn atom: rotation x scale, the Y axis x the stretch, the translation its position
[[nodiscard]] glm::mat4 DrawMatrix(const Effect::DrawAtom& atom);
/// DrawAt 0x67C150 of one piece with the drawn matrix `model` and the DrawData colour `argb`:
/// - the base colour: argb x the land light under the matrix's translation (+0x21, 0x67C175..0x67C1F6, LitColour);
/// - every vertex through the matrix into the world (0x67C279..0x67C30A); +0x22 (the vertices stuck to the land,
///   0x67C310..0x67C38C) stays 0 for the pieces (ctor fn_006C8A90, 0x6C8AA5);
/// - every vertex of the base colour (+0x24 != the count, 0x67C47F..0x67C4C4), then the model light ([0xC029C0] = 1
///   and one normal per vertex, 0x67C4CE..0x67C6B2): the light [0xEA9E90] through SetInverse of the matrix
///   (0x67C508..0x67C51E), I = fistp(255 n.l) (0x67C61D..0x67C62C), the ambient [0xC39264] (model_light::Apply). The
///   second light [0xD4EC08] (0x67C6C3) is 0;
/// - the alpha table 0xC387C8 when the DrawData alpha byte is not 0xFF (0x67C9B7..0x67C9C0), back to 0xC38728 after
///   the draw (0x67C9F7).
/// Appends one batch (or joins the last one) with `tag`
void AppendPiece(graphics::world_triangles::Frame& out, const explode_object::Piece& piece, const glm::mat4& model,
                 uint32_t argb, const void* tag = nullptr);
/// Every piece of the running effects drawn by `path`, in effect order and, inside, their collections' order
/// (Effect::Collect). Sorted: no tag; Queued / Immediate: tagged with their atom (Effect::DrawAtom::atom), for
/// world_triangles::Submit's `only` at the atom's place among its effect's items
void Build(graphics::world_triangles::Frame& out, DrawPath path);
/// One piece atom (an item of a Queued / Immediate effect whose creator is Kind::GJMesh), tagged with its atom
void BuildAtom(graphics::world_triangles::Frame& out, const Effect::DrawAtom& atom);
} // namespace gj_mesh
} // namespace openblack::psys
