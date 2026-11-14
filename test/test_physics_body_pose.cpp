/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Body::SetUpPos and Body::SettleOnLand against the original: the expected bits come from an
// independent float32 model of the FPU at 24-bit precision. No island: GetNormal = (0, 1, 0), GetAltitude = 0.

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "ECS/PhysicsGround.h"
#include "Physics/Body.h"

using openblack::physics::Body;
using openblack::physics::PhysicsData;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

Body MakeBody()
{
	constexpr std::array<glm::vec3, 3> k_Local = {glm::vec3(1.3f, 0.2f, -0.7f), glm::vec3(-0.9f, 0.6f, 0.4f),
	                                              glm::vec3(0.1f, -1.1f, 0.8f)};
	constexpr std::array<std::array<uint32_t, 3>, 1> k_Triangles = {{{0, 1, 2}}};
	constexpr PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	glm::mat3 rotation;
	rotation[0] = glm::vec3(0.36f, 0.48f, -0.8f);
	rotation[1] = glm::vec3(-0.6f, 0.8f, 0.0f);
	rotation[2] = glm::vec3(0.64f, 0.48f, 0.6f);
	Body body;
	body.Initialise(1.7f, 1.0f);
	body.SetUpConstants(6.75f, k_Data, true);
	body.BuildShape(k_Local, k_Triangles, glm::vec3(0.21f, -0.13f, 0.37f), 1.5f, 1.0f, rotation,
	                glm::vec3(1810.5f, 54.25f, 2640.75f));
	return body;
}

void ExpectBits(glm::vec3 v, uint32_t x, uint32_t y, uint32_t z)
{
	EXPECT_EQ(Bits(v.x), x);
	EXPECT_EQ(Bits(v.y), y);
	EXPECT_EQ(Bits(v.z), z);
}
} // namespace

TEST(PhysicsBodyPose, SetUpPos)
{
	const auto body = MakeBody();
	ExpectBits(body.Centre(), 0x44E2653Du, 0x425A2F98u, 0x45250D78u);
	ExpectBits(body.Axes()[0], 0x3EB851ECu, 0x3EF5C28Eu, 0xBF4CCCCCu);
	ExpectBits(body.Axes()[1], 0xBF19999Au, 0x3F4CCCCCu, 0x00000000u);
	ExpectBits(body.Axes()[2], 0x3F23D70Au, 0x3EF5C28Eu, 0x3F19999Au);
	ExpectBits(body.Points()[0].world, 0x44E2620Au, 0x425BFA59u, 0x4524F61Cu);
}

TEST(PhysicsBodyPose, FollowObject)
{
	// the resting proxy's end of turn: centre = position + (rows . com) x scale, summed in the original's order
	auto body = MakeBody();
	const glm::vec3 position(1700.25f, 31.5f, 2600.75f);
	body.FollowObject(position);
	const auto& r = body.Axes();
	const glm::vec3 com(0.21f, -0.13f, 0.37f);
	const float scale = 1.7f;
	const float tx = ((r[2].x * com.z) + (r[1].x * com.y)) + (com.x * r[0].x);
	const float ty = ((r[2].y * com.z) + (r[0].y * com.x)) + (r[1].y * com.y);
	const float tz = ((r[2].z * com.z) + (r[0].z * com.x)) + (r[1].z * com.y);
	EXPECT_EQ(Bits(body.Centre().x), Bits((tx * scale) + position.x));
	EXPECT_EQ(Bits(body.Centre().y), Bits((ty * scale) + position.y));
	EXPECT_EQ(Bits(body.Centre().z), Bits((tz * scale) + position.z));
	// and the object's origin taken back from the body is the position again (to the last bits)
	const auto origin = body.ObjectOrigin();
	EXPECT_NEAR(origin.x, position.x, 1e-3f);
	EXPECT_NEAR(origin.y, position.y, 1e-3f);
	EXPECT_NEAR(origin.z, position.z, 1e-3f);
}

TEST(PhysicsBodyPose, SettleOnLand)
{
	auto body = MakeBody();
	const openblack::ecs::PhysicsGround noIsland(nullptr);
	body.SettleOnLand(noIsland, false, true);
	ExpectBits(body.Axes()[0], 0x3ED21B54u, 0x00000000u, 0xBF6973B1u);
	ExpectBits(body.Axes()[1], 0x00000000u, 0x3F800000u, 0x00000000u);
	ExpectBits(body.Axes()[2], 0x3F6973B1u, 0x80000000u, 0x3ED21B54u);
	ExpectBits(body.Centre(), 0x44E2653Du, 0x3F8CCCC0u, 0x45250D78u);
}

// A body drawn between turns: the object's origin from its lerped rows and centre as the drawing works it out, each row
// times the scale first, then t_i = ((r1_i cy) + (r2_i cz)) + (r0_i cx) (the values worked out from the original's
// float operations; the rows times the scaled centre of mass gave 0xBF3FA137, 0x4024B14E, 0x3CCDDC50)
TEST(PhysicsBodyPose, TheDrawnOriginIsWorkedOutAsTheDrawingWorksItOut)
{
	const auto body = MakeBody();
	glm::mat3 rotation;
	rotation[0] =
	    glm::vec3(std::bit_cast<float>(0xBF062980u), std::bit_cast<float>(0x3DB529B4u), std::bit_cast<float>(0xBE852A79u));
	rotation[1] =
	    glm::vec3(std::bit_cast<float>(0x3E54D407u), std::bit_cast<float>(0x3E80BCD3u), std::bit_cast<float>(0xBF5E7300u));
	rotation[2] =
	    glm::vec3(std::bit_cast<float>(0xBF79420Cu), std::bit_cast<float>(0x3F2CC8BFu), std::bit_cast<float>(0xBEF66BE7u));
	const glm::vec3 centre(std::bit_cast<float>(0xBFCC08A9u), std::bit_cast<float>(0x403E53DFu),
	                       std::bit_cast<float>(0xBE36B375u));
	ExpectBits(body.ObjectOrigin(rotation, centre), 0xBF3FA136u, 0x4024B14Du, 0x3CCDDC60u);
}

// A saved game's pose: the rows and the centre taken as they are, and the points placed by them as SetUpPos places
// them, so a body put back with the pose another one had is where that one was
TEST(BodyPose, ARestoredPoseIsTheSavedRowsAndCentreWithThePointsPlacedByThem)
{
	const auto saved = MakeBody();
	auto restored = MakeBody();
	glm::mat3 other(1.0f);
	restored.SetUpPos(other, glm::vec3(10.0f, 20.0f, 30.0f));
	restored.RestorePose(saved.Axes(), saved.Centre());
	EXPECT_EQ(restored.Axes(), saved.Axes());
	EXPECT_EQ(restored.Centre(), saved.Centre());
	ASSERT_EQ(restored.Points().size(), saved.Points().size());
	for (size_t i = 0; i < saved.Points().size(); ++i)
	{
		ExpectBits(restored.Points()[i].world, Bits(saved.Points()[i].world.x), Bits(saved.Points()[i].world.y),
		           Bits(saved.Points()[i].world.z));
	}
}
