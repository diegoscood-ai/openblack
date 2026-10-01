/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{

/// RootsPile (the name is the Mac build's: __ct__9RootsPileFRC7LHPointffl, DrawAll__9RootsPileFv; 0x0C bytes {next,
/// LH3DObject*, ms left}, list 0xEB9A00): a pack mesh lying on the land for 15 s that fades out in its last second. The
/// crater of the beam explosion (UR_Explosion::InitCollection 0x67E395: mesh 0x251 = TreeRootsPile, scale [0x9357D4] =
/// 8, angle PSysFloatRand(2 pi), only on dry land) and the roots pile of an uprooted tree (fn_0074BD20 ->
/// fn_008251F0; that one is still HandSystem::Uproot's own copy). Not TemporaryShadow: that is fn_00825090 (list
/// 0xEB99FC, a dynamic shadow).
class RootsPile
{
public:
	/// RootsPile::RootsPile(const LHPoint&, float angle, float scale, long mesh) fn_008251C0 -> fn_00825240:
	/// - the mesh is MeshPack[mesh] (index 0 when out of range), cached in [0xEB9A04] by the first pile ever made: every
	///   later pile takes that one whatever its argument (0x825240..0x825265; both callers pass 0x251);
	/// - LH3DObject::Create(1) 0x80B4D0 (a morphable object, vtable 0x9A2E34), SetMesh (vt 0xF4), SetPosition 0x423140
	///   (vt 0x20: origin at the point, turned by the angle about Y, the scale on all three axes), vt 0x58(1) (flag 0x20
	///   only with the Light detail [0xC38224]), UpdateMelting 0x8168F0 (vt 0x1E8: draped on the land once), vt 0x40(0)
	///   (flag 0x10 off);
	/// - +8 = 0x3A98 = 15000 ms; SmokyStuff::Create(point, 1, 1.0, -1) 0x8252EB (the brown dust puff, mode 1).
	/// (pendiente) the two LH3DObject draw flags have no openblack counterpart: 0x20 (fn_008168C0, only with the Light
	/// detail) and 0x10 off (fn_007F97A0). The pile casts no static shadow either way (RenderingSystem's
	/// CastsStaticShadow asks for Fixed / MobileStatic / ... , which it is not).
	/// Returns the entity drawn for it (entt::null when the mesh is not loaded, the unit tests).
	static entt::entity Create(const glm::vec3& position, float angle, float scale, int32_t mesh);

	/// RootsPile::DrawAll fn_00825350, every frame from fn_005E5CD0 (0x5E6197, just before SmokyStuff's fn_00824140),
	/// with g_game_time_inc in ms: per pile the land light (fn_00801C90), with <= 1000 ms left the colour's alpha =
	/// ftol(ms x 0.255) and SetGlobalAlpha(1) (vt 0x48); then ms -= g_game_time_inc, and at <= 0 the pile goes
	/// (fn_00825300: out of the list, the LH3DObject deleted), else AddDrawing 0x815A70 (vt 0x100).
	static void DrawAll(int32_t milliseconds);

	/// ClearAllStuff 0x82AEFD: every pile goes (a new map)
	static void Clear();

	[[nodiscard]] static size_t Count();

	static constexpr int32_t k_LifeMilliseconds = 15000; ///< 0x8252DD: +8 = 0x3A98
	static constexpr int32_t k_FadeMilliseconds = 1000;  ///< 0x82537C: cmp 0x3E8
	static constexpr float k_AlphaPerMillisecond = 0.255f; ///< [0x9A2BA8]

	/// fn_00825350's alpha byte for the ms left (255 at the start of the fade; the caller only asks with <= 1000)
	[[nodiscard]] static int AlphaFor(int32_t millisecondsLeft);

	RootsPile() = delete;
};

} // namespace openblack::ecs
