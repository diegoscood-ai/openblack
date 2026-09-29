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
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "LandIslandInterface.h"

class btBvhTriangleMeshShape;
class btRigidBody;

namespace openblack
{

namespace lnd
{
struct LNDBlock;
struct LNDCell;
} // namespace lnd

namespace graphics
{
class Mesh;
}

struct LandVertex
{
	glm::vec3 position;
	glm::vec3 weight;                     // interpolated
	glm::u8vec4 firstMaterialID;          // w: bit i set = material i is drawn once per block (terrain-x2 mod)
	glm::u8vec4 secondMaterialID;         // w: the same for the second materials
	glm::u8vec4 materialBlendCoefficient; // force alignment 4 bytes to prevent packing
	glm::u8vec4 lightLevel;               // x: luminosity, yzw: the cell colour as a D3DCOLOR (b, g, r) for the specular
	float waterAlpha;
	glm::vec3 normal; // smooth, from the neighbouring cell altitudes (triplanar cliffs of the terrain-x2 mod)

	LandVertex(const glm::vec3& position, const glm::vec3& weight, const std::array<uint32_t, 6>& mat, const glm::uvec3& blend,
	           uint8_t lightLevel, glm::u8vec3 cellColour, float alpha, const glm::vec3& normal,
	           const std::array<bool, 6>& single = {});
};

class LandIslandInterface;

namespace dynamics
{
class LandBlockBulletMeshInterface;
}

class LandBlock
{
public:
	// 16*16 quads of 2 tris with 3 verts
	static constexpr auto k_Resolution = glm::u8vec2(16, 16);
	static constexpr uint16_t k_VertexCount = k_Resolution.x * k_Resolution.y * 2 * 3;

	LandBlock() = default;
	/// singleMaterials: materials drawn once per block even when the terrain-x2 mod repeats the others (pictures)
	void BuildMesh(LandIslandInterface& island, std::span<const uint8_t> singleMaterials = {});

	[[nodiscard]] const graphics::Mesh& GetMesh() const { return *_mesh; }
	[[nodiscard]] const lnd::LNDCell* GetCells() const;
	[[nodiscard]] glm::ivec2 GetBlockPosition() const;
	[[nodiscard]] glm::vec2 GetMapPosition() const;
	[[nodiscard]] std::unique_ptr<btRigidBody>& GetRigidBody() { return _rigidBody; };
	[[nodiscard]] const std::unique_ptr<lnd::LNDBlock>& GetLndBlock() const { return _block; };
	void SetLndBlock(const lnd::LNDBlock& block);

private:
	std::unique_ptr<lnd::LNDBlock> _block;
	std::unique_ptr<graphics::Mesh> _mesh;
	std::unique_ptr<dynamics::LandBlockBulletMeshInterface> _dynamicsMeshInterface;
	std::unique_ptr<btBvhTriangleMeshShape> _physicsMesh;
	std::unique_ptr<btRigidBody> _rigidBody;

	void BuildVertexList(std::span<LandVertex> vertices, LandIslandInterface& island, std::span<const uint8_t> singleMaterials);
};
} // namespace openblack
