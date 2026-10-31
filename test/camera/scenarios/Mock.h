/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include <3D/LandIslandInterface.h>
#include <3D/LandLine.h>
#include <Camera/Camera.h>
#include <ECS/Systems/PickingSystemInterface.h>
#include <Input/GameActionMapInterface.h>
#include <Locator.h>
#include <Windowing/WindowingInterface.h>
#include <glm/gtx/vec_swizzle.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4716)
#else
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#endif

namespace openblack
{
class Camera;
}

constexpr uint16_t k_Width = 800;
constexpr uint16_t k_Height = 600;
constexpr auto k_ScreenCentreLine = []() -> std::array<glm::u16vec2, 16> {
	std::array<glm::u16vec2, 16> results = {};
	const auto x = static_cast<uint16_t>(k_Width / 2);
	for (size_t i = 0; auto& result : results)
	{
		const auto y = i * static_cast<float>(k_Height) / results.size();
		const auto yFloor = static_cast<uint16_t>(y);
		// constexpr round
		result = {x, (y - yFloor) >= 0.5f ? yFloor + 1 : yFloor};
		++i;
	}
	return results;
}();
constexpr glm::u16vec2 k_MockMousePos = {k_Width - 10, k_Height - 10};

const uint32_t k_StabilizeFrames = 200;
const uint32_t k_InteractionFrames = 100;

class MockWindowingSystem final: public openblack::windowing::WindowingInterface
{
	[[nodiscard]] void* GetHandle() const final { assert(false); }
	[[nodiscard]] NativeHandles GetNativeHandles() const final { assert(false); }
	[[nodiscard]] uint32_t GetID() const final { assert(false); }
	[[nodiscard]] glm::ivec2 GetSize() const final { return {k_Width, k_Height}; }
	[[nodiscard]] float GetAspectRatio() const final { return static_cast<float>(k_Width) / static_cast<float>(k_Height); }
	[[nodiscard]] bool IsMinimised() const final { return false; }
	WindowingInterface& SetDisplayMode(openblack::windowing::DisplayMode) final { return *this; }
};

class MockTerrain final: public openblack::LandIslandInterface
{
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const openblack::lnd::LNDCell& GetCell(const glm::u16vec2&) const final { assert(false); }
	// The land's ray cast reads the corners: level land at 0, as GetHeightAt
	[[nodiscard]] std::array<uint16_t, 4> GetCellCorners(glm::u16vec2) const final { return {0, 0, 0, 0}; }
	void DumpTextures() const final { assert(false); }
	void DumpMaps() const final { assert(false); }
	[[nodiscard]] std::vector<openblack::LandBlock>& GetBlocks() final { assert(false); }
	[[nodiscard]] const std::vector<openblack::LandBlock>& GetBlocks() const final { assert(false); }
	[[nodiscard]] const std::vector<openblack::lnd::LNDCountry>& GetCountries() const final { assert(false); }
	[[nodiscard]] const openblack::graphics::Texture2D& GetAlbedoArray() const final { assert(false); }
	[[nodiscard]] const openblack::graphics::Texture2D& GetBump() const final { assert(false); }
	[[nodiscard]] const openblack::graphics::Texture2D& GetSmallBump() const final { assert(false); }
	[[nodiscard]] const openblack::graphics::Texture2D& GetHeightMap() const final { assert(false); }
	[[nodiscard]] const openblack::graphics::Texture2D& GetCellMap() const final { assert(false); }
	[[nodiscard]] openblack::U16Extent2 GetIndexExtent() const final { assert(false); }
	[[nodiscard]] openblack::Extent2 GetExtent() const final { assert(false); }
	uint8_t GetNoise(glm::u8vec2) final { assert(false); }
};

class MockAction: public openblack::input::GameActionInterface
{
public:
	MockAction() = default;
	virtual ~MockAction() = default;

	[[nodiscard]] bool GetBindable(openblack::input::BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindable(openblack::input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(openblack::input::BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableChanged(openblack::input::UnbindableActionMap) const final { return false; }
	[[nodiscard]] bool GetBindableRepeat(openblack::input::BindableActionMap) const final { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(openblack::input::UnbindableActionMap) const final { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return k_MockMousePos; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] float GetMouseWheelDelta() const override { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void Frame() final {}
	void ProcessEvent(const SDL_Event& event) final {}

	uint32_t frameNumber = 0;
};

// The mock land should work perfectly well and if not, there is something wrong with the physics
class MockPickingSystem: public openblack::ecs::systems::PickingSystemInterface
{
public:
	virtual ~MockPickingSystem() = default;

	/// The recorded land under a pixel, across x and z
	[[nodiscard]] virtual std::optional<glm::vec2> LandAtPixel(glm::u16vec2 screenCoord) const = 0;

	// The land has no cells: no line meets it, only the sea's level. The land under a pixel is the recorded one, at the
	// pixel the line through the near plane's point shows
	[[nodiscard]] std::optional<glm::vec3> LandAlong(glm::vec3 from, glm::vec3 to) const override
	{
		const auto hit = openblack::land_line::LandAlong(from, to, NoCells);
		return hit.has_value() ? std::optional(glm::vec3(hit->x, 0.0f, hit->y)) : std::nullopt;
	}
	// (openblack) The camera's pan stop reads this line: the cell walk over the mock terrain, level land at 0, as the
	// game's picking walks its island
	[[nodiscard]] std::optional<glm::vec3> LandOrSeaAlong(glm::vec3 from, glm::vec3 to, glm::vec3 camera) const override
	{
		const auto& terrain = openblack::Locator::terrainSystem::value();
		const auto hit = openblack::land_line::LandOrSeaAlong(from, to, camera, openblack::land_line::CellsOf(terrain),
		                                                      terrain.GetCellsPerSide());
		return hit.has_value() ? std::optional(glm::vec3(hit->x, terrain.GetHeightAt(*hit), hit->y)) : std::nullopt;
	}
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 camera, glm::vec3 nearPoint,
	                                                      [[maybe_unused]] bool withSea) const override
	{
		const auto& terrain = openblack::Locator::terrainSystem::value();
		const auto screenCoord = GetWindowCoordinates(camera + (nearPoint - camera) * 100.0f);
		if (!screenCoord.has_value())
		{
			return std::nullopt;
		}
		const auto hit = LandAtPixel(*screenCoord);
		if (!hit.has_value())
		{
			return std::nullopt;
		}
		return glm::vec3(hit->x, terrain.GetHeightAt(*hit), hit->y);
	}
	// The camera's land under a pixel is the recorded one at that pixel
	[[nodiscard]] std::optional<glm::vec3> LandFrom2DPoint(glm::ivec2 pixel, bool /*shift*/, float /*depth*/) const override
	{
		const auto& terrain = openblack::Locator::terrainSystem::value();
		const auto hit = LandAtPixel(RecordedPixel(pixel));
		if (!hit.has_value())
		{
			return std::nullopt;
		}
		return glm::vec3(hit->x, terrain.GetHeightAt(*hit), hit->y);
	}
	void PickUnderCursor(const Frame& /*unused*/) override {}
	[[nodiscard]] const Pick& GetPick() const override { return _pick; }
	[[nodiscard]] std::optional<openblack::screen_pick::MeshHit> FeelModel(entt::entity /*unused*/, glm::vec3 /*unused*/,
	                                                                       glm::vec3 /*unused*/) const override
	{
		return std::nullopt;
	}

	// The recordings are keyed by the screen pixel the land was asked for. The camera's vertical line asks at the rows
	// h * i / 16 rounded down, which the recordings have rounded to the nearest (k_ScreenCentreLine). The mouse's own
	// pixel is the one recorded, even on one of those rows: the recordings never ask the line while the mouse is on a
	// row it rounds up
	[[nodiscard]] static glm::u16vec2 RecordedPixel(glm::ivec2 pixel)
	{
		if (pixel == glm::ivec2(openblack::Locator::gameActionSystem::value().GetMousePosition()))
		{
			return glm::u16vec2(pixel);
		}
		for (size_t i = 0; i < k_ScreenCentreLine.size(); ++i)
		{
			if (pixel == glm::ivec2(k_Width / 2, k_Height * static_cast<int>(i) / 16))
			{
				return k_ScreenCentreLine.at(i);
			}
		}
		return glm::u16vec2(pixel);
	}

	[[nodiscard]] std::optional<glm::u16vec2> GetWindowCoordinates(const glm::vec3& position) const
	{
		glm::vec3 screenPosition;
		const auto size = openblack::Locator::windowing::value().GetSize();
		const auto viewport = glm::vec4(0, 0, size.x, size.y);
		if (!camera->ProjectWorldToScreen(position, viewport, screenPosition, openblack::Camera::Interpolation::Target))
		{
			return std::nullopt;
		}
		// Move point because of precision loss in project/deproject
		return static_cast<glm::u16vec2>(glm::round(glm::round(glm::xy(screenPosition * 10.0f)) / 10.0f));
	}

	uint16_t frameNumber = 0;
	const openblack::Camera* camera;

private:
	static std::optional<openblack::land_line::CellHeights> NoCells(int32_t /*unused*/, int32_t /*unused*/)
	{
		return std::nullopt;
	}
	Pick _pick;
};

// The raycasts a scenario recorded, read from its raycasts/<name>.json: for a screen coordinate, the first branch that
// lists it gives the hit (or no hit) of each frame. A coordinate or frame the recording does not have is a test error
class RecordedMockPickingSystem final: public MockPickingSystem
{
public:
	struct Branch
	{
		std::vector<glm::u16vec2> coords;
		std::map<uint16_t, std::optional<glm::vec2>> frames;
	};

	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 screenCoord) const override
	{
		for (const auto& branch : branches)
		{
			if (std::ranges::find(branch.coords, screenCoord) == branch.coords.end())
			{
				continue;
			}
			const auto found = branch.frames.find(frameNumber);
			if (found != branch.frames.end())
			{
				return found->second;
			}
			assert(false); // Shouldn't be any unaccounted raycasts
			return std::nullopt;
		}
		assert(false); // Shouldn't be any unaccounted raycasts
		return std::nullopt;
	}

	std::vector<Branch> branches;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
#endif
