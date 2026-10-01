/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Chain.h"

#include <cstdlib>

#include <algorithm>
#include <memory>

#include <spdlog/spdlog.h>

#include "PSys/PSysFile.h"
#include "PSys/PSysManager.h"
#include "PSys/PSysRegistry.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
/// ParticleChainCreator::CreateChain (vt 0x28) keeps the joints of one collection; here the collection itself is the
/// chain, so the creator only holds the material and the UV layout
std::unique_ptr<Creator> MakeChainCreator(const Object& object)
{
	auto creator = std::make_unique<ChainCreator>();
	ReadCreatorProperties(object, *creator);
	creator->kind = Creator::Kind::Chain;
	creator->texture = TextureBaseName(object.String("TextureFileName"));
	creator->additive = object.Bool("UseAdditiveAlpha", true);
	creator->writeDepth = object.Bool("MaterialUpdateZBuffer", false);
	creator->doubleSided = object.Bool("MaterialSetDoubleSided", false);
	creator->dynamicLighting = object.Bool("UseDynamicLighting", false);
	creator->frameOfHead = object.Int("FrameOfHead", 0);
	creator->frameOfTail = object.Int("FrameOfTail", 0);
	// the ctor's defaults (0x6AA739..0x6AA747): FrameHeight 64, FrameWidth 32, NumTexturesForWholeChain -1
	creator->numTexturesForWholeChain = object.Int("NumTexturesForWholeChain", -1);
	creator->frameWidth = object.Int("FrameWidth", 32);
	creator->frameHeight = object.Int("FrameHeight", 64);
	return creator;
}
} // namespace

ChainCreator::SegmentUv ChainCreator::SegmentUvOf(int segment, int segments, float scroll) const
{
	// fn_006C8920 (called from fn_0067B3F0 at 0x67BEFD with frame 0): the chain is cut in T repeats (chain +0x30,
	// NumTexturesForWholeChain, -1 -> joints - 1 at 0x6AA8DF); segment s falls in repeat k = ((s + 1) T - 1) / (n - 1)
	// (integer division), which starts at segment k (n - 1) / T and holds (k + 1)(n - 1) / T - that of them
	const int n1 = std::max(1, segments);
	const int repeats = numTexturesForWholeChain == -1 ? n1 : numTexturesForWholeChain;
	if (repeats <= 0)
	{
		return {0.0f, 0.0f, 0.0f, 0.0f}; // (port guard) the original divides by zero here; no spell file does it
	}
	const int k = ((segment + 1) * repeats - 1) / n1;
	const int first = k * n1 / repeats;
	const int count = (k + 1) * n1 / repeats - first;
	const int local = segment - first;
	// the frame: FrameOfHead in the last repeat, FrameOfTail in the first, else the argument (0); + FileOffset
	// (chain +0x34). With a single repeat the head wins (the k == T - 1 test comes first)
	const int frame = (k == repeats - 1 ? frameOfHead : k == 0 ? frameOfTail : 0) + fileOffset;
	// U across: [frame W, frame W + W] / 256; V along: H x local / count / 256 (+ chain +0x3C); 0.00390625 = 1/256
	// (0x938EBC). A repeat holding no segment (T > n - 1) divides by zero as the original does
	const float u0 = static_cast<float>(frame * frameWidth) / 256.0f;
	const float u1 = static_cast<float>(frame * frameWidth + frameWidth) / 256.0f;
	const float v0 = static_cast<float>(frameHeight) * (static_cast<float>(local) / static_cast<float>(count)) / 256.0f;
	const float v1 =
	    static_cast<float>(frameHeight) * (static_cast<float>(local + 1) / static_cast<float>(count)) / 256.0f;
	return {u0, u1, v0 + scroll, v1 + scroll};
}

std::vector<chain_atoms::Ribbon> chain_atoms::Collect()
{
	auto chains = manager::CollectChains();
	std::erase_if(chains, [](const Ribbon& chain) {
		return chain.creator == nullptr || dynamic_cast<const ChainCreator*>(chain.creator) == nullptr;
	});
	// OPENBLACK_PSYS_CHAIN_TRACE=1: how many ribbons the frame has and where the first one runs
	static const bool trace = std::getenv("OPENBLACK_PSYS_CHAIN_TRACE") != nullptr;
	if (trace && !chains.empty())
	{
		const auto& first = chains.front();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"),
		                   "PSys chains: {} ribbons, the first with {} joints of {} from ({:.1f}, {:.1f}, {:.1f}) to "
		                   "({:.1f}, {:.1f}, {:.1f}), scale {:.2f} alpha {:.0f}",
		                   chains.size(), first.joints.size(), first.creator->texture, first.joints.front().position.x,
		                   first.joints.front().position.y, first.joints.front().position.z, first.joints.back().position.x,
		                   first.joints.back().position.y, first.joints.back().position.z, first.joints.front().scale,
		                   first.joints.front().alpha);
	}
	return chains;
}

void openblack::psys::RegisterChainCreator()
{
	RegisterCreator("ParticleChainCreator", MakeChainCreator);
}
