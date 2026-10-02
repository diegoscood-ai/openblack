/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "L3DSubMesh.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <bgfx/bgfx.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "EngineConfig.h"
#include "Graphics/IndexBuffer.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/VertexBuffer.h"
#include "L3DMesh.h"
#include "Locator.h"
#include "PnTessellation.h"

using namespace openblack::graphics;

namespace bgfx
{
// Defined and exported by bgfx but not declared in bgfx.h: frees a Memory that is not handed to bgfx.
void release(const Memory* _mem);
} // namespace bgfx

namespace openblack
{

struct EnhancedL3DVertex
{
	glm::vec3 pos;
	glm::vec2 uv;
	glm::vec3 norm;
	glm::i16vec2 index;
};

namespace
{
/// The D3D states of the mode of an L3D material type (the type is the index of the tables 0xC38728 / 0xC387C8:
/// render_modes::k_Modes). 14 and 17 are the functions of modes 5 and 2 (0x82DD90, 0x82D820)
void ApplyMode(L3DSubMesh::Primitive& primitive, uint32_t type)
{
	assert(type < render_modes::k_ModeCount);
	const auto& desc = render_modes::Desc(static_cast<render_modes::Mode>(type));
	primitive.materialType = type;
	primitive.depthWrite = desc.zWrite;
	primitive.alphaTest = desc.alphaTest;
	primitive.blend = desc.blend;
	primitive.modulateAlpha = desc.alphaModulate;
	primitive.thresholdAlpha = desc.alphaTest;
}

// Mod graphics.hd-tweaks: a boned mesh whose textures are all villager textures (EngineConfig::hdTweaksSkins)
bool AllPersonSkins(const auto& primitiveSpan)
{
	const auto& config = Locator::config::value();
	if (config.hdTweaksSkins.empty() || primitiveSpan.empty())
	{
		return false;
	}
	return std::ranges::all_of(primitiveSpan, [&config](const auto& primitive) {
		return std::ranges::find(config.hdTweaksSkins, primitive.material.skinID) != config.hdTweaksSkins.end();
	});
}

void SmoothPerson(const auto& boneSpans, const bgfx::Memory*& verticesMem, const bgfx::Memory*& indicesMem,
                  std::vector<L3DSubMesh::Primitive>& primitives, uint32_t& nVertices, uint32_t& nIndices)
{
	// each bone's model matrix in the rest pose (the chain of its parents, as for the bounding box)
	std::vector<glm::mat4> restBones(boneSpans.size(), glm::mat4(1.0f));
	for (uint32_t b = 0; b < boneSpans.size(); ++b)
	{
		for (uint32_t parent = b; parent != std::numeric_limits<uint32_t>::max(); parent = boneSpans[parent].parent)
		{
			const auto& bone = boneSpans[parent];
			const auto orientation = glm::make_mat3(bone.orientation.data());
			const auto translation = glm::make_vec3(&bone.position.x) * orientation;
			restBones[b] = glm::translate(glm::mat4(orientation), translation) * restBones[b];
		}
	}

	const auto* source = reinterpret_cast<const EnhancedL3DVertex*>(verticesMem->data);
	std::vector<PnVertex> vertices(nVertices);
	for (uint32_t i = 0; i < nVertices; ++i)
	{
		vertices[i] = {source[i].pos, source[i].uv, source[i].norm, source[i].index.x};
	}
	const auto* sourceIndices = reinterpret_cast<const uint16_t*>(indicesMem->data);
	std::vector<uint16_t> indices(sourceIndices, sourceIndices + nIndices);
	std::vector<PnRange> ranges;
	ranges.reserve(primitives.size());
	for (const auto& primitive : primitives)
	{
		ranges.push_back({primitive.indicesOffset, primitive.indicesCount});
	}
	if (!TessellatePn(vertices, indices, ranges, restBones, Locator::config::value().hdTweaksSmoothLevel))
	{
		return;
	}

	const auto* newVertices = bgfx::alloc(static_cast<uint32_t>(sizeof(EnhancedL3DVertex) * vertices.size()));
	auto* target = reinterpret_cast<EnhancedL3DVertex*>(newVertices->data);
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		target[i] = {vertices[i].position, vertices[i].uv, vertices[i].normal, glm::i16vec2(vertices[i].bone, -1)};
	}
	const auto* newIndices = bgfx::alloc(static_cast<uint32_t>(sizeof(uint16_t) * indices.size()));
	std::copy(indices.begin(), indices.end(), reinterpret_cast<uint16_t*>(newIndices->data));
	bgfx::release(verticesMem);
	bgfx::release(indicesMem);
	verticesMem = newVertices;
	indicesMem = newIndices;
	for (size_t i = 0; i < primitives.size(); ++i)
	{
		primitives[i].indicesOffset = ranges[i].indicesOffset;
		primitives[i].indicesCount = ranges[i].indicesCount;
	}
	nVertices = static_cast<uint32_t>(vertices.size());
	nIndices = static_cast<uint32_t>(indices.size());
}
} // namespace

L3DSubMesh::L3DSubMesh(L3DMesh& mesh) noexcept
    : _l3dMesh(mesh)
{
}

L3DSubMesh::~L3DSubMesh() noexcept = default;

bool L3DSubMesh::Load(const l3d::L3DFile& l3d, uint32_t meshIndex) noexcept
{
	const auto& header = l3d.GetSubmeshHeaders()[meshIndex];
	const auto primitiveSpan = l3d.GetPrimitiveSpan(meshIndex);
	const auto& verticesSpan = l3d.GetVertexSpan(meshIndex);
	const auto& indexSpan = l3d.GetIndexSpan(meshIndex);
	const auto& vertexGroupSpans = l3d.GetVertexGroupSpan(meshIndex);
	const auto& boneSpans = l3d.GetBoneSpan(meshIndex);

	_flags = header.flags;

	// Count vertices and indices
	uint32_t nVertices = 0;
	uint32_t nIndices = 0;
	for (auto& primitive : primitiveSpan)
	{
		nVertices += primitive.numVertices;
		nIndices += primitive.numTriangles * 3;
	}

	// Construct bounding box
	_boundingBox.maxima = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	_boundingBox.minima = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	if (_flags.hasBones)
	{
		for (auto& primitive : primitiveSpan)
		{
			uint32_t vertexOffset = 0;
			for (uint32_t i = 0; i < primitive.numGroups; ++i)
			{
				auto matrix = glm::identity<glm::mat4>();
				for (uint32_t parent = vertexGroupSpans[i].boneIndex; parent != std::numeric_limits<uint32_t>::max();
				     parent = boneSpans[parent].parent)
				{
					const auto& bone = boneSpans[parent];
					const auto orientation = glm::make_mat3(bone.orientation.data());
					const auto translation = glm::make_vec3(&bone.position.x) * orientation;
					const auto local = glm::translate(glm::mat4(orientation), translation);
					matrix = local * matrix;
				}

				for (uint32_t j = 0; j < vertexGroupSpans[i].vertexCount; ++j)
				{
					const auto& vertex = verticesSpan[vertexOffset + j];
					const auto position = glm::xyz(matrix * glm::vec4(glm::make_vec3(&vertex.position.x), 1.0f));
					_boundingBox.maxima = glm::max(_boundingBox.maxima, position);
					_boundingBox.minima = glm::min(_boundingBox.minima, position);
				}
				vertexOffset += vertexGroupSpans[i].vertexCount;
			}
		}
	}
	else
	{
		for (uint32_t i = 0; i < nVertices; i++)
		{
			const auto position = glm::make_vec3(&verticesSpan[i].position.x);
			_boundingBox.maxima = glm::max(_boundingBox.maxima, position);
			_boundingBox.minima = glm::min(_boundingBox.minima, position);
		}
	}

	if (nVertices == 0)
	{
		return false;
	}

	// Get vertices
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(EnhancedL3DVertex) * nVertices);
	auto* verticesMemAccess = reinterpret_cast<EnhancedL3DVertex*>(verticesMem->data);
	for (uint32_t i = 0; i < nVertices; ++i)
	{
		verticesMemAccess[i].pos = glm::make_vec3(&verticesSpan[i].position.x);
		verticesMemAccess[i].uv = glm::make_vec2(&verticesSpan[i].texCoord.x);
		// TODO(bwrsandman): build normals from mesh
		verticesMemAccess[i].norm = glm::make_vec3(&verticesSpan[i].normal.x);
		verticesMemAccess[i].index.x = -1;
		verticesMemAccess[i].index.y = -1;
	}

	if (nIndices == 0)
	{
		return false;
	}

	// Get Indices
	const bgfx::Memory* indicesMem = bgfx::alloc(sizeof(uint16_t) * nIndices);
	auto* indices = reinterpret_cast<uint16_t*>(indicesMem->data);

	// Fill bone index
	uint32_t vertexIndex = 0;
	for (auto& vertexGroupSpan : vertexGroupSpans)
	{
		for (uint32_t i = 0; i < vertexGroupSpan.vertexCount; ++i)
		{
			verticesMemAccess[vertexIndex].index[0] = vertexGroupSpan.boneIndex;
			verticesMemAccess[vertexIndex].index[1] = -1;
			vertexIndex++;
		}
	}

	_collisionPositions.resize(nVertices);
	_collisionUVs.resize(nVertices);
	_skinBones.assign(nVertices, 0);
	for (uint32_t i = 0; i < nVertices; ++i)
	{
		_collisionPositions[i] = verticesMemAccess[i].pos;
		_collisionUVs[i] = verticesMemAccess[i].uv;
		_skinBones[i] = static_cast<uint16_t>(std::max<int32_t>(0, verticesMemAccess[i].index[0]));
	}
	// before the rest pose below and the hd-tweaks smoothing, which changes the vertex buffer (not these)
	_skinLocalPositions = _collisionPositions;

	uint16_t startIndex = 0;
	uint16_t startVertex = 0;
	for (auto& primitive : primitiveSpan)
	{
		// Fix indices for merged vertex buffer
		for (uint32_t j = 0; j < primitive.numTriangles * 3; j++)
		{
			indices[startIndex + j] = indexSpan[startIndex + j] + startVertex;
		}

		assert(static_cast<uint32_t>(primitive.material.type) != 0xe);
		assert(static_cast<uint32_t>(primitive.material.type) != 0x11);
		// the inline SetMaterial (0x412662..0x4126BD) reads the culling from +5 bit 0 and the tiling from +5 bit 2
		auto& added = _primitives.emplace_back(Primitive {
		    primitive.material.skinID,
		    startIndex,
		    primitive.numTriangles * 3,
		    false,
		    false,
		    Primitive::BlendMode::Disabled,
		    false,
		    false,
		    primitive.material.alphaCutoutThreshold / 255.0f,
		    glm::vec4(primitive.material.color.bgra.r, primitive.material.color.bgra.g, primitive.material.color.bgra.b,
		              primitive.material.color.bgra.a) /
		        255.0f,
		    (primitive.material.cullMode & 1) != 0,
		    (primitive.material.cullMode & 4) != 0,
		    (primitive.material.cullMode & 0x10) == 0,
		    0,
		});
		ApplyMode(added, static_cast<uint32_t>(primitive.material.type));
		_collisionRanges.emplace_back(startIndex, primitive.numTriangles * 3);

		startVertex += static_cast<uint16_t>(primitive.numVertices);
		startIndex += static_cast<uint16_t>(primitive.numTriangles * 3);
	}

	VertexDecl decl;
	decl.reserve(4);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::TexCoord0, static_cast<uint8_t>(2), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	decl.emplace_back(VertexAttrib::Attribute::Indices, static_cast<uint8_t>(2), VertexAttrib::Type::Int16);

	// build our buffers
	_collisionIndices.assign(indices, indices + nIndices);
	if (_flags.hasBones)
	{
		// Boned meshes (villagers, animals) are drawn in their rest pose: pick them in it, each vertex moved by the chain
		// of its vertex group's bone (the same transform as the bounding box above)
		uint32_t vertex = 0;
		for (const auto& vertexGroupSpan : vertexGroupSpans)
		{
			auto matrix = glm::identity<glm::mat4>();
			for (uint32_t parent = vertexGroupSpan.boneIndex; parent != std::numeric_limits<uint32_t>::max();
			     parent = boneSpans[parent].parent)
			{
				const auto& bone = boneSpans[parent];
				const auto orientation = glm::make_mat3(bone.orientation.data());
				const auto translation = glm::make_vec3(&bone.position.x) * orientation;
				matrix = glm::translate(glm::mat4(orientation), translation) * matrix;
			}
			for (uint32_t j = 0; j < vertexGroupSpan.vertexCount && vertex < nVertices; ++j, ++vertex)
			{
				_collisionPositions[vertex] = glm::xyz(matrix * glm::vec4(_collisionPositions[vertex], 1.0f));
			}
		}
	}
	// Mod graphics.hd-tweaks (smooth): the villagers' meshes as curved PN triangles. The collision data built above stays
	// the original's (the hand and the physics use it).
	// ... and the hand (Game loads it from Hand_Boned_Base2.l3d, rigid bones like the villagers)
	_hdTweaked = _flags.hasBones && (AllPersonSkins(primitiveSpan) || _l3dMesh.GetDebugName() == "Hand_Boned_Base2");
	if (_hdTweaked && Locator::config::value().hdTweaksSmoothLevel >= 2)
	{
		SmoothPerson(boneSpans, verticesMem, indicesMem, _primitives, nVertices, nIndices);
	}

	auto* vertexBuffer = new VertexBuffer(_l3dMesh.GetDebugName(), verticesMem, decl);
	auto* indexBuffer = new IndexBuffer(_l3dMesh.GetDebugName(), indicesMem, IndexBuffer::Type::Uint16);
	_mesh = std::make_unique<graphics::Mesh>(vertexBuffer, indexBuffer);

	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "{} submesh {} with {} verts and {} indices", _l3dMesh.GetDebugName(), meshIndex,
	                    nVertices, nIndices);
	return true;
}

void L3DSubMesh::SetMaterialProperties(const MaterialProperties& properties) noexcept
{
	for (auto& primitive : _primitives)
	{
		// GJUtils::SetMaterialProperties 0x57E120
		const auto mode =
		    render_modes::ModeFromProperties(static_cast<render_modes::Mode>(primitive.materialType), properties);
		ApplyMode(primitive, static_cast<uint32_t>(mode));
		primitive.twoSided = properties.doubleSided; // 0x57E1B7: byte +5 bit 0
	}
}

void L3DSubMesh::ReplaceMaterialType(uint32_t from, uint32_t to) noexcept
{
	// fn_0057E220 writes any value in the type dword; openblack only knows the 19 modes of the table 0xC38728. Never hit:
	// the only caller in the image (PhysicalShield 0x72CCCD / 0x72CCE5) uses (5, 13) and (4, 13)
	if (to >= render_modes::k_ModeCount)
	{
		return;
	}
	for (auto& primitive : _primitives)
	{
		// fn_0057E220 0x57E252: cmp [material], from; jne; 0x57E256: mov [material], to
		if (primitive.materialType != from)
		{
			continue;
		}
		// the D3D states of the new type's mode (the type picks the mode in the tables 0xC38728 / 0xC387C8); twoSided,
		// wrap, uvOffset and alphaCutoutThreshold come from the other material bytes, which fn_0057E220 does not touch
		ApplyMode(primitive, to);
	}
}

Mesh& L3DSubMesh::GetMesh() const
{
	return *_mesh;
}

} // namespace openblack
