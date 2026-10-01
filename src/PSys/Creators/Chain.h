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

/// ParticleChainCreator (props 0x6B4760)
struct ChainCreator: Creator
{
	int frameOfHead {0};             ///< +0x58, the texture frame the first segment uses
	int frameOfTail {0};             ///< +0x5C, the frame the last one uses
	int numTexturesForWholeChain {1}; ///< +0x68, how many frames the whole chain spans (-1: one per segment)
	int frameWidth {256};            ///< +0x64
	int frameHeight {256};           ///< +0x60
	bool doubleSided {false};        ///< +0x4E MaterialSetDoubleSided
	bool dynamicLighting {false};    ///< +0x51 UseDynamicLighting

	/// fn_006C8920: the U range of segment `index` of a chain of `segments`, in 0..1 of the texture
	[[nodiscard]] glm::vec2 SegmentU(int index, int segments) const;
};

namespace chain_atoms
{
/// One ribbon to draw this frame: the joints in order (at least two), with the chain's creator
using Ribbon = Effect::DrawChain;
/// Every chain collection of the running effects, interpolated since the last turn
[[nodiscard]] std::vector<Ribbon> Collect();
} // namespace chain_atoms

} // namespace openblack::psys
