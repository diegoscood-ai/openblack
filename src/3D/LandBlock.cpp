/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandBlock.h"

#include <algorithm>
#include <cassert>

#include <ranges>

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <bgfx/bgfx.h>

#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "Graphics/Mesh.h"
#include "Graphics/VertexBuffer.h"

using namespace openblack;
using namespace openblack::graphics;

LandVertex::LandVertex(const glm::vec3& position, const glm::vec3& weight, const std::array<uint32_t, 6>& mat,
                       const glm::uvec3& blend, uint8_t lightLevel, glm::u8vec3 cellColour, float alpha,
                       const glm::vec3& normal, const std::array<bool, 6>& single)
    : position {position}
    , weight {weight}
    , firstMaterialID {static_cast<uint8_t>(mat[0]), static_cast<uint8_t>(mat[1]), static_cast<uint8_t>(mat[2]),
                       static_cast<uint8_t>((single[0] ? 1u : 0u) | (single[1] ? 2u : 0u) | (single[2] ? 4u : 0u))}
    , secondMaterialID {static_cast<uint8_t>(mat[3]), static_cast<uint8_t>(mat[4]), static_cast<uint8_t>(mat[5]),
                        static_cast<uint8_t>((single[3] ? 1u : 0u) | (single[4] ? 2u : 0u) | (single[5] ? 4u : 0u))}
    , materialBlendCoefficient {blend, 0u}
    , lightLevel {lightLevel, cellColour}
    , waterAlpha {alpha}
    , normal {normal}
{
}

void LandBlock::BuildMesh(LandIslandInterface& island, std::span<const uint8_t> singleMaterials)
{
	if (_mesh != nullptr)
	{
		_mesh.reset();
	}

	VertexDecl decl;
	decl.reserve(8);
	decl.emplace_back(VertexAttrib::Attribute::Position, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	// weight
	decl.emplace_back(VertexAttrib::Attribute::TexCoord1, static_cast<uint8_t>(3), VertexAttrib::Type::Float);
	// first material id, w: once-per-block bits
	decl.emplace_back(VertexAttrib::Attribute::Color1, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8);
	// second material id, w: once-per-block bits
	decl.emplace_back(VertexAttrib::Attribute::Color2, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8);
	// material blend coefficient
	decl.emplace_back(VertexAttrib::Attribute::TexCoord2, static_cast<uint8_t>(3), VertexAttrib::Type::Uint8, true);
	// light level, align to 4 bytes
	decl.emplace_back(VertexAttrib::Attribute::Color0, static_cast<uint8_t>(4), VertexAttrib::Type::Uint8, true);
	// water alpha
	decl.emplace_back(VertexAttrib::Attribute::Color3, static_cast<uint8_t>(1), VertexAttrib::Type::Float, true);
	// smooth normal
	decl.emplace_back(VertexAttrib::Attribute::Normal, static_cast<uint8_t>(3), VertexAttrib::Type::Float);

	// reserve 16*16 quads of 2 tris with 3 verts = 1536
	const bgfx::Memory* verticesMem = bgfx::alloc(sizeof(LandVertex) * k_VertexCount);
	auto vertices = std::span(reinterpret_cast<LandVertex*>(verticesMem->data), k_VertexCount);

	BuildVertexList(vertices, island, singleMaterials);

	auto* vertexBuffer = new VertexBuffer("LandBlock", verticesMem, decl);
	_mesh = std::make_unique<Mesh>(vertexBuffer);

	_dynamicsMeshInterface = std::make_unique<dynamics::LandBlockBulletMeshInterface>(vertices);

	_physicsMesh = std::make_unique<btBvhTriangleMeshShape>(_dynamicsMeshInterface.get(), true);
	_rigidBody = std::make_unique<btRigidBody>(0.0f, nullptr, _physicsMesh.get());
	btTransform transform;
	transform.setIdentity();
	transform.setOrigin(btVector3(_block->mapX, 0, _block->mapZ));
	_rigidBody->setWorldTransform(transform);
	_rigidBody->setContactStiffnessAndDamping(300, 10);
	_rigidBody->setUserIndex(-1);
}

void LandBlock::BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island,
                                std::span<const uint8_t> singleMaterials)
{
	auto countries = island.GetCountries();

	// auto neighbourBlockR = island.GetBlock(glm::u8vec2(_block->blockX + 1, _block->blockZ));
	// auto neighbourBlockUp = island.GetBlock(glm::u8vec2(_block->blockX, _block->blockZ + 1));

	// we'll loop through each cell, 16x16
	// (the array is 17x17 but the 17th block is questionable data)

	const auto blockOffset = static_cast<glm::u16vec2>(GetBlockPosition() * 16);

	uint16_t index = 0;
	for (int x = 0; x < 16; x++)
	{
		for (int z = 0; z < 16; z++)
		{
			enum class Corner
			{
				TopLeft,
				TopRight,
				BottomLeft,
				BottomRight,

				_COUNT
			};

			std::array<glm::u16vec2, static_cast<size_t>(Corner::_COUNT)> offsets;
			offsets[static_cast<size_t>(Corner::TopLeft)] = glm::u16vec2(x, z);
			offsets[static_cast<size_t>(Corner::TopRight)] = glm::u16vec2(x + 1, z);
			offsets[static_cast<size_t>(Corner::BottomLeft)] = glm::u16vec2(x, z + 1);
			offsets[static_cast<size_t>(Corner::BottomRight)] = glm::u16vec2(x + 1, z + 1);

			std::array<const lnd::LNDCell*, static_cast<size_t>(Corner::_COUNT)> cells;
			// construct positions from cell altitudes
			std::array<glm::vec3, static_cast<size_t>(Corner::_COUNT)> pos;
			std::array<glm::vec3, static_cast<size_t>(Corner::_COUNT)> normals;
			std::array<const lnd::LNDMapMaterial*, static_cast<size_t>(Corner::_COUNT)> materials;
			for (auto [position, normal, cell, material, offset] : std::views::zip(pos, normals, cells, materials, offsets))
			{
				const auto coordinates = blockOffset + offset;
				cell = &island.GetCell(coordinates);
				position =
				    glm::vec3(offset.x * LandIslandInterface::k_CellSize,
				              static_cast<float>(island.GetCellAltitude(*cell)) * LandIslandInterface::k_HeightUnit,
				              offset.y * LandIslandInterface::k_CellSize);

				// central differences of the neighbouring altitudes (clamped at the map edge)
				const auto height = [&island](int cx, int cz) {
					const int last = island.GetCellsPerSide() - 1;
					const auto clamped = glm::u16vec2(std::clamp(cx, 0, last), std::clamp(cz, 0, last));
					return static_cast<float>(island.GetCellAltitude(island.GetCell(clamped))) * LandIslandInterface::k_HeightUnit;
				};
				const int cx = coordinates.x;
				const int cz = coordinates.y;
				normal = glm::normalize(glm::vec3(height(cx - 1, cz) - height(cx + 1, cz), 2.0f * LandIslandInterface::k_CellSize,
				                                  height(cx, cz - 1) - height(cx, cz + 1)));

				const auto& country = countries.at(cell->properties.country);
				const auto noise = island.GetNoise(blockOffset + offset);

				// BWLandEditor maps above altitude 255 keep the top material (as the editor draws them)
				const auto altitude = island.GetCellAltitude(*cell);
				material = altitude > 255 ? &country.materials.back()
				                          : &country.materials.at((altitude + noise) % country.materials.size());
			}

			// TODO(470): This is temporary way for drawing landscape, should be moved to a shader in the renderer
			// Using a lambda so we're not repeating ourselves
			auto getAlpha = [](lnd::LNDCell::Properties properties) {
				if (properties.hasWater || properties.fullWater)
				{
					return 0.0f;
				}
				if (properties.coastLine)
				{
					return 0.5f;
				}
				return 1.0f;
			};
			auto makeVert = [&getAlpha, &pos, &normals, &cells, &materials, singleMaterials](Corner corner, const glm::vec3& weight,
			                                                      const std::array<Corner, 3>& m) -> LandVertex {
				const std::array<uint32_t, 6> mat = {
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->indices[0],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->indices[0],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->indices[0],

				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->indices[1],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->indices[1],
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->indices[1],
				};
				std::array<bool, 6> single {};
				for (size_t i = 0; i < single.size(); ++i)
				{
					single[i] = mat[i] < singleMaterials.size() && singleMaterials[mat[i]] != 0;
				}
				const glm::u32vec3 blend = {
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[0])]->coefficient,
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[1])]->coefficient,
				    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				    materials[static_cast<size_t>(m[2])]->coefficient,
				};
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				const auto& cell = *cells[static_cast<size_t>(corner)];
				// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				// vertex specular = the cell's first dword read as a D3DCOLOR (fn_00874AA0): r, g, b bytes -> blue, green, red
				return {pos[static_cast<size_t>(corner)], weight, mat, blend, cell.luminosity,
				        glm::u8vec3(cell.b, cell.g, cell.r), getAlpha(cell.properties),
				        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
				        normals[static_cast<size_t>(corner)], single};
			};

			auto makeTriangle = [&makeVert, &vertices, &index](const std::array<Corner, 3>& corners, bool forward) {
				if (forward)
				{
					vertices[index++] = makeVert(corners[0], glm::vec3(1, 0, 0), corners);
					vertices[index++] = makeVert(corners[1], glm::vec3(0, 1, 0), corners);
					vertices[index++] = makeVert(corners[2], glm::vec3(0, 0, 1), corners);
				}
				else
				{
					vertices[index++] = makeVert(corners[2], glm::vec3(0, 0, 1), corners);
					vertices[index++] = makeVert(corners[1], glm::vec3(0, 1, 0), corners);
					vertices[index++] = makeVert(corners[0], glm::vec3(1, 0, 0), corners);
				}
			};

			// cell splitting
			// winding order = clockwise
			if (!cells[static_cast<size_t>(Corner::TopLeft)]->properties.split)
			{
				makeTriangle({Corner::TopLeft, Corner::TopRight, Corner::BottomRight}, true);    //  ┐
				makeTriangle({Corner::TopLeft, Corner::BottomLeft, Corner::BottomRight}, false); // └
			}
			else
			{
				makeTriangle({Corner::BottomLeft, Corner::TopLeft, Corner::TopRight}, true);      // ┌
				makeTriangle({Corner::BottomLeft, Corner::BottomRight, Corner::TopRight}, false); //  ┘
			}
		}
	}
}

const lnd::LNDCell* LandBlock::GetCells() const
{
	assert(_block);
	return _block ? _block->cells.data() : nullptr;
}

glm::ivec2 LandBlock::GetBlockPosition() const
{
	assert(_block);
	return {_block ? _block->blockX : -1, _block ? _block->blockZ : -1};
}

glm::vec2 LandBlock::GetMapPosition() const
{
	assert(_block);
	return {_block->mapX, _block->mapZ};
}

void LandBlock::SetLndBlock(const lnd::LNDBlock& block)
{
	_block = std::make_unique<lnd::LNDBlock>(block);
}
