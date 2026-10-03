/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The frame animations of textures, one function per clock of the original (wiki: rendering-objects.md, "Texturas
/// animadas por fotogramas"). Pure logic: no rendering state.
///
/// The original never blends two frames. Every user picks one whole frame (__ftol, which truncates, or an integer
/// division) and draws once; what looks smooth comes from many frames at 15 to 25 a second and from the continuous UV
/// scrolls. There is no common clock in the original either: every user keeps its own accumulator (per object, or a
/// global where the original has a global) with its own constants, so each one has its own function here and the
/// caller keeps the state where the original keeps it. The only frame blending is AnimatedSprite::blend, an opt-in for
/// mods that is off by default.
///
/// Two engine primitives draw the frames: the cell of an LH3DSprite (SpriteCell, the UVs are billboard::CellUv) and the
/// UV offset of an LH3DObject (UvOffset, SetAnimatedUV_1 0x7F9B70).
///
/// The clocks are float, as the original's x87 runs them: fn_007DEE00 does fninit then clears the precision bits
/// (0x7DEE05..0x7DEE13, and 0xFCFF: 24-bit significand), and the game calls it from pc_main (0x641C6F through
/// fn_007DEDD0, 0x641D7E), at the start of every GGame::EndTurn (0x54E964, 0x54E974, 0x54E984) and in
/// GGame::Process3dEngine (0x54E426, 0x54E4D1); the CRT's 53-bit default (__setdefaultprecision 0x7CC962) only holds
/// before pc_main. Every fadd, fmul and fsub on the stack is then rounded like a float operation, so the value kept on
/// the stack between a fmod and the __ftol is the float one
namespace openblack::graphics::frame_anim
{

// ---------------------------------------------------------------------------------------------------------------------
// Primitives

/// LH3DSprite +0x28 bits 0-5, the cell (LH3DSprite::Draw 0x840530 reads flags & 0x3F); the users write
/// flags = (flags & ~0x3F) | (cell & 0x3F)
[[nodiscard]] constexpr uint8_t SpriteCell(int cell) noexcept
{
	return static_cast<uint8_t>(static_cast<uint32_t>(cell) & 0x3Fu);
}
/// The four corner UVs of a sprite cell: SpriteCell then billboard::CellUv (0xC390CC / 0xC390DC)
[[nodiscard]] std::array<glm::vec2, 4> SpriteCellUv(int cell, uint8_t cellsPerRow = 8);

/// LH3DObject::SetAnimatedUV_1(u, v), vt +0xE8 0x7F9B70: +0x68 = u, +0x6C = v; (0, 0) clears Flags1 0x400, which
/// nothing reads. Every LH3DObject draw copies it to [0xECA62C] / [0xECA630] ([0xECA628] = 1 when not both 0)
using UvOffset = glm::vec2;
/// [0xECA628] (0x80C42C..0x80C485): the offset is on unless both are 0
[[nodiscard]] constexpr bool IsAnimatedUv(const UvOffset& offset) noexcept
{
	return offset.x != 0.0f || offset.y != 0.0f;
}
/// LH3DRender::DrawTriangle 0x82F8BE..0x82F8F7: every vertex UV += the offset unless the current material has bit 0x10
/// of byte +5 (L3DSubMesh::Primitive::uvOffset clear). fn_0082F920, fn_0082FD70 and fn_00884750 add it without
/// testing the bit (fixedMaterial false)
[[nodiscard]] glm::vec2 OffsetUv(const glm::vec2& uv, const UvOffset& offset, bool fixedMaterial) noexcept;
/// (openblack) the transport of an object's offset to vs_object.sc: v + 4 x round(256 frac(u)) in the w of an instance's
/// second column; the shader unpacks v in -2..2 and u in 1/256 steps. Exact for every original user: the orbs' quarters,
/// the icons' 32/256 steps, the AnimTextured cells of whole pixels; a SlideU over 1000 frames is rounded to 1/256
[[nodiscard]] float PackUvOffset(float u, float v) noexcept;

/// The texture of a ParticleMeshCreatorAnimTextured (CreateParticle 0x6A8DA0: p +0x28 TextureHeight, +0x2C
/// TextureWidth, +0x30 SlideU, +0x31 SlideV, +0x34 the frame count N)
struct AnimTexturedSheet
{
	int width {64};  ///< TextureWidth, 0..128 pixels of a 256 texture
	int height {64}; ///< TextureHeight
	bool slideU {false};
	bool slideV {false};
	int frames {1}; ///< N: NumFrames, or 1000 when sliding
};
/// Particle3DObjAnimTextured::DrawAt 0x67A530: the cells of W x H pixels of a 256 texture, cols = 256 / W (idiv),
/// u = (W / 256) (f % cols), v = (H / 256) (f / cols) (f unsigned); with a slide, u = W f / (N 256) and v = H f / (N 256)
/// ([0x8D45CC] = 256). (openblack guard) cols is at least 1: the original divides by 256 / W unchecked
[[nodiscard]] UvOffset AnimTexturedCell(int frame, const AnimTexturedSheet& sheet) noexcept;

// ---------------------------------------------------------------------------------------------------------------------
// The clocks of the original. The state is the caller's, kept where the original keeps it.

/// The integer clocks take g_game_time_inc [0xEA9EC0] as it is: game_clock::FrameGameMs() (whole ms, the remainder of
/// the turn kept by the game clock itself, GGame::Loop 0x54D2B2..0x54D3A6).

/// OneOffSpellSeed::UpdateFrame 0x72A570 (the miracle bubble, O_Bibble_up.l3d 4 x 4): +0x74 = fmod(+0x74 + ms x 18
/// ([0x981FB4]) x 0.001, 16 (double [0x982820])), f = ftol, SetAnimatedUV_1((f % 4) x 0.25, (f / 4) x 0.25 [0x981FB8])
[[nodiscard]] UvOffset OneOffFrame(float& phase, float milliseconds) noexcept;

/// SpellSeedGraphic::DrawSpellGraphic 0x519AD0, the creature spell phials' branch (0x519B79..0x519C1B): +0x34 =
/// fmod(+0x34 + (-15) ([0x8D86F0]) dt, 32 (double [0x8D8740])), + 32 ([0x8CF134]) when negative, f = ftol, u = (f % 8)
/// (1/256) 32, v = (f / 8) (1/256) 32 ([0x8D86CC] x [0x8CF134]): 32 frames of an 8 x 4 sheet, 15 a second backwards.
/// dt = g_game_time_inc x 0.001. When the fmod is a tiny negative the + 32 rounds to 32 itself (24-bit precision, see
/// the top of this file): f = 32, (0, 0.5)
[[nodiscard]] UvOffset SpellIconFrame(float& phase, float seconds) noexcept;

/// PHandFX::Draw 0x68D0C0 (0x68D29B..0x68D374): +0x58 += dt x rate (+0x5C = -20); with rate > 0, fmod(.., 2 N) once
/// above 2 N; with rate <= 0, fmod(.., 2 N) + 2 N once below 0 (N = the byte +0x60 = 32). f = fistp(+0x58) % N: fistp
/// ROUNDS to the nearest (the FPU default), not __ftol; u = (f % 8) x 0.125, v = (f / 8) x 0.125 ([0x8AB620]), an 8 x 4
/// sheet. The original writes it into the hand mesh's vertices, not through SetAnimatedUV_1
[[nodiscard]] UvOffset HandFlowFrame(float& phase, float seconds, float rate = -20.0f, int frames = 32) noexcept;

/// AtomCore's frame step, fn_00673EA0 (0x673FB8..0x6740D8): previous (+0x108) = current (+0x10C); then only when
/// [0xC029DC] (1) and PlayAnim (+0x118) == 1 (else 0x673FC6..0x673FDE jump to 0x67406A: no step and no wrap),
/// current = dt [0xD4E0EC] x rate (+0x110) + previous, and with rate > 0, while both are above 2 N they go down by N;
/// with rate <= 0, while either is below 0 both go up by 2 N (N = +0x114)
void PSysFrameAdvance(float& previous, float& current, float dt, float rate, int frames, bool play) noexcept;
/// fn_00679920 (0x679A7D..0x679B03): the frame between two steps, t' = clamp(t, 0, 5) ([0x9357B8]) when looped
/// (+0x119 LoopAnim), else clamp(t, 0, 1): previous + (current - previous) t'
[[nodiscard]] float PSysFrameLerp(float previous, float current, float t, bool loop) noexcept;
/// fn_00679920 (0x679AC2..0x679B61): the whole frame drawn (DrawData +0x10). Looped: ftol(fmod(f, N)), + N first when the
/// fmod is negative; otherwise ftol(f) clamped to 0..N - 1
[[nodiscard]] int PSysFrameIndex(float frame, int frames, bool loop) noexcept;

/// The counter of LH3DMist +0x84 (fn_007FA300) and of each LH3DSmoke puff (fn_007F8E00): cell = (c x 45 / 900) & 15 in
/// integers (0x7FA3F4..0x7FA41B, x 0x91A2B3C5 sar 9 = / 900). c = 900 is kept (the wrap is c > 900), so c / 20 reaches
/// 45: the cells run 0..15, 0..15, 0..13 (14 at c = 900) and back to 0; cells 14 and 15 are skipped once every 900
[[nodiscard]] constexpr int MistCell(int counter) noexcept
{
	return (counter * 45 / 900) & 15;
}
/// fn_007FA300: SetAnimatedUV_1((f & 7) x 0.125, (f >> 3 & 7) x 0.125 + 0.25) in the effect branch (+0x80 & 2, the sky
/// clouds and the effect mists: rows 2-3 of smoke.raw, 0x7FA44D [0x8AB3D4], 0x7FA466), without the 0.25 in the other
/// branch (the map mists: rows 0-1, 0x7FA69E)
[[nodiscard]] UvOffset MistCellUv(int cell, bool effect) noexcept;
/// The mist clock, fn_007FA300 (0x7FA3B6..0x7FA3F0): only run by the Draw of a mist the Z-sorter got (AddDrawing
/// 0x7FA7F0 sends only the ones whose sphere touches the screen)
struct MistClock
{
	int counter {0};        ///< +0x84
	float remainder {0.0f}; ///< (openblack) the fraction of ms x 0.255 kept between frames, see Advance
};
/// The original: +0x84 += ftol(g_game_time_inc x 0.255 ([0x9A2BA8])); if > 900 (0x384), %= 900
void MistAdvanceExact(MistClock& clock, uint32_t gameTimeIncMs) noexcept;
/// (openblack) the same with the fraction of ms x 0.255 kept, as every mist, cloud and smoke of the port does: the
/// original's ftol of each frame would stop the animation under 4 ms a frame (openblack runs uncapped)
void MistAdvance(MistClock& clock, float milliseconds) noexcept;
/// LH3DMist ctor 0x7F9560 (0x7F95DC..0x7F95FB): +0x84 = ftol(Random(0, 16)) & 15 (Random 0x81D180): cell 0
[[nodiscard]] constexpr int MistStartCounter(float random0To16) noexcept
{
	return static_cast<int>(random0To16) & 15;
}
/// LH3DSmoke fn_007F8E00: dt = min(g_game_time_inc x 0.001, 100) ([0x8AB41C], 0x7F8F45); every puff's age +=
/// ftol(dt x 255), back past 900; its cell is MistCell(age). (openblack) the fraction of dt x 255 is kept in remainder
/// and the whole step is the same for the puffs of one smoke, as their dt is
[[nodiscard]] int SmokeAgeStep(float& remainder, float milliseconds) noexcept;

/// Fire flames, fn_007321B0: cell = ftol(fmod(-25 ([0x999668]) age, 32) + 32 ([0x8D8740] / [0x8CF134])), the age of
/// SpritePos +0x2C (pushed at 0x7323AC), written to the sprite at 0x7323B7..0x7323CA. The + 32 is unconditional: at
/// age 0, -25 x 0 = -0, fmod gives -0 and the cell is 32 (a flame drawn before any time went by)
[[nodiscard]] int FireCell(float age) noexcept;
/// SteamGetOffsetFromAge 0x7321E0: cell = ftol(fmod(25 ([0x99966C]) age, 32)), the steam of fn_007323F0 (0x7324B2) and
/// the grey smoke of fn_0073250A (0x7325A2)
[[nodiscard]] int SteamCell(float age) noexcept;

/// The fish of a fish farm, fn_008248E0 (0x824960..0x8249CE): dt = min(g_game_time_inc x 0.001, 0.1) ([0x8AB22C]),
/// +0x1C += dt x speed (+0x14) x 25 ([0x8C7BD0]); the cell (8 + (ftol(+0x1C) & 15)) & 0x3F goes to the sprite BEFORE
/// the wrap +0x1C -= 15 ftol(+0x1C x (1/15)) ([0x8C9D38], [0x8C2C40]), so a frame of 15.x shows cell 23. @return the cell
[[nodiscard]] uint8_t FishFrame(float& frame, float seconds, float speed) noexcept;
/// fn_008248E0's dt: min(dt, 0.1) (0x824960..0x824971, 0x3DCCCCCD)
[[nodiscard]] float FishDt(float seconds) noexcept;

/// The street lanterns and bonfires, fn_00823570 (0x823599..0x82362D): one GLOBAL clock [0xEB99C4] += g_game_time_inc,
/// if > 700 ([0xC383C8]) %= 700, a = c x 31 / 700 (integers); run only while the village light alpha [0xEB99BC] is not 0
/// and the list [0xEB99B8] is not empty (0x82357A..0x823593). @return a
[[nodiscard]] int LanternAdvance(int& clockMs, uint32_t gameTimeIncMs) noexcept;
/// The GLOBAL start table 0xC383BC of the light's three sprites, one entry each: {0, 13, 0} in the file, and every new
/// light (fn_00823240, 0x8233E4..0x8233F8) writes ftol(Random(0, 31)) into all three, so every light uses the values of
/// the last one made (all in phase). LanternStart is that value
using LanternStarts = std::array<int, 3>;
inline constexpr LanternStarts k_LanternFileStarts = {0, 13, 0}; ///< 0xC383BC, 0xC383C0, 0xC383C4
/// fn_00823240 0x8233E4..0x8233F8: ftol(Random(0, 31)) ([0x41F80000] = 31), random a Random 0x81D180 value in [0, 31]
[[nodiscard]] constexpr int LanternStart(float random0To31) noexcept
{
	return static_cast<int>(random0To31);
}
/// fn_00823570 0x8235E8..0x823632: flame i (0, 1) of a light, cell = (10 i + 31 - ((start[i] + a) & 31)) & 31, start
/// from the global table; the third sprite (the glow) only takes the colour
[[nodiscard]] uint8_t LanternCell(int a, int flame, const LanternStarts& starts) noexcept;

/// The citadel's leashes, fn_00466730 (from CitadelHeart::DrawNow 0x46733D), 0x46690A..0x466955: +0x74 += 10 ([0x8C8404])
/// dt; +0x74 -= 15 ([0x8C2C40]) ftol(+0x74 x (1/15) [0x8C9D38]); cell = ftol(+0x74) & 0x3F (after the wrap): 15 cells
/// at 10 a second
[[nodiscard]] uint8_t LeashCell(float& phase, float seconds) noexcept;
/// fn_00466730 0x466855..0x46687B: +0x60 += 0.5 ([0x8C8400]) dt, -= ftol(+0x60); SetAnimatedUV_1(+0x60, the colour row:
/// n x 0.125 [0x8AB620], or 0.375 (0x3EC00000)) at 0x4668A8
[[nodiscard]] float LeashScroll(float& u, float seconds) noexcept;

/// RenderParticleGoldenShower, fn_006CA990 (0x6CAB1F..0x6CAB5B): cell = ((t / 50) + base + byte[drop + 8]) % 32, signed C
/// division and remainder (x 0x51EB851F sar 4), & 0x3F. (inferido) t in milliseconds
[[nodiscard]] uint8_t GoldenShowerCell(int32_t t, int32_t base, uint8_t dropOffset) noexcept;
/// CreatureRoom::DrawAdditional 0x788630 (0x7889A5..0x7889CC): cell = 31 - (((GetTickCount() >> 5) + i) & 31): the real
/// clock, 31.25 a second backwards, each sprite i one step on
[[nodiscard]] uint8_t CreatureRoomCell(uint32_t tickCount, int sprite) noexcept;
/// The 3D cursor, CameraModeNew3 0x456BED..0x456D55: cell = (GetTickCount() / 50) & 15 (x 0x51EB851F sar 4): the real
/// clock, 20 a second, 16 cells, 8 a row (+0x30)
[[nodiscard]] uint8_t CursorCell(uint32_t tickCount) noexcept;
/// HelpSystem fn_005C0700 (0x5C0A7A..0x5C0AAF): [0xD15AB0] += fn_005557E0() (g_delta_time capped to 500 when
/// g_game+0x205A28 is 1, else g_game_time_inc), cell = ([0xD15AB0] / 200) & 15 (x 0x51EB851F sar 6): 5 a second
[[nodiscard]] uint8_t HelpSystemCell(int32_t& clockMs, int32_t stepMs) noexcept;
/// JCSpecial, fn_00828A70 (0x828CDE..0x828D7A), each of its 3 sprites: f += ms x 0.01 ([0x8C4B10]); f > 15 restarts it
/// at 0 (not a fmod); cell = ftol(f) & 15
[[nodiscard]] uint8_t JCSpecialCell(float& frame, float milliseconds) noexcept;
/// PlayerSymbolSprite::Draw 0x69D7E0: layer 0, A (+0xC) -= ms x 0.02 ([0x937538], 0x69D7E0..0x69D81B); layer 1, B
/// (+0x10) -= ms x 0.023 ([0x937534], 0x69D81D..0x69D853); each + 32 ([0x8CF134]) while below 0; the cells ftol(A) & 0x3F
/// (0x69D896..0x69D8AB) and ftol(B) & 0x3F (0x69D8CD..0x69D8E2) of the two glows of one symbol (two draws, not a blend).
/// The ctor fn_0069D5A0 starts both at 0. ms = g_game_time_inc
[[nodiscard]] uint8_t PlayerSymbolCell(float& phase, float milliseconds, int layer) noexcept;
/// PlayerSymbolSprite::Draw 0x69D855..0x69D88B: the second glow's angle +0x14 += ms x 0.002 ([0x92A544]), - 2 pi
/// ([0x8AB210]) while above 2 pi; written to its sprite's +0x14 at 0x69D8C5. The ctor starts it at 0. @return the angle
[[nodiscard]] float PlayerSymbolSpin(float& angle, float milliseconds) noexcept;
/// SmokyStuff, fn_00823F70 (from fn_00824140), 0x8240F9..0x824115: cell = ftol(+0xBC x 15) & 0x3F (+0xBC the life,
/// 1 down to 0)
[[nodiscard]] uint8_t SmokyStuffCell(float life) noexcept;
/// The collision dust of fn_00846010 (ECS/Physics/Dust.cpp): cell = 16 + ((rand % 16 + ftol(2 age)) & 15), rows 2-3 of
/// blobs.raw
[[nodiscard]] uint8_t DustCell(uint32_t seed, float age) noexcept;

/// InfluenceCircle::Draw 0x826D2D..0x826D83, the border's scroll: one GLOBAL clock [0xEB9A40] (int ms) = (c +
/// g_game_time_inc) % 10000 (signed idiv), then [0xECA628] = 1 and the offset u = float(c) x 0.0001 ([0x9A391C]),
/// v = float(-c) x 0.0002 ([0x9000DC]): u runs 0 -> 1 and v 0 -> -2 every 10 s, whole repeats of a tiled texture, so it
/// is seamless. Run only when the draw passes its camera gate (influence::CurtainAlpha); [0xECA628] = 0 after the
/// circles (0x826F8B). @return the offset
[[nodiscard]] UvOffset InfluenceScroll(int32_t& clockMs, uint32_t gameTimeIncMs) noexcept;

/// DesignedWaterFall 0x5E3770 (0x5E392E..0x5E3972): V -= 0.5 ([0x8AA3B4]) dt, minus its whole part (ftol), so it stays
/// in -1..0; SetAnimatedUV_1(0, V). @return V
[[nodiscard]] float WaterfallScroll(float& v, float seconds) noexcept;

/// GoolooGooloo 0x5E6540 -> fn_005E6390, the ghost of an object taken away: t goes from 500 ([0xBF3588]) ms to 0; x = t /
/// 500, SetAnimatedUV_1(2 cos x, 1.7 sin(0.7 x)) (doubles [0x92B338] = 1.7000000476837158, the float 1.7 widened, and
/// [0x900AE0] = 0.7), drawn, then (0, 0)
/// and drawn again with mode 10; the material's byte +4 [0xEA1AB4] = 255 - ftol(255 t / 500) ((inferido) its ALPHAREF)
struct Gooloo
{
	UvOffset uv;
	uint8_t materialByte;
};
[[nodiscard]] Gooloo GoolooFrame(float t) noexcept;

/// RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 (only when [0xC029B8], 1): the offset between two steps, lerp(+0x24
/// -> +0x2C, t) and lerp(+0x28 -> +0x30, t) with t = DrawData +0x14, then the period (+0x3C, +0x40) taken off while
/// above it (nothing added below 0), written to [0xECA62C] / [0xECA630] with the tiling forced. The only offset
/// interpolated between turns: the UV, not a frame
[[nodiscard]] UvOffset RotatingUv(const UvOffset& previous, const UvOffset& current, float t, const glm::vec2& period) noexcept;

/// The whole clock of a RenderParticleGJMeshRotatingUV (the ZR_SurfRevol atom's draw object, ctor 0x6C8A90): the rule
/// adds its step to `destination` (+0x34 / +0x38) and GameUpdate 0x6C8BC0 moves it down the chain once a step, so a
/// draw between two steps interpolates. `previous` and `current` are +0x24 / +0x28 and +0x2C / +0x30, `period` the
/// tiling +0x3C / +0x40 (TextureWidth / 256, TextureHeight / 256).
struct RotatingUvClock
{
	UvOffset previous {0.0f, 0.0f};
	UvOffset current {0.0f, 0.0f};
	UvOffset destination {0.0f, 0.0f};
	glm::vec2 period {1.0f, 1.0f};

	/// GameUpdate 0x6C8BC0, called once a step per atom at the end of PostUpdateAtoms fn_00673EA0 (0x674080, vt+0x108):
	/// per axis, while both `destination` and `current` are under -2 period it adds the period to both (0x6C8BDF..
	/// 0x6C8C5A) and while both are over +2 period it takes it off both (0x6C8C5B..0x6C8CCC) - the pair moves together,
	/// so their difference, which is what DrawAt interpolates, never changes. Then previous = current and current =
	/// destination (0x6C8CCD..0x6C8CE2).
	void GameUpdate() noexcept;
	/// RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 with t = DrawData +0x14 (the draw fraction of the step that
	/// PSysManager::AddDrawing 0x6797D4 keeps at its +0xB0)
	[[nodiscard]] UvOffset Interpolated(float t) const noexcept { return RotatingUv(previous, current, t, period); }
};

/// The chains' v-scroll, fn_0067B3F0 (0x67BE88..0x67BED5, only when [0xC029B8]): chain +0x3C += g_game_time_inc x rate
/// (+0x4C) x 0.001 ([0x8AA3B0]), fmod (FrameHeight +0x1C x (1/256) [0x9357A8]), + that when negative. The rate is
/// set only by UR_SimpleBeam (SpeedV +0x48, 0x6762EE) and UR_Plasma::CreateArc (its AtomData +0x54, not read, x SpeedV
/// +0x58, 0x676898), neither ported; 0 for every other chain (the ctor 0x6C8830 sets +0x3C = +0x4C = 0). @return +0x3C
[[nodiscard]] float ChainScroll(float& scroll, float milliseconds, float rate, int frameHeight) noexcept;
/// One segment of a chain's ribbon, fn_006C8920: the chain +0x1C FrameHeight, +0x20 FrameWidth, +0x24 FrameOfHead,
/// +0x28 FrameOfTail, +0x30 the textures over the whole chain (NumTexturesForWholeChain, or joints - 1 for -1, CreateChain
/// 0x6AA8DC), +0x34 FileOffset (copied by CreateChain 0x6AA8B8..0x6AA8EB)
struct ChainSheet
{
	int frameWidth {32};  ///< ctor 0x6AA740: 0x20
	int frameHeight {64}; ///< ctor 0x6AA739: 0x40
	int frameOfHead {0};
	int frameOfTail {0};
	int fileOffset {0};
	int textures {1}; ///< T: the original divides by it unchecked (idiv 0x6C893E), see ChainSegmentUv
};
/// fn_006C8920 (segment i of S = joints - 1): k = ((i + 1) T - 1) / S, its first segment b = k S / T, its count
/// n = (k + 1) S / T - b, j = i - b (integer divisions); F = FileOffset + (k == T - 1 ? FrameOfHead : k == 0 ?
/// FrameOfTail : 0); uv0 = (F W, H j / n) / 256, uv1 = ((F + 1) W, H j / n) / 256, uv2 = (F W, H (j + 1) / n) / 256,
/// uv3 = ((F + 1) W, H (j + 1) / n) / 256 ([0x938EBC]), then the scroll +0x3C added to the four v. Along the chain
/// the V runs; across it the U, over one W-pixel column of the sheet. (openblack guard) S, T and n are at least 1: the
/// original divides by S (idiv 0x6C8936) and T (idiv 0x6C893E, 0x6C8949) unchecked, so T = 0 (DefineProperties allows
/// -1..32, and -1 is replaced by CreateChain) would fault there, and by n in float (0x6C896B, 0x6C8971)
[[nodiscard]] std::array<glm::vec2, 4> ChainSegmentUv(int segment, int segments, const ChainSheet& sheet, float scroll) noexcept;

// ---------------------------------------------------------------------------------------------------------------------
// Loaders

/// A GJBitmap of GJBitmap::LoadBitmapFromFile 0x57CA90: `frames` frames of pitch x pitch texels of `channels` bytes,
/// one after the other, rows along the first axis (the land stamps' x, fn_0086D060 0x86D1EC)
struct StackedFrames
{
	int pitch {0};
	int frames {0};
	int channels {0}; ///< the bpp asked for: 3 (RGB, the light maps) or 1 (grey, the shadow maps)
	std::vector<uint8_t> data;
};
/// GJBitmap::LoadBitmapFromFile 0x57CA90 (name, pitch, bpp, framesInFile, framesInUse) on the file's bytes: nothing
/// unless the file is exactly bpp x pitch^2 x framesInFile bytes (0x57CAC7..0x57CAD4); min(framesInUse, framesInFile)
/// frames (0x57CADB..0x57CADF), which fn_0057CB40 (0x57CB40..0x57CC3F) takes out of the file's grid of
/// ftol(sqrt(framesInFile)) frames per row (frame f at column f % n, row f / n) into pitch x pitch frames one after
/// the other. Callers: ParticleLightMapCreator::GetBitmap 0x6A9D40 (bpp 3), ParticleMistCreator::GetBitmap 0x6AA540
/// (bpp 1 or 3), fn_007311A0 0x7312B6 (S_LMFireBall); the file reading is land_light::LoadBitmapFile's
[[nodiscard]] std::optional<StackedFrames> LoadBitmapFromFile(std::span<const uint8_t> bytes, int pitch, int bpp,
                                                             int framesInFile, int framesInUse);
/// The texels of one frame (fn_006CA280 0x6CA2D0..0x6CA30E): frame % frames (unsigned word), bpp x that x pitch^2
/// bytes in; null for a bitmap without data (0x6CA2E6)
[[nodiscard]] const uint8_t* FrameTexels(const StackedFrames& bitmap, int frame) noexcept;

/// (mod) an animated GIF: its frames as RGBA images of the same size, one after the other, and their delays
struct GifFrames
{
	int width {0};
	int height {0};
	int frames {0};
	std::vector<uint8_t> rgba;    ///< frames x height x width x 4
	std::vector<int> delaysMs;    ///< as stored in the file (centiseconds x 10)
	[[nodiscard]] const uint8_t* Frame(int frame) const noexcept;
};
/// (mod) stbi_load_gif_from_memory; nothing when the bytes are not a GIF with at least one frame
[[nodiscard]] std::optional<GifFrames> LoadGif(std::span<const uint8_t> bytes);
/// (mod) the delay a browser shows: under 20 ms (or none) is 100 ms
[[nodiscard]] constexpr int GifDelayMs(int storedMs) noexcept
{
	return storedMs >= 20 ? storedMs : 100;
}

// ---------------------------------------------------------------------------------------------------------------------
// Mods

/// (mod) a frame clock from per-frame durations (a GIF's delays, or a .cfg): looped, whole frames
struct DelayClock
{
	std::vector<float> ends; ///< seconds at the end of each frame

	[[nodiscard]] static DelayClock FromDelays(std::span<const int> storedDelaysMs);
	[[nodiscard]] static DelayClock FixedRate(int frames, float framesPerSecond);
	[[nodiscard]] float Length() const noexcept { return ends.empty() ? 0.0f : ends.back(); }
	struct Sample
	{
		size_t frame {0};
		size_t next {0};       ///< the frame after it (looped)
		float fraction {0.0f}; ///< how far into this frame, 0..1
	};
	/// The frame shown at `seconds` (looped over the length)
	[[nodiscard]] Sample At(float seconds) const noexcept;
};

/// (mod) an animated sprite: consecutive cells (or texture layers) from `first`, timed by its clock
struct AnimatedSprite
{
	uint16_t first {0};
	uint8_t cellsPerRow {8};
	DelayClock clock;
	/// (mod, opt-in) blend each frame into the next by its fraction. Off by default: the original never blends
	bool blend {false};

	struct Frame
	{
		uint16_t cell {0};
		uint16_t nextCell {0};
		float weight {0.0f}; ///< of nextCell: always 0 unless blend
	};
	[[nodiscard]] Frame At(float seconds) const noexcept;
};

} // namespace openblack::graphics::frame_anim
