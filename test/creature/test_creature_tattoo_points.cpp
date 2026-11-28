/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The triangles the places for a species' tattoos are on, found once from its meshes as the species loads, on
// synthetic primitives rather than the game's meshes

#include <array>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRig.h"

using namespace openblack;
using namespace openblack::creature;
using creature_tattoo_editor::TexturedPrimitive;
using creature_tattoo_editor::TexturedVertex;

namespace
{
using Mesh = CreatureRig::Mesh;

constexpr auto MeshIndex(Mesh mesh)
{
	return static_cast<size_t>(mesh);
}

/// The base mesh: one primitive on skin 1 whose one triangle covers the top left half of the skin, its vertices moved by
/// bones 7, 8 and 9
struct BaseMesh
{
	std::array<TexturedVertex, 3> vertices {TexturedVertex {.uv = {0.0f, 0.0f}, .bone = 7},
	                                        TexturedVertex {.uv = {1.0f, 0.0f}, .bone = 8},
	                                        TexturedVertex {.uv = {0.0f, 1.0f}, .bone = 9}};
	std::vector<std::array<uint16_t, 3>> triangles {{0, 1, 2}};
	std::array<TexturedPrimitive, 1> primitives {TexturedPrimitive {.skin = 1, .vertices = vertices, .triangles = triangles}};
};

const std::vector<glm::vec3> k_BasePositions {{1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 0.0f}};
const std::vector<glm::vec3> k_EvilPositions {{1.0f, 5.0f, 0.0f}, {2.0f, 5.0f, 0.0f}, {3.0f, 5.0f, 0.0f}};

creature_tattoo::Site At(uint8_t u, uint8_t v)
{
	return {.enabled = true, .u = u, .v = v, .skin = 1, .size = 0.25f, .mirror = false, .rotation = 0};
}
} // namespace

TEST(CreatureTattooPoints, EachUsedPlaceKeepsItsTriangleInEveryMesh)
{
	const BaseMesh base;
	creature_tattoo::Sites sites {};
	sites.at(0) = At(64, 32);
	// Unused, though a triangle holds it
	sites.at(1) = At(64, 32);
	sites.at(1).enabled = false;
	// Outside the triangle
	sites.at(2) = At(200, 200);
	std::array<PrimitivePositions, CreatureRig::k_MeshCount> positions {};
	positions.at(MeshIndex(Mesh::Base)) = {k_BasePositions};
	positions.at(MeshIndex(Mesh::Evil)) = {k_EvilPositions};

	const auto points = FindTattooPoints(sites, base.primitives, positions);
	ASSERT_TRUE(points.at(0).has_value());
	const auto& point = *points.at(0);
	EXPECT_EQ(point.bones, (std::array<uint32_t, 3> {7, 8, 9}));
	EXPECT_FLOAT_EQ(point.u, 0.25f);
	EXPECT_FLOAT_EQ(point.v, 0.125f);
	EXPECT_EQ(point.vertices.at(MeshIndex(Mesh::Evil)).at(1), k_EvilPositions.at(1));
	// The meshes the species lacks take the base's vertices
	for (const auto mesh : {Mesh::Base, Mesh::Good, Mesh::Thin, Mesh::Fat, Mesh::Weak, Mesh::Strong})
	{
		for (size_t i = 0; i < 3; ++i)
		{
			EXPECT_EQ(point.vertices.at(MeshIndex(mesh)).at(i), k_BasePositions.at(i));
		}
	}
	for (size_t i = 1; i < points.size(); ++i)
	{
		EXPECT_FALSE(points.at(i).has_value()) << "place " << i;
	}
}

TEST(CreatureTattooPoints, AMeshWithoutTheVertexTakesTheBases)
{
	const BaseMesh base;
	creature_tattoo::Sites sites {};
	sites.at(3) = At(10, 10);
	std::array<PrimitivePositions, CreatureRig::k_MeshCount> positions {};
	positions.at(MeshIndex(Mesh::Base)) = {k_BasePositions};
	// The good mesh lists only the first two of the primitive's vertices
	positions.at(MeshIndex(Mesh::Good)) = {{k_EvilPositions.at(0), k_EvilPositions.at(1)}};

	const auto points = FindTattooPoints(sites, base.primitives, positions);
	ASSERT_TRUE(points.at(3).has_value());
	const auto& good = points.at(3)->vertices.at(MeshIndex(Mesh::Good));
	EXPECT_EQ(good.at(0), k_EvilPositions.at(0));
	EXPECT_EQ(good.at(1), k_EvilPositions.at(1));
	EXPECT_EQ(good.at(2), k_BasePositions.at(2));

	// Without the base's own vertex there is no triangle to keep
	positions.at(MeshIndex(Mesh::Base)) = {{k_BasePositions.at(0), k_BasePositions.at(1)}};
	positions.at(MeshIndex(Mesh::Good)).clear();
	EXPECT_FALSE(FindTattooPoints(sites, base.primitives, positions).at(3).has_value());
}

TEST(CreatureTattooPoints, APlaceIsPosedAsTheBodyIsDrawn)
{
	std::array<std::optional<CreatureRig::TattooPoint>, creature_tattoo::k_SlotCount> points {};
	CreatureRig::TattooPoint point {.vertices = {}, .bones = {0, 0, 1}, .u = 0.25f, .v = 0.5f};
	for (auto& vertices : point.vertices)
	{
		vertices = {glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)};
	}
	points.at(2) = point;
	// The third vertex's bone lifts it one more
	const std::vector bones {glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 10.0f}),
	                         glm::translate(glm::mat4(1.0f), {0.0f, 1.0f, 10.0f})};
	const auto placement = glm::translate(glm::mat4(1.0f), {5.0f, 0.0f, 0.0f});

	const auto posed = PosedTattooPoints(points, {}, bones, placement);
	ASSERT_EQ(posed.size(), 1u);
	EXPECT_EQ(posed[0].site, 2);
	// The triangle is (5, 0, 10), (6, 0, 10), (5, 2, 10): a quarter of the way to the second and half to the third
	EXPECT_EQ(posed[0].position, glm::vec3(5.25f, 1.0f, 10.0f));
	// The third's way crossed with the second's
	EXPECT_EQ(posed[0].inwardNormal, glm::vec3(0.0f, 0.0f, -2.0f));
}
