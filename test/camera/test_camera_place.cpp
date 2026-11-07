/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"

using namespace openblack;

namespace
{
constexpr glm::vec3 k_StartOrigin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_StartFocus {1000.0f, 0.0f, 1060.0f};
// Where the camera is put: looking south-west, a little down
constexpr glm::vec3 k_PlacedOrigin {2430.6f, 68.6f, 2574.4f};
constexpr glm::vec3 k_PlacedFocus {2420.4f, 66.0f, 2546.3f};
} // namespace

// Put there, the world camera's targets are the new place at once
TEST(CameraPlaceAt, TheWorldCamerasTargetsAreThePlace)
{
	DefaultWorldCameraModel model(k_StartOrigin, k_StartFocus);
	model.PlaceAt(k_PlacedOrigin, k_PlacedFocus);
	EXPECT_EQ(model.GetTargetOrigin(), k_PlacedOrigin);
	EXPECT_EQ(model.GetTargetFocus(), k_PlacedFocus);
}

// A model that has no use for being put somewhere ignores it
TEST(CameraPlaceAt, OtherModelsIgnoreIt)
{
	class Still final: public CameraModel
	{
	public:
		std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
		{
			return std::nullopt;
		}
		void HandleActions(std::chrono::microseconds /*dt*/) override {}
		void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
		[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return k_StartOrigin; }
		[[nodiscard]] glm::vec3 GetTargetFocus() const override { return k_StartFocus; }
	};
	Still model;
	model.PlaceAt(k_PlacedOrigin, k_PlacedFocus);
	EXPECT_EQ(model.GetTargetOrigin(), k_StartOrigin);
	EXPECT_EQ(model.GetTargetFocus(), k_StartFocus);
}
