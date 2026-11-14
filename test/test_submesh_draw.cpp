/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the scene draws of a model and how it lights it: a part of the first level of detail of the drawn status is
// drawn, a collision-only part never, a window part whatever its level of detail bits; the hand is lit half again as
// bright as anything else. The game's pointer is drawn after the scene, the films' overlays and the menus.
// docs/wiki/project/progress/rendering/level_of_detail.md, models_and_lighting.md and render_pipeline.md

#include <cstdint>

#include <entt/core/hashed_string.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/matrix.hpp>
#include <gtest/gtest.h>

#include "3D/AllMeshes.h"
#include "Graphics/SubMeshDraw.h"
#include "Visual/RenderModes.h"
#include "Visual/RenderPass.h"

using namespace openblack;
using graphics::submesh_draw::CreatureBodyPosedLight;
using graphics::submesh_draw::Drawn;
using graphics::submesh_draw::LightBoost;
using graphics::submesh_draw::PosedLight;

// A part is drawn by its level of detail mask's first bit
TEST(SubMeshDraw, APartIsDrawnByTheFirstBitOfItsLevelOfDetailMask)
{
	EXPECT_TRUE(Drawn(false, true, 0b0001u, false));
	EXPECT_TRUE(Drawn(false, true, 0b0111u, false));
	EXPECT_FALSE(Drawn(false, true, 0b0110u, false));
	EXPECT_FALSE(Drawn(false, true, 0u, false));
	// a window part has no level of detail bits, and is drawn without them
	EXPECT_TRUE(Drawn(false, true, 0u, true));
}

// A collision-only part is never drawn, nor a part of a status the draw does not take
TEST(SubMeshDraw, ACollisionOnlyPartIsNeverDrawn)
{
	EXPECT_FALSE(Drawn(true, true, 0b0001u, false));
	EXPECT_FALSE(Drawn(true, true, 0u, true));
	EXPECT_FALSE(Drawn(false, false, 0b0001u, false));
	EXPECT_FALSE(Drawn(false, false, 0u, true));
}

// The hand is lit half again as bright as the land's light; anything else as it is
TEST(SubMeshDraw, TheHandIsLitHalfAgainAsBright)
{
	constexpr auto k_Hand = entt::hashed_string("hand").value();
	EXPECT_EQ(LightBoost(k_Hand, k_Hand), 1.5f);
	EXPECT_EQ(LightBoost(entt::hashed_string("rock").value(), k_Hand), 1.0f);
}

// The hand's body is lit by the posed models' own rule, as the original poses the hand; every other model here by the
// other models' rule (docs/wiki/rendering/model-lighting.md)
TEST(SubMeshDraw, TheHandTakesThePosedModelsLight)
{
	constexpr auto k_Hand = entt::hashed_string("hand").value();
	EXPECT_TRUE(PosedLight(k_Hand, k_Hand));
	EXPECT_FALSE(PosedLight(entt::hashed_string("rock").value(), k_Hand));
}

// A creature's body is posed as the original's animated models are, so it takes their light with the switch on, but
// not in its reflection, which the original draws unlit
TEST(SubMeshDraw, ACreaturesBodyTakesThePosedModelsLightButNotInItsReflection)
{
	EXPECT_TRUE(CreatureBodyPosedLight(false, true));
	EXPECT_FALSE(CreatureBodyPosedLight(true, true));
	EXPECT_FALSE(CreatureBodyPosedLight(false, false));
	EXPECT_FALSE(CreatureBodyPosedLight(true, false));
}

// The mirrored hand keeps its own faces towards the camera: its triangles are drawn turned round, as the original turns
// them as it mirrors the hand, so the hand takes the other side's culling; the unmirrored hand and every other model
// keep theirs, and a two-sided part stays two-sided
TEST(SubMeshDraw, TheMirroredHandIsCulledAsItsTurnedTrianglesAre)
{
	using graphics::render_modes::Cull;
	using graphics::render_modes::TurnedRound;
	using graphics::submesh_draw::TrianglesTurnedRound;
	constexpr auto k_Hand = entt::hashed_string("hand").value();
	constexpr auto k_Rock = entt::hashed_string("rock").value();
	// the hand's matrix: turned, placed and scaled, its x negated for the right hand
	const auto turn = glm::rotate(0.7f, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(-0.3f, glm::vec3(1.0f, 0.0f, 0.0f));
	const auto right =
	    glm::translate(glm::vec3(1200.5f, 35.25f, -870.75f)) * turn * glm::scale(glm::vec3(-0.0058f, 0.0058f, 0.0058f));
	const auto left = glm::translate(glm::vec3(1200.5f, 35.25f, -870.75f)) * turn * glm::scale(glm::vec3(0.0058f));
	const float rightDeterminant = glm::determinant(glm::mat3(right));
	const float leftDeterminant = glm::determinant(glm::mat3(left));
	EXPECT_TRUE(TrianglesTurnedRound(k_Hand, k_Hand, rightDeterminant));
	EXPECT_FALSE(TrianglesTurnedRound(k_Hand, k_Hand, leftDeterminant));
	EXPECT_FALSE(TrianglesTurnedRound(k_Rock, k_Hand, rightDeterminant));
	EXPECT_EQ(TurnedRound(Cull::Ccw, true), Cull::Cw);
	EXPECT_EQ(TurnedRound(Cull::Cw, true), Cull::Ccw);
	EXPECT_EQ(TurnedRound(Cull::None, true), Cull::None);
	EXPECT_EQ(TurnedRound(Cull::Ccw, false), Cull::Ccw);
}

// The game's pointer is drawn over everything the game draws: after the scene, the films' overlays, the menus and the
// debug windows; only openblack's own views come after it (the mesh viewer, the alpha fill, the temple's map target)
TEST(SubMeshDraw, ThePointerIsDrawnOverEverythingTheGameDraws)
{
	using enum graphics::RenderPass;
	for (const auto pass :
	     {Reflection, Main, MainBlended, FinishFrameIcons, FinishFrameQueue, FinishFrame3D, ScreenOverlay, Interface, ImGui})
	{
		EXPECT_LT(static_cast<int>(pass), static_cast<int>(Cursor)) << static_cast<int>(pass);
	}
}

// The light column is the mesh pack's mesh 530
TEST(SubMeshDraw, TheLightColumnIsMesh530)
{
	EXPECT_EQ(static_cast<uint32_t>(MeshId::SpellColumn), 530u);
}
