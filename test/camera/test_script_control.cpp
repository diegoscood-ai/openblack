/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The script control of the camera and the game speed (ECS/Systems/Implementations/ScriptControlSystem.h) with the
// switches that follow it (Help/ScriptControl.h): START/END_CAMERA_CONTROL and the task-stop callback, over the
// script camera of the test's script state and a camera of the test's own.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Audio/Services/ScriptAudioState.h"
#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Systems/Implementations/ScriptControlSystem.h"
#include "Help/ScriptControl.h"

using namespace openblack;
using namespace openblack::help;
using openblack::ecs::systems::ScriptControlSystem;

namespace
{
/// A script VM: `current` runs now; types[task] is its VMScriptType (1 when it does not exist)
struct TestVm
{
	uint32_t current {0};
	std::vector<uint32_t> types;

	script_control::Vm Make()
	{
		script_control::Vm vm;
		vm.taskNumber = [this]() { return current; };
		vm.currentTaskType = [this]() { return TypeOf(current); };
		vm.taskType = [this](uint32_t task) { return TypeOf(task); };
		return vm;
	}
	[[nodiscard]] uint32_t TypeOf(uint32_t task) const { return task != 0 && task < types.size() ? types[task] : 1; }
};

/// No script camera mode before and after each test
struct ScriptCameraReset
{
	ScriptCameraReset() { script_camera::Reset(); }
	~ScriptCameraReset() { script_camera::Reset(); }
	ScriptCameraReset(const ScriptCameraReset&) = delete;
	ScriptCameraReset& operator=(const ScriptCameraReset&) = delete;
	ScriptCameraReset(ScriptCameraReset&&) = delete;
	ScriptCameraReset& operator=(ScriptCameraReset&&) = delete;
};
} // namespace

TEST(ScriptControlSystem, CameraControl)
{
	const ScriptCameraReset reset;
	audio::ScriptAudioState audio;
	TestVm vm;
	vm.types = {0, 1, 0x10, 0x8};
	Camera world;
	ScriptControlSystem control;
	script_control::CameraControl camera;
	camera.Reset();
	vm.current = 1;
	// START_CAMERA_CONTROL outside the citadel: the script camera mode taken -> owner, highlights and leashes off
	EXPECT_TRUE(script_control::StartCameraControl(camera, control, world, vm.Make(), false));
	EXPECT_EQ(control.GetCameraOwner(), 1u);
	EXPECT_TRUE(script_camera::Active());
	EXPECT_EQ(camera.drawHighlight, 0);
	EXPECT_EQ(camera.drawLeash, 0);
	// not taken (the script mode cannot be left): false, nothing written
	ScriptControlSystem otherControl;
	script_control::CameraControl other;
	EXPECT_FALSE(script_control::StartCameraControl(other, otherControl, world, vm.Make(), false));
	EXPECT_EQ(otherControl.GetCameraOwner(), 0u);
	EXPECT_EQ(other.drawLeash, 1);
	// inside the citadel only TempleHelp / TempleSpecial (0x18), without touching the switches
	EXPECT_FALSE(script_control::StartCameraControl(other, otherControl, world, vm.Make(), true));
	vm.current = 2;
	EXPECT_TRUE(script_control::StartCameraControl(other, otherControl, world, vm.Make(), true));
	EXPECT_EQ(otherControl.GetCameraOwner(), 2u);
	EXPECT_EQ(other.drawLeash, 1);
	EXPECT_EQ(other.drawHighlight, 1);
	// END_CAMERA_CONTROL: only the owner
	EXPECT_FALSE(script_control::EndCameraControl(camera, control, world, audio, vm.Make()));
	EXPECT_EQ(control.GetCameraOwner(), 1u);
	EXPECT_TRUE(script_camera::Active());
	vm.current = 1;
	audio.creatureSound = 0;
	camera.field7C = 5;
	EXPECT_TRUE(script_control::EndCameraControl(camera, control, world, audio, vm.Make()));
	EXPECT_EQ(control.GetCameraOwner(), 0u);
	EXPECT_FALSE(script_camera::Active());
	EXPECT_EQ(camera.drawHighlight, 1);
	EXPECT_EQ(camera.drawLeash, 1);
	EXPECT_EQ(camera.field7C, 0);
	EXPECT_EQ(audio.creatureSound.load(), 1);
	// the task-stop callback gives it back too
	EXPECT_TRUE(script_control::StartCameraControl(camera, control, world, vm.Make(), false));
	EXPECT_FALSE(control.TaskStopped(world, 2).camera);
	EXPECT_EQ(control.GetCameraOwner(), 1u);
	EXPECT_TRUE(control.TaskStopped(world, 1).camera);
	EXPECT_EQ(control.GetCameraOwner(), 0u);
	EXPECT_FALSE(script_camera::Active());
	EXPECT_EQ(script_control::k_ScriptEndFov, 1.2217305f);
}

TEST(ScriptControlSystem, GameSpeed)
{
	ScriptControlSystem control;
	// nobody has it: the first task takes it, another can't, and only the one with it may set the speed
	control.StartGameSpeed(3);
	control.StartGameSpeed(4);
	EXPECT_TRUE(control.MaySetGameSpeed(3));
	EXPECT_FALSE(control.MaySetGameSpeed(4));
	// another task can't give it back; the one with it can, and then nobody has it
	EXPECT_FALSE(control.EndGameSpeed(4));
	EXPECT_TRUE(control.EndGameSpeed(3));
	EXPECT_TRUE(control.MaySetGameSpeed(0));
	// with nobody holding it any task's END_GAME_SPEED sets the speed back
	EXPECT_TRUE(control.EndGameSpeed(5));
	// a stopped task gives it back
	control.StartGameSpeed(6);
	Camera world;
	EXPECT_FALSE(control.TaskStopped(world, 7).gameSpeed);
	EXPECT_TRUE(control.TaskStopped(world, 6).gameSpeed);
	EXPECT_TRUE(control.MaySetGameSpeed(0));
}
