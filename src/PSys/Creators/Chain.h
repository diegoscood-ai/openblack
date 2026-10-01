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

#include <vector>

#include <glm/vec2.hpp>

#include "PSys/PSys.h"

// Chain particles: ParticleChainCreator (0x6AA900, ChainJoint::DrawAt 0x679E80, ribbon fn_0067B3F0, UV fn_006C8920).
// Every atom of the collection is one joint of a chain; the collection is drawn as a single camera-facing ribbon
// through its joints, textured with S_Lightning.raw / S_Beam.raw. Used by the lightning bolt and the storm lightning
// (Rules/Lightning.cpp), the gesture trail and the creature beam. Report: tmp_dis\psys\part_render.md §10; wiki
// docs/bw1-notes/magic.md.

namespace openblack::psys
{

/// ParticleChainCreator (props 0x6B4760; ctor 0x6AA6E0..0x6AA751, CreateChain 0x6AA880 copies the layout to the Chain)
struct ChainCreator: Creator
{
	int frameOfHead {0};              ///< +0x58 -> chain +0x24, the frame of the last repeat (fn_006C8920)
	int frameOfTail {0};              ///< +0x5C -> chain +0x28, the frame of the first repeat
	int numTexturesForWholeChain {-1}; ///< +0x68 -> chain +0x30 (ctor 0x6AA747: -1, CreateChain 0x6AA8DF: joints - 1)
	int frameWidth {32};              ///< +0x64 -> chain +0x20 (ctor 0x6AA740), texels across the ribbon
	int frameHeight {64};             ///< +0x60 -> chain +0x1C (ctor 0x6AA739), texels along one repeat
	bool doubleSided {false};         ///< +0x4E MaterialSetDoubleSided
	bool dynamicLighting {false};     ///< +0x51 UseDynamicLighting

	/// fn_006C8920's four texture coordinates of one segment, in 0..1 of the 256 x 256 texture: U runs across the
	/// ribbon (u0 on the joint + side vertices, u1 on the joint - side ones), V along it (v0 at the segment's first
	/// joint, v1 at its second)
	struct SegmentUv
	{
		float u0, u1, v0, v1;
	};
	/// `segment` of a chain of `segments` segments (joints - 1); `scroll` is chain +0x3C, the V offset
	[[nodiscard]] SegmentUv SegmentUvOf(int segment, int segments, float scroll = 0.0f) const;
};

namespace chain_atoms
{
/// One ribbon to draw this frame: the joints in order (at least two), with the chain's creator
using Ribbon = Effect::DrawChain;
/// Every chain collection of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Ribbon> Collect();
} // namespace chain_atoms

} // namespace openblack::psys
