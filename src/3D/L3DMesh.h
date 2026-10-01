/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <L3DFile.h>
#include <glm/gtc/quaternion.hpp>

#include "AxisAlignedBoundingBox.h"
#include "Graphics/Mesh.h"
#include "Graphics/ShaderProgram.h"
#include "L3DSubMesh.h"

class btConvexShape;

namespace openblack
{
namespace l3d
{
class L3DFile;
}

constexpr std::array<std::string_view, 32> k_L3DMeshFlagNames {
    "Unknown1",
    "Unknown2",
    "Unknown3",
    "Unknown4",
    "Unknown5",
    "Unknown6",
    "Unknown7",
    "Unknown8",
    "HasBones",
    "Unknown10",
    "HasChimney",
    "HasDoorPosition",
    "Packed",
    "NoDraw",
    "Unknown15",
    "ContainsLandscapeFeature",
    "Unknown17",
    "Unknown18",
    "ContainsUV2",
    "ContainsNameData",
    "ContainsExtraMetrics",
    "ContainsEBone",
    "ContainsTnLData",
    "ContainsNewEP",
    "Unknown25",
    "Unknown26",
    "Unknown27",
    "Unknown28",
    "Unknown29",
    "Unknown30",
    "Unknown31",
    "Unknown32",
};
} // namespace openblack

namespace openblack::graphics
{
class L3DSubMesh;

using SkinId = uint32_t;

// todo: template this
inline l3d::L3DMeshFlags operator&(l3d::L3DMeshFlags a, l3d::L3DMeshFlags b)
{
	return static_cast<l3d::L3DMeshFlags>(static_cast<std::underlying_type<l3d::L3DMeshFlags>::type>(a) &
	                                      static_cast<std::underlying_type<l3d::L3DMeshFlags>::type>(b));
}

class L3DMesh
{
public:
	struct Footprint
	{
		std::unique_ptr<graphics::Texture2D> texture;
		std::unique_ptr<graphics::Mesh> mesh;
	};
	explicit L3DMesh(std::string debugName = "") noexcept;
	virtual ~L3DMesh() noexcept;

	bool Load(const l3d::L3DFile& l3d) noexcept;
	bool LoadFromFilesystem(const std::filesystem::path& path) noexcept;
	bool LoadFromFile(const std::filesystem::path& path) noexcept;
	bool LoadFromBuffer(const std::vector<uint8_t>& data) noexcept;
	/// One sub-mesh from run-time triangles (L3DMeshGenerated.cpp)
	bool LoadGenerated(const std::vector<L3DSubMesh::GeneratedPrimitive>& primitives) noexcept;

	[[nodiscard]] uint8_t GetNumSubMeshes() const { return static_cast<uint8_t>(_subMeshes.size()); }
	[[nodiscard]] const std::vector<std::unique_ptr<L3DSubMesh>>& GetSubMeshes() const { return _subMeshes; }
	[[nodiscard]] const std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>>& GetSkins() const { return _skins; }
	[[nodiscard]] const std::vector<Footprint>& GetFootprints() const
	{
		return _footprintSource ? _footprintSource->GetFootprints() : _footprints;
	}
	/// A generated mesh (a broken building) keeps the landscape footprint of the mesh it was made from.
	void SetFootprintSource(std::shared_ptr<const L3DMesh> source) noexcept;
	[[nodiscard]] const std::vector<uint32_t>& GetBoneParents() const { return _bonesParents; }
	[[nodiscard]] const std::vector<glm::mat4>& GetBoneMatrices() const { return _bonesDefaultMatrices; }
	[[nodiscard]] const std::optional<glm::vec3>& GetDoorPos() const { return _doorPos; }
	/// The chimney (flag HasChimney 0x400): extra point [1] in the mesh space (LH3DStaticObject::GetChimneyPos 0x7F9F10)
	[[nodiscard]] const std::optional<glm::vec3>& GetChimneyPos() const { return _chimneyPos; }
	[[nodiscard]] const std::vector<glm::mat4>& GetExtraMetrics() const { return _extraMetrics; }
	/// Ground blob points of the EBone block (animals): bone index and position in that bone's space, 2 or 4 of them
	[[nodiscard]] const std::vector<std::pair<uint32_t, glm::vec3>>& GetBlobPoints() const { return _blobPoints; }
	/// The first point of the EBone block (bones[0], matrices[0] position), whatever the others: the shark's wake
	/// (fn_00774E30 0x775049-0x7750A3). Empty without an EBone block.
	[[nodiscard]] const std::optional<std::pair<uint32_t, glm::vec3>>& GetEBonePoint0() const { return _eBonePoint0; }
	[[nodiscard]] bool HasPhysicsMesh() const { return _physicsMesh != nullptr; }
	[[nodiscard]] btConvexShape& GetPhysicsMesh() { return *_physicsMesh; }
	[[nodiscard]] const btConvexShape& GetPhysicsMesh() const { return *_physicsMesh; }
	[[nodiscard]] float GetMass() const { return _physicsMass; }
	[[nodiscard]] AxisAlignedBoundingBox GetBoundingBox() const { return _boundingBox; }
	/// Nearest hit of the ray origin + t * direction (mesh space, direction need not be unit) with the triangles of the
	/// drawn (non-physics) sub-meshes, both faces. Returns t.
	[[nodiscard]] std::optional<float> RayIntersect(const glm::vec3& origin, const glm::vec3& direction) const noexcept;

private:
	l3d::L3DMeshFlags _flags;
	std::string _debugName;

	std::unordered_map<SkinId, std::unique_ptr<graphics::Texture2D>> _skins;
	std::vector<Footprint> _footprints; ///< If ContainsLandscapeFeature() is true
	std::shared_ptr<const L3DMesh> _footprintSource;
	std::vector<std::unique_ptr<L3DSubMesh>> _subMeshes;
	std::vector<uint32_t> _bonesParents;
	std::vector<glm::mat4> _bonesDefaultMatrices;
	std::optional<glm::vec3> _doorPos;
	std::optional<glm::vec3> _chimneyPos;
	std::vector<glm::mat4> _extraMetrics;
	std::vector<std::pair<uint32_t, glm::vec3>> _blobPoints;
	std::optional<std::pair<uint32_t, glm::vec3>> _eBonePoint0;
	/// Bounding box if no physics mesh was found
	std::unique_ptr<btConvexShape> _physicsMesh;
	float _physicsMass {1.0f}; // TODO(bwrsandman): Find somewhere in file a value
	AxisAlignedBoundingBox _boundingBox {
	    {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
	    {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()},
	};
	std::string _nameData;

public:
	[[nodiscard]] const std::string& GetDebugName() const { return _debugName; }
	[[nodiscard]] const std::string& GetNameData() const { return _nameData; }

	[[nodiscard]] uint32_t GetFlags() const { return static_cast<uint32_t>(_flags); }

	[[nodiscard]] bool IsBoned() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasBones); }
	[[nodiscard]] bool HasDoorPosition() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasDoorPosition); }
	[[nodiscard]] bool HasChimney() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::HasChimney); }
	[[nodiscard]] bool IsPacked() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::Packed); }
	[[nodiscard]] bool IsNoDraw() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::NoDraw); }
	[[nodiscard]] bool ContainsLandscapeFeature() const
	{
		return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsLandscapeFeature);
	}
	[[nodiscard]] bool IsContainsUV2() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsUV2); }
	[[nodiscard]] bool IsContainsNameData() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsNameData); }
	[[nodiscard]] bool ContainsExtraMetrics() const
	{
		return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsExtraMetrics);
	}
	[[nodiscard]] bool IsContainsEBone() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsEBone); }
	[[nodiscard]] bool IsContainsTnLData() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsTnLData); }
	[[nodiscard]] bool IsContainsNewEP() const { return static_cast<bool>(_flags & l3d::L3DMeshFlags::ContainsNewEP); }
	// const bool IsContainsNewData() const { return _flags & 0xFC8000; } // ???
};
} // namespace openblack::graphics
