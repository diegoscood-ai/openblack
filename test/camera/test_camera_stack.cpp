/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <memory>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraModel.h"
#include "ECS/Systems/Implementations/CameraStack.h"

using namespace openblack;
using openblack::camera_stack::StackedMode;
using openblack::ecs::systems::CameraStack;

namespace
{
/// A camera model that does nothing: only which one comes back is looked at
class StillModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return glm::vec3(0.0f); }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return glm::vec3(0.0f); }
};
} // namespace

// A model kept for a player's mode is given back once, the first kept for that mode first; a mode that keeps none gets
// none
TEST(CameraStack, AKeptModelIsTakenOnceTheFirstKeptFirst)
{
	CameraStack stack;
	auto first = std::make_unique<StillModel>();
	auto second = std::make_unique<StillModel>();
	auto other = std::make_unique<StillModel>();
	const CameraModel* firstModel = first.get();
	const CameraModel* secondModel = second.get();
	const CameraModel* otherModel = other.get();
	stack.Keep(1, std::move(first));
	stack.Keep(2, std::move(other));
	stack.Keep(1, std::move(second));
	EXPECT_EQ(stack.KeptCount(), 3u);
	EXPECT_EQ(stack.TakeKept(3), nullptr);
	EXPECT_EQ(stack.TakeKept(1).get(), firstModel);
	EXPECT_EQ(stack.TakeKept(1).get(), secondModel);
	EXPECT_EQ(stack.TakeKept(1), nullptr);
	EXPECT_EQ(stack.TakeKept(2).get(), otherModel);
	EXPECT_EQ(stack.KeptCount(), 0u);
}

// Every model kept for a mode goes with DropKept; the others stay
TEST(CameraStack, DropKeptTakesEveryModelOfThatModeOnly)
{
	CameraStack stack;
	stack.Keep(1, std::make_unique<StillModel>());
	stack.Keep(2, std::make_unique<StillModel>());
	stack.Keep(1, std::make_unique<StillModel>());
	stack.DropKept(1);
	EXPECT_EQ(stack.KeptCount(), 1u);
	EXPECT_EQ(stack.TakeKept(1), nullptr);
	EXPECT_NE(stack.TakeKept(2), nullptr);
}

// The models no player's mode on the stack keeps any more go; those of the modes still there stay
TEST(CameraStack, DropLostKeepsOnlyTheModelsOfModesStillKeepingOne)
{
	CameraStack stack;
	const auto lower = stack.Modes().PushPlayer(true);
	stack.Keep(lower, std::make_unique<StillModel>());
	const auto upper = stack.Modes().PushPlayer(true);
	stack.Keep(upper, std::make_unique<StillModel>());
	stack.DropLost();
	EXPECT_EQ(stack.KeptCount(), 2u);
	ASSERT_TRUE(stack.Modes().PopPlayersMode().has_value());
	stack.DropLost();
	EXPECT_EQ(stack.KeptCount(), 1u);
	EXPECT_EQ(stack.TakeKept(upper), nullptr);
	EXPECT_NE(stack.TakeKept(lower), nullptr);
}

// The clear empties the stack of modes and drops every kept model
TEST(CameraStack, ClearEmptiesTheModesAndTheKeptModels)
{
	CameraStack stack;
	stack.Modes().Push({.kind = StackedMode::Kind::Script});
	stack.Keep(stack.Modes().PushPlayer(true), std::make_unique<StillModel>());
	stack.Clear();
	EXPECT_TRUE(stack.Modes().Empty());
	EXPECT_FALSE(stack.Modes().PlayersModeGone());
	EXPECT_EQ(stack.KeptCount(), 0u);
}
