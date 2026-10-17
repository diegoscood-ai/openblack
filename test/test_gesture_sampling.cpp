/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The gestures' land under a pixel: the line from the eye through the pixel's point on the near plane, the interface's
// land along it and the land or a point along it, with a fake screen, picking and land. The locator is only used to
// inject the fakes

#include <chrono>
#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Components/Transform.h"
#include "ECS/HandLandPoint.h"
#include "ECS/Systems/PickingSystemInterface.h"
#include "Locator.h"
#include "Magic/Gestures/GestureInput.h"
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
