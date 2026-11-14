/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FrameBody.h"

#include <optional>

#include "Camera/Camera.h"
#include "Creature/CreatureLightbulb.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Animations.h"
#include "ECS/CarriedProps.h"
#include "ECS/Components/CreatureLightbulbs.h"
#include "ECS/CreatureLoop.h"
#include "ECS/CreaturePose.h"
#include "ECS/DesignedScenery.h"
#include "ECS/DiscipleMarks.h"
#include "ECS/FireFlies.h"
#include "ECS/FishShoals.h"
#include "ECS/FloatingNumber.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Scaffolds.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/SuperVillager.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ExplosionSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/HighDetailSystemInterface.h"
#include "ECS/Systems/IntroSystemInterface.h"
#include "ECS/Systems/RewardSystemInterface.h"
#include "ECS/Systems/SharkSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/VegetationInterface.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Trees.h"
#include "ECS/VillagerAnimations.h"
#include "GameClock.h"
#include "GameServices.h"
#include "Gui/CinemaBars.h"
#include "Locator.h"
#include "Particles/Rules/Forest.h"
#include "Particles/TownBelief.h"
#include "Profiler.h"
#include "Video/VideoPlayer.h"
#include "Windowing/WindowingInterface.h"

namespace openblack::simulation
{

void PlayFrameEffects(std::chrono::microseconds deltaTime)
{
	auto& profiler = Locator::profiler::value();
	auto& camera = Locator::camera::value();
	// with the landscape's draw: the physics objects drawn between their last two turn poses (before the
	// trees' bending after the hand, which takes them as sources, and before the drawing), and the dust
	// the profile's "Frame Updaters" (no return before its End below: Profiler::End checks the level)
	profiler.Begin(Profiler::Stage::FrameUpdaters);
	Locator::dynamicsSystem::value().UpdateFrame(game_clock::TurnFraction(), game_clock::FrameGameSeconds());

	// the wind the ripe fields and the trees sway in, once per frame like the trees' pre-draw, by the frame's whole game
	// ms (none in pause)
	{
		auto sway = profiler.BeginScoped(Profiler::Stage::VegetationUpdate);
		Locator::vegetation::value().Update(
		    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())));
	}
	// the trees' draw: their brightness this frame and the rustle of the tall ones by the camera (their bends come
	// after the hand)
	ecs::UpdateTrees(std::chrono::duration<float>(deltaTime).count());

	// Fireflies: none drawn this frame until the object draw list draws it (its Draw steps it)
	ecs::HideFireFlies();
	// the script highlights: the hidden ones' meshes taken away and given back (their pose is set by their Draw, from
	// the object draw list)
	ecs::script_highlight::SyncMeshes();
	// the town desire flags over the stores: a new store takes them (they step only as the object draw list draws them)
	Locator::townDesireSystem::value().UpdateFlags();

	// Water rings: the frame's game time, in milliseconds
	Locator::waterRingSystem::value().Update(
	    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())));
	// the static the creatures' eyes fizz through slides on, once a frame as the frame starts, by the frame's game time
	TheCreatureFizzSystem().UpdateFrame(game_clock::FrameGameSeconds());
	// the designed scenery of Land 3 (waterfall) and Land 4 (ark, dinosaur), by land number
	ecs::designed_scenery::Update(static_cast<float>(game_clock::FrameGameMs()));
	// The marks on the ground, the blasts' rubble and the uprooted trees' craters (just before the smoky stuff): fade
	// and go
	{
		auto explosions = profiler.BeginScoped(Profiler::Stage::ExplosionUpdate);
		Locator::explosionSystem::value().Update(static_cast<float>(game_clock::FrameGameMs()));
	}
	// the reward chests from the sky fall and thump down in the world's draw, by the frame's game time
	Locator::rewardSystem::value().Update(static_cast<float>(game_clock::FrameGameMs()));
	// the missionaries' boat before its draw, the smoky stuff, after its draw (ecs/MissionaryBoat.h). The smoky stuff also
	// moves the smoke an object leaves when it goes (ecs/DisappearSmoke.h), in game time, boat or not.
	ecs::missionary_boat::Update(static_cast<float>(game_clock::FrameGameMs()));
	// in the landscape's draw, before the SuperVillager loop: the intro hand's grip, from the hand as the last frame
	// left it; then, just before the boat's after-draw, and the light's Z-object callback of this frame: the intro
	// light, the debug camera's points and the intro hand (Locator::introSystem). (approximate) after the boat's update
	// here. The light's CRT Random draws: (approximate, pending): the original draws in the Z drain, after the clouds,
	// night lights and smoke; to move to the end of Renderer::PreDraw
	TheIntroSystem().Update(game_clock::FrameGameMs());

	// in the landscape's draw: the SuperVillagers go without a script wide screen; the others take the on-screen
	// test and set this frame's fade and yaw parameters of the smooth drawing (ecs/SuperVillager.h)
	TheHighDetailSystem().Update();
	// Villagers and animals drawn between turns, turning smoothly, on the slope (ecs/MobileDrawing.h)
	ecs::UpdateMobileDrawing(game_clock::TurnFraction(), static_cast<float>(game_clock::FrameGameMs()));
	// after the villagers' draw: a SuperVillager with feature 7 at the intro hand's grip
	ecs::super_villager::FollowHand();
	// the disciples' glows, and the symbol of the one in the hand or under the pointer, over the villagers where they
	// are drawn this frame (as the original draws them with the villager's own drawing)
	ecs::disciple_marks::Update(camera.GetOrigin(), Locator::windowing::has_value()
	                                                    ? static_cast<float>(Locator::windowing::value().GetSize().x)
	                                                    : 0.0f);
	// Skeletal animation of villagers and animals (ecs/Animations.h), in milliseconds of game time
	ecs::UpdateVillagerAnimations();
	ecs::UpdateAnimalAnimations();
	ecs::UpdateAnimations(static_cast<float>(game_clock::FrameGameMs()));
	ecs::UpdateCarriedProps();
	// the scaffold the local hand holds, as it is drawn: its phantom's appear and disappear sounds (ECS/Scaffolds.h)
	if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value())
	{
		ecs::scaffolds::PlayHeldPhantomSound(*held);
	}
	// the creatures between turns, which stand still with the world while it is paused in the citadel
	// (ECS/CreatureLoop.h)
	if (!ecs::creature_loop::FrozenInCitadel(game_clock::IsPaused(), game_clock::IsInsideCitadel()))
	{
		ecs::creature_loop::UpdateFrame(game_clock::TurnFraction(), game_clock::FrameGameMs(), profiler);
	}
	// every frame of the world's view (not inside the citadel nor during the falling spell's film) places the
	// creatures' body models where they stand, which a turn's screen test of a creature reads (ECS/CreaturePose.h)
	if (game_clock::SequenceMode() == game_clock::k_SequenceModeNone)
	{
		ecs::creature_pose::PlaceBodies(Locator::entitiesRegistry::value());
	}
	// the sharks drawn between turns, heading, wake rings (the shark system)
	Locator::sharkSystem::value().Update(
	    std::chrono::duration<float, std::milli>(static_cast<float>(game_clock::FrameGameMs())), game_clock::TurnFraction());

	// FishFarm shoals, moved with the frame's game time
	ecs::UpdateFishShoals(game_clock::FrameGameSeconds(), camera.GetOrigin());
	// after the fish and the boat's after-draw: each SuperVillager's eyes after its body, on screen only
	// (ecs/SuperVillagerEyes.h); the swim rings' Random(0, 2 pi) of the driver loop came before them, in Update.
	// Neither argument is read by ours, which steps the eyes by the frame's game milliseconds
	TheHighDetailSystem().PlaceEyes(game_clock::VisualMs(), camera.GetViewProjectionMatrix());
	// the town centres' draw: the town belief symbols' PSys step, once a rendered frame
	particles::town_belief::Step();
	// the floating numbers rise and fade by the frame's game time, and go once their time is up
	ecs::floating_number::StepAll(Locator::entitiesRegistry::value(), game_clock::FrameGameSeconds());
	// the creatures' light bulbs live on by the frame's game time as the world's view draws them (not inside the citadel
	// nor during the falling spell's film), and go once their life is over; how each is drawn this frame is kept for the
	// renderer
	{
		const bool worldDrawn = game_clock::SequenceMode() == game_clock::k_SequenceModeNone;
		const auto milliseconds = game_clock::FrameGameMs();
		Locator::entitiesRegistry::value().Each<ecs::components::CreatureLightbulbs>(
		    [worldDrawn, milliseconds](entt::entity /*creature*/, ecs::components::CreatureLightbulbs& bulbs) {
			    if (worldDrawn)
			    {
				    bulbs.looks = creature_lightbulb::StepAll(bulbs.bulbs, milliseconds);
			    }
			    else
			    {
				    bulbs.looks.clear();
			    }
		    });
	}
	// the forest miracle's camera particle's draw: its drawn frame is its camera path's time, after the camera's update
	particles::forest_camera::UpdateFrame(game_clock::TurnFraction());

	// from the help system's Draw3D: the cinema bars slide with the game time of this frame, or the wall clock's inside
	// the temple with the game paused
	CinematicDirector().Update(std::chrono::duration<float, std::milli>(
	    gui::CinemaBarsFrameMs(game_clock::IsInsideCitadel(), game_clock::IsPaused(),
	                           static_cast<float>(game_clock::FrameGameMs()), static_cast<float>(game_clock::FrameRealMs()))));
	// the scripts' words on the screen fade by the game's time of the frame, and stand still while a video plays
	if (!video::IsPlaying())
	{
		CinematicDirector().UpdateTexts(static_cast<int32_t>(game_clock::FrameGameMs()));
	}
	profiler.End(Profiler::Stage::FrameUpdaters);
}

} // namespace openblack::simulation
