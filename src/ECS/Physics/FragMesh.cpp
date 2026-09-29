/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FragMesh.h"

#include <algorithm>
#include <cmath>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
constexpr float k_Near = 0.01f;       // the double at 0x8C7A10
constexpr size_t k_MaxTriangles = 2048; // g_kept / g_broken, per primitive and impact
constexpr int k_MaxGroups = 64;

float Ground(glm::vec3 p)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(p.x, p.z)) : 0.0f;
}

bool Near(glm::vec3 a, glm::vec3 b)
{
	return std::abs(a.x - b.x) < k_Near && std::abs(a.y - b.y) < k_Near && std::abs(a.z - b.z) < k_Near;
}

float LengthSquared(glm::vec3 v)
{
	return glm::dot(v, v);
}

int SizeClass(const FragMesh::Triangle& t)
{
	const float longest = std::max({LengthSquared(t.v[1].pos - t.v[0].pos), LengthSquared(t.v[2].pos - t.v[1].pos),
	                                LengthSquared(t.v[0].pos - t.v[2].pos)});
	return longest > 32.0f ? 3 : longest > 8.0f ? 2 : longest > 2.0f ? 1 : 0;
}

/// fn_7F8A20: the vertices within r of the line through pos along vel (of the point pos when vel is 0).
int InsideCount(const FragMesh::Triangle& t, glm::vec3 pos, glm::vec3 vel, float r)
{
	const auto d = LengthSquared(vel) > 0.0f ? glm::normalize(vel) : glm::vec3(0.0f);
	int n = 0;
	for (const auto& v : t.v)
	{
		const auto w = v.pos - pos;
		if (glm::length(w - glm::dot(w, d) * d) < r)
		{
			++n;
		}
	}
	return n;
}

/// fn_7F7710: halves the triangle at the middle of its longest edge.
std::array<FragMesh::Triangle, 2> Split(const FragMesh::Triangle& t)
{
	int k = 2;
	float longest = LengthSquared(t.v[0].pos - t.v[2].pos);
	if (LengthSquared(t.v[1].pos - t.v[0].pos) > longest)
	{
		k = 0;
		longest = LengthSquared(t.v[1].pos - t.v[0].pos);
	}
	if (LengthSquared(t.v[2].pos - t.v[1].pos) > longest)
	{
		k = 1;
	}
	const auto& a = t.v[k];
	const auto& b = t.v[(k + 1) % 3];
	const auto& c = t.v[(k + 2) % 3];
	const FragMesh::Vertex m {(a.pos + b.pos) * 0.5f, (a.uv + b.uv) * 0.5f};
	std::array<FragMesh::Triangle, 2> out;
	out[0].v = {a, m, c};
	out[1].v = {m, b, c};
	for (auto& o : out)
	{
		o.sizeClass = t.sizeClass - 1;
		o.sizeClassRef = t.sizeClassRef - 1;
	}
	return out;
}

/// fn_7F8810: kept (0 vertices inside), broken (3), or halved and tested again; small triangles go by majority.
void Classify(const FragMesh::Triangle& t, glm::vec3 pos, glm::vec3 vel, float r, std::vector<FragMesh::Triangle>& kept,
              std::vector<FragMesh::Triangle>& broken)
{
	const int n = InsideCount(t, pos, vel, r);
	int c = n;
	if (t.sizeClass <= 0)
	{
		c = n == 1 ? 0 : n == 2 ? 3 : n;
	}
	if (c == 0)
	{
		if (kept.size() < k_MaxTriangles)
		{
			kept.push_back(t);
		}
	}
	else if (c == 3)
	{
		if (broken.size() < k_MaxTriangles)
		{
			broken.push_back(t);
		}
	}
	else
	{
		for (const auto& half : Split(t))
		{
			Classify(half, pos, vel, r, kept, broken);
		}
	}
}

/// Two triangles share an edge when at least two of their vertices match.
bool SharesEdge(const FragMesh::Triangle& a, const FragMesh::Triangle& b)
{
	int matches = 0;
	for (const auto& va : a.v)
	{
		for (const auto& vb : b.v)
		{
			if (Near(va.pos, vb.pos))
			{
				++matches;
				break;
			}
		}
	}
	return matches >= 2;
}

glm::vec3 RandomSpin(float scale)
{
	auto& rng = Locator::rng::value();
	const auto r = [&]() { return static_cast<float>(rng.NextValue(0, 200) - 100) * scale; };
	return {r(), r(), r()};
}

uint32_t g_NextMesh = 0;
} // namespace

std::shared_ptr<FragMesh> FragMesh::FromEntity(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* meshComponent = registry.TryGet<const Mesh>(entity);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshComponent == nullptr || !meshes.Contains(meshComponent->id))
	{
		return nullptr;
	}
	const auto mesh = meshes.Handle(meshComponent->id);
	const auto& transform = registry.Get<const Transform>(entity);
	// objects that morph with the landscape are baked with it (the original's g_morph)
	const bool morph = registry.AllOf<MorphWithTerrain>(entity);
	const float originGround = Ground(transform.position);
	auto result = std::make_shared<FragMesh>();
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		// LOD 0, status 0 (flags & 0x20000000 && !(flags & 0x3F0))
		if ((subMesh->GetFlags().lodMask & 1) == 0 || subMesh->GetFlags().status != 0 || subMesh->IsPhysics())
		{
			continue;
		}
		const auto& positions = subMesh->GetCollisionPositions();
		const auto& uvs = subMesh->GetCollisionUVs();
		const auto& indices = subMesh->GetCollisionIndices();
		for (const auto& primitive : subMesh->GetPrimitives())
		{
			Primitive fp {primitive, {}};
			for (uint32_t i = primitive.indicesOffset; i + 2 < primitive.indicesOffset + primitive.indicesCount; i += 3)
			{
				Triangle t;
				for (uint32_t k = 0; k < 3; ++k)
				{
					const auto index = indices.at(i + k);
					t.v.at(k).pos = transform.position + transform.rotation * (transform.scale * positions.at(index));
					if (morph)
					{
						t.v.at(k).pos.y += Ground(t.v.at(k).pos) - originGround;
					}
					t.v.at(k).uv = index < uvs.size() ? uvs[index] : glm::vec2(0.0f);
				}
				t.sizeClass = t.sizeClassRef = SizeClass(t);
				fp.triangles.push_back(t);
			}
			result->_originalTriangleCount += static_cast<int>(fp.triangles.size());
			result->ComputeAdjacency(fp);
			result->_primitives.push_back(std::move(fp));
		}
	}
	return result->_originalTriangleCount > 0 ? result : nullptr;
}

void FragMesh::ComputeAdjacency(Primitive& primitive) const
{
	auto& tris = primitive.triangles;
	for (auto& t : tris)
	{
		t.neighbour = {-1, -1, -1};
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
				const auto& a = tris[i].v.at(k).pos;
				const auto& b = tris[i].v.at((k + 1) % 3).pos;
				bool hasA = false;
				bool hasB = false;
				for (const auto& v : tris[j].v)
				{
					hasA = hasA || Near(a, v.pos);
					hasB = hasB || Near(b, v.pos);
				}
				if (hasA && hasB)
				{
					tris[i].neighbour.at(k) = static_cast<int>(j);
				}
			}
		}
	}
}

glm::vec3 FragMesh::Centroid() const
{
	glm::vec3 sum(0.0f);
	size_t n = 0;
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			for (const auto& v : t.v)
			{
				sum += v.pos;
				++n;
			}
		}
	}
	return n > 0 ? sum / static_cast<float>(n) : sum;
}

void FragMesh::Translate(glm::vec3 offset)
{
	for (auto& p : _primitives)
	{
		for (auto& t : p.triangles)
		{
			for (auto& v : t.v)
			{
				v.pos += offset;
			}
		}
	}
}

float FragMesh::Area() const
{
	float area = 0.0f;
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			area += 0.5f * glm::length(glm::cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos));
		}
	}
	return area;
}

void FragMesh::UniqueVertices(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals) const
{
	for (const auto& p : _primitives)
	{
		for (const auto& t : p.triangles)
		{
			auto n = glm::cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
			const float len = glm::length(n);
			n = len > 0.0f ? n / len : glm::vec3(0.0f);
			for (const auto& v : t.v)
			{
				if (std::find(positions.begin(), positions.end(), v.pos) == positions.end())
				{
					positions.push_back(v.pos);
					normals.push_back(n);
				}
			}
		}
	}
}

size_t FragMesh::TriangleCount() const
{
	size_t n = 0;
	for (const auto& p : _primitives)
	{
		n += p.triangles.size();
	}
	return n;
}

std::vector<FragMesh::Piece> FragMesh::Impact(glm::vec3 pos, glm::vec3 vel, float radius)
{
	std::vector<Piece> pieces;
	std::vector<Primitive> remaining;
	for (auto& p : _primitives)
	{
		std::vector<Triangle> kept;
		std::vector<Triangle> broken;
		for (const auto& t : p.triangles)
		{
			Classify(t, pos, vel, radius, kept, broken);
		}
		if (!kept.empty())
		{
			Primitive k {p.material, std::move(kept)};
			ComputeAdjacency(k);
			remaining.push_back(std::move(k));
		}
		if (!broken.empty())
		{
			// FragPrimitive::Impact 0x76E2D0: the broken triangles of the primitive fly as one Fragment with the
			// hitter's velocity; its unconnected parts come off it at rest (CreateFragment -> SplitUnconnectedGroups)
			auto piece = std::make_shared<FragMesh>();
			Primitive b {p.material, std::move(broken)};
			piece->ComputeAdjacency(b);
			piece->_primitives.push_back(std::move(b));
			piece->_originalTriangleCount = static_cast<int>(piece->TriangleCount());
			const auto c = piece->Centroid();
			piece->Translate(-c);
			const auto constructed = piece->TriangleCount(); // the Fragment's lifetime is counted before its split
			auto parts = piece->SplitUnconnectedGroups(false, c);
			if (piece->TriangleCount() > 0)
			{
				const auto c2 = piece->Centroid();
				piece->Translate(-c2);
				pieces.push_back({piece, c + c2, vel, RandomSpin(0.01f), constructed});
			}
			for (auto& part : parts)
			{
				pieces.push_back(std::move(part));
			}
		}
	}
	_primitives = std::move(remaining);
	auto fallen = SplitUnconnectedGroups(true, glm::vec3(0.0f));
	for (auto& f : fallen)
	{
		pieces.push_back(std::move(f));
	}
	return pieces;
}

std::vector<FragMesh::Piece> FragMesh::SplitUnconnectedGroups(bool groundCheck, glm::vec3 offset)
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
		if (static_cast<int>(count.size()) >= k_MaxGroups)
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
			for (const auto& v : t.v)
			{
				if (v.pos.y < Ground(v.pos) + 0.1f)
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
			Primitive part {p.material, {}};
			for (const auto& t : p.triangles)
			{
				if (t.group == static_cast<int>(g))
				{
					auto copy = t;
					copy.sizeClassRef = copy.sizeClass + 1;
					part.triangles.push_back(copy);
				}
			}
			if (part.triangles.empty())
			{
				continue;
			}
			auto piece = std::make_shared<FragMesh>();
			piece->ComputeAdjacency(part);
			piece->_primitives.push_back(std::move(part));
			piece->_originalTriangleCount = static_cast<int>(piece->TriangleCount());
			const auto c = piece->Centroid();
			piece->Translate(-c);
			pieces.push_back({piece, c + offset, glm::vec3(0.0f), RandomSpin(0.02f), piece->TriangleCount()});
		}
	}
	// everything not anchored goes: the pieces above, and lone triangles (and groups past the 64th)
	for (auto& p : _primitives)
	{
		std::erase_if(p.triangles, [&](const Triangle& t) { return t.group < 0 || !anchored.at(t.group); });
		ComputeAdjacency(p);
	}
	std::erase_if(_primitives, [](const Primitive& p) { return p.triangles.empty(); });
	return pieces;
}

float FragMesh::GetRemaining()
{
	int n = 0;
	for (const auto& p : _primitives)
	{
		n += static_cast<int>(std::count_if(p.triangles.begin(), p.triangles.end(),
		                                    [](const Triangle& t) { return t.sizeClass == t.sizeClassRef; }));
	}
	const float f = _originalTriangleCount > 0 ? static_cast<float>(n) / static_cast<float>(_originalTriangleCount) : 0.0f;
	_remaining = std::clamp(f, 0.0f, 1.0f);
	return _remaining;
}

void FragMesh::Merge(const FragMesh& piece, const glm::mat4& transform)
{
	for (auto p : piece._primitives)
	{
		for (auto& t : p.triangles)
		{
			for (auto& v : t.v)
			{
				v.pos = glm::vec3(transform * glm::vec4(v.pos, 1.0f));
			}
			// rubble does not count as building (sizeClassRef stays one above sizeClass)
		}
		ComputeAdjacency(p);
		_primitives.push_back(std::move(p));
	}
}

entt::id_type FragMesh::BuildMesh(const glm::mat4& worldToLocal, const std::string& name,
                                  std::vector<graphics::L3DSubMesh::GeneratedPrimitive> extra) const
{
	const glm::mat3 normalMatrix(glm::transpose(glm::inverse(glm::mat3(worldToLocal))));
	std::vector<graphics::L3DSubMesh::GeneratedPrimitive> primitives;
	for (const auto& p : _primitives)
	{
		graphics::L3DSubMesh::GeneratedPrimitive out;
		out.material = p.material;
		out.material.twoSided = true;
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
			}
			auto n = glm::cross(t.v[1].pos - t.v[0].pos, t.v[2].pos - t.v[0].pos);
			n = LengthSquared(n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
			// front face, and the back face 0.45 behind it
			const auto f0 = add(t.v[0].pos, t.v[0].uv, n);
			const auto f1 = add(t.v[1].pos, t.v[1].uv, n);
			const auto f2 = add(t.v[2].pos, t.v[2].uv, n);
			out.indices.insert(out.indices.end(), {f0, f1, f2});
			const auto back = -0.45f * n;
			const auto b0 = add(t.v[0].pos + back, t.v[0].uv, -n);
			const auto b1 = add(t.v[1].pos + back, t.v[1].uv, -n);
			const auto b2 = add(t.v[2].pos + back, t.v[2].uv, -n);
			out.indices.insert(out.indices.end(), {b2, b1, b0});
			// a side wall on each open edge
			for (int k = 0; k < 3; ++k)
			{
				if (t.neighbour.at(k) != -1)
				{
					continue;
				}
				// quad (k, k+3, k+1) (k+1, k+3, k+4): the front edge lit like the front, the back edge like the back
				const auto& a = t.v.at(k);
				const auto& b = t.v.at((k + 1) % 3);
				const auto s0 = add(a.pos, a.uv, n);
				const auto s1 = add(b.pos, b.uv, n);
				const auto s3 = add(a.pos + back, a.uv, -n);
				const auto s4 = add(b.pos + back, b.uv, -n);
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
	const auto id = entt::hashed_string((name + "/" + std::to_string(g_NextMesh++)).c_str()).value();
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
