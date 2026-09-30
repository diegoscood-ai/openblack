/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HdTweaks.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <future>
#include <unordered_map>
#include <string_view>

#include <PackFile.h>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "ECS/Components/Hand.h"
#include "ECS/DetailMeshes.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Mods/ModRegistry.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::resources::hd_tweaks
{
namespace
{
// the options the loaded textures and meshes were made with
bool s_loadedTextures = false;
int s_loadedSmoothLevel = 0;
bool s_begun = false;
// the HD images being decoded, all at once on other threads (one image at a time took seconds)
std::unordered_map<uint32_t, std::future<Texture2DLoader::DecodedImage>> s_decoding;

bool IsSkin(const std::vector<uint32_t>& skins, uint32_t skinID)
{
	return std::ranges::find(skins, skinID) != skins.end();
}

// the meshes L3DSubMesh smooths (IsSmoothedPerson): boned, with a sub-mesh whose textures are all villager textures
bool IsPersonMesh(const graphics::L3DMesh& mesh, const std::vector<uint32_t>& skins)
{
	return mesh.IsBoned() && std::ranges::any_of(mesh.GetSubMeshes(), [&skins](const auto& subMesh) {
		       const auto& primitives = subMesh->GetPrimitives();
		       return !primitives.empty() && std::ranges::all_of(primitives, [&skins](const auto& primitive) {
			              return IsSkin(skins, primitive.skinID);
		              });
	       });
}
// hook OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth> (e.g. 600:hd,round): at that frame the options change as in
// the Mods menu (the mod on), to check the reload
void RunTestHook()
{
	static int frame = 0;
	const char* test = std::getenv("OPENBLACK_TEST_HD_TWEAKS");
	if (test == nullptr || !Locator::mods::has_value())
	{
		return;
	}
	const std::string_view value(test);
	const auto colon = value.find(':');
	const auto comma = value.find(',');
	if (colon == std::string_view::npos || comma == std::string_view::npos || ++frame != std::atoi(test))
	{
		return;
	}
	auto& registry = Locator::mods::value();
	auto* mod = registry.Find("graphics.hd-tweaks");
	if (mod == nullptr)
	{
		return;
	}
	registry.SetEnabled(*mod, true);
	const std::string_view choices[] = {value.substr(colon + 1, comma - colon - 1), value.substr(comma + 1)};
	for (size_t i = 0; i < mod->GetOptions().size() && i < 2; ++i)
	{
		const auto& option = mod->GetOptions()[i];
		const auto choice = std::ranges::find(option.choices, choices[i]);
		if (choice != option.choices.end())
		{
			registry.SetOption(*mod, i, static_cast<size_t>(choice - option.choices.begin()));
		}
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "HD-Tweaks test: options set to {} at frame {}", value.substr(colon + 1),
	                   frame);
}
} // namespace

HdTextures Begin()
{
	auto& config = Locator::config::value();
	// the list is read even with the mod off, so that it can be turned on later
	auto hdTextures = Locator::mods::has_value()
	                      ? HdTextures(Locator::mods::value().GetModFilesDirectory("graphics.hd-tweaks"))
	                      : HdTextures();
	config.hdTweaksSkins = hdTextures.Ids();
	s_loadedTextures = config.hdTweaksTextures;
	s_loadedSmoothLevel = config.hdTweaksSmoothLevel;
	s_begun = true;
	s_decoding.clear();
	if (config.hdTweaksTextures)
	{
		for (const auto id : config.hdTweaksSkins)
		{
			if (const auto image = hdTextures.ImagePath(id); std::filesystem::exists(image))
			{
				s_decoding[id] = std::async(std::launch::async, [image] { return Texture2DLoader::Decode(image); });
			}
		}
	}
	return hdTextures;
}

void LoadTexture(const HdTextures& hdTextures, const std::string& name, const pack::G3DTexture& g3dTexture)
{
	auto& textureManager = Locator::resources::value().GetTextures();
	if (const auto image = Locator::config::value().hdTweaksTextures ? hdTextures.Find(g3dTexture.header.id, g3dTexture.ddsData)
	                                                                 : std::filesystem::path();
	    !image.empty())
	{
		if (auto decoding = s_decoding.find(g3dTexture.header.id); decoding != s_decoding.end())
		{
			try
			{
				textureManager.Load(g3dTexture.header.id, Texture2DLoader::FromImageTag {}, name, decoding->second.get());
				s_decoding.erase(decoding);
				return;
			}
			catch (const std::exception& error)
			{
				s_decoding.erase(decoding);
				SPDLOG_LOGGER_ERROR(spdlog::get("game"), "HD textures: {}", error.what());
			}
		}
		else
		{
			textureManager.Load(g3dTexture.header.id, Texture2DLoader::FromImageTag {}, name, image);
			return;
		}
	}
	textureManager.Load(g3dTexture.header.id, Texture2DLoader::FromPackTag {}, name, g3dTexture);
}

void Update()
{
	RunTestHook();
	ecs::detail_meshes::Update();
	const auto& config = Locator::config::value();
	if (!s_begun || (config.hdTweaksTextures == s_loadedTextures && config.hdTweaksSmoothLevel == s_loadedSmoothLevel))
	{
		return;
	}
	const auto start = std::chrono::steady_clock::now();
	const bool texturesChanged = config.hdTweaksTextures != s_loadedTextures;
	const bool meshesChanged = config.hdTweaksSmoothLevel != s_loadedSmoothLevel;
	const auto hdTextures = Begin();

	auto& fileSystem = Locator::filesystem::value();
	pack::PackFile pack;
	const auto result =
	    pack.ReadFile(*fileSystem.GetData(fileSystem.GetPath<filesystem::Path::Data>() / "AllMeshes.g3d"));
	if (result != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "HD-Tweaks: unable to reload AllMeshes.g3d: {}",
		                    pack::ResultToStr(result));
		return;
	}

	const auto packRead = std::chrono::steady_clock::now();
	auto& resources = Locator::resources::value();
	const auto& skins = config.hdTweaksSkins;
	int textures = 0;
	if (texturesChanged)
	{
		auto& textureManager = resources.GetTextures();
		for (const auto& [name, g3dTexture] : pack.GetTextures())
		{
			if (!IsSkin(skins, g3dTexture.header.id))
			{
				continue;
			}
			if (textureManager.Contains(g3dTexture.header.id))
			{
				textureManager.Erase(g3dTexture.header.id);
			}
			LoadTexture(hdTextures, name, g3dTexture);
			++textures;
		}
	}

	const auto texturesDone = std::chrono::steady_clock::now();
	int meshes = 0;
	if (meshesChanged)
	{
		auto& meshManager = resources.GetMeshes();
		const auto& packMeshes = pack.GetMeshes();
		for (size_t i = 0; i < packMeshes.size() && i < k_MeshNames.size(); ++i)
		{
			const auto meshId = static_cast<MeshId>(i);
			if (!meshManager.Contains(meshId) || !IsPersonMesh(*meshManager.Handle(HashIdentifier(meshId)), skins))
			{
				continue;
			}
			meshManager.Erase(meshId);
			meshManager.Load(meshId, L3DLoader::FromBufferTag {}, k_MeshNames.at(i), packMeshes[i]);
			++meshes;
		}
		// the hand (Game: Hand_Boned_Base2.l3d)
		if (meshManager.Contains(ecs::components::Hand::k_MeshId))
		{
			meshManager.Erase(ecs::components::Hand::k_MeshId);
			meshManager.Load(ecs::components::Hand::k_MeshId, L3DLoader::FromDiskTag {},
			                 fileSystem.GetPath<filesystem::Path::CreatureMesh>() / "Hand_Boned_Base2.l3d");
			++meshes;
		}
	}

	const auto ms = [](auto from, auto to) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
	};
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "HD-Tweaks: {} textures and {} meshes reloaded in {} ms (pack {} ms, textures {} ms, meshes {} ms)",
	                   textures, meshes, ms(start, std::chrono::steady_clock::now()), ms(start, packRead),
	                   ms(packRead, texturesDone), ms(texturesDone, std::chrono::steady_clock::now()));
}

} // namespace openblack::resources::hd_tweaks
