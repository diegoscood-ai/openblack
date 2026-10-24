/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DamageMesh.h"

#include <cmath>

#include <algorithm>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "3D/LandMorph.h"
#include "Common/GameRandom.h"
#include "Common/ModelInverseSquareRoot.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Graphics/ArgbColour.h"
#include "Graphics/ModelLight.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::physics::damage;

namespace
{
constexpr float k_SameCorner = 0.01f; // vertices closer than this on every axis match
constexpr size_t k_MostListed = 2048; // the most kept or broken triangles, per primitive and impact
constexpr int k_MostGroups = 64;
constexpr float k_BackOffset = 0.45f;      // how far the back copy sits behind the front
constexpr float k_ScaffoldRate = 5.0f;     // one over the length of a scaffold phase
constexpr float k_ScaffoldDrop = -2.0f;    // -2 x half height x (1 - 5 pct)
constexpr uint32_t k_NoTint = 0xFFFFFFFFu; // the default tint of a damage mesh and of a building's draw: none

// The land height under a point, for the anchor test below: a point sample, not the morph
float LandHeightUnder(glm::vec3 p)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z)) : 0.0f;
}

bool Same(glm::vec3 a, glm::vec3 b)
{
	return std::abs(a.x - b.x) < k_SameCorner && std::abs(a.y - b.y) < k_SameCorner && std::abs(a.z - b.z) < k_SameCorner;
}

float LengthSquared(glm::vec3 v)
{
	return glm::dot(v, v);
}

int SizeClass(const Mesh::Triangle& t)
{
	// the shortest squared edge (from 1e6 down), so a long thin triangle is halved no further
	float shortest = 1000000.0f;
	for (size_t i = 0; i < 3; ++i)
	{
		shortest = std::min(shortest, LengthSquared(t.corners.at(i).position - t.corners.at((i + 1) % 3).position));
	}
	return shortest > 32.0f ? 3 : shortest > 8.0f ? 2 : shortest > 2.0f ? 1 : 0;
}

/// The vertices within r of the line through pos along vel (of the point pos when vel is 0).
int CornersNear(const Mesh::Triangle& t, glm::vec3 pos, glm::vec3 vel, float r)
{
	const auto d = LengthSquared(vel) > 0.0f ? glm::normalize(vel) : glm::vec3(0.0f);
	int n = 0;
	for (const auto& v : t.corners)
	{
		const auto w = v.position - pos;
		if (glm::length(w - glm::dot(w, d) * d) < r)
		{
			++n;
		}
	}
	return n;
}

/// Halves the triangle at the middle of its longest edge.
std::array<Mesh::Triangle, 2> Halve(const Mesh::Triangle& t)
{
	int k = 2;
	float longest = LengthSquared(t.corners[0].position - t.corners[2].position);
	if (LengthSquared(t.corners[1].position - t.corners[0].position) > longest)
	{
		k = 0;
		longest = LengthSquared(t.corners[1].position - t.corners[0].position);
	}
	if (LengthSquared(t.corners[2].position - t.corners[1].position) > longest)
	{
		k = 1;
	}
	const auto& a = t.corners[k];
	const auto& b = t.corners[(k + 1) % 3];
	const auto& c = t.corners[(k + 2) % 3];
	const Mesh::Corner m {(a.position + b.position) * 0.5f, (a.uv + b.uv) * 0.5f};
	std::array<Mesh::Triangle, 2> out;
	out[0].corners = {a, m, c};
	out[1].corners = {m, b, c};
	for (auto& o : out)
	{
		o.sizeClass = t.sizeClass - 1;
		o.countedClass = t.countedClass - 1;
	}
	return out;
}

/// Kept (0 vertices inside), broken (3), or halved and tested again; small triangles go by majority.
void SortInto(const Mesh::Triangle& t, glm::vec3 pos, glm::vec3 vel, float r, std::vector<Mesh::Triangle>& kept,
              std::vector<Mesh::Triangle>& broken)
{
	const int n = CornersNear(t, pos, vel, r);
	int c = n;
	if (t.sizeClass <= 0)
	{
		c = n == 1 ? 0 : n == 2 ? 3 : n;
	}
	if (c == 0)
	{
		if (kept.size() < k_MostListed)
		{
			kept.push_back(t);
		}
	}
	else if (c == 3)
	{
		if (broken.size() < k_MostListed)
		{
			broken.push_back(t);
		}
	}
	else
	{
		for (const auto& half : Halve(t))
		{
			SortInto(half, pos, vel, r, kept, broken);
		}
	}
}

/// Two triangles share an edge when at least two of their vertices match.
bool SharesEdge(const Mesh::Triangle& a, const Mesh::Triangle& b)
{
	int matches = 0;
	for (const auto& va : a.corners)
	{
		for (const auto& vb : b.corners)
		{
			if (Same(va.position, vb.position))
			{
				++matches;
				break;
			}
		}
	}
	return matches >= 2;
}

/// (GameRand(201) - 100) x scale on each axis, z drawn first and x last, as the original evaluates the point's
/// arguments right to left (Impact with scale 0.01, SplitUnconnectedGroups with 0.02)
glm::vec3 RandomSpin(float scale)
{
	const auto r = [scale]() {
		const auto n = static_cast<int32_t>(game_random::GameRand(201)) - 100;
		const float v = static_cast<float>(n);
		return v * scale;
	};
	const float z = r();
	const float y = r();
	const float x = r();
	return {x, y, z};
}

} // namespace

std::shared_ptr<Mesh> Mesh::FromEntity(entt::entity entity)
{
	return FromEntity(entity, Locator::entitiesRegistry::value().AllOf<ecs::components::MorphWithTerrain>(entity));
}

std::shared_ptr<Mesh> Mesh::FromEntity(entt::entity entity, bool followsLand)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* meshComponent = registry.TryGet<const ecs::components::Mesh>(entity);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshComponent == nullptr || !meshes.Contains(meshComponent->id))
	{
		return nullptr;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto& transform = registry.Get<const ecs::components::Transform>(entity);
	// objects that follow the landscape are baked with it: v.y - (H0 - H) on each vertex
	const auto ground = land_morph::CurrentAltitude();
	const float originGround = ground(glm::vec2(transform.position.x, transform.position.z));
	auto result = std::make_shared<Mesh>();
	const auto& subMeshes = mesh->GetSubMeshes();
	for (size_t subMeshIndex = 0; subMeshIndex < subMeshes.size(); ++subMeshIndex)
	{
		const auto& subMesh = subMeshes[subMeshIndex];
		// only LOD 0 with status 0
		if ((subMesh->GetFlags().lodMask & 1) == 0 || subMesh->GetFlags().status != 0 || subMesh->IsPhysics())
		{
			continue;
		}
		const auto& positions = subMesh->GetCollisionPositions();
		const auto& uvs = subMesh->GetCollisionUVs();
		const auto& indices = subMesh->GetCollisionIndices();
		const auto& primitives = subMesh->GetPrimitives();
		for (size_t primitiveIndex = 0; primitiveIndex < primitives.size(); ++primitiveIndex)
		{
			const auto& primitive = primitives[primitiveIndex];
			Primitive fp {
			    primitive, {}, {meshComponent->id, static_cast<uint16_t>(subMeshIndex), static_cast<uint16_t>(primitiveIndex)}};
			for (uint32_t i = primitive.indicesOffset; i + 2 < primitive.indicesOffset + primitive.indicesCount; i += 3)
			{
				Triangle t;
				for (uint32_t k = 0; k < 3; ++k)
				{
					const auto index = indices.at(i + k);
					t.corners.at(k).position =
					    transform.position + transform.rotation * (transform.scale * positions.at(index));
					if (followsLand)
					{
						t.corners.at(k).position.y = land_morph::Raised(ground, t.corners.at(k).position, originGround);
					}
					t.corners.at(k).uv = index < uvs.size() ? uvs[index] : glm::vec2(0.0f);
				}
				t.sizeClass = t.countedClass = SizeClass(t);
				fp.triangles.push_back(t);
			}
			result->_trianglesAtCreation += static_cast<int>(fp.triangles.size());
			result->LinkNeighbours(fp);
			result->_primitives.push_back(std::move(fp));
		}
	}
	return result->_trianglesAtCreation > 0 ? result : nullptr;
}

std::shared_ptr<Mesh> Mesh::FromPrimitives(std::vector<Primitive> primitives)
{
	auto result = std::make_shared<Mesh>();
	for (auto& primitive : primitives)
	{
		result->_trianglesAtCreation += static_cast<int>(primitive.triangles.size());
		result->LinkNeighbours(primitive);
		result->_primitives.push_back(std::move(primitive));
	}
	return result;
}

void Mesh::LinkNeighbours(Primitive& primitive) const
{
	auto& tris = primitive.triangles;
	for (auto& t : tris)
	{
		t.neighbours = {-1, -1, -1};
	}
	for (size_t i = 0; i < tris.size(); ++i)
	{
		for (size_t j = 0; j < tris.size(); ++j)
		{
			if (i == j)
			{
				continue;
			}
			for (int k = 0; k < 3; ++k)
			{
				const auto& a = tris[i].corners.at(k).position;
				const auto& b = tris[i].corners.at((k + 1) % 3).position;
				bool hasA = false;
				bool hasB = false;
				for (const auto& v : tris[j].corners)
				{
					hasA = hasA || Same(a, v.position);
					hasB = hasB || Same(b, v.position);
				}
				if (hasA && hasB)
				{
					tris[i].neighbours.at(k) = static_cast<int>(j);
				}
			}
		}
	}
}

glm::vec3 Mesh::Centroid() const
{
	glm::vec3 sum(0.0f);
	size_t n = 0;
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			for (const auto& v : t.corners)
			{
				sum += v.position;
				++n;
			}
		}
	}
	return n > 0 ? sum / static_cast<float>(n) : sum;
}

void Mesh::Translate(glm::vec3 offset)
{
	for (auto& p : _primitives)
	{
		for (auto& t : p.triangles)
		{
			for (auto& v : t.corners)
			{
				v.position += offset;
			}
		}
	}
}

float Mesh::Area() const
{
	float area = 0.0f;
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			area += 0.5f * glm::length(glm::cross(t.corners[1].position - t.corners[0].position,
			                                      t.corners[2].position - t.corners[0].position));
		}
	}
	return area;
}

void Mesh::UniqueVertices(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals) const
{
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			auto n = glm::cross(t.corners[1].position - t.corners[0].position, t.corners[2].position - t.corners[0].position);
			const float len = glm::length(n);
			n = len > 0.0f ? n / len : glm::vec3(0.0f);
			for (const auto& v : t.corners)
			{
				if (std::find(positions.begin(), positions.end(), v.position) == positions.end())
				{
					positions.push_back(v.position);
					normals.push_back(n);
				}
			}
		}
	}
}

std::vector<glm::vec3> Mesh::DustCorners() const
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	UniqueVertices(positions, normals);
	// the first vertex found is only noted
	if (!positions.empty())
	{
		positions.erase(positions.begin());
	}
	return positions;
}

bool Mesh::IsSliver() const
{
	const float area = Area();
	std::vector<glm::vec3> points;
	std::vector<glm::vec3> normals;
	UniqueVertices(points, normals);
	// the hull's radius is at least the farthest vertex, front or back
	float radius = 0.0f;
	for (size_t i = 0; i < points.size(); ++i)
	{
		radius = std::max({radius, glm::length(points[i]), glm::length(points[i] - k_BackOffset * normals[i])});
	}
	return 0.2f * radius > area / (2.0f * radius);
}

size_t Mesh::TriangleCount() const
{
	size_t n = 0;
	for (const auto& p : _primitives)
	{
		n += p.triangles.size();
	}
	return n;
}

void Mesh::Impact(glm::vec3 pos, glm::vec3 vel, float radius, const LaunchPiece& launch)
{
	std::vector<Primitive> remaining;
	for (auto& p : _primitives)
	{
		std::vector<Triangle> kept;
		std::vector<Triangle> broken;
		for (const auto& t : p.triangles)
		{
			SortInto(t, pos, vel, radius, kept, broken);
		}
		if (!kept.empty())
		{
			Primitive k {p.material, std::move(kept), p.source};
			LinkNeighbours(k);
			remaining.push_back(std::move(k));
		}
		if (!broken.empty())
		{
			// the broken triangles of the primitive fly as one piece with the hitter's velocity
			auto piece = std::make_shared<Mesh>();
			Primitive b {p.material, std::move(broken), p.source};
			piece->LinkNeighbours(b);
			piece->_primitives.push_back(std::move(b));
			piece->_trianglesAtCreation = static_cast<int>(piece->TriangleCount());
			const auto c = piece->Centroid();
			piece->Translate(-c);
			const auto constructed = piece->TriangleCount(); // the piece's lifetime is counted before its split
			// its unconnected parts come off it at rest, and fly before it
			piece->SplitUnconnectedGroups(false, c, launch);
			// then it is centred on what is left of it, three GameRand(201) are drawn and dropped (its velocity is the
			// hitter's alone), and its spin is drawn. The draws come whether or not the split left it any triangles
			auto centre = c;
			if (piece->TriangleCount() > 0)
			{
				const auto c2 = piece->Centroid();
				piece->Translate(-c2);
				centre = c + c2;
			}
			for (int i = 0; i < 3; ++i)
			{
				static_cast<void>(game_random::GameRand(201));
			}
			const auto spin = RandomSpin(0.01f);
			if (piece->TriangleCount() > 0)
			{
				launch({piece, centre, vel, spin, constructed});
			}
		}
	}
	_primitives = std::move(remaining);
	// what no longer touches the landscape falls, after every broken piece
	SplitUnconnectedGroups(true, glm::vec3(0.0f), launch);
}

void Mesh::SplitUnconnectedGroups(bool groundCheck, glm::vec3 offset, const LaunchPiece& launch)
{
	struct Ref
	{
		size_t primitive;
		size_t triangle;
	};
	std::vector<Ref> all;
	for (size_t p = 0; p < _primitives.size(); ++p)
	{
		for (size_t t = 0; t < _primitives[p].triangles.size(); ++t)
		{
			_primitives[p].triangles[t].group = -1;
			all.push_back({p, t});
		}
	}
	const auto tri = [&](const Ref& r) -> Triangle& { return _primitives[r.primitive].triangles[r.triangle]; };
	std::vector<int> count;
	for (const auto& start : all)
	{
		if (static_cast<int>(count.size()) >= k_MostGroups)
		{
			break;
		}
		if (tri(start).group != -1)
		{
			continue;
		}
		const int g = static_cast<int>(count.size());
		count.push_back(1);
		tri(start).group = g;
		std::vector<Ref> stack {start};
		while (!stack.empty())
		{
			const auto current = stack.back();
			stack.pop_back();
			for (const auto& other : all)
			{
				if (tri(other).group == -1 && SharesEdge(tri(current), tri(other)))
				{
					tri(other).group = g;
					++count[g];
					stack.push_back(other);
				}
			}
		}
	}
	std::vector<bool> anchored(count.size(), false);
	if (groundCheck)
	{
		for (const auto& r : all)
		{
			const auto& t = tri(r);
			if (t.group < 0)
			{
				continue;
			}
			for (const auto& v : t.corners)
			{
				if (v.position.y < LandHeightUnder(v.position) + 0.1f)
				{
					anchored.at(t.group) = true;
				}
			}
		}
	}
	else if (!anchored.empty())
	{
		anchored[0] = true;
	}
	std::vector<Piece> pieces;
	for (size_t g = 0; g < count.size(); ++g)
	{
		if (anchored[g] || count[g] == 1)
		{
			continue;
		}
		for (const auto& p : _primitives)
		{
			Primitive part {p.material, {}, p.source};
			for (const auto& t : p.triangles)
			{
				if (t.group == static_cast<int>(g))
				{
					auto copy = t;
					copy.countedClass = copy.sizeClass + 1;
					part.triangles.push_back(copy);
				}
			}
			if (part.triangles.empty())
			{
				continue;
			}
			auto piece = std::make_shared<Mesh>();
			piece->LinkNeighbours(part);
			piece->_primitives.push_back(std::move(part));
			piece->_trianglesAtCreation = static_cast<int>(piece->TriangleCount());
			const auto c = piece->Centroid();
			piece->Translate(-c);
			pieces.push_back({piece, c + offset, glm::vec3(0.0f), glm::vec3(0.0f), piece->TriangleCount()});
		}
	}
	// everything not anchored goes: the pieces above, and lone triangles (and groups past the 64th)
	for (auto& p : _primitives)
	{
		std::erase_if(p.triangles, [&](const Triangle& t) { return t.group < 0 || !anchored.at(t.group); });
		LinkNeighbours(p);
	}
	std::erase_if(_primitives, [](const Primitive& p) { return p.triangles.empty(); });
	// each piece in turn: its own loose parts fly first (where they are: it is not centred again), then its spin is
	// drawn and it is launched at rest
	for (auto& piece : pieces)
	{
		piece.mesh->SplitUnconnectedGroups(false, piece.centre, launch);
		piece.angularVelocity = RandomSpin(0.02f);
		launch(piece);
	}
}

int Mesh::SizeClassOf(const Triangle& triangle)
{
	return SizeClass(triangle);
}

float Mesh::GetRemaining()
{
	int n = 0;
	for (const auto& p : _primitives)
	{
		n += static_cast<int>(std::count_if(p.triangles.begin(), p.triangles.end(),
		                                    [](const Triangle& t) { return t.sizeClass == t.countedClass; }));
	}
	const float f = _trianglesAtCreation > 0 ? static_cast<float>(n) / static_cast<float>(_trianglesAtCreation) : 0.0f;
	_remaining = std::clamp(f, 0.0f, 1.0f);
	return _remaining;
}

glm::vec3 Mesh::StuckCorner(const glm::mat3& rows, glm::vec3 centre, glm::vec3 corner)
{
	const glm::vec3& r0 = rows[0];
	const glm::vec3& r1 = rows[1];
	const glm::vec3& r2 = rows[2];
	const float x = ((corner.x * r0.x + corner.z * r2.x) + corner.y * r1.x) + centre.x;
	const float y = ((corner.z * r2.y + corner.y * r1.y) + corner.x * r0.y) + centre.y;
	const float z = ((corner.x * r0.z + corner.z * r2.z) + corner.y * r1.z) + centre.z;
	return {x, y, z};
}

void Mesh::Merge(const Mesh& piece, const glm::mat3& rows, glm::vec3 centre)
{
	// only the piece's first primitive (a piece is made with one)
	if (piece._primitives.empty())
	{
		return;
	}
	auto p = piece._primitives.front();
	for (auto& t : p.triangles)
	{
		for (auto& v : t.corners)
		{
			v.position = StuckCorner(rows, centre, v.position);
		}
		// rubble does not count as building (countedClass stays one above sizeClass)
	}
	LinkNeighbours(p);
	_primitives.push_back(std::move(p));
}

entt::id_type Mesh::BuildMesh(const glm::mat4& worldToLocal, const std::string& name,
                              std::vector<graphics::L3DSubMesh::GeneratedPrimitive> extra) const
{
	const glm::mat3 normalMatrix(glm::transpose(glm::inverse(glm::mat3(worldToLocal))));
	std::vector<graphics::L3DSubMesh::GeneratedPrimitive> primitives;
	for (const auto& p : _primitives)
	{
		graphics::L3DSubMesh::GeneratedPrimitive out;
		out.material = p.material;
		out.material.twoSided = true;
		out.cpuDrawn = true;
		const auto add = [&](glm::vec3 world, glm::vec2 uv, glm::vec3 normal) {
			out.positions.push_back(glm::vec3(worldToLocal * glm::vec4(world, 1.0f)));
			out.uvs.push_back(uv);
			const auto n = normalMatrix * normal;
			out.normals.push_back(LengthSquared(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f));
			return static_cast<uint16_t>(out.positions.size() - 1);
		};
		for (const auto& t : p.triangles)
		{
			// the original flushes every 256 vertices; here a primitive is cut before a sub-mesh's 16-bit limit
			if (out.positions.size() + 18 > 0xFFFF)
			{
				primitives.push_back(std::move(out));
				out = graphics::L3DSubMesh::GeneratedPrimitive {};
				out.material = p.material;
				out.material.twoSided = true;
				out.cpuDrawn = true;
			}
			auto n = glm::cross(t.corners[1].position - t.corners[0].position, t.corners[2].position - t.corners[0].position);
			n = LengthSquared(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
			// front face, and the back face 0.45 behind it
			const auto f0 = add(t.corners[0].position, t.corners[0].uv, n);
			const auto f1 = add(t.corners[1].position, t.corners[1].uv, n);
			const auto f2 = add(t.corners[2].position, t.corners[2].uv, n);
			out.indices.insert(out.indices.end(), {f0, f1, f2});
			const auto back = -0.45f * n;
			const auto b0 = add(t.corners[0].position + back, t.corners[0].uv, -n);
			const auto b1 = add(t.corners[1].position + back, t.corners[1].uv, -n);
			const auto b2 = add(t.corners[2].position + back, t.corners[2].uv, -n);
			out.indices.insert(out.indices.end(), {b2, b1, b0});
			// a side wall on each open edge
			for (int k = 0; k < 3; ++k)
			{
				if (t.neighbours.at(k) != -1)
				{
					continue;
				}
				// quad (k, k+3, k+1) (k+1, k+3, k+4): the front edge lit like the front, the back edge like the back
				const auto& a = t.corners.at(k);
				const auto& b = t.corners.at((k + 1) % 3);
				const auto s0 = add(a.position, a.uv, n);
				const auto s1 = add(b.position, b.uv, n);
				const auto s3 = add(a.position + back, a.uv, -n);
				const auto s4 = add(b.position + back, b.uv, -n);
				out.indices.insert(out.indices.end(), {s0, s3, s1, s1, s3, s4});
			}
		}
		if (!out.indices.empty())
		{
			primitives.push_back(std::move(out));
		}
	}
	// world-space extras (the partly built draw) in the same space
	for (auto& e : extra)
	{
		for (auto& p : e.positions)
		{
			p = glm::vec3(worldToLocal * glm::vec4(p, 1.0f));
		}
		for (auto& n : e.normals)
		{
			const auto m = normalMatrix * n;
			n = LengthSquared(m) > 0.0f ? glm::normalize(m) : glm::vec3(0.0f, 1.0f, 0.0f);
		}
		primitives.push_back(std::move(e));
	}
	if (primitives.empty())
	{
		return 0;
	}
	const auto id = Locator::resources::value().NextGeneratedMeshId("fragmesh", name);
	try
	{
		Locator::resources::value().GetMeshes().Load(id, resources::L3DLoader::FromGeneratedTag {}, name, primitives);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "FragMesh: {}", e.what());
		return 0;
	}
	return id;
}

glm::vec3 Mesh::LightDirection(glm::vec3 light, glm::vec3 position)
{
	// light - position, axis by axis, each stored as a float
	const glm::vec3 d(light.x - position.x, light.y - position.y, light.z - position.z);
	// (y y + z z) + x x into ModelInverseSquareRoot, then each axis times it
	const float inverse = gutils::ModelInverseSquareRoot((d.y * d.y + d.z * d.z) + d.x * d.x);
	return {d.x * inverse, d.y * inverse, d.z * inverse};
}

Mesh::DrawLight Mesh::ObjectLight(glm::vec3 position, uint32_t tint, uint32_t tintSpecular, const FrameLight& frame)
{
	const auto outDirection = LightDirection(frame.light, position);
	DrawLight out {
	    .lit = frame.lit,
	    .direction = outDirection,
	    .ambient = frame.ambient,
	};
	if (!frame.lit)
	{
		return out; // openblack guard: no land light table loaded, the face is drawn unlit
	}
	// the land light's colour and specular at the position
	const auto sample = land_light::At(land_light::CurrentTable(), glm::vec2(position.x, position.z));
	out.colour = sample.diffuse;
	out.specular = sample.specular;
	// tinted unless the tint is the default pair (no tint, no specular)
	if (tint != k_NoTint || tintSpecular != 0)
	{
		out.colour = argb_colour::MultiplyArgbShift8(out.colour, tint);
		out.specular = argb_colour::AddArgbSaturated(out.specular, tintSpecular);
	}
	// the haze at the position tints the colour and returns the new specular
	out.specular =
	    graphics::haze::ApplyObject(frame.haze, graphics::haze::Depth(frame.view, position), out.specular, &out.colour);
	return out;
}

void Mesh::AppendTriangle(std::vector<graphics::world_triangles::Vertex>& out, const Triangle& triangle,
                          const glm::mat4* matrix, const DrawLight& light)
{
	namespace wt = graphics::world_triangles;
	std::array<glm::vec3, 3> front;
	for (size_t k = 0; k < front.size(); ++k)
	{
		const auto& v = triangle.corners.at(k).position;
		if (matrix == nullptr)
		{
			front.at(k) = v;
			continue;
		}
		// the original's matrix rows are glm's columns
		const auto& m = *matrix;
		front.at(k) = {((v.z * m[2].x + v.y * m[1].x) + m[0].x * v.x) + m[3].x,
		               ((v.y * m[1].y + m[0].y * v.x) + m[2].y * v.z) + m[3].y,
		               ((v.y * m[1].z + m[0].z * v.x) + m[2].z * v.z) + m[3].z};
	}
	// a = v2 - v0, b = v1 - v0, n = b x a
	const glm::vec3 a(front[2].x - front[0].x, front[2].y - front[0].y, front[2].z - front[0].z);
	const glm::vec3 b(front[1].x - front[0].x, front[1].y - front[0].y, front[1].z - front[0].z);
	glm::vec3 n(b.y * a.z - b.z * a.y, b.z * a.x - b.x * a.z, b.x * a.y - b.y * a.x);
	// (x x + y y) + z z into ModelInverseSquareRoot, then each axis times it
	const float inverse = gutils::ModelInverseSquareRoot((n.x * n.x + n.y * n.y) + n.z * n.z);
	n = {n.x * inverse, n.y * inverse, n.z * inverse};
	// (l.z n.z + l.y n.y) + l.x n.x, x 255 in Intensity, then the two sides
	model_light::TwoSidedColours colours {light.colour, light.colour};
	if (light.lit)
	{
		const float dot = (light.direction.z * n.z + light.direction.y * n.y) + light.direction.x * n.x;
		colours = model_light::TwoSided(light.colour, dot, light.ambient);
	}
	// the back copy, v - 0.45 n with 0.45 n stored first
	const glm::vec3 offset(k_BackOffset * n.x, k_BackOffset * n.y, k_BackOffset * n.z);
	std::array<glm::vec3, 3> back;
	for (size_t k = 0; k < back.size(); ++k)
	{
		back.at(k) = {front.at(k).x - offset.x, front.at(k).y - offset.y, front.at(k).z - offset.z};
	}
	const uint32_t specular = wt::ToAbgr(light.specular);
	const uint32_t frontColour = wt::ToAbgr(colours.front);
	const uint32_t backColour = wt::ToAbgr(colours.back);
	const auto frontVertex = [&](size_t k) {
		return wt::Vertex {front.at(k), triangle.corners.at(k).uv, frontColour, specular};
	};
	const auto backVertex = [&](size_t k) { return wt::Vertex {back.at(k), triangle.corners.at(k).uv, backColour, specular}; };
	// (f0, f1, f2) and (b2, b1, b0)
	out.insert(out.end(), {frontVertex(0), frontVertex(1), frontVertex(2), backVertex(2), backVertex(1), backVertex(0)});
	// the walls of the open edges
	for (size_t k = 0; k < 3; ++k)
	{
		if (triangle.neighbours.at(k) != -1)
		{
			continue;
		}
		const size_t next = (k + 1) % 3;
		out.insert(out.end(),
		           {frontVertex(k), backVertex(k), frontVertex(next), frontVertex(next), backVertex(k), backVertex(next)});
	}
}

void Mesh::AppendDraw(graphics::world_triangles::Frame& out, const glm::mat4* matrix, const DrawLight& light) const
{
	std::vector<graphics::world_triangles::Vertex> vertices;
	for (const auto& p : _primitives)
	{
		vertices.clear();
		for (const auto& t : p.triangles)
		{
			AppendTriangle(vertices, t, matrix, light);
		}
		// pass 0: the primitive's material through the current (normal) table
		out.Append(p.source, graphics::render_modes::Table::Normal, 0xFF, vertices);
	}
}

float openblack::physics::damage::RepairedShare(float life, std::optional<float> repairStart)
{
	if (repairStart.has_value())
	{
		const float base = *repairStart;
		const float a = 1.0f - base;
		const float b = life - base;
		return (a == 0.0f || b == 0.0f) ? 0.0f : b / a;
	}
	return life * k_RepairedWithoutRepair;
}

float openblack::physics::damage::DrawShare(float built, float repaired)
{
	// the smaller of the share built and the share repaired
	return built <= repaired ? built : repaired;
}

bool openblack::physics::damage::RebuildsOnBlow(float drawShare)
{
	return drawShare >= k_RebuildDrawShare && drawShare != 1.0f;
}

openblack::physics::damage::BlowModel
openblack::physics::damage::ModelForBlow(const std::shared_ptr<const Mesh>& current, float drawShare,
                                         const std::function<std::shared_ptr<Mesh>()>& fromIntact)
{
	if (!current)
	{
		return {fromIntact(), false};
	}
	// repaired enough since it was broken: the old broken model goes and a new one is made from the intact model, which no
	// blow has struck yet
	if (RebuildsOnBlow(drawShare))
	{
		return {fromIntact(), true};
	}
	return {std::make_shared<Mesh>(*current), false};
}

PartialBuild openblack::physics::damage::PartialBuildOf(float share, float footHeight, float halfHeight, float scale)
{
	// the share clamped to [0, 1]
	const float pct = share < 0.0f ? 0.0f : (share > 1.0f ? 1.0f : share);
	PartialBuild build;
	// d = ((half x scale) x pct) x 2 + the foot
	build.modelCut = halfHeight * scale * pct * 2.0f + footHeight;
	// pct > 0.8 cut, else whole (risen or rising)
	if (pct <= k_ScaffoldCutFrom)
	{
		if (pct < k_ScaffoldRisen)
		{
			// (1 - 5 pct) x half x -2
			build.scaffoldDrop = (1.0f - pct * k_ScaffoldRate) * halfHeight * k_ScaffoldDrop;
		}
	}
	else
	{
		// d = ((1 - (pct - 0.8) x 5) x half x scale) x 2 + the foot
		build.scaffoldCut = (1.0f - (pct - k_ScaffoldCutFrom) * k_ScaffoldRate) * halfHeight * scale * 2.0f + footHeight;
	}
	return build;
}
