/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VideoPlayer.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <spdlog/spdlog.h>

#include "3D/ScreenFade.h"
#include "Audio/Services/GameMusic.h"
#include "Game.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"

using namespace openblack;
using namespace openblack::video;

float video::FadeAlpha(int32_t frame, int32_t fadeStart, int32_t end) noexcept
{
	// 0x54DB5F `sub edx, ecx` (f - start), 0x54DB61 `sub eax, ecx` (end - start), 0x54DB6B..0x54DB70 `cdq; xor; sub;
	// inc` (|end - start| + 1): 32-bit wrapping arithmetic, as unsigned here
	const auto done = static_cast<int32_t>(static_cast<uint32_t>(frame) - static_cast<uint32_t>(fadeStart));
	const auto length = static_cast<int32_t>(static_cast<uint32_t>(end) - static_cast<uint32_t>(fadeStart));
	const uint32_t sign = length < 0 ? 0xFFFFFFFFu : 0u;
	const auto span = static_cast<int32_t>(((static_cast<uint32_t>(length) ^ sign) - sign) + 1u);
	// 0x54DB67 fild, 0x54DB75 fidiv, 0x54DB79 fsubr [0x8AA390] (1.0), 0x54DB7F fstp: each rounded to 24 bits
	return 1.0f - static_cast<float>(done) / static_cast<float>(span);
}

uint32_t video::VertexColour(float alpha, bool fallingSpell) noexcept
{
	const uint32_t base = fallingSpell ? k_FallingSpellAlpha : k_OpaqueAlpha; // 0x54DC11..0x54DC1B
	// 0x54DC29 fild, 0x54DC3C fmul +0x250194, 0x54DC42 __ftol (truncates), 0x54DC4D the low byte
	const auto byte = static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(base) * alpha)) & 0xFFu;
	return k_VertexRgb | byte << 24;
}

ScreenRect video::FullScreenRect(int32_t screenWidth, int32_t screenHeight, float letterboxScale) noexcept
{
	// 0x54DBEB fild H, 0x54DBF1 fild W, 0x54DBF7 fmul 0.5625, 0x54DBFD fsubp, 0x54DBFF fmul [0xBEC16C], 0x54DC05 __ftol
	const float bars =
	    (static_cast<float>(screenHeight) - static_cast<float>(screenWidth) * k_LetterboxAspect) * letterboxScale;
	const int32_t letterbox = static_cast<int32_t>(bars) / 2; // 0x54DC0A..0x54DC0F `cdq; sub; sar 1`
	// DrawToScreen(colour, 0, bars, W + 1, H - 2 bars + 1) 0x54DC5A..0x54DC6D
	return {0, letterbox, screenWidth + 1, screenHeight - 2 * letterbox + 1};
}

uint32_t video::FramesDue(uint64_t elapsedMs, uint32_t fpsNumerator, uint32_t fpsDenominator) noexcept
{
	if (fpsNumerator == 0 || fpsDenominator == 0)
	{
		return 0;
	}
	const uint64_t due = elapsedMs * fpsNumerator / (static_cast<uint64_t>(fpsDenominator) * 1000u) + 1u;
	return static_cast<uint32_t>(std::min<uint64_t>(due, std::numeric_limits<uint32_t>::max()));
}

VideoPlayer::Hooks VideoPlayer::GameHooks()
{
	Hooks hooks;
	hooks.isPaused = []() { return game_clock::IsPaused(); };
	hooks.pauseGame = [](bool paused) { game_clock::Pause(paused); };
	// the original always has g_game +0x25005C; without a HelpSystem (openblack only, tools) there is no wide screen
	hooks.wideScreen = []() {
		const auto* helpSystem = help::Get();
		return helpSystem != nullptr ? helpSystem->GetWideScreen() : 0;
	};
	// HelpSystem's hook moves the bars (ScreenFade::SetWideScreen) and tells audio (Game.cpp)
	hooks.setWideScreen = [](int32_t on) {
		if (auto* helpSystem = help::Get(); helpSystem != nullptr)
		{
			helpSystem->SetWideScreen(on, 0);
		}
	};
	// fn_005C6C40 0x54D9EF: the bars at 100 % at once (ScreenFade::SnapWideScreen)
	hooks.snapWideScreen = []() {
		if (Game::Instance() != nullptr)
		{
			Game::Instance()->GetScreenFade().SnapWideScreen();
		}
	};
	hooks.stopScriptMusic = []() {
		const auto lock = audio::game_music::Lock();
		if (auto* gameMusic = audio::game_music::Get(); gameMusic != nullptr)
		{
			gameMusic->ScriptStopMusic();
		}
	};
	hooks.makeDecoder = []() -> std::unique_ptr<IVideoDecoder> { return std::make_unique<NullVideoDecoder>(); };
	return hooks;
}

VideoPlayer::VideoPlayer(Hooks hooks)
    : _hooks(std::move(hooks))
{
}

VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::Play(const std::filesystem::path& path)
{
	// 0x54D923 ClearTipVideo: openblack has no loading screen, so no tip video
	_isIntro = false; // 0x54D928
	Stop();           // 0x54D939 DeleteVideo(true)
	_alpha = 1.0f;    // 0x54D946
	// 0x54D963 GAudio +0x1C = -1 (GameMusic::_alignmentType, the music playing forgotten): not ported, audio has no
	// setter (pending; ProcessMusic 0x427EB3 does it again in the fade zone)
	_previousPause = _hooks.isPaused && _hooks.isPaused(); // 0x54D943..0x54D95C
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(true); // 0x54D96A PauseGame(1)
	}
	// 0x54D96F..0x54D999 the film's sound bank (LHBankRegister): both callers pass NULL, not ported
	const bool opened = Open(path, k_FadeSeconds); // 0x54D9A5 fn_0054AB20(file, 5)
	// 0x54D9AD fn_0054AB00, the 16 ms timer of VideoPoll: Process polls instead (aproximado)
	_previousWideScreen = _hooks.wideScreen ? _hooks.wideScreen() : 0; // 0x54D9BE
	if (_previousWideScreen == 0 && _hooks.setWideScreen)
	{
		_hooks.setWideScreen(1); // 0x54D9D0..0x54D9E4 SetWideScreen(+0x45E8 == 0, 0)
	}
	// 0x54D9EF HelpSystem fn_005C6C40 (+0x45F0 = -FLT_MAX, the bars at 100 % at once): reached after the test of
	// 0x54D9D0 whether the bars were turned on here or were already on
	if (_hooks.snapWideScreen)
	{
		_hooks.snapWideScreen();
	}
	return opened;
}

bool VideoPlayer::Open(const std::filesystem::path& path, int32_t fadeSeconds)
{
	_finished = 0;    // 0x54ABD0 (0xD01990, the film's music started, 0x54ABD6: not ported)
	_framesReady = 0; // 0x54ABE2
	if (_movie)
	{
		Stop(); // 0x54ABE8..0x54ABF1 DeleteVideo(false)
	}
	// 0x54AC05..0x54AC23: new(0x68), the ctor, g_game +0x250188 = the player, whether the file opens or not
	_movie = std::make_unique<Movie>();
	_playing.store(true, std::memory_order_release);
	auto& movie = *_movie;

	// LHVideoPlayer::Open(path, 16 bits, one texture false) 0x54AC3B -> fn_00844E70
	bool opened = movie.file.Open(path); // BinkOpen 0x844E8C
	if (opened)
	{
		movie.decoder = _hooks.makeDecoder ? _hooks.makeDecoder() : std::make_unique<NullVideoDecoder>();
		opened = movie.decoder != nullptr && movie.decoder->Open(movie.file);
	}
	movie.fps = 1; // 0x844EA1
	if (opened)
	{
		// BinkGetSummary 0x844EB6..0x844EE1: +0 width, +4 height, +8 fps, +0xC frames, +0x10 = 0
		movie.open = true;
		movie.width = movie.file.Width();
		movie.height = movie.file.Height();
		movie.fps = static_cast<int32_t>(movie.file.Fps());
		movie.totalFrames = static_cast<int32_t>(movie.file.FrameCount());
		movie.frame = 0;
		// +0x48: width * height * 2 bytes, zeroed; the textures show what it holds
		movie.framebuffer.assign(static_cast<size_t>(movie.width) * movie.height, 0);
		movie.texture.assign(movie.framebuffer.size() * 4, 0);
		graphics::rgb16::Expand(_format, movie.framebuffer, movie.texture);
	}
	else if (auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->warn("Video: cannot play {}: {}", path.string(),
		             movie.file.GetError().empty() ? std::string("the decoder refused it") : movie.file.GetError());
	}
	_endFrame = movie.totalFrames;                                 // 0x54AC4B..0x54AC4E
	_fadeStartFrame = movie.totalFrames - movie.fps * fadeSeconds; // 0x54AC60..0x54AC70
	// 0x54AC76..0x54ACD8 the pre-roll (BinkNextFrame / BinkWait / BinkService, up to 10 times): no frame decoded
	return opened;
}

void VideoPlayer::SetSchedule(int32_t fadeStartFrame, int32_t endFrame)
{
	if (!_movie)
	{
		return; // 0x68F49C
	}
	_fadeStartFrame = fadeStartFrame;
	_endFrame = endFrame;
}

void VideoPlayer::ScheduleIntro()
{
	if (!_movie)
	{
		return; // 0x68F49C
	}
	SetSchedule(_movie->fps * k_IntroFadeStartSeconds, _movie->fps * k_IntroEndSeconds); // 0x68F4AF / 0x68F4C6
	_isIntro = true;                                                                     // 0x68F4D1
}

void VideoPlayer::Process(uint32_t realMs)
{
	// 0x54DAD7 BinkService: nothing to service
	if (_finished != 0)
	{
		FinishedVideo(); // 0x54DADD..0x54DAE7
	}
	if (!_movie)
	{
		return; // 0x54DAF9
	}
	// (aproximado) the timer thread's VideoPoll: the frames due by the wall clock since the first Process of the film
	if (_movie->clockStarted)
	{
		_movie->elapsedMs += realMs;
	}
	_movie->clockStarted = true;
	Poll();
	if (!_movie)
	{
		return; // the poll ended it: no draw, the 3D world drawn (0x54DD5E)
	}

	_alpha = 1.0f; // 0x54DB05, every frame
	const int32_t frame = _movie->frame;
	if (frame >= _endFrame) // 0x54DB0E `jl`
	{
		_alpha = 0.0f; // 0x54DB14
		Stop();        // 0x54DB1A DeleteVideo(true)
		return;
	}
	if (frame > _fadeStartFrame) // 0x54DB27 `jle`
	{
		const bool paused = _hooks.isPaused && _hooks.isPaused();
		if (_previousPause != paused && _hooks.pauseGame)
		{
			_hooks.pauseGame(_previousPause); // 0x54DB2F..0x54DB42
		}
		_alpha = FadeAlpha(frame, _fadeStartFrame, _endFrame); // 0x54DB4A..0x54DB7F
	}
	// 0x54DB85..0x54DBD1: opaque and no new frame -> wait up to 1000 x 0.5 ms for one. (aproximado) not blocking: the
	// picture of the last frame decoded stays, the frames come at the same times
	// 0x54DBEB..0x54DC6D DrawToScreen(VertexColour, FullScreenRect): the renderer reads GetFrame() (milestone V3)
	_framesReady = 0; // 0x54DC72
}

void VideoPlayer::Poll()
{
	for (;;)
	{
		if (!_movie)
		{
			return;
		}
		auto& movie = *_movie;
		if (movie.frame > _endFrame) // 0x54AA61..0x54AA6A `jle`
		{
			Stop(); // 0x54AA6E DeleteVideo(true)
			return;
		}
		// 0x54AA94..0x54AAA9: no HBINK, or BinkWait says it is not time yet
		if (!movie.open || static_cast<uint32_t>(std::max(movie.frame, 0)) >=
		                       FramesDue(movie.elapsedMs, movie.file.FpsNumerator(), movie.file.FpsDenominator()))
		{
			return;
		}
		DecodeNextFrame(); // 0x54AAB7
		++_framesReady;    // 0x54AAC1
	}
}

void VideoPlayer::DecodeNextFrame()
{
	auto& movie = *_movie;
	if (!movie.open)
	{
		return; // 0x8450B8
	}
	if (movie.frame > movie.totalFrames)
	{
		++movie.frame; // 0x8450C4 `jg` -> 0x845164
		return;
	}
	// 0x8450E3 BinkNextFrame, 0x8450FE BinkDoFrame (0: decoded)
	const auto pixels = movie.decoder->DecodeNext(static_cast<uint32_t>(movie.frame));
	if (pixels.size() >= movie.framebuffer.size() * 4 && !movie.framebuffer.empty())
	{
		// 0x845146 BinkCopyToBuffer(flags 9 / 10): the 16-bit framebuffer; 0x84514E UploadToTextures fn_00845420:
		// the textures, sampled by D3D as Expand
		graphics::rgb16::Quantize(_format, pixels, movie.framebuffer);
		graphics::rgb16::Expand(_format, movie.framebuffer, movie.texture);
		++movie.serial;
	}
	++movie.frame; // 0x845164
}

void VideoPlayer::Skip()
{
	if (_fallingSpell)
	{
		// 0x54DA00..0x54DA12: GGame::EndFallingSpellVideo (milestone V6)
		if (_hooks.endFallingSpellVideo)
		{
			_hooks.endFallingSpellVideo();
		}
		return;
	}
	if (!_movie)
	{
		return; // 0x54DA20
	}
	const int32_t frame = _movie->frame;
	if (frame > _fadeStartFrame) // 0x54DA25 `jle`
	{
		Stop(); // 0x54DA2F: already fading, it ends now
		return;
	}
	_fadeStartFrame = frame;                                             // 0x54DA3E
	_endFrame = std::min(frame + k_SkipFadeFrames, _movie->totalFrames); // 0x54DA44..0x54DA57 (signed `jle`)
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(_previousPause); // 0x54DA63 PauseGame(VideoPreviousPause)
	}
}

bool VideoPlayer::EscapeKey(bool shift, bool ctrl)
{
	if (!_movie)
	{
		return false; // 0x63F3E6 -> 0x63F4D7: the key's other use
	}
	if (shift || ctrl)
	{
		return true; // 0x63F3C6 / 0x63F3D6 -> 0x63F407
	}
	if (_noSkip)
	{
		return true; // 0x63F3F3 -> 0x63F407
	}
	Skip(); // 0x63F3F5
	if (_hooks.stopScriptMusic)
	{
		_hooks.stopScriptMusic(); // 0x63F402 GAudio::StartScriptMusic(0)
	}
	return true;
}

void VideoPlayer::Stop()
{
	if (_movie)
	{
		_movie.reset(); // 0x54A959 the dtor (BinkClose), 0x54A969 +0x250188 = 0
		_playing.store(false, std::memory_order_release);
		++_finished;     // 0x54A977
		_noSkip = false; // 0x54A97D
	}
	// 0x54A983..0x54A99A timeKillEvent and 0xD01990 = 0: no timer here
}

void VideoPlayer::FinishedVideo()
{
	if (_hooks.pauseGame)
	{
		_hooks.pauseGame(_previousPause); // 0x54D8D9
	}
	const int32_t wideScreen = _hooks.wideScreen ? _hooks.wideScreen() : 0; // 0x54D8E4
	if (wideScreen != _previousWideScreen && _hooks.setWideScreen)
	{
		_hooks.setWideScreen(wideScreen == 0 ? 1 : 0); // 0x54D8F2..0x54D902 SetWideScreen(+0x45E8 == 0, 0)
	}
	_noSkip = false; // 0x54D907
	_finished = 0;   // 0x54D90E
}

bool VideoPlayer::CoversScreen() const
{
	// 0x54DD56..0x54DD74: +0x250188 != NULL && +0x250194 == 1.0f, and not FallingSpellVideo
	return _movie != nullptr && _alpha == 1.0f && !_fallingSpell;
}

std::optional<VideoPlayer::Frame> VideoPlayer::GetFrame() const
{
	if (!_movie)
	{
		return std::nullopt;
	}
	const auto& movie = *_movie;
	return Frame {
	    .rgba = movie.texture,
	    .rgb16 = movie.framebuffer,
	    .format = _format,
	    .width = movie.width,
	    .height = movie.height,
	    .alpha = _alpha,
	    .colour = VertexColour(_alpha, _fallingSpell),
	    .fallingSpell = _fallingSpell,
	    .frame = movie.frame,
	    .serial = movie.serial,
	};
}

int32_t VideoPlayer::Fps() const
{
	return _movie ? _movie->fps : 0;
}

int32_t VideoPlayer::TotalFrames() const
{
	return _movie ? _movie->totalFrames : 0;
}

int32_t VideoPlayer::CurrentFrame() const
{
	return _movie ? _movie->frame : 0;
}

VideoPlayer& video::Get()
{
	static VideoPlayer s_player(VideoPlayer::GameHooks());
	return s_player;
}

bool video::IsPlaying()
{
	return Get().IsPlaying();
}
