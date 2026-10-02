/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpellVideo.h"

#include <exception>
#include <utility>

#include "3D/ScreenFade.h"
#include "Audio/Audio.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "Locator.h"
#include "VideoPlayer.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
/// Process3dEngine 0x54E2AD `cmp ecx, 3; je 0x54E3C7`: the mode that runs neither fade
constexpr int32_t k_SequenceModeNoFade = 3;
/// Temple::UpdateFade 0x794293 `fmul [0x8AA3B0]`: g_delta_time in seconds
constexpr float k_MsToSeconds = 0.001f;
/// Temple::UpdateFade 0x794346 `fmul [0x8AB270]`
constexpr float k_AlphaScale = 255.0f;
} // namespace

int32_t video::FallingSpellFilmMs(int32_t frame, int32_t fps) noexcept
{
	if (fps == 0)
	{
		return 0; // (openblack) the callers never divide by 0: fps <= 0 becomes 0x18 first
	}
	// 0x526E61..0x526E71: frame * 1000 in 32 bits (wrapping), then `cdq; idiv`
	const auto product = static_cast<int32_t>(static_cast<uint32_t>(frame) * 1000u);
	return product / fps;
}

bool TempleFade::Runs(int32_t mode) const noexcept
{
	if (mode == k_SequenceModeNoFade)
	{
		return false; // 0x54E2AD
	}
	// 0x54E2B3 fld [0xE06020], 0x54E2B9 fcomp [0xC2A150], 0x54E2C4 `je` (not equal) -> UpdateFade, 0x54E2C9 mode 1 ->
	// UpdateFade, else 0x54E2D7 GScript::ProcessFade(1)
	return target != current || mode == k_SequenceModeCitadel;
}

uint32_t TempleFade::Update(uint32_t deltaMs) noexcept
{
	// The x87 runs at 24 bits (precision control 00): fn_007DEE00 (fninit; `and cw, 0xFCFF`; fldcw 0x7DEE0D) is called
	// by Process3dEngine after FinishFrame (0x54E426, 0x54E4D1), by EndTurn (0x54E964..) and InitOneTimeOnly, so every
	// fmul / fadd / fsub here rounds to float, as float arithmetic does. (inferido) that nothing between the previous
	// frame's call and 0x54E2DE raises the precision again (D3D7 without FPUPRESERVE keeps it at 24 bits too)
	const float step = static_cast<float>(deltaMs) * k_MsToSeconds; // 0x79428D fild, 0x794293 fmul
	uint32_t colourRgb = rgb;
	if (current < target) // 0x794280..0x79429E (C0: current < target)
	{
		const float value = current + step; // 0x7942E6 fadd
		current = value;                    // 0x7942EC fst
		// 0x7942F2..0x7942FD (neither C0 nor C3): only past the target; landing on it exactly leaves target == current,
		// and Process3dEngine then stops calling UpdateFade (0x54E2C4), the fade held, [0xE06028] never + 1
		if (value > target)
		{
			++done;            // 0x7942FF..0x794311
			current = target;  // 0x79430B
			target = 0.0f;     // 0x794316
		}
	}
	else
	{
		const float value = current - step; // 0x7942A0 fsubr
		current = value;                    // 0x7942A6 fst
		if (value < target)                 // 0x7942AC..0x7942B7 (C0)
		{
			current = target; // 0x7942B9..0x7942CA
			++done;           // 0x7942CF
			if (target <= 0.0f) // 0x7942BE..0x7942DA (C0 or C3)
			{
				rgb = 0; // 0x7942DC..0x7942DE [0xE06024] = 0 (esi = 0)
				colourRgb = 0;
			}
		}
	}
	// 0x794326..0x79434C: 0xFF above 1.0, else ftol(current * 255) (the product rounded to float, __ftol truncates)
	const uint32_t alpha = current > 1.0f ? 0xFFu : static_cast<uint32_t>(static_cast<int32_t>(current * k_AlphaScale));
	// 0x794352..0x79435D: alpha << 24 + (rgb & 0xFFFFFF), then fn_0053CE60 writes [0xFA51D8]
	return (alpha << 24) + (colourRgb & 0x00FFFFFFu);
}

FallingSpellVideo::Hooks FallingSpellVideo::GameHooks()
{
	Hooks hooks;
	// (inferido) openblack has no GPlayer: the local player is PLAYER_ONE (as audio's localPlayerNumber, Game.cpp), and
	// its creature any entity with a Creature of that owner
	hooks.hasCreature = []() {
		if (!Locator::entitiesRegistry::has_value())
		{
			return false;
		}
		bool found = false;
		Locator::entitiesRegistry::value().Each<const ecs::components::Creature>(
		    [&found](entt::entity, const ecs::components::Creature& creature) {
			    found = found || creature.owner == PlayerNames::PLAYER_ONE;
		    });
		return found;
	};
	hooks.filmPath = []() {
		const std::filesystem::path path = "Data/Spells/fall/fall.bik";
		if (!Locator::filesystem::has_value())
		{
			return path;
		}
		try
		{
			return Locator::filesystem::value().FindPath(path);
		}
		catch (const std::exception&)
		{
			return path; // the player is made anyway and ends on the next frame (0x54AC05)
		}
	};
	// The update's 11 sounds, all 2D (+0x08 = 0): a Play is LH_SamplePlayOptions' ctor 0x10010E90 (sample_play::Options'
	// defaults) with bank +0x04, owner +0x20 and sample +0x24 (pitch +0x48 = 133 once, 0x527119, no caller mask) through
	// GAudio::PlaySoundEffect(options*) 0x429E30; a Stop is LHSampleStop(bank, owner, sample) called directly. The
	// owners 1 and 2 are not things: Owner::Key, so each loop can be stopped alone
	hooks.sound = [](const FallingSpellSound& sound) {
		const auto bank = static_cast<audio::SfxBank>(sound.bank); // 1 InGame, 3 Spells, 5 ScriptSfx (BankTables.h)
		const auto owner = sound.owner == 0 ? audio::Owner::None() : audio::Owner::Key(sound.owner);
		if (sound.kind == FallingSpellSound::Kind::Stop)
		{
			audio::StopSoundEffect(sound.sample, owner, bank);
			return;
		}
		audio::PlayOptions options;
		options.sample = {audio::Bank(bank), sound.sample};
		options.owner = owner;
		options.is3D = false;
		options.pitch = sound.pitch; // the ctor's 100 but for 0x527119
		audio::PlaySoundEffect(options);
	};
	// 0x5271B0: LHMusicStop(1) 0x1000E530, every music channel fades out
	hooks.musicStop = [](int32_t fade) { audio::MusicStop(fade); };
	hooks.setScreenFadeColour = [](uint32_t argb) {
		if (Game::Instance() != nullptr)
		{
			Game::Instance()->GetScreenFade().SetColour(argb);
		}
	};
	return hooks;
}

FallingSpellVideo::FallingSpellVideo(VideoPlayer& player, Hooks hooks)
    : _player(player)
    , _hooks(std::move(hooks))
{
}

void FallingSpellVideo::KickOff()
{
	if (!_hooks.hasCreature || !_hooks.hasCreature())
	{
		return; // 0x5539BE..0x5539C0 `je 0x553A0E`: no creature, nothing at all
	}
	End(); // 0x5539C4 EndFallingSpellVideo: the previous one, if any
	Start();
}

void FallingSpellVideo::Start()
{
	_mode = k_SequenceModeFallingSpell; // 0x5539D5
	// 0x5539DF new(0x40) and fn_00527240 0x5539ED: +0x00 = +0x04 = 0; a failed new (0x553A00) is not modelled
	_active = true;                    // 0x5539F4 FallingSpellVideo = the object
	_player.SetFallingSpellVideo(true);
	Init(); // 0x5539F9
}

void FallingSpellVideo::Init()
{
	// 0x526068..0x526070: Close if +0x00 (never on a new object)
	// 0x526075..0x5261EB (not ported): fall.cm2, the CreatureFalling, its hand glows; +0x08 the path, +0x04 the creature
	_lastMs = 0x64; // 0x52615F +0x0C = 100
	// 0x5261F0..0x5261FC: PlayFullScreenMovie("data\spells\fall\fall.bik", NULL)
	const std::filesystem::path path = _hooks.filmPath ? _hooks.filmPath() : std::filesystem::path("Data/Spells/fall/fall.bik");
	_player.Play(path);
	// 0x526201..0x5262D5 inside the video section
	if (_player.IsPlaying()) // 0x52620B..0x526213 +0x250188 != NULL
	{
		// 0x526219..0x52621E: fps <= 0 -> 0x18 into the player (aproximado: read in Update instead; a film that opens
		// always has fps >= 1 here, BikFile refuses 0)
		// 0x526225..0x5262BF (not ported): the camera at the path's point of frame * 1000 / fps, its points * 0.8
		_player.SetSchedule(_player.EndFrame(), _player.EndFrame()); // 0x5262C4..0x5262CF +0x25018C = +0x250190
	}
	// 0x5262DA +0x00 = 1; 0x5262E0..0x526311 +0x10 the camera's position [0xEA9E90] kept; 0x52631E the sprite, 0x526335
	// the 16 sprite records (not ported)
	_sparklesOn = false; // 0x526425 +0x1C = 0
	_state = 0;          // 0x526428 +0x20 = 0
	_soundState = 0;     // 0x52642B +0x24 = 0
	// 0x52642E..0x52644C the light bursts +0x3C, +0x28 = 0, +0x30 = 1.0f, +0x2C = 0, RegisterFinishFrameCallback(0,
	// 0x526480, this) 0x526466 (not ported)
}

void FallingSpellVideo::Close()
{
	// 0x5264AD: only with +0x00. 0x5264B6 RemoveFinishFrameCallback, +0x00 = 0, [0xC64204] = 0, the creature deleted,
	// the path freed (fn_0086D4D0), the camera put back (fn_0081E1F0 with +0x10), the sprites (not ported)
}

void FallingSpellVideo::End()
{
	if (!_active)
	{
		return; // 0x553A1A
	}
	_mode = k_SequenceModeNone; // 0x553A1C
	Close();                    // 0x553A2D
	// 0x553A3E fn_00527250 (the dtor: Close again only with +0x00, now 0) and delete 0x553A44
	_active = false; // 0x553A4E FallingSpellVideo = NULL
	_player.SetFallingSpellVideo(false);
	_player.Skip(); // 0x553A58 fn_0054DA00, without FallingSpellVideo: the 48-frame fade (or the end in the fade zone)
}

void FallingSpellVideo::Sound(FallingSpellSound::Kind kind, FallingSpellSound::Bank bank, int32_t sample,
                              uint32_t owner, uint32_t address, int32_t pitch) const
{
	if (_hooks.sound)
	{
		_hooks.sound(FallingSpellSound {kind, bank, sample, owner, pitch, address});
	}
}

void FallingSpellVideo::Update()
{
	using Kind = FallingSpellSound::Kind;
	using Bank = FallingSpellSound::Bank;
	if (!_active)
	{
		return; // 0x526E15 (+0x00)
	}
	if (!_player.IsPlaying())
	{
		++_state; // 0x526E21..0x526E29
		return;   // 0x526E41
	}
	int32_t fps = _player.Fps();
	if (fps <= 0)
	{
		fps = k_FallingSpellFallbackFps; // 0x526E49..0x526E4E
	}
	const int32_t ms = FallingSpellFilmMs(_player.CurrentFrame(), fps); // 0x526E55..0x526E71
	// 0x526E74..0x526E8B: the creature animated by ms - +0x0C (vt +0x14, not ported); 0x526EA6 +0x0C = ms
	_lastMs = ms;
	// 0x526E8E..0x526F1C the camera on the path at ms, ChangeFov(0x3F490FDB = pi / 4) (not ported)

	// +0x24, each test after the one before (one call can go through all three)
	if (_soundState == 0 && ms > k_FallingSpellSoundOneMs) // 0x526F21..0x526F3A
	{
		_soundState = 1;                                         // 0x526F40
		Sound(Kind::Play, Bank::ScriptSfx, 0x97, 0, 0x526F6E); // 151 ScreenRumble
	}
	if (_soundState == 1 && ms > k_FallingSpellSoundTwoMs) // 0x526F79..0x526F87
	{
		_soundState = 2;                                     // 0x526F8D
		Sound(Kind::Play, Bank::Spells, 0x38, 0, 0x526FBB); // 56 S_LasersbeamExplode_02
		Sound(Kind::Stop, Bank::InGame, 0xAC, 1, 0x526FD6); // 172 G_Volcano_02, owner 1
	}
	if (_soundState == 2 && ms > k_FallingSpellSoundThreeMs) // 0x526FE2..0x526FF0
	{
		_soundState = 3;                                     // 0x526FF6
		Sound(Kind::Play, Bank::InGame, 0xA6, 0, 0x527024); // 166 G_Creed_01
	}

	// +0x20
	if (_state == 0 && ms > k_FallingSpellStateOneMs) // 0x52702F..0x52703C
	{
		_sparklesOn = true; // 0x527047 +0x1C = 1
		_state = 1;         // 0x52704A
		Sound(Kind::Play, Bank::InGame, 0xA8, 0, 0x527074); // 168 G_CitadelExplode_01
		Sound(Kind::Play, Bank::InGame, 0xAC, 1, 0x5270A2); // 172 G_Volcano_02, owner 1
	}
	if (_state == 1 && ms > k_FallingSpellStateTwoMs) // 0x5270AD..0x5270BB
	{
		_state = 2;                                          // 0x5270C1
		Sound(Kind::Play, Bank::Spells, 0x1E, 0, 0x5270EF); // 30 S_HealChakra
		Sound(Kind::Play, Bank::InGame, 0xA6, 2, 0x527125, 0x85); // 166 G_Creed_01, owner 2, pitch 133 (0x527119)
	}
	if (_state == 2 && ms > k_FallingSpellStateThreeMs) // 0x527130..0x527142
	{
		// 0x527148..0x527170: the Temple fade to white, from 0 to 1
		_fade.rgb = k_FallingSpellFadeRgb; // 0x527152 [0xE06024]
		_fade.target = 1.0f;               // 0x52715C [0xE06020]
		_fade.current = 0.0f;              // 0x527166 [0xC2A150]
		_fade.done = 0;                    // 0x527170 [0xE06028]
		Sound(Kind::Stop, Bank::InGame, 0xA6, 0, 0x527181); // 166 G_Creed_01
		Sound(Kind::Stop, Bank::InGame, 0xA6, 2, 0x52719D); // 166 G_Creed_01, owner 2
		++_state;                                            // 0x5271A3 -> 3
		if (_hooks.musicStop)
		{
			_hooks.musicStop(k_FallingSpellMusicFade); // 0x5271B0 LHMusicStop(1)
		}
		Sound(Kind::Play, Bank::InGame, 0xA8, 0, 0x5271E1); // 168 G_CitadelExplode_01
	}
	if (_state == 3 && _fade.done != 0) // 0x5271EC..0x5271FA
	{
		_state = k_FallingSpellEndState; // 0x5271FC
		// 0x527203..0x527221: the fade back from white, from 1 to 0
		_fade.rgb = k_FallingSpellFadeRgb;
		_fade.target = 0.0f;
		_fade.current = 1.0f;
		_fade.done = 0;
	}
}

void FallingSpellVideo::ProcessFrame(uint32_t realMs)
{
	// 0x54DD83..0x54DD95: switch (+0x205A28), case 2 at 0x54DD9B
	if (_mode == k_SequenceModeFallingSpell)
	{
		// 0x54DDAB LH3DAtmos::Update3D, 0x54DDB5 g_mode_cleaning = 0 (not ported)
		Update(); // 0x54DDBB fn_00553A60: 0x526E00 when FallingSpellVideo != NULL
		// 0x54DDC0..0x54DDD2; (openblack) `!_active`: the original reads FallingSpellVideo->+0x20 with no test (mode 2
		// without the object only after a failed new(0x40), which crashes in Init)
		if (!_player.IsPlaying() || !_active || _state == k_FallingSpellEndState)
		{
			End(); // 0x54DDD6, then 0x54E2A4
		}
		// else 0x54DDE0 FallingSpell::Draw (the renderer draws the film; the creature and the sprites are not ported),
		// 0x54DDF5 / 0x54DDFD the liquid particles
	}
	// 0x54E2A4..0x54E2DE
	if (_fade.Runs(_mode))
	{
		const uint32_t colour = _fade.Update(realMs); // 0x54E2DE Temple::UpdateFade
		if (_hooks.setScreenFadeColour)
		{
			_hooks.setScreenFadeColour(colour); // 0x794361 fn_0053CE60
		}
	}
	// else 0x54E2D7 GScript::ProcessFade(1): openblack's ScreenFade::ProcessTurn, once a turn (Game.cpp)
}

FallingSpellVideo& video::GetFallingSpell()
{
	static FallingSpellVideo s_fallingSpell(video::Get(), FallingSpellVideo::GameHooks());
	return s_fallingSpell;
}
