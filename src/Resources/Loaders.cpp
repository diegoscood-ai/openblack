/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Resources/Loaders.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>
#include <utility>

#include <GLWFile.h>
#include <PackFile.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include "3D/L3DMesh.h"
#include "3D/Light.h"
#include "Audio/AudioManagerInterface.h"
#include "Common/StringUtils.h"
#include "Common/Zip.h"
#include "FileSystem/FileSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureUpscale.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::filesystem;
using namespace openblack::resources;

L3DLoader::result_type L3DLoader::operator()(FromBufferTag, const std::string& debugName,
                                             const std::vector<uint8_t>& data) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(debugName);
	if (!mesh->LoadFromBuffer(data))
	{
		throw std::runtime_error("Unable to load mesh");
	}

	return mesh;
}

L3DLoader::result_type L3DLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	auto mesh = std::make_shared<graphics::L3DMesh>(path.stem().string());
	auto pathExt = string_utils::LowerCase(path.extension().string());

	if (pathExt == ".l3d")
	{
		if (!mesh->LoadFromFilesystem(path))
		{
			throw std::runtime_error("Unable to load mesh");
		}
	}
	else if (pathExt == ".zzz")
	{
		auto stream = Locator::filesystem::value().Open(path, Stream::Mode::Read);
		uint32_t decompressedSize = 0;
		stream->Read(&decompressedSize);
		auto buffer = std::vector<uint8_t>(stream->Size() - sizeof(decompressedSize));
		stream->Read(buffer.data(), buffer.size());
		auto decompressedBuffer = zip::Inflate(buffer, decompressedSize);
		if (!mesh->LoadFromBuffer(decompressedBuffer))
		{
			throw std::runtime_error("Unable to load decompressed mesh");
		}
	}

	return mesh;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromPackTag, const std::string& name,
                                                         const pack::G3DTexture& g3dTexture) const
{
	// some assumptions:
	// - no mipmaps
	// - no cubemap or volume textures
	// - always dxt1 or dxt3
	// - all are compressed
	auto texture2D = std::make_shared<graphics::Texture2D>(name);
	graphics::TextureFormat internalFormat;
	if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT1"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression1;
	}
	else if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT3"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression2;
	}
	else if (g3dTexture.ddsHeader.format.fourCC.data() == std::string("DXT5"))
	{
		internalFormat = graphics::TextureFormat::BlockCompression3;
	}
	else
	{
		throw std::runtime_error("Unsupported compressed texture format");
	}

	texture2D->Create(static_cast<uint16_t>(g3dTexture.ddsHeader.width), static_cast<uint16_t>(g3dTexture.ddsHeader.height), 1,
	                  internalFormat, graphics::Wrapping::Repeat, graphics::SurfaceTextureFilter(),
	                  bgfx::makeRef(g3dTexture.ddsData.data(), static_cast<uint32_t>(g3dTexture.ddsData.size())));
	return texture2D;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromImageTag, const std::string& name,
                                                         const std::filesystem::path& imagePath) const
{
	std::ifstream stream(imagePath, std::ios::binary);
	const std::vector<uint8_t> file((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
	int width = 0;
	int height = 0;
	int channels = 0;
	auto* pixels = stbi_load_from_memory(file.data(), static_cast<int>(file.size()), &width, &height, &channels, 4);
	if (pixels == nullptr)
	{
		throw std::runtime_error("Unable to decode " + imagePath.string());
	}
	auto texture2D = std::make_shared<graphics::Texture2D>(name);
	// always mipmapped: the images are 4 times the size of the originals, and without mip levels they would shimmer as
	// soon as the villager is a few metres away
	texture2D->Create(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1, graphics::TextureFormat::RGBA8,
	                  graphics::Wrapping::Repeat, graphics::Filter::LinearMipmapLinear,
	                  bgfx::copy(pixels, static_cast<uint32_t>(width * height * 4)));
	stbi_image_free(pixels);
	return texture2D;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromDiskTag, const std::filesystem::path& rawTexturePath) const
{
	bool found = false;
	const std::array<uint16_t, 12> resolutions = {{1024, 512, 256, 128, 64, 40, 32, 14, 12, 6}};

	const auto data = Locator::filesystem::value().ReadAll(rawTexturePath);
	graphics::TextureFormat format = graphics::TextureFormat::R8;
	uint16_t width = 0;
	uint16_t height = 0;
	for (auto res : resolutions)
	{
		if (found)
		{
			break;
		}

		width = res;
		height = res;

		const typename decltype(data)::size_type pixelCount = width * height;

		if (data.size() == pixelCount)
		{
			format = graphics::TextureFormat::R8;
			found = true;
		}
		if (data.size() == 3 * pixelCount)
		{
			format = graphics::TextureFormat::RGB8;
			found = true;
		}
	}
	if (!found)
	{
		throw std::runtime_error("Unable to load texture: Ambiguous size and format: " + std::to_string(data.size()));
	}

	auto texture = std::make_shared<graphics::Texture2D>(("raw" / rawTexturePath.stem()).string());
	// Mod graphics.terrain-x2 (upscale option): the sea's sky.raw / skya.raw upscaled 2x with Lanczos-3, like the
	// landscape materials
	const auto stem = string_utils::LowerCase(rawTexturePath.stem().string());
	if (Locator::config::value().terrainTexturesX2 && (stem == "sky" || stem == "skya"))
	{
		const size_t channels = format == graphics::TextureFormat::RGB8 ? 3 : 1;
		std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4, 255);
		for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
		{
			for (size_t c = 0; c < 3; ++c)
			{
				rgba[i * 4 + c] = data[i * channels + (channels == 3 ? c : 0)];
			}
		}
		const auto upscaled = graphics::UpscaleRgba8Lanczos2x(rgba.data(), width, height, 1);
		texture->Create(width * 2, height * 2, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::Repeat,
		                graphics::SurfaceTextureFilter(), bgfx::copy(upscaled.data(), static_cast<uint32_t>(upscaled.size())));
		return texture;
	}
	texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::SurfaceTextureFilter(),
	                bgfx::copy(data.data(), static_cast<uint32_t>(data.size())));

	return texture;
}

L3DAnimLoader::result_type L3DAnimLoader::operator()(FromBufferTag, const std::vector<uint8_t>& data) const
{
	auto animation = std::make_shared<L3DAnim>();
	animation->LoadFromBuffer(data);
	return animation;
}

L3DAnimLoader::result_type L3DAnimLoader::operator()(FromDiskTag, const std::filesystem::path& path) const
{
	auto animation = std::make_shared<L3DAnim>();

	if (!animation->LoadFromFilesystem(path))
	{
		throw std::runtime_error("Unable to load animation");
	}

	return animation;
}

LevelLoader::result_type LevelLoader::operator()(FromDiskTag, const std::filesystem::path& path, Level::LandType landType) const
{
	return std::make_shared<Level>(Level::ParseLevel(path, landType));
}

CreatureMindLoader::result_type CreatureMindLoader::operator()(FromDiskTag, const std::filesystem::path& /*unused*/) const
{
	return std::make_shared<creature::CreatureMind>();
}

SoundLoader::result_type SoundLoader::operator()(BaseLoader<audio::Sound>::FromBufferTag,
                                                 const pack::AudioBankSampleHeader& header,
                                                 const std::vector<std::vector<uint8_t>>& buffer) const
{
	auto sound = std::make_shared<audio::Sound>();
	// Let's clean up the names as they're very difficult to read from the debug GUI
	sound->name = std::filesystem::path(header.name.data()).filename().string();
	sound->id = header.id;
	sound->priority = header.priority;
	sound->sampleRate = static_cast<int>(header.sampleRate);
	sound->bitRate = 0;
	// LH_BankSample +0x244 (unknown10/11) are override flags: a field of the .sad only counts when its bit is set
	// (LHaudiodllR 0x10011420): 0x1 pitch (+0x260, percent of the wav's rate), 0x20 volume (+0x25C, 0..127).
	// Otherwise the game's values apply: pitch 100, volume 127. The pitch deviation (percent) always applies.
	const uint32_t overrides = static_cast<uint32_t>(header.unknown10) | (static_cast<uint32_t>(header.unknown11) << 16);
	sound->volume = (overrides & 0x20u) != 0 ? static_cast<float>(std::min<int>(header.volume, 127)) / 127.0f : 1.0f;
	sound->pitch = (overrides & 0x1u) != 0 && header.pitch != 0 ? header.pitch : 100;
	sound->pitchDeviation = header.pitchDeviation;
	sound->playType = static_cast<audio::PlayType>(header.loopType);
	sound->buffer = buffer;
	return sound;
}

LightLoader::result_type LightLoader::operator()(BaseLoader<Lights>::FromDiskTag, const std::filesystem::path& path) const
{
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Loading lights from file: {}", path.string());
	glw::GLWFile glw;

	const auto result = glw.ReadFile(*Locator::filesystem::value().GetData(path));
	if (result != glw::GLWResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Failed to open glw file from filesystem {}: {}", path.string(),
		                    glw::ResultToStr(result));
		throw glw::ResultToStr(result);
	}
	auto lights = std::make_shared<Lights>();
	for (auto entry : glw.GetGlows())
	{
		Glow glow;
		glow.backgroundColour = glm::vec4(entry.red * 0.5f, entry.green * 0.5f, entry.blue * 0.5f, 1.0f);
		glow.brightSpotColour = glm::vec4 {100.0f / 256.0f, 172 / 256.0f, 146.0f / 256.0f, 1.0f};
		glow.backgroundScale = (1.0f / 3) * 2.0f;
		glow.brightSpotScale = 1.3f;
		glow.position = glm::vec3(entry.posX, entry.posY, entry.posZ);
		lights->emitters.emplace_back(LightEmitter {glow});
	}
	return lights;
}
