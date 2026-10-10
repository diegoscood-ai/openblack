/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/DrawList/FrameInputs.h: one drawn frame's camera as the block cull and the objects' on-screen test read it
// (the matrix from the eye, the focus, the field of view and the aspect, its view direction, half the viewport, the
// screen clamp, the near plane's half width), the land's blocks as the list reads them (the block table's slot, a block
// outside the table, the state a land load leaves, the place on the block grid, the cells) and the detail index of a
// detail level. Synthetic values only.

#include <cmath>
#include <cstdint>

#include <vector>

#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <LNDFile.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "3D/LandBlock.h"
#include "Dynamics/LandBlockBulletMeshInterface.h"
#include "ECS/DrawList/FrameInputs.h"
#include "ECS/DrawList/GameBlocks.h"
#include "ECS/DrawList/LandClip.h"
#include "Graphics/Mesh.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
draw_list::FrameCamera Camera640x480()
{
	return {
	    .eye = {100.0f, 50.0f, 100.0f},
	    .focus = {200.0f, 0.0f, 300.0f},
	    .horizontalFov = 1.22173f,
	    .aspect = 640.0f / 480.0f,
	    .screen = {640, 480},
	    .nearClip = 1.5f,
	    .landReflection = true,
	};
}
} // namespace

TEST(DrawListFrame, TheBlockCullsCameraFromTheDrawnCamera)
{
	const auto camera = Camera640x480();
	const auto drawCamera = draw_list::MakeDrawCamera(camera);
	EXPECT_EQ(drawCamera.eye, camera.eye);
	EXPECT_EQ(drawCamera.focus, camera.focus);
	EXPECT_EQ(drawCamera.nearW, 1.5f);
	EXPECT_EQ(drawCamera.half, glm::vec2(320.0f, 240.0f));
	ASSERT_TRUE(drawCamera.maxScreen.has_value());
	EXPECT_EQ(*drawCamera.maxScreen, glm::vec2(639.0f, 479.0f)); // the width and the height less one
	EXPECT_TRUE(drawCamera.landReflection);
	// the frame's own world-to-clipping matrix, which does not depend on the near clip
	const auto matrices = affine::FrameMatrices(camera.eye, camera.focus, camera.horizontalFov, camera.aspect);
	EXPECT_EQ(drawCamera.worldToClipping.m, matrices.worldToClipping.m);
	// the view direction is the world-to-camera matrix's third column
	const auto& toCamera = matrices.worldToCamera.m;
	EXPECT_EQ(drawCamera.viewDirection, glm::vec3(toCamera[2], toCamera[5], toCamera[8]));
	auto nearer = camera;
	nearer.nearClip = 0.1f;
	EXPECT_EQ(draw_list::MakeDrawCamera(nearer).worldToClipping.m, matrices.worldToClipping.m);
}

TEST(DrawListFrame, TheOnScreenTestsViewSharesTheMatrixAndTheNearClip)
{
	const auto camera = Camera640x480();
	const auto drawCamera = draw_list::MakeDrawCamera(camera);
	const auto view = draw_list::MakeOnScreenView(camera, drawCamera);
	EXPECT_EQ(view.worldToClipping.m, drawCamera.worldToClipping.m);
	EXPECT_EQ(view.eye, camera.eye);
	EXPECT_EQ(view.nearClip, 1.5f);
	EXPECT_EQ(view.screen, glm::ivec2(640, 480));
	EXPECT_EQ(view.nearHalfWidth, draw_list::NearHalfWidth(camera.horizontalFov, 1.5f));
}

TEST(DrawListFrame, TheNearPlanesHalfWidthIsRoundedOnce)
{
	// a 90 degree field of view (the float nearest pi / 2): the tangent of its half is within half a float step of 1
	EXPECT_EQ(draw_list::NearHalfWidth(1.5707964f, 1.0f), 1.0f);
	// the product is kept wide and rounded once
	const float fov = 1.22173f;
	const float nearClip = 0.3f;
	const auto once = static_cast<float>(std::tan(static_cast<double>(fov * 0.5f)) * static_cast<double>(nearClip));
	EXPECT_EQ(draw_list::NearHalfWidth(fov, nearClip), once);
}

TEST(DrawListFrame, TheLandsBlocksInTheLandsOrderWithTheirSlotsAndLoadStates)
{
	std::vector<LandBlock> land(3);
	lnd::LNDBlock record {};
	record.mapX = 160.0f;
	record.mapZ = 320.0f;
	record.blockX = 1;
	record.blockZ = 2;
	record.highestAltitude = 120;
	record.frameVisibility = 7;
	record.clipped = 1;
	record.meshLOD = 2;
	record.meshBlending = 3;
	record.meshLODType = 5;
	record.valueSorting = 42.5f;
	land[0].SetLndBlock(record);
	record.blockX = 31;
	record.blockZ = 31;
	land[1].SetLndBlock(record);
	record.blockX = 32; // outside the 32 x 32 table
	land[2].SetLndBlock(record);

	std::vector<systems::DrawListBlock> blocks {{}};
	draw_list::CollectBlocks(land, blocks);
	ASSERT_EQ(blocks.size(), 3u);
	EXPECT_EQ(blocks[0].mapPos, glm::vec2(160.0f, 320.0f));
	EXPECT_EQ(blocks[0].highestAltitude, 120);
	EXPECT_EQ(blocks[0].slot, (1u << 5u) + 2u);
	EXPECT_EQ(blocks[1].slot, (31u << 5u) + 31u);
	EXPECT_EQ(blocks[2].slot, draw_list::k_BlockSlots);
	// the place on the block grid, outside the table too, and the land's own cell records
	EXPECT_EQ(blocks[0].coords, glm::ivec2(1, 2));
	EXPECT_EQ(blocks[1].coords, glm::ivec2(31, 31));
	EXPECT_EQ(blocks[2].coords, glm::ivec2(32, 31));
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		EXPECT_EQ(blocks[i].cells.size(), draw_list::k_BlockCells);
		EXPECT_EQ(blocks[i].cells.data(), land[i].GetCells());
	}
	// a land load clears the record's visibility, its partly-outside flag and its level of detail, and keeps the distance
	for (const auto& block : blocks)
	{
		EXPECT_EQ(block.initial.visibility, 0u);
		EXPECT_FALSE(block.initial.partlyOutside);
		EXPECT_EQ(block.initial.lod.meshLod, 0u);
		EXPECT_EQ(block.initial.lod.meshBlending, 0u);
		EXPECT_EQ(block.initial.lod.meshLodType, 0u);
		EXPECT_EQ(block.initial.distance, 42.5f);
	}
}

TEST(DrawListFrame, TheDetailIndexIsTheLevelUpToFour)
{
	for (uint8_t level = 0; level <= 4; ++level)
	{
		EXPECT_EQ(draw_list::DetailIndexOf(level), static_cast<int32_t>(level));
	}
	// openblack's 5 and 6 have no counterpart: the original's start-up makes 5 or more 4
	EXPECT_EQ(draw_list::DetailIndexOf(5), 4);
	EXPECT_EQ(draw_list::DetailIndexOf(6), 4);
	EXPECT_EQ(draw_list::DetailIndexOf(255), 4);
}
