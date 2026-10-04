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
#include <span>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "AxisAlignedBoundingBox.h"

#include "../Graphics/RenderModes.h"
#include "../Graphics/RenderPass.h"

namespace openblack::graphics
{
class L3DMesh;
class Mesh;
class ShaderProgram;

/// GJUtils::MaterialProperties (render_modes, GJUtils::SetMaterialProperties 0x57E120)
using MaterialProperties = render_modes::MaterialProperties;

class L3DSubMesh
{
public:
	struct Primitive
	{
		/// the mode's ALPHABLENDENABLE and SRCBLEND / DESTBLEND (render_modes::k_Modes)
		using BlendMode = render_modes::Blend;

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
		bool wrap;        ///< material byte +5 bit 2: D3DTADDRESS_WRAP, else CLAMP (0x851782)
		/// material byte +5 bit 4 clear: the object's texture offset (+0x68 / +0x6C) is added to the UVs.
		/// LH3DRender::DrawTriangle 0x82F8CC skips the materials with the bit (the rock of waterfall3.l3d).
		bool uvOffset;
		/// the L3D material type (l3d::L3DMaterial::Type) = the mode (render_modes::Mode) the fields above come from
		/// (SetMaterialProperties changes it)
		uint32_t materialType;
	};

public:
	explicit L3DSubMesh(graphics::L3DMesh& mesh) noexcept;
	~L3DSubMesh() noexcept;

	/// `vertices`, when not empty, replaces the file's vertices of this sub-mesh (the same count and order: the hand's
	/// good / evil morph, Morphable::MorphVertices 0x618D10)
	bool Load(const l3d::L3DFile& l3d, uint32_t meshIndex, std::span<const l3d::L3DVertex> vertices = {}) noexcept;
	/// GJUtils::SetMaterialProperties 0x57E120 on every primitive: a new material type (blending, Z write) and the
	/// double-sided bit
	void SetMaterialProperties(const MaterialProperties& properties) noexcept;
	/// The inner loop of fn_0057E220 0x57E220: every primitive whose material type (dword +0) is `from` becomes `to`
	/// (0x57E252..0x57E256). Only the type changes: the byte +5 bits (two-sided, wrap, uv offset) and ALPHAREF stay.
	void ReplaceMaterialType(uint32_t from, uint32_t to) noexcept;

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
	/// Mod graphics.hd-tweaks: a sub-mesh it smooths and lights per pixel, a villager's (boned, all its textures in
	/// EngineConfig::hdTweaksSkins) or the hand's
	[[nodiscard]] bool IsHdTweaked() const { return _hdTweaked; }
	[[nodiscard]] graphics::Mesh& GetMesh() const;
	[[nodiscard]] const AxisAlignedBoundingBox& GetBoundingBox() const { return _boundingBox; }
	[[nodiscard]] const std::vector<Primitive>& GetPrimitives() const { return _primitives; }
	/// Bind-pose positions and merged triangle indices, kept on the CPU for ray picking
	/// (LH3DObject::CheckTriangleCollide).
	[[nodiscard]] const std::vector<glm::vec3>& GetCollisionPositions() const { return _collisionPositions; }
	[[nodiscard]] const std::vector<uint16_t>& GetCollisionIndices() const { return _collisionIndices; }
	/// Texture coordinates of the collision positions (FragMesh copies them)
	[[nodiscard]] const std::vector<glm::vec2>& GetCollisionUVs() const { return _collisionUVs; }
	/// Per collision vertex: the bone of its vertex group (0 without bones, like vs_object's max(0, -1)) and its position
	/// in that bone's space as the file has it, to pose it on the CPU with the bone matrices: the rigid skin of the
	/// skinned branch of fn_00850900 (0x850B04..0x850B2B: one bone matrix per vertex group {count, bone})
	[[nodiscard]] const std::vector<uint16_t>& GetSkinBones() const { return _skinBones; }
	[[nodiscard]] const std::vector<glm::vec3>& GetSkinLocalPositions() const { return _skinLocalPositions; }
	/// Each primitive's triangles in GetCollisionIndices (first index, index count), in the order of GetPrimitives: the
	/// file's, which the hd-tweaks smoothing does not keep in the primitives' own offsets
	[[nodiscard]] const std::vector<std::pair<uint32_t, uint32_t>>& GetCollisionRanges() const { return _collisionRanges; }

private:
	graphics::L3DMesh& _l3dMesh;

	openblack::l3d::L3DSubmeshHeader::Flags _flags;
	bool _hdTweaked {false};

	std::unique_ptr<graphics::Mesh> _mesh;
	std::vector<Primitive> _primitives;

	AxisAlignedBoundingBox _boundingBox;
	std::vector<glm::vec3> _collisionPositions;
	std::vector<uint16_t> _collisionIndices;
	std::vector<glm::vec2> _collisionUVs;
	std::vector<uint16_t> _skinBones;
	std::vector<glm::vec3> _skinLocalPositions;
	std::vector<std::pair<uint32_t, uint32_t>> _collisionRanges;
};
} // namespace openblack::graphics
