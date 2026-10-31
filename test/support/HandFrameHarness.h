/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The player's hand in the world of the game's own systems (support/WorldHarness.h), played frame by frame as the game
// plays it: the turns that are due (the turn's packets, the hand's turn, the physics, the hand's end of turn), the packets
// flushed, the frame clock, then the hand's frame in the game's order (its state, the mouse ray and the land under the
// cursor, its point, its update with the buttons, the miracles' frame's first step, which turns the one-shot orbs to the
// camera, then its placement). The input is what the game's input step leaves for the frame: the cursor and the hand's
// buttons in the input state, and the mouse in the game's actions. The camera stays where the test puts it. The clock
// runs on ticks the frames advance. On the game's data the hand has its model and its animations (hh.HBN), and its
// state machine runs; on synthetic data it has neither, and only what needs no animation runs (the packets, the
// cursor's pick). Include it after LOCATOR_IMPLEMENTATIONS.

#include <cmath>
#include <cstdint>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vector_relational.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/PlayerArchetype.h"
#include "ECS/Components/Hand.h"
#include "ECS/DrawList/BlockCull.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Systems/Implementations/HandGrain.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/PickingSystem.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Help/HelpProfile.h"
#include "Input/GameCursor.h"
#include "Input/GamePackets.h"
#include "Input/InterfaceActive.h"
#include "Input/MouseButtons.h"
#include "support/FakeGameActions.h"
#include "support/FakeWindowing.h"
#include "support/RecordingAudio.h"
#include "support/RecordingSoundTags.h"
#include "support/WorldHarness.h"

namespace openblack::test::world
{
class HandFrameHarness: public WorldHarness
{
protected:
	/// A frame's step: under 50 ms, so that the last ping gives a throw no turns of prediction, and not a divisor of
	/// 180 ms, so that the release's countdown never ends on exactly 0
	static constexpr std::chrono::milliseconds k_Frame {33};
	/// The window
	static constexpr glm::ivec2 k_Screen {640, 480};
	/// Why a test of the hand's state machine skips its synthetic run: `if (!GameData()) { GTEST_SKIP() <<
	/// k_NeedsAnimations; }` then `ASSERT_TRUE(HasAnimator())`
	static constexpr std::string_view k_NeedsAnimations =
	    "the hand's states need its animations (hh.HBN) and its model, which only the game's data has";

	/// What the frame passed to the hand
	struct FrameSeen
	{
		glm::vec3 rayOrigin {0.0f};
		glm::vec3 rayDirection {0.0f};
		glm::vec3 nearPoint {0.0f};
		/// The hand's land under the cursor
		std::optional<glm::vec3> handLand;
		/// The interface's land under the cursor
		std::optional<glm::vec3> interfaceLand;
		/// The point given to the hand's placement (none over the sky)
		std::optional<glm::vec3> point;
	};

	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		if (!GameData())
		{
			FillSyntheticFrameRows();
		}
		Locator::windowing::emplace<FakeWindowing>(k_Screen);
		_actions = &static_cast<FakeGameActions&>(Locator::gameActionSystem::emplace<FakeGameActions>());
		_config = &Locator::config::emplace();
		_camera = &Locator::camera::emplace(glm::vec3(1000.0f, k_Ground, 1000.0f));
		_camera->SetProjectionMatrixPerspective(_config->cameraXFov,
		                                        static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y),
		                                        _config->cameraNearClip, _config->cameraFarClip);
		_picking = &Locator::pickingSystem::emplace<ecs::systems::PickingSystem>();
		_tags = &static_cast<RecordingSoundTags&>(Locator::soundTagSystem::emplace<RecordingSoundTags>());
		_audio = &static_cast<RecordingAudio&>(Locator::audio::emplace<RecordingAudio>());
		static_cast<void>(ecs::archetypes::PlayerArchetype::Create(PlayerNames::PLAYER_ONE));
		ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE) = {};
		help_profile::Reset();
		game_packets::Reset();
		// the clock on the frames' ticks, the game timer started at turn 1, the engine timer with it
		s_ticks = k_StartTicks;
		game_clock::SetTickSource(&Ticks);
		game_clock::Reset();
		game_clock::SetTurn(1);
		game_clock::Start(false);
		game_clock::StartEngineTimer();
		_clockSet = true;
		// the cursor in the middle of the window, no button held
		input::GameCursorRef() = k_Screen / 2;
		_actions->mouse = glm::uvec2(k_Screen / 2);
		input::HandButtonsRef() = {};
		interface_active::SetBoxHasMouse(false);
		// the hand's model, as the game loads it, then the hand's own start: its animations (hh.HBN), its morphs and its
		// handlers
		if (GameData())
		{
			Locator::resources::value().GetMeshes().Load(
			    ecs::components::Hand::k_MeshId, resources::L3DLoader::FromDiskTag {},
			    Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
		}
		_hand = &static_cast<ecs::systems::HandSystem&>(Locator::handSystem::emplace<ecs::systems::HandSystem>());
		static_cast<void>(_hand->Initialize());
		_actions->hands = _hand->GetPlayerHandPositions();
	}

	void TearDown() override
	{
		if (Locator::handSystem::has_value())
		{
			Locator::handSystem::reset();
			_hand = nullptr;
		}
		if (_clockSet)
		{
			game_packets::Reset();
			if (Locator::inputState::has_value())
			{
				input::HandButtonsRef() = {};
				input::GameCursorRef() = glm::ivec2(0);
			}
			game_clock::SetTickSource(nullptr);
			game_clock::Reset();
			help_profile::Reset();
			ecs::effects::alignment::Of(PlayerNames::PLAYER_ONE) = {};
			// a pour a test started goes with it
			ecs::systems::hand_grain::Reset();
		}
		WorldHarness::TearDown();
	}

	/// The hand has its animations: the game's data only
	[[nodiscard]] bool HasAnimator() const { return _hand != nullptr && _hand->GetBoneMatrices() != nullptr; }

	[[nodiscard]] ecs::systems::HandSystem& Hand() const { return *_hand; }
	[[nodiscard]] RecordingSoundTags& Tags() const { return *_tags; }
	[[nodiscard]] RecordingAudio& Sounds() const { return *_audio; }
	[[nodiscard]] Camera& View() const { return *_camera; }
	[[nodiscard]] FakeGameActions& Actions() const { return *_actions; }
	[[nodiscard]] const FrameSeen& LastFrame() const { return _seen; }

	// ---- the input, as the game's input step leaves it ------------------------------------------------------------

	/// The cursor at a pixel of the window
	void SetMouse(glm::ivec2 pixel)
	{
		input::GameCursorRef() = pixel;
		_actions->mouse = glm::uvec2(glm::max(pixel, glm::ivec2(0)));
	}
	/// The pixel the camera shows a point of the world at: the mouse ray's construction undone (the camera's axes, its
	/// horizontal field of view and its aspect)
	[[nodiscard]] glm::ivec2 PixelOf(glm::vec3 world) const
	{
		const glm::mat4 view = _camera->GetViewMatrix(Camera::Interpolation::Current);
		const glm::vec3 right(view[0][0], view[1][0], view[2][0]);
		const glm::vec3 up(view[0][1], view[1][1], view[2][1]);
		const glm::vec3 forward(view[0][2], view[1][2], view[2][2]);
		const glm::vec3 d = world - _camera->GetEye();
		const float along = glm::dot(d, forward);
		const float x = glm::dot(d, right) / along;
		const float y = glm::dot(d, up) / along;
		const float tanHalf = std::tan(_camera->GetHorizontalFieldOfView() * 0.5f);
		const float sx = x / (2.0f * tanHalf) + 0.5f;
		const float sy = 0.5f - y * _camera->GetAspect() / (2.0f * tanHalf);
		return {static_cast<int>(std::lround(sx * static_cast<float>(k_Screen.x))),
		        static_cast<int>(std::lround(sy * static_cast<float>(k_Screen.y)))};
	}
	/// The cursor over a point of the world
	void MouseOver(glm::vec3 world) { SetMouse(PixelOf(world)); }
	void PressAction() { input::HandButtonsRef().action = true; }
	void ReleaseAction() { input::HandButtonsRef().action = false; }
	void PressGrip()
	{
		input::HandButtonsRef().gripping = true;
		_actions->buttons = static_cast<uint8_t>(_actions->buttons | (1u << static_cast<uint8_t>(input::MouseButton::Left)));
	}
	void ReleaseGrip()
	{
		input::HandButtonsRef().gripping = false;
		_actions->buttons = static_cast<uint8_t>(_actions->buttons & ~(1u << static_cast<uint8_t>(input::MouseButton::Left)));
	}
	/// The action button double clicked: the next frame's hand takes it
	void DoubleClickAction() { _doubleClick = true; }
	/// The camera at `eye`, looking at `focus`
	void LookFrom(glm::vec3 eye, glm::vec3 focus) { _camera->SetOrigin(eye).SetFocus(focus); }

	// ---- the frame and the turn -----------------------------------------------------------------------------------

	/// One frame of `dt`, as the game plays it
	void Frame(std::chrono::milliseconds dt = k_Frame)
	{
		s_ticks += static_cast<uint32_t>(dt.count());
		// the turns that are due (at most one a frame)
		while (game_clock::TurnDue())
		{
			Turn();
		}
		// the packets sent so far go to the session: the next turn applies them
		game_packets::Flush();
		// the frame clock, then the real clock
		game_clock::UpdateFrameClock();
		game_clock::UpdateRealClock();
		CameraFrame(dt);
		HandFrame(dt);
	}
	void Frames(int n, std::chrono::milliseconds dt = k_Frame)
	{
		for (int i = 0; i < n; ++i)
		{
			Frame(dt);
		}
	}
	/// Frames until `done` holds after one, at most `max`; whether it did
	bool FramesUntil(const std::function<bool()>& done, int max, std::chrono::milliseconds dt = k_Frame)
	{
		for (int i = 0; i < max; ++i)
		{
			Frame(dt);
			if (done())
			{
				return true;
			}
		}
		return false;
	}

	/// The turn's start: the packets the last flush passed on, the hand's turn, the turn number; then the test's look
	void BeforePhysics() override
	{
		game_packets::DispatchQueuedPackets();
		_hand->ProcessTurn();
		game_clock::StartTurn();
		OnTurnApplied();
	}
	/// The hand's end of turn: the grain's raise with the turn's length, then the hold
	void AfterPhysics() override
	{
		ecs::systems::hand_grain::GameTurnUpdate(static_cast<float>(game_clock::MsPerTurn()) * 0.001f);
		_hand->GameTurnUpdate();
	}
	/// The test's look at the world between the turn's packets and its physics
	virtual void OnTurnApplied() {}
	/// The camera's own update: none, the camera stays where the test put it
	virtual void CameraStep(std::chrono::milliseconds /*dt*/) {}

	/// A ring of the player's influence
	static entt::entity InfluenceOver(glm::vec3 centre, float radius)
	{
		return influence::CreateRing(centre, PlayerNames::PLAYER_ONE, radius, false);
	}

private:
	/// The camera's part of the frame: its near plane from its height over the land at the quantised point, as the game
	/// takes it from the camera drawn the frame before, then its own update
	void CameraFrame(std::chrono::milliseconds dt)
	{
		const auto eye = _camera->GetOrigin();
		const auto lookup = [](float m) { return map_coords::ToMetres(map_coords::MetresToFixedForHandLookup(m)); };
		const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(lookup(eye.x), lookup(eye.z)));
		const float nearClip = ecs::draw_list::NearClipFor(eye.y - ground, false);
		_camera->SetPixelNearClip(nearClip);
		CameraStep(dt);
		if (nearClip != _config->cameraNearClip)
		{
			_config->cameraNearClip = nearClip;
			_camera->SetProjectionMatrixPerspective(_config->cameraXFov,
			                                        static_cast<float>(k_Screen.x) / static_cast<float>(k_Screen.y),
			                                        _config->cameraNearClip, _config->cameraFarClip);
		}
	}

	/// The hand's part of the frame, in the game's order
	void HandFrame(std::chrono::milliseconds dt)
	{
		const auto step = std::chrono::duration_cast<std::chrono::microseconds>(dt);
		const auto buttons = input::HandButtonsRef();
		// the state change first: the cursor's point and the interface's pass see this frame's state
		_hand->PrepareState();
		// no box takes the input
		interface_active::SetBoxHasMouse(false);
		// the mouse ray from the eye, the hand's land and the interface's under the cursor
		const auto cursor = input::GameCursorRef();
		const auto ray = _camera->RayFromEye(glm::vec2(cursor) / glm::vec2(k_Screen));
		_seen = {};
		_seen.rayOrigin = ray.origin;
		_seen.rayDirection = ray.direction;
		_point = glm::zero<glm::vec3>();
		if (!glm::any(glm::isnan(ray.origin)) && !glm::any(glm::isnan(ray.direction)))
		{
			const float nearClip = _camera->GetPixelNearClip();
			_seen.nearPoint = _camera->PixelNearPoint(ray.origin, k_Screen, cursor, nearClip);
			_seen.handLand = _picking->LandFrom2DPoint(cursor, true, nearClip + 1.0f);
			_seen.interfaceLand = _picking->LandUnderPixel(ray.origin, _seen.nearPoint, true);
			const auto point = _hand->ResolveCursorPoint(
			    ray.origin, ray.direction,
			    {.hand = _seen.handLand, .interface = _seen.interfaceLand, .nearPoint = _seen.nearPoint}, buttons.gripping,
			    step);
			_point = point.value_or(glm::zero<glm::vec3>());
		}
		// the hand's animation with the cursor's motion and the buttons
		const auto mouseDelta = glm::vec2(input::MouseMotion(_motion, cursor));
		if (std::exchange(_doubleClick, false))
		{
			_hand->ActionDoubleClick();
		}
		_hand->Update(step, mouseDelta, buttons.gripping, buttons.action);
		// the miracles' frame's first step, in game time: the one-shot orbs turned to the camera, which the cursor's pick
		// of an orb reads. The rest of that frame (the weather, the gestures, the worship sites, the vortices) is not this
		// world's
		if (Locator::miracleFxSystem::has_value())
		{
			Locator::miracleFxSystem::value().UpdateGlobes(game_clock::FrameGameSeconds());
		}
		// the hand's placement on the point under the cursor
		const bool overLand = _point != glm::zero<glm::vec3>();
		_seen.point = overLand ? std::optional(_point) : std::nullopt;
		_hand->Place(_seen.point, _camera->GetForward(), buttons.gripping, step);
		_actions->hands = _hand->GetPlayerHandPositions();
	}

	/// The rows a synthetic world's hand reads: the influence's gradient (0.4, 0.2, 0.2), as the game's tables have it
	void FillSyntheticFrameRows() const
	{
		Info().influence.percentageFullInfluence = 0.4f;
		Info().influence.percentageDistanceForDecreasingGradient = 0.2f;
		Info().influence.valueOfSmall = 0.2f;
	}

	static uint32_t Ticks() { return s_ticks; }
	static constexpr uint32_t k_StartTicks = 100000;
	static inline uint32_t s_ticks {0};

	// put back in reverse order
	RestoreService<Locator::windowing> _restoreWindowing;
	RestoreService<Locator::gameActionSystem> _restoreActions;
	RestoreService<Locator::config> _restoreConfig;
	RestoreService<Locator::camera> _restoreCamera;
	RestoreService<Locator::pickingSystem> _restorePicking;
	RestoreService<Locator::soundTagSystem> _restoreTags;
	RestoreService<Locator::audio> _restoreAudio;
	RestoreService<Locator::handSystem> _restoreHand;
	FakeGameActions* _actions {nullptr};
	EngineConfig* _config {nullptr};
	Camera* _camera {nullptr};
	ecs::systems::PickingSystemInterface* _picking {nullptr};
	RecordingSoundTags* _tags {nullptr};
	RecordingAudio* _audio {nullptr};
	ecs::systems::HandSystem* _hand {nullptr};
	input::MouseMotionState _motion;
	FrameSeen _seen;
	glm::vec3 _point {0.0f};
	bool _doubleClick {false};
	bool _clockSet {false};
};
} // namespace openblack::test::world
