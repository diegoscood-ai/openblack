/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "PSys/PSys.h"

// Mesh particles: ParticleMeshCreator (CreateParticle 0x6A8B00, Particle3DObj::DrawAt 0x679FD0) and
// ParticleMeshCreatorAnimTextured (0x6A8DA0, Particle3DObjAnimTextured::DrawAt 0x67A530, a UV frame offset) and
// ParticleAnimCreator (fn_006A97F0, Particle3DAnim::DrawAt 0x67A8E0, a skinned mesh playing a .anm). The atoms are drawn
// as mesh instances with the objects (ECS RenderingSystem), the animated ones with their bones. Wiki:
// docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// The frames of a cycle of a ParticleAnimCreator atom: fn_006A97F0 0x6A9854 (+0x114 = 0x3E8) and the / 1000 of
/// GetCycleTimeFromFrame 0x6C85F0
inline constexpr int k_AnimFrames = 1000;

/// ParticleBaseMeshCreator (0x6B37A0) + ParticleMeshCreator (0x6B38B0) / ParticleMeshCreatorAnimTextured (0x6B3970) /
/// ParticleAnimCreator (0x6B3D70)
struct MeshCreator: Creator
{
	entt::id_type meshId {0}; ///< MeshEnum (+0x44, a mesh of the pack) or MeshFileName (+0x34, GJUtils::GetSharedMesh)
	bool faceCamera {false};  ///< +0x4D
	bool faceCameraSprite {false}; ///< +0x4C
	float heightStretch {1.0f};    ///< +0x48
	bool scriptHighlightPulse {false}; ///< +0x4E UseScriptHightlightPulse
	// ParticleMeshCreator (ctor 0x6A8960: +0x57 and +0x58 are 1, the other flags 0)
	bool additive {false};            ///< +0x55 UseAdditiveAlpha (applied with MeshChangeMaterialProps)
	bool writeDepth {false};          ///< +0x56 MaterialUpdateZBuffer
	bool doubleSided {true};          ///< +0x57 MaterialSetDoubleSided
	bool changeMaterialProps {true};  ///< +0x58 MeshChangeMaterialProps
	/// +0x59, MaterialProperties +4 (no property; 1 from the ctors 0x6A897D / 0x6A8BD0, ParticleAnimCreator's +0x94
	/// 0x6A9225): 0 would make every material TexturedAlpha (GJUtils::SetMaterialProperties 0x57E13D)
	bool materialAlpha {true};
	/// UseGlobalAlpha: +0x5A (ctor 0) of ParticleMeshCreator / AnimTextured, +0xA5 (ctor 1, 0x6A93CA) of ParticleAnimCreator.
	/// Only AnimTextured's CreateParticle (0x6A8E04..0x6A8E17) and ParticleAnimCreator's (0x6A983A..0x6A9840) pass it to
	/// the particle's +0x24; ParticleMeshCreator::CreateParticle 0x6A8B00 never does (the Particle3DObj ctor 0x6C7A23
	/// clears the bit), so its atoms never use their object's alpha table (see globalAlpha below)
	bool useGlobalAlpha {false};
	/// GJUtils::SetMaterialProperties fn_0057E1D0 on the mesh, done once at the creator's first particle as fn_006A8A40 /
	/// fn_006A8CC0 / fn_006A95E0 do when they first fetch the mesh (+0x50 / +0x34 still 0)
	mutable bool materialsSet {false};
	bool neverClip {false};           ///< +0x5B
	/// +0x5D CastHumanShadow (DefineProperties 0x6B393E; ParticleMeshCreator only: AnimTextured's +0x5D is its NeverClip,
	/// vt+0x98 0x6A8D7C). CreateParticle 0x6A8B55..0x6A8B7F gives each atom's particle a node of 0x10 bytes (+0x1C;
	/// fn_006CA340: +0 / +4 the links, +8 the holder of fn_008745A0 -> fn_0087FD50, whose si+0xC = 1 at 0x8745C8 and
	/// holder+4 = 0 at 0x8745C1, +0xC 0); every Particle3DObj::DrawAt puts the particle's object (+0x20) in the node's +0xC
	/// and pushes the node on the list [0xD4EDCC] (0x67A45D..0x67A494, on every path of DrawAt); every frame GGame::
	/// Process3dEngine 0x54DEAD -> PSysLightMaps::AddDrawing 0x6CA6E0 -> fn_006CA540 (0xD4EDB0, its +0x1C is that list)
	/// updates each node's shadow with its object (fn_006CA3D0 0x6CA5A9 -> fn_00874850, the generic update of the
	/// physics objects) and fn_006CA660 empties the list (0x6CA69E..0x6CA6CC). The particle's dtor fn_006C7A80 0x6C7AA6
	/// takes the shadow out (fn_006CA370 -> fn_008745E0 -> fn_0087FF10). Here: mesh_atoms::HumanShadows, the list of the
	/// last Collect, which graphics::shadow_list reads. The property is 0 in all 20 mesh creators of the spell files
	/// (tmp_dis\psys\stats.txt)
	bool castHumanShadow {false};
	/// +0x54, the argument of vt+0x78 / vt+0x80 (CreateLH3DObject 0x6A8ACE / 0x6A8D65; fn_008168A0: obj+4 bit 0x40, the
	/// receiver of the projected shadows): 0 from the ctors (0x6A8986, 0x6A8BDE), no property. The atoms never receive
	static constexpr bool k_ReceivesShadow = false;
	bool drawWithLandscapeColour {false}; ///< +0x5E: the particle's +0x24 bit 2 (CreateParticle 0x6A8B82)
	bool drawCutByPlane {false};          ///< +0x5F: the particle's +0x24 bit 4
	// ParticleMeshCreatorAnimTextured
	bool animTextured {false};
	int textureWidth {64};  ///< +0x6C
	int textureHeight {64}; ///< +0x68
	bool slideU {false};    ///< +0x70
	bool slideV {false};    ///< +0x71
	bool randomiseInitFrame {false}; ///< +0x72
	bool randomiseFrameRate {false}; ///< +0x73
	float frameRate {1.0f};    ///< +0x60
	float frameRateMax {10.0f}; ///< +0x78
	int numFrames {1};          ///< +0x64
	bool playAnimation {false}; ///< +0x5E
	float initialOffsetFrac {0.0f}; ///< +0x7C
	float stretchY {1.0f};          ///< +0x80
	// ParticleAnimCreator (DefineProperties 0x6B3D70; ctor defaults 0x6A93A7..0x6A93E5): the skinned mesh of
	// Particle3DAnim (DrawAt 0x67A8E0), an LH3DObject of type 2 (CreateLH3DObject 0x6A9760) playing a .anm
	bool animated {false};
	entt::id_type animId {0};     ///< +0x40: AnimFileName (std::string at +0x80, its pointer +0x84 read by fn_006A9570 0x6A959D; loaded by fn_00839900; AnimEnum +0x7C is -1)
	float speedUpFactor {1.0f};   ///< +0x44 SpeedUpFactor (ctor 1.0)
	bool animPlay {false};        ///< +0xA1 PlayAnim (ctor 0)
	bool animRandomInitFrame {false}; ///< +0xA2 RandomiseInitFrame (ctor 0)

	/// fn_006A85E0's atom part is Effect::NewAtom's; this is CreateParticle's (0x6A8DA0: frame, frame rate, StretchY;
	/// ParticleAnimCreator's fn_006A97F0: the clip's frame rate, PlayAnim, the random first frame)
	void InitAtom(Effect& effect, Atom& atom) const override;
	/// The frame count of an atom (+0x114): NumFrames, or 1000 for the sliding textures and the animated meshes
	/// (fn_006A97F0 0x6A9854 / 0x6A988C: 0x3E8)
	[[nodiscard]] int FramesPerAtom() const override
	{
		return animated || slideU || slideV ? k_AnimFrames : std::max(1, numFrames);
	}
	/// Particle3DObjAnimTextured::DrawAt 0x67A530: the UV offset (vt 0xE8) of a frame
	[[nodiscard]] glm::vec2 UvOffset(int frame) const;
};

/// fn_006A97F0 0x6A985F..0x6A9882: the atom's frame rate +0x110 = 1000 ([0x8AB228]) / the clip's ms (LH3DAnim +0x20,
/// fild) x SpeedUpFactor x 1000 ([0x8AB228]): one cycle of 1000 frames in the clip's length, faster by the factor
[[nodiscard]] float AnimFrameRate(int32_t clipMs, float speedUpFactor) noexcept;
/// Particle3DAnim::GetCycleTimeFromFrame 0x6C85F0: the clip's ms (LH3DAnim +0x20) x frame / 1000 in integers (imul,
/// then x 0x10624DD3 sar 6 and the sign bit added: the / 1000 rounds towards 0), the time DrawAt gives vt 0x188
[[nodiscard]] int32_t AnimCycleTime(int32_t clipMs, int frame) noexcept;

namespace mesh_atoms
{
/// One mesh atom to draw this frame (the PSR matrix of fn_00679920 x the scale, the Y axis x the stretch)
struct Instance
{
	entt::id_type meshId;
	glm::mat4 model;
	float alpha;       ///< 0..1 (atom alpha x collection alpha)
	glm::vec2 uv;      ///< the AnimTextured offset (0, 0 otherwise)
	bool translucent;  ///< additive or alpha < 1: drawn with the blended objects
	bool additive;     ///< material mode 13 (GJUtils::SetMaterialProperties 0x57E120): SRCALPHA / ONE, no Z write
	std::array<uint8_t, 3> colour; ///< the DrawData colour's r, g, b (SetColour vt 0x2C, or x the land light)
	bool landscapeColour; ///< DrawWithLandscapeColor: the colour x the land light (fn_0080BEC0), else the colour alone
	/// (milagros2 rayo3, asked by session shaders) the DrawData specular +0xC (the atom's +0x90, D3DCOLOR) that
	/// Particle3DObj::DrawAt gives the object with the colour: fn_0080BEC0(colour, specular) 0x67A012..0x67A01C (added to
	/// the land's specular by fn_0080BF10) or SetColour vt 0x2C (obj +0x50, 0x67A023..0x67A02F, fn_007F9770)
	uint32_t specular {0};
	/// The particle's +0x24 bit 0: LH3DObject::SetGlobalAlpha (vt 0x48 fn_007F9D60, flags +4 bit 0x80; 0x67A216..0x67A227 /
	/// 0x67A9D6), whose draw then takes the mode table 0xC387C8 (fn_0080DB30 0x80DEED..0x80DF09) and so blends the
	/// opaque modes 0, 2, 4, 9, 17 with the colour's alpha. Without it the materials' own modes (table 0xC38728): the
	/// alpha only shows in the modes that blend
	bool globalAlpha {true};
	/// A ParticleAnimCreator atom's bones (graphics::ComputePose at the time of its frame, what the type 2 object's draw
	/// fn_008175B0 gets from LH3DAnim::GetPose 0x8177B8..0x8177CE); empty for the still meshes
	std::vector<glm::mat4> pose {};
	/// The particle's +0x24 bit 4, DrawCutByPlane: the creator's +0x5F (CreateParticle 0x6A8B94..0x6A8B9A), tested by
	/// fn_00679F20 (`test al, 4` 0x679F29) on both paths, which then draws through vt+0x11C (0x679F4A, cut by the
	/// default plane) instead of vt+0x104 (0x679F52)
	bool cutByPlane {false};
	/// Its effect's draw path: Sorted, its own Z object at the model's translation (fn_00679F60, opaque too); Queued /
	/// Immediate, drawn at its place in its effect's items (manager::OrderedEffect, matched by `atom`)
	DrawPath path {DrawPath::Sorted};
	uint32_t effect {0};      ///< the effect's id (manager::Drawable::effect)
	const Atom* atom {nullptr}; ///< the atom (Effect::DrawAtom::atom), the key into its effect's items
};
/// Every mesh atom of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Instance> Collect();
/// Whether any effect has a mesh atom (without interpolating them or working out their poses)
[[nodiscard]] bool Any();
/// One node of the list [0xD4EDCC] (CastHumanShadow, MeshCreator::castHumanShadow): an atom drawn this frame whose
/// particle's object casts a shadow list shadow, updated as the physics objects' (fn_00874850 with holder+4 = 0: the
/// light straight above, si+0xC = 1: not over the objects)
struct HumanShadow
{
	const Atom* atom;     ///< the key of its ShadowInfo (the particle, CreateParticle 0x6A8B7F .. dtor 0x6C7AA6)
	entt::id_type meshId; ///< the particle's object's mesh (radius mesh+0x30)
	glm::mat4 model;      ///< obj+0x14..0x43 as DrawAt leaves it (Instance::model): the vertices and obj+0x38..0x40
	float scale;          ///< obj+0x44 = the drawn PSR's +0x30 (0x67A000..0x67A009 / 0x67A445..0x67A44E)
};
/// The atoms of the last Collect with CastHumanShadow, in Collect's order (fn_006CA540 walks [0xD4EDCC] from its head,
/// the last pushed first: (aproximado) the order only changes which shadow is updated first)
[[nodiscard]] const std::vector<HumanShadow>& HumanShadows();
} // namespace mesh_atoms

} // namespace openblack::psys
