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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <BinkFile.h>

#include "Graphics/Rgb16.h"
#include "VideoDecoder.h"

/// The full screen film: the game's video state and its player. Wiki: docs/wiki/video/playback.md#the-full-screen-video.
///
/// - Play keeps the previous pause and pauses the game, keeps the wide screen and turns it on, and opens the film: it
///   plays to its last frame with a 5 s fade (the intro is cut to 60 s with the fade from 58 s).
/// - Process, once a frame: the end (alpha 0, Stop), the fade zone (the pause given back,
///   alpha = 1 - (f - start) / (|end - start| + 1)), and the picture drawn with the vertex colour 0x00FFFFFF | alpha << 24
///   over the letterboxed screen. While alpha is 1 and it is not the falling spell's film, the 3D world is not drawn,
///   and a frame with no new picture waits for the next one, 0.5 ms at a time, at most 1000 times (Hooks::holdFrame).
/// - FinishedVideo, the frame after Stop: the pause and the wide screen as they were.
/// - ESC skips: a 48-frame fade from the current frame, unless skipping is disabled.
/// - The original decodes on a 16 ms multimedia timer paced by the Bink clock.
///   (approximate) here the frames are decoded in Process from the wall clock ms and the film's frame rate: the same
///   frames at the same times, without the timer thread.
/// - Each picture goes through the 16-bit framebuffer (graphics::rgb16) and back to RGBA8 as the original's renderer
///   samples it: GetFrame() is what the renderer uploads.
///
/// Not ported: the sound bank argument (both callers pass none: no bank registered, no music started at frame 3), the
/// clearing of the loading screen's tip picture (it has always gone before a film can start: the land's start clears
/// it), the hiding of the HUD (inferred), the CD path fallback (the caller passes the real path) and the statistics
/// string.
namespace openblack::video
{

/// The fade starts 5 s of frames before the end
inline constexpr int32_t k_FadeSeconds = 5;
/// A skip fades out over 48 frames from the current one (2 s at 24 fps)
inline constexpr int32_t k_SkipFadeFrames = 0x30;
/// The intro's fade starts at 58 s
inline constexpr int32_t k_IntroFadeStartSeconds = 58;
/// The intro ends at 60 s (INTRO.bik lasts 66.7 s)
inline constexpr int32_t k_IntroEndSeconds = 60;
/// The vertex alpha of the falling spell's film is 0x50, any other 0xFF
inline constexpr uint32_t k_OpaqueAlpha = 0xFF;
inline constexpr uint32_t k_FallingSpellAlpha = 0x50;
/// The vertex colour's R, G, B bytes are 0xFF
inline constexpr uint32_t k_VertexRgb = 0x00FFFFFF;
/// 0.5625 (9 / 16), the height of a 16:9 picture over the screen width
inline constexpr float k_LetterboxAspect = 0.5625f;
/// The letterbox scale, always 1.0f in the original
inline constexpr float k_LetterboxScale = 1.0f;
/// While the opaque film has no new frame the draw waits up to 1000 polls of 0.5 ms, each poll followed by its 0.5 ms
inline constexpr int32_t k_WaitPolls = 1000;
inline constexpr int32_t k_WaitPollMicroseconds = 500;
/// The original's periodic 16 ms poll timer (approximate: not used, see Process)
inline constexpr uint32_t k_PollTimerMs = 16;
/// The Bink open flags: BINKNOTHREADEDIO (inferred) | BINKNOSKIP (no frame is ever skipped, so every frame is decoded
/// even when late)
inline constexpr uint32_t k_BinkOpenFlags = 0x08080000;
/// The 16-bit format of the framebuffer on the target hardware (Graphics/Rgb16.h)
inline constexpr graphics::rgb16::Format k_DefaultFormat = graphics::rgb16::Format::Rgb555;

/// The alpha in the fade zone: 1 - (f - start) / (|end - start| + 1), the int difference divided by the int at
/// 24-bit precision: float arithmetic
[[nodiscard]] float FadeAlpha(int32_t frame, int32_t fadeStart, int32_t end) noexcept;
/// 0x00FFFFFF | trunc(base * alpha) << 24, base 0x50 for the falling spell's film else 0xFF (truncated; only the low
/// byte is kept)
[[nodiscard]] uint32_t VertexColour(float alpha, bool fallingSpell) noexcept;

/// The rectangle the film is drawn to
struct ScreenRect
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};
/// bars = trunc((H - W * 0.5625) * letterboxScale) / 2 (signed division), the rectangle (0, bars, W + 1,
/// H - 2 bars + 1). Unlike CinemaBars::BarHeight the bars are
/// not clamped: on a screen wider than 16:9 they are negative and the picture overflows the screen at the top and
/// bottom
[[nodiscard]] ScreenRect FullScreenRect(int32_t screenWidth, int32_t screenHeight,
                                        float letterboxScale = k_LetterboxScale) noexcept;
/// (approximate) The Bink clock: how many frames are due `elapsedMs` after the film started, frame i at
/// i * den * 1000 / num ms (frame 0 at once)
[[nodiscard]] uint32_t FramesDue(uint64_t elapsedMs, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept;
/// The same in microseconds: frame i at i * den * 1000000 / num us (FramesDue of whole milliseconds gives the same)
[[nodiscard]] uint32_t FramesDueAt(std::chrono::microseconds elapsed, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept;
/// How long an opaque film's frame with no new picture waits for one, polling the film's clock 0.5 ms at a time from
/// `elapsed`: each poll is followed by its 0.5 ms, and the wait ends after the poll that finds a frame due beyond the
/// `decoded` ones, or after 1000 polls (0.5 s)
[[nodiscard]] std::chrono::microseconds WaitForNextFrame(std::chrono::microseconds elapsed, uint32_t decoded,
                                                         uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept;

/// The land number of the playground
inline constexpr int32_t k_PlaygroundLand = 6;
/// The land's state that the Escape key meets on its way to a film's skip
struct EscapeWay
{
	/// The playground land, the sixth, in a game of one player
	bool playground {false};
	/// The local player's game is over (its temple has fallen)
	bool gameOver {false};
	/// A script holds the wide screen
	bool scriptWideScreen {false};
};
/// Whether Escape gets as far as the film's skip. Once the game is over it is taken by the game's own question first,
/// unless a script holds the wide screen; otherwise, in the playground, the land's own Escape takes it. (Inside the
/// temple and with the menu open, the temple and the menu have taken it before)
[[nodiscard]] constexpr bool EscapeReachesTheFilm(const EscapeWay& way) noexcept
{
	if (way.gameOver)
	{
		return way.scriptWideScreen;
	}
	return !way.playground;
}

class VideoPlayer
{
public:
	/// What the player asks of the game; Hooks() are openblack's (game_clock, HelpSystem, GameMusic), the tests give
	/// their own
	struct Hooks
	{
		/// Whether the game is paused: game_clock::IsPaused
		std::function<bool()> isPaused;
		/// game_clock::Pause
		std::function<void(bool paused)> pauseGame;
		/// help::HelpSystem::GetWideScreen
		std::function<int32_t()> wideScreen;
		/// HelpSystem::SetWideScreen(on, 0)
		std::function<void(int32_t on)> setWideScreen;
		/// The wide screen bars at 100 % at once
		/// (CinemaBars::Snap); Play calls it every time, also when the script had the bars on already
		std::function<void()> snapWideScreen;
		/// Stops the script music after the ESC skip
		std::function<void()> stopScriptMusic;
		/// The music forgets the alignment music it plays when a film starts (GameMusic::ForgetAlignmentMusic)
		std::function<void()> forgetAlignmentMusic;
		/// Ends the falling spell's film when it is skipped. Unset:
		/// nothing (FallingSpellVideo.h: GameHooks() connects video::GetFallingSpell().End())
		std::function<void()> endFallingSpellVideo;
		/// The picture decoder (VideoDecoder.h). Unset: NullVideoDecoder
		std::function<std::unique_ptr<IVideoDecoder>()> makeDecoder;
		/// Holds the game's frame up while the opaque film waits for its next picture (game_clock::HoldFrame); the
		/// next frame's real ms then take the wait in. Unset: the frame does not wait
		std::function<void(std::chrono::microseconds duration)> holdFrame;
	};
	/// openblack's hooks: game_clock::Pause / IsPaused / HoldFrame, help::Get()'s wide screen (nothing without a
	/// HelpSystem), audio::game_music's ScriptStopMusic and ForgetAlignmentMusic, and BinkDecoder (none, so the film is
	/// skipped, without OPENBLACK_BINK_DECODER or with the films turned off:
	/// ecs::systems::VideoSystemInterface::FilmsEnabled)
	[[nodiscard]] static Hooks GameHooks();

	/// What the renderer draws: the vertex colour and the textures of the mosaic
	struct Frame
	{
		/// The picture as the original's renderer samples it: the 16-bit framebuffer expanded (graphics::rgb16::Expand),
		/// width * height * 4
		std::span<const uint8_t> rgba;
		/// The 16-bit framebuffer itself, width * height texels of `format` (pitch width * 2)
		std::span<const uint16_t> rgb16;
		graphics::rgb16::Format format;
		uint32_t width;
		uint32_t height;
		/// The film's current alpha
		float alpha;
		/// The vertex colour 0x00FFFFFF | alpha << 24 (VertexColour)
		uint32_t colour;
		/// The falling spell's film: drawn over the 3D world with alpha 0x50
		bool fallingSpell;
		/// The frames decoded so far
		int32_t frame;
		/// Goes up with each picture decoded: upload when it changes
		uint32_t serial;
	};

	explicit VideoPlayer(Hooks hooks);
	~VideoPlayer();
	VideoPlayer(const VideoPlayer&) = delete;
	VideoPlayer& operator=(const VideoPlayer&) = delete;

	/// Plays a film with a 5 s fade: the film before deleted, alpha 1, the pause kept and the
	/// game paused, the film opened, the wide screen kept and turned on. As in the original the player exists even
	/// when the file does not open: it ends on the next Process. True if the file opened
	bool Play(const std::filesystem::path& path);
	/// The fade start and the end frames, as the intro sequence writes them after Play
	/// (nothing without a film)
	void SetSchedule(int32_t fadeStartFrame, int32_t endFrame);
	/// The intro: SetSchedule(fps * 58, fps * 60) and the intro flag set (only with a film)
	void ScheduleIntro();
	/// The per frame service, `realMs` the wall clock ms of the frame
	/// (game_clock::FrameRealMs: the game is paused)
	void Process(uint32_t realMs);
	/// The skip. The falling spell's film ends (endFallingSpellVideo); in the fade zone the film ends at
	/// once; else the fade starts now and lasts 48 frames (never past the last frame), and the pause is given back
	void Skip();
	/// The ESC key: with Shift
	/// or Ctrl held (inferred) nothing; with a film and skipping allowed, Skip() and
	/// the script music stopped. True when a film holds the key; false without a film (openblack's own ESC goes on).
	/// Alt is not checked. The key's earlier ways (the game over's question, the playground's own Escape, an open box,
	/// the temple) are the caller's: EscapeReachesTheFilm
	bool EscapeKey(bool shift, bool ctrl);
	/// The film gone, the finished count + 1 (FinishedVideo on the next Process), skipping allowed
	void Stop();

	/// A film is open (also the audio's GameQueries::videoPlaying). Safe from any thread
	[[nodiscard]] bool IsPlaying() const { return _playing.load(std::memory_order_acquire); }
	/// A film, alpha == 1.0f and not the falling spell's: the 3D world is not drawn
	[[nodiscard]] bool CoversScreen() const;
	/// The picture of the film for the renderer; nullopt without a film
	[[nodiscard]] std::optional<Frame> GetFrame() const;

	/// Set while the new profile box is up, ESC does not skip; Stop / FinishedVideo
	/// clear it
	void SetNoSkip(bool noSkip) { _noSkip = noSkip; }
	/// Whether the falling spell's film is playing (FallingSpellVideo.h)
	void SetFallingSpellVideo(bool on) { _fallingSpell = on; }
	/// The 16-bit format of the framebuffer; k_DefaultFormat
	void SetFormat(graphics::rgb16::Format format) { _format = format; }

	/// The film's fps, frame count and current frame (0 without a film)
	[[nodiscard]] int32_t Fps() const;
	[[nodiscard]] int32_t TotalFrames() const;
	[[nodiscard]] int32_t CurrentFrame() const;
	[[nodiscard]] int32_t FadeStartFrame() const { return _fadeStartFrame; }
	[[nodiscard]] int32_t EndFrame() const { return _endFrame; }
	[[nodiscard]] float Alpha() const { return _alpha; }
	[[nodiscard]] bool IsIntro() const { return _isIntro; }
	[[nodiscard]] bool PreviousPause() const { return _previousPause; }
	[[nodiscard]] int32_t PreviousWideScreen() const { return _previousWideScreen; }
	[[nodiscard]] int32_t FinishedCount() const { return _finished; }
	[[nodiscard]] bool NoSkip() const { return _noSkip; }
	/// How long the last Process held its frame up waiting for the film's next picture (0 when it did not wait)
	[[nodiscard]] std::chrono::microseconds LastWait() const { return _lastWait; }

private:
	/// An open film and its decoding state
	struct Movie
	{
		BikFile file;
		std::unique_ptr<IVideoDecoder> decoder;
		bool open {false}; ///< The Bink handle is open
		uint32_t width {0};
		uint32_t height {0};
		int32_t fps {1}; ///< 1 before the summary is read
		int32_t totalFrames {0};
		int32_t frame {0};
		std::vector<uint16_t> framebuffer; ///< width * height * 2 bytes, zeroed
		std::vector<uint8_t> texture;      ///< the textures' picture, as sampled
		uint32_t serial {0};
		bool pictureToCopy {false}; ///< a picture was decoded this Poll: its colours are made at the end of it
		/// (approximate) Bink's clock since the first Process
		std::chrono::microseconds elapsed {0};
		/// The waits already on the film's clock that the next frames' real ms still hold
		std::chrono::microseconds waitedAhead {0};
		bool clockStarted {false};
	};

	/// The counters cleared, the player made and opened, end = total, fade = total - fps * fadeSeconds
	bool Open(const std::filesystem::path& path, int32_t fadeSeconds);
	/// For each frame due: past the end Stop, else DecodeNextFrame and the frames ready + 1
	void Poll();
	/// Decodes the next picture (its colours are made by Poll, for the last one decoded)
	void DecodeNextFrame();
	/// The frame after Stop: the pause and the wide screen as they were
	void FinishedVideo();

	Hooks _hooks;
	std::unique_ptr<Movie> _movie;      ///< The film playing
	std::atomic<bool> _playing {false}; ///< _movie != nullptr, for other threads
	int32_t _fadeStartFrame {0};
	int32_t _endFrame {0};
	float _alpha {0.0f};
	bool _isIntro {false};
	int32_t _framesReady {0};
	int32_t _finished {0};
	bool _previousPause {false};
	int32_t _previousWideScreen {0};
	bool _noSkip {false};
	bool _fallingSpell {false};                        ///< The falling spell's film is playing
	graphics::rgb16::Format _format {k_DefaultFormat}; ///< The framebuffer's 16-bit format
	std::chrono::microseconds _lastWait {0};           ///< The last Process's wait for a picture
};

/// The game's one, with GameHooks()
[[nodiscard]] VideoPlayer& Get();
/// Get().IsPlaying(): what audio connects GameQueries::videoPlaying to
[[nodiscard]] bool IsPlaying();

} // namespace openblack::video
