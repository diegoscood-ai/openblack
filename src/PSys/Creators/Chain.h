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

/// ParticleChainCreator (props 0x6B4760; ctor 0x6AA6xx..0x6AA74E)
struct ChainCreator: Creator
{
	int frameOfHead {0};              ///< +0x58 (chain +0x24): the frame of the textures' last stretch
	int frameOfTail {0};              ///< +0x5C (chain +0x28): the frame of the first stretch
	int numTexturesForWholeChain {-1}; ///< +0x68 (chain +0x30), ctor -1 (0x6AA747): one per segment
	int frameWidth {32};              ///< +0x64 (chain +0x20), ctor 0x20 (0x6AA740)
	int frameHeight {64};             ///< +0x60 (chain +0x1C), ctor 0x40 (0x6AA739)
	bool doubleSided {false};         ///< +0x4E MaterialSetDoubleSided
	bool dynamicLighting {false};     ///< +0x51 UseDynamicLighting

	/// fn_006C8920 (frame_anim::ChainSegmentUv): the four UVs of segment `index` of a chain of `segments` (uv0, uv1 at
	/// its first joint, uv2, uv3 at the next), the chain's v-scroll on their v
	[[nodiscard]] std::array<glm::vec2, 4> SegmentUv(int index, int segments, float scroll) const;
};

namespace chain_atoms
{
/// One ribbon to draw this frame: the joints in order (at least two), with the chain's creator
using Ribbon = Effect::DrawChain;
/// Every chain collection of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Ribbon> Collect();
/// fn_0067B3F0 0x67BE88..0x67BED5 every drawn frame: each chain's v-scroll += g_game_time_inc x its rate x 0.001, kept in
/// one frame height (frame_anim::ChainScroll)
void AdvanceScroll(float milliseconds);
} // namespace chain_atoms

} // namespace openblack::psys
