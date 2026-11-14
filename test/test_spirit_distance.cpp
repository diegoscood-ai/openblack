/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cases (0222f9f5, test_spirit_distance.cpp) on our help::spirits::AdvisorSpiritController: how far in
// front of the camera the advisors hover, at their files' depths whatever the screen and the field of view, as wide on
// the screen at every resolution, and never so close that the near plane cuts into them, even at the deepest near
// plane the camera uses. The camera is his SpiritView, copied here as a fake: a pure projection from the camera's
// values, given to the controller through its pointFromScreen query. The advisors' numbers below are the ones the
// game's two advisor files hold, written out here as plain values.

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Camera/NearClipping.h"
#include "Help/Spirits.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
/// The camera as the advisors see it
struct SpiritView
{
	glm::vec3 eye {0.0f};
	glm::vec3 right {1.0f, 0.0f, 0.0f};
	glm::vec3 up {0.0f, 1.0f, 0.0f};
	/// Away from the eye
	glm::vec3 forward {0.0f, 0.0f, 1.0f};
	/// How far across and up the view reaches at a depth of 1, each side of the middle
	glm::vec2 halfExtent {1.0f, 0.75f};
	float nearClip {1.0f};
	/// The world to the camera's clipping space, whose w is the depth in front of the camera
	glm::mat4 viewProjection {1.0f};
	Screen screen {};
};

glm::vec2 HalfResolution(const Screen& screen)
{
	return {static_cast<float>(screen.width) * 0.5f, static_cast<float>(screen.height) * 0.5f};
}

/// The world point under a pixel at a depth in front of the camera, at the near plane for a depth of 0
glm::vec3 PointFromScreen(const SpiritView& view, glm::vec2 pixel, float depth)
{
	const glm::vec2 half = HalfResolution(view.screen);
	const float acrossAtNear = ((pixel.x - half.x) * (view.halfExtent.x * view.nearClip)) / half.x;
	const float upAtNear = ((half.y - pixel.y) * (view.halfExtent.y * view.nearClip)) / half.y;
	float across = acrossAtNear;
	float up = upAtNear;
	float ahead = view.nearClip;
	if (depth != 0.0f)
	{
		across = (depth / view.nearClip) * acrossAtNear;
		up = (depth / view.nearClip) * upAtNear;
		ahead = depth;
	}
	return view.eye + (view.right * across + view.up * up + view.forward * ahead);
}

/// A world point on the screen, nothing when it is closer than the near plane: its pixel (truncated) and its depth
std::optional<ProjectedPoint> ProjectPoint(const SpiritView& view, const glm::vec3& point)
{
	const glm::vec4 clip = view.viewProjection * glm::vec4(point, 1.0f);
	if (clip.w < view.nearClip)
	{
		return std::nullopt;
	}
	const glm::vec2 half = HalfResolution(view.screen);
	const float inverse = 1.0f / clip.w;
	return ProjectedPoint {.x = static_cast<int32_t>((inverse * clip.x + 1.0f) * half.x),
	                       .y = static_cast<int32_t>(half.y - inverse * clip.y * half.y),
	                       .depth = clip.w};
}

/// A world point in pixels. Nothing when it is closer than the near plane; off the screen, nothing unless forced
std::optional<glm::vec2> WorldToPixel(const SpiritView& view, const glm::vec3& point, bool force)
{
	const glm::vec4 clip = view.viewProjection * glm::vec4(point, 1.0f);
	if (clip.w < view.nearClip)
	{
		return std::nullopt;
	}
	const bool offScreen = clip.x > clip.w || clip.x < -clip.w || clip.y > clip.w || clip.y < -clip.w;
	if (offScreen && !force)
	{
		return std::nullopt;
	}
	const glm::vec2 half = HalfResolution(view.screen);
	const float inverse = 1.0f / clip.w;
	if (offScreen)
	{
		const float x = inverse * clip.x;
		const float y = inverse * clip.y;
		return glm::vec2((x + 1.0f) * half.x, (1.0f - y) * half.y);
	}
	// On the screen it is kept on the screen
	const glm::vec2 pixel((inverse * clip.x + 1.0f) * half.x, half.y - inverse * clip.y * half.y);
	return glm::clamp(pixel, glm::vec2(0.0f), glm::vec2(view.screen.width, view.screen.height));
}

/// The good advisor's file: the depth it hovers at, the depth it comes to when it talks (before the 1.2 the game
/// multiplies it by), its scale and its height in its own units
constexpr float k_GoodNear = 8.733334f;
constexpr float k_GoodFar = 6.026667f;
constexpr float k_GoodScale = 0.02552f;
constexpr float k_GoodSize = 63.42295f;
/// The evil advisor's
constexpr float k_EvilNear = 8.733334f;
constexpr float k_EvilFar = 5.64f;
constexpr float k_EvilScale = 0.0268267f;
constexpr float k_EvilSize = 79.15389f;

/// How far towards the camera each advisor's mesh reaches from its root, in its rest pose at its usual size and turned
/// the most its hover turns it (about 0.8 radians at the screen's edges), as measured from the files
constexpr float k_GoodReach = 0.831f;
constexpr float k_EvilReach = 1.067f;

/// The game's horizontal field of view, 70 degrees
constexpr float k_GameFieldOfView = 1.2217305f;

constexpr std::array<Screen, 5> k_Screens {Screen {.width = 640, .height = 480}, Screen {.width = 1024, .height = 768},
                                           Screen {.width = 1280, .height = 720}, Screen {.width = 1920, .height = 1080},
                                           Screen {.width = 3440, .height = 1440}};
constexpr std::array<float, 3> k_FieldsOfView {1.0f, k_GameFieldOfView, 1.6f};

DudeData Dude(float nearDepth, float farDepth, float scale, float size)
{
	DudeData data;
	data.nearDepth = nearDepth;
	data.farDepth = farDepth * 1.2f;
	data.scale = scale;
	data.modelSize = size;
	return data;
}

/// A camera somewhere over the land looking down at it, its field of view across the screen, as the game makes its
/// view for the advisors
SpiritView MakeView(Screen screen, float xFov, float nearClip)
{
	const float aspect = static_cast<float>(screen.width) / static_cast<float>(screen.height);
	const float yFov = std::atan(std::tan(xFov * 0.5f) / aspect) * 2.0f;
	const glm::mat4 projection = glm::perspective(yFov, aspect, nearClip, 65536.0f);
	const glm::vec3 eye(1200.0f, 140.0f, 900.0f);
	const glm::mat4 viewMatrix = glm::lookAt(eye, glm::vec3(1260.0f, 60.0f, 1010.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 toWorld = glm::inverse(viewMatrix);
	SpiritView view;
	view.eye = glm::vec3(toWorld[3]);
	view.right = glm::vec3(toWorld[0]);
	view.up = glm::vec3(toWorld[1]);
	view.forward = glm::vec3(toWorld[2]) * projection[2][3];
	view.halfExtent = {1.0f / projection[0][0], 1.0f / projection[1][1]};
	view.nearClip = nearClip;
	view.viewProjection = projection * viewMatrix;
	view.screen = screen;
	return view;
}

struct Fixture
{
	SpiritView view;
	bool talking {false};
	DudeData good {Dude(k_GoodNear, k_GoodFar, k_GoodScale, k_GoodSize)};
	DudeData evil {Dude(k_EvilNear, k_EvilFar, k_EvilScale, k_EvilSize)};
	AdvisorSpiritController control;

	Fixture(Screen screen, float xFov, float nearClip)
	    : view(MakeView(screen, xFov, nearClip))
	    , control(good, evil, MakeQueries(), screen)
	{
	}

	Queries MakeQueries()
	{
		Queries q;
		q.localRand = [](int32_t) { return 0u; };
		q.localFloatRand = [](float) { return 0.0f; };
		q.random = [](float a, float) { return a; };
		q.nearClip = [this]() { return view.nearClip; };
		q.pointFromScreen = [this](glm::vec2 pixel, float depth) { return PointFromScreen(view, pixel, depth); };
		q.talkedRecently = [this](int) { return talking; };
		return q;
	}

	void Frames(int count)
	{
		FrameInput input;
		input.dt = 0.1f;
		input.frameMs = 100;
		input.screen = view.screen;
		input.mouse = {view.screen.HalfWidth(), view.screen.HalfHeight()};
		for (int i = 0; i < count; ++i)
		{
			input.tickMs += static_cast<uint32_t>(input.frameMs);
			control.Update(input);
		}
	}

	/// How far ahead of the camera a point is
	[[nodiscard]] float Depth(const glm::vec3& point) const { return glm::dot(point - view.eye, view.forward); }
};
} // namespace

TEST(SpiritDistance, HoverAtTheFilesDepthOnTheirPixel)
{
	for (const auto& screen : k_Screens)
	{
		for (const float xFov : k_FieldsOfView)
		{
			Fixture f(screen, xFov, near_clipping::k_Highest);
			f.Frames(1);
			for (int d = 0; d < 2; ++d)
			{
				const AdvisorSpirit& dude = f.control.Dude(d);
				ASSERT_EQ(dude.Closeness(), 0.0f);
				// Where an advisor rests, a third of the way in from its side of the screen
				const float hx = d == 0 ? 0.66f : -0.66f;
				const glm::vec3 point = dude.HoverTo3D(hx, 0.0f, 0.0f, false);
				EXPECT_NEAR(f.Depth(point), k_GoodNear, 1e-3f);
				const auto projected = ProjectPoint(f.view, point);
				ASSERT_TRUE(projected.has_value());
				const auto expectedX = static_cast<int32_t>((hx + 1.0f) * static_cast<float>(screen.HalfWidth()));
				EXPECT_NEAR(projected->x, expectedX, 1);
				EXPECT_NEAR(projected->y, screen.HalfHeight(), 1);
			}
		}
	}
}

TEST(SpiritDistance, AsWideOnEveryScreen)
{
	// The advisors' depth doesn't change with the screen and the field of view runs across it, so an advisor takes
	// the same share of the screen's width at every resolution
	std::vector<float> shares;
	for (const auto& screen : k_Screens)
	{
		Fixture f(screen, k_GameFieldOfView, near_clipping::k_Highest);
		f.Frames(1);
		const AdvisorSpirit& dude = f.control.Dude(1);
		const glm::vec3 middle = dude.HoverTo3D(0.0f, 0.0f, 0.0f, false);
		const float width = k_EvilSize * k_EvilScale * dude.ModelScale();
		const auto edge = WorldToPixel(f.view, middle + f.view.right * width, false);
		ASSERT_TRUE(edge.has_value());
		shares.push_back((edge->x - static_cast<float>(screen.HalfWidth())) / static_cast<float>(screen.width));
	}
	for (const float share : shares)
	{
		EXPECT_NEAR(share, shares.front(), 2.0f / 640.0f);
	}
	// Its height is about an eighth of the screen's width
	EXPECT_NEAR(shares.front(), 0.132f, 0.002f);
}

TEST(SpiritDistance, NeverInsideTheNearPlane)
{
	for (const auto& screen : k_Screens)
	{
		for (const float xFov : k_FieldsOfView)
		{
			Fixture f(screen, xFov, near_clipping::k_Highest);
			f.control.SpiritEject(1, false);
			f.control.SpiritEject(2, false);
			// Talking brings them in to their nearer depth
			f.talking = true;
			f.Frames(30);
			const std::array<float, 2> reaches {k_GoodReach, k_EvilReach};
			const std::array<float, 2> nearer {k_GoodFar * 1.2f, k_EvilFar * 1.2f};
			for (int d = 0; d < 2; ++d)
			{
				const AdvisorSpirit& dude = f.control.Dude(d);
				ASSERT_FLOAT_EQ(dude.Closeness(), 1.0f);
				// The closest it comes: right in front of its partner, pushed the furthest forward it goes
				const glm::vec3 closest = dude.HoverTo3D(0.0f, 0.0f, -1.0f, false);
				EXPECT_NEAR(f.Depth(closest), nearer.at(d) * 0.7f, 1e-3f);
				// Even then all of it stays beyond the deepest near plane
				EXPECT_GT(f.Depth(closest) - reaches.at(d), near_clipping::k_Highest);
			}
		}
	}
}
