/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Camera/CameraHelp.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "Help/InterfaceInteraction.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InterfaceActive.h"
#include "Locator.h"
#include "support/RestoreService.h"

// SET_INTERFACE_INTERACTION's 16 levels against the table read from the original game.

using namespace openblack;
using input::BindableActionMap;
namespace ii = openblack::help::interface_interaction;

namespace
{
struct Expected
{
	int32_t level;
	uint8_t interfaceAfterFrom1; ///< the interface state after the call when it was 0x01 (inactive) before
	int32_t features;
	std::optional<float> reach; ///< the R handed to the hand (HandSystemInterface::SetHandReach); none: untouched
	bool autoPitch;             ///< bit 0x40 after the call
	bool cameraMovesAllowed;
	bool realmZoomsAllowed;
};

const std::array<Expected, 15> k_Levels {{
    {0, 0x00, 0x1BF, 1800.0f, false, true, true},
    {1, 0x07, 0x48, 75.0f, true, true, false},
    {2, 0x07, 0x48, 1800.0f, true, true, false},
    {3, 0x07, 0x02, 1800.0f, false, true, false},
    {4, 0x07, 0x18, 1800.0f, false, true, false},
    {5, 0x07, 0x24, 1800.0f, false, true, false},
    {6, 0x00, 0x02, 1800.0f, false, true, false},
    {7, 0x00, 0x26, 1800.0f, false, true, false},
    {8, 0x07, 0x00, std::nullopt, false, false, false},
    {10, 0x07, 0x4A, 1800.0f, true, true, false},
    {11, 0x07, 0x01, 1800.0f, false, true, false},
    {12, 0x05, 0x00, std::nullopt, false, false, false},
    {13, 0x07, 0x1A, 1800.0f, false, true, false},
    {14, 0x07, 0x1B, 1800.0f, false, true, false},
    {15, 0x07, 0x3F, 1800.0f, false, true, false},
}};

/// An action map that only records the blocked sets it is given
class RecordingActions final: public input::GameActionInterface
{
public:
	std::vector<BindableActionMap> blocked;

	[[nodiscard]] bool GetBindable(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindable(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableChanged(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableChanged(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(input::UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] float GetMouseWheelDelta() const override { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void SetBlockedActions(BindableActionMap actions) override { blocked.push_back(actions); }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}
};

[[nodiscard]] bool HasBit(BindableActionMap set, int32_t bit)
{
	return ((static_cast<uint64_t>(set) >> bit) & 1u) != 0;
}

[[nodiscard]] bool Has(BindableActionMap set, BindableActionMap action)
{
	return (static_cast<uint64_t>(set) & static_cast<uint64_t>(action)) != 0;
}

class InterfaceInteractionTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		_help = &Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>().Get();
		ii::Reset();
		interface_active::SetFlags(0);
	}

	// the level, the camera features and the interface flags a test set do not reach the next test
	void TearDown() override
	{
		ii::Reset();
		interface_active::SetFlags(0);
	}

	/// The camera help Set writes, put in by the test
	camera_help::CameraHelp* _help {nullptr};

private:
	test::RestoreService<Locator::cameraHelpSystem> _restoreHelp;
};
} // namespace

TEST_F(InterfaceInteractionTest, EveryLevelAsTheJumpTable)
{
	for (const auto& e : k_Levels)
	{
		ii::Reset();
		_help->Reset();
		_help->Enable(0x40, 0); // an auto-pitch already on: the -1 mask clears it
		interface_active::SetFlags(0x01);
		ii::Set(e.level); // no hand system here: Set only asks LevelHandReach
		SCOPED_TRACE(e.level);
		EXPECT_EQ(ii::GetLevel(), e.level);
		EXPECT_EQ(interface_active::GetFlags(), e.interfaceAfterFrom1);
		EXPECT_EQ(_help->features, e.features);
		EXPECT_EQ(ii::LevelHandReach(e.level), e.reach);
		EXPECT_EQ(_help->IsFeatureEnabled(camera_help::Feature::AutoPitch), e.autoPitch);
		EXPECT_EQ(ii::CameraMovesAllowed(), e.cameraMovesAllowed);
		EXPECT_EQ(ii::RealmZoomsAllowed(), e.realmZoomsAllowed);
	}
}

TEST_F(InterfaceInteractionTest, AutoPitchParameters)
{
	ii::Set(1);
	EXPECT_FLOAT_EQ(_help->autoPitch, 0.448799f);
	EXPECT_FLOAT_EQ(_help->autoPitchHeight, 15.0f);
	ii::Set(3); // the parameters stay, the bit goes with the -1 mask
	EXPECT_FLOAT_EQ(_help->autoPitch, 0.448799f);
	EXPECT_FALSE(_help->IsFeatureEnabled(camera_help::Feature::AutoPitch));
}

TEST_F(InterfaceInteractionTest, InvalidLevelsOnlyStoreTheLevel)
{
	ii::Set(4);
	for (const int32_t level : {9, 16, -1})
	{
		ii::Set(level);
		EXPECT_EQ(ii::GetLevel(), level);
		EXPECT_EQ(interface_active::GetFlags(), 0x06);
		EXPECT_EQ(_help->features, 0x18);
		EXPECT_TRUE(ii::CameraMovesAllowed());
		EXPECT_FALSE(ii::RealmZoomsAllowed());
	}
}

TEST_F(InterfaceInteractionTest, InvalidLevelsLeaveTheReach)
{
	for (const int32_t level : {9, 16, -1, 0x7FFFFFFF})
	{
		EXPECT_FALSE(ii::LevelHandReach(level).has_value()) << level;
	}
}

TEST_F(InterfaceInteractionTest, ControlMapActionGate)
{
	ii::Set(0);
	EXPECT_TRUE(ii::KeyShortcutsEnabled());
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_FALSE(ii::IsActionBlocked(action));
	}
	ii::Set(13); // realm zooms not allowed: 17..19
	EXPECT_FALSE(ii::KeyShortcutsEnabled());
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_EQ(ii::IsActionBlocked(action), action >= 17 && action <= 19) << action;
	}
	ii::Set(8); // camera moves not allowed: 3..19 but 5
	for (int32_t action = 0; action < 33; ++action)
	{
		EXPECT_EQ(ii::IsActionBlocked(action), action >= 3 && action <= 19 && action != 5) << action;
	}
}

TEST_F(InterfaceInteractionTest, EnableCameraFeaturesMask)
{
	// the free functions write the camera help in the slot
	camera_help::EnableCameraFeatures(0x08, -1);
	EXPECT_EQ(_help->features, 0x08);
	camera_help::SetAutoPitch(1.0f, 2.0f, true);
	EXPECT_EQ(_help->features, 0x48);
	camera_help::SetAutoPitch(1.0f, 2.0f, false);
	EXPECT_EQ(_help->features, 0x08);
	EXPECT_FLOAT_EQ(_help->autoPitchHeight, 2.0f);
}

TEST_F(InterfaceInteractionTest, BlockedActionsAsTheControlMap)
{
	for (const int32_t level : {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 9, 16, -1})
	{
		ii::Set(level);
		const auto blocked = ii::BlockedActions();
		for (int32_t action = 0; action < 33; ++action)
		{
			EXPECT_EQ(HasBit(blocked, action), ii::IsActionBlocked(action)) << level << " " << action;
		}
		EXPECT_EQ(static_cast<uint64_t>(blocked) >> 33, 0u) << level;
	}
}

TEST_F(InterfaceInteractionTest, TheInterfaceLevelsSetTheKeys)
{
	ii::Set(0);
	EXPECT_EQ(ii::BlockedActions(), BindableActionMap::NONE);

	ii::Set(1); // JUST_GRAB: only the places are blocked
	EXPECT_EQ(static_cast<uint64_t>(ii::BlockedActions()), static_cast<uint64_t>(BindableActionMap::ZOOM_TO_TEMPLE) |
	                                                           static_cast<uint64_t>(BindableActionMap::ZOOM_TO_CREATURE) |
	                                                           static_cast<uint64_t>(BindableActionMap::ZOOM_TO_REALM));

	ii::Set(8); // JUST_HAND_MOVE: the camera's keys too, but not talking or the hand
	const auto blocked = ii::BlockedActions();
	EXPECT_TRUE(Has(blocked, BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	EXPECT_TRUE(Has(blocked, BindableActionMap::ZOOM_OUT));
	EXPECT_FALSE(Has(blocked, BindableActionMap::TALK));
	EXPECT_FALSE(Has(blocked, BindableActionMap::MOVE));

	ii::Set(0);
	EXPECT_EQ(ii::BlockedActions(), BindableActionMap::NONE);
}

TEST_F(InterfaceInteractionTest, ActionMapIsToldOnSet)
{
	test::RestoreService<Locator::gameActionSystem> restoreActions;
	auto& actions = static_cast<RecordingActions&>(Locator::gameActionSystem::emplace<RecordingActions>());
	ii::Set(8);
	ASSERT_EQ(actions.blocked.size(), 1u);
	EXPECT_EQ(actions.blocked.back(), ii::BlockedActions());
	EXPECT_TRUE(Has(actions.blocked.back(), BindableActionMap::MOVE_LEFT));
	ii::Set(0);
	ASSERT_EQ(actions.blocked.size(), 2u);
	EXPECT_EQ(actions.blocked.back(), BindableActionMap::NONE);
	ii::Set(13);
	ii::Reset(); // a new land: nothing blocked
	ASSERT_EQ(actions.blocked.size(), 4u);
	EXPECT_EQ(actions.blocked.back(), BindableActionMap::NONE);
}
