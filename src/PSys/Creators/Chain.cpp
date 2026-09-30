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
	creator->numTexturesForWholeChain = object.Int("NumTexturesForWholeChain", 1);
	creator->frameWidth = std::max(1, object.Int("FrameWidth", 256));
	creator->frameHeight = std::max(1, object.Int("FrameHeight", 256));
	return creator;
}
} // namespace

glm::vec2 ChainCreator::SegmentU(int index, int segments) const
{
	// fn_006C8920: the texture is repeated NumTexturesForWholeChain times along the whole chain (-1: once per segment);
	// with the default 256 x 256 frame the frame grid is a single cell, so U just runs along the ribbon and wraps
	const int count = std::max(1, segments);
	const float tiles = numTexturesForWholeChain < 0 ? static_cast<float>(count)
	                                                 : static_cast<float>(std::max(1, numTexturesForWholeChain));
	const float head = static_cast<float>(frameOfHead) * static_cast<float>(frameWidth) / 256.0f;
	const float u0 = head + tiles * static_cast<float>(index) / static_cast<float>(count);
	const float u1 = head + tiles * static_cast<float>(index + 1) / static_cast<float>(count);
	return {u0, u1};
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
