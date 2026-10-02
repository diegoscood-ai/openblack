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

#include <filesystem>
#include <functional>

/// The falling spell's film of runblack.exe W120 (fall.bik): GGame::KickOffFallingSpellVideo 0x5539A0 /
/// EndFallingSpellVideo 0x553A10, the FallingSpell object (new(0x40), Init 0x526060, Close 0x5264A0, its update
/// 0x526E00, Draw 0x5267D0) and the Temple fade it drives (Temple::UpdateFade 0x794280). Wiki: video.md, "La caída del
/// hechizo"; reading of the original: dev\tmp_dis\video\original.md §5.2.
///
/// - CHL 203 SET_AVI_SEQUENCE(on, 2): PSysGlobal::StartAVISequence 0x68F459 -> KickOff (only when the local player
///   has a creature), StopAVISequence 0x68F4F7 -> End.
/// - KickOff: the previous one ended, g_game +0x205A28 = 2, FallingSpellVideo = the object, Init: PlayFullScreenMovie
///   ("data\spells\fall\fall.bik", NULL) 0x5261FC (the game paused, the wide screen on, as the intro) and the fade start
///   moved to the end (0x5262C4..0x5262CF): the film has no fade of its own.
/// - Process3dEngine case 2 (0x54DD9B..0x54DE02), every frame: the land is not drawn; fn_00553A60 runs the update
///   0x526E00 (the state +0x20 and the sounds by the film's ms), then without a film or at state 4 EndFallingSpellVideo,
///   else FallingSpell::Draw (the film with base alpha 0x50 first, LHVideoPlayer::thedraw 0x52689F, then the falling
///   creature and its sprites) and the liquid particles.
/// - State: 0 -> 1 at 13.45 s, 1 -> 2 at 37.75 s, 2 -> 3 at 43.9 s (a 1 s white Temple fade up, LHMusicStop(1)),
///   3 -> 4 when that fade is up (the fade goes back down from white): EndFallingSpellVideo, whose fn_0054DA00 is then
///   the normal skip (no FallingSpellVideo any more): a 48-frame fade of the film with base alpha 0xFF, the pause given
///   back. ESC (fn_0054DA00 0x54DA0C) ends it the same way.
///
/// Not ported (no CreatureFalling, no camera path, no LightBurst in openblack; the session milagros owns spells): Init's
/// fall.cm2 camera path (LHFileLength / LHLoadData / fn_0086D4A0 0x52607F..0x5260B6), the CreatureFalling copy of the
/// player's creature (new(0x57B8) 0x5260D1, ctor 0x52610E, vtable 0x8D8BD8) and its hand glows (0x526186..0x5261EB),
/// the camera along the path with ChangeFov(pi / 4) (0x526259..0x5262BF, 0x526EA9..0x526F1C), the 16 sprites (new(0x200)
/// 0x526335) and the light bursts (new(0x400) 0x52642E, the finish frame callback 0x526480 -> 0x526530), everything
/// FallingSpell::Draw draws besides the film, Close's camera put back (fn_0081E1F0 0x5264F4), LH3DAtmos::Update3D
/// (0x54DDAB) and the other readers of +0x205A28 (GCamera::Update 0x44233C.., fn_00516CB0, fn_00517080,
/// AddPlayerSparkles 0x55264D, fn_005739F0, Process3dEngine 0x54E3D2 / 0x54E4BC). The sounds and the music stop are
/// events (Hooks::sound / musicStop); GameHooks() plays them through Audio.h (PlaySoundEffect / StopSoundEffect /
/// MusicStop).
/// (aproximado) In mode 2 the original draws the film inside Process3dEngine (thedraw 0x52689F), so FinishFrame's bars
/// and fade go over it; openblack draws bars, film, fade (Renderer::DrawFinishFrameOverlays): the same picture while
/// the bars are at 100 % (the film's letterbox is their height). The frame EndFallingSpellVideo runs, the original
/// still shows the film with base alpha 0x50 (DrawToScreen kept its colour at 0x54DC6D, drawn by FinishFrame's
/// callback); openblack takes the colour when it draws, 0xFF (under the white fade, then 1 - g_delta_time / 1000 opaque).
namespace openblack::video
{

class VideoPlayer;

/// g_game +0x205A28 (KickOffFallingSpellVideo 0x5539D5 `mov [esi+0x205A28], 2`; 1 is the citadel's, GoInsideCitadel
/// 0x554004; EndFallingSpellVideo 0x553A1C writes 0)
inline constexpr int32_t k_SequenceModeNone = 0;
inline constexpr int32_t k_SequenceModeCitadel = 1;
inline constexpr int32_t k_SequenceModeFallingSpell = 2;
/// Process3dEngine 0x54DDCE `cmp [ecx+0x20], 4`: the state that ends the falling spell
inline constexpr int32_t k_FallingSpellEndState = 4;
/// FallingSpell::Init 0x52621E / update 0x526E4E: a film whose fps is <= 0 is given 0x18
inline constexpr int32_t k_FallingSpellFallbackFps = 0x18;
/// The update 0x526E00: the state +0x20 moves on when the film's ms are past these (`jle`, signed)
inline constexpr int32_t k_FallingSpellStateOneMs = 0x348A;   ///< 13450, 0x527034
inline constexpr int32_t k_FallingSpellStateTwoMs = 0x9376;   ///< 37750, 0x5270B3
inline constexpr int32_t k_FallingSpellStateThreeMs = 0xAB7C; ///< 43900, 0x52713A
/// The update 0x526E00: the sound state +0x24
inline constexpr int32_t k_FallingSpellSoundOneMs = 0x442A;   ///< 17450, 0x526F32
inline constexpr int32_t k_FallingSpellSoundTwoMs = 0x4BFA;   ///< 19450, 0x526F7F
inline constexpr int32_t k_FallingSpellSoundThreeMs = 0x7BA2; ///< 31650, 0x526FE8
/// 0x527152 / 0x527203: [0xE06024] = 0xFFFFFF, the Temple fade is white
inline constexpr uint32_t k_FallingSpellFadeRgb = 0x00FFFFFF;
/// 0x5271AE `push 1`: LHMusicStop(1), every channel fades out (MusicEngine.h)
inline constexpr int32_t k_FallingSpellMusicFade = 1;

/// FallingSpell's film time: frame * 1000 / fps (0x526E61..0x526E71: `lea` x3 = 125, `shl 3`, `cdq; idiv`, 32-bit)
[[nodiscard]] int32_t FallingSpellFilmMs(int32_t frame, int32_t fps) noexcept;

/// A sound of the update 0x526E00, for audio to play or stop. The banks are GGlobal::Global [0xCD3B20] +0x3AC / +0x3B4 /
/// +0x3BC, numbered as audio::SfxBank (BankTables.h); dev\tmp_dis\audio\sfx_inventory.md 0x526F6E..0x5271E1
struct FallingSpellSound
{
	enum class Kind : uint8_t
	{
		/// GAudio::PlaySoundEffect(LH_SamplePlayOptions*) 0x429E30 with the options' ctor values and bank +0x04, is3D
		/// +0x08 = 0, owner +0x20, sample +0x24 (and pitch +0x48 once)
		Play,
		/// LHSampleStop(bank, owner, sample) through GGlobal +0x14 (the import 0x8A97A4, called directly)
		Stop,
	};
	enum class Bank : uint8_t
	{
		InGame = 1,    ///< +0x3AC audio/sfx/game/ingame.sad
		Spells = 3,    ///< +0x3B4 audio/sfx/game/spells.sad
		ScriptSfx = 5, ///< +0x3BC audio/sfx/script/scriptsfx.sad
	};
	/// LH_SamplePlayOptions' ctor 0x10010E90 pitch +0x48 (sample_play::Options, audio's reading of the DLL): 100 percent
	static constexpr int32_t k_DefaultPitch = 100;

	Kind kind;
	Bank bank;
	int32_t sample;
	/// The options +0x20 / LHSampleStop's 2nd argument: 0, 1 or 2 (no game thing: three "owners" so the three loops can
	/// be stopped one by one)
	uint32_t owner;
	int32_t pitch;
	/// The call in the exe
	uint32_t address;

	bool operator==(const FallingSpellSound&) const = default;
};

/// Temple::UpdateFade 0x794280 and its globals: a full screen colour fade at 1.0 a second of g_delta_time, shared by
/// the citadel and the falling spell; its colour goes to the screen fade [0xFA51D8] (fn_0053CE60 0x794361)
struct TempleFade
{
	/// [0xE06020] Temple::Dat_00E06020, the target
	float target {0.0f};
	/// [0xC2A150] Temple::Dat_00C2A150, the current value. The exe's .data has 1.0f, but DoLogo (GGame::Loop's first
	/// pass, 0x54D060, single player) sets current = target = 0, [0xE06024] = 0xFF000000 and [0xE06028] = 0
	/// (0x5FA0E1..0x5FA0FF), and OnNewGame current = target = 0 (0x553984): at rest, as here
	float current {0.0f};
	/// [0xE06024], the colour's RGB (DoLogo's 0xFF000000 is 0 once masked, 0x794355)
	uint32_t rgb {0};
	/// [0xE06028], + 1 each time the fade reaches its target
	int32_t done {0};

	/// Process3dEngine 0x54E2A4..0x54E2DE: Temple::UpdateFade unless target == current and the mode is not the
	/// citadel's (then GScript::ProcessFade(1)); mode 3 (0x54E2AD) runs neither
	[[nodiscard]] bool Runs(int32_t mode) const noexcept;
	/// Temple::UpdateFade 0x794280 with g_delta_time = `deltaMs`: the colour written to [0xFA51D8]. Float arithmetic, as
	/// the x87 at 24 bits (fn_007DEE00)
	uint32_t Update(uint32_t deltaMs) noexcept;
};

/// GGame::FallingSpellVideo 0xCD3B10 (FallingSpell, 0x40 bytes) and g_game +0x205A28
class FallingSpellVideo
{
public:
	struct Hooks
	{
		/// KickOffFallingSpellVideo 0x5539A5..0x5539C0: g_game->players[PlayerIndex +0x205A59].creature != NULL
		/// (players at +0x18, 0xA60 bytes each, GPlayer::creature +0xA4C: `+0xA64 + 0xA60 * index`)
		std::function<bool()> hasCreature;
		/// FallingSpell::Init 0x5261F7: "data\spells\fall\fall.bik" (0xBE9C38) as a path to open
		std::function<std::filesystem::path()> filmPath;
		/// The sounds of the update 0x526E00. Unset: nothing
		std::function<void(const FallingSpellSound& sound)> sound;
		/// 0x5271B0 LHMusicStop(1). Unset: nothing
		std::function<void(int32_t fade)> musicStop;
		/// fn_0053CE60 0x794361: [0xFA51D8] = the Temple fade's colour (ScreenFade::SetColour)
		std::function<void(uint32_t argb)> setScreenFadeColour;
	};
	/// openblack's: the local player's creature (PLAYER_ONE, inferido), FindPath("Data/Spells/fall/fall.bik"), the
	/// sounds through audio::PlaySoundEffect(PlayOptions) / StopSoundEffect (2D, owners None / Key(1) / Key(2)),
	/// audio::MusicStop and Game's ScreenFade
	[[nodiscard]] static Hooks GameHooks();

	FallingSpellVideo(VideoPlayer& player, Hooks hooks);

	/// GGame::KickOffFallingSpellVideo 0x5539A0: nothing without the local player's creature (0x5539C0 `je 0x553A0E`),
	/// else EndFallingSpellVideo (0x5539C4) and Start()
	void KickOff();
	/// 0x5539C9..0x5539F9: +0x205A28 = 2, new(0x40) (Game.cpp line 0x1ADD) and its ctor fn_00527240 (+0x00 = +0x04 = 0),
	/// FallingSpellVideo = it, FallingSpell::Init 0x526060. (openblack) OPENBLACK_TEST_VIDEO=fall calls it directly
	void Start();
	/// GGame::EndFallingSpellVideo 0x553A10: without FallingSpellVideo nothing; else +0x205A28 = 0, FallingSpell::Close
	/// 0x5264A0, delete, FallingSpellVideo = NULL and fn_0054DA00 (the normal skip of the film, VideoPlayer::Skip)
	void End();
	/// Process3dEngine, after the film's service (0x54DAB5..0x54DD76, VideoPlayer::Process): case 2 0x54DD83..0x54DDDB
	/// (the update 0x526E00 through fn_00553A60, then EndFallingSpellVideo without a film or at state 4), then
	/// 0x54E2A4..0x54E2DE Temple::UpdateFade when it runs, `realMs` = g_delta_time (game_clock::FrameRealMs)
	void ProcessFrame(uint32_t realMs);
	/// FallingSpell's update 0x526E00 alone (no symbol; fn_00553A60 jumps to it when FallingSpellVideo != NULL)
	void Update();

	/// GGame::FallingSpellVideo != NULL
	[[nodiscard]] bool IsActive() const { return _active; }
	/// g_game +0x205A28
	[[nodiscard]] int32_t Mode() const { return _mode; }
	/// Process3dEngine 0x54DD83..0x54DE02: in mode 2 the land, the objects, the hand and the interface are not drawn
	/// (case 0, 0x54DE57, is skipped): only the film (and, not ported, the falling creature, its sprites and the
	/// liquid particles)
	[[nodiscard]] bool HidesWorld() const { return _mode == k_SequenceModeFallingSpell; }
	/// FallingSpell +0x20: 0..4
	[[nodiscard]] int32_t State() const { return _state; }
	/// FallingSpell +0x24: 0..3
	[[nodiscard]] int32_t SoundState() const { return _soundState; }
	/// FallingSpell +0x1C: 1 from state 1 (0x527047). (not ported) FallingSpell::Draw rewrites it with whether a sprite
	/// is still seen (0x526DCA)
	[[nodiscard]] bool SparklesOn() const { return _sparklesOn; }
	/// FallingSpell +0x0C: the film ms of the last update (100 after Init, 0x52615F)
	[[nodiscard]] int32_t LastMs() const { return _lastMs; }
	[[nodiscard]] const TempleFade& GetTempleFade() const { return _fade; }
	[[nodiscard]] TempleFade& GetTempleFade() { return _fade; }

private:
	/// FallingSpell::Init 0x526060 (the film's part)
	void Init();
	/// FallingSpell::Close 0x5264A0 (the film's part)
	void Close();
	void Sound(FallingSpellSound::Kind kind, FallingSpellSound::Bank bank, int32_t sample, uint32_t owner,
	           uint32_t address, int32_t pitch = FallingSpellSound::k_DefaultPitch) const;

	VideoPlayer& _player;
	Hooks _hooks;
	bool _active {false};             ///< GGame::FallingSpellVideo 0xCD3B10 != NULL (and FallingSpell +0x00)
	int32_t _mode {k_SequenceModeNone}; ///< g_game +0x205A28
	int32_t _lastMs {0};              ///< +0x0C
	bool _sparklesOn {false};         ///< +0x1C
	int32_t _state {0};               ///< +0x20
	int32_t _soundState {0};          ///< +0x24
	TempleFade _fade;
};

/// The game's one, over video::Get() with GameHooks()
[[nodiscard]] FallingSpellVideo& GetFallingSpell();

} // namespace openblack::video
