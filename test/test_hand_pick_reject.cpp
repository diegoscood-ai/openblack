/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_pick::MissesFromAbove, the hand pick's cheapest reject: every sphere it rejects is one the ray's line misses in
// space by more than the rounding of the pick's own sphere test, so the pick finds the same object with or without it.

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <optional>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "Creature/LeashPull.h"
#include "ECS/HandPickReject.h"

using openblack::ecs::hand_pick::MissesFromAbove;

namespace
{
/// A fixed pseudo-random sequence in [0, 1)
class Sequence
{
public:
	float Next()
	{
		_state = _state * 1664525u + 1013904223u;
		return static_cast<float>(_state >> 8) / 16777216.0f;
	}

private:
	uint32_t _state {12345u};
};

/// The distance in space from the centre to the ray's line, in double precision
double LineDistance(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& centre)
{
	const glm::dvec3 o(origin);
	const glm::dvec3 d = glm::normalize(glm::dvec3(dir));
	const glm::dvec3 v = glm::dvec3(centre) - o;
	return glm::length(v - d * glm::dot(v, d));
}

/// The game's 640 by 480 screen, a near clip of 1, for a camera at `eye` looking at `target` with a 60 degree field of
/// view, the mouse at its top left
openblack::ecs::hand_pick::CollisionScreen ScreenLookingAt(const glm::vec3& eye, const glm::vec3& target)
{
	const auto matrices = openblack::affine::FrameMatrices(eye, target, glm::radians(60.0f), 4.0f / 3.0f);
	const auto cameraToWorld = openblack::affine::Inverse(matrices.worldToCamera);
	return {
	    .worldToClipping = matrices.worldToClipping,
	    .cameraUp = glm::vec3(cameraToWorld.m[3], cameraToWorld.m[4], cameraToWorld.m[5]),
	    .nearClip = 1.0f,
	    .size = glm::ivec2(640, 480),
	    .mouse = glm::ivec2(0),
	};
}

/// Where a sphere's circle lies on the screen: its centre's pixel and depth, and its radius in pixels
struct ScreenCircle
{
	glm::ivec2 pixel;
	int radius;
	float depth;
};

ScreenCircle CircleOf(const openblack::ecs::hand_pick::CollisionScreen& screen, const glm::vec3& centre, float radius)
{
	using openblack::creature_leash::ProjectRopePoint;
	const auto middle = ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, centre);
	const auto above =
	    ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, centre + screen.cameraUp * radius);
	return {.pixel = middle.pixel, .radius = std::abs(middle.pixel.y - above.pixel.y), .depth = middle.depth};
}
} // namespace

TEST(HandPickReject, ARejectedSphereIsMissedInSpace)
{
	Sequence random;
	int rejected = 0;
	int kept = 0;
	for (int i = 0; i < 200000; ++i)
	{
		// a camera up to 1.5 km above and around a 5 km island, looking down at any angle
		const glm::vec3 origin(random.Next() * 5000.0f, 5.0f + random.Next() * 1500.0f, random.Next() * 5000.0f);
		glm::vec3 dir(random.Next() * 2.0f - 1.0f, -random.Next(), random.Next() * 2.0f - 1.0f);
		if (i % 50 == 0)
		{
			dir = glm::vec3(0.0f, -1.0f, 0.0f); // straight down
		}
		dir = glm::normalize(dir);
		const float radius = 0.2f + random.Next() * 40.0f;
		// a centre near the ray half of the time
		glm::vec3 centre(random.Next() * 5000.0f, random.Next() * 200.0f, random.Next() * 5000.0f);
		if (i % 2 == 0)
		{
			const float t = random.Next() * 2000.0f;
			centre =
			    origin + dir * t + glm::vec3(random.Next() - 0.5f, random.Next() - 0.5f, random.Next() - 0.5f) * 4.0f * radius;
		}
		if (MissesFromAbove(glm::vec2(origin.x, origin.z), glm::vec2(dir.x, dir.z), glm::vec2(centre.x, centre.z), radius))
		{
			++rejected;
			ASSERT_GT(LineDistance(origin, dir, centre), static_cast<double>(radius) * 1.05 + 0.5) << i;
			// the pick's own float sphere test agrees
			const glm::vec3 oc = origin - centre;
			const float b = glm::dot(oc, dir);
			const float c = glm::dot(oc, oc) - radius * radius;
			ASSERT_TRUE((c > 0.0f && b > 0.0f) || b * b - c < 0.0f) << i;
		}
		else
		{
			++kept;
		}
	}
	// both outcomes are exercised
	EXPECT_GT(rejected, 1000);
	EXPECT_GT(kept, 1000);
}

TEST(HandPickReject, NearTheLineIsKept)
{
	const glm::vec2 origin(100.0f, 100.0f);
	const glm::vec2 dir(1.0f, 0.0f);
	EXPECT_FALSE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 102.0f), 2.0f));             // on the edge
	EXPECT_FALSE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 103.1f), 2.0f));             // within the margin
	EXPECT_TRUE(MissesFromAbove(origin, dir, glm::vec2(300.0f, 103.3f), 2.0f));              // past 1.1 r + 1 m
	EXPECT_FALSE(MissesFromAbove(origin, glm::vec2(0.0f), glm::vec2(101.0f, 100.0f), 1.0f)); // straight down
	EXPECT_TRUE(MissesFromAbove(origin, glm::vec2(0.0f), glm::vec2(110.0f, 100.0f), 1.0f));
}

// A spell icon's seed graphic sends a sphere for its icon: at the graphic's mesh point, raised by the mesh's box centre
// when it draws a mesh, of radius twice its size
TEST(HandPickReject, ASpellIconsGraphicSendsASphereTwiceItsSize)
{
	using openblack::ecs::hand_pick::SpellGraphicSphere;
	const glm::vec3 meshPoint(3319.5f, 83.0f, 3247.5f);
	const auto withMesh = SpellGraphicSphere(meshPoint, glm::vec3(0.0f, 0.75f, 0.0f), 1.0f);
	EXPECT_EQ(withMesh.centre, glm::vec3(3319.5f, 83.75f, 3247.5f));
	EXPECT_EQ(withMesh.radius, 2.0f);
	const auto noMesh = SpellGraphicSphere(meshPoint, std::nullopt, 1.5f);
	EXPECT_EQ(noMesh.centre, meshPoint);
	EXPECT_EQ(noMesh.radius, 3.0f);
}

// A vortex between the lands sends a sphere for itself at its point, of seven tenths of its radius across the land
TEST(HandPickReject, AVortexSendsASphereOfSevenTenthsOfItsWidth)
{
	using openblack::ecs::hand_pick::VortexSphere;
	const glm::vec3 point(1500.0f, 40.0f, 900.0f);
	const auto sphere = VortexSphere(point, 10.0f);
	EXPECT_EQ(sphere.centre, point);
	EXPECT_EQ(sphere.radius, 0.7f * 10.0f);
}

// A miracle dispenser sends a sphere for itself round the bubble it floats: eight tenths of its height above its place,
// of radius half again its radius across the land
TEST(HandPickReject, AMiracleDispenserSendsAWideSphereRoundItsBubble)
{
	using openblack::ecs::hand_pick::DispenserSphere;
	const auto sphere = DispenserSphere(glm::vec3(100.0f, 5.0f, 200.0f), 10.0f, 4.0f);
	EXPECT_EQ(sphere.centre, glm::vec3(100.0f, 13.0f, 200.0f));
	EXPECT_EQ(sphere.radius, 6.0f);
}

// A click 1.5 m beside an icon's graphic's mesh point, at its depth, where the icon's own triangles are missed: the
// sphere, twice the graphic's size, takes the click on the screen, at the graphic's depth, nearer than an object 2 m
// behind it; 2.5 m beside, it is missed
TEST(HandPickReject, AClickNearAnIconsGraphicTakesTheIcon)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	using openblack::ecs::hand_pick::SpellGraphicSphere;
	auto screen = ScreenLookingAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 10.0f, 100.0f));
	screen.mouse = CircleOf(screen, glm::vec3(0.0f, 10.0f, 21.0f), 1.0f).pixel;
	const auto near = SpellGraphicSphere(glm::vec3(1.5f, 10.0f, 21.0f), std::nullopt, 1.0f);
	const auto hit = InvisibleDrawCollision(screen, near.centre, near.radius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(*hit, 21.0f, 1e-3f);
	EXPECT_LT(*hit, CircleOf(screen, glm::vec3(0.0f, 10.0f, 23.0f), 1.0f).depth);
	const auto far = SpellGraphicSphere(glm::vec3(2.5f, 10.0f, 21.0f), std::nullopt, 1.0f);
	EXPECT_FALSE(InvisibleDrawCollision(screen, far.centre, far.radius).has_value());
}

// An opened reward chest is picked through the town's new miracle spinning above it: the seed graphic's own sphere,
// twice its 0.8 size, sent for the chest; a chest not yet open sends none
TEST(HandPickReject, AnOpenedChestIsPickedByItsTownSeedsSphere)
{
	using openblack::ecs::hand_pick::OpenedChestSeedSphere;
	const glm::vec3 meshPoint(2560.0f, 24.0f, 2580.0f);
	const auto opened = OpenedChestSeedSphere(true, meshPoint, glm::vec3(0.0f, 0.5f, 0.0f), 0.8f);
	ASSERT_TRUE(opened.has_value());
	EXPECT_EQ(opened->centre, glm::vec3(2560.0f, 24.5f, 2580.0f));
	EXPECT_EQ(opened->radius, 1.6f);
	EXPECT_FALSE(OpenedChestSeedSphere(false, meshPoint, glm::vec3(0.0f, 0.5f, 0.0f), 0.8f).has_value());
}

// An object an invisible sphere picks is placed by the hand's own test of its model when it has one (a miracle
// dispenser, a one-shot orb), so that a line passing beside the model keeps the hand on the land behind; a spell icon,
// which the game places by the land under it, and an object without a model are met at the sphere's point
TEST(HandPickReject, ASpherePickedObjectWithAModelIsFeltByItsModel)
{
	using openblack::ecs::hand_pick::SpherePickFeelsOwnModel;
	EXPECT_TRUE(SpherePickFeelsOwnModel(true, false));
	EXPECT_FALSE(SpherePickFeelsOwnModel(true, true));
	EXPECT_FALSE(SpherePickFeelsOwnModel(false, false));
}

// A teleport stone's invisible draw collision is tested on the screen as the game tests it: the stone's centre
// projected to its pixel, the circle's radius how far up the screen the point 3 m above it along the camera's up lies,
// the mouse's pixel strictly inside, and the hit's distance the centre's camera depth. A centre in front of the near
// clip is never hit; one projected off the screen still is
TEST(HandPickReject, AStoneIsHitWhenTheMouseIsInsideItsScreenCircle)
{
	using openblack::creature_leash::ProjectRopePoint;
	using openblack::ecs::hand_pick::CollisionScreen;
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	constexpr float k_StoneRadius = 3.0f;
	const auto matrices = openblack::affine::FrameMatrices(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f, 50.0f, 100.0f),
	                                                       glm::radians(60.0f), 4.0f / 3.0f);
	const auto cameraToWorld = openblack::affine::Inverse(matrices.worldToCamera);
	CollisionScreen screen {
	    .worldToClipping = matrices.worldToClipping,
	    .cameraUp = glm::vec3(cameraToWorld.m[3], cameraToWorld.m[4], cameraToWorld.m[5]),
	    .nearClip = 1.0f,
	    .size = glm::ivec2(640, 480),
	    .mouse = glm::ivec2(0),
	};
	const glm::vec3 stone(0.0f, 50.0f, 30.0f);
	const auto middle = ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, stone);
	const auto above =
	    ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, stone + screen.cameraUp * k_StoneRadius);
	ASSERT_TRUE(middle.projected);
	const int radius = std::abs(middle.pixel.y - above.pixel.y);
	ASSERT_GT(radius, 2);
	// just inside the circle, across and up: a hit at the centre's depth
	screen.mouse = middle.pixel + glm::ivec2(radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, stone, k_StoneRadius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, middle.depth);
	EXPECT_NEAR(*hit, 30.0f, 1e-3f);
	screen.mouse = middle.pixel - glm::ivec2(0, radius - 1);
	EXPECT_TRUE(InvisibleDrawCollision(screen, stone, k_StoneRadius).has_value());
	// on the circle is outside it
	screen.mouse = middle.pixel + glm::ivec2(radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, stone, k_StoneRadius).has_value());
	screen.mouse = middle.pixel + glm::ivec2(0, radius);
	EXPECT_FALSE(InvisibleDrawCollision(screen, stone, k_StoneRadius).has_value());
	// a centre in front of the near clip, whatever the mouse
	screen.mouse = glm::ivec2(320, 240);
	EXPECT_FALSE(InvisibleDrawCollision(screen, glm::vec3(0.0f, 50.0f, 0.5f), k_StoneRadius).has_value());
	// a centre projected left of the screen, its circle reaching the screen's first column
	const glm::vec3 offScreen(-18.0f, 50.0f, 30.0f);
	const auto left = ProjectRopePoint(screen.worldToClipping, screen.nearClip, screen.size, offScreen);
	ASSERT_LT(left.pixel.x, 0);
	screen.mouse = glm::ivec2(0, left.pixel.y);
	EXPECT_TRUE(InvisibleDrawCollision(screen, offScreen, k_StoneRadius).has_value());
}

// A miracle dispenser's sphere round its bubble is tested on the screen: the mouse a pixel inside the circle of its
// radius, half again the dispenser's across the land, takes the dispenser at the bubble's depth; on the circle it is
// missed
TEST(HandPickReject, AMiracleDispenserIsHitInsideItsBubblesScreenCircle)
{
	using openblack::ecs::hand_pick::DispenserSphere;
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	const auto sphere = DispenserSphere(glm::vec3(100.0f, 5.0f, 200.0f), 10.0f, 4.0f);
	auto screen = ScreenLookingAt(glm::vec3(100.0f, 13.0f, 150.0f), glm::vec3(100.0f, 13.0f, 250.0f));
	const auto circle = CircleOf(screen, sphere.centre, sphere.radius);
	ASSERT_GT(circle.radius, 2);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, sphere.centre, sphere.radius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	EXPECT_NEAR(*hit, 50.0f, 1e-3f);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, sphere.centre, sphere.radius).has_value());
}

// A temple's leash post is a sphere of radius 1 at its point tested on the screen: the mouse a pixel inside its circle
// above the post takes it at the post's depth, nearer than an object a metre behind; on the circle it is missed
TEST(HandPickReject, ALeashPostIsHitInsideItsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	constexpr float k_PostRadius = 1.0f;
	const glm::vec3 post(400.0f, 30.0f, 520.0f);
	auto screen = ScreenLookingAt(glm::vec3(400.0f, 34.0f, 500.0f), glm::vec3(400.0f, 30.0f, 560.0f));
	const auto circle = CircleOf(screen, post, k_PostRadius);
	ASSERT_GT(circle.radius, 2);
	screen.mouse = circle.pixel - glm::ivec2(0, circle.radius - 1);
	const auto hit = InvisibleDrawCollision(screen, post, k_PostRadius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	EXPECT_LT(*hit, CircleOf(screen, post + glm::vec3(0.0f, 0.0f, 1.0f), k_PostRadius).depth);
	screen.mouse = circle.pixel - glm::ivec2(0, circle.radius);
	EXPECT_FALSE(InvisibleDrawCollision(screen, post, k_PostRadius).has_value());
}

// A fireball's sphere, at its atom's drawn position with the atom's scale as its radius, is tested on the screen: a
// ball seen from far off has a circle of a few pixels, the mouse inside it takes the ball at its depth, beside it
// misses; a ball behind the camera is never hit
TEST(HandPickReject, AFireballIsHitInsideItsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	constexpr float k_BallScale = 1.5f;
	const glm::vec3 ball(800.0f, 60.0f, 900.0f);
	auto screen = ScreenLookingAt(glm::vec3(800.0f, 80.0f, 700.0f), ball);
	const auto circle = CircleOf(screen, ball, k_BallScale);
	ASSERT_GT(circle.radius, 1);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, ball, k_BallScale);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, ball, k_BallScale).has_value());
	screen.mouse = glm::ivec2(320, 240);
	EXPECT_FALSE(InvisibleDrawCollision(screen, glm::vec3(800.0f, 80.0f, 650.0f), 100.0f).has_value());
}

// A spell icon's seed graphic sends the icon's sphere, at its mesh point raised by its mesh's box centre, of twice its
// size, tested on the screen: the mouse a pixel inside the circle round that centre takes the icon at its depth, on the
// circle it is missed
TEST(HandPickReject, ASpellIconIsHitInsideItsGraphicsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	using openblack::ecs::hand_pick::SpellGraphicSphere;
	const auto sphere = SpellGraphicSphere(glm::vec3(1200.0f, 40.0f, 1300.0f), glm::vec3(0.0f, 1.5f, 0.0f), 1.0f);
	auto screen = ScreenLookingAt(glm::vec3(1200.0f, 60.0f, 1260.0f), glm::vec3(1200.0f, 41.5f, 1300.0f));
	const auto circle = CircleOf(screen, sphere.centre, sphere.radius);
	ASSERT_GT(circle.radius, 2);
	screen.mouse = circle.pixel + glm::ivec2(0, circle.radius - 1);
	const auto hit = InvisibleDrawCollision(screen, sphere.centre, sphere.radius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	screen.mouse = circle.pixel + glm::ivec2(0, circle.radius);
	EXPECT_FALSE(InvisibleDrawCollision(screen, sphere.centre, sphere.radius).has_value());
}

// A one-shot orb's seed graphic sends the orb's sphere, twice its size round the graphic's mesh point, tested on the
// screen: of two orbs on one line from the eye, both under the mouse, the nearer's depth is the smaller, so it takes the
// cursor; a pixel past the far one's circle, beside the line, misses it
TEST(HandPickReject, TheNearerOfTwoOrbsUnderTheMouseHasTheSmallerDepth)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	using openblack::ecs::hand_pick::SpellGraphicSphere;
	const glm::vec3 eye(600.0f, 50.0f, 600.0f);
	const auto nearOrb = SpellGraphicSphere(glm::vec3(600.0f, 45.0f, 640.0f), std::nullopt, 0.8f);
	const auto farOrb = SpellGraphicSphere(glm::vec3(600.0f, 40.0f, 680.0f), std::nullopt, 0.8f);
	auto screen = ScreenLookingAt(eye, farOrb.centre);
	const auto farCircle = CircleOf(screen, farOrb.centre, farOrb.radius);
	ASSERT_GT(farCircle.radius, 2);
	screen.mouse = farCircle.pixel;
	const auto nearHit = InvisibleDrawCollision(screen, nearOrb.centre, nearOrb.radius);
	const auto farHit = InvisibleDrawCollision(screen, farOrb.centre, farOrb.radius);
	ASSERT_TRUE(nearHit.has_value());
	ASSERT_TRUE(farHit.has_value());
	EXPECT_LT(*nearHit, *farHit);
	screen.mouse = farCircle.pixel + glm::ivec2(farCircle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, farOrb.centre, farOrb.radius).has_value());
}

// An opened reward chest is picked through its town seed's sphere tested on the screen: the mouse a pixel inside the
// circle round the seed takes the chest at the seed's depth; on the circle it is missed
TEST(HandPickReject, AnOpenedChestIsHitInsideItsTownSeedsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	using openblack::ecs::hand_pick::OpenedChestSeedSphere;
	const auto sphere = OpenedChestSeedSphere(true, glm::vec3(300.0f, 22.0f, 410.0f), glm::vec3(0.0f, 0.5f, 0.0f), 0.8f);
	ASSERT_TRUE(sphere.has_value());
	auto screen = ScreenLookingAt(glm::vec3(300.0f, 30.0f, 380.0f), sphere->centre);
	const auto circle = CircleOf(screen, sphere->centre, sphere->radius);
	ASSERT_GT(circle.radius, 2);
	screen.mouse = circle.pixel - glm::ivec2(circle.radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, sphere->centre, sphere->radius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	screen.mouse = circle.pixel - glm::ivec2(circle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, sphere->centre, sphere->radius).has_value());
}

// A scroll's click sphere, half a metre wider than its mesh, is tested on the screen: the mouse a pixel inside the
// circle of the wider radius takes the scroll, outside the circle of the mesh's own radius; on the wider circle it is
// missed
TEST(HandPickReject, AScrollIsHitInsideItsClickSpheresScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	constexpr float k_MeshRadius = 2.0f;
	constexpr float k_ClickRadius = k_MeshRadius + 0.5f;
	const glm::vec3 scroll(900.0f, 35.0f, 1100.0f);
	auto screen = ScreenLookingAt(glm::vec3(900.0f, 45.0f, 1070.0f), scroll);
	const auto circle = CircleOf(screen, scroll, k_ClickRadius);
	const auto meshCircle = CircleOf(screen, scroll, k_MeshRadius);
	ASSERT_GT(circle.radius - 1, meshCircle.radius);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, scroll, k_ClickRadius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	EXPECT_FALSE(InvisibleDrawCollision(screen, scroll, k_MeshRadius).has_value());
	screen.mouse = circle.pixel + glm::ivec2(circle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, scroll, k_ClickRadius).has_value());
}

// A vortex's sphere, seven tenths of its radius across the land at its point, is tested on the screen: seen from above,
// the mouse a pixel inside its circle takes the vortex at its point's depth, on the circle it is missed
TEST(HandPickReject, AVortexIsHitInsideItsScreenCircle)
{
	using openblack::ecs::hand_pick::InvisibleDrawCollision;
	using openblack::ecs::hand_pick::VortexSphere;
	const auto sphere = VortexSphere(glm::vec3(1500.0f, 40.0f, 900.0f), 10.0f);
	auto screen = ScreenLookingAt(glm::vec3(1500.0f, 140.0f, 860.0f), sphere.centre);
	const auto circle = CircleOf(screen, sphere.centre, sphere.radius);
	ASSERT_GT(circle.radius, 2);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius - 1, 0);
	const auto hit = InvisibleDrawCollision(screen, sphere.centre, sphere.radius);
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(*hit, circle.depth);
	screen.mouse = circle.pixel + glm::ivec2(circle.radius, 0);
	EXPECT_FALSE(InvisibleDrawCollision(screen, sphere.centre, sphere.radius).has_value());
}

// A shield's dome, physical or spiritual, and a lamp that is only a model with a light are drawn with no collision, so
// the cursor never finds them and the hand reaches what stands inside a shield; a street lantern and every other model
// are found (docs/wiki/magic/shields.md#openblack)
TEST(HandPickReject, TheCursorNeverFindsAShieldsDomeNorALampModel)
{
	using openblack::ecs::hand_pick::NoCollision;
	using openblack::ecs::hand_pick::NoCollisionOf;
	EXPECT_EQ(NoCollisionOf({.shield = true}), NoCollision::Shield);
	EXPECT_EQ(NoCollisionOf({.lampModel = true}), NoCollision::Lamp);
	EXPECT_EQ(NoCollisionOf({.lampModel = true, .streetLantern = true}), NoCollision::None);
	EXPECT_EQ(NoCollisionOf({}), NoCollision::None);
}
