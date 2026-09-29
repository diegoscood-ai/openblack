/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Texture2D.h"

#include <cassert>

#include <bgfx/bgfx.h>
#include <glm/ext/vector_uint2_sized.hpp>
#include <spdlog/spdlog.h>
#include <stb_image_write.h>

#include "EngineConfig.h"
#include "GraphicsHandleBgfx.h"
#include "Locator.h"
#include "TextureMipmaps.h"

namespace bgfx
{
// Defined and exported by bgfx but not declared in bgfx.h: frees a Memory that is not handed to bgfx.
void release(const Memory* _mem);
} // namespace bgfx

namespace openblack::graphics
{

Filter SurfaceTextureFilter()
{
	const auto& config = Locator::config::value();
	return config.textureMipmaps || config.anisotropicFiltering ? Filter::LinearMipmapLinear : Filter::Linear;
}

Texture2D::Texture2D(std::string name)
    : _name(std::move(name))
    , _handle(BGFX_INVALID_HANDLE)
{
}

Texture2D::~Texture2D()
{
	if (bgfx::isValid(toBgfx(_handle)))
	{
		bgfx::destroy(toBgfx(_handle));
	}
}

void Texture2D::Create(uint16_t width, uint16_t height, uint16_t layers, TextureFormat format, Wrapping wrapping, Filter filter,
                       const void* memory) noexcept
{
	uint64_t flags = BGFX_TEXTURE_NONE;
	switch (wrapping)
	{
	case Wrapping::ClampEdge:
		flags |= BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		break;
	case Wrapping::ClampBorder:
		flags |= BGFX_SAMPLER_U_BORDER | BGFX_SAMPLER_V_BORDER;
		break;
	case Wrapping::Repeat:
		break;
	case Wrapping::MirroredRepeat:
		flags |= BGFX_SAMPLER_U_MIRROR | BGFX_SAMPLER_V_MIRROR;
		break;
	}
	switch (filter)
	{
	case Filter::Nearest:
		flags |= BGFX_SAMPLER_POINT;
		break;
	case Filter::Linear:
		break;
	case Filter::LinearMipmapLinear:
		// mipmap mod: trilinear (the default bgfx mip filter), optionally anisotropic
		if (Locator::config::value().anisotropicFiltering)
		{
			flags |= BGFX_SAMPLER_MIN_ANISOTROPIC | BGFX_SAMPLER_MAG_ANISOTROPIC;
		}
		break;
	default:
		assert(false);
	}

	const auto* bgfxMemory = reinterpret_cast<const bgfx::Memory*>(memory);
	const bool hasMips = filter == Filter::LinearMipmapLinear && bgfxMemory != nullptr;
	if (hasMips)
	{
		const auto chain = BuildRgba8MipChain(bgfxMemory->data, width, height, layers, toBgfx(format));
		bgfx::release(bgfxMemory);
		bgfxMemory = bgfx::copy(chain.data(), static_cast<uint32_t>(chain.size()));
		format = TextureFormat::RGBA8;
	}
	_samplerFlags = static_cast<uint32_t>(flags & BGFX_SAMPLER_BITS_MASK);
	_handle = fromBgfx(bgfx::createTexture2D(width, height, hasMips, layers, toBgfx(format), flags, bgfxMemory));
	bgfx::setName(toBgfx(_handle), _name.c_str());
	bgfx::frame();

	bgfx::TextureInfo textureInfo;
	bgfx::calcTextureSize(textureInfo, width, height, 1, false, false, layers, toBgfx(format));
	_resolution = glm::u16vec2(textureInfo.width, textureInfo.height);
	_numLayers = textureInfo.numLayers;
	_format = fromBgfx(textureInfo.format);
	_stride = textureInfo.width * textureInfo.bitsPerPixel / 8;
	_storageSize = textureInfo.storageSize;

	bgfx::frame();
}

void Texture2D::DumpTexture() const
{
	assert(!_name.empty());
	std::vector<uint8_t> pixels;
	pixels.resize(_storageSize);
	bgfx::readTexture(toBgfx(_handle), pixels.data());
	bgfx::frame();
	// TODO(bwrsandman): get the number of components from _info.format
	const auto numComponents = 4u;
	for (uint16_t i = 0; i < _numLayers; ++i)
	{
		auto filename = "dump/" + _name + "_" + std::to_string(i) + ".png";
		SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Writing texture layer {} to {}.", i, filename.c_str());
		auto* currentPixels = &pixels[i * _stride * _resolution.y];
		auto writeResult =
		    stbi_write_png(filename.c_str(), _resolution.x, _resolution.y, numComponents, currentPixels, _stride);
		if (writeResult == 0)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Writing texture to {} failed!", filename.c_str());
			break;
		}
	}
}

} // namespace openblack::graphics
