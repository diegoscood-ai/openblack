/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Video/VideoPlayer.h"

/// The films the start-up and loading screens show themselves, a frame at a time and outside the full-screen player:
/// the logos, the pre-intro and the tips. Each picture goes through the player's 16-bit framebuffer, as the game
/// copies its videos, and is sampled as the full-screen player's is. Wiki:
/// docs/wiki/video/playback.md#startup-and-loading-screen.
namespace openblack::video
{

/// A film opened with openblack's own decoder, decoded a frame at a time from 0
class VideoFile
{
public:
	/// None when the file isn't there, can't be read or decoded, or the films are off (or built without the decoder)
	[[nodiscard]] static std::optional<VideoFile> Open(const std::filesystem::path& path);

	VideoFile(VideoFile&&) noexcept;
	VideoFile& operator=(VideoFile&&) noexcept;
	VideoFile(const VideoFile&) = delete;
	VideoFile& operator=(const VideoFile&) = delete;
	~VideoFile();

	/// Decodes a frame, from 0, and copies it through the 16-bit framebuffer. False when it is damaged or past the
	/// end: the picture stays the last good one
	bool Decode(uint32_t frame);
	/// The picture decoded last as the renderer samples it, width * height * 4 bytes of RGBA8
	[[nodiscard]] std::span<const uint8_t> GetRgba() const;
	/// The 16-bit framebuffer itself, width * height texels of k_DefaultFormat
	[[nodiscard]] std::span<const uint16_t> GetRgb16() const;
	/// Goes up with each picture decoded
	[[nodiscard]] uint32_t GetSerial() const;

	[[nodiscard]] uint32_t Width() const;
	[[nodiscard]] uint32_t Height() const;
	[[nodiscard]] uint32_t FrameCount() const;
	[[nodiscard]] uint32_t FpsNumerator() const;
	[[nodiscard]] uint32_t FpsDenominator() const;

private:
	struct State;
	explicit VideoFile(std::unique_ptr<State> state);

	std::unique_ptr<State> _state;
};

/// One frame of a film kept on its own, after the film is closed
struct StillPicture
{
	/// As VideoFile::GetRgba
	std::vector<uint8_t> rgba;
	/// As VideoFile::GetRgb16
	std::vector<uint16_t> rgb16;
	uint32_t width {0};
	uint32_t height {0};
};

/// A frame of a film, none when it can't be had
[[nodiscard]] std::optional<StillPicture> ReadStill(const std::filesystem::path& path, uint32_t frame);

/// One textured rectangle of a picture drawn over the screen, in pixels, and its part of the picture from 0 to 1
struct PictureTile
{
	glm::vec2 min;
	glm::vec2 max;
	glm::vec2 uvMin;
	glm::vec2 uvMax;
};

/// How a picture of a size is drawn over a rectangle of the screen, as the full-screen player draws its textures: in
/// tiles of 256 by 256 texels, those of the last row and column cut to what is left, each texel `rect.width / width` by
/// `rect.height / height` pixels, and each tile sampled from half a texel inside its edges
[[nodiscard]] std::vector<PictureTile> PictureTiles(uint32_t width, uint32_t height, ScreenRect rect);

} // namespace openblack::video
