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
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "BikFile.h"
#include "Graphics/Rgb16.h"
#include "VideoDecoder.h"

/// The full screen film of runblack.exe W120: GGame's video state and its LHVideoPlayer (0x68 bytes, 0x844C60..
/// 0x845B30). Wiki: video.md; reading of the original: dev\tmp_dis\video\original.md.
///
/// - PlayFullScreenMovie 0x54D920 keeps the pause in VideoPreviousPause and pauses the game, keeps the wide screen and
///   turns it on, and opens the film (fn_0054AB20): it plays to its last frame with a 5 s fade (StartAVISequence(1)
///   0x68F450 cuts INTRO.bik to 60 s with the fade from 58 s).
/// - Process3dEngine 0x54DA80, once a frame: the end (alpha 0, DeleteVideo), the fade zone (the pause given back,
///   alpha = 1 - (f - start) / (|end - start| + 1)), and the picture drawn with the vertex colour 0x00FFFFFF | alpha << 24
///   over the letterboxed screen. While alpha is 1 and it is not the falling spell's film, the 3D world is not drawn.
/// - FinishedVideo 0x54D8D0, the frame after DeleteVideo: the pause and the wide screen as they were.
/// - ESC (GGame::ProcessKey 0x63F3B9) skips: a 48-frame fade from the current frame, unless the no-skip byte 0xD01984.
/// - The original decodes on a 16 ms multimedia timer (fn_0054AB00 0x54AB00, VideoPoll 0x54AA40) paced by BinkWait.
///   (aproximado) here the frames are decoded in Process from the wall clock ms and the film's frame rate: the same
///   frames at the same times, without the timer thread.
/// - Each picture goes through the 16-bit framebuffer +0x48 (BinkCopyToBuffer, graphics::rgb16) and back to RGBA8 as
///   D3D samples it: GetFrame() is what the renderer uploads (milestone V3).
///
/// Not ported: the sound bank argument (both callers pass NULL, 0x68F482 / FallingSpell::Init 0x5261FC: no
/// LHBankRegister, no LHMusicPlay at frame 3, fn_0054A9B0), ClearTipVideo (no loading screen), GAudio +0x1C = -1
/// (0x54D963: [0xCD3B20] is the GAudio of LHBankRegister and StartScriptMusic, +0x1C audio's GameMusic::_alignmentType,
/// "the music playing" forgotten; pending: audio has no setter, and ProcessMusic 0x427EB3 does it again in the fade),
/// the hiding of the HUD fn_005C6C40 (0x54D9EF, inferido), the CD path fallback of fn_0054AB20 (only with
/// [0xD46AB8] != 0; the caller passes the real path) and the statistics string (BinkGetRealtime 0x54DC8D).
namespace openblack::video
{

/// fn_0054AB20(file, 5) from PlayFullScreenMovie (`push 5` 0x54D9A2): the fade starts 5 s of frames before the end
inline constexpr int32_t k_FadeSeconds = 5;
/// fn_0054DA00 0x54DA47 (`add ecx, 0x30`): a skip fades out over 48 frames from the current one (2 s at 24 fps)
inline constexpr int32_t k_SkipFadeFrames = 0x30;
/// StartAVISequence(1) 0x68F4A1..0x68F4AF (fps * 29 * 2): the intro's fade starts at 58 s
inline constexpr int32_t k_IntroFadeStartSeconds = 58;
/// StartAVISequence(1) 0x68F4C3 (`imul edx, edx, 0x3C`): the intro ends at 60 s (INTRO.bik lasts 66.7 s)
inline constexpr int32_t k_IntroEndSeconds = 60;
/// Process3dEngine 0x54DC11..0x54DC1B (`neg; sbb; and 0xFFFFFF51; add 0xFF`): the vertex alpha of the falling
/// spell's film is 0x50, any other 0xFF
inline constexpr uint32_t k_OpaqueAlpha = 0xFF;
inline constexpr uint32_t k_FallingSpellAlpha = 0x50;
/// Process3dEngine 0x54DC2D..0x54DC37: the vertex colour's R, G, B bytes are 0xFF
inline constexpr uint32_t k_VertexRgb = 0x00FFFFFF;
/// Process3dEngine 0x54DBF7: [0x8C7A50] = 0.5625 (9 / 16), the height of a 16:9 picture over the screen width
inline constexpr float k_LetterboxAspect = 0.5625f;
/// GGame::VideoLetterboxScale [0xBEC16C] = 1.0f, read only at 0x54DBFF
inline constexpr float k_LetterboxScale = 1.0f;
/// Process3dEngine 0x54DB9E / 0x54DBBD: while the opaque film has no new frame the draw waits up to 1000 polls of
/// Good_sleep_us(0x1F4) = 0.5 s (aproximado: openblack does not block, the previous picture stays)
inline constexpr int32_t k_WaitPolls = 1000;
inline constexpr int32_t k_WaitPollMicroseconds = 500;
/// fn_0054AB00: timeSetEvent(0x10 ms, resolution 5, VideoPoll, periodic) (aproximado: not used, see Process)
inline constexpr uint32_t k_PollTimerMs = 16;
/// BinkOpen's flags at 0x844E86: 0x08000000 (BINKNOTHREADEDIO, inferido) | 0x00080000 (BINKNOSKIP: no frame is ever
/// skipped, so every frame is decoded even when late)
inline constexpr uint32_t k_BinkOpenFlags = 0x08080000;
/// The 16-bit format of the framebuffer: [0xEDD46C] == 0 on the target hardware (Graphics/Rgb16.h)
inline constexpr graphics::rgb16::Format k_DefaultFormat = graphics::rgb16::Format::Rgb555;

/// Process3dEngine 0x54DB4A..0x54DB7F in the fade zone: 1 - (f - start) / (|end - start| + 1), the int difference
/// `fild`-ed and `fidiv`-ed by the int, at 24-bit precision (fn_007DEE00): float arithmetic
[[nodiscard]] float FadeAlpha(int32_t frame, int32_t fadeStart, int32_t end) noexcept;
/// Process3dEngine 0x54DC11..0x54DC4D: 0x00FFFFFF | ftol(base * alpha) << 24, base 0x50 for the falling spell's film
/// else 0xFF (__ftol truncates; the low byte is stored, 0x54DC4D)
[[nodiscard]] uint32_t VertexColour(float alpha, bool fallingSpell) noexcept;

/// The rectangle LHVideoPlayer::DrawToScreen gets (0x54DBEB..0x54DC6D)
struct ScreenRect
{
	int32_t x;
	int32_t y;
	int32_t width;
	int32_t height;
};
/// Process3dEngine 0x54DBEB..0x54DC66: bars = ftol((H - W * 0.5625) * VideoLetterboxScale) / 2 (signed `cdq; sub;
/// sar`), the rectangle (0, bars, W + 1, H - 2 bars + 1). Unlike ScreenFade::LetterboxHeight (fn_0081E8B0) the bars are
/// not clamped: on a screen wider than 16:9 they are negative and the picture overflows the screen at the top and
/// bottom
[[nodiscard]] ScreenRect FullScreenRect(int32_t screenWidth, int32_t screenHeight,
                                        float letterboxScale = k_LetterboxScale) noexcept;
/// (aproximado) BinkWait: how many frames are due `elapsedMs` after the film started, frame i at i * den * 1000 / num
/// ms (frame 0 at once)
[[nodiscard]] uint32_t FramesDue(uint64_t elapsedMs, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept;

class VideoPlayer
{
public:
	/// What the player asks of the game; Hooks() are openblack's (game_clock, HelpSystem, GameMusic), the tests give
	/// their own
	struct Hooks
	{
		/// (g_game->flags >> 2) & 1 (0x54D943..0x54D959): game_clock::IsPaused
		std::function<bool()> isPaused;
		/// PauseGame 0x54AE20: game_clock::Pause
		std::function<void(bool paused)> pauseGame;
		/// HelpSystem +0x45E8 (0x54D9B8, FinishedVideo 0x54D8E4): help::HelpSystem::GetWideScreen
		std::function<int32_t()> wideScreen;
		/// HelpSystem::SetWideScreen(on, 0) 0x5C6AD0 (0x54D9E4, FinishedVideo 0x54D902)
		std::function<void(int32_t on)> setWideScreen;
		/// GAudio::StartScriptMusic(0) 0x428230 after the ESC skip (ProcessKey 0x63F3FA..0x63F402)
		std::function<void()> stopScriptMusic;
		/// GGame::EndFallingSpellVideo 0x553A10 (fn_0054DA00 0x54DA0C: the skip of the falling spell's film). Unset:
		/// nothing (milestone V6, the FallingSpell object is not ported)
		std::function<void()> endFallingSpellVideo;
		/// The picture decoder (VideoDecoder.h). Unset: NullVideoDecoder
		std::function<std::unique_ptr<IVideoDecoder>()> makeDecoder;
	};
	/// openblack's hooks: game_clock::Pause / IsPaused, help::Get()'s wide screen (nothing without a HelpSystem),
	/// audio::game_music's ScriptStopMusic, NullVideoDecoder
	[[nodiscard]] static Hooks GameHooks();

	/// What the renderer draws (milestone V3): LHVideoPlayer::DrawToScreen's colour and the textures of the mosaic
	struct Frame
	{
		/// The picture as D3D samples it: the 16-bit framebuffer expanded (graphics::rgb16::Expand), width * height * 4
		std::span<const uint8_t> rgba;
		/// The framebuffer +0x48 itself, width * height texels of `format` (pitch width * 2, 0x845141)
		std::span<const uint16_t> rgb16;
		graphics::rgb16::Format format;
		uint32_t width;  ///< +0x00
		uint32_t height; ///< +0x04
		/// g_game +0x250194
		float alpha;
		/// The vertex colour 0x00FFFFFF | alpha << 24 (VertexColour)
		uint32_t colour;
		/// GGame::FallingSpellVideo != NULL: drawn over the 3D world with alpha 0x50
		bool fallingSpell;
		/// LHVideoPlayer +0x10 (the frames decoded so far)
		int32_t frame;
		/// Goes up with each picture decoded (DecodeNextFrame's UploadToTextures 0x84514E): upload when it changes
		uint32_t serial;
	};

	explicit VideoPlayer(Hooks hooks);
	~VideoPlayer();
	VideoPlayer(const VideoPlayer&) = delete;
	VideoPlayer& operator=(const VideoPlayer&) = delete;

	/// PlayFullScreenMovie 0x54D920 (+ fn_0054AB20(file, 5)): the film before deleted, alpha 1, the pause kept and the
	/// game paused, the film opened, the wide screen kept and turned on. As in the original the player exists even
	/// when the file does not open (0x54AC05..0x54AC23): it ends on the next Process. True if the file opened
	bool Play(const std::filesystem::path& path);
	/// The fade start +0x25018C and the end +0x250190, as StartAVISequence(1) writes them after PlayFullScreenMovie
	/// (nothing without a film, 0x68F49C)
	void SetSchedule(int32_t fadeStartFrame, int32_t endFrame);
	/// StartAVISequence(1) 0x68F48E..0x68F4D1: SetSchedule(fps * 58, fps * 60) and +0x250530 = 1 (only with a film)
	void ScheduleIntro();
	/// The per frame service of Process3dEngine 0x54DAB5..0x54DD76, `realMs` the wall clock ms of the frame
	/// (game_clock::FrameRealMs: the game is paused)
	void Process(uint32_t realMs);
	/// fn_0054DA00: the skip. The falling spell's film ends (EndFallingSpellVideo); in the fade zone the film ends at
	/// once; else the fade starts now and lasts 48 frames (never past the last frame), and the pause is given back
	void Skip();
	/// GGame::ProcessKey 0x63F3B9..0x63F402, the ESC key (LH_KEY 1, the first entry of the table 0x63F6A0): with Shift
	/// or Ctrl held ([0x9A161C] = 0x10, [0x9A161E] = 0x20; inferido: the masks the same function maps to DIK_LSHIFT
	/// 0x2A / DIK_LCONTROL 0x1D at 0x63F2C6..0x63F2EC) nothing; with a film and not the no-skip byte, Skip() and
	/// StartScriptMusic(0). True when a film holds the key; false without a film (openblack's own ESC goes on;
	/// the original's 0x63F4D7). Alt ([0x9A1620] = 0x40) is not checked. Not modelled: ProcessKey's earlier ESC paths
	/// that leave before the table (0x63EF7A..0x63F2A4: the box of +0x205A10, land 6's fade back, an active SetupBox
	/// or dialog, inside the citadel), none of which openblack has
	bool EscapeKey(bool shift, bool ctrl);
	/// DeleteVideo 0x54A940: the film gone, VideoFinished + 1 (FinishedVideo on the next Process), skipping allowed
	void Stop();

	/// g_game +0x250188 != NULL (also the audio's GameQueries::videoPlaying, 0x427DF8). Safe from any thread
	[[nodiscard]] bool IsPlaying() const { return _playing.load(std::memory_order_acquire); }
	/// Process3dEngine 0x54DD5E..0x54DD7D: a film, alpha == 1.0f and not the falling spell's: the 3D world is not drawn
	[[nodiscard]] bool CoversScreen() const;
	/// The picture of the film for the renderer; nullopt without a film
	[[nodiscard]] std::optional<Frame> GetFrame() const;

	/// 0xD01984: 1 while the new profile box is up (pc_main 0x641E30), ESC does not skip; DeleteVideo / FinishedVideo
	/// clear it (0x54A97D / 0x54D907)
	void SetNoSkip(bool noSkip) { _noSkip = noSkip; }
	/// GGame::FallingSpellVideo 0xCD3B10 != NULL (KickOffFallingSpellVideo 0x5539A0; milestone V6)
	void SetFallingSpellVideo(bool on) { _fallingSpell = on; }
	/// The 16-bit format of the framebuffer ([0xEDD46C]); k_DefaultFormat
	void SetFormat(graphics::rgb16::Format format) { _format = format; }

	/// LHVideoPlayer +0x08 / +0x0C / +0x10 (0 without a film)
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

private:
	/// LHVideoPlayer (new(0x68), ctor fn_00844D00, Open fn_00844E70)
	struct Movie
	{
		BikFile file;
		std::unique_ptr<IVideoDecoder> decoder;
		bool open {false};                 ///< +0x1C, the HBINK, != NULL
		uint32_t width {0};                ///< +0x00
		uint32_t height {0};               ///< +0x04
		int32_t fps {1};                   ///< +0x08 (1 before BinkGetSummary, 0x844EA1)
		int32_t totalFrames {0};           ///< +0x0C
		int32_t frame {0};                 ///< +0x10
		std::vector<uint16_t> framebuffer; ///< +0x48, width * height * 2 bytes, zeroed (0x7C64EE)
		std::vector<uint8_t> texture;      ///< the textures' picture, as sampled
		uint32_t serial {0};
		uint64_t elapsedMs {0}; ///< (aproximado) Bink's clock since the first Process
		bool clockStarted {false};
	};

	/// fn_0054AB20 steps 2..6: the counters cleared, the player made and opened, end = total, fade = total - fps * 5
	bool Open(const std::filesystem::path& path, int32_t fadeSeconds);
	/// VideoPoll 0x54AA40 for each frame due: past the end DeleteVideo, else DecodeNextFrame and VideoFramesReady + 1
	void Poll();
	/// LHVideoPlayer::DecodeNextFrame fn_008450B0
	void DecodeNextFrame();
	/// GGame::FinishedVideo 0x54D8D0
	void FinishedVideo();

	Hooks _hooks;
	std::unique_ptr<Movie> _movie;                     ///< g_game +0x250188
	std::atomic<bool> _playing {false};                ///< _movie != nullptr, for other threads
	int32_t _fadeStartFrame {0};                       ///< g_game +0x25018C
	int32_t _endFrame {0};                             ///< g_game +0x250190
	float _alpha {0.0f};                               ///< g_game +0x250194
	bool _isIntro {false};                             ///< g_game +0x250530
	int32_t _framesReady {0};                          ///< GGame::VideoFramesReady 0xD01988
	int32_t _finished {0};                             ///< GGame::VideoFinished 0xD0198C
	bool _previousPause {false};                       ///< GGame::VideoPreviousPause 0xD0199C
	int32_t _previousWideScreen {0};                   ///< 0xD019A0
	bool _noSkip {false};                              ///< 0xD01984
	bool _fallingSpell {false};                        ///< GGame::FallingSpellVideo 0xCD3B10 != NULL
	graphics::rgb16::Format _format {k_DefaultFormat}; ///< [0xEDD46C]
};

/// The game's one (GGame's fields), with GameHooks()
[[nodiscard]] VideoPlayer& Get();
/// Get().IsPlaying(): g_game +0x250188 != NULL, what audio connects GameQueries::videoPlaying to
[[nodiscard]] bool IsPlaying();

} // namespace openblack::video
