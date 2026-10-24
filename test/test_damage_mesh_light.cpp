/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The per-face light of a damage mesh (src/Physics/DamageMesh.h) against the original: the two sides of one face
// (model_light::TwoSided, emulated below step by step), the table-and-Newton ModelInverseSquareRoot, and the vertices of
// one triangle (DamageMesh::AppendTriangle: front, back 0.45 behind, the walls of the open edges).

#include <cmath>
#include <cstdint>

#include <bit>
#include <optional>
#include <random>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Common/ModelInverseSquareRoot.h"
#include "Graphics/ModelLight.h"
#include "Graphics/WorldTriangles.h"
#include "Physics/DamageMesh.h"

using namespace openblack;
using DamageMesh = openblack::physics::damage::Mesh;

namespace
{
/// The original's two-sided face light step by step: I = the intensity, the ambient and the object's colour;
/// returns {front, back}
model_light::TwoSidedColours EmulateFace(int32_t intensity, int32_t ambient, uint32_t colour)
{
	const int32_t eaxI = intensity;
	const int32_t edxI = -eaxI;
	int32_t edi = 0; // the front factor
	if (eaxI < 0)
	{
		edi = ambient;
	}
	else
	{
		edi = ((0xFF - ambient) * eaxI >> 8) + ambient;
	}
	int32_t back = 0;
	if (edxI < 0)
	{
		back = ambient;
	}
	else
	{
		back = ((0xFF - ambient) * edxI >> 8) + ambient;
	}
	const auto f = static_cast<uint32_t>(edi);
	const auto b = static_cast<uint32_t>(back);
	uint32_t esi = colour;
	uint32_t ecx = esi;
	uint32_t eax = esi & 0xFF00u;
	uint32_t edx = esi;
	esi &= 0xFF000000u;
	const uint32_t alpha = esi;
	esi = eax;
	ecx &= 0xFF0000u;
	esi *= f;
	uint32_t ebx = ecx * f;
	ebx &= 0xFF0000FFu;
	edx &= 0xFFu;
	esi &= 0xFF0000u;
	esi |= ebx;
	ebx = edx * f;
	eax *= b;
	ecx *= b;
	edx *= b;
	eax &= 0xFF0000u;
	ecx &= 0xFF0000FFu;
	eax |= ecx;
	edx &= 0xFF00u;
	eax |= edx;
	ebx &= 0xFF00u;
	esi |= ebx;
	eax = (eax >> 8) | alpha;
	esi = (esi >> 8) | alpha;
	return {esi, eax};
}

uint32_t ArgbOf(uint32_t abgr)
{
	return graphics::world_triangles::ToAbgr(abgr); // the swap is its own inverse
}

DamageMesh::Triangle FlatTriangle()
{
	DamageMesh::Triangle t;
	t.corners[0] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
	t.corners[1] = {{1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}};
	return t;
}
} // namespace

TEST(DamageMeshSize, ShortestEdgeDecides)
{
	// the fragment build classes a triangle by its shortest squared edge: > 32, > 8, > 2, else 0
	DamageMesh::Triangle t;
	t.corners[0] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}};
	t.corners[1] = {{10.0f, 0.0f, 0.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{10.0f, 0.0f, 1.2f}, {1.0f, 1.0f}};
	// edges 100, 1.44, 101.44: a long thin wall triangle is class 0, not 3
	EXPECT_EQ(DamageMesh::SizeClassOf(t), 0);
	// a 6 x 6 right triangle: edges 36, 36, 72
	t.corners[1] = {{6.0f, 0.0f, 0.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{0.0f, 0.0f, 6.0f}, {0.0f, 1.0f}};
	EXPECT_EQ(DamageMesh::SizeClassOf(t), 3);
	// 3 x 3: 9 -> 2; 2 x 2: 4 -> 1; exactly 2 (sqrt 2 sides) is not above 2 -> 0
	t.corners[1] = {{3.0f, 0.0f, 0.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{0.0f, 0.0f, 3.0f}, {0.0f, 1.0f}};
	EXPECT_EQ(DamageMesh::SizeClassOf(t), 2);
	t.corners[1] = {{2.0f, 0.0f, 0.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{0.0f, 0.0f, 2.0f}, {0.0f, 1.0f}};
	EXPECT_EQ(DamageMesh::SizeClassOf(t), 1);
	t.corners[1] = {{1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}};
	t.corners[2] = {{2.0f, 0.0f, 0.0f}, {0.0f, 1.0f}};
	EXPECT_EQ(DamageMesh::SizeClassOf(t), 0);
}

TEST(DamageMeshLight, TwoSidedMatchesTheOriginal)
{
	std::mt19937 rng(7);
	std::uniform_int_distribution<uint32_t> colours;
	std::uniform_real_distribution<float> dots(-1.0f, 1.0f);
	for (int i = 0; i < 200000; ++i)
	{
		const uint32_t colour = colours(rng);
		const float dot = dots(rng);
		const int ambient = i % 3 == 0 ? model_light::k_DefaultAmbient : static_cast<int>(colour % 256);
		const auto mine = model_light::TwoSided(colour, dot, ambient);
		const auto original = EmulateFace(model_light::Intensity(dot), ambient, colour);
		ASSERT_EQ(mine.front, original.front) << std::hex << colour << " " << dot;
		ASSERT_EQ(mine.back, original.back) << std::hex << colour << " " << dot;
	}
}

TEST(DamageMeshLight, TwoSidedValues)
{
	// I = 255: the front 90 + (165 255 >> 8) = 254, the back -255 -> the ambient 90; alpha kept
	auto c = model_light::TwoSided(0xFFFFFFFFu, 1.0f, 90);
	EXPECT_EQ(c.front, 0xFFFDFDFDu);
	EXPECT_EQ(c.back, 0xFF595959u);
	// lit from behind: the other way round
	c = model_light::TwoSided(0x80FFFFFFu, -1.0f, 90);
	EXPECT_EQ(c.front, 0x80595959u);
	EXPECT_EQ(c.back, 0x80FDFDFDu);
	// edge on: I = 0, both at the ambient
	c = model_light::TwoSided(0xFF808080u, 0.0f, 90);
	EXPECT_EQ(c.front, c.back);
	EXPECT_EQ(c.front, 0xFF2D2D2Du); // (128 * 90) >> 8 = 45
}

TEST(DamageMeshLight, InverseSquareRoot)
{
	// golden values of the table + Newton step
	EXPECT_EQ(std::bit_cast<uint32_t>(gutils::ModelInverseSquareRoot(1.0f)), 0x3F7FFFA0u);
	EXPECT_EQ(std::bit_cast<uint32_t>(gutils::ModelInverseSquareRoot(2.0f)), 0x3F3504F3u);
	EXPECT_EQ(std::bit_cast<uint32_t>(gutils::ModelInverseSquareRoot(3.0f)), 0x3F13CCFEu);
	EXPECT_EQ(std::bit_cast<uint32_t>(gutils::ModelInverseSquareRoot(100.0f)), 0x3DCCCCA2u);
	std::mt19937 rng(3);
	std::uniform_real_distribution<float> logs(-10.0f, 14.0f);
	for (int i = 0; i < 100000; ++i)
	{
		const float x = std::exp2(logs(rng));
		const float r = gutils::ModelInverseSquareRoot(x);
		const double truth = 1.0 / std::sqrt(static_cast<double>(x));
		// one Newton step from a 7-bit guess: within 1e-4, and never above the true value (but for the float roundings)
		ASSERT_NEAR(r / truth, 1.0, 1e-4) << x;
		ASSERT_LE(r / truth, 1.0 + 1e-6) << x;
		// 4 x adds 2 to the exponent: the same table entry, the guess halved, the result exactly halved
		ASSERT_EQ(gutils::ModelInverseSquareRoot(4.0f * x), 0.5f * r) << x;
	}
}

TEST(DamageMeshLight, LightDirection)
{
	const auto l = DamageMesh::LightDirection({3.0f, 14.0f, -2.0f}, {3.0f, 4.0f, -2.0f});
	EXPECT_EQ(l.x, 0.0f);
	EXPECT_EQ(l.z, 0.0f);
	// 10 x ModelInverseSquareRoot(100)
	EXPECT_EQ(l.y, 10.0f * std::bit_cast<float>(0x3DCCCCA2u));
}

TEST(DamageMeshLight, TriangleVertices)
{
	auto t = FlatTriangle();
	DamageMesh::DrawLight light;
	light.lit = true;
	light.direction = {0.0f, -1.0f, 0.0f}; // the face normal (v1 - v0) x (v2 - v0) = (0, -1, 0)
	light.colour = 0xFF808080u;
	light.specular = 0xFF102030u;
	light.ambient = 90;
	std::vector<graphics::world_triangles::Vertex> out;
	DamageMesh::AppendTriangle(out, t, nullptr, light);
	// front, back, and a wall (2 triangles) on each of the 3 open edges
	ASSERT_EQ(out.size(), 6u + 3u * 6u);
	const float inverse = gutils::ModelInverseSquareRoot(1.0f);
	const auto expected = model_light::TwoSided(light.colour, -1.0f * -inverse, 90);
	EXPECT_EQ(model_light::Intensity(inverse), 255);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(out[i].position, t.corners.at(i).position);
		EXPECT_EQ(out[i].uv, t.corners.at(i).uv);
		EXPECT_EQ(ArgbOf(out[i].abgr), expected.front);
		EXPECT_EQ(ArgbOf(out[i].specular), light.specular);
		// the back copy in reverse order, 0.45 behind along -n = (0, +1, 0)
		const auto& backVertex = out[3 + i];
		const auto& source = t.corners.at(2 - i);
		EXPECT_EQ(backVertex.position.x, source.position.x);
		EXPECT_EQ(backVertex.position.y, 0.0f - 0.45f * -inverse);
		EXPECT_EQ(backVertex.position.z, source.position.z);
		EXPECT_EQ(backVertex.uv, source.uv);
		EXPECT_EQ(ArgbOf(backVertex.abgr), expected.back);
	}
	EXPECT_EQ(expected.front, 0xFF7F7F7Fu); // (128 * 254) >> 8
	EXPECT_EQ(expected.back, 0xFF2D2D2Du);  // (128 * 90) >> 8
	// the first wall, edge 0: (f0, b0, f1), (f1, b0, b1)
	EXPECT_EQ(out[6].position, out[0].position);
	EXPECT_EQ(out[7].position, out[5].position);
	EXPECT_EQ(out[8].position, out[1].position);
	EXPECT_EQ(out[9].position, out[1].position);
	EXPECT_EQ(out[10].position, out[5].position);
	EXPECT_EQ(out[11].position, out[4].position);
	EXPECT_EQ(ArgbOf(out[7].abgr), expected.back);
	EXPECT_EQ(ArgbOf(out[8].abgr), expected.front);

	// closed edges: no walls
	t.neighbours = {1, 2, 3};
	out.clear();
	DamageMesh::AppendTriangle(out, t, nullptr, light);
	EXPECT_EQ(out.size(), 6u);

	// unlit (no land light): the colour as it is on both sides
	light.lit = false;
	out.clear();
	DamageMesh::AppendTriangle(out, t, nullptr, light);
	EXPECT_EQ(ArgbOf(out[0].abgr), light.colour);
	EXPECT_EQ(ArgbOf(out[3].abgr), light.colour);
}

TEST(DamageMeshLight, TriangleThroughTheMatrix)
{
	const auto t = FlatTriangle();
	DamageMesh::DrawLight light;
	light.lit = true;
	light.direction = {0.0f, 1.0f, 0.0f};
	light.colour = 0xFFFFFFFFu;
	// half a turn about x: the face turns to +y and meets the light
	const glm::mat4 matrix = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, 5.0f)) *
	                         glm::mat4(glm::mat3(1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f));
	std::vector<graphics::world_triangles::Vertex> out;
	DamageMesh::AppendTriangle(out, t, &matrix, light);
	EXPECT_EQ(out[0].position, glm::vec3(10.0f, 2.0f, 5.0f));
	EXPECT_EQ(out[1].position, glm::vec3(11.0f, 2.0f, 5.0f));
	EXPECT_EQ(out[2].position, glm::vec3(10.0f, 2.0f, 4.0f));
	const auto expected = model_light::TwoSided(light.colour, gutils::ModelInverseSquareRoot(1.0f), 90);
	EXPECT_EQ(ArgbOf(out[0].abgr), expected.front);
	EXPECT_EQ(ArgbOf(out[3].abgr), expected.back);
	EXPECT_EQ(expected.front, 0xFFFDFDFDu);
	// the back copy below the turned face
	EXPECT_LT(out[3].position.y, 2.0f);
}

// The rules of the partly built draw over a broken building, with made-up numbers
TEST(DamageMeshPartialBuild, DrawShareIsTheSmallerOfBuiltAndRepaired)
{
	namespace damage = openblack::physics::damage;
	EXPECT_EQ(damage::DrawShare(0.5f, 0.7f), 0.5f);
	EXPECT_EQ(damage::DrawShare(0.9f, 0.3f), 0.3f);
	EXPECT_EQ(damage::DrawShare(0.4f, 0.4f), 0.4f);
}

TEST(DamageMeshPartialBuild, RepairedShareCountsFromTheRepairStart)
{
	namespace damage = openblack::physics::damage;
	// no broken model: a little short of the life
	EXPECT_FLOAT_EQ(damage::RepairedShare(0.5f, std::nullopt), 0.5f * 0.98f);
	// from the life the repair started at
	EXPECT_FLOAT_EQ(damage::RepairedShare(0.6f, 0.2f), (0.6f - 0.2f) / (1.0f - 0.2f));
	// nothing regained since, or a repair that started whole, counts as none
	EXPECT_EQ(damage::RepairedShare(0.2f, 0.2f), 0.0f);
	EXPECT_EQ(damage::RepairedShare(0.5f, 1.0f), 0.0f);
}

TEST(DamageMeshPartialBuild, TheScaffoldRisesStandsThenIsCut)
{
	namespace damage = openblack::physics::damage;
	// half built: the model cut half way up, the scaffold whole and standing
	const auto half = damage::PartialBuildOf(0.5f, 10.0f, 2.0f, 1.0f);
	EXPECT_EQ(half.modelCut, 12.0f);
	EXPECT_FALSE(half.scaffoldDrop.has_value());
	EXPECT_FALSE(half.scaffoldCut.has_value());
	// a tenth built: the scaffold still rising, down by (1 - 5 x 0.1) x half x 2
	const auto early = damage::PartialBuildOf(0.1f, 10.0f, 2.0f, 1.0f);
	ASSERT_TRUE(early.scaffoldDrop.has_value());
	EXPECT_FLOAT_EQ(*early.scaffoldDrop, (1.0f - 0.1f * 5.0f) * 2.0f * -2.0f);
	EXPECT_FALSE(early.scaffoldCut.has_value());
	// nine tenths built: the scaffold cut down from its top
	const auto late = damage::PartialBuildOf(0.9f, 10.0f, 2.0f, 1.0f);
	EXPECT_FALSE(late.scaffoldDrop.has_value());
	ASSERT_TRUE(late.scaffoldCut.has_value());
	EXPECT_FLOAT_EQ(*late.scaffoldCut, (1.0f - (0.9f - 0.8f) * 5.0f) * 2.0f * 1.0f * 2.0f + 10.0f);
	// the share is held between 0 and 1
	EXPECT_EQ(damage::PartialBuildOf(2.0f, 10.0f, 2.0f, 1.0f).modelCut,
	          damage::PartialBuildOf(1.0f, 10.0f, 2.0f, 1.0f).modelCut);
	EXPECT_EQ(damage::PartialBuildOf(-1.0f, 10.0f, 2.0f, 1.0f).modelCut, 10.0f);
}
