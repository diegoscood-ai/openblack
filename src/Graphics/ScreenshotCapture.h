/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "GraphicsHandle.h"
#include "RenderPass.h"

namespace openblack::graphics
{

/// How a picture of the screen is read
enum class ScreenshotRoute : uint8_t
{
	/// From the window's own buffers, as it is presented
	Screen,
	/// The frame's screen passes drawn into a target of the window's size and read back from it, for a window with no
	/// buffers to read (Vulkan drops them while the window is minimised)
	Offscreen,
	/// Not at all, for the reason given
	Refused,
};

struct ScreenshotPlan
{
	ScreenshotRoute route;
	/// Why a refused picture can't be taken
	std::string why;
};

/// How to read a picture of the frame being made: from the screen, unless the renderer keeps no screen while the window
/// is minimised and it is, when the frame is drawn aside and read back if the renderer can do that at that size
[[nodiscard]] ScreenshotPlan PlanScreenshot(bool screenDroppedWhenMinimised, bool minimised, bool canReadBack,
                                            glm::u16vec2 size);

/// The picture's path in bgfx's note that it couldn't read the screen for it; none for any other note
[[nodiscard]] std::optional<std::string_view> UncapturedScreenshotPath(std::string_view note);

/// The pictures the renderer gave up, by path, with why. Recorded from the render thread, taken from the game's.
class ScreenshotFailures
{
public:
	void Record(std::string path, std::string why);
	/// Why the picture at a path was given up, once; none while it may still come
	[[nodiscard]] std::optional<std::string> Take(const std::string& path);

private:
	std::mutex _mutex;
	std::unordered_map<std::string, std::string> _failures;
};

/// A frame as the renderer reads it back: BGRA8 pixels, rows pitch bytes apart, the bottom row first when yflip. The
/// pixels stay valid only while the function that gets the view runs.
struct ScreenView
{
	uint32_t width {0};
	uint32_t height {0};
	uint32_t pitch {0};
	bool yflip {false};
	std::span<const uint8_t> bgra;
};

/// Makes a picture from a frame. The renderer calls it on its own thread, so it must not use the game's data.
using ScreenPicturer = std::function<std::vector<uint8_t>(const ScreenView& screen)>;

/// The pictures that the game asks for in memory, by the name it asks under (a name that starts with k_Prefix). The
/// renderer makes each picture from the frame when it reads the frame. Thus it does not copy the full frame, and the
/// game thread does not read it.
class ScreenPictures
{
public:
	/// The renderer keeps a picture in memory when it reads the frame for a path that starts with this
	static constexpr std::string_view k_Prefix = "memory:";
	[[nodiscard]] static bool InMemory(std::string_view path) { return path.starts_with(k_Prefix); }

	/// Keeps the function that makes the picture for a name, until the frame for it is read
	void Ask(std::string name, ScreenPicturer picturer);
	/// Makes the picture for a name from the frame. Returns false if nobody asked for a picture under the name.
	bool Make(const std::string& name, const ScreenView& screen);
	/// The picture made for a name, one time; none while it is not made
	[[nodiscard]] std::optional<std::vector<uint8_t>> Take(const std::string& name);
	/// Removes a picture that the renderer gave up
	void Forget(const std::string& name);

private:
	std::mutex _mutex;
	std::unordered_map<std::string, ScreenPicturer> _asked;
	std::unordered_map<std::string, std::vector<uint8_t>> _made;
};

/// Which frames the targets aside hold while the frames are kept (ShownFrameTargets), and the pictures asked of them:
/// each frame kept is drawn into the target the frame kept before it was not, so the frame on the screen stays whole in
/// its target while the next one is drawn. Used from the game's thread.
class ShownFrames
{
public:
	/// The targets drawn into in turn
	static constexpr size_t k_Targets = 2;
	/// The name a frame's picture is asked under
	[[nodiscard]] static std::string Name(uint32_t frame);
	[[nodiscard]] static bool IsName(std::string_view name);
	/// The frame on the screen once framesDrawn frames have been handed over: the last of them; none before the first
	[[nodiscard]] static std::optional<uint32_t> OnScreen(uint32_t framesDrawn);

	/// The frame numbered frame is drawn into a target: the one the frame kept before it is not in
	[[nodiscard]] size_t Drawn(uint32_t frame);
	/// The target that holds a frame, if one does
	[[nodiscard]] std::optional<size_t> Holding(uint32_t frame) const;
	/// No target holds a frame any more (they were let go, or made again)
	void Forget();

	struct Asked
	{
		std::string name;
		/// Whether this is the first picture asked of the frame, which reads it back; a later one shares that one
		bool first {false};
	};
	/// A picture asked of the frame on the screen once framesDrawn frames have been handed over: its name, when a
	/// target holds that frame
	[[nodiscard]] std::optional<Asked> Ask(uint32_t framesDrawn);

	struct ReadBack
	{
		size_t target {0};
		std::string name;
	};
	/// The read-backs asked for since the last time, once each
	[[nodiscard]] std::vector<ReadBack> TakeReadBacks();
	/// The last frame drawn into a target
	[[nodiscard]] std::optional<uint32_t> Last() const;

private:
	std::array<std::optional<uint32_t>, k_Targets> _held;
	std::optional<size_t> _lastTarget;
	/// The last frame a picture was asked of
	std::optional<uint32_t> _asked;
	std::vector<ReadBack> _readBacks;
};

/// Writes BGRA8 pixels, rows pitch bytes apart, to a PNG without their alpha. The file is written aside and renamed once
/// whole, so that whoever waits for it never reads half of it. Why not, if it couldn't.
[[nodiscard]] std::string WriteScreenshotPng(const std::filesystem::path& path, uint32_t width, uint32_t height, uint32_t pitch,
                                             std::span<const uint8_t> bgra, bool yflip);

/// Pictures of frames drawn aside: the frame's screen passes are drawn into a target of the window's size instead of
/// the window, copied into a texture the processor can read and written out a couple of frames later, when the pixels
/// are back. The window shows nothing new for that frame, which is why it is only for a window nobody sees.
class OffscreenScreenshots
{
public:
	/// The passes that draw to the window
	static constexpr std::array k_ScreenPasses = {
	    RenderPass::Main,          RenderPass::MainBlended,   RenderPass::FinishFrameIcons, RenderPass::FinishFrameQueue,
	    RenderPass::FinishFrame3D, RenderPass::ScreenOverlay, RenderPass::Interface,        RenderPass::ImGui,
	    RenderPass::Cursor,        RenderPass::OpaqueAlpha};

	OffscreenScreenshots() = default;
	OffscreenScreenshots(const OffscreenScreenshots&) = delete;
	OffscreenScreenshots& operator=(const OffscreenScreenshots&) = delete;
	~OffscreenScreenshots();

	/// Draws the frame being made aside, at the window's size, for a picture written to path once read back
	void Begin(const std::filesystem::path& path, glm::u16vec2 size);
	/// Call this when bgfx has the frame with the number frame. It gives the passes back to the window. It writes each
	/// picture whose pixels are back, or makes it when the game asked for it in memory. It records each picture that
	/// it cannot write.
	void FrameEnded(uint32_t frame, ScreenshotFailures& failures, ScreenPictures& pictures);
	/// Whether the window's passes draw aside this frame
	[[nodiscard]] bool Redirected() const { return _redirected; }

private:
	struct Capture
	{
		std::filesystem::path path;
		glm::u16vec2 size;
		FrameBufferHandle frameBuffer;
		TextureHandle readBack;
		std::vector<uint8_t> pixels;
		/// The frame bgfx has the pixels back by
		uint32_t readyAt;
	};
	static void Release(const Capture& capture);

	std::vector<Capture> _captures;
	/// Whether the window's passes draw aside this frame
	bool _redirected {false};
};

/// The two targets of the window's size that the frame's screen passes draw into while the frames are kept, each frame
/// into the one ShownFrames gives, then copied to the window (RenderPass::ShownFrameCopy) before the debug windows, so the
/// frame on the screen can be read back on demand afterwards. They are of the window's formats: 8-bit colour and a
/// 24-bit depth with stencil.
class ShownFrameTargets
{
public:
	/// The passes that draw the game's frame to the window: every screen pass but the debug windows and the pointer
	static constexpr std::array k_Passes = {
	    RenderPass::Main,          RenderPass::MainBlended,   RenderPass::FinishFrameIcons, RenderPass::FinishFrameQueue,
	    RenderPass::FinishFrame3D, RenderPass::ScreenOverlay, RenderPass::Interface};

	ShownFrameTargets() = default;
	ShownFrameTargets(const ShownFrameTargets&) = delete;
	ShownFrameTargets& operator=(const ShownFrameTargets&) = delete;
	~ShownFrameTargets();

	/// Whether this renderer can keep the frames: it copies and reads back textures and draws into the targets' formats
	[[nodiscard]] static bool Supported();
	/// Makes the targets at a size unless they are of it. Whether they were made (what they held is gone)
	bool Make(glm::u16vec2 size);
	/// Lets the targets go
	void Release();
	[[nodiscard]] bool Made() const { return _made; }
	[[nodiscard]] glm::u16vec2 Size() const { return _size; }
	/// The colour of a target, which the copy to the window samples
	[[nodiscard]] TextureHandle Colour(size_t target) const { return _colours.at(target); }
	/// Draws the frame being made's screen passes into a target
	void Redirect(size_t target);
	/// Copies a target, as the frame being made begins, into a texture read back, for the picture asked under name
	void ReadBack(size_t target, std::string name);
	/// Whether a read-back is on its way
	[[nodiscard]] bool Reading() const { return !_readBacks.empty(); }
	/// Call when bgfx has the frame with the number frame. It gives the passes back to the window, and makes each picture
	/// whose pixels are back.
	void FrameEnded(uint32_t frame, ScreenPictures& pictures);

private:
	struct Pending
	{
		std::string name;
		glm::u16vec2 size {0, 0};
		TextureHandle readBack;
		std::vector<uint8_t> pixels;
		/// The frame bgfx has the pixels back by
		uint32_t readyAt {0};
	};

	glm::u16vec2 _size {0, 0};
	std::array<TextureHandle, ShownFrames::k_Targets> _colours {};
	TextureHandle _depth {};
	std::array<FrameBufferHandle, ShownFrames::k_Targets> _frameBuffers {};
	std::vector<Pending> _readBacks;
	bool _made {false};
	bool _redirected {false};
};

} // namespace openblack::graphics
