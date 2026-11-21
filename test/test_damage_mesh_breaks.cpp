/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The order in which a breaking building's pieces are made (src/Physics/DamageMesh.h), on small synthetic meshes: a
// piece's own loose parts fly before it, each spins before the next is made, three synced draws are dropped before a
// broken piece's spin, and a piece sheds its dust at every distinct corner but the first.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandomTesting.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "support/RestoreService.h"

using namespace openblack;
using DamageMesh = openblack::physics::damage::Mesh;

namespace
{
/// A square of two triangles in the plane z = 0, from (x, y) to (x + w, y + h): (a, b, c) and (a, c, d)
std::vector<DamageMesh::Triangle> Square(float x, float y, float w = 1.0f, float h = 1.0f)
{
	const glm::vec3 a {x, y, 0.0f};
	const glm::vec3 b {x + w, y, 0.0f};
	const glm::vec3 c {x + w, y + h, 0.0f};
	const glm::vec3 d {x, y + h, 0.0f};
	std::vector<DamageMesh::Triangle> out;
	for (const auto& corners : {std::array {a, b, c}, std::array {a, c, d}})
	{
		DamageMesh::Triangle t;
		for (size_t k = 0; k < 3; ++k)
		{
			t.corners.at(k) = {corners.at(k), glm::vec2(0.0f)};
		}
		t.sizeClass = t.countedClass = DamageMesh::SizeClassOf(t);
		out.push_back(t);
	}
	return out;
}

DamageMesh::Primitive PrimitiveOf(const std::vector<std::vector<DamageMesh::Triangle>>& squares)
{
	DamageMesh::Primitive primitive {graphics::L3DSubMesh::Primitive {}, {}, {}};
	for (const auto& square : squares)
	{
		primitive.triangles.insert(primitive.triangles.end(), square.begin(), square.end());
	}
	return primitive;
}

/// A piece as it was launched, and how many synced draws had been made by then
struct Launched
{
	DamageMesh::Piece piece;
	uint32_t drawsBefore {0};
};

/// The synced stream returns 0, 1, 2 ... in turn and counts its draws; the land is gone, so its height is 0
class DamageMeshBreakTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::terrainSystem::reset();
		game_random::testing::SetGameRand(
		    [this](uint32_t n) {
			    EXPECT_EQ(n, 201u);
			    return _draws++;
		    },
		    nullptr);
	}

	DamageMesh::LaunchPiece Recorder()
	{
		return [this](const DamageMesh::Piece& piece) { _launched.push_back({piece, _draws}); };
	}

	/// The spin drawn from three draws starting at `first`: z first, x last
	static glm::vec3 Spin(uint32_t first, float scale)
	{
		const auto axis = [scale](uint32_t draw) { return static_cast<float>(static_cast<int32_t>(draw) - 100) * scale; };
		return {axis(first + 2), axis(first + 1), axis(first)};
	}

	const test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	game_random::testing::ScopedState _random;
	uint32_t _draws {0};
	std::vector<Launched> _launched;
};

void ExpectSpin(glm::vec3 actual, glm::vec3 expected)
{
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}
} // namespace

TEST_F(DamageMeshBreakTest, APieceLetsGoOfItsLoosePartBeforeItFlies)
{
	// A wall of two squares that don't touch, struck through both, and a square on the ground well away from the blow
	auto mesh = DamageMesh::FromPrimitives(
	    {PrimitiveOf({Square(0.0f, 5.0f), Square(3.0f, 5.0f)}), PrimitiveOf({Square(20.0f, -1.0f)})});
	const glm::vec3 velocity {0.0f, 0.0f, 1.0f};
	EXPECT_EQ(mesh->Impact({2.0f, 5.5f, 0.0f}, velocity, 2.2f, Recorder()), 2u);

	// The second square comes loose of what broke off: it is made, spins (x 0.02) and flies first, at rest
	ASSERT_EQ(_launched.size(), 2u);
	const auto& part = _launched.at(0);
	EXPECT_EQ(part.drawsBefore, 3u);
	EXPECT_EQ(part.piece.mesh->TriangleCount(), 2u);
	EXPECT_EQ(part.piece.lifeTriangles, 2u);
	EXPECT_NEAR(part.piece.centre.x, 3.5f, 1e-5f);
	EXPECT_NEAR(part.piece.centre.y, 5.5f, 1e-5f);
	EXPECT_EQ(part.piece.velocity, glm::vec3(0.0f));
	ExpectSpin(part.piece.angularVelocity, Spin(0, 0.02f));

	// Then three draws are dropped, and the piece left spins (x 0.01) and flies with the blow, about what is left of it;
	// its life counts every triangle it broke off with
	const auto& piece = _launched.at(1);
	EXPECT_EQ(piece.drawsBefore, 9u);
	EXPECT_EQ(piece.piece.mesh->TriangleCount(), 2u);
	EXPECT_EQ(piece.piece.lifeTriangles, 4u);
	EXPECT_NEAR(piece.piece.centre.x, 0.5f, 1e-5f);
	EXPECT_NEAR(piece.piece.centre.y, 5.5f, 1e-5f);
	EXPECT_EQ(piece.piece.velocity, velocity);
	ExpectSpin(piece.piece.angularVelocity, Spin(6, 0.01f));

	// The piece that broke off was made before the part that came loose of it
	EXPECT_EQ(piece.piece.made, 0u);
	EXPECT_EQ(part.piece.made, 1u);

	// The square on the ground holds: nothing more falls, and no other draw is made
	EXPECT_EQ(_draws, 9u);
	EXPECT_EQ(mesh->TriangleCount(), 2u);
}

TEST_F(DamageMeshBreakTest, ALoosePartLetsGoOfItsOwnPartsFirst)
{
	// A square that stays, and a loose group of three squares in a row: the outer two in one primitive, joined only by
	// the middle one of another primitive. The outer pair comes off as one part, which is in two pieces itself
	auto mesh =
	    DamageMesh::FromPrimitives({PrimitiveOf({Square(10.0f, 5.0f)}), PrimitiveOf({Square(0.0f, 5.0f), Square(2.0f, 5.0f)}),
	                                PrimitiveOf({Square(1.0f, 5.0f)})});
	const glm::vec3 offset {100.0f, 0.0f, 0.0f};
	mesh->SplitUnconnectedGroups(false, offset, Recorder());

	ASSERT_EQ(_launched.size(), 3u);
	// The outer pair's own loose square first, where it was
	const auto& own = _launched.at(0);
	EXPECT_EQ(own.drawsBefore, 3u);
	EXPECT_EQ(own.piece.mesh->TriangleCount(), 2u);
	EXPECT_NEAR(own.piece.centre.x, 102.5f, 1e-4f);
	ExpectSpin(own.piece.angularVelocity, Spin(0, 0.02f));
	// Then the outer pair, with what is left of it: not centred again, its life from both squares
	const auto& pair = _launched.at(1);
	EXPECT_EQ(pair.drawsBefore, 6u);
	EXPECT_EQ(pair.piece.mesh->TriangleCount(), 2u);
	EXPECT_EQ(pair.piece.lifeTriangles, 4u);
	EXPECT_NEAR(pair.piece.centre.x, 101.5f, 1e-4f);
	ExpectSpin(pair.piece.angularVelocity, Spin(3, 0.02f));
	// Then the middle square of the other primitive
	const auto& middle = _launched.at(2);
	EXPECT_EQ(middle.drawsBefore, 9u);
	EXPECT_EQ(middle.piece.lifeTriangles, 2u);
	EXPECT_NEAR(middle.piece.centre.x, 101.5f, 1e-4f);
	ExpectSpin(middle.piece.angularVelocity, Spin(6, 0.02f));
	for (const auto& launched : _launched)
	{
		EXPECT_EQ(launched.piece.velocity, glm::vec3(0.0f));
	}
	// Each piece was made before its own loose parts: the outer pair, its loose square, then the middle one
	EXPECT_EQ(pair.piece.made, 0u);
	EXPECT_EQ(own.piece.made, 1u);
	EXPECT_EQ(middle.piece.made, 2u);

	EXPECT_EQ(_draws, 9u);
	EXPECT_EQ(mesh->TriangleCount(), 2u); // the first square
}

TEST(DamageMeshPiece, DustAtEveryCornerButTheFirst)
{
	// Two triangles (a, b, c) and (a, c, d): four distinct corners, the dust at b, c and d
	const auto mesh = DamageMesh::FromPrimitives({PrimitiveOf({Square(0.0f, 0.0f)})});
	const auto dust = mesh->DustCorners();
	ASSERT_EQ(dust.size(), 3u);
	EXPECT_EQ(dust.at(0), glm::vec3(1.0f, 0.0f, 0.0f));
	EXPECT_EQ(dust.at(1), glm::vec3(1.0f, 1.0f, 0.0f));
	EXPECT_EQ(dust.at(2), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(DamageMeshPiece, AThinStripIsASliver)
{
	// About its middle, as a piece is: a unit square flies on, a long thin strip of the same kind is too thin
	EXPECT_FALSE(DamageMesh::FromPrimitives({PrimitiveOf({Square(-0.5f, -0.5f)})})->IsSliver());
	EXPECT_TRUE(DamageMesh::FromPrimitives({PrimitiveOf({Square(-5.0f, -0.025f, 10.0f, 0.05f)})})->IsSliver());
}

/// A fake float draw: hands out the values given in turn and keeps what each draw was asked for
struct FakeFloatDraws
{
	std::vector<float> values;
	std::vector<float> asked;
	[[nodiscard]] float operator()(float x)
	{
		asked.push_back(x);
		const auto value = values.at(asked.size() - 1);
		return value;
	}
};

TEST(DamageMeshSurfacePoint, TheFirstPrimitiveIsAlwaysTakenAfterItsDrawThenATriangleAndTwoShares)
{
	// two primitives, a unit square at the origin and one beside it: the first draw would pick the second, but the first
	// is taken. Its second triangle (a, c, d) at shares 0.25 and 0.5: a + (c - a) 0.25 + (d - a) 0.5
	const auto mesh = DamageMesh::FromPrimitives({PrimitiveOf({Square(0.0f, 0.0f)}), PrimitiveOf({Square(5.0f, 0.0f)})});
	FakeFloatDraws draws {.values = {1.9f, 1.5f, 0.25f, 0.5f}};
	const auto point = mesh->RandomSurfacePoint([&draws](float x) { return draws(x); });
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, glm::vec3(0.25f, 0.75f, 0.0f));
	EXPECT_EQ(draws.asked, (std::vector<float> {2.0f, 2.0f, 1.0f, 1.0f}));
}

TEST(DamageMeshSurfacePoint, SharesAddingUpToMoreThanOneAreFoldedBackInside)
{
	// 0.75 and 0.5 fold to 0.25 and 0.5; the first triangle (a, b, c): a + (b - a) 0.25 + (c - a) 0.5
	const auto mesh = DamageMesh::FromPrimitives({PrimitiveOf({Square(0.0f, 0.0f)})});
	FakeFloatDraws draws {.values = {0.0f, 0.0f, 0.75f, 0.5f}};
	const auto point = mesh->RandomSurfacePoint([&draws](float x) { return draws(x); });
	ASSERT_TRUE(point.has_value());
	EXPECT_EQ(*point, glm::vec3(0.75f, 0.5f, 0.0f));
	// shares adding up to exactly 1 stay as they are
	FakeFloatDraws edge {.values = {0.0f, 0.0f, 0.5f, 0.5f}};
	EXPECT_EQ(mesh->RandomSurfacePoint([&edge](float x) { return edge(x); }), glm::vec3(1.0f, 0.5f, 0.0f));
}

TEST(DamageMeshSurfacePoint, TheTriangleDrawIsCutToAWholeNumberWithinTheList)
{
	const auto mesh = DamageMesh::FromPrimitives({PrimitiveOf({Square(0.0f, 0.0f)})});
	// a draw of the whole count takes the last triangle (a, c, d), at its first corner with no shares
	FakeFloatDraws draws {.values = {0.0f, 2.0f, 0.0f, 0.0f}};
	EXPECT_EQ(mesh->RandomSurfacePoint([&draws](float x) { return draws(x); }), glm::vec3(0.0f, 0.0f, 0.0f));
	// 0.99 is cut to the first triangle (a, b, c): all of its first side is b
	FakeFloatDraws first {.values = {0.0f, 0.99f, 1.0f, 0.0f}};
	EXPECT_EQ(mesh->RandomSurfacePoint([&first](float x) { return first(x); }), glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(DamageMeshSurfacePoint, AMeshWithNoPrimitivesDrawsNothingAndGivesNoPoint)
{
	const auto mesh = DamageMesh::FromPrimitives({});
	FakeFloatDraws draws;
	EXPECT_EQ(mesh->RandomSurfacePoint([&draws](float x) { return draws(x); }), std::nullopt);
	EXPECT_TRUE(draws.asked.empty());
	// a first primitive with no triangles: its draw only
	const auto empty = DamageMesh::FromPrimitives({PrimitiveOf({}), PrimitiveOf({Square(0.0f, 0.0f)})});
	FakeFloatDraws once {.values = {1.0f}};
	EXPECT_EQ(empty->RandomSurfacePoint([&once](float x) { return once(x); }), std::nullopt);
	EXPECT_EQ(once.asked, (std::vector<float> {2.0f}));
}

TEST(DamageMeshSurfacePoint, ATrianglesPointIsItsFirstCornerAlongItsTwoSides)
{
	DamageMesh::Triangle triangle;
	triangle.corners = {DamageMesh::Corner {{1.0f, 2.0f, 3.0f}, glm::vec2(0.0f)},
	                    DamageMesh::Corner {{5.0f, 2.0f, 3.0f}, glm::vec2(0.0f)},
	                    DamageMesh::Corner {{1.0f, 10.0f, 7.0f}, glm::vec2(0.0f)}};
	EXPECT_EQ(openblack::physics::damage::TrianglePoint(triangle, 0.5f, 0.25f), glm::vec3(3.0f, 4.0f, 4.0f));
}
