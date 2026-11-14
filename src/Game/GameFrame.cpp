/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <SDL.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>

#include "3D/Billboard.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Camera/ScriptCamera.h"
#include "Common/HelpText.h"
#include "Debug/DebugGuiInterface.h"
#include "ECS/ChallengePictures.h"
#include "ECS/Components/NotDrawn.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/DrawGate.h"
#include "ECS/GroundMarks.h"
#include "ECS/MapCells.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PlayerColours.h"
#include "ECS/Registry.h"
#include "ECS/SnowLevels.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/SavePictureSystemInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/VillagerLabelSource.h"
#include "Engine/FrameSnapshot.h"
#include "Engine/GpuCommands.h"
#include "EngineConfig.h"
#include "Fire/FireGraphic.h"
#include "Game.h"
#include "Game/OverlayFills.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Graphics/KeyOrMouseLayout.h"
#include "Graphics/ObjectOrder.h"
#include "Graphics/OverlayFrame.h"
#include "Graphics/RendererInterface.h"
#include "Gui/GameInterface.h"
#include "Gui/VillagerLabels.h"
#include "Hand/HandDemo.h"
#include "Hand/HandTricons.h"
#include "Help/HelpSystem.h"
#include "Help/InputPromptIcon.h"
#include "Help/ToolTips.h"
#include "Input/FixedMouse.h"
#include "Input/GameActionMapInterface.h"
#include "Input/GameCursor.h"
#include "Input/InterfaceActive.h"
#include "Input/RealInput.h"
#include "Locator.h"
#include "Profiler.h"
#include "Windowing/WindowingInterface.h"

// The frame's logic -> draw split: the end of the frame's logic lays out what the draw reads (Engine/FrameSnapshot.h),
// and the draw reads only that copy

using namespace openblack;
using namespace openblack::game_overlay;

void Game::FillHandTricons(bool boxActive, graphics::OverlayFrame& out)
{
	out.handTricons = {};
	out.handDemoMouse.reset();
	// A turn whose hand state holds the red cross back keeps it away for at least 250 ms, counted down every frame by
	// the frame's time ((inferred) the wall clock's), whether or not the icons are drawn
	if (Locator::handSystem::has_value())
	{
		constexpr int32_t k_CrossHeldMs = 250;
		const auto holdBacks = Locator::handSystem::value().GetCameraIconFacts().crossHoldBacks;
		if (holdBacks != _handCrossHoldBacks)
		{
			_handCrossHoldBacks = holdBacks;
			_handCrossHeldMs = std::max(_handCrossHeldMs, k_CrossHeldMs);
		}
		_handCrossHeldMs = std::max(_handCrossHeldMs - static_cast<int32_t>(game_clock::FrameRealMs()), 0);
	}
	// Faded and drawn only with the camera and the hand, the interface active, out of the temple and with no box up;
	// otherwise the strengths stay where they were
	if (!Locator::camera::has_value() || !Locator::handSystem::has_value() || !Locator::gameActionSystem::has_value() ||
	    out.width == 0 || out.height == 0 || !interface_active::IsActive() || game_clock::IsInsideCitadel() ||
	    (Locator::temple::has_value() && Locator::temple::value().Active()) || boxActive)
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const auto cues = camera.GetModel().GetHandCues();
	// The player's own world camera picks them, or a hand demo's recording while it plays under the player's camera or
	// a script's; under any other camera (a placed path's, the creature's) they stay as they were. So do they while the
	// hand is hidden
	const auto demoHints = hand_demo::PlayingHints();
	const bool scriptCamera = script_camera::Drives();
	const bool playerCamera = !scriptCamera && cues.icons.has_value() &&
	                          !(Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera());
	auto& handSystem = Locator::handSystem::value();
	const auto hands = handSystem.GetPlayerHands();
	const auto& registry = Locator::entitiesRegistry::value();
	if (hands.empty() || !registry.Valid(hands[0]) ||
	    !hand_tricons::Drawn({
	        .playerCamera = playerCamera,
	        .scriptCamera = scriptCamera,
	        .demoPlaying = demoHints.has_value(),
	        .handHidden = registry.AllOf<ecs::components::NotDrawn>(hands[0]),
	    }))
	{
		return;
	}
	// At the hand's point projected as the sprites are, without clipping, the point the tooltip's icon is drawn by: a
	// hand off the screen keeps the icons on its edge. Not with the hand at or behind the eye
	const auto& transform = registry.Get<ecs::components::Transform>(hands[0]);
	const auto onScreen = screen_point::Project(graphics::billboard::CameraFrame::From(camera),
	                                            screen_point::LensOf(out.width, out.height, camera.GetHorizontalFieldOfView()),
	                                            transform.position);
	if (!onScreen.has_value())
	{
		return;
	}
	const glm::ivec2 hand {graphics::key_or_mouse::StoredInt(onScreen->pixel.x),
	                       graphics::key_or_mouse::StoredInt(onScreen->pixel.y)};
	const auto facts = handSystem.GetCameraIconFacts();
	const auto& actions = Locator::gameActionSystem::value();
	hand_tricons::FadeFrame fade {
	    .seconds = static_cast<float>(game_clock::CameraFrameMs(demoHints.has_value())) * 0.001f,
	    // the cross goes at once, and stays away, while a hand demo plays or while it is held back
	    .demonstration = demoHints.has_value() || _handCrossHeldMs > 0,
	};
	if (demoHints.has_value())
	{
		// The recording's icons and the rotate arrow's turn
		fade.icons = *demoHints;
		_handTriconAngle = hand_demo::PlayingHintAngle();
	}
	else
	{
		auto frame = *cues.icons;
		frame.handInInfluence = facts.inInfluence;
		frame.tickMs = game_clock::TickCount();
		const auto icons = hand_tricons::WorldCamera(frame, _handTriconAngle);
		_handTriconAngle = icons.rotateAngle;
		fade.icons = icons.icons;
		fade.cameraBusy =
		    frame.gripping || frame.rotatingAroundMouse || frame.turn != 0.0f || frame.tilt != 0.0f || frame.zoom != 0.0f;
		// The cross brightens with a thing under the hand, and shows at once with the action button
		if (actions.Get(input::BindableActionMap::ACTION))
		{
			fade.crossNudge = hand_tricons::CrossNudge::Action;
		}
		else if (facts.overObject)
		{
			fade.crossNudge = hand_tricons::CrossNudge::OverThing;
		}
	}
	// The camera icons go where the tooltips are switched off or the hand's state doesn't show them; the cross stays
	fade.icons = hand_tricons::Shown(fade.icons, help::tooltips::Level() != 0, facts.showsCameraIcons);
	// what the scripts ask of the icons shown
	Locator::camera::value().SetShownHandIcons(fade.icons);
	hand_tricons::Fade(_handTriconFades, fade);
	// A quarter of the way to the cursor the hand follows, unless the clear view is coming in
	const glm::ivec2 screen {out.width, out.height};
	const auto centre = hand_tricons::Place(
	    hand,
	    hand_tricons::FollowedCursor(hand_demo::IsPlaying(), input::GameCursorRef(), glm::ivec2(actions.GetMousePosition())),
	    screen, CinematicDirector().IsWideScreenOn(), cues.clearView <= hand_tricons::k_ClearViewLeans);
	const float halfSize = hand_tricons::IconHalfSize(camera.GetProjectionMatrix()[0][0], screen);
	out.handTricons = hand_tricons::Sprites(glm::vec2(centre), halfSize, _handTriconFades, _handTriconAngle);
	// While a hand demo plays, its mouse and the word "Demo" by the icons, on the side it keeps until it is past a third
	// of the screen, with the buttons whose press the hand took lit until what they started ended
	if (hand_demo::IsPlaying())
	{
		_handDemoMouseOnLeft = hand_tricons::LabelOnLeft(centre.x, out.width, _handDemoMouseOnLeft);
		const auto place = hand_tricons::PlaceDemoMouse(centre, _handDemoMouseOnLeft);
		graphics::OverlayFrame::HandDemoMouse mouse {
		    .at = place.at, .align = place.align, .text = helptext::Get(hand_tricons::k_DemoText)};
		std::u16string keyName;
		const int32_t action = hand_tricons::DemoMouseAction(facts.moveLit, facts.actionLit);
		if (action == -1)
		{
			// no button: the mouse row while the move action is on the mouse, else none
			int32_t animType = 0;
			int32_t clickType = 0;
			int32_t row = 3;
			const bool moveOnMouse = help::input_prompt::ResolveAction(1, animType, clickType, row, keyName) && animType == 0;
			mouse.row = moveOnMouse ? help::input_prompt::MouseRow() : 3;
		}
		else if (!help::input_prompt::ResolveAction(action, mouse.animType, mouse.clickType, mouse.row, keyName))
		{
			mouse.animType = 0;
			mouse.clickType = 0;
			mouse.row = help::input_prompt::MouseRow();
		}
		out.handDemoMouse = std::move(mouse);
	}
}

void Game::PlaceVillagerNames(bool listDrew, bool landDrawn)
{
	namespace labels = gui::villager_labels;
	_villagerNames.clear();
	const auto fade = labels::StepNameFadeFrame(_villagerNameFade, Locator::config::value().showVillagerNames,
	                                            game_clock::FrameGameMs(), landDrawn);
	auto* helpSystem = help::Get();
	if (listDrew && Locator::camera::has_value() && Locator::rendererInterface::has_value())
	{
		const auto& camera = Locator::camera::value();
		const auto& renderer = Locator::rendererInterface::value();
		const auto resolution = renderer.GetResolution();
		const auto requests = ecs::villager_label_source::Gather({
		    .drawn = Locator::objectDrawListSystem::value().DrawnThisFrame(),
		    .worldToClipping = graphics::billboard::CameraFrame::From(camera).clipMatrices.worldToClipping,
		    .nearClip = camera.GetPixelNearClip(),
		    .screen = glm::ivec2(resolution),
		    .nameFade = fade.drawn,
		    .allDetails = Locator::config::value().showVillagerDetails,
		    .turn = game_clock::Turn(),
		    .wideScreen = helpSystem != nullptr && helpSystem->GetWideScreen() != 0,
		    .inHand = Locator::handSystem::has_value() ? Locator::handSystem::value().GetRenderHandObject() : std::nullopt,
		    .landNumber = ecs::player_colours::LandNumber(),
		});
		labels::Board board;
		const labels::WidthFn width = [&renderer](std::u16string_view text, float lineHeight) {
			return renderer.MeasureText(help::TextFont::J0, text, lineHeight);
		};
		for (const auto& request : requests)
		{
			board.Add(request, resolution.x, width);
		}
		_villagerNames = board.GetLabels();
	}
	_villagerNameFade = fade.next;
	// a script that holds the wide screen drops the names without drawing them
	if (helpSystem != nullptr && helpSystem->IsScriptWideScreen())
	{
		_villagerNames.clear();
	}
}

void Game::LogicFrame(engine::FrameSnapshot& out, uint32_t timeMs)
{
	auto& profiler = Locator::profiler::value();
	const auto& config = Locator::config::value();
	// the PreDraw (the end of the frame's logic, nothing between it and the draw): the overlays read here, the
	// draw reads only the copy
	FillOverlayFrame(CinematicDirector(), _interface && _interface->IsBoxActive(), out.overlay);
	FillGestureIcons(out.overlay);
	FillFloatingNumbers(out.overlay.width, out.overlay.height, out.overlay.floatingNumbers);
	FillVillagerNames(_villagerNames, out.overlay.villagerNames);
	FillHandTricons(_interface && _interface->IsBoxActive(), out.overlay);
	ecs::super_villager::FillFrame(out.superVillagers);
	// what the object draw list leaves out of this frame's draw, for the renderer's row mask
	ecs::draw_gate::FillFrame(
	    _objectListRan,
	    _objectListRan ? Locator::objectDrawListSystem::value().DrawnThisFrame() : std::span<const entt::entity> {},
	    _objectListRan ? Locator::objectDrawListSystem::value().OnScreenThisFrame() : std::span<const entt::entity> {},
	    Locator::entitiesRegistry::value(),
	    [](entt::entity object) {
		    // the town desire flags too, even one off the map: only the list draws a flag
		    return ecs::map_cells::IsObjectInMap(object) ||
		           std::as_const(Locator::entitiesRegistry::value()).AllOf<ecs::components::TownDesireFlag>(object);
	    },
	    out.objectList);
	out.objectList.applied = config.drawFollowsList;
	// the list's tail, as the original draws it after the list's passes: the falling roots, the souls, the chests falling
	// from the sky (the frame one lands too), then the ground marks, each the newest first. Read only
	out.objectList.tail.clear();
	out.objectList.physics.clear();
	out.objectList.sunk.clear();
	if (!config.objectTailUnplaced)
	{
		auto roots = Locator::handSystem::has_value() ? Locator::handSystem::value().FallingRootsNewestFirst()
		                                              : std::vector<entt::entity> {};
		std::vector<std::pair<uint32_t, entt::entity>> falling;
		std::as_const(Locator::entitiesRegistry::value())
		    .Each<const ecs::components::Reward>([&falling](entt::entity chest, const ecs::components::Reward& reward) {
			    if (reward.state == ecs::components::Reward::State::Falling || reward.goesIntoMap)
			    {
				    falling.emplace_back(reward.serial, chest);
			    }
		    });
		std::ranges::sort(falling, std::greater {});
		std::vector<entt::entity> chests;
		chests.reserve(falling.size());
		for (const auto& [serial, chest] : falling)
		{
			chests.push_back(chest);
		}
		// the physics objects the physics list draws, in its order: the awake ones not wholly under the water (centre y
		// above minus the radius); the others stay where they were drawn
		if (Locator::dynamicsSystem::has_value())
		{
			const bool drawSunk = config.drawSunkPhysicsObjects;
			Locator::dynamicsSystem::value().ForEachEntry([&out, drawSunk](const ecs::PhysicsEntry& entry) {
				if (entry.entity == entt::null || entry.body.resting)
				{
					return;
				}
				// a flying one wholly under the water is out of the map and the physics list skips it: nothing draws it
				if (entry.body.Centre().y > -entry.body.Radius())
				{
					out.objectList.physics.push_back(entry.entity);
				}
				else if (!drawSunk)
				{
					out.objectList.sunk.push_back(entry.entity);
				}
			});
		}
		out.objectList.tail = graphics::object_order::TailOrder(roots, {}, ecs::villager_soul::NewestFirst(), chests,
		                                                        ecs::ground_marks::NewestFirst());
	}
	// the trees drawn out of the map this frame (in a hand, flying, held by a creature, carried) take less snow from now on
	ecs::snow_levels::NoteTreesDrawnOutOfMap(Locator::entitiesRegistry::value(), out.objectList.physics);
	_objectListFrame = &out.objectList;
	// the knocked town's read-out over the buildings whose Draw the object list called this frame
	FillKnockReadout(
	    _objectListRan ? Locator::objectDrawListSystem::value().DrawnThisFrame() : std::span<const entt::entity> {},
	    graphics::billboard::CameraFrame::From(Locator::camera::value()), out.overlay.knockReadout, out.overlay.knockRows);
	TheIntroSystem().FillFrame(out.overlay.introLight);

	profiler.Begin(Profiler::Stage::SceneDraw); // ends in DrawFrame, after DrawScene (as the scope did)
	// the draw reads a copy of the camera and the frame's clocks, taken here: nothing between this and the
	// draw moves them (PreDraw, which may read the live ones, gets the same values)
	Locator::camera::value().CopyViewTo(*_drawCamera);
	out.camera = _drawCamera.get();
	// which fires are drawn this frame, in what order, and the origin their steam and smoke keep: those of the objects
	// whose draws run, as they run, sorted by the draw's camera
	if (Locator::fireSystem::has_value())
	{
		ecs::fire::graphic::PrepareDraw(out.objectList, _drawCamera->GetOrigin());
	}
	out.hand.position.reset();
	if (Locator::handSystem::has_value())
	{
		out.hand.position = glm::vec3(Locator::handSystem::value().GetHandMatrix()[3]);
	}
	out.time = timeMs; // TODO(#481): get actual time
	out.timeOfDay = Locator::skySystem::value().GetTime();
	out.clock = {.frameGameMs = game_clock::FrameGameMs(),
	             .turn = game_clock::Turn(),
	             .turnFraction = game_clock::TurnFraction(),
	             .paused = game_clock::IsPaused()};
	out.bumpMapStrength = config.bumpMapStrength;
	out.smallBumpMapStrength = config.smallBumpMapStrength;
	out.drawSky = config.drawSky;
	out.drawWater = config.drawWater;
	out.drawIsland = config.drawIsland;
	out.drawEntities = config.drawEntities;
	out.drawSprites = config.drawSprites;
	out.drawBoundingBoxes = config.drawBoundingBoxes;
	out.wireframe = config.wireframe;
	const auto& screenshots = Locator::screenshotRequest::value();
	out.frameCount = screenshots.Frame();
	out.screenshot = screenshots.Due();
	out.screenPictures.clear();
	// The challenges' pictures asked for this frame, of the frame now drawn
	for (auto& name : ecs::challenge_pictures::TakeRequests(Locator::entitiesRegistry::value()))
	{
		out.screenPictures.push_back({.name = std::move(name), .picturer = &ecs::challenge_pictures::Take});
	}
	// and the saved games' pictures asked while the frame on the screen was not kept, of the frame now drawn
	const bool savePictures = Locator::savePictures::has_value();
	if (savePictures)
	{
		for (auto& name : Locator::savePictures::value().TakeRequests())
		{
			out.screenPictures.push_back({.name = std::move(name), .picturer = &ecs::systems::SaveScreenPicture});
		}
	}
	// While a running script can take a challenge's picture, every frame is kept aside, as a picture is of the frame
	// already on the screen when the script runs
	const auto snapshotTask =
	    Locator::vm::has_value() ? ecs::challenge_pictures::TaskThatSnapshots(_snapshotScripts, Locator::vm::value().GetTasks())
	                             : std::nullopt;
	// While a profile is kept every frame is kept aside too, as the saved games' pictures are of the frame on the screen as
	// the player goes into the temple or the game saves by itself
	const bool keptForSaves = savePictures && Locator::savePictures::value().KeepsShownFrames();
	out.keepShownFrame = snapshotTask.has_value() || keptForSaves;
	// (debug) a line each time the keeping starts or stops, with the task that starts it
	if (out.keepShownFrame != std::exchange(_keepingShownFrames, out.keepShownFrame))
	{
		if (!snapshotTask.has_value() && keptForSaves)
		{
			spdlog::get("game")->debug("Save pictures: the frames are kept from frame {}, a profile is kept", out.frameCount);
		}
		else if (snapshotTask.has_value())
		{
			const auto& task = Locator::vm::value().GetTasks().at(*snapshotTask);
			spdlog::get("game")->debug("Challenge pictures: the frames are kept from frame {}, task {} running {} can take one",
			                           out.frameCount, *snapshotTask, task.name);
		}
		else
		{
			spdlog::get("game")->debug("Challenge pictures: the frames are no longer kept from frame {}, no running script can "
			                           "take one",
			                           out.frameCount);
		}
	}
	{
		auto preDraw = profiler.BeginScoped(Profiler::Stage::PreDraw);
		Locator::rendererInterface::value().PreDraw(out.Desc(Locator::entitiesRegistry::value()));
	}
}

void Game::DrawFrame(const engine::FrameSnapshot& in)
{
	auto& profiler = Locator::profiler::value();
	{
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw); // what the draw thread will run
		auto desc = in.Desc(Locator::entitiesRegistry::value());
		// the interface's panels go in the end of the frame, between the help text and the fade
		if (_interface && Locator::windowing::has_value())
		{
			const auto size = static_cast<glm::u16vec2>(Locator::windowing::value().GetSize());
			desc.interfaceLayers = {
			    .fightPanel = [this, size]() { _interface->DrawFightPanelLayer(size); },
			    .scriptTexts = [this, size]() { _interface->DrawScriptTextLayer(size); },
			    .creaturePanel = [this, size]() { _interface->DrawCreaturePanelLayer(size); },
			};
		}
		Locator::rendererInterface::value().DrawScene(desc);
	}
	profiler.End(Profiler::Stage::SceneDraw); // begun in LogicFrame

	// the in-game menus over the scene, timed by the game's tick count as the original's pointer is. (pending) the
	// real mouse read here: into the snapshot
	if (_interface && Locator::windowing::has_value())
	{
		// the game's cursor (the dispatched motion, OPENBLACK_MOUSE_AT, a hand demo or an input recording), live as in a
		// playback, not the real pointer's own state, which runs ahead of the events
		const glm::ivec2 mouse = input::FixedMouse().value_or(input::GameCursorRef());
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		// Not under the ignored real input: the real mouse over a debug window would put a pointer in the shots, and the
		// checks' shots keep no pointer for the hidden hand
		_interface->Draw(static_cast<glm::u16vec2>(Locator::windowing::value().GetSize()), mouse, game_clock::TickCount(),
		                 !input::IgnoreRealInput() && (Locator::debugGui::value().IsMouseOverWindow() ||
		                                               Locator::handSystem::value().ShowsHiddenHandPointer()));
	}

	{
		auto section = profiler.BeginScoped(Profiler::Stage::GuiDraw);
		const engine::gpu::ScopedPhase drawPhase(engine::gpu::Phase::Draw);
		// The pictures kept in memory are of the screen as the player sees it: no debug windows in them
		for (const auto& request : in.screenPictures)
		{
			Locator::rendererInterface::value().RequestScreenPicture(request.name, request.picturer);
		}
		const bool picturesAsked = !in.screenPictures.empty();
		// Skip drawing Debug UI for screenshots
		if (in.screenshot.has_value())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Requesting a screenshot at frame {}...", in.frameCount);
			Locator::rendererInterface::value().RequestScreenshot(*in.screenshot);
			// test hook OPENBLACK_SCREENSHOT_GUI=1: the debug UI (menus, windows) in the screenshot too
			if (std::getenv("OPENBLACK_SCREENSHOT_GUI") != nullptr && !picturesAsked)
			{
				Locator::debugGui::value().Draw();
			}
		}
		else if (!picturesAsked)
		{
			// (pending) ImGui is still drawn from the logic's state: the single-thread mode only, until its draw data
			// is copied
			Locator::debugGui::value().Draw();
		}
		// The frame kept aside for a later turn's picture: the window shows it pixel for pixel, the debug windows over it
		if (in.keepShownFrame)
		{
			Locator::rendererInterface::value().KeepShownFrame();
		}
	}

	if (std::getenv("OPENBLACK_DRAW_STATS") != nullptr && (in.frameCount % 30 == 0 || in.frameCount < 8))
	{
		const auto* stats = bgfx::getStats();
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "DRAWSTATS frame {} draws {}", in.frameCount, stats->numDraw);
	}
}
