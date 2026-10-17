/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/HandMeshRay.h"

using namespace openblack::ecs::hand_mesh_ray;
using Flags = openblack::l3d::L3DSubmeshHeader::Flags;

namespace
{
/// The flags of a sub-mesh drawn at the first level of detail, as the meshes ship them
Flags FirstLevel()
{
	Flags flags {};
	flags.unknown2 = 0b10;
	flags.lodMask = 0b111;
	return flags;
}

/// A sub-mesh of triangles, at the first level of detail unless its flags say otherwise
struct FakeSubMesh
{
	std::vector<glm::vec3> positions;
	std::vector<uint16_t> indices;
	Flags flags = FirstLevel();

	[[nodiscard]] SubMesh View() const { return {.flags = flags, .positions = positions, .indices = indices}; }
};

/// One triangle in the plane z = depth, (0, 0) (2, 0) (0, 2) in x and y; `reversed` swaps its last two corners
void AddTriangle(FakeSubMesh& subMesh, float depth, bool reversed = false)
{
	const auto first = static_cast<uint16_t>(subMesh.positions.size());
	subMesh.positions.emplace_back(0.0f, 0.0f, depth);
	subMesh.positions.emplace_back(2.0f, 0.0f, depth);
	subMesh.positions.emplace_back(0.0f, 2.0f, depth);
	const auto second = static_cast<uint16_t>(first + (reversed ? 2 : 1));
	const auto third = static_cast<uint16_t>(first + (reversed ? 1 : 2));
	subMesh.indices.insert(subMesh.indices.end(), {first, second, third});
}

const glm::vec3 k_Origin(0.5f, 0.5f, -5.0f);
const glm::vec3 k_Forward(0.0f, 0.0f, 1.0f);

std::optional<Hit> TestMesh(const std::vector<FakeSubMesh>& fakes, const glm::mat4& model = glm::mat4(1.0f),
                            glm::vec3 origin = k_Origin, glm::vec3 direction = k_Forward)
{
	std::vector<SubMesh> views;
	for (const auto& fake : fakes)
	{
		views.push_back(fake.View());
	}
	return Nearest(views, model, origin, direction);
}
} // namespace

TEST(HandMeshRay, TheNearestTriangleWinsNotTheFirst)
{
	FakeSubMesh subMesh;
	AddTriangle(subMesh, 3.0f); // first in the file, further
	AddTriangle(subMesh, 1.0f);
	const auto hit = TestMesh({subMesh});
	ASSERT_TRUE(hit.has_value());
	EXPECT_FLOAT_EQ(hit->distance, 6.0f);
	EXPECT_FLOAT_EQ(hit->point.z, 1.0f);
}

TEST(HandMeshRay, TheNearestAcrossSubMeshes)
{
	FakeSubMesh far;
	AddTriangle(far, 4.0f);
	FakeSubMesh near;
	AddTriangle(near, 2.0f);
	const auto hit = TestMesh({far, near});
	ASSERT_TRUE(hit.has_value());
	EXPECT_FLOAT_EQ(hit->distance, 7.0f);
}

TEST(HandMeshRay, BothFacesAreHitAndTheNormalFacesAwayFromTheRay)
{
	FakeSubMesh front;
	AddTriangle(front, 0.0f);
	FakeSubMesh back;
	AddTriangle(back, 0.0f, true);
	const auto a = TestMesh({front});
	const auto b = TestMesh({back});
	ASSERT_TRUE(a.has_value());
	ASSERT_TRUE(b.has_value());
	EXPECT_FLOAT_EQ(a->distance, 5.0f);
	EXPECT_FLOAT_EQ(b->distance, 5.0f);
	// (V1 - V0) x (V2 - V0) is +z for the first winding and -z for the other; both come out along the ray
	EXPECT_EQ(a->normal, glm::vec3(0.0f, 0.0f, 1.0f));
	EXPECT_EQ(b->normal, glm::vec3(0.0f, 0.0f, 1.0f));
	// from the other side the normal turns with the ray
	const auto c = TestMesh({front}, glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 5.0f), -k_Forward);
	ASSERT_TRUE(c.has_value());
	EXPECT_EQ(c->normal, glm::vec3(0.0f, 0.0f, -1.0f));
}

TEST(HandMeshRay, OnlyTheFirstLevelOfDetailIsTested)
{
	FakeSubMesh lower; // drawn only further away
	AddTriangle(lower, 1.0f);
	lower.flags.lodMask = 0b110;
	FakeSubMesh drawn;
	AddTriangle(drawn, 3.0f);
	const auto hit = TestMesh({lower, drawn});
	ASSERT_TRUE(hit.has_value());
	EXPECT_FLOAT_EQ(hit->point.z, 3.0f);
}

TEST(HandMeshRay, PhysicsAndStatusSubMeshesOfTheFirstLevelAreTested)
{
	FakeSubMesh physics;
	AddTriangle(physics, 1.0f);
	physics.flags.isPhysics = 1;
	FakeSubMesh status;
	AddTriangle(status, 2.0f);
	status.flags.status = 1;
	FakeSubMesh drawn;
	AddTriangle(drawn, 3.0f);
	EXPECT_FLOAT_EQ(TestMesh({drawn, status}).value().point.z, 2.0f);
	EXPECT_FLOAT_EQ(TestMesh({drawn, status, physics}).value().point.z, 1.0f);
	// a physics sub-mesh as the meshes ship it, without the first level, is not
	physics.flags.lodMask = 0;
	EXPECT_FLOAT_EQ(TestMesh({drawn, physics}).value().point.z, 3.0f);
}

TEST(HandMeshRay, ASubMeshWithNoLevelIsOfEveryLevelUnlessItCarriesBitEleven)
{
	Flags flags {};
	EXPECT_TRUE(Tested(flags));
	flags.unknown2 = 0b10;
	EXPECT_FALSE(Tested(flags));
	flags.unknown2 = 0b01;
	EXPECT_TRUE(Tested(flags));
	flags.lodMask = 0b100;
	EXPECT_FALSE(Tested(flags));
	flags.lodMask = 0b001;
	EXPECT_TRUE(Tested(flags));
}

TEST(HandMeshRay, ARayNearlyAlongThePlaneMisses)
{
	FakeSubMesh subMesh;
	// a big triangle in the plane y = 0, so that the grazing ray still crosses it
	subMesh.positions = {{-1000.0f, 0.0f, -1000.0f}, {1000.0f, 0.0f, -1000.0f}, {0.0f, 0.0f, 1000.0f}};
	subMesh.indices = {0, 1, 2};
	const glm::vec3 origin(0.0f, 1.0f, 0.0f);
	const auto steep = glm::normalize(glm::vec3(0.0f, -0.006f, std::sqrt(1.0f - 0.006f * 0.006f)));
	const auto grazing = glm::normalize(glm::vec3(0.0f, -0.004f, std::sqrt(1.0f - 0.004f * 0.004f)));
	EXPECT_TRUE(TestMesh({subMesh}, glm::mat4(1.0f), origin, steep).has_value());
	EXPECT_FALSE(TestMesh({subMesh}, glm::mat4(1.0f), origin, grazing).has_value());
}

TEST(HandMeshRay, NothingBehindTheRaysStart)
{
	FakeSubMesh subMesh;
	AddTriangle(subMesh, -6.0f);
	EXPECT_FALSE(TestMesh({subMesh}).has_value());
}

TEST(HandMeshRay, APointOnAnEdgeMisses)
{
	FakeSubMesh subMesh;
	AddTriangle(subMesh, 0.0f);
	// along the edge from (0, 0) to (2, 0)
	EXPECT_FALSE(TestMesh({subMesh}, glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, -5.0f)).has_value());
	EXPECT_TRUE(TestMesh({subMesh}, glm::mat4(1.0f), glm::vec3(1.0f, 0.001f, -5.0f)).has_value());
}

TEST(HandMeshRay, TheTestIsInWorldSpaceWithTheObjectsMatrix)
{
	FakeSubMesh subMesh;
	AddTriangle(subMesh, 1.0f);
	// scaled by 2 and moved 10 along x: the triangle is at z = 2, from x = 10 to 14
	const auto model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)), glm::vec3(2.0f));
	const auto hit = TestMesh({subMesh}, model, glm::vec3(11.0f, 1.0f, -5.0f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_FLOAT_EQ(hit->distance, 7.0f);
	EXPECT_EQ(hit->point, glm::vec3(11.0f, 1.0f, 2.0f));
	EXPECT_EQ(hit->normal, glm::vec3(0.0f, 0.0f, 1.0f));
	// in the mesh's space the ray would have missed it (x 11 is past its corner at 2)
	EXPECT_FALSE(TestMesh({subMesh}, glm::mat4(1.0f), glm::vec3(11.0f, 1.0f, -5.0f)).has_value());
}

TEST(HandMeshRay, TheHandsDistanceIsAlongTheRay)
{
	const glm::vec3 origin(1.0f, 2.0f, 3.0f);
	const auto direction = glm::normalize(glm::vec3(1.0f, -1.0f, 2.0f));
	EXPECT_NEAR(AlongRay(origin, direction, origin + direction * 12.5f), 12.5f, 1e-5f);
	EXPECT_FLOAT_EQ(AlongRay(origin, direction, origin), 0.0f);
}
