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

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "Graphics/RenderModes.h"
#include "Graphics/RenderPass.h"

/// The pass under the sea of GLandscape::Draw (0x5E48AE..0x5E4E8C), CPU side; the GPU twin is
/// assets/shaders/sea_plane.sh and the two must stay the same. Wiki: rendering-objects.md, "La pasada bajo el mar".
///
/// The original draws, before the sea and into the frame itself, three kinds of things:
/// - A, the mirrored land (fn_007FF4F0): the height unit [0xC3720C] = 0.67 times [0x8AB678] = -1.0 (0x7FF515..0x7FF52F),
///   [0xFA92DC] = 1 (0x7FF535: the land blocks swap their indices), the light table >> 1 (0x7FF53F..0x7FF564), no small
///   bump ([0xC37210] = 0, 0x7FF566..0x7FF577) and no Z write (0x5E48C5..0x5E4900);
/// - B, LH3DObject vt+0x118 DrawUnderWater (fn_00811010 static, fn_00810E20 animated, fn_00813300 complex, all to the
///   vertex routine fn_00850FC0): the object mirrored in y = 0 (fsubp 0x851094 / 0x8510BD / 0x8510E5), clipped
///   against the user plane on the mirrored point (d > 0 out, 0x85111C..0x851149), in a constant colour obj+0x4C /
///   +0x50 (SetColorSpecular vt+0x2C before the call);
/// - C, LH3DObject vt+0x11C DrawCutByPlane (fn_0080C050 static / complex, fn_00811C70 animated, to fn_00858BA0):
///   NOT mirrored (0x858C5D..0x858CAE), clipped on the point itself (d < 0 out, 0x858D08..0x858D86), lit
///   90 + 165 I >> 8 in obj+0x4C with the light in the object's space (fn_00855340, 0x80C0EE).
/// B and C share one CPU clipper, fn_0081D2C0 (its only callers 0x8515E7 / 0x8516AD and 0x85930E / 0x8593C5), against
/// the one user plane of fn_00822560 (world [0xF03128], view [0xF03118]).
///
/// openblack draws that pass into the reflection target with a mirrored camera (ReflectionXZCamera) instead, which
/// already gives B's mirror; what C draws there is mirrored back ("unmirror"), and the plane is a fragment discard on
/// the REAL world y (fs_object). Every rule here only says which of those three things a draw is and what follows.
namespace openblack::graphics::sea_pass
{

/// fn_00822560's plane read on the real world y: which side of y = 0 a draw keeps
enum class SeaPlane : int8_t
{
	None = 0,      ///< not a sea draw: no discard
	KeepAbove = 1, ///< y >= 0 (the default plane, with B and with C)
	KeepBelow = -1 ///< y <= 0 (C with a plane (0, -1, 0, 0): the shark's, the net's, the swimmers')
};

/// The plane of the CRT initialisers, world fn_0084A380 (0x84A390..0x84A3AE: [0xF0312C] = 0x3F800000 = 1.0f, the rest
/// 0) and view fn_0084A3C0 (0x84A3D0..: [0xF0311C] = 1.0f), and the one every user puts back (0x5E4D76)
inline constexpr glm::vec4 k_DefaultPlane {0.0f, 1.0f, 0.0f, 0.0f};
/// The swimmers' plane: GLandscape::Draw 0x5E4C4A..0x5E4C5A (dwords, [esp+0x44] = 0xBF800000 = -1.0f), then
/// fn_00822560 0x5E4C5E
inline constexpr glm::vec4 k_SwimPlane {0.0f, -1.0f, 0.0f, 0.0f};
/// The net's own plane: fn_00829BC0 0x829C91..0x829CB4 (dwords, [esp+0x28] = 0xBF800000 = -1.0f at 0x829C9C, the rest
/// 0), fn_00822560 0x829CB7, then the default plane again (0x829D25..0x829D45). It runs in fn_00824B90 (0x5E4B2B),
/// before the swimmers' loop: not k_SwimPlane's write, the same values
inline constexpr glm::vec4 k_NetPlane {0.0f, -1.0f, 0.0f, 0.0f};
/// The shark's own plane under the water: fn_00774E30 0x774FF5..0x775015 (dwords, [esp+0x50] = 0xBF800000 = -1.0f at
/// 0x774FFD, the rest 0), fn_00822560 0x77501A, then the default plane again (0x7750E6..0x775106). It runs in
/// fn_00775120 (0x5E4B26), before the swimmers' loop: not k_SwimPlane's write, the same values
inline constexpr glm::vec4 k_SharkPlane {0.0f, -1.0f, 0.0f, 0.0f};

/// The two LH3DObject draws of the pass
enum class Mechanism : uint8_t
{
	UnderWater, ///< B, vt+0x118: the plane tested on the mirrored point, d > 0 out (fn_00850FC0 0x85113C..0x851149)
	CutByPlane  ///< C, vt+0x11C: the plane tested on the point, d < 0 out (fn_00858BA0 0x858D49..0x858D80)
};

/// The side a plane (0, b, 0, 0) keeps on the real y. B: d = b (-y) > 0 is out, so b > 0 keeps y >= 0; C: d = b y < 0
/// is out, so b > 0 keeps y >= 0 too: the mirror and the opposite test cancel. Both compare d with [0x8AA398] = 0.0f
/// (fcomp; B `test ah, 0x41 / jne`, C `test ah, 1 / je` 0x858D7B). (openblack) a plane with x, z or w != 0 is not one
/// any caller of the pass sets: None
[[nodiscard]] constexpr SeaPlane Kept([[maybe_unused]] Mechanism mechanism, glm::vec4 plane)
{
	if (plane.x != 0.0f || plane.z != 0.0f || plane.w != 0.0f || plane.y == 0.0f)
	{
		return SeaPlane::None;
	}
	return plane.y > 0.0f ? SeaPlane::KeepAbove : SeaPlane::KeepBelow;
}

/// CPU twin of sea_plane.sh SeaPlaneDiscard: whether a point at the real worldY stays. (aproximado) strict per pixel,
/// y = 0 is kept by both sides; the original clips whole triangles (fn_0081D2C0)
[[nodiscard]] constexpr bool KeptAt(SeaPlane plane, float worldY)
{
	switch (plane)
	{
	case SeaPlane::KeepAbove:
		return !(worldY < 0.0f);
	case SeaPlane::KeepBelow:
		return !(worldY > 0.0f);
	case SeaPlane::None:
		break;
	}
	return true;
}

/// What a sea draw lights with: one rule per mechanism, never mixed
enum class SeaLight : uint8_t
{
	Normal,   ///< not a sea draw: the model light of the normal Draw
	Constant, ///< B: a constant obj+0x4C / +0x50 read by fn_00811010, no vertex light. The hand sets both with
	          ///< SetColorSpecular (0x5E496C..0x5E4975); the boat writes only +0x4C (`mov [eax+0x4C], 0xFF303070`
	          ///< 0x5E016C) and leaves +0x50 as its hull's last Draw left it, so its specular 0 is (inferido)
	LastDraw, ///< B: obj+0x4C / +0x50 as the last Draw left them, fn_00801C90's land light and cell specular
	          ///< (PhysicsObject::DrawAll 0x646F9F), no vertex light, no haze
	Cut       ///< C: fn_00858BA0, 90 + 165 I >> 8 (R14: [0xC39264] = 90, 0x858CE1..0x858D03) in obj+0x4C, + obj+0x50
};

/// One sea draw: the light, the plane and whether it is mirrored back
struct SeaDraw
{
	SeaLight light {SeaLight::Normal};
	SeaPlane plane {SeaPlane::None};
	/// a vt+0x11C draw (never mirrored, 0x858C5D..0x858CAE) inside openblack's mirrored Reflection pass
	bool unmirror {false};
	uint32_t argb {0xFFFFFFFFu}; ///< Constant / Cut: SetColorSpecular's colour (obj+0x4C), 0xAARRGGBB
	uint32_t specular {0u};      ///< Constant / Cut: SetColorSpecular's specular (obj+0x50)
	/// Cut: each instance's own colour instead of argb (the PSys mesh atoms, DrawData +8 / +0xC: Particle3DObj::DrawAt
	/// 0x67A01C / 0x67A02F before vt+0x11C 0x679F4A)
	bool perInstanceColour {false};
};

/// B, a constant colour (vt+0x2C then vt+0x118): KeepAbove with the default plane, mirrored by the pass
[[nodiscard]] constexpr SeaDraw UnderWater(uint32_t argb, uint32_t specular)
{
	return {.light = SeaLight::Constant,
	        .plane = Kept(Mechanism::UnderWater, k_DefaultPlane),
	        .unmirror = false,
	        .argb = argb,
	        .specular = specular};
}
/// B, in the colour the last Draw left (PhysicsObject::DrawAll 0x646F9F, the held object 0x5E49A2)
[[nodiscard]] constexpr SeaDraw UnderWaterLastDraw()
{
	return {.light = SeaLight::LastDraw, .plane = Kept(Mechanism::UnderWater, k_DefaultPlane), .unmirror = false};
}
/// C in a pass: mirrored back inside openblack's mirrored Reflection pass, as it is everywhere else
[[nodiscard]] constexpr SeaDraw Cut(SeaPlane plane, uint32_t argb, uint32_t specular, RenderPass pass)
{
	return {.light = SeaLight::Cut,
	        .plane = plane,
	        .unmirror = pass == RenderPass::Reflection,
	        .argb = argb,
	        .specular = specular};
}

/// C for the PSys mesh atoms with DrawCutByPlane (fn_00679F20: +0x24 & 4 `test al, 4` 0x679F29, vt+0x11C 0x679F4A),
/// in the pass's default plane (they draw in the model pass, after GLandscape::Draw put it back, 0x5E4D76) and each in
/// its own DrawData colour and specular (SetColorSpecular vt+0x2C 0x67A02F). (aproximado) the vertex alpha is 0xFF:
/// fn_00858BA0 takes it from obj+0x4C & 0xFF000000 ([0xC37D8C] 0x858C42 -> [ebp-0x24], OR'd in at 0x858D60), i.e.
/// DrawData+8's alpha, which psys::mesh_atoms::Instance does not carry yet
[[nodiscard]] constexpr SeaDraw CutAtoms(RenderPass pass)
{
	auto draw = Cut(Kept(Mechanism::CutByPlane, k_DefaultPlane), 0xFFFFFFFFu, 0u, pass);
	draw.perInstanceColour = true;
	return draw;
}

/// The surfaces whose culling the pass decides
enum class Surface : uint8_t
{
	Model, ///< every LH3DObject: the material's CULLMODE
	Sky,   ///< the sky dome, culled as a whole (inferido: the sky meshes' own modes)
	Land   ///< the land blocks
};

/// One pass's state: GLandscape::Draw 0x5E48B3..0x5E4900 + fn_007FF4F0
struct SeaPassState
{
	bool mirrored;        ///< RenderPass::Reflection: openblack's mirrored camera (ReflectionXZCamera)
	float landLightScale; ///< 0.5: the land light table >> 1 (fn_007FF4F0 0x7FF53F..0x7FF564), else 1
	bool landWriteZ;      ///< false: ZWRITEENABLE 0 around the mirrored land (GLandscape::Draw 0x5E48C5..0x5E4900)
	bool smallBump;       ///< false: UseSmallBump [0xC37210] = 0 (fn_007FF4F0 0x7FF566..0x7FF577)

	/// D3DRS_CULLMODE in one place. Model: the material's ((~mat+5) & 1) 2 + 1 (the Draw 0x84C34A, B 0x851798..0x8517D1,
	/// C 0x8594CF..0x8594ED, `push 0x16`), flipped by the mirrored camera and flipped back by unmirror. Sky: the CCW of
	/// the dome, flipped by the mirrored camera. Land: openblack's blocks wind the other way (an openblack fact, no
	/// address), so it starts from Cw; the original's mirrored land swaps its indices instead ([0xFA92DC] = 1, 0x7FF535;
	/// read in fn_00875C60 0x875D7B / 0x8766B7 and fn_00876910 0x876A2A / 0x87707B)
	[[nodiscard]] constexpr render_modes::Cull FaceCull(Surface surface, bool twoSided, bool unmirror) const
	{
		switch (surface)
		{
		case Surface::Sky:
			return mirrored ? render_modes::Cull::Cw : render_modes::Cull::Ccw;
		case Surface::Land:
			return mirrored ? render_modes::Cull::Ccw : render_modes::Cull::Cw;
		case Surface::Model:
			break;
		}
		return render_modes::CullFor(twoSided, mirrored && !unmirror);
	}
};

/// The state of a pass: only RenderPass::Reflection is the pass under the sea
[[nodiscard]] constexpr SeaPassState ForPass(RenderPass pass)
{
	const bool reflection = pass == RenderPass::Reflection;
	return {.mirrored = reflection, .landLightScale = reflection ? 0.5f : 1.0f, .landWriteZ = !reflection,
	        .smallBump = !reflection};
}

/// u_objectClip (sea_plane.sh): x the plane (1 KeepAbove, -1 KeepBelow, 0 None), y 1 = unmirror
[[nodiscard]] constexpr glm::vec4 PackClip(const SeaDraw& draw)
{
	return {static_cast<float>(static_cast<int8_t>(draw.plane)), draw.unmirror ? 1.0f : 0.0f, 0.0f, 0.0f};
}
/// u_objectClip of a draw that is not a sea draw (the sky, the mesh viewer): no plane, no unmirror
inline constexpr glm::vec4 k_NoClip {0.0f, 0.0f, 0.0f, 0.0f};

/// A point drawn through the mirrored camera back to where the original draws it: (x, -y, z), fn_00850FC0's fsubp
/// (0x851094) undone
[[nodiscard]] constexpr glm::vec3 Unmirror(glm::vec3 p)
{
	return {p.x, -p.y, p.z};
}
/// A view matrix of the mirrored camera times diag(1, -1, 1, 1): the main view (ReflectionXZCamera::GetViewMatrix), so
/// what it draws lands where Unmirror puts it (the moon's DrawUnderWater, fn_0086A930 0x86AC46)
[[nodiscard]] inline glm::mat4 UnmirrorView(const glm::mat4& view)
{
	glm::mat4 result = view;
	result[1] = -view[1];
	return result;
}

// The constant colours of the pass, SetColorSpecular (vt+0x2C, edx = colour, the pushed dword = specular) before
// the draw
inline constexpr uint32_t k_HandColour = 0x65A0A0A0u;     ///< the hand, `mov edx, 0x65A0A0A0` 0x5E496E
inline constexpr uint32_t k_HandSpecular = 0u;            ///< `push 0` 0x5E496C
inline constexpr uint32_t k_CreatureColour = 0x65A0A0D0u; ///< the creature, `mov edx, 0x65A0A0D0` 0x5E4ACF
inline constexpr uint32_t k_CreatureSpecular = 0x30u;     ///< `push 0x30` 0x5E4ACD
inline constexpr uint32_t k_SwimmerColour = 0xFF303070u;  ///< the swimming SuperVillagers, `mov edx` 0x5E4C69
inline constexpr uint32_t k_SwimmerSpecular = 0u;         ///< `push esi` (esi = 0) 0x5E4C68
// the boat's 0xFF303070 (`mov [eax+0x4C]` 0x5E016C) stays ecs::petit_navire::k_ReflectionColour, the shark's in
// components::CutByPlane::belowColour

/// GLandscape::Draw's creature test before its DrawUnderWater (0x5E4A88..0x5E4AC9): its land block visible
/// (+0x920 & 1, 0x5E4A88), the block's +0x9BC <= [0xC37200] = 100000.0f (fcomp, `test ah, 0x41 / je` 0x5E4A97..0x5E4AA2),
/// its y (+0x3C) < [0x8AB35C] = 6.0f (0x5E4AAB) and its +0xA0 < [0x8AB244] = 0.2f (0x5E4ABE) (inferido: what +0x9BC
/// and +0xA0 mean; the block's +0x9BC is the camera distance of LH3DIsland::PreDraw)
inline constexpr float k_CreatureMaxBlockDistance = 100000.0f;
inline constexpr float k_CreatureMaxY = 6.0f;
inline constexpr float k_CreatureMaxA0 = 0.2f;

} // namespace openblack::graphics::sea_pass
