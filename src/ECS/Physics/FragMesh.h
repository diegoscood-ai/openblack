/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/L3DSubMesh.h"

namespace openblack::ecs::physics
{
/// FragMesh (0x7F6F00..0x7F8A20): a building copied into world-space triangles that a rock can knock pieces out of.
/// Every LOD 0 primitive keeps its material; the triangles know their edge neighbours and a size class (how many
/// times they may still be halved).
class FragMesh
{
public:
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
	};
	struct Triangle
	{
		int group {-1};
		std::array<Vertex, 3> v;
		std::array<int, 3> neighbour {-1, -1, -1}; ///< edge k = (v[k], v[k+1]); -1 = open edge (a side wall is drawn)
		int sizeClass {0};
		int sizeClassRef {0};
	};
	struct Primitive
	{
		graphics::L3DSubMesh::Primitive material;
		std::vector<Triangle> triangles;
	};
	/// A piece that broke off, centred on its centroid, and how it starts flying.
	struct Piece
	{
		std::shared_ptr<FragMesh> mesh;
		glm::vec3 centre;
		glm::vec3 velocity;
		glm::vec3 angularVelocity;
		size_t lifeTriangles {0}; ///< the triangles when the Fragment was constructed (its lifetime: 100 turns each)
	};

	/// FragMesh::FragMesh (0x7F6F00): the LOD 0 sub-meshes of the object's mesh through its world matrix.
	static std::shared_ptr<FragMesh> FromEntity(entt::entity entity);

	/// FragMesh::Impact (0x7F7D40): an infinite cylinder along vel through pos of radius r knocks triangles out (one
	/// flying piece per primitive, split into connected pieces), then everything left without ground contact falls.
	std::vector<Piece> Impact(glm::vec3 pos, glm::vec3 vel, float radius);
	/// SplitUnconnectedGroups (0x76DC30): pieces connected by shared edges; with groundCheck the ones touching the
	/// landscape (y < ground + 0.1) stay, otherwise only group 0 stays. Lone triangles vanish.
	std::vector<Piece> SplitUnconnectedGroups(bool groundCheck, glm::vec3 offset);
	/// fn_7F7230: the triangles still counting / the original count, 0..1.
	float GetRemaining();
	[[nodiscard]] float Remaining() const { return _remaining; }
	[[nodiscard]] glm::vec3 Centroid() const;
	void Translate(glm::vec3 offset);
	[[nodiscard]] float Area() const;
	[[nodiscard]] size_t TriangleCount() const;
	/// Fragment::SetUpPhysOb (0x76EC50): the triangles' distinct vertices (exact match), each with the normal of the
	/// first triangle it was found in.
	void UniqueVertices(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals) const;
	/// Adds a landed piece back as rubble (Fragment::EndPhysics), its triangles through the matrix.
	void Merge(const FragMesh& piece, const glm::mat4& transform);
	/// The drawn mesh (FragMesh::Draw 0x7F7960): each triangle flat, with a back face 0.45 behind it and a side wall on
	/// every open edge, in the space given by worldToLocal. Registers it in the mesh cache; returns its id.
	[[nodiscard]] entt::id_type BuildMesh(const glm::mat4& worldToLocal, const std::string& name,
	                                      std::vector<graphics::L3DSubMesh::GeneratedPrimitive> extra = {}) const;

	entt::entity lastHitter {entt::null};

private:
	void ComputeAdjacency(Primitive& primitive) const;

	std::vector<Primitive> _primitives;
	int _originalTriangleCount {0};
	float _remaining {1.0f};
};
} // namespace openblack::ecs::physics
