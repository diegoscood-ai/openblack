/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// When a blow on a broken building makes its damage mesh afresh from the intact model (src/Physics/DamageMesh.h): only
// once the building is drawn at least a fifth repaired, but not wholly. A town's building is drawn a little repaired
// right after a blow, so it waits for its villagers; one with no town is made afresh by every later blow. The model is
// made afresh before the new blow strikes it, on small synthetic meshes. And how a big piece that lands is added back
// to its building: its first primitive, each corner through the body's matrix in the original's order.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <memory>
#include <optional>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandomTesting.h"
#include "Locator.h"
#include "Physics/DamageMesh.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace damage = openblack::physics::damage;
using DamageMesh = openblack::physics::damage::Mesh;

namespace
{
/// The repair site's starting life when a blow leaves the building at `life`: 1.1 x life - 0.1, in the game's order
float RepairStartAfterBlow(float life)
{
	const float scaled = 1.1f * life;
	return scaled - 0.1f;
}

/// How much of a built building is drawn standing at `life`, with its repair site's starting life if it has one
float DrawnShare(float life, std::optional<float> repairStart)
{
	return damage::DrawShare(1.0f, damage::RepairedShare(life, repairStart));
}

/// A square of two triangles standing on the ground (y = -1 to 0) in the plane z = 0, from x to x + 1
std::vector<DamageMesh::Triangle> GroundSquare(float x)
{
	const glm::vec3 a {x, -1.0f, 0.0f};
	const glm::vec3 b {x + 1.0f, -1.0f, 0.0f};
	const glm::vec3 c {x + 1.0f, 0.0f, 0.0f};
	const glm::vec3 d {x, 0.0f, 0.0f};
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

/// The intact model: two squares far apart on the ground, each its own primitive
std::shared_ptr<DamageMesh> Intact()
{
	std::vector<DamageMesh::Primitive> primitives;
	for (const float x : {0.0f, 20.0f})
	{
		primitives.push_back(DamageMesh::Primitive {graphics::L3DSubMesh::Primitive {}, GroundSquare(x), {}});
	}
	return DamageMesh::FromPrimitives(std::move(primitives));
}

/// The synced stream returns 0, 1, 2 and so on; the land is gone, so its height is 0
class DamageMeshRebuildTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::terrainSystem::reset();
		game_random::testing::SetGameRand([this](uint32_t /*n*/) { return _draws++; }, nullptr);
	}

	/// A blow straight through the square at x, as a thrown rock's, recording the centres of the pieces it knocks off
	void Strike(DamageMesh& mesh, float x)
	{
		mesh.Impact({x + 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1.5f,
		            [this](const DamageMesh::Piece& piece) { _pieces.push_back(piece.centre); });
	}

	/// A fresh intact model, counting the calls
	std::shared_ptr<DamageMesh> FromIntact()
	{
		++_madeFromIntact;
		return Intact();
	}

	/// The building after its first blow, which knocks the left square out
	std::shared_ptr<const DamageMesh> BrokenOnce()
	{
		auto first = damage::ModelForBlow(nullptr, 1.0f, [this]() { return FromIntact(); }).mesh;
		Strike(*first, 0.0f);
		EXPECT_EQ(first->TriangleCount(), 2u);
		_pieces.clear();
		return first;
	}

	const test::RestoreService<Locator::terrainSystem> _restoreTerrain;
	game_random::testing::ScopedState _random;
	uint32_t _draws {0};
	int _madeFromIntact {0};
	std::vector<glm::vec3> _pieces;
};
} // namespace

TEST(DamageMeshMerge, ACornerGoesThroughTheBodysMatrixInTheOriginalsOrder)
{
	// rows chosen so that any other order of the sums rounds the 1 away: x = ((x r0.x + z r2.x) + y r1.x) + c.x,
	// y = ((z r2.y + y r1.y) + x r0.y) + c.y, z = ((x r0.z + z r2.z) + y r1.z) + c.z
	glm::mat3 rows(1.0f);
	rows[0] = {1e8f, 1.0f, 1e8f};
	rows[1] = {1.0f, -1e8f, 1.0f};
	rows[2] = {-1e8f, 1e8f, -1e8f};
	const glm::vec3 centre {0.25f, 0.5f, 0.75f};
	const auto corner = DamageMesh::StuckCorner(rows, centre, glm::vec3(1.0f));
	EXPECT_EQ(corner.x, 1.25f);
	EXPECT_EQ(corner.y, 1.5f);
	EXPECT_EQ(corner.z, 1.75f);
	// with plain rows: the corner turned and moved to the centre
	EXPECT_EQ(DamageMesh::StuckCorner(glm::mat3(1.0f), {10.0f, 0.0f, 20.0f}, {1.0f, 2.0f, 3.0f}),
	          glm::vec3(11.0f, 2.0f, 23.0f));
}

namespace
{
// Produced by running the original game's own end of physics for a sticking piece natively, on random rows, centres
// and corners (game-sized and wide-ranging values, chosen where another order of the sums or wider arithmetic would
// give other bits). Rows are the body's three rows, one after another.
struct StuckCornerCase
{
	std::array<uint32_t, 9> rows;   // f32 bits, in
	std::array<uint32_t, 3> centre; // f32 bits, in
	std::array<uint32_t, 3> corner; // f32 bits, in
	std::array<uint32_t, 3> moved;  // f32 bits, out
};
constexpr std::array<StuckCornerCase, 12> k_StuckCornerCases {{
    {{0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000, 0x00000000, 0x00000000, 0x00000000, 0x3F800000},
     {0x3F6F6FF7, 0xBEA00C6F, 0xBE2C508F},
     {0xBE1B91E5, 0xBD2A3F31, 0xBF800000},
     {0x3F488B7E, 0xBEB55455, 0xBF958A12}},
    {{0x4CBEBC20, 0x3F800000, 0x4CBEBC20, 0x3F800000, 0xCCBEBC20, 0x3F800000, 0xCCBEBC20, 0x4CBEBC20, 0xCCBEBC20},
     {0x3E800000, 0x3F000000, 0x3F400000},
     {0x3F800000, 0x3F800000, 0x3F800000},
     {0x3FA00000, 0x3FC00000, 0x3FE00000}},
    {{0x4CBEBC20, 0x3F800000, 0x4CBEBC20, 0x3F800000, 0xCCBEBC20, 0x3F800000, 0xCCBEBC20, 0x4CBEBC20, 0xCCBEBC20},
     {0x3E800000, 0x3F000000, 0x3F400000},
     {0xBEFC220F, 0x42DEEFFE, 0xBEBDB8FB},
     {0xCB39FEF9, 0xD026A74E, 0xCB39FEF8}},
    {{0x4CBEBC20, 0x3F800000, 0x4CBEBC20, 0x3F800000, 0xCCBEBC20, 0x3F800000, 0xCCBEBC20, 0x4CBEBC20, 0xCCBEBC20},
     {0x3E800000, 0x3F000000, 0x3F400000},
     {0xC3E7413B, 0x42006859, 0x41B17325},
     {0xD1348FB9, 0xCE6C870B, 0xD1348FB9}},
    {{0x4CBEBC20, 0x3F800000, 0x4CBEBC20, 0x3F800000, 0xCCBEBC20, 0x3F800000, 0xCCBEBC20, 0x4CBEBC20, 0xCCBEBC20},
     {0x3E800000, 0x3F000000, 0x3F400000},
     {0x3F000000, 0x427B5B37, 0x3EE17F3F},
     {0x4AB5D04E, 0xCFB9F665, 0x4AB5D050}},
    {{0xBF3DE639, 0xBF2D9F53, 0xBD5B50F5, 0xBF60F7D6, 0xBF1650F8, 0x3F53576D, 0x3F800000, 0x4239EECF, 0xC2744445},
     {0xC2DF5934, 0x3F01B708, 0xC1927C83},
     {0xBF1102BD, 0x3F70A3B6, 0x451AEDF1},
     {0x4513ECA8, 0x47E10D1E, 0xC813D87B}},
    {{0xBF3DE639, 0xBF2D9F53, 0xBD5B50F5, 0xBF60F7D6, 0xBF1650F8, 0x3F53576D, 0x3F800000, 0x4239EECF, 0xC2744445},
     {0xC2DF5934, 0x3F01B708, 0xC1927C83},
     {0x3F207773, 0xC2996633, 0x43107E02},
     {0x42C782AE, 0x45D34CA4, 0xC60B2549}},
    {{0xC78B63B0, 0x4816AD64, 0xC37AD533, 0x4468D37B, 0xC52536A5, 0x3F0A738C, 0xBF11F179, 0x41AA0754, 0xBBC50F1D},
     {0xC50D9A8D, 0x3DC38894, 0x401EB106},
     {0x4149F92D, 0x43F1D3C5, 0xC7754BFB},
     {0xC8CB945D, 0xC92275FC, 0xC51DCA77}},
    {{0xBDA2703F, 0x41D47769, 0xBF81A033, 0x4932C4C0, 0x4033F604, 0xC1C791D4, 0xC3058E91, 0x41C01B4D, 0xC71D51B6},
     {0x3C054D9D, 0xBB1BBE35, 0x4858C27B},
     {0x3D0F39B4, 0x41A2335B, 0xC06A7BBF},
     {0x4B628AA5, 0xC1F05717, 0x48B42E62}},
    {{0xBE4BFC18, 0x3E210A61, 0xCACCDFDF, 0xC0D77B43, 0xC1BC38D8, 0xC51F5157, 0x3F648D85, 0x46FE338A, 0xC25DC257},
     {0x47EA8641, 0xBF048038, 0xBB928AD9},
     {0xC41ECEC7, 0xC9D11792, 0x47A71D1F},
     {0x4B32FF69, 0x4F285779, 0x500089C4}},
    {{0xC2F5AA4F, 0x43BAC9B5, 0xC10D8159, 0x45AABBE5, 0x3AAD4792, 0x4652BE3A, 0xC5F3032A, 0xC198B0B9, 0xC316A57D},
     {0x3C76B158, 0xC023677B, 0x3A8B10E1},
     {0x3D7925C0, 0x42BDA6D9, 0x43023B42},
     {0xC8F18861, 0xC51A1572, 0x4999BAAF}},
    {{0xC2F5AA4F, 0x43BAC9B5, 0xC10D8159, 0x45AABBE5, 0x3AAD4792, 0x4652BE3A, 0xC5F3032A, 0xC198B0B9, 0xC316A57D},
     {0x3C76B158, 0xC023677B, 0x3A8B10E1},
     {0x48934B2B, 0xC1C80AA9, 0x4AAAD88C},
     {0xD1225120, 0x4AB244FB, 0xCE49CA12}},
}};
} // namespace

TEST(DamageMeshMerge, StuckCornersMatchTheOriginalsBits)
{
	for (const auto& c : k_StuckCornerCases)
	{
		glm::mat3 rows(1.0f);
		for (int i = 0; i < 3; ++i)
		{
			rows[i] = {std::bit_cast<float>(c.rows.at(3 * i)), std::bit_cast<float>(c.rows.at(3 * i + 1)),
			           std::bit_cast<float>(c.rows.at(3 * i + 2))};
		}
		const glm::vec3 centre {std::bit_cast<float>(c.centre[0]), std::bit_cast<float>(c.centre[1]),
		                        std::bit_cast<float>(c.centre[2])};
		const glm::vec3 corner {std::bit_cast<float>(c.corner[0]), std::bit_cast<float>(c.corner[1]),
		                        std::bit_cast<float>(c.corner[2])};
		const auto moved = DamageMesh::StuckCorner(rows, centre, corner);
		EXPECT_EQ(std::bit_cast<uint32_t>(moved.x), c.moved[0]);
		EXPECT_EQ(std::bit_cast<uint32_t>(moved.y), c.moved[1]);
		EXPECT_EQ(std::bit_cast<uint32_t>(moved.z), c.moved[2]);
	}
}

TEST(DamageMeshMerge, OnlyThePiecesFirstPrimitiveIsAddedAfterTheBuildingsOwn)
{
	const auto building = Intact();
	ASSERT_EQ(building->TriangleCount(), 4u);
	std::vector<DamageMesh::Primitive> parts;
	for (const float x : {0.0f, 40.0f})
	{
		parts.push_back(DamageMesh::Primitive {graphics::L3DSubMesh::Primitive {}, GroundSquare(x), {}});
	}
	const auto piece = DamageMesh::FromPrimitives(std::move(parts));
	building->Merge(*piece, glm::mat3(1.0f), {100.0f, 0.0f, 0.0f});
	EXPECT_EQ(building->TriangleCount(), 6u);
	std::vector<glm::vec3> corners;
	std::vector<glm::vec3> normals;
	building->UniqueVertices(corners, normals);
	const auto has = [&corners](glm::vec3 p) { return std::find(corners.begin(), corners.end(), p) != corners.end(); };
	// the first primitive, moved to the centre, comes last
	EXPECT_TRUE(has({100.0f, -1.0f, 0.0f}));
	EXPECT_TRUE(has({101.0f, 0.0f, 0.0f}));
	EXPECT_EQ(corners.back(), glm::vec3(100.0f, 0.0f, 0.0f));
	// the second is not added
	EXPECT_FALSE(has({140.0f, -1.0f, 0.0f}));
	EXPECT_FALSE(has({141.0f, 0.0f, 0.0f}));
}

TEST(DamageMeshRebuild, FromAFifthDrawnUpToButNotWhole)
{
	EXPECT_TRUE(damage::RebuildsOnBlow(0.2f));
	EXPECT_FALSE(damage::RebuildsOnBlow(std::nextafter(0.2f, 0.0f)));
	EXPECT_TRUE(damage::RebuildsOnBlow(std::nextafter(0.2f, 1.0f)));
	EXPECT_TRUE(damage::RebuildsOnBlow(0.5f));
	EXPECT_TRUE(damage::RebuildsOnBlow(std::nextafter(1.0f, 0.0f)));
	EXPECT_FALSE(damage::RebuildsOnBlow(1.0f));
	EXPECT_FALSE(damage::RebuildsOnBlow(1.0f / 11.0f));
	EXPECT_FALSE(damage::RebuildsOnBlow(0.0f));
}

TEST(DamageMeshRebuild, ATownsBuildingRightAfterABlowIsNotRebuilt)
{
	// Its repair site starts at 1.1 x life - 0.1: it is drawn (life - s) / (1 - s) = 1/11 repaired, whatever its life
	for (const float life : {0.95f, 0.75f, 0.5f, 0.32f, 0.11f})
	{
		const float share = DrawnShare(life, RepairStartAfterBlow(life));
		EXPECT_NEAR(share, 1.0f / 11.0f, 1e-4f) << life;
		EXPECT_FALSE(damage::RebuildsOnBlow(share)) << life;
	}
}

TEST(DamageMeshRebuild, ATownsBuildingIsRebuiltOnceItsLifeHasRisen)
{
	// Broken down to 0.5, its site starts at 0.45: it is rebuilt from life 0.45 + 0.2 x 0.55 = 0.56 on
	const float start = RepairStartAfterBlow(0.5f);
	EXPECT_FALSE(damage::RebuildsOnBlow(DrawnShare(0.5f, start)));
	EXPECT_FALSE(damage::RebuildsOnBlow(DrawnShare(0.55f, start)));
	EXPECT_TRUE(damage::RebuildsOnBlow(DrawnShare(0.57f, start)));
	EXPECT_TRUE(damage::RebuildsOnBlow(DrawnShare(0.99f, start)));
}

TEST(DamageMeshRebuild, ABuildingWithNoTownIsRebuiltByEveryLaterBlow)
{
	// No repair site: drawn 0.98 x its life repaired, so rebuilt while that is at least a fifth
	EXPECT_TRUE(damage::RebuildsOnBlow(DrawnShare(0.32f, std::nullopt)));
	EXPECT_TRUE(damage::RebuildsOnBlow(DrawnShare(0.21f, std::nullopt)));
	EXPECT_FALSE(damage::RebuildsOnBlow(DrawnShare(0.2f, std::nullopt)));
	EXPECT_FALSE(damage::RebuildsOnBlow(DrawnShare(0.1f, std::nullopt)));
}

TEST_F(DamageMeshRebuildTest, TheFirstBlowStrikesTheIntactModel)
{
	auto model = damage::ModelForBlow(nullptr, 1.0f, [this]() { return FromIntact(); });
	ASSERT_NE(model.mesh.get(), nullptr);
	EXPECT_FALSE(model.rebuilt);
	EXPECT_EQ(_madeFromIntact, 1);
	EXPECT_EQ(model.mesh->TriangleCount(), 4u);
}

TEST_F(DamageMeshRebuildTest, ABlowBelowAFifthStrikesWhatIsLeft)
{
	const auto broken = BrokenOnce();
	// Right after it the share is 1/11: the second blow strikes a copy of what is left and knocks the right square out
	const float share = DrawnShare(0.5f, RepairStartAfterBlow(0.5f));
	auto second = damage::ModelForBlow(broken, share, [this]() { return FromIntact(); });
	ASSERT_NE(second.mesh.get(), nullptr);
	EXPECT_FALSE(second.rebuilt);
	EXPECT_EQ(_madeFromIntact, 1);
	EXPECT_NE(second.mesh.get(), broken.get());
	EXPECT_EQ(second.mesh->TriangleCount(), 2u);
	Strike(*second.mesh, 20.0f);
	EXPECT_EQ(second.mesh->TriangleCount(), 0u);
	ASSERT_EQ(_pieces.size(), 1u);
	EXPECT_NEAR(_pieces.at(0).x, 20.5f, 1e-4f);
	// the old damage mesh is left as it was
	EXPECT_EQ(broken->TriangleCount(), 2u);
}

TEST_F(DamageMeshRebuildTest, ARebuildRestoresTheIntactModelBeforeTheBlowStrikes)
{
	const auto broken = BrokenOnce();
	// Repaired far enough: the damage mesh is made afresh from the intact model, both squares standing, and only then
	// does the blow strike it. The left square stands again; the right one breaks off
	auto second = damage::ModelForBlow(broken, 0.5f, [this]() { return FromIntact(); });
	ASSERT_NE(second.mesh.get(), nullptr);
	EXPECT_TRUE(second.rebuilt);
	EXPECT_EQ(_madeFromIntact, 2);
	EXPECT_EQ(second.mesh->TriangleCount(), 4u);
	Strike(*second.mesh, 20.0f);
	EXPECT_EQ(second.mesh->TriangleCount(), 2u);
	EXPECT_NEAR(second.mesh->Centroid().x, 0.5f, 1e-4f);
	ASSERT_EQ(_pieces.size(), 1u);
	EXPECT_NEAR(_pieces.at(0).x, 20.5f, 1e-4f);
	EXPECT_EQ(broken->TriangleCount(), 2u);
}

TEST_F(DamageMeshRebuildTest, AWholeDrawIsNotRebuilt)
{
	const auto broken = BrokenOnce();
	auto second = damage::ModelForBlow(broken, 1.0f, [this]() { return FromIntact(); });
	ASSERT_NE(second.mesh.get(), nullptr);
	EXPECT_FALSE(second.rebuilt);
	EXPECT_EQ(_madeFromIntact, 1);
	EXPECT_EQ(second.mesh->TriangleCount(), 2u);
}
