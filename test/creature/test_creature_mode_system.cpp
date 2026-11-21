/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Creature Mode's system on fakes: C and the double click lock onto a creature, the camera is taken from the player's
// model and handed back as it was, the cursor keys and a fresh grip put a new player's camera over it, the creature
// going, a script's camera and a script's cinema bars end it, and a script's interface level holds C back. Its input
// only comes from real keys and mouse, which no recorded run presses, so these tests stand in for them.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <entt/resource/resource.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/CameraPath.h"
#include "3D/OrientedText.h"
#include "3D/TempleDoors.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraChecks.h"
#include "Camera/CameraModeStack.h"
#include "Camera/CameraZones.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureFollow.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Camera/DefaultWorldCameraModel.h"
#include "Camera/ScriptCamera.h"
#include "Camera/WorldCamera.h"
#include "Common/EventManager.h"
#include "Creature/CreatureMode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/Events/CameraEvents.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraZoneSystemInterface.h"
#include "ECS/Systems/Implementations/CameraHelpSystem.h"
#include "ECS/Systems/Implementations/CameraPathSystem.h"
#include "ECS/Systems/Implementations/CreatureModeSystem.h"
#include "ECS/Systems/Implementations/InputState.h"
#include "ECS/Systems/Implementations/ScriptState.h"
#include "GameClock.h"
#include "Help/InterfaceInteraction.h"
#include "Input/GameActionMapInterface.h"
#include "Input/InterfaceActive.h"
#include "Input/MouseButtons.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using input::BindableActionMap;
using input::UnbindableActionMap;
using openblack::ecs::systems::CreatureModeSystem;
using Frame = openblack::ecs::systems::CreatureModeSystemInterface::Frame;

namespace
{
constexpr glm::vec3 k_CreatureAt {1000.0f, 0.0f, 1000.0f};
constexpr glm::vec3 k_OtherAt {1200.0f, 0.0f, 900.0f};
constexpr auto k_Step = 16ms;
constexpr ecs::systems::CameraPathSystemInterface::PathOwner k_PathOwner = 7;

/// A placed path's .cam data: a second over 11 points along x, looking ahead of itself
entt::resource<CameraPath> StraightPath()
{
	constexpr uint32_t k_Points = 11;
	std::vector<uint8_t> data(3 * sizeof(uint32_t) + k_Points * 6 * sizeof(float));
	const std::array<uint32_t, 3> header {static_cast<uint32_t>(data.size()), 1000, k_Points};
	std::memcpy(data.data(), header.data(), sizeof(header));
	for (uint32_t i = 0; i < k_Points; ++i)
	{
		const auto x = static_cast<float>(i) * 10.0f;
		const std::array<float, 6> point {x, 5.0f, 0.0f, x + 1.0f, 5.0f, 0.0f};
		std::memcpy(&data[sizeof(header) + i * sizeof(point)], point.data(), sizeof(point));
	}
	auto path = std::make_shared<CameraPath>("test");
	EXPECT_TRUE(path->LoadFromBuffer(data));
	return entt::resource<CameraPath>(path);
}

/// The keys and buttons held this frame, and those of them pressed this frame
class FakeActions final: public input::GameActionInterface
{
public:
	void Hold(std::initializer_list<BindableActionMap> held)
	{
		_held = 0;
		for (const auto action : held)
		{
			_held |= static_cast<uint64_t>(action);
		}
		_pressed = 0;
		_doubleClick = false;
		_doubleClickPressed = false;
	}
	/// Pressed this frame: held, and changed
	void Press(BindableActionMap action)
	{
		Hold({});
		_held = static_cast<uint64_t>(action);
		_pressed = _held;
	}
	void DoubleClick(bool pressedThisFrame)
	{
		Hold({});
		_doubleClick = true;
		_doubleClickPressed = pressedThisFrame;
	}

	[[nodiscard]] bool GetBindable(BindableActionMap action) const override
	{
		return (_held & ~_blocked & static_cast<uint64_t>(action)) != 0;
	}
	/// The script's interface level's blocked actions read as not held, as the game's action map does
	void SetBlockedActions(BindableActionMap actions) override { _blocked = static_cast<uint64_t>(actions); }
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap action) const override
	{
		return action == UnbindableActionMap::DOUBLE_CLICK && _doubleClick;
	}
	[[nodiscard]] bool GetBindableChanged(BindableActionMap action) const override
	{
		return (_pressed & static_cast<uint64_t>(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap action) const override
	{
		return action == UnbindableActionMap::DOUBLE_CLICK && _doubleClickPressed;
	}
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap) const override { return false; }
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap) const override { return false; }
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return {}; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return {}; }
	[[nodiscard]] float GetMouseWheelDelta() const override { return 0.0f; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override { return {}; }
	void Frame() override {}
	void ProcessEvent(const SDL_Event&) override {}

private:
	uint64_t _held {0};
	uint64_t _blocked {0};
	uint64_t _pressed {0};
	bool _doubleClick {false};
	bool _doubleClickPressed {false};
};

/// A camera with the player's own model, a land with the player's creature and another god's, the keys, and the
/// script's and the interface's state, all put back as they were afterwards. No temple
class CreatureModeSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::scriptState::emplace<ecs::systems::ScriptState>();
		Locator::inputState::emplace<ecs::systems::InputState>();
		Locator::temple::reset();
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		_playerModel = &_camera->GetModel();
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_yours = MakeCreature(registry, PlayerNames::PLAYER_ONE, k_CreatureAt);
		_theirs = MakeCreature(registry, PlayerNames::PLAYER_TWO, k_OtherAt);
	}

	static entt::entity MakeCreature(ecs::Registry& registry, PlayerNames owner, glm::vec3 at)
	{
		const auto creature = registry.Create();
		registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {
		                                                         .owner = owner,
		                                                         .species = CreatureType::Cow,
		                                                         .size = 1.0f,
		                                                     });
		registry.Assign<ecs::components::Transform>(creature, at, glm::mat3(1.0f), glm::vec3(1.0f));
		return creature;
	}

	/// The system with the player's creature found by the lookup
	[[nodiscard]] std::unique_ptr<CreatureModeSystem> WithYourCreature() const
	{
		const auto yours = _yours;
		return std::make_unique<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
			if (player == PlayerNames::PLAYER_ONE)
			{
				return yours;
			}
			return std::nullopt;
		});
	}

	[[nodiscard]] bool PlayerHasTheCamera() const { return &_camera->GetModel() == _playerModel; }
	[[nodiscard]] CreatureCameraModel* CreatureCamera() const
	{
		return dynamic_cast<CreatureCameraModel*>(&_camera->GetModel());
	}

	FakeActions* _actions {nullptr};
	Camera* _camera {nullptr};
	const CameraModel* _playerModel {nullptr};
	entt::entity _yours {entt::null};
	entt::entity _theirs {entt::null};

private:
	test::RestoreService<Locator::scriptState> _restoreScript;
	test::RestoreService<Locator::inputState> _restoreInput;
	test::RestoreService<Locator::temple> _restoreTemple;
	test::RestoreService<Locator::gameActionSystem> _restoreActions;
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
};
} // namespace

TEST_F(CreatureModeSystemTest, WithoutALookupNoPlayerHasACreatureAndCDoesNothing)
{
	// As the game has it until the leash service finds the players' creatures
	CreatureModeSystem mode;
	EXPECT_FALSE(mode.PlayersCreature().has_value());
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	EXPECT_FALSE(mode.IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode.GetView().has_value());
}

TEST_F(CreatureModeSystemTest, CLocksOntoThePlayersCreatureAndLetsGoAgain)
{
	const auto mode = WithYourCreature();
	ASSERT_EQ(mode->PlayersCreature(), _yours);

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_TRUE(mode->GetView().has_value());

	// Still held, it is not pressed again
	_actions->Hold({BindableActionMap::ZOOM_TO_CREATURE});
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

// C letting go of the follow pops it, and the pop restarts the mode under it: the player's camera starts again and flies
// back to its own view. A follow ended any other way (a hand demo taking the camera) gives the model back with no restart
TEST_F(CreatureModeSystemTest, CLettingGoRestartsThePlayersCameraUnderItAndLeaveDoesNot)
{
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	int restarts = 0;
	Locator::events::value().AddHandler(
	    std::function([&restarts](const ecs::events::PlayerCameraRestarted& /*restarted*/) { ++restarts; }));
	const auto mode = WithYourCreature();

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	ASSERT_TRUE(mode->IsActive());
	_actions->Hold({});
	mode->Update(k_Step, {});
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_EQ(restarts, 1);

	ASSERT_TRUE(mode->Enter(_yours));
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_EQ(restarts, 1);
}

TEST_F(CreatureModeSystemTest, EnterStartsFromWhereTheCameraIsHeadingAndLeaveHandsThePlayersModelBack)
{
	const auto mode = WithYourCreature();
	const auto origin = _camera->GetOriginZoomer().GetDestination();
	const auto focus = _camera->GetFocusZoomer().GetDestination();
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_NE(CreatureCamera(), nullptr);
	const auto height = creature_mode::CreatureHeight(1.0f);
	const auto start = creature_follow::Start(origin, focus, k_CreatureAt, height);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().yaw, start.yaw);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().pitch, start.pitch);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().distance, start.distance);

	mode->Leave();
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->GetView().has_value());
}

TEST_F(CreatureModeSystemTest, EntersOnlyOnACreatureAndOnlyFromThePlayersModel)
{
	const auto mode = WithYourCreature();
	auto& registry = Locator::entitiesRegistry::value();
	const auto stone = registry.Create();
	registry.Assign<ecs::components::Transform>(stone, k_CreatureAt, glm::mat3(1.0f), glm::vec3(1.0f));
	EXPECT_FALSE(mode->Enter(stone));
	EXPECT_TRUE(PlayerHasTheCamera());

	// Another model has the camera, as the editor's or the temple's: it is not taken from it
	auto other = std::make_unique<CreatureCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f), k_OtherAt, 15.0f);
	const auto* otherModel = other.get();
	auto player = _camera->SetModel(std::move(other));
	EXPECT_FALSE(mode->Enter(_yours));
	EXPECT_FALSE(mode->IsActive());
	EXPECT_EQ(&_camera->GetModel(), otherModel);
	_camera->SetModel(std::move(player));
}

TEST_F(CreatureModeSystemTest, FollowsTheCreatureAsItMoves)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	const glm::vec3 moved {1010.0f, 4.0f, 1020.0f};
	Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_yours).position = moved;
	mode->Update(k_Step, {});
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_EQ(CreatureCamera()->GetTargetFocus(), creature_follow::Focus(moved, creature_mode::CreatureHeight(1.0f)));
}

TEST_F(CreatureModeSystemTest, InItsPenTheCameraFramesThePenSizesHeight)
{
	const auto mode = WithYourCreature();
	auto& registry = Locator::entitiesRegistry::value();
	registry.Assign<ecs::components::CreatureDrawPose>(_yours, ecs::components::CreatureDrawPose {.size = 0.22f});
	const auto origin = _camera->GetOriginZoomer().GetDestination();
	const auto focus = _camera->GetFocusZoomer().GetDestination();
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_NE(CreatureCamera(), nullptr);
	const auto height = creature_mode::CreatureHeight(0.22f);
	const auto start = creature_follow::Start(origin, focus, k_CreatureAt, height);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().distance, start.distance);
	EXPECT_FLOAT_EQ(CreatureCamera()->GetView().pitch, start.pitch);

	const glm::vec3 moved {1010.0f, 4.0f, 1020.0f};
	registry.Get<ecs::components::Transform>(_yours).position = moved;
	mode->Update(k_Step, {});
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_EQ(CreatureCamera()->GetTargetFocus(), creature_follow::Focus(moved, height));
	EXPECT_NE(CreatureCamera()->GetTargetFocus(), creature_follow::Focus(moved, creature_mode::CreatureHeight(1.0f)));
}

TEST_F(CreatureModeSystemTest, TheCursorKeysAlonePutANewPlayersCameraOverTheFollowWhosePopGivesItBack)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	ASSERT_TRUE(mode.Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	EXPECT_EQ(mode.GetCreatureInView(), _yours);

	// With Shift they turn the camera round the creature, and it stays
	_actions->Hold({BindableActionMap::ROTATE_ON, BindableActionMap::MOVE_LEFT});
	const auto yaw = CreatureCamera()->GetView().yaw;
	_camera->HandleActions(k_Step);
	mode.Update(k_Step, {});
	EXPECT_TRUE(mode.IsActive());
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_NE(CreatureCamera()->GetView().yaw, yaw);

	// Alone they ask for a camera of the player's own: a new player's camera from the camera as it is goes on top, not the
	// one from before the follow, and the follow stays under it on its creature, out of view
	const auto yawUnder = follow->GetView().yaw;
	_actions->Hold({BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(mode.GetCreature(), _yours);
	EXPECT_FALSE(mode.GetView().has_value());
	EXPECT_FALSE(mode.GetCreatureInView().has_value());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_FALSE(follow->WantsToLeave()); // spent
	// Meanwhile the follow under it is not updated
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_FLOAT_EQ(follow->GetView().yaw, yawUnder);
	// Popped (the end of a refused creature hold), the follow has the view again and stays with no keys held
	_actions->Hold({});
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_FLOAT_EQ(follow->GetSecondsInMode(), 0.0f);
	_camera->HandleActions(k_Step);
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_EQ(mode.GetCreatureInView(), _yours);
	// and leaving it gives back the player's camera from before the follow
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// The follow reads the land grip at every update, from the interface's hand state (docs/wiki/camera/game-camera.md#openblack):
// a grip held from before the follow had the view puts a new player's camera over it at once, and one still held when that
// camera's pop gives the follow the view back does so again. Gripping with the action button too (Zoom Landscape) does not
TEST_F(CreatureModeSystemTest, AGripOfTheLandHeldAcrossFramesPutsANewPlayersCameraOverTheFollowAtEachOfItsUpdates)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	constexpr Frame k_Grip {.interfaceHandState = creature_mode::k_GripLandscapeHandState};
	constexpr int32_t k_ZoomLandscapeHandState = 22;
	ASSERT_TRUE(mode.Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);

	mode.Update(k_Step, {.interfaceHandState = k_ZoomLandscapeHandState});
	EXPECT_EQ(CreatureCamera(), follow);
	// the grip, held since before the lock-on: a new player's camera on top, the follow under it on its creature
	mode.Update(k_Step, k_Grip);
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode.IsActive());
	EXPECT_FALSE(mode.GetCreatureInView().has_value());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	// The grip held on is the player's camera's while it is the current mode: no other mode goes on
	mode.Update(k_Step, k_Grip);
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	// popped with the grip still held, the follow has the view again, and its next update puts a new one on again
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	mode.Update(k_Step, k_Grip);
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// The double click that locks onto a creature leaves the follow as it is however long its second press is held: the window
// gives that press as the double click alone, so the hand does not grip the land and its hand state is not Grip Landscape
// (the first press's grip ended at its release). A press of the land after that grips it, and the follow goes under a new
// player's camera
TEST_F(CreatureModeSystemTest, TheDoubleClickThatLocksOnLeavesTheFollowWhileItsSecondPressIsHeld)
{
	const auto mode = WithYourCreature();
	input::MouseButtonsState buttons;
	std::ignore = input::ApplyMouseButton(buttons, {.button = input::MouseButton::Left, .down = true});
	std::ignore = input::ApplyMouseButton(buttons, {.button = input::MouseButton::Left, .down = false});
	std::ignore = input::ApplyMouseButton(buttons, {.button = input::MouseButton::Left, .down = true, .doubleClick = true});
	ASSERT_FALSE(input::HandGripping(buttons));

	constexpr Frame k_NotGripping {.interfaceHandState = 3};
	mode->LockOnNext(_theirs);
	mode->Update(k_Step, k_NotGripping);
	ASSERT_EQ(mode->GetCreature(), _theirs);
	const auto* const follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	for (int frame = 0; frame < 10; ++frame)
	{
		mode->Update(k_Step, k_NotGripping);
	}
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_EQ(mode->GetCreature(), std::optional(_theirs));

	// released, then pressed again: a grip of the land
	std::ignore = input::ApplyMouseButton(buttons, {.button = input::MouseButton::Left, .down = false});
	std::ignore = input::ApplyMouseButton(buttons, {.button = input::MouseButton::Left, .down = true});
	ASSERT_TRUE(input::HandGripping(buttons));
	mode->Update(k_Step, {.interfaceHandState = creature_mode::k_GripLandscapeHandState});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_TRUE(mode->IsActive());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
}

// A load of a game saved in Creature Mode locks onto the creature with the view the save kept, not the one it would start
// from: a grip of the land gives the camera back
TEST_F(CreatureModeSystemTest, ARestoreLocksOntoTheCreatureWithTheSavedView)
{
	const auto mode = WithYourCreature();
	const creature_follow::View saved {.yaw = 2.0f, .pitch = 0.9f, .distance = 222.0f};
	ASSERT_TRUE(mode->Restore(_theirs, saved, {}));
	EXPECT_EQ(mode->GetStyle()->timeFactor, 1.0f);
	EXPECT_EQ(mode->GetCreature(), std::optional(_theirs));
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_EQ(CreatureCamera()->GetView().yaw, 2.0f);
	EXPECT_EQ(CreatureCamera()->GetView().pitch, 0.9f);
	EXPECT_EQ(CreatureCamera()->GetView().distance, 222.0f);
	// a grip of the land puts a new player's camera over the restored follow, which stays under it, out of view
	mode->Update(k_Step, {.interfaceHandState = creature_mode::k_GripLandscapeHandState});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(mode->GetView().has_value());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
}

// A load of a script's follow of a thing that is not a creature, saved in a cut scene, makes the player's follow of it
// with the saved view and style: it looks at the follow's point of the thing, keeps to the way the thing faces, and
// ends when the thing is no longer there
TEST_F(CreatureModeSystemTest, ARestoreFollowsAThingThatIsNotACreature)
{
	const auto mode = WithYourCreature();
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	const glm::vec3 drawn {120.0f, 8.0f, 140.0f};
	bool there = true;
	script_camera::detail::SetThingReaderForTests(
	    [villager, drawn, &there](entt::entity entity) -> std::optional<script_camera::ThingInfo> {
		    if (entity != villager || !there)
		    {
			    return std::nullopt;
		    }
		    return script_camera::ThingInfo {.mapPoint = drawn, .height = 2.0f, .drawnPoint = drawn, .wallHugAngle = 256};
	    });
	const creature_follow::View saved {.yaw = 0.5f, .pitch = 0.4f, .distance = 30.0f};
	const creature_follow::Style style {.timeFactor = 0.2f, .behind = true, .clearView = false};
	const bool restored = mode->Restore(villager, saved, style);
	const auto view = mode->GetView();
	const auto kept = mode->GetStyle();
	const auto* camera = CreatureCamera();
	const auto focus = camera != nullptr ? camera->GetTargetFocus() : glm::vec3(0.0f);
	mode->Update(k_Step, {});
	const bool following = mode->IsActive();
	there = false;
	mode->Update(k_Step, {});
	// a thing gone comes off at the turn's camera checks
	const bool followingUntilTheChecks = mode->IsActive();
	mode->RemoveInvalid();
	script_camera::detail::SetThingReaderForTests(nullptr);

	ASSERT_TRUE(restored);
	ASSERT_TRUE(view.has_value());
	EXPECT_EQ(view->yaw, 0.5f);
	EXPECT_EQ(view->distance, 30.0f);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(kept->timeFactor, 0.2f);
	EXPECT_TRUE(kept->behind);
	EXPECT_EQ(focus, drawn + glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_TRUE(following);
	EXPECT_TRUE(followingUntilTheChecks);
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, TheCreatureGoingEndsItAtTheTurnsCameraChecks)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	Locator::entitiesRegistry::value().Destroy(_yours);
	// Until the turn's camera checks the follow keeps the camera and reads nothing
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode->RemoveInvalid()); // the follow that had the camera went
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	// and nothing is left for a later check
	EXPECT_FALSE(mode->RemoveInvalid());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, TheTurnsCameraChecksLeaveAFollowWhoseCreatureIsThere)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	EXPECT_FALSE(mode->RemoveInvalid());
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AScriptsCameraRefusesIt)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(mode->Enter(_yours));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

// A script's camera mode goes over the follow, which stays on the stack under it with its model and is not updated
// (docs/wiki/camera/script-camera.md#pending); the script's end puts a new player's camera over it, the turn's check
// deletes only the ended script mode, and the pop of the new player's mode (a creature hold's end) gives the follow the
// view again, its time in the mode from 0
TEST_F(CreatureModeSystemTest, AScriptsCameraOverTheFollowLeavesItUnderAndThePopOfThePlayersModeAfterItsEndGivesItBack)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	ASSERT_TRUE(mode.Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	ASSERT_TRUE(script_camera::ScriptModeCurrent());
	// Under the script mode the follow keeps its model and does nothing, its creature moving or not
	const auto focus = follow->GetTargetFocus();
	Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_yours).position = k_OtherAt;
	mode.Update(k_Step, {});
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_EQ(follow->GetTargetFocus(), focus);
	EXPECT_TRUE(script_camera::Modes().FollowOnStack());
	EXPECT_FALSE(script_camera::Modes().FollowCurrent());
	// The script's end: a new player's camera from the camera as it is goes on top, the follow under it
	ASSERT_TRUE(script_camera::End());
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode.IsActive());
	EXPECT_FALSE(mode.GetView().has_value());
	ASSERT_TRUE(script_camera::Modes().PlayerCurrent());
	// The turn's check deletes the ended script mode only, and restarts nothing
	camera_checks::CheckModes();
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(CreatureCamera(), nullptr);
	// Popped, the follow has the view again
	script_camera::Get().modeSeconds = 7.0f;
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_FLOAT_EQ(follow->GetSecondsInMode(), 0.0f);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	mode.Update(k_Step, {});
	EXPECT_NE(follow->GetTargetFocus(), focus); // its update runs again
	// and leaving it gives back the player's camera from before the follow
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(script_camera::Modes().FollowOnStack());
}

// A script's camera mode over the follow, popped (the end of a hold of the creature that had it) leaves the follow the
// current mode again, its model the camera's still and its time in the mode from 0
TEST_F(CreatureModeSystemTest, AScriptModeOverTheFollowPoppedLeavesTheFollowCurrent)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	follow->SetSecondsInMode(3.0f);
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	ASSERT_NE(script_camera::TakeOffScriptMode(_camera->GetOriginZoomer(), _camera->GetFocusZoomer()),
	          camera_stack::ModeStack::Popped::None);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_FLOAT_EQ(follow->GetSecondsInMode(), 0.0f);
	EXPECT_TRUE(mode->GetView().has_value());
}

TEST_F(CreatureModeSystemTest, ADualCameraOverTheFollowLeavesItUnderAndItsReleaseGivesItTheViewAgain)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	// A dual camera goes on top: the follow stays under it, and no other follow starts meanwhile
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(CreatureCamera(), follow);
	// Released, the follow has the view again from the zoomers as the dual camera left them, its mode's time from 0
	ASSERT_TRUE(script_camera::ReleaseDual());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), script_camera::Get().position.GetCurrentValue());
	EXPECT_FLOAT_EQ(follow->GetSecondsInMode(), 0.0f);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
}

// The stack's push order (docs/wiki/camera/game-camera.md#openblack): a current dual camera can be left, so a follow
// started over it goes on top, from the zoomers as the dual camera left them; the dual camera's release then finds no
// current dual camera and pops nothing; C lets go of the follow, and as the dual camera under it is then the current
// mode, not a player's, a new player's mode goes over it from the camera's zoomers as the follow left them, whose pop
// leaves the dual camera current again
TEST_F(CreatureModeSystemTest, AFollowStartedOverADualCameraGoesOnTopAndTheDualsReleasePopsNothing)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	auto& state = script_camera::Get();
	state.position.SetDestinationWithTime({1100.0f, 80.0f, 1000.0f}, 2.0f); // where the dual camera sends the zoomers
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_EQ(_camera->GetOriginZoomer().GetDestination(), glm::vec3(1100.0f, 80.0f, 1000.0f));
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	EXPECT_EQ(CreatureCamera(), follow);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().Size(), 2u); // the dual camera and the new player's mode over it
	EXPECT_EQ(state.position.GetDestination(), _camera->GetOriginZoomer().GetDestination());
	// the player's camera kept for it is the camera's again
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
}

// The frames a dual camera drives the camera, the update does not run, but C is read whatever mode is current: it pushes
// a follow over the dual camera, which then no longer drives. A double click the player's camera took waits for the
// update, as it is that camera's own
TEST_F(CreatureModeSystemTest, COverADrivingDualCameraLocksOnOverIt)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(script_camera::Drives());
	mode->LockOnNext(_theirs);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->ReadCreatureKey();
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	EXPECT_NE(CreatureCamera(), nullptr);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	// the double click is left for the update
	_actions->Hold({});
	mode->Update(k_Step, {});
	EXPECT_EQ(mode->GetCreature(), _theirs);
}

// Under a script's camera mode, which cannot be left, C read on its own makes no follow
TEST_F(CreatureModeSystemTest, COverADrivingScriptModeMakesNoFollow)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->ReadCreatureKey();
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
}

// A script's camera mode is a follow: following the player's creature, C pops it as it lets go of Creature Mode's own,
// and the player's camera under it starts again; the script still thinks it has the camera. Following another thing,
// C refuses as over any script mode still going
TEST_F(CreatureModeSystemTest, CPopsAScriptModeFollowingThePlayersCreatureAndNoOther)
{
	const test::RestoreService<Locator::events> restoreEvents;
	Locator::events::emplace<EventManager>();
	int restarts = 0;
	Locator::events::value().AddHandler(
	    std::function([&restarts](const ecs::events::PlayerCameraRestarted& /*restarted*/) { ++restarts; }));
	const auto mode = WithYourCreature();

	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	script_camera::PositionFollow(_theirs);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->ReadCreatureKey();
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_FALSE(mode->IsActive());
	EXPECT_EQ(restarts, 0);

	script_camera::PositionFollow(_yours);
	_actions->Hold({});
	mode->ReadCreatureKey();
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->ReadCreatureKey();
	EXPECT_FALSE(script_camera::ScriptModeCurrent());
	EXPECT_TRUE(script_camera::Modes().PlayersModeCurrent());
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_EQ(restarts, 1);
}

// C letting go of a follow started over a dual camera that went over another follow: that follow, which has the
// camera's model again, is under the dual camera, so a new player's mode goes over the dual camera and the follow keeps
// its model under it; the new mode's pop leaves the dual camera current, and its release gives the follow the view
TEST_F(CreatureModeSystemTest, CLettingGoOverADualCameraOverAFollowPutsANewPlayersCameraOverThemBoth)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_theirs));
	auto* first = CreatureCamera();
	ASSERT_NE(first, nullptr);
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_NE(CreatureCamera(), first);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _theirs);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_FLOAT_EQ(script_camera::Get().modeSeconds, 0.0f);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), first);
	EXPECT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_TRUE(mode->GetView().has_value());
}

// C letting go of a follow started over a placed path: the path, then the current mode, is not a player's, so a new
// player's mode goes over it, the path staying under it without the camera until that mode is popped
TEST_F(CreatureModeSystemTest, CLettingGoOverAPlacedPathPutsANewPlayersModeOverIt)
{
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	auto& paths = static_cast<ecs::systems::CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<ecs::systems::CameraPathSystem>([] { return true; }));
	ASSERT_TRUE(paths.Begin(k_PathOwner, StraightPath(), glm::mat4(1.0f), 1.0f));
	ASSERT_TRUE(paths.HoldsCamera());
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_FALSE(paths.HoldsCamera());
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_EQ(paths.PlacedCount(), 1u);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(paths.HoldsCamera());
}

// Only a current mode that cannot be left refuses a follow (docs/wiki/camera/game-camera.md#openblack): a script's
// camera mode under a current dual camera does not, so the follow goes over the dual camera, both under it; let go of,
// the dual camera is current again, and its release leaves the script mode current, which refuses the next follow
TEST_F(CreatureModeSystemTest, AFollowStartedOverADualCameraOverAScriptModeGoesOnTop)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(script_camera::Active()); // the script mode is on the stack under it
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Active());
	EXPECT_EQ(script_camera::Modes().Size(), 2u); // the script mode and the dual camera, the follow over them
	// The dual camera's release finds no current dual camera and pops nothing
	EXPECT_FALSE(script_camera::ReleaseDual());
	EXPECT_EQ(CreatureCamera(), follow);
	// Let go of, the follow leaves the dual camera current, and the script mode once that is released
	mode->Leave();
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(script_camera::ReleaseDual());
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_FALSE(mode->Enter(_yours));
	EXPECT_FALSE(mode->IsActive());
}

TEST_F(CreatureModeSystemTest, AScriptsCinemaBarsRefuseItAndLeaveAFollowFollowingWithNoKeys)
{
	const auto mode = WithYourCreature();
	interface_active::SetActive(false);
	EXPECT_FALSE(mode->Enter(_yours));
	EXPECT_TRUE(PlayerHasTheCamera());

	// Under the bars a follow goes on, and the keys that would give the camera back or turn it are not read
	interface_active::SetActive(true);
	ASSERT_TRUE(mode->Enter(_yours));
	interface_active::SetActive(false);
	const auto yaw = CreatureCamera()->GetView().yaw;
	_actions->Hold({BindableActionMap::ROTATE_ON, BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	_actions->Hold({BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_EQ(CreatureCamera()->GetView().yaw, yaw);

	// With the interface on again the keys alone put a new player's camera over the follow, which stays under it
	interface_active::SetActive(true);
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(mode->GetView().has_value());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
}

TEST_F(CreatureModeSystemTest, ADoubleClickTakenOnACreatureLocksOntoItInTheNextUpdate)
{
	const auto mode = WithYourCreature();
	// A double click waiting is not the mode's to take: only the player's camera takes it. Its second press is no grip of
	// the land, so the hand state is not Grip Landscape
	_actions->DoubleClick(true);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());

	// The camera took it on another god's creature: the next update, before the camera moves, locks onto it
	mode->LockOnNext(_theirs);
	EXPECT_FALSE(mode->IsActive());
	mode->Update(k_Step, {});
	EXPECT_EQ(mode->GetCreature(), _theirs);
	const auto* const locked = CreatureCamera();
	EXPECT_NE(locked, nullptr);

	// Once only: the updates after keep the creature's camera it made
	mode->Update(k_Step, {});
	EXPECT_EQ(mode->GetCreature(), _theirs);
	EXPECT_EQ(CreatureCamera(), locked);
}

// Every follow is a mode of its own (docs/wiki/camera/game-camera.md#openblack): C locking onto the player's creature while
// another is followed goes over that follow, which stays under it with its model; C letting go pops it, and as the
// follow under it is then the current mode, puts a new player's camera over that one, whose pop gives it the view again
TEST_F(CreatureModeSystemTest, CLockingOnOverAnotherFollowLeavesItUnderAndCLettingGoPutsANewPlayersCameraOverIt)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	ASSERT_TRUE(mode.Enter(_theirs));
	auto* first = CreatureCamera();
	ASSERT_NE(first, nullptr);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	EXPECT_EQ(mode.GetCreature(), _yours);
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_NE(CreatureCamera(), first);
	EXPECT_EQ(script_camera::Modes().OtherCount(), 2u);

	_actions->Hold({});
	mode.Update(k_Step, {});
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	// The first follow is under a new player's camera, not the one from before the follows
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(mode.GetCreature(), _theirs);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), first);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode.IsActive());
}

// A second follow whose creature goes is taken off at the turn's camera checks: the follow under it has the view again,
// its model the camera's, the mode's seconds running on as the stack's one count
TEST_F(CreatureModeSystemTest, ASecondFollowWhoseCreatureGoesLeavesTheFirstTheView)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_theirs));
	auto* first = CreatureCamera();
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_NE(CreatureCamera(), first);
	Locator::entitiesRegistry::value().Destroy(_yours);
	EXPECT_TRUE(mode->RemoveInvalid());
	EXPECT_EQ(CreatureCamera(), first);
	EXPECT_EQ(mode->GetCreature(), _theirs);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_FALSE(mode->RemoveInvalid());
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// A follow under another deleted from the bottom of a full stack: the newer follow keeps the view, and has the player's
// camera from before the follows under it from then on
TEST_F(CreatureModeSystemTest, AFollowDeletedFromUnderANewerOneLeavesThatOneThePlayersCameraUnderIt)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_theirs));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* second = CreatureCamera();
	const auto lowest = script_camera::Modes().LowestOther();
	ASSERT_TRUE(lowest.has_value());
	mode->DeleteFollow(lowest->follow);
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_EQ(CreatureCamera(), second);
	EXPECT_EQ(mode->GetCreature(), _yours);
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->IsActive());
}

// A follow locked on with the camera's stack full goes through the stack's one push
// (docs/wiki/camera/game-camera.md#openblack): the bottom mode is deleted first whatever it is, through its owner, the
// player's own mode, then the lowest follow by Creature Mode (what was under it handed to the follow over it), then the
// lowest dual camera; the stack never holds more than its 12 places, and letting each follow go gives back the one under
// it, the last the player's camera from before the follows
TEST_F(CreatureModeSystemTest, AFollowLockedOnWithTheStackFullDeletesTheBottomModeThroughItsOwner)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	auto& stack = script_camera::Modes();
	ASSERT_TRUE(mode.Enter(_theirs));
	for (int i = 0; i < 10; ++i)
	{
		stack.Push({.kind = camera_stack::StackedMode::Kind::Dual, .dual = {.point = {static_cast<float>(i), 0.0f, 0.0f}}},
		           stack.OtherCount());
	}
	ASSERT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	ASSERT_TRUE(mode.Enter(_yours));
	EXPECT_TRUE(stack.PlayersModeGone());
	EXPECT_EQ(stack.OtherCount(), 2u);
	auto* second = CreatureCamera();
	ASSERT_NE(second, nullptr);
	// the first follow, pushed before the dual cameras, is the bottom: Creature Mode deletes it
	ASSERT_TRUE(mode.Enter(_theirs));
	EXPECT_EQ(stack.OtherCount(), 2u);
	EXPECT_EQ(stack.TakeOtherSystemsDrops(), 0u);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	auto* third = CreatureCamera();
	ASSERT_NE(third, nullptr);
	EXPECT_NE(third, second);
	EXPECT_EQ(mode.GetCreature(), _theirs);
	// then the lowest dual camera
	ASSERT_TRUE(mode.Enter(_yours));
	ASSERT_EQ(stack.Size(), 9u);
	EXPECT_EQ(stack.Modes().front().dual.point.x, 1.0f);
	EXPECT_EQ(stack.Places() + stack.OtherCount(), camera_stack::ModeStack::k_Places);
	EXPECT_TRUE(stack.FollowCurrent());
	mode.Leave();
	EXPECT_EQ(CreatureCamera(), third);
	mode.Leave();
	EXPECT_EQ(CreatureCamera(), second);
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode.IsActive());
	EXPECT_EQ(stack.OtherCount(), 0u);
}

// A follow popped off a stack it leaves empty, with the player's own mode deleted by a full stack
// (docs/wiki/camera/game-camera.md#openblack): C letting go of it makes a new player's mode, the bottom from then on,
// which starts from the camera as it is with no flight back, its mode's seconds from 0, before any turn's check
TEST_F(CreatureModeSystemTest, CLettingGoOfAFollowThatEmptiesTheStackWithThePlayersModeGoneStartsANewPlayersMode)
{
	const auto mode = WithYourCreature();
	auto& stack = script_camera::Modes();
	// Eleven dual cameras fill the stack over the player's own mode: the follow's push deletes that mode
	for (int i = 0; i < 11; ++i)
	{
		stack.Push({.kind = camera_stack::StackedMode::Kind::Dual, .dual = {.point = {static_cast<float>(i), 0.0f, 0.0f}}},
		           stack.OtherCount());
	}
	ASSERT_TRUE(mode->Enter(_yours));
	ASSERT_TRUE(stack.PlayersModeGone());
	// The dual cameras under the follow go, leaving it alone on the stack
	std::ignore = stack.RemoveInvalid([](const camera_stack::DualMode& /*dual*/) { return false; });
	ASSERT_TRUE(stack.Empty());
	ASSERT_TRUE(stack.FollowCurrent());
	const auto& player = dynamic_cast<const DefaultWorldCameraModel&>(*_playerModel);
	const auto restarts = player.GetRestarts().count;
	script_camera::Get().modeSeconds = 1.0f;

	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(stack.PlayersModeGone());
	EXPECT_EQ(stack.OtherCount(), 0u);
	EXPECT_EQ(player.GetRestarts().count, restarts + 1);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
}

TEST_F(CreatureModeSystemTest, LeavingWhileAnotherHasTheCameraWaitsForItBack)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	// Another model takes the camera from the creature's, as the temple's does
	auto creatureModel = _camera->SetModel(
	    std::make_unique<CreatureCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f), k_OtherAt, 15.0f));
	mode->Leave();
	EXPECT_FALSE(mode->IsActive());
	mode->Update(k_Step, {});
	EXPECT_FALSE(PlayerHasTheCamera());

	// Given back to the creature's model, it goes back to the player's
	_camera->SetModel(std::move(creatureModel));
	mode->Update(k_Step, {});
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, WhileAnotherHasItsCameraADoubleClickLeavesItAsItIs)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	auto* following = CreatureCamera();
	// Another camera takes the view over from the creature's, as the creature interaction's does, and keeps it
	auto creatureModel = _camera->SetModel(
	    std::make_unique<CreatureCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f), k_OtherAt, 15.0f));
	EXPECT_FALSE(mode->Enter(_theirs));
	EXPECT_TRUE(mode->IsActive());

	// Given back, the creature's camera follows its creature again
	_camera->SetModel(std::move(creatureModel));
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(CreatureCamera(), following);
}

TEST_F(CreatureModeSystemTest, WhileTheCreatureInteractionHasTheViewCLeavesItAsItIs)
{
	const auto mode = WithYourCreature();
	ASSERT_TRUE(mode->Enter(_yours));
	auto* following = CreatureCamera();
	// The creature interaction takes the view over from the creature's camera; its camera cannot be left
	auto creatureModel =
	    _camera->SetModel(std::make_unique<CreatureInteractCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f)));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);

	// Given back, the creature's camera follows its creature again
	_camera->SetModel(std::move(creatureModel));
	_actions->Hold({});
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(CreatureCamera(), following);
}

TEST_F(CreatureModeSystemTest, WhileTheCreatureInteractionHasThePlayersViewCDoesNotLockOn)
{
	const auto mode = WithYourCreature();
	auto playerModel =
	    _camera->SetModel(std::make_unique<CreatureInteractCameraModel>(glm::vec3(0.0f, 50.0f, 0.0f), glm::vec3(0.0f)));
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	_camera->SetModel(std::move(playerModel));
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AScriptsInterfaceLevelHoldsCBack)
{
	// The levels write the camera help's features too, so it is put in for them
	const test::RestoreService<Locator::cameraHelpSystem> restoreHelp;
	Locator::cameraHelpSystem::emplace<ecs::systems::CameraHelpSystem>();
	const auto mode = WithYourCreature();

	// Camera moves allowed but no zooms to places (JUST_GRAB_DOUBLE_CLICK_AND_ROTATE): C does not lock on
	help::interface_interaction::Set(13);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(PlayerHasTheCamera());

	// No camera moves at all (JUST_HAND_MOVE): nor does it
	help::interface_interaction::Set(8);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_FALSE(mode->IsActive());

	// The normal level gives it back
	help::interface_interaction::Set(0);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	ASSERT_TRUE(mode->IsActive());

	// Locked on, a level that holds C back keeps the creature's camera: C does not let go either
	help::interface_interaction::Set(13);
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
}

// As the original's follow mode, put on the camera's stack of modes and taken off it by a new player's mode: locking on,
// C letting go, the cursor keys alone and a fresh grip each start the camera mode's seconds again; the creature going
// takes the follow off and the seconds run on
TEST_F(CreatureModeSystemTest, TakingTheCameraAndGivingItBackStartsTheCameraModesSecondsAgain)
{
	const auto mode = WithYourCreature();
	auto& seconds = script_camera::Get().modeSeconds;

	seconds = 7.0f;
	ASSERT_TRUE(mode->Enter(_yours));
	EXPECT_EQ(seconds, 0.0f);

	seconds = 7.0f;
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	ASSERT_FALSE(mode->IsActive());
	EXPECT_EQ(seconds, 0.0f);

	// the keys and the grip put a new player's camera over the follow, which starts the mode's seconds again too; a new
	// follow then locks on over that camera
	ASSERT_TRUE(mode->Enter(_yours));
	seconds = 7.0f;
	_actions->Hold({BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode->Update(k_Step, {});
	ASSERT_FALSE(mode->GetView().has_value());
	EXPECT_EQ(seconds, 0.0f);

	_actions->Hold({});
	ASSERT_TRUE(mode->Enter(_yours));
	mode->Update(k_Step, {});
	seconds = 7.0f;
	mode->Update(k_Step, {.interfaceHandState = creature_mode::k_GripLandscapeHandState});
	ASSERT_FALSE(mode->GetView().has_value());
	EXPECT_EQ(seconds, 0.0f);

	ASSERT_TRUE(mode->Enter(_yours));
	seconds = 7.0f;
	Locator::entitiesRegistry::value().Destroy(_yours);
	mode->Update(k_Step, {});
	// the creature gone, the follow comes off at the turn's camera checks, with the mode's seconds running on
	mode->RemoveInvalid();
	ASSERT_FALSE(mode->IsActive());
	EXPECT_EQ(seconds, 7.0f);
}

namespace
{
/// The zones a test sets: a fence it puts between the follow's place and its creature
class FenceZones final: public ecs::systems::CameraZoneSystemInterface
{
public:
	bool SetZones(std::string_view /*fileName*/) final { return false; }
	[[nodiscard]] const camera_zones::Zones& GetZones() const final { return zones; }
	void ClearMap() final {}
	void ClearForScriptsRestart() final {}
	[[nodiscard]] const std::string& GetFileName() const final { return _fileName; }

	camera_zones::Zones zones;

private:
	std::string _fileName;
};

/// A fence across the land halfway between the follow's place and its focus, the focus inside it: the follow's next
/// update finds its place outside the fence
void FenceOffThePlace(FenceZones& fence, CreatureCameraModel& follow, const Camera& camera)
{
	const auto unfenced = follow.Update(k_Step, camera);
	ASSERT_TRUE(unfenced.has_value());
	ASSERT_NE(unfenced->origin.z, unfenced->focus.z);
	const float middle = (unfenced->origin.z + unfenced->focus.z) * 0.5f;
	const float far = unfenced->focus.z > middle ? 20000.0f : -20000.0f;
	fence.zones.fence = {{-20000.0f, 0.0f, middle}, {20000.0f, 0.0f, middle}, {20000.0f, 0.0f, far}, {-20000.0f, 0.0f, far}};
	fence.zones.fenceOn = true;
	follow.Update(k_Step, camera);
	ASSERT_TRUE(follow.LeftTheZone());
	ASSERT_FALSE(follow.WantsToLeave());
}
} // namespace

TEST_F(CreatureModeSystemTest, AFollowLeavingTheFenceGoesUnderANewPlayersCameraWhosePopGivesItBack)
{
	const test::RestoreService<Locator::cameraZoneSystem> restoreZones;
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	auto& fence = static_cast<FenceZones&>(Locator::cameraZoneSystem::emplace<FenceZones>());
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	ASSERT_TRUE(mode.Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	FenceOffThePlace(fence, *follow, *_camera);
	mode.Update(k_Step, {});
	// A new player's camera has the view, not the one from before the follow; the follow stays under it, on its creature
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode.IsActive());
	EXPECT_EQ(mode.GetCreature(), _yours);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	// Popped (the end of a refused creature hold), the follow has the view again, its time in the mode from 0
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_FLOAT_EQ(follow->GetSecondsInMode(), 0.0f);
	EXPECT_FALSE(follow->LeftTheZone());
	EXPECT_TRUE(script_camera::Modes().Empty());
	// and leaving it gives back the player's camera from before the follow
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, AFollowUnderANewPlayersCameraWhoseCreatureGoesLeavesThePlayersCameraUnderIt)
{
	const test::RestoreService<Locator::cameraZoneSystem> restoreZones;
	auto& fence = static_cast<FenceZones&>(Locator::cameraZoneSystem::emplace<FenceZones>());
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	FenceOffThePlace(fence, *CreatureCamera(), *_camera);
	mode->Update(k_Step, {});
	ASSERT_TRUE(script_camera::Modes().PlayerCurrent());
	const auto over = script_camera::Modes().Current()->id;
	EXPECT_FALSE(script_camera::Modes().KeepsModel(over));
	// The turn's camera checks take the follow off from under the new player's camera quietly: the player's camera from
	// before the follow is under it from then on
	Locator::entitiesRegistry::value().Destroy(_yours);
	EXPECT_FALSE(mode->RemoveInvalid()); // quietly
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(script_camera::Modes().KeepsModel(over));
	EXPECT_FALSE(PlayerHasTheCamera());
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(CreatureModeSystemTest, CWithTheFollowUnderANewPlayersCameraLocksOnAfresh)
{
	const test::RestoreService<Locator::cameraZoneSystem> restoreZones;
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	auto& fence = static_cast<FenceZones&>(Locator::cameraZoneSystem::emplace<FenceZones>());
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	ASSERT_TRUE(mode.Enter(_yours));
	auto* first = CreatureCamera();
	FenceOffThePlace(fence, *first, *_camera);
	mode.Update(k_Step, {});
	const auto over = script_camera::Modes().Current()->id;
	fence.zones.fenceOn = false;
	// The current mode is not a follow, so C is no leave: a new follow starts over the new player's camera, and the one
	// under it stays there with its model (each follow is a mode of its own)
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	EXPECT_TRUE(mode.IsActive());
	ASSERT_NE(CreatureCamera(), nullptr);
	EXPECT_NE(CreatureCamera(), first);
	EXPECT_FALSE(script_camera::Modes().KeepsModel(over));
	EXPECT_EQ(script_camera::Modes().OtherCount(), 2u);
	// C lets go of the new one: the new player's camera is current again, and its pop gives the first follow the view
	_actions->Hold({});
	mode.Update(k_Step, {});
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	ASSERT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().Current()->id, over);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), first);
	mode.Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// The follow's leave by the keys goes over the whole stack: a dual camera under the follow stays under it, not current
// again while the new player's mode and the follow are over it
TEST_F(CreatureModeSystemTest, TheCursorKeysOverAFollowOverADualCameraLeaveTheDualCameraUnder)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto& mode =
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    });
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	ASSERT_TRUE(mode.Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	_actions->Hold({BindableActionMap::MOVE_LEFT});
	_camera->HandleActions(k_Step);
	mode.Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_TRUE(mode.IsActive());
	// the new player's mode popped: the follow has the view again, the dual camera still under it
	_actions->Hold({});
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_FALSE(script_camera::DualCurrent());
}

// A follow a grip went over stays while its creature is there; once it goes, the turn's camera checks take the follow
// off quietly from under the new player's camera, which keeps the player's camera from before the follow under it
TEST_F(CreatureModeSystemTest, AFollowAGripWentOverIsTakenOffByTheTurnsChecksOnceItsCreatureGoes)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	mode->Update(k_Step, {});
	mode->Update(k_Step, {.interfaceHandState = creature_mode::k_GripLandscapeHandState});
	ASSERT_TRUE(script_camera::Modes().PlayerCurrent());
	const auto over = script_camera::Modes().Current()->id;
	EXPECT_FALSE(mode->RemoveInvalid());
	EXPECT_TRUE(mode->IsActive());
	Locator::entitiesRegistry::value().Destroy(_yours);
	EXPECT_FALSE(mode->RemoveInvalid()); // quietly
	EXPECT_FALSE(mode->IsActive());
	EXPECT_TRUE(script_camera::Modes().KeepsModel(over));
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(PlayerHasTheCamera());
}

// A place key (the temple's or the realm's) flies only a player's mode: over any other current mode it makes a new one
// first (camera_checks::PutNewPlayersModeOver, as the keys call it). Over the follow that is the current mode, a new
// player's camera from the camera as it is goes on top, and the follow stays under it on its creature until that mode
// is popped
TEST_F(CreatureModeSystemTest, APlaceKeyPutsANewPlayersCameraOverTheFollowWhosePopGivesItBack)
{
	// the camera's pop gives the follow back through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	EXPECT_FALSE(mode->GetView().has_value());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_EQ(script_camera::Get().modeSeconds, 0.0f);
	// A second press flies the player's camera on top: no other mode is made
	EXPECT_FALSE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// Over two follows, every follow stays under the new player's camera, not only the older one
TEST_F(CreatureModeSystemTest, APlaceKeyOverTwoFollowsLeavesBothUnderTheNewPlayersCamera)
{
	// the camera's pop gives the follow back through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_theirs));
	auto* first = CreatureCamera();
	ASSERT_TRUE(mode->Enter(_yours));
	auto* second = CreatureCamera();
	ASSERT_NE(second, first);
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 2u);
	EXPECT_EQ(mode->GetCreature(), _yours);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), second);
	mode->Leave();
	EXPECT_EQ(CreatureCamera(), first);
	EXPECT_EQ(mode->GetCreature(), _theirs);
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->IsActive());
}

// With the player's mode current, a place key makes no mode: it flies that one
TEST_F(CreatureModeSystemTest, APlaceKeyWithNoFollowMakesNoMode)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	EXPECT_FALSE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_TRUE(script_camera::Modes().Empty());
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
}

// A place key while a dual camera pushed over the follow is the current mode: the new player's camera goes over the dual
// camera, from the zoomers as it had them, and the follow keeps its model under them both, not taken off. The new mode's
// pop leaves the dual camera current, the follow's model the camera's again
TEST_F(CreatureModeSystemTest, APlaceKeyOverADualCameraOverTheFollowPutsANewPlayersCameraOverThemBoth)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	auto& state = script_camera::Get();
	state.position.SetPosition({900.0f, 60.0f, 900.0f}); // where the dual camera has the zoomers
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), glm::vec3(900.0f, 60.0f, 900.0f));
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_FALSE(camera_checks::PutNewPlayersModeOver(mode));
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_TRUE(script_camera::DualCurrent());
}

// With no follow, a place key over a current dual camera puts a new player's mode over it from the zoomers as the dual
// camera had them, the player's camera starting again there; its pop leaves the dual camera current
TEST_F(CreatureModeSystemTest, APlaceKeyOverADualCameraPutsANewPlayersModeOverIt)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	script_camera::Get().position.SetPosition({900.0f, 60.0f, 900.0f});
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), glm::vec3(900.0f, 60.0f, 900.0f));
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(camera_checks::CurrentTop(), camera_checks::Top::Player);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(script_camera::DualCurrent());
}

// A place key while a placed path pushed over the follow has the camera: the new player's camera goes over the path,
// which keeps its place under it without the camera, and the follow keeps its model under them both. The new mode's
// pop gives the path the camera again, the follow's model the camera's
TEST_F(CreatureModeSystemTest, APlaceKeyOverAPlacedPathOverTheFollowPutsANewPlayersCameraOverThemBoth)
{
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	auto& paths = static_cast<ecs::systems::CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<ecs::systems::CameraPathSystem>([] { return true; }));
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	ASSERT_TRUE(paths.Begin(k_PathOwner, StraightPath(), glm::mat4(1.0f), 1.0f));
	ASSERT_TRUE(paths.HoldsCamera());
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_EQ(paths.PlacedCount(), 1u);
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(paths.HoldsCamera());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_FALSE(script_camera::Modes().FollowCurrent());
}

// With no follow, a place key over a placed path that has the camera puts a new player's mode over it
TEST_F(CreatureModeSystemTest, APlaceKeyOverAPlacedPathPutsANewPlayersModeOverIt)
{
	const test::RestoreService<Locator::cameraPathSystem> restorePaths;
	auto& paths = static_cast<ecs::systems::CameraPathSystem&>(
	    Locator::cameraPathSystem::emplace<ecs::systems::CameraPathSystem>([] { return true; }));
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(paths.Begin(k_PathOwner, StraightPath(), glm::mat4(1.0f), 1.0f));
	EXPECT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(paths.HoldsCamera());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(paths.HoldsCamera());
}

// A script's camera mode still going cannot be left: a place key makes no new player's mode over it, and the mode
// current is not a player's, so the keys fly nothing
TEST_F(CreatureModeSystemTest, APlaceKeyOverAScriptsCameraModeMakesNoneAndFliesNothing)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	EXPECT_FALSE(camera_checks::PutNewPlayersModeOver(mode));
	EXPECT_TRUE(script_camera::ScriptModeCurrent());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);
	EXPECT_NE(camera_checks::CurrentTop(), camera_checks::Top::Player);
}

namespace
{
/// The temple as far as the camera's modes know of it: going in, its rooms' camera takes the camera's model and keeps the
/// one it took aside, in the citadel's sequence mode; going out it gives that model back and the camera's current mode
/// goes on as the way out has it (camera_checks::LeaveCitadel), as the temple interior does
class FakeTemple final: public TempleInteriorInterface
{
public:
	FakeTemple() = default;
	~FakeTemple() override { game_clock::SetSequenceMode(_sequenceBefore); }
	FakeTemple(const FakeTemple&) = delete;
	FakeTemple& operator=(const FakeTemple&) = delete;
	FakeTemple(FakeTemple&&) = delete;
	FakeTemple& operator=(FakeTemple&&) = delete;

	/// Leaving for this place of the map, when set
	std::optional<world_camera::Place> leaveTo;
	/// The way in keeps the camera's model on an island's camera of its own, as the temple does, rather than aside
	bool keepsWorldCamera {false};

	[[nodiscard]] Camera* GetWorldCamera() override { return _active ? _world.get() : nullptr; }

	[[nodiscard]] bool Active() const override { return _active; }
	[[nodiscard]] glm::vec3 GetPosition() const override { return {}; }
	[[nodiscard]] TempleRoom GetCurrentRoom() const override { return TempleRoom::Main; }
	void SetCurrentRoom(TempleRoom) override {}
	[[nodiscard]] std::optional<TempleRoom> GetTransitionRoom() const override { return std::nullopt; }
	void SetTransitionRoom(std::optional<TempleRoom>) override {}
	void GoToRoom(TempleRoom) override {}
	void EnterRoom(TempleRoom) override {}
	void LookAtScroll(size_t) override {}
	[[nodiscard]] bool IsRoomDrawn(TempleRoom) const override { return _active; }
	[[nodiscard]] TempleDoors& GetDoors() override { return _doors; }
	[[nodiscard]] const TempleDoors& GetDoors() const override { return _doors; }
	[[nodiscard]] std::optional<TempleCursorHit> GetCursorHit() const override { return std::nullopt; }
	void SetInterface(gui::GameInterface*) override {}
	bool HoldControl(bool, float) override { return false; }
	[[nodiscard]] std::vector<TempleSubMeshTexture> GetScrollTextures(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<uint32_t> GetHiddenSubMeshes(TempleRoom) const override { return {}; }
	[[nodiscard]] std::vector<TempleSubMeshGlow> GetControlGlows(TempleRoom) const override { return {}; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetText() const override { return _text; }
	[[nodiscard]] const graphics::Texture2D* GetTextTexture() const override { return nullptr; }
	[[nodiscard]] const std::vector<OrientedTextVertex>& GetMap() const override { return _text; }
	[[nodiscard]] uint32_t GetVisits() const override { return 0; }
	[[nodiscard]] const std::vector<TempleMapMarker>& GetMapMarkers() const override { return _markers; }
	[[nodiscard]] const TempleLight& GetLight() const override { return _light; }
	[[nodiscard]] const std::vector<TempleCaveTrophy>& GetCaveTrophies() const override { return _trophies; }
	[[nodiscard]] float GetMapMarkerTurn() const override { return 0.0f; }
	[[nodiscard]] float GetPoolTime() const override { return 0.0f; }
	void Escape() override {}
	void RequestLeave() override {}
	void FadeToWhite() override {}
	void Update(std::chrono::microseconds) override {}
	void ProcessTurn() override {}
	[[nodiscard]] glm::vec2 GetWaterfallSlide() const override { return {}; }
	void Activate() override { Activate(TempleRoom::Main); }
	void Activate(TempleRoom) override
	{
		auto& camera = Locator::camera::value();
		auto outside =
		    camera.SetModel(std::make_unique<DefaultWorldCameraModel>(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f)));
		if (keepsWorldCamera)
		{
			_world = std::make_unique<Camera>();
			camera.CopyViewTo(*_world);
			std::ignore = _world->SetModel(std::move(outside));
		}
		else
		{
			_outsideModel = std::move(outside);
		}
		_templeModel = &camera.GetModel();
		_active = true;
		game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
	}
	void Deactivate() override
	{
		if (_world != nullptr)
		{
			_outsideModel = _world->SetModel(nullptr);
			_world.reset();
		}
		Locator::camera::value().SetModel(std::move(_outsideModel));
		_templeModel = nullptr;
		_active = false;
		game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
		camera_checks::LeaveCitadel(std::exchange(leaveTo, std::nullopt));
	}

	/// The rooms' camera's model while the player is inside
	[[nodiscard]] const CameraModel* TempleModel() const { return _templeModel; }

private:
	bool _active {false};
	std::unique_ptr<CameraModel> _outsideModel;
	std::unique_ptr<Camera> _world;
	const CameraModel* _templeModel {nullptr};
	int32_t _sequenceBefore {game_clock::SequenceMode()};
	TempleDoors _doors {[](const temple_sounds::Options&) {}};
	std::vector<OrientedTextVertex> _text;
	std::vector<TempleMapMarker> _markers;
	std::vector<TempleCaveTrophy> _trophies;
	TempleLight _light;
};

constexpr world_camera::Place k_MapPlace {.origin = {1500.0f, 80.0f, 1570.0f}, .focus = {1500.0f, 30.0f, 1500.0f}};
} // namespace

// The map's clear inside the temple (a script's change of land), with the follow the current mode and its model on the
// island's camera behind the temple: the follow goes and that camera has the player's camera the follow went over again
// at once, started again as the clear's new player's mode, which the way out starts again; nothing waits for Creature
// Mode's next update
TEST_F(CreatureModeSystemTest, AClearInsideTheTempleGivesTheIslandsCameraANewPlayersModeThatTheWayOutStartsAgain)
{
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	const auto* player = dynamic_cast<const DefaultWorldCameraModel*>(_playerModel);
	ASSERT_NE(player, nullptr);
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	temple.keepsWorldCamera = true;
	temple.Activate(TempleRoom::Main);
	auto* world = temple.GetWorldCamera();
	ASSERT_NE(world, nullptr);
	ASSERT_EQ(&world->GetModel(), follow);
	const auto restarts = player->GetRestarts().count;
	camera_checks::ClearStack(false);
	EXPECT_FALSE(mode->IsActive());
	EXPECT_EQ(&world->GetModel(), _playerModel);
	EXPECT_EQ(player->GetRestarts().count, restarts + 1);
	EXPECT_EQ(script_camera::Modes().OtherCount(), 0u);
	// the rooms' camera keeps its own model
	EXPECT_EQ(&_camera->GetModel(), temple.TempleModel());
	temple.Deactivate();
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->IsActive());
	mode->Update(k_Step, {});
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(mode->IsActive());
}

// The way into the temple touches no camera mode: the follow stays on the stack, the current mode, while the temple's
// camera has the camera's model, and a save inside writes it; given its model back on the way out, it goes on, and
// nothing restarts
TEST_F(CreatureModeSystemTest, AFollowGoesOnThroughTheTemple)
{
	// the camera's pops and the temple's way out reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_NE(follow, nullptr);
	const auto view = follow->GetView();
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	temple.Activate(TempleRoom::Main);
	ASSERT_EQ(&_camera->GetModel(), temple.TempleModel());
	for (int i = 0; i < 5; ++i)
	{
		mode->Update(k_Step, {});
	}
	EXPECT_TRUE(mode->IsActive());
	EXPECT_EQ(mode->GetCreature(), _yours);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_EQ(&_camera->GetModel(), temple.TempleModel());
	// it has not the camera, but it is the current mode a save writes
	EXPECT_FALSE(mode->GetView().has_value());
	const auto current = mode->GetCurrentFollow();
	ASSERT_TRUE(current.has_value());
	EXPECT_EQ(current->view.yaw, view.yaw);
	EXPECT_EQ(current->view.distance, view.distance);
	// C and the double click take nothing inside
	_actions->Press(BindableActionMap::ZOOM_TO_CREATURE);
	mode->Update(k_Step, {});
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(mode->Enter(_theirs));
	_actions->Hold({});

	temple.Deactivate();
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	EXPECT_FALSE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	ASSERT_TRUE(mode->GetView().has_value());
	mode->Update(k_Step, {});
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_EQ(mode->GetCreature(), _yours);
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// Leaving the temple for a place of the map with the follow the current mode: the camera at the place, and a new player's
// mode from there on top, the follow staying under it until that mode is popped. The way out reaches Creature Mode as
// the game's camera services do
TEST_F(CreatureModeSystemTest, LeavingTheTempleForTheMapPutsANewPlayersModeOverTheFollow)
{
	// the temple's way out reaches Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	temple.Activate(TempleRoom::Main);
	mode->Update(k_Step, {});
	temple.leaveTo = k_MapPlace;
	temple.Deactivate();
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), k_MapPlace.origin);
	EXPECT_EQ(_camera->GetFocusZoomer().GetCurrentValue(), k_MapPlace.focus);
	EXPECT_EQ(CreatureCamera(), nullptr);
	EXPECT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&_camera->GetModel()), nullptr);
	EXPECT_FALSE(PlayerHasTheCamera());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
	EXPECT_TRUE(mode->IsActive());
	EXPECT_FALSE(mode->GetCurrentFollow().has_value());
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	EXPECT_TRUE(script_camera::Modes().FollowCurrent());
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// A follow under a new player's mode goes into the temple and out again under it: inside, the new mode's model is the one
// the temple keeps aside and the follow is not the current mode; on the way out that player's mode starts again, a place
// of the map picked or not, and no other mode is made
TEST_F(CreatureModeSystemTest, AFollowUnderANewPlayersModeStaysUnderItThroughTheTemple)
{
	// the temple's way out and the camera's pop reach Creature Mode through the locator
	const test::RestoreService<Locator::creatureModeSystem> restoreMode;
	const auto yours = _yours;
	auto* mode = &static_cast<CreatureModeSystem&>(
	    Locator::creatureModeSystem::emplace<CreatureModeSystem>([yours](PlayerNames player) -> std::optional<entt::entity> {
		    return player == PlayerNames::PLAYER_ONE ? std::optional(yours) : std::nullopt;
	    }));
	ASSERT_TRUE(mode->Enter(_yours));
	auto* follow = CreatureCamera();
	ASSERT_TRUE(camera_checks::PutNewPlayersModeOver(mode));
	const auto* players = &_camera->GetModel();
	ASSERT_NE(dynamic_cast<const DefaultWorldCameraModel*>(players), nullptr);
	const auto stacked = script_camera::Modes().Size();
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	for (const auto& leaveTo : {std::optional<world_camera::Place> {}, std::optional(k_MapPlace)})
	{
		temple.Activate(TempleRoom::Main);
		for (int i = 0; i < 3; ++i)
		{
			mode->Update(k_Step, {});
		}
		EXPECT_TRUE(mode->IsActive());
		EXPECT_FALSE(mode->GetCurrentFollow().has_value());
		EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
		temple.leaveTo = leaveTo;
		temple.Deactivate();
		EXPECT_EQ(&_camera->GetModel(), players);
		EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
		EXPECT_EQ(script_camera::Modes().Size(), stacked);
		EXPECT_EQ(script_camera::Modes().OtherCount(), 1u);
		EXPECT_EQ(mode->GetCreature(), _yours);
	}
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), k_MapPlace.origin);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_EQ(CreatureCamera(), follow);
	mode->Leave();
	EXPECT_TRUE(PlayerHasTheCamera());
}

// Leaving the temple with a dual camera the current mode: with no place picked the dual camera goes on as it is; leaving
// for a place of the map cuts the zoomers there (the dual camera's are the camera's) and puts a new player's mode over
// the dual camera from there, whose pop leaves the dual camera current again
TEST_F(CreatureModeSystemTest, LeavingTheTempleForTheMapPutsANewPlayersModeOverADualCamera)
{
	script_camera::StartDual(_yours, _theirs, _camera->GetOriginZoomer(), _camera->GetFocusZoomer());
	ASSERT_TRUE(script_camera::DualCurrent());
	auto& temple = static_cast<FakeTemple&>(Locator::temple::emplace<FakeTemple>());
	temple.Activate(TempleRoom::Main);
	temple.Deactivate();
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Modes().Size(), 1u);

	temple.Activate(TempleRoom::Main);
	temple.leaveTo = k_MapPlace;
	temple.Deactivate();
	EXPECT_EQ(_camera->GetOriginZoomer().GetCurrentValue(), k_MapPlace.origin);
	EXPECT_EQ(_camera->GetFocusZoomer().GetCurrentValue(), k_MapPlace.focus);
	EXPECT_TRUE(PlayerHasTheCamera());
	EXPECT_FALSE(script_camera::DualCurrent());
	EXPECT_FALSE(script_camera::Drives());
	EXPECT_TRUE(script_camera::Modes().PlayerCurrent());
	EXPECT_EQ(script_camera::Modes().Size(), 2u);
	ASSERT_TRUE(script_camera::PopPlayersMode());
	EXPECT_TRUE(script_camera::DualCurrent());
	EXPECT_EQ(script_camera::Get().position.GetCurrentValue(), k_MapPlace.origin);
}
