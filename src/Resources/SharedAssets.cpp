/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SharedAssets.h"

#include <exception>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::resources::shared_assets
{

LoadResult LoadPowerUpBand(std::string_view label)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return LoadResult::Failed;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(k_PowerUpBandMesh.value()))
	{
		return LoadResult::AlreadyLoaded;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		meshes.Load(
		    k_PowerUpBandMesh.value(), L3DLoader::FromDiskTag {},
		    fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Data>() / "Spells" / "Meshes" / "Power_Up_Band.L3d"));
		meshes.Handle(k_PowerUpBandMesh.value())
		    ->SetMaterialProperties({.additive = true, .zWrite = false, .doubleSided = true, .change = true, .alpha = true});
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "{}: cannot load Power_Up_Band.L3d: {}", label, e.what());
		return LoadResult::Failed;
	}
	return LoadResult::LoadedNow;
}

LoadResult LoadSpriteSheet3a(std::string_view label)
{
	auto& textures = Locator::resources::value().GetTextures();
	if (textures.Contains(k_SpriteSheet3a.value()))
	{
		return LoadResult::AlreadyLoaded;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		textures.Load(k_SpriteSheet3a.value(), Texture2DLoader::FromDiskTag {},
		              fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "S_SpriteSheet3a.raw"));
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "{}: cannot load S_SpriteSheet3a.raw: {}", label, e.what());
		return LoadResult::Failed;
	}
	return LoadResult::LoadedNow;
}

LoadResult LoadLeashCollar(std::string_view label)
{
	if (!Locator::resources::has_value() || !Locator::filesystem::has_value())
	{
		return LoadResult::Failed;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	if (meshes.Contains(k_LeashCollarMesh.value()))
	{
		return LoadResult::AlreadyLoaded;
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		auto& textures = Locator::resources::value().GetTextures();
		if (!textures.Contains(k_LeashCollarTexture.value()))
		{
			const auto directory = fileSystem.GetPath<filesystem::Path::Textures>();
			// a missing leasha.raw is allowed: the cut takes the alpha from what is there
			textures.Load(k_LeashCollarTexture.value(), Texture2DLoader::FromColourAlphaTag {}, "misc/leash_collar",
			              Texture2DLoader::ColourAlphaDesc {
			                  .colour = fileSystem.FindPath(directory / "leash.raw"),
			                  .alpha = directory / "leasha.raw",
			                  .packing = Texture2DLoader::ColourAlpha::Argb4444,
			                  .wrap = graphics::Wrapping::Repeat,
			              });
		}
		meshes.Load(k_LeashCollarMesh.value(), L3DLoader::FromDiskTag {},
		            fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Misc>() / "leash.l3d"));
		// the file's primitives name a skin it does not carry; the game gives every textured one the rope's texture
		meshes.Handle(k_LeashCollarMesh.value())->SetSkinOfTextured(k_LeashCollarTexture.value());
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "{}: cannot load the leash collar: {}", label, e.what());
		return LoadResult::Failed;
	}
	return LoadResult::LoadedNow;
}

} // namespace openblack::resources::shared_assets
