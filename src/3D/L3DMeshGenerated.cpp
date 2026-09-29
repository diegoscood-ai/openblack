/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Meshes built at run time from triangles (building fragments, FragMesh): one sub-mesh, drawn like a pack mesh.

#include <cfloat>
#include <stdexcept>

#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Resources/Loaders.h"

using namespace openblack;
using namespace openblack::graphics;

namespace
{
struct GeneratedVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 norm;
	glm::i16vec2 index;
};
} // namespace

bool L3DSubMesh::LoadGenerated(const std::vector<GeneratedPrimitive>& primitives) noexcept
{
	_flags = {};
	_flags.lodMask = 1;
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	for (const auto& p : primitives)
	{
		nVertices += static_cast<uint32_t>(p.positions.size());
		nIndices += static_cast<uint32_t>(p.indices.size());
	}
	if (nVertices == 0 || nIndices == 0 || nVertices > 0xFFFF)
	{
		return false;
	}
	_boundingBox.maxima = glm::vec3(-FLT_MAX);
	_boundingBox.minima = glm::vec3(FLT_MAX);
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(GeneratedVertex) * nVertices);
	auto* vertices = reinterpret_cast<GeneratedVertex*>(verticesMem->data);
	const bgfx::Memory* indicesMem = bgfx::alloc(sizeof(uint16_t) * nIndices);
	auto* indices = reinterpret_cast<uint16_t*>(indicesMem->data);
	_collisionPositions.clear();
	_collisionIndices.clear();
	_collisionUVs.clear();
	_primitives.clear();
	uint32_t vertex = 0;
	uint32_t index = 0;
	for (const auto& p : primitives)
	{
		const auto base = vertex;
		for (size_t i = 0; i < p.positions.size(); ++i, ++vertex)
		{
			vertices[vertex] = {p.positions[i], i < p.uvs.size() ? p.uvs[i] : glm::vec2(0.0f),
			                    i < p.normals.size() ? p.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f), glm::i16vec2(-1, -1)};
			_collisionPositions.push_back(p.positions[i]);
			_collisionUVs.push_back(vertices[vertex].uv);
			_boundingBox.maxima = glm::max(_boundingBox.maxima, p.positions[i]);
			_boundingBox.minima = glm::min(_boundingBox.minima, p.positions[i]);
		}
		auto material = p.material;
		material.indicesOffset = index;
		material.indicesCount = static_cast<uint32_t>(p.indices.size());
		for (const auto i : p.indices)
		{
			const auto merged = static_cast<uint16_t>(base + i);
			indices[index++] = merged;
			_collisionIndices.push_back(merged);
		}
		_primitives.push_back(material);
	}
	VertexDecl decl;
	decl.reserve(4);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Indices, static_cast<uint8_t>(2), VertexAttrib::Type::Int16);
	auto* vertexBuffer = new VertexBuffer(_l3dMesh.GetDebugName(), verticesMem, decl);
	auto* indexBuffer = new IndexBuffer(_l3dMesh.GetDebugName(), indicesMem, IndexBuffer::Type::Uint16);
	_mesh = std::make_unique<graphics::Mesh>(vertexBuffer, indexBuffer);
	return true;
}

bool L3DMesh::LoadGenerated(const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) noexcept
{
	auto subMesh = std::make_unique<L3DSubMesh>(*this);
	if (!subMesh->LoadGenerated(primitives))
	{
		return false;
	}
	_boundingBox = subMesh->GetBoundingBox();
	_subMeshes.clear();
	_subMeshes.emplace_back(std::move(subMesh));
	return true;
}

resources::L3DLoader::result_type resources::L3DLoader::operator()(FromGeneratedTag, const std::string& debugName,
                                                                   const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(debugName);
	if (!mesh->LoadGenerated(primitives))
	{
		throw std::runtime_error("Unable to generate mesh " + debugName);
	}
	return mesh;
}
