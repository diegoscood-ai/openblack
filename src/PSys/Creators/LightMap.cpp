/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LightMap.h"

#include <cmath>

#include <algorithm>
#include <memory>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "Common/StringUtils.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "PSys/PSysFile.h"
#include "PSys/PSysRegistry.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
constexpr int k_AtlasCell = 32; ///< one frame upscaled to 32 x 32 in an 8 x 8 atlas: the sprite path's cell layout
constexpr int k_AtlasSide = 8;

/// GJBitmap::LoadBitmapFromFile(name, Pitch, 3, NumFramesInFile, NumFramesInUse) (GetBitmap 0x6A9D40): Pitch x Pitch
/// square frames stacked in the file, RGB (3 B/px) or grey (1 B/px) depending on its size
struct Bitmap
{
	int pitch {0};
	int frames {0};
	int channels {0};
	std::vector<uint8_t> data;
};

Bitmap LoadBitmap(const std::string& path, int pitch, int framesInFile)
{
	Bitmap bitmap;
	if (!Locator::filesystem::has_value() || pitch <= 0 || framesInFile <= 0)
	{
		return bitmap;
	}
	auto name = path;
	std::replace(name.begin(), name.end(), '\\', '/');
	if (name.starts_with("./"))
	{
		name = name.substr(2);
	}
	if (string_utils::LowerCase(name).starts_with("data/"))
	{
		name = name.substr(5);
	}
	try
	{
		auto& fileSystem = Locator::filesystem::value();
		auto bytes = fileSystem.ReadAll(fileSystem.GetPath<filesystem::Path::Data>() / name);
		const auto pixels = static_cast<size_t>(pitch) * static_cast<size_t>(pitch) * static_cast<size_t>(framesInFile);
		bitmap.channels = bytes.size() >= pixels * 3 ? 3 : 1;
		if (bytes.size() < pixels * static_cast<size_t>(bitmap.channels))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: light map {}: {} bytes, {} x {} x {} expected", name, bytes.size(),
			                   pitch, pitch, framesInFile);
			return bitmap;
		}
		bitmap.pitch = pitch;
		bitmap.frames = framesInFile;
		bitmap.data = std::move(bytes);
	}
	catch (const std::exception& e)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "PSys: light map {}: {}", name, e.what());
	}
	return bitmap;
}

/// Bilinear sample of one frame (the frames are 5 x 5 or so: they are drawn upscaled, as the blit's stretch does)
glm::vec3 Sample(const Bitmap& bitmap, int frame, float u, float v)
{
	const auto at = [&bitmap, frame](int x, int y) {
		const auto index = (static_cast<size_t>(frame) * static_cast<size_t>(bitmap.pitch) * static_cast<size_t>(bitmap.pitch) +
		                    static_cast<size_t>(y) * static_cast<size_t>(bitmap.pitch) + static_cast<size_t>(x)) *
		                   static_cast<size_t>(bitmap.channels);
		if (bitmap.channels == 3)
		{
			return glm::vec3(bitmap.data[index], bitmap.data[index + 1], bitmap.data[index + 2]);
		}
		return glm::vec3(bitmap.data[index]);
	};
	const float fx = std::clamp(u * static_cast<float>(bitmap.pitch) - 0.5f, 0.0f, static_cast<float>(bitmap.pitch - 1));
	const float fy = std::clamp(v * static_cast<float>(bitmap.pitch) - 0.5f, 0.0f, static_cast<float>(bitmap.pitch - 1));
	const int x0 = static_cast<int>(fx);
	const int y0 = static_cast<int>(fy);
	const int x1 = std::min(x0 + 1, bitmap.pitch - 1);
	const int y1 = std::min(y0 + 1, bitmap.pitch - 1);
	const float tx = fx - static_cast<float>(x0);
	const float ty = fy - static_cast<float>(y0);
	const auto top = at(x0, y0) * (1.0f - tx) + at(x1, y0) * tx;
	const auto bottom = at(x0, y1) * (1.0f - tx) + at(x1, y1) * tx;
	return top * (1.0f - ty) + bottom * ty;
}

/// The frames of the .raw as one 8 x 8 atlas of 32 x 32 cells, registered under "raw/<name>" and "raw/<name>a" so that
/// the sprite pass (RendererPSys.cpp) draws them like any other animated sprite
std::string BuildAtlas(const std::string& path, int pitch, int framesInFile)
{
	auto name = path;
	std::replace(name.begin(), name.end(), '\\', '/');
	if (const auto slash = name.find_last_of('/'); slash != std::string::npos)
	{
		name = name.substr(slash + 1);
	}
	if (const auto dot = name.find_last_of('.'); dot != std::string::npos)
	{
		name = name.substr(0, dot);
	}
	const std::string id = "lightmap_" + name;
	if (!Locator::resources::has_value())
	{
		return id;
	}
	auto& textures = Locator::resources::value().GetTextures();
	const auto diffuseId = entt::hashed_string(("raw/" + id).c_str()).value();
	const auto alphaId = entt::hashed_string(("raw/" + id + "a").c_str()).value();
	if (textures.Contains(diffuseId))
	{
		return id;
	}
	const auto bitmap = LoadBitmap(path, pitch, framesInFile);
	if (bitmap.frames == 0)
	{
		return id;
	}
	constexpr int side = k_AtlasSide * k_AtlasCell;
	resources::Texture2DLoader::DecodedImage diffuse {static_cast<uint16_t>(side), static_cast<uint16_t>(side),
	                                                 std::vector<uint8_t>(static_cast<size_t>(side) * side * 4, 0)};
	auto alpha = diffuse;
	for (int frame = 0; frame < std::min(bitmap.frames, k_AtlasSide * k_AtlasSide); ++frame)
	{
		const int cellX = (frame % k_AtlasSide) * k_AtlasCell;
		const int cellY = (frame / k_AtlasSide) * k_AtlasCell;
		for (int y = 0; y < k_AtlasCell; ++y)
		{
			for (int x = 0; x < k_AtlasCell; ++x)
			{
				const auto colour = Sample(bitmap, frame, (static_cast<float>(x) + 0.5f) / k_AtlasCell,
				                           (static_cast<float>(y) + 0.5f) / k_AtlasCell);
				// (aproximado) the port's quad: alpha = max(R, G, B), the frame bilinearly upscaled to 32 x 32, and the
				// global light-map level 0xECA664 = clamp(fade) x 190 of AddDrawing 0x6CA6E0 is not applied
				// (part_render.md §8)
				const auto luminance = static_cast<uint8_t>(std::clamp(std::max({colour.r, colour.g, colour.b}), 0.0f, 255.0f));
				const auto offset = (static_cast<size_t>(cellY + y) * side + static_cast<size_t>(cellX + x)) * 4;
				for (int c = 0; c < 3; ++c)
				{
					diffuse.rgba[offset + static_cast<size_t>(c)] = static_cast<uint8_t>(std::clamp(colour[c], 0.0f, 255.0f));
					alpha.rgba[offset + static_cast<size_t>(c)] = luminance;
				}
				diffuse.rgba[offset + 3] = luminance;
				alpha.rgba[offset + 3] = luminance;
			}
		}
	}
	textures.Load(diffuseId, resources::Texture2DLoader::FromImageTag {}, "raw/" + id, diffuse);
	textures.Load(alphaId, resources::Texture2DLoader::FromImageTag {}, "raw/" + id + "a", alpha);
	return id;
}

std::unique_ptr<Creator> MakeLightMapCreator(const Object& object)
{
	auto creator = std::make_unique<LightMapCreator>();
	ReadCreatorProperties(object, *creator);
	// ctor 0x6A9CE0 defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter
	creator->pitch = std::max(1, object.Int("Pitch", 1));
	creator->numFramesInFile = std::max(1, object.Int("NumFramesInFile", 1));
	creator->numFramesInUse = std::clamp(object.Int("NumFramesInUse", 1), 1, creator->numFramesInFile);
	creator->randJitter = object.Float("RandJitter", 0.0f);
	creator->useRandJitter = object.Bool("UseRandJitter", false);
	creator->shiftX = object.Float("ShiftX", 0.0f);
	creator->shiftZ = object.Float("ShiftZ", 0.0f);
	// drawn by the sprite pass: a flat additive quad of the atlas, one cell per frame
	creator->kind = Creator::Kind::Sprite;
	creator->texture = BuildAtlas(object.String("TextureFileName"), creator->pitch, creator->numFramesInFile);
	creator->spritesPerRow = k_AtlasSide;
	creator->numFrames = creator->numFramesInUse;
	creator->fileOffset = 0;
	creator->initFrame = 0;
	creator->frameRate = object.Float("FrameRate", 1.0f);
	creator->playAnim = object.Bool("PlayAnim", false);
	creator->loopAnim = object.Bool("LoopAnim", false);
	creator->additive = true;
	creator->writeDepth = false;
	creator->horizontal = true; // it lies on the ground
	creator->ignoreRotation = false;
	creator->stretch = 1.0f;
	creator->scaleAlpha = 255;
	return creator;
}
} // namespace

void LightMapCreator::InitAtom(Effect& effect, Atom& atom) const
{
	if (useRandJitter && randJitter != 0.0f)
	{
		// DrawAt 0x67B220: LocalFloatRand(RandJitter) per axis. (aproximado) the original adds it on every draw (the
		// light flickers); here it is added once, when the atom is made
		atom.position += glm::vec3(effect.Random(randJitter), effect.Random(randJitter), effect.Random(randJitter));
	}
}

void openblack::psys::RegisterLightMapCreator()
{
	RegisterCreator("ParticleLightMapCreator", MakeLightMapCreator);
}
