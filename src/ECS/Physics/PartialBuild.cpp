/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PartialBuild.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <glm/geometric.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;

namespace
{
struct Vertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 normal;
};

Vertex Lerp(const Vertex& a, const Vertex& b, float t)
{
	return {a.pos + (b.pos - a.pos) * t, a.uv + (b.uv - a.uv) * t, glm::normalize(a.normal + (b.normal - a.normal) * t)};
}

/// The part of a triangle under y = h (0, 3 or 4 corners), and the cut edge when the plane crosses it.
struct Clipped
{
	std::vector<Vertex> polygon;
	bool cut {false};
	std::array<Vertex, 2> edge {};
};

Clipped ClipBelow(const std::array<Vertex, 3>& t, float h)
{
	Clipped out;
	std::vector<Vertex> onPlane;
	for (size_t i = 0; i < 3; ++i)
	{
		const auto& a = t.at(i);
		const auto& b = t.at((i + 1) % 3);
		const bool aIn = a.pos.y <= h;
		const bool bIn = b.pos.y <= h;
		if (aIn)
		{
			out.polygon.push_back(a);
		}
		if (aIn != bIn)
		{
			const auto p = Lerp(a, b, (h - a.pos.y) / (b.pos.y - a.pos.y));
			out.polygon.push_back(p);
			onPlane.push_back(p);
		}
	}
	if (onPlane.size() == 2)
	{
		out.cut = true;
		out.edge = {onPlane[0], onPlane[1]};
	}
	return out;
}

void AddPolygon(graphics::L3DSubMesh::GeneratedPrimitive& out, const std::vector<Vertex>& polygon)
{
	if (polygon.size() < 3 || out.positions.size() + polygon.size() > 0xFFFF)
	{
		return;
	}
	const auto base = static_cast<uint16_t>(out.positions.size());
	for (const auto& v : polygon)
	{
		out.positions.push_back(v.pos);
		out.uvs.push_back(v.uv);
		out.normals.push_back(v.normal);
	}
	for (uint16_t i = 1; i + 1 < polygon.size(); ++i)
	{
		out.indices.insert(out.indices.end(), {base, static_cast<uint16_t>(base + i), static_cast<uint16_t>(base + i + 1)});
	}
}

/// The world-space triangles of a primitive with smooth vertex normals (averaged over the sub-mesh's faces).
std::vector<std::array<Vertex, 3>> Triangles(const graphics::L3DSubMesh& subMesh, const graphics::L3DSubMesh::Primitive& primitive,
                                             const Transform& transform, glm::vec3 offset)
{
	const auto& positions = subMesh.GetCollisionPositions();
	const auto& uvs = subMesh.GetCollisionUVs();
	const auto& indices = subMesh.GetCollisionIndices();
	std::vector<glm::vec3> world(positions.size());
	for (size_t i = 0; i < positions.size(); ++i)
	{
		world[i] = transform.position + transform.rotation * (transform.scale * positions[i]) + offset;
	}
	std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));
	for (size_t i = 0; i + 2 < indices.size(); i += 3)
	{
		const auto n = glm::cross(world[indices[i + 1]] - world[indices[i]], world[indices[i + 2]] - world[indices[i]]);
		for (size_t k = 0; k < 3; ++k)
		{
			normals[indices[i + k]] += n;
		}
	}
	std::vector<std::array<Vertex, 3>> out;
	for (uint32_t i = primitive.indicesOffset; i + 2 < primitive.indicesOffset + primitive.indicesCount; i += 3)
	{
		std::array<Vertex, 3> t;
		for (uint32_t k = 0; k < 3; ++k)
		{
			const auto index = indices.at(i + k);
			const auto n = normals[index];
			t.at(k) = {world[index], index < uvs.size() ? uvs[index] : glm::vec2(0.0f),
			           glm::dot(n, n) > 0.0f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f)};
		}
		out.push_back(t);
	}
	return out;
}
} // namespace

std::vector<graphics::L3DSubMesh::GeneratedPrimitive> PartialBuild::Build(entt::entity building, entt::id_type meshId, float percent)
{
	std::vector<graphics::L3DSubMesh::GeneratedPrimitive> result;
	auto& meshes = Locator::resources::value().GetMeshes();
	if (percent <= 0.0f || !meshes.Contains(meshId))
	{
		return result; // nothing is drawn at 0 %
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(building);
	const auto mesh = meshes.Handle(meshId);
	const float height = mesh->GetBoundingBox().Size().y * transform.scale.y; // H x scale, H = 2 x half height
	const float cut = transform.position.y + percent * height;
	// the scaffold: the sub-mesh with the highest status
	uint32_t scaffoldStatus = 0;
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		scaffoldStatus = std::max<uint32_t>(scaffoldStatus, subMesh->GetFlags().status);
	}
	for (const auto& subMesh : mesh->GetSubMeshes())
	{
		const auto flags = subMesh->GetFlags();
		if (subMesh->IsPhysics() || (flags.lodMask & 1) == 0)
		{
			continue;
		}
		if (flags.status == 0)
		{
			// the main mesh: nothing when the cut is under 0.2
			if (cut - transform.position.y < 0.2f)
			{
				continue;
			}
			for (const auto& primitive : subMesh->GetPrimitives())
			{
				const float k = primitive.twoSided ? 0.2f : 0.35f; // fn_85C0E0: cullMode bit 0 -> 0.2
				graphics::L3DSubMesh::GeneratedPrimitive out;
				out.material = primitive;
				out.material.twoSided = true; // the inner pass is drawn without culling
				for (const auto& t : Triangles(*subMesh, primitive, transform, glm::vec3(0.0f)))
				{
					auto inner = t;
					for (auto& v : inner)
					{
						v.pos -= k * glm::vec3(v.normal.x, 0.0f, v.normal.z);
					}
					const auto outerCut = ClipBelow(t, cut);
					const auto innerCut = ClipBelow(inner, cut);
					AddPolygon(out, outerCut.polygon);
					AddPolygon(out, innerCut.polygon);
					// fn_820B20: the cap joining the two cut outlines at the cut height
					if (outerCut.cut && innerCut.cut)
					{
						const glm::vec3 up(0.0f, 1.0f, 0.0f);
						AddPolygon(out, {{outerCut.edge[0].pos, outerCut.edge[0].uv, up},
						                 {outerCut.edge[1].pos, outerCut.edge[1].uv, up},
						                 {innerCut.edge[1].pos, innerCut.edge[1].uv, up},
						                 {innerCut.edge[0].pos, innerCut.edge[0].uv, up}});
					}
				}
				if (!out.indices.empty())
				{
					result.push_back(std::move(out));
				}
			}
		}
		else if (flags.status == scaffoldStatus)
		{
			// the scaffold rises out of the ground (pct < 0.2), stands, then is cut away from the top (pct > 0.8)
			glm::vec3 offset(0.0f);
			float scaffoldCut = 1e9f;
			if (percent < 0.2f)
			{
				offset = -glm::normalize(transform.rotation[1]) * ((1.0f - 5.0f * percent) * height);
			}
			else if (percent > 0.8f)
			{
				scaffoldCut = transform.position.y + (1.0f - 5.0f * (percent - 0.8f)) * height;
			}
			for (const auto& primitive : subMesh->GetPrimitives())
			{
				graphics::L3DSubMesh::GeneratedPrimitive out;
				out.material = primitive;
				for (const auto& t : Triangles(*subMesh, primitive, transform, offset))
				{
					AddPolygon(out, ClipBelow(t, scaffoldCut).polygon);
				}
				if (!out.indices.empty())
				{
					result.push_back(std::move(out));
				}
			}
		}
	}
	return result;
}
