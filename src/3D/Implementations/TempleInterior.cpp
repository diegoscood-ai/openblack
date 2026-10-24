/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TempleInterior.h"

#include <cmath>
#include <cstdio>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fmt/format.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <spdlog/spdlog.h>

#include "3D/AffineMatrix.h"
#include "3D/CameraPath.h"
#include "3D/CreatureBody.h"
#include "3D/CreatureCaveEffects.h"
#include "3D/CreatureCaveTrophies.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/Light.h"
#include "3D/TempleChallengePick.h"
#include "3D/TemplePool.h"
#include "3D/TempleScrolls.h"
#include "3D/TempleSigns.h"
#include "3D/TempleSounds.h"
#include "3D/TempleWaterfall.h"
#include "Audio/Audio.h"
#include "CHLApi.h"
#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Camera/TempleCameraModel.h"
#include "Common/EventManager.h"
#include "Common/GameRandom.h"
#include "Common/MachineClock.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureRig.h"
#include "Creature/TattooEditor.h"
#include "ECS/Archetypes/CreatureCopyArchetype.h"
#include "ECS/Archetypes/GlowArchetype.h"
#include "ECS/ChallengeRecords.h"
#include "ECS/Components/ChallengeRecord.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Influence/Influence.h"
#include "ECS/LeashHandTie.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/GameStatsSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/Implementations/RenderingSystem.h"
#include "ECS/Systems/Implementations/RenderingSystemTemple.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TattooEditorSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Gui/GameInterface.h"
#include "Gui/GameMenu.h"
#include "Gui/ScreenFade.h"
#include "Help/DialogueText.h"
#include "Input/GameActionMapInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Temple/OptionsRoomRules.h"
#include "Temple/TempleHelpScripts.h"
#include "Temple/TempleKeys.h"
#include "Temple/TempleRoomAccess.h"
#include "Video/FallingSpellVideo.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;

using Indoors = TempleRoom;
const std::unordered_multimap<Indoors, std::string_view> k_TempleInteriorParts {
    {Indoors::Challenge, "challenge_l3d"},
    // {"challengelo_l3d", Indoors::ChallengeLO},
    {Indoors::Challenge, "challengedome_l3d"},
    {Indoors::Challenge, "challengefloor_l3d"},
    // {"challengefloorlo_l3d", Indoors::ChallengeFloorLO},
    {
        Indoors::CreatureCave,
        "creature_l3d",
    },
    // {"creaturelo_l3d", Indoors::CreatureCaveLO},
    {Indoors::CreatureCave, "creaturewater_l3d"},
    // {"creaturewaterlo_l3d", Indoors::CreatureCaveWaterLO},
    {Indoors::Credits, "credits_l3d"},
    // {"creditslo_l3d", Indoors::CreditsLO},
    {Indoors::Credits, "creditsdome_l3d"},
    {Indoors::Credits, "creditsfloor_l3d"},
    // {"creditsfloorlo_l3d", Indoors::CreditsFloorLO},
    {Indoors::Main, "main_l3d"},
    // {"mainlo_l3d", Indoors::MainLO},
    {Indoors::Main, "mainfloor_l3d"},
    // {"mainfloorlo_l3d", Indoors::MainFloorLO},
    {Indoors::Main, "mainwater_l3d"},
    // {"mainwaterlo_l3d", Indoors::MainWaterLO},
    // {"movement_l3d", Indoors::Movement}, // Navmesh
    {Indoors::Multi, "multi_l3d"},
    // {"multilo_l3d", Indoors::MultiLO},
    {Indoors::Multi, "multidome_l3d"},
    {Indoors::Multi, "multifloor_l3d"},
    // {"multifloorlo_l3d", Indoors::MultiFloorLO},
    {Indoors::Options, "options_l3d"},
    // {"optionslo_l3d", Indoors::OptionsLO},
    {Indoors::Options, "optionsdome_l3d"},
    {Indoors::Options, "optionsfloor_l3d"},
    // {"optionsfloorlo_l3d", Indoors::OptionsFloorLO},
    {Indoors::SaveGame, "savegame_l3d"},
    // {"savegamelo_l3d", Indoors::SaveGameLO},
    {Indoors::SaveGame, "savegamedome_l3d"},
    {Indoors::SaveGame, "savegamefloor_l3d"},
    // {"savegamefloorlo_l3d", Indoors::SaveGameFloorLO},
};

const std::unordered_map<Indoors, std::string_view> k_TempleInteriorGlows {
    {Indoors::Challenge, "challenge"},   //
    {Indoors::CreatureCave, "creature"}, //
    {Indoors::Credits, "credits"},       //
    {Indoors::Main, "main"},             //
    {Indoors::Multi, "multi"},           //
    {Indoors::Options, "options"},       //
    {Indoors::SaveGame, "savegame"},     //
};

inline void addRoomToRegistry(std::string_view assetName, Indoors templeRoom, glm::vec3 position, glm::mat3 rotation,
                              glm::vec3 scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const entt::id_type meshId = entt::hashed_string(fmt::format("temple/interior/{}", assetName).c_str()).value();
	// "<room>_l3d" is the room itself and "<room>floor_l3d" its floor
	const auto roomName = k_TempleInteriorGlows.at(templeRoom);
	auto mesh = ecs::components::TempleInteriorMesh::Other;
	if (assetName == fmt::format("{}_l3d", roomName))
	{
		mesh = ecs::components::TempleInteriorMesh::Room;
	}
	else if (assetName == fmt::format("{}floor_l3d", roomName))
	{
		mesh = ecs::components::TempleInteriorMesh::Floor;
	}
	else if (assetName == fmt::format("{}water_l3d", roomName) && templeRoom == Indoors::CreatureCave)
	{
		mesh = ecs::components::TempleInteriorMesh::Water;
	}
	else if (assetName == fmt::format("{}water_l3d", roomName) && templeRoom == Indoors::Main)
	{
		mesh = ecs::components::TempleInteriorMesh::Pool;
	}
	auto entity = registry.Create();
	registry.Assign<ecs::components::TempleInteriorPart>(entity, templeRoom, mesh);
	registry.Assign<ecs::components::Transform>(entity, position, rotation, scale);
	registry.Assign<ecs::components::Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
}

inline void addGlowsToRegistry(Indoors templeRoom)
{
	const auto& glowManager = Locator::resources::value().GetGlows();
	const auto glowId =
	    entt::hashed_string(fmt::format("temple/interior/glow/{}", k_TempleInteriorGlows.at(templeRoom)).c_str()).value();
	const auto glows = glowManager.Handle(glowId);
	for (const auto& glow : glows->emitters)
	{
		ecs::archetypes::GlowArchetype::Create(glow, templeRoom);
	}
}

namespace
{
/// How far through its swing the main room's door is before the room behind it is drawn
constexpr float k_MainRoomOpenSwing = 0.01f;

/// The colour the temple fades to before the player leaves it for the island, and out of as they arrive
constexpr uint32_t k_White = 0x00FFFFFF;

/// Plays one of the temple's sounds, which its doors, buttons, scrolls and camera ask for
void PlayTempleSound(const temple_sounds::Options& options)
{
	audio::PlayOptions play;
	static_cast<temple_sounds::Options&>(play) = options;
	audio::PlaySoundEffect(play);
}

/// The temple's fade: a colour over the whole screen, which it shares with the falling spell's film and which goes to
/// the screen's fade every frame while the player is in the temple
gui::ScreenFade& Fade()
{
	return video::GetFallingSpell().GetScreenFade();
}

/// A place the temple's movement.l3d marks
std::optional<glm::vec3> CreatureCaveMovementPlace(size_t index)
{
	constexpr entt::id_type k_Movement = entt::hashed_string("temple/interior/movement_l3d").value();
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_Movement) || meshes.Handle(k_Movement)->GetExtraMetrics().size() <= index)
	{
		return std::nullopt;
	}
	return glm::vec3(meshes.Handle(k_Movement)->GetExtraMetrics()[index][3]);
}

/// The fire of the creature's room, the second place movement.l3d marks
constexpr size_t k_CreatureCaveFirePlace = 1;
std::optional<glm::vec3> CreatureCaveFire()
{
	return CreatureCaveMovementPlace(k_CreatureCaveFirePlace);
}

/// Whether the creature's room is to show the player's creature: so from the start of a game, until a script says
/// otherwise
bool CreatureShown()
{
	return Locator::creatureCaveSystem::has_value() && Locator::creatureCaveSystem::value().IsCreatureShown();
}

/// The copy of the player's creature that stands by the creature's room's fire, when the player has a creature with a
/// body. It stands between the first and the third places movement.l3d marks
void MakeCreatureCopy(const std::optional<glm::vec3>& fire)
{
	if (!Locator::creatureCaveSystem::has_value() || !fire.has_value())
	{
		return;
	}
	const auto creature = Locator::creatureCaveSystem::value().GetCreature();
	const auto first = CreatureCaveMovementPlace(0);
	const auto third = CreatureCaveMovementPlace(2);
	if (!creature.has_value() || !first.has_value() || !third.has_value())
	{
		return;
	}
	static_cast<void>(
	    ecs::archetypes::CreatureCopyArchetype::Create(*creature, {.first = *first, .third = *third, .fire = *fire}));
}

/// The copy goes with the rooms
void DestroyCreatureCopies()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> copies;
	registry.Each<const ecs::components::CreatureCopy>(
	    [&copies](const entt::entity entity, auto&&...) { copies.push_back(entity); });
	for (const auto entity : copies)
	{
		registry.Destroy(entity);
	}
}

/// The places for tattoos on the creature's room's copy of the creature, as this frame poses it, in the world: none while
/// there is no copy or its species has none
std::vector<creature_tattoo_editor::SitePoint> CopyTattooPoints()
{
	std::vector<creature_tattoo_editor::SitePoint> points;
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return points;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	registry.Each<const ecs::components::CreatureCopy, const ecs::components::CreatureMorph,
	              const ecs::components::CreatureAnimation>(
	    [&](const entt::entity entity, const ecs::components::CreatureCopy& copy, const ecs::components::CreatureMorph& shape,
	        const ecs::components::CreatureAnimation& animation) {
		    const auto rigId = creature::GetRigId(copy.species);
		    if (rigs.Contains(rigId))
		    {
			    points = creature::PosedTattooPoints(rigs.Handle(rigId)->tattooPoints, shape.drawn, animation.boneMatrices,
			                                         ecs::DrawnBodyModel(registry, entity));
		    }
	    });
	return points;
}

/// While the tattoo editor is open, the place on the copy's body the pointer is over: the places as the copy was posed
/// and drawn this frame, seen from the camera, nearest the pointer in screen pixels
void HoverCopyTattooPoints()
{
	if (!Locator::tattooEditorSystem::has_value() || !Locator::tattooEditorSystem::value().IsOpen() ||
	    !Locator::camera::has_value() || !Locator::windowing::has_value() || !Locator::gameActionSystem::has_value())
	{
		return;
	}
	const auto& camera = Locator::camera::value();
	const creature_tattoo_editor::SiteView view {
	    .eye = camera.GetEye(),
	    .worldToClip = camera.GetViewProjectionMatrix(),
	    .nearPlane = camera.GetDrawnNearClip(Locator::config::value().cameraNearClip),
	    .screenSize = Locator::windowing::value().GetSize(),
	};
	const auto pointer = glm::ivec2(Locator::gameActionSystem::value().GetMousePosition());
	Locator::tattooEditorSystem::value().Hover(creature_tattoo_editor::SitesOnScreen(CopyTattooPoints(), view), pointer);
}

/// The copy's frame in the creature's room, after it was drawn: it gets up for the tattoo editor and looks at the pointer
/// while a mouse button is held, and rests again by the fire once the editor has closed, turning to face the editor's
/// eye and then the fire
void StepCreatureCopy(creature_tattoo_editor::CopyLife& life)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	creature_tattoo_editor::CopyFrame frame {.ticks = game_clock::TickCount(),
	                                         .frameMs = static_cast<int32_t>(game_clock::FrameRealMs())};
	if (Locator::tattooEditorSystem::has_value() && Locator::tattooEditorSystem::value().IsOpen())
	{
		const auto& view = Locator::tattooEditorSystem::value().GetView();
		frame.editorOpen = true;
		frame.eye = view.eye;
		frame.focus = view.focus;
	}
	if (Locator::gameActionSystem::has_value())
	{
		// any of the three buttons, wherever their events went; the point under the pointer a little into the screen
		using input::MouseButton;
		const auto& actions = Locator::gameActionSystem::value();
		frame.leftHeld = actions.IsMouseButtonHeld(MouseButton::Left);
		frame.buttonHeld =
		    frame.leftHeld || actions.IsMouseButtonHeld(MouseButton::Middle) || actions.IsMouseButtonHeld(MouseButton::Right);
		if (Locator::camera::has_value() && Locator::windowing::has_value())
		{
			const auto& camera = Locator::camera::value();
			const auto plane =
			    screen_point::NearPlaneOf(Locator::windowing::value().GetSize(), camera.GetHorizontalFieldOfView(),
			                              camera.GetDrawnNearClip(Locator::config::value().cameraNearClip));
			frame.pointer =
			    screen_point::PixelPoint(affine::WorldToCamera(camera.GetOrigin(), camera.GetFocus()), camera.GetEye(), plane,
			                             glm::ivec2(actions.GetMousePosition()), creature_tattoo_editor::k_CopyLookDepth);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	registry.Each<ecs::components::CreatureCopy, ecs::components::CreatureAnimation, const ecs::components::Transform>(
	    [&](const entt::entity entity, ecs::components::CreatureCopy& copy, ecs::components::CreatureAnimation& animation,
	        const ecs::components::Transform& transform) {
		    frame.sleeping = copy.sleeping;
		    frame.heading = copy.heading;
		    frame.turning = copy.turn.has_value() || copy.turnRequested.has_value();
		    frame.place = transform.position;
		    frame.fire = copy.fire;
		    const auto rigId = creature::GetRigId(copy.species);
		    const creature::CreatureRig* rig = rigs.Contains(rigId) ? &*rigs.Handle(rigId) : nullptr;
		    const auto hasAnimation = [rig](size_t index) {
			    return rig != nullptr && rig->GetAnimation(creature::CreatureRig::Mesh::Base, index) != nullptr;
		    };
		    const auto stepped =
		        creature_tattoo_editor::CopyStep(life, animation.body, animation.gesture, animation.lookAt, frame, hasAnimation,
		                                         [](int32_t n) { return game_random::LocalRand(n); });
		    if (stepped.restFlag.has_value())
		    {
			    copy.restFlag = *stepped.restFlag;
		    }
		    if (stepped.turn.has_value())
		    {
			    copy.turnRequested = stepped.turn;
		    }
		    // its eyes look where it does
		    if (auto* eyes = registry.TryGet<ecs::components::CreatureEyes>(entity); eyes != nullptr)
		    {
			    eyes->lookAt = animation.lookAt;
		    }
	    });
}

/// The belts' and medals' meshes, data/citadel/icons/<icon>.l3d as "temple/icons/<icon>", and the texture they are all
/// drawn with, read once into the caches; one that can't be read is left out
void LoadCaveIcons()
{
	using filesystem::Path;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshes = resources.GetMeshes();
	for (uint32_t i = 0; i < CreatureCaveTrophies::k_IconCount; ++i)
	{
		const auto icon = CreatureCaveTrophies::IconName(i);
		const auto id = entt::hashed_string(fmt::format("temple/icons/{}", icon).c_str()).value();
		try
		{
			if (!meshes.Contains(id))
			{
				meshes.Load(id, resources::L3DLoader::FromDiskTag {},
				            fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon));
			}
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The creature room's {} can't be read: {}", icon, e.what());
		}
	}
	auto& textures = resources.GetTextures();
	try
	{
		if (!textures.Contains(TempleCaveTrophy::k_Texture))
		{
			using resources::Texture2DLoader;
			textures.Load(TempleCaveTrophy::k_Texture, Texture2DLoader::FromColourAlphaTag {}, "temple/icons",
			              Texture2DLoader::ColourAlphaDesc {
			                  .colour = fileSystem.GetPath<Path::Textures>() / "icons.raw",
			                  .alpha = fileSystem.GetPath<Path::Textures>() / "iconsa.raw",
			                  .wrap = graphics::Wrapping::Repeat,
			              });
		}
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "The creature room's icons can't be read: {}", e.what());
	}
}
} // namespace

TempleInterior::TempleInterior()
    : _doors(PlayTempleSound)
    , _toggles(PlayTempleSound)
{
}

namespace
{
/// What the rooms' scrolls tell of the game: made up, but for what openblack keeps
TempleScrolls::Facts GatherScrollFacts()
{
	auto facts = TempleScrolls::Facts::Mock();
	// The world scroll: the game's statistics of the local player
	if (Locator::playerSystem::has_value() && Locator::gameStatsSystem::has_value())
	{
		const auto world = ecs::player_statistics::World(Locator::playerSystem::value().GetLocalPlayer());
		facts.population = world.population;
		facts.believersPercent = world.believersPercent;
		facts.malePercent = world.malePercent;
		facts.deaths = world.deaths;
		facts.births = world.births;
		facts.sacrifices = world.sacrifices;
		facts.buildings = world.buildings;
		facts.wonders = world.wonders;
		facts.disciples = world.disciples;
		facts.discipleKinds = world.discipleKinds;
	}
	// The challenge room's scroll lists the player's challenges as their scripts recorded them
	auto listing = ecs::challenge_records::List(Locator::entitiesRegistry::value());
	facts.challengesDiscovered = listing.discovered;
	facts.challengesCompleted = listing.completed;
	facts.challenges = std::move(listing.titles);
	// The save game room's scroll: the time the player played in all their games, from what the profile keeps and the
	// seconds since the first land was loaded, on the system clock, and how many times they saved and loaded
	if (Locator::playerProfileSystem::has_value())
	{
		const auto& profiles = Locator::playerProfileSystem::value();
		facts.timePlayed = std::chrono::seconds(profiles.TimePlayed(machine_clock::WallTime()));
		if (const auto profile = profiles.GetCurrentProfile(); profile.has_value())
		{
			facts.saveCount = static_cast<int32_t>(profile->statistics.saves);
			facts.loadCount = static_cast<int32_t>(profile->statistics.loads);
		}
	}
	// The creature's scrolls tell of the player's own creature, and are blank without one
	if (Locator::creatureCaveSystem::has_value())
	{
		const auto snapshot = Locator::creatureCaveSystem::value().Snapshot();
		facts.creature = snapshot.has_value() ? std::optional(creature_cave::FactsOf(*snapshot)) : std::nullopt;
		// What the creature counts of its life: the people, animals and creatures it killed, its fights and the fights
		// it won, its poos and the mushrooms it ate
		if (facts.creature.has_value() && Locator::playerSystem::has_value())
		{
			const auto tallies = ecs::player_statistics::CreatureTalliesOf(Locator::playerSystem::value().GetLocalPlayer());
			facts.creature->tallies = {
			    static_cast<int32_t>(tallies.peopleKilled),    static_cast<int32_t>(tallies.animalsKilled),
			    static_cast<int32_t>(tallies.creaturesKilled), static_cast<int32_t>(tallies.battlesFought),
			    static_cast<int32_t>(tallies.battlesWon),      static_cast<int32_t>(tallies.poos),
			    static_cast<int32_t>(tallies.mushroomsEaten),
			};
		}
	}
	// The library's help scrolls list the tips that the player read on the signs, by their categories
	if (Locator::scriptState::has_value())
	{
		for (uint32_t category = 0; category < facts.libraryHelp.size(); ++category)
		{
			const auto read = ecs::script_highlight::did_you_know_read::In(static_cast<DykCategory>(category));
			facts.libraryHelp.at(category).assign(read.begin(), read.end());
		}
	}
	// The library's last scroll shows the help texts that the scripts showed
	if (Locator::helpTextSystem::has_value())
	{
		if (const auto* dialogue = std::as_const(Locator::helpTextSystem::value()).GetDialogue(); dialogue != nullptr)
		{
			facts.history = help::StoryHistoryView {
			    .back = [dialogue](int32_t linesBack) { return dialogue->GetHistory(linesBack); },
			    .count = dialogue->GetHistoryCount(),
			};
		}
	}
	return facts;
}
} // namespace

void TempleInterior::SetInterface(gui::GameInterface* interface)
{
	_interface = interface;
	_scrolls.reset();
	_signs.reset();
	_text.clear();
	_scrollPictures.clear();
	if (_active)
	{
		CreateScrolls();
	}
}

void TempleInterior::CreateScrolls()
{
	if (_interface == nullptr)
	{
		return;
	}
	// The parchment every scroll is written on, read the first time the rooms are set up and kept in the byte cache
	const auto& parchment =
	    resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                Locator::filesystem::value().GetPath<filesystem::Path::Textures>() / "ChallengeScroll.raw",
	                                "temple scrolls' parchment");
	_scrolls = std::make_unique<TempleScrolls>(_interface->GetTexts(), _interface->GetFont(), parchment, PlayTempleSound,
	                                           [] { return audio::TickCount(); });
	_scrolls->Create(GatherScrollFacts());
	_signs = std::make_unique<TempleSigns>(_interface->GetTexts(), _interface->GetFont());

	// The main room finds the buttons of what the map shows as it is set up
	auto& meshes = Locator::resources::value().GetMeshes();
	const entt::id_type mainMeshId = entt::hashed_string("temple/interior/main_l3d").value();
	std::vector<std::string> names;
	if (meshes.Contains(mainMeshId))
	{
		for (const auto& subMesh : meshes.Handle(mainMeshId)->GetSubMeshes())
		{
			names.push_back(subMesh->GetName());
		}
	}
	_toggles.Find(names);

	// and the challenge room its pictures, frames and buttons
	const entt::id_type challengeMeshId = entt::hashed_string("temple/interior/challenge_l3d").value();
	names.clear();
	if (meshes.Contains(challengeMeshId))
	{
		for (const auto& subMesh : meshes.Handle(challengeMeshId)->GetSubMeshes())
		{
			names.push_back(subMesh->GetName());
		}
	}
	_challengeControls.Find(names);
}

bool TempleInterior::HoldControl(bool pressed, float mouseY)
{
	// The room's press, on one of its controls or on the room itself, which the doors' tooltips wait for
	const bool dialogUp = _interface != nullptr && _interface->GetMenu().IsOpen();
	_pressHeld = RoomPressHeld(_pressHeld, pressed, _wasPressed, std::exchange(_pressRoomChanged, false), dialogUp);
	_wasPressed = pressed;
	const auto hit = GetCursorHit();
	const auto hoveredMainSubMesh =
	    hit.has_value() && hit->room == TempleRoom::Main ? hit->subMesh : std::optional<uint32_t> {};
	bool held = _toggles.Hold(pressed, hoveredMainSubMesh);
	// The challenge room's pictures and buttons, shown for the challenges found so far
	UpdateChallengeControls();
	const auto hoveredChallengeSubMesh =
	    hit.has_value() && hit->room == TempleRoom::Challenge ? hit->subMesh : std::optional<uint32_t> {};
	std::optional<TempleChallengeControls::Click> challengeClick;
	held = _challengeControls.Hold(pressed, hoveredChallengeSubMesh, challengeClick) || held;
	if (challengeClick.has_value())
	{
		ClickChallenge(*challengeClick);
	}
	if (_scrolls != nullptr)
	{
		std::optional<TempleScrolls::Focus> focus;
		TempleScrolls::LazyFacts facts(GatherScrollFacts);
		held = _scrolls->Hold(pressed, mouseY, hit, facts, focus) || held;
		if (focus.has_value() && _cameraModel != nullptr)
		{
			// Looking at a scroll wooshes, unless the camera is looking at one already
			if (!_cameraModel->IsLookingAtSubMesh())
			{
				PlayTempleSound(temple_sounds::Woosh(audio::TickCount()));
			}
			_cameraModel->LookAtSubMesh(focus->position, focus->lookAt);
			// Sent to the challenge room's scroll, the camera no longer looks at a picture
			if (_currentRoom == TempleRoom::Challenge)
			{
				_challengeControls.LookedAtScroll();
			}
			// Sent to look at a scroll, the camera starts the scroll's help
			if (const auto content = _scrolls->GetFocusedContent(); content.has_value())
			{
				temple_help::GameScripts scripts;
				temple_help::LookAtScroll(scripts, *content);
			}
		}
	}
	return held;
}

std::vector<uint32_t> TempleInterior::GetHiddenSubMeshes(TempleRoom room) const
{
	switch (room)
	{
	case TempleRoom::Main:
		return _toggles.GetHidden();
	case TempleRoom::Challenge:
		return _challengeControls.GetHidden();
	default:
		return {};
	}
}

bool TempleInterior::IsControl(TempleRoom room, uint32_t subMesh) const
{
	return (room == TempleRoom::Main && _toggles.IsControl(subMesh)) ||
	       (room == TempleRoom::Challenge && _challengeControls.IsControl(subMesh)) ||
	       (_scrolls != nullptr && _scrolls->IsControl(room, subMesh));
}

bool TempleInterior::IsControlHeld() const
{
	return _toggles.IsHeld() || _challengeControls.IsHeld() || (_scrolls != nullptr && _scrolls->IsHeld());
}

std::vector<TempleSubMeshTexture> TempleInterior::GetScrollTextures(TempleRoom room) const
{
	auto textures = _scrolls != nullptr ? _scrolls->GetTextures(room) : std::vector<TempleSubMeshTexture> {};
	// The challenge room's pictures are drawn with their own textures too
	if (room == TempleRoom::Challenge)
	{
		std::ranges::copy(_challengePictures.GetTextures(_challengeControls.GetPictures()), std::back_inserter(textures));
	}
	return textures;
}

std::vector<TempleSubMeshGlow> TempleInterior::GetControlGlows(TempleRoom room) const
{
	// The control under the cursor brightens by up to 12, 12 and 20 of 255 as the glow
	// runs out
	if (!_hovered.has_value() || _hovered->first != room || !IsControl(room, _hovered->second) || _hoverGlow <= 0.0f)
	{
		return {};
	}
	const auto channel = [this](float strength) {
		return static_cast<float>(static_cast<int32_t>(_hoverGlow * strength * 0.002f)) / 255.0f;
	};
	return {{_hovered->second, glm::vec3(channel(12.0f), channel(12.0f), channel(20.0f))}};
}

const graphics::Texture2D* TempleInterior::GetTextTexture() const
{
	return _interface != nullptr ? &_interface->GetFontTexture() : nullptr;
}

void TempleInterior::UpdateOptionsAndFutureRooms()
{
	const bool arrived = _cameraModel != nullptr && _cameraModel->IsInControl() && !_transitionRoom.has_value();
	if (_interface != nullptr)
	{
		auto& menu = _interface->GetMenu();
		menu.SetInsideTemple(_active);
		if (_cameraModel != nullptr)
		{
			_cameraModel->SetDialogOpen(menu.IsOpen());
		}
		// The options room, while no dialog is up: once the camera has come into the room it opens the game's
		// options, and once they are closed it goes back to the main room
		if (_currentRoom == TempleRoom::Options)
		{
			using temple::options_room::Step;
			switch (temple::options_room::Update(
			    _optionsShown, {.dialogUp = menu.IsOpen(), .arrived = arrived, .restartPending = menu.IsRestartPending()}))
			{
			case Step::OpenOptions:
				menu.Open();
				menu.ShowPage(gui::GameMenu::Page::Options, gui::GameMenu::PageSwitch::Replace);
				break;
			case Step::GoBackToMainRoom:
				GoToRoom(TempleRoom::Main);
				break;
			case Step::None:
				break;
			}
		}
		else if (_optionsShown)
		{
			// The options belong to their room: put elsewhere, as the debug window can, the player leaves them behind
			_optionsShown = false;
			menu.Close();
		}
	}

	// The future room: once the camera has come into the room with no dialog up, "The future is still uncertain..."
	// fades in to half over two seconds, in whole steps of the frames' whole milliseconds, from the start each time the
	// camera comes in again. Once shown they show at every coming in of this visit, dialog or not, and while they do the
	// room turns slowly by itself.
	std::optional<gui::GameInterface::Message> message;
	_futureAlpha.reset();
	if (_currentRoom == TempleRoom::Multi && _cameraModel != nullptr)
	{
		const bool dialogOpen = _interface != nullptr && _interface->GetMenu().IsOpen();
		_futureAlpha = temple::future_room::Step(_futureWords, _cameraModel->IsInControl(), dialogOpen,
		                                         static_cast<float>(game_clock::FrameRealMs()));
		if (_futureAlpha.has_value() && _interface != nullptr)
		{
			message = gui::GameInterface::Message {
			    .text = std::u16string(_interface->GetTexts().Get("HELP_TEXT_DIALOG_ADDITION_129")),
			    .alpha = *_futureAlpha / 255.0f,
			};
		}
	}
	if (_cameraModel != nullptr)
	{
		_cameraModel->SetFutureWordsShown(_futureWords.shown);
	}
	if (_interface != nullptr)
	{
		_interface->SetMessage(std::move(message));
	}
}

void TempleInterior::UpdateToolTips(const std::optional<TempleCursorHit>& hit)
{
	if (_interface == nullptr || _cameraModel == nullptr)
	{
		return;
	}
	// The rooms choose the tooltip as they are drawn
	const auto scrolls = _scrolls != nullptr ? _scrolls->GetControls(_currentRoom) : std::vector<TempleScrolls::Control> {};
	const auto toggles = _currentRoom == TempleRoom::Main ? _toggles.GetControls() : std::vector<TempleToggles::Control> {};
	const bool inChallengeRoom = _currentRoom == TempleRoom::Challenge;
	const auto challenges = inChallengeRoom ? _challengeControls.GetToolTipControls() : std::vector<TempleChallengeControl> {};
	const TempleToolTipInput input {
	    .room = _currentRoom,
	    .inControl = _cameraModel->IsInControl() && !_transitionRoom.has_value(),
	    .zoom = _cameraModel->GetSubMeshZoom(),
	    .lookingAtScroll = _cameraModel->IsLookingAtSubMesh(),
	    .pressHeld = _pressHeld,
	    .overPool = _cameraModel->IsOverPool(),
	    .pressingPool = _cameraModel->IsPressingPool(),
	    .hoveredDoor = _cameraModel->GetHoveredDoor(),
	    .overWayBack = _cameraModel->IsOverWayBack(),
	    .caveTarget = _cameraModel->GetCaveTarget(),
	    .zoomingToCaveTarget = _cameraModel->IsZoomingToCaveTarget(),
	    .hoveredSubMesh = hit.has_value() && hit->room == _currentRoom ? hit->subMesh : std::nullopt,
	    .scrolls = scrolls,
	    .toggles = toggles,
	    .challenges = challenges,
	    .challengeHighDetail = inChallengeRoom ? _challengeControls.GetHighDetailSlot() : std::nullopt,
	    .pictureLookedAt = inChallengeRoom && _challengeControls.IsPictureLookedAt(),
	};
	UpdateTempleToolTip(_toolTip, input);
	// (the tooltip is drawn by the hand, as on the land: Game's input prompts read the hand's own position)
}

void TempleInterior::LeaveForMapPoint(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto onMap = point - _templePosition;
	const auto world = _map.ToWorld(onMap);
	const float altitude = Locator::terrainSystem::value().GetHeightAt(world);
	// Only for a point near the middle of the map, or on land above the sea
	const auto leave = TempleMap::LeaveFor(onMap, world, altitude);
	if (!leave.has_value())
	{
		return;
	}
	// Leaving the temple puts the camera 50 above the place and 70 along z from it, looking at it
	// (pending) the original starts its force-feedback mouse's command-success effect here; openblack drives no force
	// feedback
	_leaveTo = std::make_pair(leave->origin, leave->focus);
	// The main room's camera fades the temple out to white, and the room leaves once it is
	Fade().FadeThrough(k_White);
	_leavingForMapPoint = true;
}

void TempleInterior::FadeToWhite()
{
	Fade().FadeThrough(k_White);
}

void TempleInterior::FadeIntoRoom()
{
	// Going to another room covers the cut from 1.2, in whatever colour is still fading
	Fade().FadeFrom(1.2f);
}

void TempleInterior::UpdateMapMarkers(float seconds)
{
	_mapMarkers.clear();
	if (_mapTriangles.empty())
	{
		return;
	}
	using namespace ecs::components;
	auto& registry = Locator::entitiesRegistry::value();
	// The players' colours as this land gives them
	const auto landNumber = influence::LandNumber();
	if (_toggles.IsShown(TempleToggles::Display::Temples))
	{
		// Every player's temple
		registry.Each<const Temple, const Transform>([this, landNumber](const Temple& temple, const Transform& transform) {
			_mapMarkers.push_back({.kind = TempleMapMarkerKind::Temple,
			                       .position = _map.MarkerPosition(glm::vec2(transform.position.x, transform.position.z)),
			                       .colour = TempleMap::MarkerColour(temple.owner, landNumber)});
		});
	}
	if (_toggles.IsShown(TempleToggles::Display::Creatures))
	{
		// Every creature, in its player's colour
		registry.Each<const Creature, const Transform>(
		    [this, landNumber](const Creature& creature, const Transform& transform) {
			    _mapMarkers.push_back({.kind = TempleMapMarkerKind::Creature,
			                           .position = _map.MarkerPosition(glm::vec2(transform.position.x, transform.position.z)),
			                           .colour = TempleMap::MarkerColour(creature.owner, landNumber)});
		    });
	}
	if (_toggles.IsShown(TempleToggles::Display::Challenges))
	{
		// Every challenge not yet done, in white, where the camera was when its picture was taken: placed on the map from
		// that point itself, not from its cell as the temples and creatures are
		registry.Each<const ChallengeRecord>([this](const ChallengeRecord& record) {
			if (record.success >= 1.0f || !record.position.has_value())
			{
				return;
			}
			const auto& at = *record.position;
			_mapMarkers.push_back({.kind = TempleMapMarkerKind::Challenge,
			                       .position = _map.ToMap(glm::vec2(at.x, at.z)),
			                       .colour = glm::u8vec3(0xFF),
			                       .glow = at + glm::vec3(0.0f, 0.1f, 0.0f)});
		});
	}
	// TODO(raffclar): the map also marks the miracles being cast and the players' influence, as their buttons show them
	// The markers turn a radian a second while the map is drawn
	_mapMarkerTurn += seconds;
}

void TempleInterior::UpdateCaveTrophies()
{
	using namespace CreatureCaveTrophies;
	_caveTrophies.clear();
	const auto facts = GatherScrollFacts();
	constexpr entt::id_type k_CreatureRoom = entt::hashed_string("temple/interior/creature_l3d").value();
	auto& meshes = Locator::resources::value().GetMeshes();
	if (!facts.creature.has_value() || !meshes.Contains(k_CreatureRoom))
	{
		return;
	}
	std::vector<int32_t> percents;
	percents.reserve(facts.creature->miracles.size());
	for (const auto& miracle : facts.creature->miracles)
	{
		percents.push_back(miracle.percent);
	}
	const auto trophies = Choose(facts.creature->fightBalance, LearningOf(percents));
	if (!trophies.empty() && !_caveIconsLoaded)
	{
		_caveIconsLoaded = true;
		LoadCaveIcons();
	}
	const auto& points = meshes.Handle(k_CreatureRoom)->GetExtraMetrics();
	const auto temple = glm::translate(glm::mat4(1.0f), _templePosition) * glm::eulerAngleY(_templeRotation.y);
	for (const auto& trophy : trophies)
	{
		const entt::id_type mesh = entt::hashed_string(fmt::format("temple/icons/{}", IconName(trophy.icon)).c_str()).value();
		if (trophy.point >= points.size() || !meshes.Contains(mesh))
		{
			continue;
		}
		// The creature's room gives each its point's matrix with its axes made unit long, turned a quarter back about
		// the point's own x axis
		auto place = points[trophy.point];
		const auto across = glm::normalize(glm::vec3(place[0]));
		const auto up = glm::normalize(glm::vec3(place[1]));
		const auto forward = glm::normalize(glm::vec3(place[2]));
		place[0] = glm::vec4(across, 0.0f);
		place[1] = glm::vec4(forward, 0.0f);
		place[2] = glm::vec4(-up, 0.0f);
		_caveTrophies.push_back({
		    .mesh = mesh,
		    .model = temple * place,
		    .colour = trophy.medal ? k_MedalColour : k_BeltColour,
		    .environmentMapped = trophy.environmentMapped,
		});
	}
}

void TempleInterior::StopCreatureCaveSounds()
{
	audio::StopSoundEffect(temple_sounds::k_CreatureCaveFireSample, audio::Owner {}, audio::SfxBank::InGame);
	audio::StopSoundEffect(temple_sounds::k_CreatureCaveWaterSample, audio::Owner {}, audio::SfxBank::InGame);
	_creatureCaveSounds = false;
}

void TempleInterior::UpdateCreatureCave()
{
	if (!IsRoomDrawn(TempleRoom::CreatureCave))
	{
		if (_creatureCaveSounds)
		{
			StopCreatureCaveSounds();
		}
		return;
	}
	// Every frame the room is drawn it plays the sounds of its water and fire, which carry on as they are while they
	// play, and stop once it is no longer drawn
	const auto fire = CreatureCaveFire();
	PlayTempleSound(temple_sounds::CreatureCaveWater());
	if (fire.has_value())
	{
		PlayTempleSound(temple_sounds::CreatureCaveFire(*fire));
	}
	_creatureCaveSounds = true;
	_creatureCaveFadeSet = true;
	// Its waterfall slides on only while it is drawn, and keeps its place from one visit to the next
	_waterfallSlide = temple_waterfall::Advance(_waterfallSlide, game_clock::FrameRealMs());
	// The room starts the first time it is drawn in a visit, with the copy of the player's creature by its fire, before
	// the smoke of its effects: only then, so the copy is made for the whole visit or not at all, as the room is to show
	// the creature at that moment
	if (creature_tattoo_editor::MakesCopy(std::exchange(_creatureCaveStarted, true), CreatureShown()))
	{
		MakeCreatureCopy(fire);
	}
	// Its effects start around its fire the first time it is drawn, and move on each frame it is
	if (_creatureCaveEffects == nullptr && fire.has_value() && CreatureCaveEffects::CanBeMade())
	{
		_creatureCaveEffects = std::make_unique<CreatureCaveEffects>(*fire, _light);
	}
	// The copy of the creature by its fire is posed for the frame it is drawn in, by the frame's real time, before the
	// effects move on: the room draws it first
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().UpdateCopies(creature_tattoo_editor::CopyFrameMs(game_clock::FrameRealMs()));
	}
	if (_creatureCaveEffects != nullptr)
	{
		// By the frame's real whole milliseconds, which the room's draw gives its effects for their time
		_creatureCaveEffects->Update(game_clock::FrameRealMs(), game_clock::TickCount());
	}
	// Then, with the tattoo editor open on the copy, the place on its body under the pointer, on the pose just drawn;
	// and what the copy does next
	HoverCopyTattooPoints();
	StepCreatureCopy(_copyLife);
}

TempleInterior::~TempleInterior() = default;

namespace
{
/// Each room's own mesh, "temple/interior/<room>_l3d", by room
constexpr std::array<entt::id_type, static_cast<size_t>(TempleRoom::Unknown)> k_RoomMeshIds {
    entt::hashed_string("temple/interior/challenge_l3d").value(), //
    entt::hashed_string("temple/interior/creature_l3d").value(),  //
    entt::hashed_string("temple/interior/credits_l3d").value(),   //
    entt::hashed_string("temple/interior/main_l3d").value(),      //
    entt::hashed_string("temple/interior/multi_l3d").value(),     //
    entt::hashed_string("temple/interior/options_l3d").value(),   //
    entt::hashed_string("temple/interior/savegame_l3d").value(),  //
};

/// Each room's path in, data/citadel/engine/<room>.cam, which the game loads as "temple/<room>"
TempleCameraModel::Paths LoadCameraPaths()
{
	TempleCameraModel::Paths paths;
	auto& cameraPaths = Locator::resources::value().GetCameraPaths();
	for (const auto& [room, name] : k_TempleInteriorGlows)
	{
		const auto pathName = fmt::format("temple/{}", name);
		const entt::id_type id = entt::hashed_string(pathName.c_str()).value();
		if (!cameraPaths.Contains(id))
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Temple camera path {} isn't loaded", pathName);
			continue;
		}
		paths.at(static_cast<size_t>(room)) = cameraPaths.Handle(id);
	}
	return paths;
}
} // namespace

void TempleInterior::ApplyLens() const
{
	const auto& config = Locator::config::value();
	auto& camera = Locator::camera::value();
	const auto aspect = Locator::windowing::has_value() ? Locator::windowing::value().GetAspectRatio() : 1.0f;
	const auto lens = camera.GetModel().GetLens();
	camera.SetProjectionMatrixPerspective(lens.has_value() ? lens->horizontalFieldOfView : config.cameraXFov, aspect,
	                                      lens.has_value() ? lens->nearClip : config.cameraNearClip, config.cameraFarClip);
}

bool TempleInterior::IsRoomDrawn(TempleRoom room) const
{
	if (room == _currentRoom || room == _transitionRoom)
	{
		return true;
	}
	// From another room, the main room is drawn whole only while its door is open, and otherwise just
	// its doors (the submeshes with joints)
	return room == TempleRoom::Main && !_transitionRoom.has_value() && _doors.GetSwing() > k_MainRoomOpenSwing;
}

bool TempleInterior::IsRoomAvailable(TempleRoom room) const
{
	// The local player's own creature opens its room
	bool hasCreature = false;
	if (Locator::playerSystem::has_value())
	{
		const auto& players = Locator::playerSystem::value();
		const auto creature = players.GetPrimaryCreature(players.GetLocalPlayer());
		hasCreature = creature.has_value() && *creature != entt::null;
	}
	// (pending) openblack keeps no room the player is kept in once the game is over
	return temple::IsRoomAvailable(room, {.roomsOpened = _roomsOpened, .hasCreature = hasCreature, .keptIn = std::nullopt});
}

void TempleInterior::TellRoomNotAvailable()
{
	chlapi::CHLApi::TellRoomNotAvailable();
}

void TempleInterior::GoToRoom(TempleRoom room)
{
	// The player cannot go to a room that is not open
	if (!IsRoomAvailable(room))
	{
		return;
	}
	if (_active && _cameraModel != nullptr)
	{
		if (room != _currentRoom)
		{
			FadeIntoRoom();
		}
		_cameraModel->GoToRoom(room);
	}
}

void TempleInterior::EnterRoom(TempleRoom room)
{
	if (_active && _cameraModel != nullptr && !_cameraModel->GoThroughDoorTo(room))
	{
		GoToRoom(room);
	}
}

void TempleInterior::SetCurrentRoom(TempleRoom room)
{
	// Coming into the challenge room, no picture is looked at in high detail, and the low detail ones are read again
	if (room == TempleRoom::Challenge && room != _currentRoom)
	{
		_challengeControls.Enter();
		_challengePictures.Reread();
	}
	// Coming into the library, the story history shows its newest line first
	if (room == TempleRoom::Credits && room != _currentRoom && _scrolls != nullptr)
	{
		_scrolls->ShowLatestHistory();
	}
	// A press held across a change of room is not the new room's
	if (room != _currentRoom)
	{
		_pressRoomChanged = true;
	}
	_currentRoom = room;
	// Coming into a room, on the way into the temple or from another room, stops the help running and starts the
	// room's own
	if (_active && _enteredRoom != room)
	{
		_enteredRoom = room;
		temple_help::GameScripts scripts;
		temple_help::EnterRoom(scripts, room, temple_help::HelpSystemOn());
	}
}

void TempleInterior::LookAtScroll(size_t place)
{
	if (!_active || _scrolls == nullptr || _cameraModel == nullptr)
	{
		return;
	}
	TempleScrolls::LazyFacts facts(GatherScrollFacts);
	if (const auto focus = _scrolls->LookAt(_currentRoom, place, facts); focus.has_value())
	{
		_cameraModel->LookAtSubMesh(focus->position, focus->lookAt);
	}
}

void TempleInterior::UpdateChallengeControls()
{
	// The room's slots take the challenges in the order they were found: a quest's in a gold frame, any other's in a
	// silver one
	std::vector<temple_challenge_room::Challenge> challenges;
	if (Locator::entitiesRegistry::has_value())
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		for (const auto record : ecs::challenge_records::InOrder(registry))
		{
			const auto& found = registry.Get<const ecs::components::ChallengeRecord>(record);
			challenges.push_back({.gold = found.quest, .success = found.success});
		}
	}
	_challengeControls.SetChallenges(challenges);
}

void TempleInterior::ClickChallenge(const TempleChallengeControls::Click& click)
{
	if (_cameraModel == nullptr)
	{
		return;
	}
	// A button chooses its challenge at once, a picture once the camera has come all the way to it. Chosen, unless it
	// is won already, the screen fades to white with a woosh, and the player leaves once it is white
	if (click.control == temple_challenge_room::Control::Button ||
	    temple_challenge_pick::ClickOn(_cameraModel->GetSubMeshZoom()) == temple_challenge_pick::Click::Choose)
	{
		const auto challenge = _challengeControls.ChallengeOf(click.slot);
		if (_replay.has_value() || !challenge.has_value() || !temple_challenge_pick::CanChoose(challenge->success))
		{
			return;
		}
		const auto records = ecs::challenge_records::InOrder(std::as_const(Locator::entitiesRegistry::value()));
		if (click.slot >= records.size())
		{
			return;
		}
		_replay = records[click.slot];
		Fade().FadeThrough(k_White);
		if (const auto woosh = temple_challenge_pick::ChosenWoosh(challenge->success, audio::TickCount()); woosh.has_value())
		{
			PlayTempleSound(*woosh);
		}
		return;
	}
	if (click.control != temple_challenge_room::Control::Picture)
	{
		return;
	}
	// The camera turns to the picture: it looks at the middle of the picture's box from 25 units out along its frame's z
	// axis, with a woosh unless it is looking at something already
	auto& meshes = Locator::resources::value().GetMeshes();
	const entt::id_type meshId = k_RoomMeshIds.at(static_cast<size_t>(TempleRoom::Challenge));
	if (!meshes.Contains(meshId))
	{
		return;
	}
	const auto& subMeshes = meshes.Handle(meshId)->GetSubMeshes();
	if (click.subMesh >= subMeshes.size())
	{
		return;
	}
	const auto& frame = subMeshes[click.subMesh]->GetFrame();
	const auto middle = (frame.min + frame.max) * 0.5f;
	if (!_cameraModel->IsLookingAtSubMesh())
	{
		PlayTempleSound(temple_sounds::Woosh(audio::TickCount()));
	}
	_cameraModel->LookAtSubMesh(glm::vec3(frame.toMesh * glm::vec4(middle + glm::vec3(0.0f, 0.0f, 25.0f), 1.0f)),
	                            glm::vec3(frame.toMesh * glm::vec4(middle, 1.0f)));
	// and the picture is shown in high detail
	_challengeControls.LookAt(click.slot);
}

void TempleInterior::UpdateChallengeCaption()
{
	if (_interface == nullptr)
	{
		return;
	}
	// While the camera is any way close to the picture last looked at, its challenge's title and how it went are written
	// at the top of the screen, showing more the closer the camera is
	std::optional<gui::GameInterface::Caption> caption;
	const auto slot = _challengeControls.GetLookedAtSlot();
	const float closeness = _cameraModel != nullptr ? _cameraModel->GetSubMeshZoom() : 0.0f;
	if (IsRoomDrawn(TempleRoom::Challenge) && slot.has_value() && closeness != 0.0f)
	{
		const auto& registry = std::as_const(Locator::entitiesRegistry::value());
		const auto records = ecs::challenge_records::InOrder(registry);
		if (*slot < records.size())
		{
			namespace room = temple_challenge_room;
			const auto& challenge = registry.Get<const ecs::components::ChallengeRecord>(records[*slot]);
			const auto& texts = _interface->GetTexts();
			const auto names = texts.GetHelpNames();
			const auto title = names.empty() ? std::u16string()
			                                 : std::u16string(texts.Get(names[room::TitleText(challenge.title, names.size())]));
			const auto taken =
			    room::TimeTaken(challenge.startedTurn, challenge.doneTurn, game_clock::Turn(), game_clock::MsPerTurn());
			caption = gui::GameInterface::Caption {
			    .lines = {title, room::DetailsLine(texts.Get("HELP_TEXT_ROOM_CITADEL_CHALLENGE_ROOM_TIME"), taken,
			                                       texts.Get("HELP_TEXT_ROOM_CITADEL_CHALLENGE_ROOM_PERCENTCOMPLETE"),
			                                       challenge.success)},
			    .alpha = room::TextAlpha(closeness),
			};
		}
	}
	_interface->SetCaption(std::move(caption));
}

void TempleInterior::LeaveToReplayChallenge()
{
	const auto record = *std::exchange(_replay, std::nullopt);
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto* challenge = registry.Valid(record) ? registry.TryGet<const ecs::components::ChallengeRecord>(record) : nullptr;
	const auto number = challenge != nullptr ? std::optional(challenge->challenge) : std::nullopt;
	// The player leaves the temple, which gives the island its camera back, and then the challenge's reminder starts,
	// which puts the camera where the challenge's picture was taken. A record gone just leaves
	Deactivate();
	if (number.has_value())
	{
		chlapi::CHLApi::StartChallengeScript(*number, true);
	}
}

std::optional<TempleCursorHit> TempleInterior::GetCursorHit() const
{
	if (!_active || _cameraModel == nullptr)
	{
		return std::nullopt;
	}
	// The cursor is over the submesh the game picks while drawing the room, and the hand goes where the pick meets
	// it, or onto the floor where the cursor meets that this frame
	const auto& hit = _cameraModel->GetCursorHit();
	const bool onFloor = hit.has_value() && hit->floor;
	const auto& ray = _cameraModel->GetCursorRay();
	if (ray.has_value())
	{
		// The game picks along the ray through every room's mesh it draws, and from another room just the main room's
		// doors
		const auto toRoom =
		    glm::inverse(glm::translate(glm::mat4(1.0f), _templePosition) * glm::eulerAngleY(_templeRotation.y));
		const auto origin = glm::vec3(toRoom * glm::vec4(ray->origin, 1.0f));
		const auto direction = glm::vec3(toRoom * glm::vec4(ray->focus - ray->origin, 0.0f));
		auto& meshes = Locator::resources::value().GetMeshes();
		std::optional<graphics::L3DMesh::PickHit> nearest;
		TempleRoom nearestRoom = TempleRoom::Unknown;
		// The rooms in the same order as ever, which settles a tie between two rooms' picks
		for (const auto& room : k_TempleInteriorGlows | std::views::keys)
		{
			const bool drawn = IsRoomDrawn(room);
			if (!drawn && room != TempleRoom::Main)
			{
				continue;
			}
			const entt::id_type meshId = k_RoomMeshIds.at(static_cast<size_t>(room));
			if (!meshes.Contains(meshId))
			{
				continue;
			}
			// The side rooms' doors aren't drawn shut, so nor are they picked
			const auto hidden = GetHiddenSubMeshes(room);
			if (const auto pick = meshes.Handle(meshId)->Pick(origin, direction, !drawn, room != TempleRoom::Main, hidden);
			    pick.has_value() && (!nearest.has_value() || pick->distance < nearest->distance))
			{
				nearest = pick;
				nearestRoom = room;
			}
		}
		if (nearest.has_value())
		{
			return TempleCursorHit {
			    .point = onFloor ? hit->point : ray->origin + (ray->focus - ray->origin) * nearest->distance,
			    .normal = onFloor ? hit->normal : -glm::normalize(ray->focus - ray->origin),
			    .room = nearestRoom,
			    .subMesh = nearest->subMesh,
			};
		}
	}
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	return TempleCursorHit {.point = hit->point, .normal = hit->normal};
}

void TempleInterior::Escape()
{
	// (pending) the room the game keeps the player in once the game is over is not kept
	if (temple::OnEscape(_currentRoom, std::nullopt) == temple::EscapeResult::GoToMainRoom)
	{
		GoToRoom(Indoors::Main);
	}
	else
	{
		RequestLeave();
	}
}

glm::vec2 TempleInterior::GetWaterfallSlide() const
{
	return temple_waterfall::Offset(_waterfallSlide);
}

void TempleInterior::ProcessTurn()
{
	// The temple's turn submits the room's tooltip, or none under a dialog, and the help system's turn that follows
	// keeps it or ends it
	if (_active && _interface != nullptr && !_interface->GetMenu().IsOpen() && _toolTip.index.has_value())
	{
		_interface->GetToolTips().Submit(*_toolTip.index, _toolTip.action, _toolTip.arrows);
	}
}

void TempleInterior::Update(std::chrono::microseconds dt)
{
	if (_active)
	{
		// Once the main room is drawn, the other rooms are open for the rest of the game
		if (IsRoomDrawn(TempleRoom::Main))
		{
			_roomsOpened = true;
		}
		const float milliseconds = std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(dt).count();

		UpdateOptionsAndFutureRooms();

		// The glow of the control under the cursor runs out each frame, and drawing the room sets it off
		// again as the cursor moves onto another submesh, while no control is held
		_hoverGlow = std::max(0.0f, _hoverGlow - std::floor(milliseconds));
		// Picked once a frame: nothing the pick looks at changes before the tooltip is chosen
		const auto hit = GetCursorHit();
		if (!IsControlHeld())
		{
			std::optional<std::pair<TempleRoom, uint32_t>> hovered;
			if (hit.has_value() && hit->subMesh.has_value())
			{
				hovered = std::make_pair(hit->room, *hit->subMesh);
			}
			if (hovered != _hovered)
			{
				_hovered = hovered;
				_hoverGlow = 500.0f;
			}
		}

		// As the rooms are drawn, the scroll the camera is close to has its text drawn in front of it, and the signs
		// their labels
		_text.clear();
		_scrollPictures.clear();
		if (_scrolls != nullptr && _cameraModel != nullptr)
		{
			// The facts are gathered only if a scroll is written this frame
			TempleScrolls::LazyFacts facts(GatherScrollFacts);
			_scrolls->SetFocus(_cameraModel->GetSubMeshZoom(), facts);
			// While the camera is close to the story history, its buttons show, and a press turns its first line
			if (_interface != nullptr)
			{
				auto& historyBox = _interface->GetStoryHistoryBox();
				historyBox.SetShown(_scrolls->IsHistoryOpen());
				if (const auto turn = historyBox.TakeTurn(); turn.has_value())
				{
					_scrolls->TurnHistory(*turn, facts);
				}
			}
			_scrolls->AppendFocusedText(_text, facts, &_scrollPictures);
		}
		if (_signs != nullptr)
		{
			// The highlighted sign pulses with the game's clock
			const auto ticks = game_clock::TickCount();
			for (const auto room : {TempleRoom::Main, TempleRoom::CreatureCave, TempleRoom::Credits})
			{
				if (IsRoomDrawn(room))
				{
					// The main room lights the label of the door its camera last found under the cursor
					const auto highlighted = room == TempleRoom::Main && _cameraModel != nullptr
					                             ? TempleSigns::MainRoomSignOfDoor(_cameraModel->GetHighlightedDoor())
					                             : std::nullopt;
					_signs->Append(_text, room, highlighted, ticks);
				}
			}
		}

		// The challenge room's pictures follow the records' pictures while the room is drawn
		if (IsRoomDrawn(TempleRoom::Challenge))
		{
			_challengePictures.Update(std::as_const(Locator::entitiesRegistry::value()),
			                          _challengeControls.GetHighDetailSlot());
		}
		UpdateChallengeCaption();
		UpdateToolTips(hit);

		// The main room takes the land's heights and brightness for the map every frame the main room is drawn
		_mapTriangles.clear();
		if (IsRoomDrawn(TempleRoom::Main) && Locator::terrainSystem::has_value())
		{
			const auto& island = Locator::terrainSystem::value();
			_map.Build(TempleMap::CellsOf(island), _mapTriangles);
		}
		// The players' influence circles stand over the map as just built, while the main room's button shows them
		_mapInfluence.reset();
		if (_toggles.ShowsInfluenceOnTheMap(_currentRoom == TempleRoom::Main))
		{
			auto placement = _map.GetInfluencePlacement();
			placement.offset += GetPosition();
			_mapInfluence = placement;
		}
		UpdateMapMarkers(milliseconds / 1000.0f);
		// The temple is lit by the alignment, pulsing with the game's clock
		const float alignment =
		    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetCameraAlignment() : 0.0f;
		_light = TempleLight::At(alignment, game_clock::TickCount());
		// and faded with depth as the room the player is in was last set, before this frame draws it
		_light.fade = temple_fade::Of(true, _currentRoom == TempleRoom::CreatureCave, _creatureCaveFadeSet);
		// The creature's room chooses the belts and medals every frame it is drawn
		if (IsRoomDrawn(TempleRoom::CreatureCave))
		{
			UpdateCaveTrophies();
		}
		else
		{
			_caveTrophies.clear();
		}
		// The main room moves the pool's shimmer on while the main room is drawn, by the frame's whole milliseconds, at
		// least one, as the game's frame time ((approximate) the frame's milliseconds cut to whole ones stand for it)
		if (IsRoomDrawn(TempleRoom::Main))
		{
			_poolTime = temple_pool::Advance(_poolTime, std::max(1, static_cast<int32_t>(milliseconds)));
		}
		if (_cameraModel != nullptr)
		{
			if (const auto point = _cameraModel->TakeMapDoubleClick(); point.has_value())
			{
				LeaveForMapPoint(*point);
			}
		}
		// The creature's room's sounds and effects, while it is drawn
		UpdateCreatureCave();
	}

	// Once the screen is white the player leaves to replay the challenge chosen
	if (_active && _replay.has_value() && Fade().GetTurns() > 0)
	{
		_leaveRequested = false;
		LeaveToReplayChallenge();
	}
	if (_leavingForMapPoint && Fade().GetTurns() > 0)
	{
		RequestLeave();
	}
	if (_leaveRequested)
	{
		_leaveRequested = false;
		Deactivate();
	}
}

void TempleInterior::Activate(TempleRoom room)
{
	// The player cannot enter the temple straight into a room that is not open
	if (_active || !IsRoomAvailable(room))
	{
		return;
	}

	auto& config = Locator::config::value();
	auto& camera = Locator::camera::value();

	_playerPositionOutside = camera.GetOrigin();
	_playerRotationOutside = camera.GetRotation();

	// The main room frames the map on the island as it is each visit, and draws its texture afresh
	++_visits;
	if (Locator::terrainSystem::has_value())
	{
		_map.Frame(TempleMap::CellsOf(Locator::terrainSystem::value()));
	}

	config.drawIsland = false;
	config.drawWater = false;

	// Create temple entities
	// (inferred) no source; _templeRotation is never written (0), so the sign does not show
	auto rotation = glm::eulerAngleY(_templeRotation.y);
	auto scale = glm::vec3(1.0f);

	for (const auto& [roomType, assetName] : k_TempleInteriorParts)
	{
		addRoomToRegistry(assetName, roomType, _templePosition, rotation, scale);
	}
	for (const auto& [roomType, assetName] : k_TempleInteriorGlows)
	{
		addGlowsToRegistry(roomType);
	}

	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystemTemple>();

	// The temple's camera takes over from the island's, coming into the room along its path
	_active = true;
	_leaveRequested = false;
	_leavingForMapPoint = false;
	_replay.reset();
	// Entering the temple covers the way in from 1.2
	FadeIntoRoom();
	_transitionRoom.reset();
	// The main room's doorway is kept from the last visit, as the game never resets it
	// The creature's room's effects start around its fire once the room is first drawn (UpdateCreatureCave)
	_currentRoom = room;
	// Put straight into the challenge room, no picture is looked at in high detail, and the low detail ones are read
	if (room == TempleRoom::Challenge)
	{
		_challengeControls.Enter();
		_challengePictures.Reread();
	}
	_enteredRoom.reset();
	// Going in, the help stops, the advisors fly home and the world's dialogue is put away, before the room's help starts
	{
		temple_help::GameScripts scripts;
		temple_help::GameDialogue dialogue;
		_heldDialogue = temple_help::PutAwayDialogue(scripts, dialogue);
	}
	auto model = std::make_unique<TempleCameraModel>(LoadCameraPaths(), _currentRoom);
	_cameraModel = model.get();
	_outsideCameraModel = camera.SetModel(std::move(model));
	_cameraModel->StartIntro(_currentRoom, false);
	camera.SetOrigin(_cameraModel->GetTargetOrigin());
	camera.SetFocus(_cameraModel->GetTargetFocus());
	ApplyLens();
	CreateScrolls();
	// going inside the citadel takes the player's creature's leash off, once the world goes on after leaving
	ecs::leash_tie::OnGoInsideCitadel(Locator::leashSystem::has_value() ? &Locator::leashSystem::value() : nullptr,
	                                  PlayerNames::PLAYER_ONE);
	// going inside the citadel pauses the world, whose pause until then is kept to give back on leaving
	_pausedOutside = game_clock::IsPaused();
	game_clock::Pause(true);
	// and sets the citadel sequence mode
	game_clock::SetSequenceMode(game_clock::k_SequenceModeCitadel);
}

void TempleInterior::Deactivate()
{
	if (!_active)
	{
		return;
	}
	_scrolls.reset();
	_signs.reset();
	_challengePictures.Clear();
	_text.clear();
	_scrollPictures.clear();
	if (_interface != nullptr)
	{
		// The story history's buttons go with the temple. The options the options room opened stay up: leaving the
		// temple puts no dialog away.
		_interface->GetStoryHistoryBox().SetShown(false);
		_interface->SetMessage(std::nullopt);
		_interface->SetCaption(std::nullopt);
		_interface->GetMenu().SetInsideTemple(false);
		// The temple's tooltip does not follow the player out
		_interface->GetToolTips().Clear();
	}
	_optionsShown = false;
	// The future room's words start again with the next visit
	_futureWords = {};
	_futureAlpha.reset();
	// The creature's room's fire and water stop, and its effects go before the rest of the rooms
	if (_creatureCaveSounds)
	{
		StopCreatureCaveSounds();
	}
	_creatureCaveEffects.reset();
	// and the copy of the creature by its fire, made again as the room starts in the next visit
	DestroyCreatureCopies();
	_creatureCaveStarted = false;

	auto& registry = Locator::entitiesRegistry::value();
	auto& config = Locator::config::value();
	config.drawIsland = true;
	config.drawWater = true;
	registry.Each<const ecs::components::TempleInteriorPart>(
	    [&registry](const entt::entity entity, auto&&...) { registry.Destroy(entity); });

	// leaving the citadel: the temple's fire and water samples stop
	audio::LeaveCitadel();

	auto& camera = Locator::camera::value();
	Locator::rendereringSystem::emplace<ecs::systems::RenderingSystem>();
	if (_outsideCameraModel != nullptr)
	{
		camera.SetModel(std::move(_outsideCameraModel));
	}
	_cameraModel = nullptr;
	_transitionRoom.reset();
	_leavingForMapPoint = false;
	_replay.reset();
	_caveTrophies.clear();
	// Leaving the temple goes out to the island all white, which fades over a second
	Fade().FadeFrom(1.0f, k_White);
	ApplyLens();
	// The world's dialogue comes back as the player leaves, then the help running stops
	_enteredRoom.reset();
	{
		temple_help::GameDialogue dialogue;
		temple_help::BringBackDialogue(dialogue, _heldDialogue);
	}
	temple_help::GameScripts scripts;
	temple_help::Leave(scripts);
	camera.SetOrigin(_playerPositionOutside);
	camera.SetFocus(_playerPositionOutside + glm::quat(_playerRotationOutside) * glm::vec3(0.0f, 0.0f, 1.0f));
	if (_leaveTo.has_value())
	{
		camera.SetOrigin(_leaveTo->first);
		camera.SetFocus(_leaveTo->second);
		_leaveTo.reset();
	}
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().IsPathing())
	{
		Locator::cameraPathSystem::value().Stop();
	}
	_active = false;
	// leaving the citadel: no sequence mode, and the world paused as it was before the player went in. The paused time
	// does not count, so the world goes on from the turn it stopped at
	game_clock::SetSequenceMode(game_clock::k_SequenceModeNone);
	game_clock::Pause(_pausedOutside);
}