/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldStart.h"

#include <array>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/PlayerOwnSymbols.h"
#include "Camera/PlayerCameraScript.h"
#include "Common/StringUtils.h"
#include "Creature/CreatureSkin.h"
#include "ECS/Archetypes/OneOffSpellSeedArchetype.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/Hand.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameServices.h"
#include "Help/DialogueText.h"
#include "Help/HelpSystem.h"
#include "Help/PlayerChoices.h"
#include "Level.h"
#include "Locator.h"
#include "Parsers/InfoFile.h"
#include "Resources/BlobStream.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"

namespace openblack::simulation
{

bool RegisterWorldData(WorldDataViewInterface& view)
{
	using filesystem::Path;
	auto& fileSystem = Locator::filesystem::value();
	auto& resources = Locator::resources::value();
	auto& meshManager = resources.GetMeshes();
	auto& textureManager = resources.GetTextures();
	auto& animationManager = resources.GetAnimations();
	auto& levelManager = resources.GetLevels();
	auto& glowManager = resources.GetGlows();
	auto& cameraPathManager = resources.GetCameraPaths();

	fileSystem.Iterate(
	    fileSystem.GetPath<Path::Citadel>() / "OutsideMeshes", false, [&meshManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });

	view.StartupStep("temple outside meshes");

	fileSystem.Iterate( //
	    fileSystem.GetPath<filesystem::Path::Citadel>() / "engine", false,
	    [&meshManager, &glowManager, &cameraPathManager](const std::filesystem::path& f) {
		    if (f.extension() == ".zzz")
		    {
			    if (f.stem().string().ends_with("lo_l3d"))
			    {
				    SPDLOG_LOGGER_WARN(
				        spdlog::get("game"),
				        "Skipping lo duplicate lo meshes. See https://github.com/openblack/openblack/issues/727");
				    return;
			    }
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple mesh: {}", f.stem().string());
			    try
			    {
				    meshManager.Load(fmt::format("temple/interior/{}", f.stem().string()), resources::L3DLoader::FromDiskTag {},
				                     f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".glw")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple glows: {}", f.stem().string());
			    try
			    {
				    glowManager.Load(fmt::format("temple/interior/glow/{}", f.stem().string()),
				                     resources::LightLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
		    else if (f.extension() == ".cam")
		    {
			    SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading interior temple cam: {}", f.stem().string());
			    try
			    {
				    cameraPathManager.Load(fmt::format("temple/{}", f.stem().string()),
				                           resources::CameraPathLoader::FromDiskTag {}, f);
			    }
			    catch (std::runtime_error& err)
			    {
				    SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			    }
		    }
	    });
	// The main room's markers of the temples, creatures and challenges on its map
	for (const auto* icon : {"I_citadel_on_map", "I_creature_on_map", "I_challenge_on_map"})
	{
		try
		{
			meshManager.Load(fmt::format("temple/icons/{}", icon), resources::L3DLoader::FromDiskTag {},
			                 fileSystem.GetPath<Path::Citadel>() / "icons" / fmt::format("{}.l3d", icon));
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	view.StartupStep("temple interior meshes, glows, cameras and icons");

	pack::PackFile pack;

	// read once into the byte cache: the exploding objects parse the same copy later
	auto packResult = pack::PackResult::ErrCantOpen;
	try
	{
		const auto& packBytes = resources::LoadBlob(Locator::resources::value().GetBlobs(),
		                                            fileSystem.FindPath(fileSystem.GetPath<Path::Data>() / "AllMeshes.g3d"));
		packResult = pack.ReadFile(*resources::BlobStream(packBytes));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "AllMeshes.g3d: {}", e.what());
	}
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllMeshes.g3d: {}", pack::ResultToStr(packResult));
		return false;
	}

	view.StartupStep("mesh pack read");
	const auto& meshes = pack.GetMeshes();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; const auto& mesh : meshes)
	{
		const auto meshId = static_cast<MeshId>(i);
		// a pack may have more meshes than openblack has names for
		const auto name = i < k_MeshNames.size() ? k_MeshNames[i] : fmt::format("Mesh{}", i);
		meshManager.Load(meshId, resources::L3DLoader::FromBufferTag {}, name, mesh);
		++i;
	}

	view.StartupStep("mesh pack meshes");
	const auto& textures = pack.GetTextures();
	for (auto const& [name, g3dTexture] : textures)
	{
		textureManager.Load(g3dTexture.header.id, resources::Texture2DLoader::FromPackTag {}, name, g3dTexture);
	}

	view.StartupStep("mesh pack textures");
	pack::PackFile animationPack;
	packResult = animationPack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<Path::Data>() / "AllAnims.anm"));
	if (packResult != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_CRITICAL(spdlog::get("game"), "Unable to load AllAnims.anm: {}", pack::ResultToStr(packResult));
		return false;
	}

	const auto& animations = animationPack.GetAnimations();
	// TODO (#749) use std::views::enumerate
	for (size_t i = 0; i < animations.size(); i++)
	{
		animationManager.Load(i, resources::L3DAnimLoader::FromBufferTag {}, animations[i]);
	}
	view.StartupStep("animation pack");

	fileSystem.Iterate(fileSystem.GetPath<Path::CreatureMesh>(), false, [&meshManager](const std::filesystem::path& f) {
		const auto& fileName = f.stem().string();
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading creature mesh: {}", fileName);
		try
		{
			if (string_utils::BeginsWith(fileName, "Hand"))
			{
				return;
			}

			const auto meshId = creature::GetIdFromMeshName(fileName);
			if (!meshId.has_value())
			{
				// A file whose name names no appearance is not a species' body; loading it would take the id of the
				// real mesh of that species
				return;
			}
			meshManager.Load(*meshId, resources::L3DLoader::FromDiskTag {}, f);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	view.StartupStep("creature meshes");

	// Load loose one-off assets
	{
		// the ids of the loose assets (the coffre's animation and mesh share one id, in different caches)
		constexpr auto k_CoffreId = entt::hashed_string("coffre");
		constexpr auto k_ConeMesh = entt::hashed_string("cone");
		constexpr auto k_MarkerMesh = entt::hashed_string("marker");
		constexpr auto k_RiverMesh = entt::hashed_string("river");
		constexpr auto k_River2Mesh = entt::hashed_string("river2");
		constexpr auto k_MetreSphereMesh = entt::hashed_string("metre_sphere");

		using AFromDiskTag = resources::L3DAnimLoader::FromDiskTag;
		animationManager.Load(k_CoffreId.value(), AFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.anm");

		using LFromDiskTag = resources::L3DLoader::FromDiskTag;
		LoadHandMesh();
		view.StartupStep("hand mesh");
		// What moves each species' body, and the meshes whose skins are painted
		resources::LoadCreatureRigs(Locator::resources::value());
		view.StartupStep("creature rigs");
		// The eyes every creature is drawn with
		for (const auto& [id, file] : {std::pair {ecs::components::CreatureEyes::k_EyeballMeshId, "Eyeball.l3d"},
		                               std::pair {ecs::components::CreatureEyes::k_EyelidMeshId, "Eyelid.l3d"}})
		{
			if (const auto path = fileSystem.GetPath<Path::Data>() / file; fileSystem.Exists(path))
			{
				meshManager.Load(id, LFromDiskTag {}, path);
			}
		}
		meshManager.Load(k_CoffreId.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Misc>() / "coffre.l3d");
		// The closed reward chest
		if (const auto path = fileSystem.GetPath<Path::Misc>() / "chest0.l3d"; fileSystem.Exists(path))
		{
			meshManager.Load(entt::hashed_string("misc/chest0").value(), LFromDiskTag {}, path);
		}
		meshManager.Load(k_ConeMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "cone.l3d");
		meshManager.Load(k_MarkerMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "marker.l3d");
		meshManager.Load(k_RiverMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river.l3d");
		meshManager.Load(k_River2Mesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "river2.l3d");
		meshManager.Load(k_MetreSphereMesh.value(), LFromDiskTag {}, fileSystem.GetPath<Path::Data>() / "metre_sphere.l3d");
		view.StartupStep("one-off meshes");
		// the profile's own symbol pictures, as the game starts: the profile played is known, and the symbols the player
		// chooses from take them as their textures are first made, the menus' and the particles' (as the original, which
		// reads them once a run just after it sets the profile and before it makes those textures)
		player_own_symbols::ReadForProfile();
		// the in-game menus
		view.MakeInterface();
		view.StartupStep("game interface");
		// the dispensers' bubble, made with the one-shot orb: .\data\spells\meshes\O_Bibble_up.l3d (not in the
		// test data)
		try
		{
			meshManager.Load(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName, LFromDiskTag {},
			                 fileSystem.GetPath<Path::Data>() / "Spells" / "Meshes" / "O_bibble_up.l3d");
			// the shared mesh with MaterialProperties {1, 1, 0, 1, 1}: the change flag rewrites every primitive:
			// the cap's AlphaTextured (4) -> 6 -> additive 13 -> with Z write 12 (SRCALPHA / ONE, alpha = texture x
			// diffuse), and the double-sided bit cleared. The object alpha's mode table keeps mode 12, so the bubble
			// ADDS its texture x 0x95 to what is behind it: the bright, pearly bubble of the original
			meshManager.Handle(resources::HashIdentifier(ecs::archetypes::OneOffSpellSeedArchetype::k_MeshName))
			    ->SetMaterialProperties(
			        {.additive = true, .zWrite = true, .doubleSided = false, .change = true, .alpha = true});
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
		// the temples' leash posts: their collar in the rope's texture (the game loads it with the first temple)
		resources::shared_assets::LoadLeashCollar("Leash posts");
	}

	view.StartupStep("spell seed bubble and leash collar");

	// TODO(raffclar): #400: Parse level files within the resource loader
	// TODO(raffclar): #405: Determine campaign levels from the challenge script file
	// Load the campaign levels
	fileSystem.Iterate(fileSystem.GetPath<Path::Scripts>(), false, [&levelManager](const std::filesystem::path& f) {
		const auto& name = f.stem().string();
		if (f.extension() != ".txt" || name.rfind("InfoScript", 0) != std::string::npos)
		{
			return;
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading campaign level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("campaign/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Campaign);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});
	// Load Playgrounds
	// Attempt to load additional levels as playgrounds
	fileSystem.Iterate(fileSystem.GetPath<Path::Playgrounds>(), false, [&levelManager](const std::filesystem::path& f) {
		if (f.extension() != ".txt")
		{
			return;
		}
		const auto& name = f.stem().string();
		if (levelManager.Contains(fmt::format("playgrounds/{}", name)))
		{
			// Already added
			return;
		}

		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading custom level: {}", f.stem().string());
		try
		{
			if (Level::IsLevelFile(f))
			{
				levelManager.Load(fmt::format("playgrounds/{}", name), resources::LevelLoader::FromDiskTag {}, f,
				                  Level::LandType::Skirmish);
			}
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	});

	view.StartupStep("level list");

	view.StartAudio();

	view.StartupStep("sound and music banks");
	// The pre-intro, once there is audio for its music
	view.ContinueStartupScreens();
	{
		InfoFile infoFile;
		auto result = infoFile.LoadFromFile(Locator::filesystem::value().GetPath<filesystem::Path::Scripts>() / "info.dat");
		if (!result)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to load game info data.");
			return false;
		}
		Locator::infoConstants::reset(result.release());
	}

	view.StartupStep("info tables");

	view.StartHelp();
	view.StartupStep("help");

	fileSystem.Iterate(fileSystem.GetPath<Path::Textures>(), false, [&textureManager](const std::filesystem::path& f) {
		// The game ships a grey ice map it never loads, cut short of a whole texture
		constexpr std::string_view k_UnusedTexture = "s_iceenvmapgrey.raw";
		if (string_utils::LowerCase(f.extension().string()) == ".raw" &&
		    string_utils::LowerCase(f.filename().string()) != k_UnusedTexture)
		{
			SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading raw texture: {}", f.stem().string());
			try
			{
				textureManager.Load(fmt::format("raw/{}", f.stem().string()), resources::Texture2DLoader::FromDiskTag {}, f);
			}
			catch (std::runtime_error& err)
			{
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
			}
		}
	});
	view.StartupStep("raw textures");

	// The noise that makes the snow's edges on the land ragged, read now into the byte cache for the land's prints step
	// (ecs::land_prints)
	(void)resources::LoadOptionalBlob(Locator::resources::value().GetBlobs(),
	                                  fileSystem.GetPath<Path::WeatherSystem>() / "snowmap.raw", "snow's noise");

	// The texture every creature's hair is drawn with, and its alpha beside it
	for (const auto& [id, name] : {std::pair {ecs::components::CreatureHair::k_TextureId, "C_Ape_Hair.raw"},
	                               std::pair {ecs::components::CreatureHair::k_AlphaTextureId, "C_Ape_Haira.raw"}})
	{
		const auto path = fileSystem.GetPath<Path::Data>() / name;
		if (!fileSystem.Exists(path))
		{
			continue;
		}
		try
		{
			textureManager.Load(id, resources::Texture2DLoader::FromDiskTag {}, path);
		}
		catch (std::runtime_error& err)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "{}", err.what());
		}
	}

	LoadCreatureSkinArt();
	view.StartupStep("snow, hair and skin art textures");

	return true;
}

void LoadHandMesh()
{
	auto& meshes = Locator::resources::value().GetMeshes();
	meshes.Erase(ecs::components::Hand::k_MeshId);
	meshes.Load(ecs::components::Hand::k_MeshId, resources::L3DLoader::FromDiskTag {},
	            Locator::filesystem::value().GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
}

// What creatures' tattoos and marks are painted with. The symbols the game ships with fill the cells no player's
// symbol has been written into.
void LoadCreatureSkinArt()
{
	auto& fileSystem = Locator::filesystem::value();
	using filesystem::Path;
	try
	{
		const auto dataDirectory = fileSystem.GetPath<Path::Data>();
		const auto texturesDirectory = fileSystem.GetPath<Path::Textures>();
		Locator::resources::value().GetCreatureSkinArt().Load(
		    creature_skin::k_ArtId, resources::CreatureSkinArtLoader::FromDiskTag {},
		    resources::CreatureSkinArtLoader::Paths {
		        .symbols = texturesDirectory / "PlayersSymbols.raw",
		        .defaultSymbols = texturesDirectory / "OriginalChooseSymbol.raw",
		        .freshDamage = dataDirectory / "damage_new256.raw",
		        .freshDamageAlpha = dataDirectory / "damage_new256A.raw",
		        .oldDamage = dataDirectory / "damage_old256.raw",
		        .oldDamageAlpha = dataDirectory / "damage_old256A.raw",
		        .palette = dataDirectory / "tattoocols.raw",
		    });
	}
	catch (const std::exception& err)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Creature skin art: {}", err.what());
	}
}

void ApplyPlayerSettings(const player_profiles::Settings& settings)
{
	// The help system, the dialogue and the creature help, each passed over while not made
	help::ApplyPlayerChoices(settings, help::Get(), TheDialogue(),
	                         Locator::creatureHelpSystem::has_value() ? &Locator::creatureHelpSystem::value() : nullptr);
	// The hand stays as the menu's box sets it, until the mirror is checked
	// The push scrolling, which the player's camera reads (Camera/EdgeScroll.h)
	player_camera::Get().pushScrolling = settings.pushScrolling;
}

} // namespace openblack::simulation
