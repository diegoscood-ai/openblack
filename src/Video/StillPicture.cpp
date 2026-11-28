/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StillPicture.h"

#include <utility>

#include <BinkFile.h>
#include <spdlog/spdlog.h>

#include "Graphics/Rgb16.h"
#include "Video/VideoDecoder.h"

using namespace openblack;
using namespace openblack::video;

/// The file and its decoder stay where they are: the decoder reads the file it opened
struct VideoFile::State
{
	BikFile file;
	std::unique_ptr<IVideoDecoder> decoder;
	/// width * height texels, zeroed when the film opens
	std::vector<uint16_t> framebuffer;
	/// The framebuffer as it is sampled
	std::vector<uint8_t> rgba;
	uint32_t serial {0};
};

VideoFile::VideoFile(std::unique_ptr<State> state)
    : _state(std::move(state))
{
}

VideoFile::VideoFile(VideoFile&&) noexcept = default;
VideoFile& VideoFile::operator=(VideoFile&&) noexcept = default;
VideoFile::~VideoFile() = default;

std::optional<VideoFile> VideoFile::Open(const std::filesystem::path& path)
{
	auto state = std::make_unique<State>();
	if (!state->file.Open(path))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Video: cannot open {}: {}", path.generic_string(), state->file.GetError());
		return std::nullopt;
	}
	// The full-screen player's decoder: none when the films are off or the build has none
	state->decoder = VideoPlayer::GameHooks().makeDecoder();
	if (state->decoder == nullptr || !state->decoder->Open(state->file))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Video: {} is not decoded", path.generic_string());
		return std::nullopt;
	}
	state->framebuffer.assign(static_cast<size_t>(state->file.Width()) * state->file.Height(), 0);
	state->rgba.assign(state->framebuffer.size() * 4, 0);
	graphics::rgb16::Expand(k_DefaultFormat, state->framebuffer, state->rgba);
	return VideoFile(std::move(state));
}

bool VideoFile::Decode(uint32_t frame)
{
	auto& state = *_state;
	if (frame >= state.file.FrameCount() || state.framebuffer.empty())
	{
		return false;
	}
	if (state.decoder->DecodeOnly(frame) < state.rgba.size())
	{
		return false;
	}
	state.decoder->CopyPicture(k_DefaultFormat, state.framebuffer, state.rgba);
	++state.serial;
	return true;
}

std::span<const uint8_t> VideoFile::GetRgba() const
{
	return _state->rgba;
}

std::span<const uint16_t> VideoFile::GetRgb16() const
{
	return _state->framebuffer;
}

uint32_t VideoFile::GetSerial() const
{
	return _state->serial;
}

uint32_t VideoFile::Width() const
{
	return _state->file.Width();
}

uint32_t VideoFile::Height() const
{
	return _state->file.Height();
}

uint32_t VideoFile::FrameCount() const
{
	return _state->file.FrameCount();
}

uint32_t VideoFile::FpsNumerator() const
{
	return _state->file.FpsNumerator();
}

uint32_t VideoFile::FpsDenominator() const
{
	return _state->file.FpsDenominator();
}

std::optional<StillPicture> video::ReadStill(const std::filesystem::path& path, uint32_t frame)
{
	auto file = VideoFile::Open(path);
	if (!file || !file->Decode(frame))
	{
		return std::nullopt;
	}
	const auto rgba = file->GetRgba();
	const auto rgb16 = file->GetRgb16();
	return StillPicture {
	    .rgba = {rgba.begin(), rgba.end()},
	    .rgb16 = {rgb16.begin(), rgb16.end()},
	    .width = file->Width(),
	    .height = file->Height(),
	};
}

std::vector<PictureTile> video::PictureTiles(uint32_t width, uint32_t height, ScreenRect rect)
{
	std::vector<PictureTile> tiles;
	if (width == 0 || height == 0)
	{
		return tiles;
	}
	constexpr uint32_t k_Tile = 0x100;
	const float sx = static_cast<float>(rect.width) / static_cast<float>(width);
	const float sy = static_cast<float>(rect.height) / static_cast<float>(height);
	const uint32_t tilesX = (width + k_Tile - 1) / k_Tile;
	const uint32_t tilesY = (height + k_Tile - 1) / k_Tile;
	tiles.reserve(static_cast<size_t>(tilesX) * tilesY);
	for (uint32_t ty = 0; ty < tilesY; ++ty)
	{
		// The last row has height & 0xFF texels (256 if that is 0), the last column likewise
		const uint32_t rows = (ty == tilesY - 1 && (height & 0xFFu) != 0) ? (height & 0xFFu) : k_Tile;
		for (uint32_t tx = 0; tx < tilesX; ++tx)
		{
			const uint32_t columns = (tx == tilesX - 1 && (width & 0xFFu) != 0) ? (width & 0xFFu) : k_Tile;
			const float x0 = static_cast<float>(rect.x) + static_cast<float>(tx * k_Tile) * sx;
			const float y0 = static_cast<float>(rect.y) + static_cast<float>(ty * k_Tile) * sy;
			tiles.push_back({
			    .min = {x0, y0},
			    .max = {x0 + static_cast<float>(columns) * sx, y0 + static_cast<float>(rows) * sy},
			    .uvMin = {(static_cast<float>(tx * k_Tile) + 0.5f) / static_cast<float>(width),
			              (static_cast<float>(ty * k_Tile) + 0.5f) / static_cast<float>(height)},
			    .uvMax = {(static_cast<float>(tx * k_Tile + columns) - 0.5f) / static_cast<float>(width),
			              (static_cast<float>(ty * k_Tile + rows) - 0.5f) / static_cast<float>(height)},
			});
		}
	}
	return tiles;
}
