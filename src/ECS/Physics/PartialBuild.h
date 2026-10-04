/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/L3DSubMesh.h"

namespace openblack::ecs::physics
{
/// What the callers of DrawPartialyBuilt fn_00816AD0 set around it
struct PartialBuildOptions
{
	/// [0xC392AC] / [0xC392B0], the inner wall offset for both cull modes, in model units. Unset: the defaults
	/// (PartialBuild::k_InnerOffsetTwoSided / k_InnerOffsetCulled). LH3DCitadel's draw 0x882A40 sets both to
	/// PartialBuild::k_TempleInnerOffset around its call (0x882A7B / 0x882A85) and restores them (0x882A9D / 0x882AA7)
	std::optional<float> innerOffset;
	/// The melting stream (LH3DObject vt+0x1F0: 1 in the morphable 0x9A2E34 and citadel 0x9A2BFC vtables, 0 in the static
	/// 0x9A2974 and complex 0x9A3068 ones): each vertex's land delta (UpdateMelting 0x8168F0, obj+0x80) added to its
	/// model y before the cut test and the matrix. Unset: whether the entity has components::MorphWithTerrain
	std::optional<bool> melting;
	/// LH3DObject +8 bit 0x200: no cap (0x85CF3B..0x85CF41). (pending) no writer of the bit was found in the exe; openblack
	/// has no equivalent, so it is clear unless a caller sets it
	bool noCap {false};
	/// MultiMapFixed::DrawBuilding 0x517FE0 calls it only for pct != 0 (nothing at all is drawn at 0); LH3DCitadel 0x882A40
	/// calls it for any +0x9C < 1, so at 0 the scaffold is still drawn, lowered by the whole height
	bool skipZero {true};
};

/// The partly built draw of a building: DrawPartialyBuilt fn_00816AD0 (LH3DStaticObject vt+0x110, also the morphable and
/// complex ones; MultiMapFixed::DrawBuilding 0x517F90 and LH3DCitadel 0x882A40 call it), in world space. The status 0
/// sub-meshes are cut at the plane y = pos.y + pct x H x scale (fn_0085C7F0: nothing of a primitive when that is under
/// 0.2 above the origin), each primitive again pushed in along its vertex normals (fn_0085C0E0: 0.35, 0.2 for two-sided
/// materials, model units) and a cap joining both cuts (fn_00820B20); the scaffold (the highest status) rises
/// (pct < 0.2), stands, or is cut from the top (pct > 0.8). A damaged building draws it over its FragMesh, at
/// GetPercentForDrawBuilding.
class PartialBuild
{
public:
	static constexpr float k_InnerOffsetTwoSided = 0.2f; ///< [0xC392AC] (0x3E4CCCCD), material +5 bit 0 set
	static constexpr float k_InnerOffsetCulled = 0.35f;  ///< [0xC392B0] (0x3EB33333)
	static constexpr float k_TempleInnerOffset = 1.0f;   ///< LH3DCitadel 0x882A7B / 0x882A85 (both cull modes)

	[[nodiscard]] static std::vector<graphics::L3DSubMesh::GeneratedPrimitive>
	Build(entt::entity building, entt::id_type mesh, float percent, const PartialBuildOptions& options = {});
	/// Build, moved into the building's own space (its Transform) and loaded as a mesh named "<tag>/<n>", with the intact
	/// model as its mark on the landscape: what DrawBuilding 0x517F90 draws for pct != 0. 0 when nothing is left. When the
	/// entity keeps components::MorphWithTerrain the melting deltas only decide the cut: the vertex shader adds them.
	[[nodiscard]] static entt::id_type BuildMesh(entt::entity building, entt::id_type intactMesh, float percent,
	                                             std::string_view tag, const PartialBuildOptions& options = {});
	/// Erases a mesh BuildMesh made (0: nothing)
	static void EraseMesh(entt::id_type id);
	PartialBuild() = delete;
};
} // namespace openblack::ecs::physics
