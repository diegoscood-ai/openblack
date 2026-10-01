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

#include "3D/FrameAnim.h"
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
	creator->fileOffset = object.Int("FileOffset", 0); // +0x54 (chain +0x34)
	creator->frameOfHead = object.Int("FrameOfHead", 0);
	creator->frameOfTail = object.Int("FrameOfTail", 0);
	// ctor 0x6AA739..0x6AA747: FrameHeight 0x40, FrameWidth 0x20, NumTexturesForWholeChain -1 (DefineProperties ranges
	// [1, 256] and [-1, 32])
	creator->numTexturesForWholeChain = object.Int("NumTexturesForWholeChain", -1);
	creator->frameWidth = std::max(1, object.Int("FrameWidth", 32));
	creator->frameHeight = std::max(1, object.Int("FrameHeight", 64));
	return creator;
}
} // namespace

std::array<glm::vec2, 4> ChainCreator::SegmentUv(int index, int segments, float scroll) const
{
	// CreateChain 0x6AA880: chain +0x30 = NumTexturesForWholeChain, or joints - 1 when -1 (0x6AA8DC..0x6AA8EB).
	// (aproximado) the joints drawn now, where the original counts the ones the chain was made with
	graphics::frame_anim::ChainSheet sheet;
	sheet.frameWidth = frameWidth;
	sheet.frameHeight = frameHeight;
	sheet.frameOfHead = frameOfHead;
	sheet.frameOfTail = frameOfTail;
	sheet.fileOffset = fileOffset;
	// only -1 is replaced; 0 (which DefineProperties allows) is left to ChainSegmentUv's openblack guard, where the
	// original would divide by zero (idiv 0x6C893E)
	sheet.textures = numTexturesForWholeChain == -1 ? segments : numTexturesForWholeChain;
	return graphics::frame_anim::ChainSegmentUv(index, segments, sheet, scroll);
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

void chain_atoms::AdvanceScroll(float milliseconds)
{
	for (const auto& chain : manager::CollectChains())
	{
		const auto* creator = dynamic_cast<const ChainCreator*>(chain.creator);
		// [0xC029B8] (1): the scroll and the offset are on
		if (creator != nullptr && chain.collection != nullptr)
		{
			(void)graphics::frame_anim::ChainScroll(chain.collection->chainScroll, milliseconds,
			                                        chain.collection->chainScrollRate, creator->frameHeight);
		}
	}
}

void openblack::psys::RegisterChainCreator()
{
	RegisterCreator("ParticleChainCreator", MakeChainCreator);
}
