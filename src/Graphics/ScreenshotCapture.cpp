/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenshotCapture.h"

#include <cstddef>
#include <cstring>

#include <algorithm>
#include <system_error>
#include <utility>

#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <spdlog/spdlog.h>

#include "GraphicsHandleBgfx.h"

namespace openblack::graphics
{

namespace
{
/// bgfx's note when it has no screen to read for a picture, before the picture's path and a full stop
constexpr std::string_view k_UncapturedNote = "Unable to capture screenshot ";
/// The first view after every pass: a copy asked for in it is made once the whole frame is drawn
constexpr auto k_CopyView = static_cast<bgfx::ViewId>(RenderPass::_count);
constexpr uint32_t k_BytesPerPixel = 4;
/// What the shown frames' names start with, after ScreenPictures::k_Prefix
constexpr std::string_view k_ShownFramePrefix = "shown/";
} // namespace

ScreenshotPlan PlanScreenshot(bool screenDroppedWhenMinimised, bool minimised, bool canReadBack, glm::u16vec2 size)
{
	if (!screenDroppedWhenMinimised || !minimised)
	{
		return {.route = ScreenshotRoute::Screen, .why = {}};
	}
	if (!canReadBack)
	{
		return {.route = ScreenshotRoute::Refused,
		        .why = "the window is minimised, which leaves this renderer nothing to read, and it can't read back a frame "
		               "drawn aside: restore the window"};
	}
	if (size.x == 0 || size.y == 0)
	{
		return {.route = ScreenshotRoute::Refused,
		        .why = "the window is minimised, which leaves this renderer nothing to read, and its size is unknown: "
		               "restore the window"};
	}
	return {.route = ScreenshotRoute::Offscreen, .why = {}};
}

std::optional<std::string_view> UncapturedScreenshotPath(std::string_view note)
{
	if (!note.starts_with(k_UncapturedNote))
	{
		return std::nullopt;
	}
	note.remove_prefix(k_UncapturedNote.size());
	if (note.ends_with('.'))
	{
		note.remove_suffix(1);
	}
	if (note.empty())
	{
		return std::nullopt;
	}
	return note;
}

void ScreenshotFailures::Record(std::string path, std::string why)
{
	const std::scoped_lock lock(_mutex);
	_failures.insert_or_assign(std::move(path), std::move(why));
}

std::optional<std::string> ScreenshotFailures::Take(const std::string& path)
{
	const std::scoped_lock lock(_mutex);
	const auto found = _failures.find(path);
	if (found == _failures.end())
	{
		return std::nullopt;
	}
	auto why = std::move(found->second);
	_failures.erase(found);
	return why;
}

void ScreenPictures::Ask(std::string name, ScreenPicturer picturer)
{
	const std::scoped_lock lock(_mutex);
	_made.erase(name);
	_asked.insert_or_assign(std::move(name), std::move(picturer));
}

bool ScreenPictures::Make(const std::string& name, const ScreenView& screen)
{
	ScreenPicturer picturer;
	{
		const std::scoped_lock lock(_mutex);
		const auto found = _asked.find(name);
		if (found == _asked.end())
		{
			return false;
		}
		picturer = std::move(found->second);
		_asked.erase(found);
	}
	// Make the picture without the lock, so that the game thread does not wait for it
	auto picture = picturer ? picturer(screen) : std::vector<uint8_t> {};
	const std::scoped_lock lock(_mutex);
	_made.insert_or_assign(name, std::move(picture));
	return true;
}

std::optional<std::vector<uint8_t>> ScreenPictures::Take(const std::string& name)
{
	const std::scoped_lock lock(_mutex);
	const auto found = _made.find(name);
	if (found == _made.end())
	{
		return std::nullopt;
	}
	auto picture = std::move(found->second);
	_made.erase(found);
	return picture;
}

void ScreenPictures::Forget(const std::string& name)
{
	const std::scoped_lock lock(_mutex);
	_asked.erase(name);
	_made.erase(name);
}

std::string ShownFrames::Name(uint32_t frame)
{
	return std::string(ScreenPictures::k_Prefix) + std::string(k_ShownFramePrefix) + std::to_string(frame);
}

bool ShownFrames::IsName(std::string_view name)
{
	return ScreenPictures::InMemory(name) && name.substr(ScreenPictures::k_Prefix.size()).starts_with(k_ShownFramePrefix);
}

std::optional<uint32_t> ShownFrames::OnScreen(uint32_t framesDrawn)
{
	if (framesDrawn == 0)
	{
		return std::nullopt;
	}
	return framesDrawn - 1;
}

size_t ShownFrames::Drawn(uint32_t frame)
{
	const size_t target = _lastTarget.has_value() ? (*_lastTarget + 1) % k_Targets : 0;
	_held.at(target) = frame;
	_lastTarget = target;
	return target;
}

std::optional<size_t> ShownFrames::Holding(uint32_t frame) const
{
	for (size_t target = 0; target < k_Targets; ++target)
	{
		if (_held.at(target) == frame)
		{
			return target;
		}
	}
	return std::nullopt;
}

void ShownFrames::Forget()
{
	_held = {};
	_lastTarget.reset();
}

std::optional<ShownFrames::Asked> ShownFrames::Ask(uint32_t framesDrawn)
{
	const auto frame = OnScreen(framesDrawn);
	if (!frame.has_value())
	{
		return std::nullopt;
	}
	const auto target = Holding(*frame);
	if (!target.has_value())
	{
		return std::nullopt;
	}
	auto name = Name(*frame);
	const bool first = _asked != frame;
	if (first)
	{
		_asked = frame;
		_readBacks.push_back({.target = *target, .name = name});
	}
	return Asked {.name = std::move(name), .first = first};
}

std::vector<ShownFrames::ReadBack> ShownFrames::TakeReadBacks()
{
	return std::exchange(_readBacks, {});
}

std::optional<uint32_t> ShownFrames::Last() const
{
	if (!_lastTarget.has_value())
	{
		return std::nullopt;
	}
	return _held.at(*_lastTarget);
}

std::string WriteScreenshotPng(const std::filesystem::path& path, uint32_t width, uint32_t height, uint32_t pitch,
                               std::span<const uint8_t> bgra, bool yflip)
{
	if (static_cast<uint64_t>(pitch) * height > bgra.size() || pitch < width * k_BytesPerPixel)
	{
		return "the pixels don't fill a " + std::to_string(width) + "x" + std::to_string(height) + " picture";
	}
	auto partPath = path;
	partPath += ".part";
	bx::FileWriter writer;
	bx::Error err;
	if (!bx::open(&writer, partPath.string().c_str(), false, &err))
	{
		return std::string(err.getMessage().getCPtr(), err.getMessage().getLength());
	}
	// The picture is opaque whatever the frame left in its alpha
	std::vector<uint8_t> opaque(bgra.begin(), bgra.end());
	for (uint32_t y = 0; y < height; ++y)
	{
		for (uint32_t x = 0; x < width; ++x)
		{
			opaque[static_cast<size_t>(y) * pitch + x * k_BytesPerPixel + 3] = 0xFF;
		}
	}
	bimg::imageWritePng(&writer, width, height, pitch, opaque.data(), bimg::TextureFormat::BGRA8, yflip, &err);
	bx::close(&writer);
	if (!err.isOk())
	{
		return std::string(err.getMessage().getCPtr(), err.getMessage().getLength());
	}
	std::error_code renameError;
	std::filesystem::rename(partPath, path, renameError);
	return renameError ? renameError.message() : std::string {};
}

OffscreenScreenshots::~OffscreenScreenshots()
{
	std::ranges::for_each(_captures, &OffscreenScreenshots::Release);
}

void OffscreenScreenshots::Begin(const std::filesystem::path& path, glm::u16vec2 size)
{
	const std::array textures = {
	    bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT),
	    bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY),
	};
	const auto frameBuffer = bgfx::createFrameBuffer(static_cast<uint8_t>(textures.size()), textures.data(), true);
	const auto readBack = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8,
	                                            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
	bgfx::setName(frameBuffer, "Screenshot");
	bgfx::setName(readBack, "Screenshot Read Back");

	for (const auto pass : k_ScreenPasses)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), frameBuffer);
	}
	_redirected = true;
	bgfx::blit(k_CopyView, readBack, 0, 0, textures[0]);

	Capture capture {
	    .path = path,
	    .size = size,
	    .frameBuffer = fromBgfx(frameBuffer),
	    .readBack = fromBgfx(readBack),
	    .pixels = std::vector<uint8_t>(static_cast<size_t>(size.x) * size.y * k_BytesPerPixel),
	    .readyAt = 0,
	};
	// The vector's buffer stays where it is while the capture moves about, so bgfx may write into it later
	capture.readyAt = bgfx::readTexture(readBack, capture.pixels.data());
	_captures.push_back(std::move(capture));
}

void OffscreenScreenshots::FrameEnded(uint32_t frame, ScreenshotFailures& failures, ScreenPictures& pictures)
{
	if (_redirected)
	{
		for (const auto pass : k_ScreenPasses)
		{
			bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), BGFX_INVALID_HANDLE);
		}
		_redirected = false;
	}
	std::erase_if(_captures, [frame, &failures, &pictures](const Capture& capture) {
		if (frame < capture.readyAt)
		{
			return false;
		}
		const bool yflip = bgfx::getCaps()->originBottomLeft;
		if (ScreenPictures::InMemory(capture.path.string()))
		{
			pictures.Make(capture.path.string(), {.width = capture.size.x,
			                                      .height = capture.size.y,
			                                      .pitch = capture.size.x * k_BytesPerPixel,
			                                      .yflip = yflip,
			                                      .bgra = capture.pixels});
			Release(capture);
			return true;
		}
		if (auto why = WriteScreenshotPng(capture.path, capture.size.x, capture.size.y, capture.size.x * k_BytesPerPixel,
		                                  capture.pixels, yflip);
		    !why.empty())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save the screenshot drawn aside at {}: {}",
			                    capture.path.string(), why);
			failures.Record(capture.path.string(), "the picture drawn aside couldn't be written: " + why);
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot drawn aside ({}x{}) saved at {}", capture.size.x,
			                   capture.size.y, capture.path.string());
		}
		Release(capture);
		return true;
	});
}

void OffscreenScreenshots::Release(const Capture& capture)
{
	bgfx::destroy(toBgfx(capture.frameBuffer));
	bgfx::destroy(toBgfx(capture.readBack));
}

ShownFrameTargets::~ShownFrameTargets()
{
	Release();
	for (const auto& readBack : _readBacks)
	{
		bgfx::destroy(toBgfx(readBack.readBack));
	}
}

bool ShownFrameTargets::Supported()
{
	const auto* caps = bgfx::getCaps();
	constexpr uint64_t k_ReadBackCaps = BGFX_CAPS_TEXTURE_READ_BACK | BGFX_CAPS_TEXTURE_BLIT;
	constexpr uint16_t k_Target = BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER;
	return (caps->supported & k_ReadBackCaps) == k_ReadBackCaps &&
	       (caps->formats[bgfx::TextureFormat::BGRA8] & k_Target) != 0 &&
	       (caps->formats[bgfx::TextureFormat::D24S8] & k_Target) != 0;
}

bool ShownFrameTargets::Make(glm::u16vec2 size)
{
	if (_made && _size == size)
	{
		return false;
	}
	Release();
	// Sampled at each pixel's own texel by the copy to the window: no filtering
	constexpr uint64_t k_ColourFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
	const auto depth = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
	_depth = fromBgfx(depth);
	for (size_t target = 0; target < ShownFrames::k_Targets; ++target)
	{
		const auto colour = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8, k_ColourFlags);
		const std::array textures = {colour, depth};
		// The depth is shared, so the frame buffers don't own their textures
		const auto frameBuffer = bgfx::createFrameBuffer(static_cast<uint8_t>(textures.size()), textures.data(), false);
		bgfx::setName(colour, "Shown Frame");
		bgfx::setName(frameBuffer, "Shown Frame");
		_colours.at(target) = fromBgfx(colour);
		_frameBuffers.at(target) = fromBgfx(frameBuffer);
	}
	bgfx::setName(depth, "Shown Frame Depth");
	_size = size;
	_made = true;
	return true;
}

void ShownFrameTargets::Release()
{
	if (!_made)
	{
		return;
	}
	for (size_t target = 0; target < ShownFrames::k_Targets; ++target)
	{
		bgfx::destroy(toBgfx(_frameBuffers.at(target)));
		bgfx::destroy(toBgfx(_colours.at(target)));
	}
	bgfx::destroy(toBgfx(_depth));
	_made = false;
	_size = {0, 0};
}

void ShownFrameTargets::Redirect(size_t target)
{
	for (const auto pass : k_Passes)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), toBgfx(_frameBuffers.at(target)));
	}
	_redirected = true;
}

void ShownFrameTargets::ReadBack(size_t target, std::string name)
{
	const auto readBack = bgfx::createTexture2D(_size.x, _size.y, false, 1, bgfx::TextureFormat::BGRA8,
	                                            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
	bgfx::setName(readBack, "Shown Frame Read Back");
	// The target holds the frame on the screen, which this frame does not draw into
	bgfx::blit(k_CopyView, readBack, 0, 0, toBgfx(_colours.at(target)));
	Pending pending {
	    .name = std::move(name),
	    .size = _size,
	    .readBack = fromBgfx(readBack),
	    .pixels = std::vector<uint8_t>(static_cast<size_t>(_size.x) * _size.y * k_BytesPerPixel),
	    .readyAt = 0,
	};
	// The vector's buffer stays where it is while the read-back moves about, so bgfx may write into it later
	pending.readyAt = bgfx::readTexture(readBack, pending.pixels.data());
	_readBacks.push_back(std::move(pending));
}

void ShownFrameTargets::FrameEnded(uint32_t frame, ScreenPictures& pictures)
{
	if (_redirected)
	{
		for (const auto pass : k_Passes)
		{
			bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), BGFX_INVALID_HANDLE);
		}
		_redirected = false;
	}
	std::erase_if(_readBacks, [frame, &pictures](const Pending& pending) {
		if (frame < pending.readyAt)
		{
			return false;
		}
		pictures.Make(pending.name, {.width = pending.size.x,
		                             .height = pending.size.y,
		                             .pitch = pending.size.x * k_BytesPerPixel,
		                             .yflip = bgfx::getCaps()->originBottomLeft,
		                             .bgra = pending.pixels});
		bgfx::destroy(toBgfx(pending.readBack));
		return true;
	});
}

} // namespace openblack::graphics
