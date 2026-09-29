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

#include <memory>
#include <vector>

#include <L3DFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "AxisAlignedBoundingBox.h"

#include "../Graphics/RenderPass.h"

namespace openblack::graphics
{
class L3DMesh;
class Mesh;
class ShaderProgram;

class L3DSubMesh
{
public:
	struct Primitive
	{
		enum class BlendMode : uint8_t
		{
			Disabled,
			Standard, ///< src_alpha, 1 - src_alpha
			Additive, ///< src_alpha, 1
		};

		uint32_t skinID;
		uint32_t indicesOffset;
		uint32_t indicesCount;
		bool depthWrite;
		bool alphaTest;
		BlendMode blend;
		bool modulateAlpha;  ///< Multiply ouput alpha by a uniform
		bool thresholdAlpha; ///< Dismiss fragments below a certain threshold
		float alphaCutoutThreshold;
		glm::vec4 colour; ///< material colour (used by untextured primitives)
		bool twoSided;    ///< material byte +5 bit 0: D3DCULL_NONE, else back faces are culled (0x84C34A)
	};

public:
	explicit L3DSubMesh(graphics::L3DMesh& mesh) noexcept;
	~L3DSubMesh() noexcept;

	bool Load(const l3d::L3DFile& l3d, uint32_t meshIndex) noexcept;

	/// A primitive built at run time (L3DMeshGenerated.cpp): the material of an existing primitive and its triangles.
	struct GeneratedPrimitive
	{
		Primitive material;
		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> uvs;
		std::vector<glm::vec3> normals;
		std::vector<uint16_t> indices; ///< into this primitive's vertices
	};
	bool LoadGenerated(const std::vector<GeneratedPrimitive>& primitives) noexcept;

	[[nodiscard]] openblack::l3d::L3DSubmeshHeader::Flags GetFlags() const { return _flags; }
	[[nodiscard]] bool IsPhysics() const { return _flags.isPhysics; }
	[[nodiscard]] graphics::Mesh& GetMesh() const;
	[[nodiscard]] const AxisAlignedBoundingBox& GetBoundingBox() const { return _boundingBox; }
	[[nodiscard]] const std::vector<Primitive>& GetPrimitives() const { return _primitives; }
	/// Bind-pose positions and merged triangle indices, kept on the CPU for ray picking
	/// (LH3DObject::CheckTriangleCollide).
	[[nodiscard]] const std::vector<glm::vec3>& GetCollisionPositions() const { return _collisionPositions; }
	[[nodiscard]] const std::vector<uint16_t>& GetCollisionIndices() const { return _collisionIndices; }
	/// Texture coordinates of the collision positions (FragMesh copies them)
	[[nodiscard]] const std::vector<glm::vec2>& GetCollisionUVs() const { return _collisionUVs; }

private:
	graphics::L3DMesh& _l3dMesh;

	openblack::l3d::L3DSubmeshHeader::Flags _flags;

	std::unique_ptr<graphics::Mesh> _mesh;
	std::vector<Primitive> _primitives;

	AxisAlignedBoundingBox _boundingBox;
	std::vector<glm::vec3> _collisionPositions;
	std::vector<uint16_t> _collisionIndices;
	std::vector<glm::vec2> _collisionUVs;
};
} // namespace openblack::graphics
