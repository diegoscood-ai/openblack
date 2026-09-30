/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/core/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

#include "PSys/PSys.h"

// Mesh particles: ParticleMeshCreator (CreateParticle 0x6A8B00, Particle3DObj::DrawAt 0x679FD0) and
// ParticleMeshCreatorAnimTextured (0x6A8DA0, Particle3DObjAnimTextured::DrawAt 0x67A530, a UV frame offset). The atoms
// are drawn as mesh instances with the objects (ECS RenderingSystem). Wiki: docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// ParticleBaseMeshCreator (0x6B37A0) + ParticleMeshCreator (0x6B38B0) / ParticleMeshCreatorAnimTextured (0x6B3970)
struct MeshCreator: Creator
{
	entt::id_type meshId {0}; ///< MeshEnum (+0x44, a mesh of the pack) or MeshFileName (+0x34, GJUtils::GetSharedMesh)
	bool faceCamera {false};  ///< +0x4D
	bool faceCameraSprite {false}; ///< +0x4C
	float heightStretch {1.0f};    ///< +0x48
	bool additive {false};         ///< +0x55 (with MeshChangeMaterialProps +0x58)
	bool changeMaterialProps {false};
	bool neverClip {false}; ///< +0x5B
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

	/// fn_006A85E0's atom part is Effect::NewAtom's; this is CreateParticle's (0x6A8DA0: frame, frame rate, StretchY)
	void InitAtom(Effect& effect, Atom& atom) const override;
	/// The frame count of an atom (+0x114): NumFrames, or 1000 for the sliding textures
	[[nodiscard]] int FramesPerAtom() const { return slideU || slideV ? 1000 : std::max(1, numFrames); }
	/// Particle3DObjAnimTextured::DrawAt 0x67A530: the UV offset (vt 0xE8) of a frame
	[[nodiscard]] glm::vec2 UvOffset(int frame) const;
};

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
};
/// Every mesh atom of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Instance> Collect();
} // namespace mesh_atoms

} // namespace openblack::psys
