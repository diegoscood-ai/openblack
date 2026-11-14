/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/Graphics/SharedPrimitiveState.h: when a primitive may keep the render state of the one drawn before it. The game
// sets each primitive's culling from its own material, so a two-sided primitive never takes a one-sided one's state.

#include <gtest/gtest.h>

#include "Graphics/SharedPrimitiveState.h"

using openblack::graphics::L3DSubMesh;
using openblack::graphics::shared_primitive_state::SharesState;

namespace
{
// A textured, one-sided, opaque primitive, as the walls of a building
L3DSubMesh::Primitive Wall()
{
	return L3DSubMesh::Primitive {
	    .skinID = 0xB,
	    .indicesOffset = 0,
	    .indicesCount = 3,
	    .depthWrite = true,
	    .alphaTest = false,
	    .blend = L3DSubMesh::Primitive::BlendMode::Disabled,
	    .modulateAlpha = false,
	    .thresholdAlpha = false,
	    .alphaCutoutThreshold = 0.0f,
	    .colour = glm::vec4(1.0f),
	    .twoSided = false,
	    .wrap = false,
	    .uvOffset = true,
	    .materialType = 1,
	};
}
} // namespace

TEST(SharedPrimitiveState, TheSameMaterialSharesTheState)
{
	auto next = Wall();
	next.indicesOffset = 3;
	next.indicesCount = 6;
	EXPECT_TRUE(SharesState(Wall(), next));
}

TEST(SharedPrimitiveState, ATwoSidedPrimitiveAfterACulledOneSetsItsOwnState)
{
	// the flat fence of the Norse workshop's yard, drawn right after its walls with the same skin and type
	auto fence = Wall();
	fence.twoSided = true;
	EXPECT_FALSE(SharesState(Wall(), fence));
	// and a one-sided primitive after a two-sided one gets its back faces culled again
	EXPECT_FALSE(SharesState(fence, Wall()));
	EXPECT_TRUE(SharesState(fence, fence));
}

TEST(SharedPrimitiveState, EveryFieldTheStateIsMadeOfMustMatch)
{
	const auto base = Wall();
	auto changed = base;
	changed.materialType = 2;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.blend = L3DSubMesh::Primitive::BlendMode::Standard;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.thresholdAlpha = true;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.depthWrite = false;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.alphaCutoutThreshold = 0.5f;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.wrap = true;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.uvOffset = false;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.partialBuildCap = true;
	EXPECT_FALSE(SharesState(base, changed));
	changed = base;
	changed.partialBuild = true;
	EXPECT_FALSE(SharesState(base, changed));
}

TEST(SharedPrimitiveState, WhatTheStateDoesNotReadIsIgnored)
{
	// the indices are bound per primitive, the colour is only read without a texture and the skin is compared by the
	// caller as the texture it gives
	auto next = Wall();
	next.indicesOffset = 30;
	next.indicesCount = 9;
	next.colour = glm::vec4(0.5f);
	next.skinID = 0xC;
	EXPECT_TRUE(SharesState(Wall(), next));
}
