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
#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string_view>
#include <utility>

#include <GLWFile.h>
#include <PackFile.h>
#include <bgfx/bgfx.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>

#include "3D/L3DMesh.h"
#include "3D/Light.h"
#include "Common/StringUtils.h"
#include "Common/Zip.h"
#include "FileSystem/FileSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/Argb4444.h"
#include "Graphics/Rgb16.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureUpscale.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::filesystem;
using namespace openblack::resources;

namespace
{
/// The .raw colour files the original creates without the alpha flag 0x40 and loads through fn_00837400, which then
/// cuts them to 16 bits (no alpha: `[tex+0x10] & 0x40` = 0, 0x838079 / 0x838D37, `je` 0x8374D2) on the switch
/// [0xEDD46C] (0 on the hardware the game targets, graphics::rgb16): the 555 branch 0x837765..0x83779F, 5 bits per
/// channel (Pack555; D3D samples Unpack555, so each byte is rgb16::Cut5). Only a colour file of exactly 0x30000 bytes
/// gets there (fn_00837300 0x837318); another one goes through fn_0087F980 (0x8380A0). Of the 69 calls to Create
/// 0x8379E0, the ones with a file and no 0x40 are Sun.raw (flags 1, 0x81E844..0x81E851) and the saved games' pictures
/// screenshots_lores_%i_map_0.raw / screenshots_hires_%i_map_0.raw (flags 1, fn_00784070 0x78408F, fn_00784640
/// 0x78466A), which openblack does not load. (The landscape converter fn_008227A0 0x822874, a command-line mode, is
/// not in the game.) The halving of [0xEDD470] (step 6, 0x8374BB) is not ported, as for the 4444 cut.
constexpr auto k_Rgb555Stems = std::to_array<std::string_view>({
    "sun", // Create(".\Data\Textures\Sun.raw", 1) 0x81E844..0x81E851
});

bool IsRgb555Stem(std::string_view lowerStem)
{
	return std::ranges::find(k_Rgb555Stems, lowerStem) != k_Rgb555Stems.end();
}
} // namespace

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

Texture2DLoader::DecodedImage Texture2DLoader::Decode(const std::filesystem::path& imagePath)
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
	DecodedImage image {static_cast<uint16_t>(width), static_cast<uint16_t>(height),
	                    std::vector<uint8_t>(pixels, pixels + static_cast<size_t>(width) * height * 4)};
	stbi_image_free(pixels);
	return image;
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromImageTag tag, const std::string& name,
                                                         const std::filesystem::path& imagePath) const
{
	return (*this)(tag, name, Decode(imagePath));
}

Texture2DLoader::result_type Texture2DLoader::operator()(FromImageTag, const std::string& name,
                                                         const DecodedImage& image) const
{
	auto texture2D = std::make_shared<graphics::Texture2D>(name);
	// always mipmapped: the images are 4 times the size of the originals, and without mip levels they would shimmer as
	// soon as the villager is a few metres away
	texture2D->Create(image.width, image.height, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::Repeat,
	                  graphics::Filter::LinearMipmapLinear,
	                  bgfx::copy(image.rgba.data(), static_cast<uint32_t>(image.rgba.size())));
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
	const auto stem = string_utils::LowerCase(rawTexturePath.stem().string());
	// The original cuts every texture with the alpha flag (and human_shadow) to ARGB4444 when it loads it, and D3D
	// filters the cut texels (Graphics/Argb4444.h): fn_00837400 only takes a 0x30000-byte colour file (0x837318) and
	// reads min(length, 0x10000) bytes of its alpha (0x837600, 0x7BCEC9; openblack cuts an alpha file only if it has
	// exactly 0x10000 bytes, aproximado);
	// fn_0081FAA0 cuts human_shadow (0x81FCDD). Mod graphics.smooth-smoke keeps smokea's 8 bits.
	// An alpha file is cut only with a valid colour file beside it: fn_00837400 needs the 0x30000-byte colour before
	// it reads xa.raw (argb4444::ColourOfAlpha), so S_IceEnvMapGreya.raw stays 8-bit.
	const auto validColourOfAlpha = [&]() {
		const auto fileStem = rawTexturePath.stem().string();
		const auto colour = graphics::argb4444::ColourOfAlpha(fileStem);
		if (colour.empty())
		{
			return false;
		}
		auto& fileSystem = Locator::filesystem::value();
		const auto colourPath = rawTexturePath.parent_path() / (std::string(colour) + ".raw");
		return fileSystem.Exists(colourPath) &&
		       fileSystem.Open(colourPath, filesystem::Stream::Mode::Read)->Size() == graphics::argb4444::k_ColourBytes;
	};
	const bool alphaFlagSize =
	    format == graphics::TextureFormat::RGB8
	        ? data.size() == graphics::argb4444::k_ColourBytes && graphics::argb4444::IsAlphaFlagColour(stem)
	        : data.size() == graphics::argb4444::k_AlphaBytes && validColourOfAlpha();
	const bool cut = (alphaFlagSize &&
	                  !(stem == "smokea" && Locator::config::value().smoothSmokeAlpha)) ||
	                 stem == graphics::argb4444::k_HumanShadowStem;
	// Mod graphics.terrain-x2 (upscale option): the sea's sky.raw / skya.raw upscaled 2x with Lanczos-3, like the
	// landscape materials, and cut after the upscale
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
		auto upscaled = graphics::UpscaleRgba8Lanczos2x(rgba.data(), width, height, 1);
		if (cut)
		{
			std::ranges::transform(upscaled, upscaled.begin(), graphics::argb4444::Cut);
		}
		texture->Create(width * 2, height * 2, 1, graphics::TextureFormat::RGBA8, graphics::Wrapping::Repeat,
		                graphics::SurfaceTextureFilter(), bgfx::copy(upscaled.data(), static_cast<uint32_t>(upscaled.size())));
		return texture;
	}
	// the 555 branch of fn_00837400 (0x837765..0x83779F): each of R, G, B to 5 bits and back as D3D samples it
	if (!cut && format == graphics::TextureFormat::RGB8 && data.size() == graphics::argb4444::k_ColourBytes &&
	    IsRgb555Stem(stem))
	{
		std::vector<uint8_t> cut555(data.size());
		std::ranges::transform(data, cut555.begin(), graphics::rgb16::Cut5);
		texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::SurfaceTextureFilter(),
		                bgfx::copy(cut555.data(), static_cast<uint32_t>(cut555.size())));
		return texture;
	}
	if (cut)
	{
		// x.raw and xa.raw stay two textures here (fn_00837400 packs them into one): each byte Cut on its own
		std::vector<uint8_t> nibbles(data.size());
		std::ranges::transform(data, nibbles.begin(), graphics::argb4444::Cut);
		texture->Create(width, height, 1, format, graphics::Wrapping::Repeat, graphics::SurfaceTextureFilter(),
		                bgfx::copy(nibbles.data(), static_cast<uint32_t>(nibbles.size())));
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
	// the volume is the low u16 at +0x25C (openblack's `volume` byte), the user parameter its high u16
	uint32_t volumeWord = 0;
	std::memcpy(&volumeWord, reinterpret_cast<const char*>(&header) + 0x25C, sizeof(volumeWord));
	sound->overrides = overrides;
	sound->volume127 = (overrides & 0x20u) != 0 ? std::min<int>(static_cast<int>(volumeWord & 0xFFFFu), 127) : 127;
	// QMixer's law (sample_play::QMixerGain): floor(master 127 * v / 127) * 258 / 32767
	sound->volume = static_cast<float>(sound->volume127 * 258) / 32767.0f;
	sound->userParam = static_cast<int>(volumeWord >> 16);
	sound->loops = (overrides & 0x40u) != 0 ? static_cast<int>(header.loop) : 0;
	sound->pitch = (overrides & 0x1u) != 0 && header.pitch != 0 ? header.pitch : 100;
	sound->pitchDeviation = header.pitchDeviation;
	sound->maxDistance = header.maxDist;
	// 0x80 minDist (+0x268), 0x100 maxDist (+0x26C), 0x200 scale (+0x270) -> QSWaveMixSetDistanceMapping; defaults
	// of LH_SamplePlayOptions (0x10010E90): 1, 9999, 0.3. 0x400 play mode (+0x274), default 3.
	sound->minDistance = (overrides & 0x80u) != 0 ? header.minDist : 1.0f;
	sound->mappingMaxDistance = (overrides & 0x100u) != 0 ? header.maxDist : 9999.0f;
	sound->scale = (overrides & 0x200u) != 0 ? header.scale : 0.3f;
	sound->playMode = (overrides & 0x400u) != 0 ? static_cast<int>(header.loopType) : 3;
	sound->cloneGroup = static_cast<uint16_t>(header.group);
	sound->atmosGroup = static_cast<uint16_t>(header.atmosGroup);
	// the atmos frequency is the whole u32 at +0x27C: openblack's `atmos` (u16) plus the struct's tail padding
	static_assert(offsetof(pack::AudioBankSampleHeader, loop) == 0x248);
	static_assert(offsetof(pack::AudioBankSampleHeader, minDist) == 0x268);
	static_assert(offsetof(pack::AudioBankSampleHeader, loopType) == 0x274);
	static_assert(offsetof(pack::AudioBankSampleHeader, atmos) == 0x27C);
	static_assert(sizeof(pack::AudioBankSampleHeader) == 0x280);
	std::memcpy(&sound->atmosFrequency, reinterpret_cast<const char*>(&header) + 0x27C, sizeof(sound->atmosFrequency));
	// +0x108 the wave's sample, +0x124 WAVEFORMATEX.wFormatTag, +0x138 / +0x13C the loop section (engine.md §1.5)
	static_assert(offsetof(pack::AudioBankSampleHeader, isBank) == 0x108);
	static_assert(offsetof(pack::AudioBankSampleHeader, unknown6a) == 0x124);
	static_assert(offsetof(pack::AudioBankSampleHeader, lStart) == 0x138);
	static_assert(offsetof(pack::AudioBankSampleHeader, lEnd) == 0x13C);
	sound->wave = header.isBank;
	sound->waveFormat = static_cast<uint16_t>(header.unknown6a);
	sound->loopStart = header.lStart;
	sound->loopEnd = header.lEnd;
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
