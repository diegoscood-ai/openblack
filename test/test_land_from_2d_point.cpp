/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// PickingSystem::LandFrom2DPoint, the island's land under a pixel, against the pieces it is made of: the pixel's point
// on the near plane (Camera::PixelNearPoint), the line moved on along itself (hand_land_point::ShiftedAlongView), the
// land or the near sea along it (land_line::LandOrSeaAlong) and the height at the hand's quantised point, bit for bit.
// A fake camera, screen and island; the locator is only used to inject them

#define LOCATOR_IMPLEMENTATIONS

#include <cstring>

#include <array>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/LandLine.h"
#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/HandLandPoint.h"
#include "ECS/Systems/Implementations/PickingSystem.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::systems::PickingSystem;
namespace hand_land_point = openblack::ecs::hand_land_point;

namespace
{
constexpr glm::ivec2 k_Screen {800, 600};
constexpr float k_NearClip = 2.5f;
/// The land's corners, in height units (20.1 m)
constexpr uint16_t k_Altitude = 30;

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

/// Level land of k_Altitude everywhere, or no land at all (only the sea), with the heights read from a function of the
/// point and every point they were asked for
class LineIsland final: public LandIslandInterface
{
public:
	LineIsland(bool land, std::function<float(glm::vec2)> height)
	    : _land(land)
	    , _height(std::move(height))
	{
	}

	/// The points GetHeightAt was asked for, in order
	mutable std::vector<glm::vec2> asked;

	[[nodiscard]] float GetHeightAt(glm::vec2 xz) const final
	{
		asked.push_back(xz);
		return _height(xz);
	}
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2 xz) const final { return _height(xz); }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final
	{
		static const lnd::LNDCell k_Empty {};
		return k_Empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2&) const final { return _land; }
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2) const final
	{
		return {k_Altitude, k_Altitude, k_Altitude, k_Altitude};
	}
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	bool _land;
	std::function<float(glm::vec2)> _height;
	std::vector<lnd::LNDCountry> _countries;
};

/// A slope, so that the height read at the quantised point differs from the one at the point itself
float Slope(glm::vec2 xz)
{
	return xz.x * 0.25f + xz.y * 0.125f;
}

float Quantised(float metres)
{
	return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(metres));
}

/// The same float bits, or both none
bool SameBits(const std::optional<glm::vec3>& a, const std::optional<glm::vec3>& b)
{
	if (a.has_value() != b.has_value())
	{
		return false;
	}
	return !a.has_value() || std::memcmp(&*a, &*b, sizeof(glm::vec3)) == 0;
}

class LandFrom2DPoint: public ::testing::Test
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
		UseIsland(true);
	}

	void UseIsland(bool land) { _land = &static_cast<LineIsland&>(Locator::terrainSystem::emplace<LineIsland>(land, Slope)); }

	/// The point the pieces give: the line from the eye through the pixel's near point, moved on or not, the land or the
	/// near sea along it, and the height at the quantised point
	[[nodiscard]] std::optional<glm::vec3> FromPieces(glm::ivec2 pixel, bool shift, float depth) const
	{
		const auto eye = _camera->GetEye();
		const auto nearPoint = _camera->PixelNearPoint(eye, k_Screen, pixel, k_NearClip);
		const auto line = shift ? hand_land_point::ShiftedAlongView(eye, nearPoint, depth, k_NearClip)
		                        : hand_land_point::Line {.from = eye, .to = nearPoint};
		const auto hit =
		    land_line::LandOrSeaAlong(line.from, line.to, eye, land_line::CellsOf(*_land), _land->GetCellsPerSide());
		if (!hit.has_value())
		{
			return std::nullopt;
		}
		return glm::vec3(hit->x, Slope({Quantised(hit->x), Quantised(hit->y)}), hit->y);
	}

	PickingSystem _picking;
	Camera* _camera {nullptr};
	FakeWindowing* _windowing {nullptr};
	LineIsland* _land {nullptr};

private:
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::windowing> _restoreWindowing;
	test::RestoreService<Locator::terrainSystem> _restoreLand;
};

constexpr std::array<glm::ivec2, 6> k_Pixels {{{400, 300}, {123, 456}, {0, 0}, {799, 599}, {650, 520}, {37, 590}}};
} // namespace

TEST_F(LandFrom2DPoint, TheHandsLineIsMovedOnByTheNearClipAndOneMore)
{
	for (const auto pixel : k_Pixels)
	{
		const auto land = _picking.LandFrom2DPoint(pixel, true, k_NearClip + 1.0f);
		ASSERT_TRUE(land.has_value()) << pixel.x << "," << pixel.y;
		EXPECT_TRUE(SameBits(land, FromPieces(pixel, true, k_NearClip + 1.0f))) << pixel.x << "," << pixel.y;
		// where PickingSystem::LandOrSeaAlong finds it along the same line
		const auto eye = _camera->GetEye();
		const auto line = hand_land_point::ShiftedAlongView(eye, _camera->PixelNearPoint(eye, k_Screen, pixel, k_NearClip),
		                                                    k_NearClip + 1.0f, k_NearClip);
		const auto along = _picking.LandOrSeaAlong(line.from, line.to, eye);
		ASSERT_TRUE(along.has_value());
		EXPECT_EQ(land->x, along->x);
		EXPECT_EQ(land->z, along->z);
	}
}

TEST_F(LandFrom2DPoint, ADepthNotAboveZeroMovesTheLineOnByTheNearClip)
{
	for (const auto pixel : k_Pixels)
	{
		const auto atNear = FromPieces(pixel, true, k_NearClip);
		EXPECT_TRUE(SameBits(_picking.LandFrom2DPoint(pixel, true, 0.0f), atNear));
		EXPECT_TRUE(SameBits(_picking.LandFrom2DPoint(pixel, true, -3.0f), atNear));
		EXPECT_TRUE(SameBits(_picking.LandFrom2DPoint(pixel, true, std::numeric_limits<float>::quiet_NaN()), atNear));
	}
}

TEST_F(LandFrom2DPoint, WithoutTheShiftTheLineStartsAtTheEyeWhateverTheDepth)
{
	for (const auto pixel : k_Pixels)
	{
		const auto unmoved = FromPieces(pixel, false, 0.0f);
		ASSERT_TRUE(unmoved.has_value());
		EXPECT_TRUE(SameBits(_picking.LandFrom2DPoint(pixel, false, 0.0f), unmoved));
		EXPECT_TRUE(SameBits(_picking.LandFrom2DPoint(pixel, false, 50.0f), unmoved));
	}
}

TEST_F(LandFrom2DPoint, TheShiftPassesLandJustUnderTheEye)
{
	// half a metre over the land, looking steeply down: moved on by 10 the line starts under the land, so it finds the
	// sea's level beyond
	_camera->SetOrigin({1000.0f, 20.6f, 900.0f});
	_camera->SetFocus({1004.0f, 0.0f, 902.0f});
	const glm::ivec2 centre {400, 300};
	const auto unmoved = _picking.LandFrom2DPoint(centre, false, 0.0f);
	const auto moved = _picking.LandFrom2DPoint(centre, true, 10.0f);
	ASSERT_TRUE(unmoved.has_value());
	ASSERT_TRUE(moved.has_value());
	EXPECT_TRUE(SameBits(unmoved, FromPieces(centre, false, 0.0f)));
	EXPECT_TRUE(SameBits(moved, FromPieces(centre, true, 10.0f)));
	EXPECT_GT(glm::distance(glm::vec2(unmoved->x, unmoved->z), glm::vec2(moved->x, moved->z)), 1.0f);
}

TEST_F(LandFrom2DPoint, OverTheSeaOnlyTheSeasLevelWithin7500OfTheEye)
{
	UseIsland(false);
	// nearly level, looking along x: the screen's middle meets the sea's level 25 km away, the bottom row near
	_camera->SetOrigin({1000.0f, 50.0f, 900.0f});
	_camera->SetFocus({6000.0f, 40.0f, 900.0f});
	EXPECT_FALSE(_picking.LandFrom2DPoint({400, 300}, true, 0.0f).has_value());
	// the top row looks up: no sea either
	EXPECT_FALSE(_picking.LandFrom2DPoint({400, 0}, true, 0.0f).has_value());
	const glm::ivec2 bottom {400, 599};
	const auto sea = _picking.LandFrom2DPoint(bottom, true, 0.0f);
	ASSERT_TRUE(sea.has_value());
	EXPECT_TRUE(SameBits(sea, FromPieces(bottom, true, 0.0f)));
	// where the moved line crosses the sea's level
	const auto eye = _camera->GetEye();
	const auto line = hand_land_point::ShiftedAlongView(eye, _camera->PixelNearPoint(eye, k_Screen, bottom, k_NearClip),
	                                                    k_NearClip, k_NearClip);
	const auto crossing = land_line::SeaLevelAlong(line.from, line.to, eye);
	ASSERT_TRUE(crossing.has_value());
	EXPECT_TRUE(crossing->inReach);
	EXPECT_EQ(sea->x, crossing->point.x);
	EXPECT_EQ(sea->z, crossing->point.y);
}

TEST_F(LandFrom2DPoint, TheHeightIsReadAtTheHandsQuantisedPoint)
{
	const glm::ivec2 pixel {123, 456};
	const auto land = _picking.LandFrom2DPoint(pixel, true, k_NearClip + 1.0f);
	ASSERT_TRUE(land.has_value());
	ASSERT_FALSE(_land->asked.empty());
	const glm::vec2 quantised(Quantised(land->x), Quantised(land->z));
	EXPECT_EQ(_land->asked.back(), quantised);
	EXPECT_EQ(land->y, Slope(quantised));
}

TEST_F(LandFrom2DPoint, NoLandWithoutACameraOrAScreen)
{
	_windowing->size = {0, 600};
	EXPECT_FALSE(_picking.LandFrom2DPoint({400, 300}, true, 0.0f).has_value());
	Locator::windowing::reset();
	EXPECT_FALSE(_picking.LandFrom2DPoint({400, 300}, true, 0.0f).has_value());
	Locator::windowing::emplace<FakeWindowing>();
	Locator::camera::reset();
	EXPECT_FALSE(_picking.LandFrom2DPoint({400, 300}, true, 0.0f).has_value());
}
