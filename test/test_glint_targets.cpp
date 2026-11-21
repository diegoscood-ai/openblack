/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The points glints sit on: the pure walk over a model's parts and the per-axis sum (Particles/GlintMaths), and the
// targets the game gives them (ECS/Systems/Implementations/ParticleWorldGlints.cpp), on fake model parts and a
// registry of its own.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <utility>
#include <vector>

#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/GlintTargets.h"
#include "Locator.h"
#include "Particles/GlintMaths.h"
#include "Worship/SpellSeedGraphic.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::particles;
using openblack::ecs::systems::GlintTargets;

namespace
{
using Range = std::pair<uint32_t, uint32_t>;

/// A model of two parts: the first with two primitives (2 and 3 vertices), the second with one primitive of 3 vertices
/// that starts one vertex into its positions, so that a walk over the positions alone would pick the wrong vertex
struct FakeModel
{
	std::vector<glm::vec3> firstPositions {
	    {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f},
	};
	std::vector<Range> firstRanges {{0, 2}, {2, 3}};
	std::vector<glm::vec3> secondPositions {
	    {-1.0f, -1.0f, -1.0f},
	    {0.0f, 6.0f, 0.0f},
	    {0.0f, 7.0f, 0.0f},
	    {0.0f, 8.0f, 0.0f},
	};
	std::vector<Range> secondRanges {{1, 3}};

	[[nodiscard]] std::vector<maths::GlintModelPart> Parts() const
	{
		return {
		    {.positions = firstPositions, .ranges = firstRanges},
		    {.positions = secondPositions, .ranges = secondRanges},
		};
	}
};

constexpr entt::id_type k_ModelId = 0x1234u;

/// The glints' targets on a registry of their own, the fake model given for one mesh id
class GlintTargetsTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	entt::entity Placed(const glm::vec3& position, const glm::mat3& rotation, float scale)
	{
		const auto entity = Reg().Create();
		Reg().Assign<ecs::components::Transform>(entity, position, rotation, glm::vec3(scale));
		return entity;
	}

	static void GiveModel(entt::entity entity, entt::id_type id = k_ModelId)
	{
		Reg().Assign<ecs::components::Mesh>(entity, id, static_cast<int8_t>(0), static_cast<int8_t>(0));
	}

	FakeModel model;
	GlintTargets targets {
	    [this](entt::id_type id) { return id == k_ModelId ? model.Parts() : std::vector<maths::GlintModelPart> {}; }};

private:
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST(GlintMaths, TheCountIsEveryVertexOfEveryPrimitive)
{
	const FakeModel model;
	const auto parts = model.Parts();
	EXPECT_EQ(maths::GlintPointCount(parts), 8u);
	EXPECT_EQ(maths::GlintPointCount({}), 0u);
}

TEST(GlintMaths, TheIndexWalksThePartsAndPrimitivesInTheFilesOrder)
{
	const FakeModel model;
	const auto parts = model.Parts();
	const std::vector<glm::vec3> expected {
	    {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f},
	    {5.0f, 0.0f, 0.0f}, {0.0f, 6.0f, 0.0f}, {0.0f, 7.0f, 0.0f}, {0.0f, 8.0f, 0.0f},
	};
	for (uint32_t i = 0; i < expected.size(); ++i)
	{
		const auto point = maths::GlintLocalPoint(parts, i);
		ASSERT_TRUE(point.has_value()) << i;
		EXPECT_EQ(*point, expected[i]) << i;
	}
	// None past the last
	EXPECT_FALSE(maths::GlintLocalPoint(parts, 8).has_value());
	EXPECT_FALSE(maths::GlintLocalPoint(parts, 1000).has_value());
}

TEST(GlintMaths, EachAxisSumsZThenYThenXThenTheTranslation)
{
	// With the x row's 1e8, the y row's -1e8 and the z row's 1, z + y rounds to -1e8 in a float and the sum is 0;
	// summed x, y, z (as glm's product does) it would be 1
	glm::mat4 model(0.0f);
	model[0] = glm::vec4(1e8f, 1e8f, 1e8f, 0.0f);
	model[1] = glm::vec4(-1e8f, -1e8f, -1e8f, 0.0f);
	model[2] = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
	model[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const auto point = maths::GlintThroughModel(model, {1.0f, 1.0f, 1.0f});
	EXPECT_EQ(point, glm::vec3(0.0f));
	// The translation is added last
	model[3] = glm::vec4(10.0f, 20.0f, 30.0f, 1.0f);
	EXPECT_EQ(maths::GlintThroughModel(model, {1.0f, 1.0f, 1.0f}), glm::vec3(10.0f, 20.0f, 30.0f));
	// A plain case: rows (2, 0, 0), (0, 3, 0), (0, 0, 4) at (1, 1, 1)
	const glm::mat4 scaled {
	    glm::vec4(2.0f, 0.0f, 0.0f, 0.0f),
	    glm::vec4(0.0f, 3.0f, 0.0f, 0.0f),
	    glm::vec4(0.0f, 0.0f, 4.0f, 0.0f),
	    glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
	};
	EXPECT_EQ(maths::GlintThroughModel(scaled, {1.0f, 2.0f, 3.0f}), glm::vec3(3.0f, 7.0f, 13.0f));
}

TEST(SeedGraphicGlints, TheCreationMatrixHasTheScaleOnItsDiagonalAndNoAngle)
{
	const glm::vec3 point(100.0f, 12.5f, -40.0f);
	const auto model = worship::seed_graphic::CreationGlintModel(point, 2.0f);
	EXPECT_EQ(model[0], glm::vec4(2.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[1], glm::vec4(0.0f, 2.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[2], glm::vec4(0.0f, 0.0f, 2.0f, 0.0f));
	EXPECT_EQ(model[3], glm::vec4(point, 1.0f));
	// A scale of 1: the identity at the point
	const auto unit = worship::seed_graphic::CreationGlintModel(point, 1.0f);
	EXPECT_EQ(unit, affine::PlacementMatrix(point, 0.0f, 1.0f));
	EXPECT_EQ(unit[0], glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(unit[3], glm::vec4(point, 1.0f));
}

TEST(SeedGraphicGlints, TheDrawnMatrixIsTheMeshAsItWasDrawnTurnedAndAtTheInfosScale)
{
	const glm::vec3 point(100.0f, 12.5f, -40.0f);
	// CREATURE_SPELL_FREEZE: the seed info's scale 0.8 times the graphic's 2, turned by its spin
	const float size = 0.8f * 2.0f;
	const auto drawn = worship::seed_graphic::DrawnGlintModel(point, 1.25f, size);
	EXPECT_EQ(drawn, affine::PlacementMatrix(point, 1.25f, size));
	EXPECT_NE(drawn, worship::seed_graphic::CreationGlintModel(point, 2.0f));
	EXPECT_EQ(drawn[3], glm::vec4(point, 1.0f));
	// Not turned yet: the size on the diagonal, which the creation matrix does not have
	const auto still = worship::seed_graphic::DrawnGlintModel(point, 0.0f, size);
	EXPECT_EQ(still[0], glm::vec4(size, 0.0f, 0.0f, 0.0f));
	EXPECT_EQ(still[1], glm::vec4(0.0f, size, 0.0f, 0.0f));
	EXPECT_EQ(still[2], glm::vec4(0.0f, 0.0f, size, 0.0f));
}

TEST_F(GlintTargetsTest, AHighlightsPointsAreItsMeshAsItIsDrawn)
{
	const auto highlight = Placed({50.0f, 3.0f, 70.0f}, affine::AngleY(0.5f), 1.5f);
	Reg().Assign<ecs::components::ScriptHighlight>(highlight).scale = 0.7f;
	GiveModel(highlight);
	EXPECT_EQ(targets.TargetPointCount(highlight), 8u);
	const auto drawn = ecs::DrawnModel(Reg(), highlight, false);
	const auto source = GlintTargets::SourceOf(Reg(), highlight);
	ASSERT_TRUE(source.has_value());
	EXPECT_EQ(source->mesh, k_ModelId);
	EXPECT_EQ(source->model, drawn);
	for (uint32_t i = 0; i < 8; ++i)
	{
		const auto point = targets.TargetPoint(highlight, i);
		ASSERT_TRUE(point.has_value()) << i;
		EXPECT_EQ(*point, maths::GlintThroughModel(drawn, *maths::GlintLocalPoint(model.Parts(), i))) << i;
	}
	EXPECT_FALSE(targets.TargetPoint(highlight, 8).has_value());
	// Its own scale, not the drawn one
	EXPECT_FLOAT_EQ(targets.TargetScale(highlight), 0.7f);
	EXPECT_EQ(targets.ObjectPosition(highlight), std::optional(glm::vec3(50.0f, 3.0f, 70.0f)));
}

TEST_F(GlintTargetsTest, AHighlightWithoutAMeshHasNoPoints)
{
	const auto highlight = Placed({50.0f, 3.0f, 70.0f}, glm::mat3(1.0f), 1.0f);
	Reg().Assign<ecs::components::ScriptHighlight>(highlight);
	EXPECT_EQ(targets.TargetPointCount(highlight), 0u);
	EXPECT_FALSE(targets.TargetPoint(highlight, 0).has_value());
	// It is still there
	EXPECT_TRUE(targets.ObjectPosition(highlight).has_value());
	// A mesh that is not loaded has none either
	GiveModel(highlight, k_ModelId + 1);
	EXPECT_EQ(targets.TargetPointCount(highlight), 0u);
}

TEST_F(GlintTargetsTest, ASeedGraphicsPointsGoThroughItsGlintMatrixNotItsDrawnOne)
{
	const glm::vec3 meshPosition(10.0f, 20.0f, 30.0f);
	// Drawn turned, at the seed info's scale x its own
	const auto graphic = Placed(meshPosition, affine::AngleY(1.0f), 0.8f * 2.0f);
	auto& seed = Reg().Assign<ecs::components::SpellSeedGraphic>(graphic);
	seed.scale = 2.0f;
	seed.meshPosition = meshPosition;
	seed.glintModel = worship::seed_graphic::CreationGlintModel(meshPosition, 2.0f);
	GiveModel(graphic);
	EXPECT_EQ(targets.TargetPointCount(graphic), 8u);
	const auto source = GlintTargets::SourceOf(Reg(), graphic);
	ASSERT_TRUE(source.has_value());
	EXPECT_EQ(source->model, seed.glintModel);
	EXPECT_NE(source->model, ecs::DrawnModel(Reg(), graphic, false));
	const auto point = targets.TargetPoint(graphic, 5);
	ASSERT_TRUE(point.has_value());
	// (0, 6, 0) x 2 at the mesh position
	EXPECT_EQ(*point, glm::vec3(10.0f, 32.0f, 30.0f));
	EXPECT_FLOAT_EQ(targets.TargetScale(graphic), 2.0f);
}

TEST_F(GlintTargetsTest, AnyOtherObjectHasNoPointsAndAScaleOfOne)
{
	const auto other = Placed({1.0f, 2.0f, 3.0f}, glm::mat3(1.0f), 4.0f);
	GiveModel(other);
	EXPECT_FALSE(GlintTargets::SourceOf(Reg(), other).has_value());
	EXPECT_EQ(targets.TargetPointCount(other), 0u);
	EXPECT_FALSE(targets.TargetPoint(other, 0).has_value());
	EXPECT_FLOAT_EQ(targets.TargetScale(other), 1.0f);
	EXPECT_EQ(targets.ObjectPosition(other), std::optional(glm::vec3(1.0f, 2.0f, 3.0f)));
	EXPECT_EQ(targets.TargetPointCount(entt::null), 0u);
	EXPECT_FLOAT_EQ(targets.TargetScale(entt::null), 1.0f);
}

TEST_F(GlintTargetsTest, AnObjectThatIsGoneOrBeingDeletedHasNoPosition)
{
	const auto marked = Placed({1.0f, 2.0f, 3.0f}, glm::mat3(1.0f), 1.0f);
	Reg().Assign<ecs::components::Unavailable>(marked);
	EXPECT_FALSE(targets.ObjectPosition(marked).has_value());
	const auto gone = Placed({1.0f, 2.0f, 3.0f}, glm::mat3(1.0f), 1.0f);
	Reg().Destroy(gone);
	EXPECT_FALSE(targets.ObjectPosition(gone).has_value());
	EXPECT_FALSE(targets.ObjectPosition(entt::null).has_value());
}
