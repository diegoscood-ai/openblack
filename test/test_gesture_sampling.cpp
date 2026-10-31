/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The gestures' land under a pixel: the line from the eye through the pixel's point on the near plane, the interface's
// land along it and the land or a point along it, with a fake screen, picking and land. Over them, the path started
// again from the hand's cursor, and what a recognition does with the path. The locator is only used to inject the fakes

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <chrono>
#include <optional>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandLandPoint.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "Input/GameCursor.h"
#include "Locator.h"
#include "Magic/Gestures/GestureBuffer.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/GestureMatch.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Particles/Utility.h"
#include "Resources/Loaders.h"
#include "Resources/Resources.h"
#include "Windowing/WindowingInterface.h"
#include "support/LandFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace sampling = openblack::magic::gestures::sampling;

namespace
{
constexpr glm::ivec2 k_Screen {800, 600};
constexpr float k_NearClip = 2.5f;

class FakeWindowing final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const final { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const final { return {}; }
	[[nodiscard]] uint32_t GetID() const final { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const final { return size; }
	[[nodiscard]] float GetAspectRatio() const final { return static_cast<float>(size.x) / static_cast<float>(size.y); }
	[[nodiscard]] bool IsMinimised() const final { return false; }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode) final { return *this; }

	glm::ivec2 size {k_Screen};
};

/// The lines it is asked about, and the land it answers with
class FakePicking final: public ecs::systems::PickingSystemInterface
{
public:
	struct Asked
	{
		glm::vec3 from;
		glm::vec3 to;
		glm::vec3 camera;
		bool withSea;
	};
	mutable std::optional<Asked> underPixel;
	mutable std::optional<Asked> landOrSea;
	std::optional<glm::vec3> land;

	[[nodiscard]] std::optional<glm::vec3> LandAlong(glm::vec3, glm::vec3) const final { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const final
	{
		landOrSea = Asked {.from = from, .to = to, .camera = camera, .withSea = true};
		return land;
	}
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint, bool withSea) const final
	{
		underPixel = Asked {.from = camera, .to = nearPoint, .camera = camera, .withSea = withSea};
		return land;
	}
	[[nodiscard]] std::optional<glm::vec3> LandFrom2DPoint(glm::ivec2, bool, float) const final { return std::nullopt; }
	void PickUnderCursor(const Frame&) final {}
	[[nodiscard]] const Pick& GetPick() const final { return _pick; }
	[[nodiscard]] std::optional<screen_pick::MeshHit> FeelModel(entt::entity, glm::vec3, glm::vec3) const final
	{
		return std::nullopt;
	}

private:
	Pick _pick;
};

class GestureSampling: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		_camera->SetProjectionMatrixPerspective(70.0f, static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y), 1.0f,
		                                        1000.0f);
		_camera->SetOrigin({1000.0f, 120.0f, 900.0f});
		_camera->SetFocus({1040.0f, 30.0f, 960.0f});
		_camera->SetPixelNearClip(k_NearClip);
		_windowing = &static_cast<FakeWindowing&>(Locator::windowing::emplace<FakeWindowing>());
		_picking = &static_cast<FakePicking&>(Locator::pickingSystem::emplace<FakePicking>());
		// a slope, so that the height read at the quantised point differs from the one at the point itself
		_land = &static_cast<test::HeightFieldIsland&>(Locator::terrainSystem::emplace<test::HeightFieldIsland>(
		    [](glm::vec2 xz) { return xz.x * 0.25f + xz.y * 0.125f; }));
	}

	[[nodiscard]] glm::vec3 NearPoint(glm::ivec2 pixel) const
	{
		return _camera->PixelNearPoint(_camera->GetEye(), k_Screen, pixel, k_NearClip);
	}

	Camera* _camera {nullptr};
	FakeWindowing* _windowing {nullptr};
	FakePicking* _picking {nullptr};
	test::HeightFieldIsland* _land {nullptr};

private:
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::pickingSystem> _restorePicking;
	test::RestoreService<Locator::terrainSystem> _restoreLand;
};
} // namespace

TEST_F(GestureSampling, TheLineStartsAtTheEyeThroughThePixelOnTheNearPlaneOfTheLastCameraUpdate)
{
	const glm::ivec2 pixel {123, 456};
	const auto line = sampling::LineThroughPixel(pixel);
	ASSERT_TRUE(line.has_value());
	EXPECT_EQ(line->eye, _camera->GetEye());
	EXPECT_EQ(line->nearPoint, NearPoint(pixel));
	// the near plane is the one the camera keeps, not the projection's
	_camera->SetPixelNearClip(0.2f);
	const auto nearer = sampling::LineThroughPixel(pixel);
	ASSERT_TRUE(nearer.has_value());
	EXPECT_EQ(nearer->nearPoint, _camera->PixelNearPoint(_camera->GetEye(), k_Screen, pixel, 0.2f));
}

TEST_F(GestureSampling, ThereIsNoLineWithoutAScreen)
{
	_windowing->size = {0, 600};
	EXPECT_FALSE(sampling::LineThroughPixel({10, 10}).has_value());
	EXPECT_FALSE(sampling::InterfaceLandUnderPixel({10, 10}).has_value());
	Locator::windowing::reset();
	EXPECT_FALSE(sampling::LineThroughPixel({10, 10}).has_value());
}

TEST_F(GestureSampling, TheInterfacesLandIsTheLandUnderThePixelWithTheSea)
{
	const glm::ivec2 pixel {400, 380};
	_picking->land = glm::vec3(1100.0f, 7.0f, 1050.0f);
	const auto land = sampling::InterfaceLandUnderPixel(pixel);
	ASSERT_TRUE(land.has_value());
	EXPECT_EQ(*land, *_picking->land);
	ASSERT_TRUE(_picking->underPixel.has_value());
	EXPECT_EQ(_picking->underPixel->from, _camera->GetEye());
	EXPECT_EQ(_picking->underPixel->to, NearPoint(pixel));
	EXPECT_TRUE(_picking->underPixel->withSea);
	// none where the line finds neither
	_picking->land.reset();
	EXPECT_FALSE(sampling::InterfaceLandUnderPixel(pixel).has_value());
}

TEST_F(GestureSampling, TheLandAlongThePixelTakesTheHeightAtTheQuantisedPoint)
{
	const glm::ivec2 pixel {300, 420};
	_picking->land = glm::vec3(1101.37f, 99.0f, 1052.81f);
	const auto point = sampling::LandOrPointAlongPixel(pixel, 400.0f);
	// the line unmoved, the sea's reach measured from the eye
	ASSERT_TRUE(_picking->landOrSea.has_value());
	EXPECT_EQ(_picking->landOrSea->from, _camera->GetEye());
	EXPECT_EQ(_picking->landOrSea->to, NearPoint(pixel));
	EXPECT_EQ(_picking->landOrSea->camera, _camera->GetEye());
	const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
	const glm::vec2 quantised(lookup(1101.37f), lookup(1052.81f));
	ASSERT_FALSE(_land->asked.empty());
	EXPECT_EQ(_land->asked.back(), quantised);
	EXPECT_EQ(point.x, 1101.37f);
	EXPECT_EQ(point.y, quantised.x * 0.25f + quantised.y * 0.125f);
	EXPECT_EQ(point.z, 1052.81f);
}

TEST_F(GestureSampling, OffTheLandThePointIsTheLengthAlongTheLine)
{
	const glm::ivec2 pixel {700, 40};
	const auto point = sampling::LandOrPointAlongPixel(pixel, 400.0f);
	EXPECT_EQ(point, ecs::hand_land_point::AlongMouseRay(_camera->GetEye(), NearPoint(pixel), 400.0f));
	EXPECT_NEAR(glm::distance(point, _camera->GetEye()), 400.0f, 1e-2f);
}

TEST_F(GestureSampling, UnderADrawnViewThePointOffTheLandStaysAlongTheMouseRayFromTheCameraPosition)
{
	_camera->SetDrawnView(
	    glm::lookAt(glm::vec3(900.0f, 300.0f, 800.0f), glm::vec3(1000.0f, 0.0f, 1000.0f), glm::vec3(0.0f, 1.0f, 0.0f)));
	const glm::ivec2 pixel {200, 100};
	const auto point = sampling::LandOrPointAlongPixel(pixel, 400.0f);
	const auto ray = _camera->RayFromEye(glm::vec2(pixel) / glm::vec2(k_Screen));
	EXPECT_EQ(point, _camera->GetOrigin() + glm::normalize(ray.direction) * 400.0f);
	// the land is still looked for from the drawn view's eye
	ASSERT_TRUE(_picking->landOrSea.has_value());
	EXPECT_EQ(_picking->landOrSea->from, _camera->GetEye());
}

TEST_F(GestureSampling, StartingACastOrAPowerUpStartsThePathAgainFromTheHandsCursor)
{
	auto& path = magic::gestures::State().system;
	path.AddSample({1.0f, 0.0f, 2.0f}, {10, 20});
	path.AddSample({3.0f, 0.0f, 4.0f}, {30, 40});
	_picking->land = glm::vec3(1100.0f, 7.0f, 1050.0f);
	magic::gestures::SetHandStatus({.mouse = {400, 380}});
	magic::gestures::ReseedBuffer();
	// the path is forgotten, and starts again at the cursor's pixel and the land under it
	ASSERT_EQ(path.Count(), 1);
	EXPECT_EQ(path.At(0).sx, 400.0f);
	EXPECT_EQ(path.At(0).sz, 380.0f);
	EXPECT_EQ(path.At(0).world, *_picking->land);
	// in the pause after a recognition nothing is taken, so the path stays empty
	magic::gestures::State().cooldown = 0.4f;
	magic::gestures::ReseedBuffer();
	EXPECT_EQ(path.Count(), 0);
}

// The gesture path takes the cursor's pixel as the game's cursor has it, in the window's own pixels: nothing rescales
// it, even past the edge of a window smaller than it
TEST_F(GestureSampling, ThePathTakesTheCursorsPixelInTheWindowsOwnPixels)
{
	auto& state = magic::gestures::State();
	state.system.Clear();
	state.cooldown = 0.0f;
	magic::gestures::SetHandStatus({});
	_picking->land = glm::vec3(1100.0f, 7.0f, 1050.0f);
	input::GameCursorRef() = {1700, 1050};
	sampling::Update(0.05f);
	sampling::Update(0.05f);
	ASSERT_GE(state.system.Count(), 1);
	const auto& last = state.system.At(state.system.Count() - 1);
	EXPECT_EQ(last.sx, 1700.0f);
	EXPECT_EQ(last.sz, 1050.0f);
	input::GameCursorRef() = {0, 0};
	state.system.Clear();
}

namespace
{
/// A PathSymbol .cam file: u32 file size, u32 (unused), u32 count, then count points of 6 floats (x, y, z, and 3 unused)
std::vector<uint8_t> EncodeShape(const std::vector<glm::vec3>& points)
{
	std::vector<uint8_t> bytes(12 + points.size() * 24, 0);
	const auto put = [&bytes](size_t offset, auto value) { std::memcpy(bytes.data() + offset, &value, sizeof(value)); };
	put(0, static_cast<uint32_t>(bytes.size()));
	put(8, static_cast<uint32_t>(points.size()));
	for (size_t i = 0; i < points.size(); ++i)
	{
		put(12 + i * 24, points[i].x);
		put(12 + i * 24 + 4, points[i].y);
		put(12 + i * 24 + 8, points[i].z);
	}
	return bytes;
}

/// Three samples on the land, whose first two span the pixels 100..500 both ways, matched as a circle over the whole path
void DrawnCircle(magic::gestures::InterfaceGestures& state)
{
	state.system.Clear();
	state.system.AddSample({1010.0f, 0.0f, 910.0f}, {100, 100});
	state.system.AddSample({1050.0f, 0.0f, 950.0f}, {500, 500});
	state.system.AddSample({1060.0f, 0.0f, 960.0f}, {600, 600});
	uint8_t keys = 0;
	for (int i = 0; i + 1 < state.system.Count(); ++i)
	{
		if ((state.system.At(i).flags & 0xBu) != 0)
		{
			++keys;
		}
	}
	state.result = {.gesture = magic::gestures::k_Circle, .start = 0, .end = keys};
	state.cooldown = 0.0f;
}
} // namespace

TEST_F(GestureSampling, OnlyARecognisedGestureLeavesATrailAndEitherWayThePathIsWipedForAPause)
{
	// the circle's shape in the resource cache: a square from (0, 1) to (1, 0)
	const test::RestoreService<Locator::resources> restoreResources;
	auto& cache = Locator::resources::emplace<resources::Resources>();
	cache.GetGestureShapes().Load(
	    entt::hashed_string("gestures/shape/4").value(), resources::GestureShapeLoader::FromBufferTag {},
	    EncodeShape({{-100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, 100.0f}, {100.0f, 0.0f, -100.0f}, {-100.0f, 0.0f, -100.0f}}));
	auto& state = magic::gestures::State();
	auto& pending = particles::utility::PendingRecognised();
	pending.clear();

	// a scribble, or a cancel: no trail
	DrawnCircle(state);
	magic::gestures::Success(false);
	EXPECT_TRUE(pending.empty());
	EXPECT_EQ(state.system.Count(), 0);
	EXPECT_EQ(state.cooldown, 0.4f);

	// a recognised gesture leaves its trail, in this interface's player's colour
	DrawnCircle(state);
	magic::gestures::Success(true);
	ASSERT_EQ(pending.size(), 1u);
	EXPECT_EQ(pending.front().player, PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(pending.front().fromInterface);
	EXPECT_EQ(pending.front().stroke.Size(), 3u);
	EXPECT_EQ(state.system.Count(), 0);
	EXPECT_EQ(state.cooldown, 0.4f);
}
