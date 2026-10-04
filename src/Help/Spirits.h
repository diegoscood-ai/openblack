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
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Help/CAnim.h"

// The two advisor spirits as pure logic (runblack.exe W120; dev\documentacion\intro\spec_spirits.md phases P2 + P3 and
// spec_spirits_motion.md, which corrects the first in its section 0): HelpDudeControl (HelpSystem+0x10, 0x5C2A40..0x5C3D82),
// the two HelpSpirit target holders (HelpSystem+0xC good, +8 evil, 0x5C4BD0..0x5C52E4) and HelpDude's motion and anim
// stack (0x5B9620..0x5C0700). Nothing here draws, plays a sound or reads the Locator: the camera, the voice and the
// random streams come in through Queries, and each frame gives, per dude, the hover position, the model matrix, the
// alpha, the in-world blend and the list of clips the renderer turns into a pose with CAnim.h (SetPose /
// ApplyAdditive).
//
// Conventions: "hover space" is hx = (px - W/2) / (W/2), hy = (py - H/2) / (W/2) (both axes over the half width,
// 0x5BBBD0). W / H are [0xE85058] / [0xE8505A] (unsigned 16 bits; W/2 and H/2 by shr). Dude 0 is the good spirit
// (MarkGood.Hd), dude 1 the evil one (fn_005C5250: help spirit type 1 -> 0, any other -> 1). Marks: (inferred) deduced
// from use, (approximate) not checked exactly, (pending) not read.

namespace openblack::help
{
struct HelpDudeFile;
}

namespace openblack::help::spirits
{

constexpr int k_Dudes = 2;
constexpr int k_GoodDude = 0;
constexpr int k_EvilDude = 1;
constexpr size_t k_AnimSlots = 80; ///< 0x50 (the anim name table 0xBF029C, the slot loop 0x5BD079)
constexpr size_t k_Zones = 6;      ///< HoverZone[6] at +0x3380 (HelpDude::Feel 0x5B9B6C)

/// The anim indices HelpDude uses by number (the name table built at 0x5BAD10; spec_spirits.md section 2)
namespace anim
{
constexpr uint32_t Stand = 0;
constexpr uint32_t Hover = 1;
constexpr uint32_t HoverLeft = 2;
constexpr uint32_t HoverRight = 3;
constexpr uint32_t Wingflap = 4;
constexpr uint32_t LeftEyeShut = 5;
constexpr uint32_t RightEyeShut = 6;
constexpr uint32_t VowelE = 7;
constexpr uint32_t FirstEmotion = 10; ///< 10..17 Normal..Furious (0x5BD1A5: clips + 0x28)
constexpr uint32_t LookLR = 18;
constexpr uint32_t LookUD = 19;
constexpr uint32_t PointLIn = 27;
constexpr uint32_t PointL = 28;
constexpr uint32_t PointRIn = 29;
constexpr uint32_t PointR = 30;
constexpr uint32_t HoverStable = 31;
constexpr uint32_t AvoidL = 32; ///< 32..35 L / R / U / D (0x5BA4CB, 0x5BED01)
constexpr uint32_t PointAtCamera = 42;
constexpr uint32_t GoInvisible = 46;
constexpr uint32_t ClingL = 65; ///< 65..68 L / R / U / D (0x5BEECB table 0x5BF5BC)
constexpr uint32_t ClingR = 66;
constexpr uint32_t ClingU = 67;
constexpr uint32_t ClingD = 68;
constexpr uint32_t GimmeFive = 69;
constexpr uint32_t LookLRStable = 70;
constexpr uint32_t LookUDStable = 71;
constexpr uint32_t Last = 0x50; ///< PLAY_SPIRIT_ANIM accepts 0..0x50 (spec_spirits.md section 1.4)
} // namespace anim

/// HelpDude +0x3490 / +0x3494 (SetState 0x5BD4A0, tables 0x5BD9A0 / 0x5BD9D0, Update1's switch 0x5BEB0A)
namespace dude_state
{
constexpr uint32_t Hover = 4;
constexpr uint32_t Point = 8;         ///< request; resolved to an intro 0x19 / 0x1A
constexpr uint32_t PointHoldL = 9;    ///< &1 left arm (Point L 28)
constexpr uint32_t PointHoldR = 0xA;  ///< &2 right arm (Point R 30)
constexpr uint32_t PointIntroL = 0x19;
constexpr uint32_t PointIntroR = 0x1A;
constexpr uint32_t PointOutroL = 0x29;
constexpr uint32_t PointOutroR = 0x2A;
constexpr uint32_t Avoid = 0x40;      ///< spec_spirits_motion section 0 (0x5BD6F7, 0x5BECE7)
constexpr uint32_t FlyToAnim = 0x80;
constexpr uint32_t Cling = 0x100;
constexpr uint32_t ClingArrive = 0x110;
constexpr uint32_t ClingLeave = 0x120;
constexpr uint32_t ScriptedAnim = 0x200;
} // namespace dude_state

/// HelpDudeControl +0xC / +0x10
enum class ControlState : int32_t
{
	Home = 0,     ///< hidden at the home point
	GoingHome = 1,
	Out = 2,
	Clinging = 3,
};

/// fn_005BBD20's edges (+0x34F4)
enum class Edge : int32_t
{
	Bottom = 0,
	Left = 1,
	Top = 2,
	Right = 3,
};

/// fn_005BD250: (1 - cos(pi t)) / 2, 0 below 0 and 1 above 1
[[nodiscard]] float Smooth(float t);

/// The screen [0xE85058] / [0xE8505A]
struct Screen
{
	uint16_t width {640};
	uint16_t height {480};
	[[nodiscard]] int32_t HalfWidth() const { return width >> 1; }
	[[nodiscard]] int32_t HalfHeight() const { return height >> 1; }
};

/// LH3DTech::ProjectPoint 0x819390's outputs
struct ProjectedPoint
{
	int32_t x {0};
	int32_t y {0};
	float depth {0.0f};
};

/// What a script's game thing gives the per-turn refresh (0x5C50C0 / 0x5C5170)
struct ObjectInfo
{
	glm::vec3 position {0.0f}; ///< (+0x14 x / 6553.6, GetAltitude + +0x1C, +0x18 / 6553.6)
	float height {0.0f};       ///< vt+0x42C Object::GetHeight (added for a point, not for a look)
};

/// One audio tag (fn_0042A6D0's 20-byte record): +0 time, +4 who, +8 action, +0xC index, +0x10 value
struct AudioTag
{
	float time {0.0f};
	int32_t who {0};    ///< 0 the speaker (T), 1 the other (O, !), 2 the good one (G), 3 the evil one (E), 4 both (B, *)
	int32_t action {0}; ///< 1 A play, 2 AS loop, 3 AR / R release, 4 E emotion, 5 L / 6 LS look mode, 7 LR restore
	int32_t index {0};  ///< anim (prefix match), emotion or look mode
	int32_t value {100};
};
/// AudioTag::BuildAudioTags 0x42AE70's per-label loop (0x42B1B4..0x42B237): fn_0042A6D0 called until the returned
/// pointer reaches the NUL, "[" then "<talker><type> <name>[digits]" entries; each tag gets the cue's time. `errors`
/// counts the "unrecognised ..." cases ([0xC5836C]).
[[nodiscard]] std::vector<AudioTag> ParseAudioTags(std::string_view label, float time, int* errors = nullptr);

/// The per-dude data HelpDude takes from its .hd (HelpDude::Load 0x5C2194, Init's x1.2 at 0x5C2C9B)
struct DudeData
{
	std::array<const CAnim*, k_AnimSlots> clips {}; ///< +0x24 (nullptr = empty slot)
	std::array<float, k_AnimSlots> loopStart {};    ///< +0x333C[i] +8
	std::array<float, k_AnimSlots> loopEnd {};      ///< +0x333C[i] +0xC
	std::array<uint8_t, k_AnimSlots> flags {};      ///< +0x2E7C
	std::array<std::array<float, 16>, 8> faces {};  ///< +0x2C38: 8 emotion records of 16 floats (0x40 bytes each)
	uint32_t startEmotion {0};                      ///< +0x2C28 (u32, spec_spirits_motion section 0)
	float modelSize {0.0f};                         ///< +0x10 (63.423 / 79.154)
	float nearDepth {15.0f};                        ///< +0x35B8 (init 0x41700000 at 0x5C1B17, the file 8.7333)
	float farDepth {10.0f};                         ///< +0x35BC x 1.2 (init 0x41200000 at 0x5C1AFB)
	float scale {5.0f};                             ///< +0x35B4 S, the model scale (init 0x40A00000 at 0x5C1B21)
	float pitchOffset {0.0f};                       ///< +0x35C0
	float fingerR0 {0.12f};                         ///< +0x35C4 (init 0x3DF5C28F at 0x5C1AEB)
	float fingerR2 {0.0f};                          ///< +0x35C8

	/// From a loaded file: the clips point into `file` (keep it alive), the far depth x 1.2 (0x5C2C9B)
	static DudeData FromFile(const HelpDudeFile& file);
};

/// One entry of the per-frame anim list, in the original's order
struct AnimLayer
{
	enum class Kind : uint8_t
	{
		Set,      ///< fn_00860E00(clip, ms) with the stand clip's key 0 as fill (fn_005BB8B0, fn_005BC7D0)
		SetBlend, ///< Set(clip, ms) and Set(clipB, msB) lerped per float by `blend` in absolute bone space (fn_005BC7D0)
		Add,      ///< fn_00861EE0(clip, ms, frames[referenceKey]) (ApplyAnim 0x5BB980 and the direct calls)
	};
	Kind kind {Kind::Add};
	uint32_t clip {0};
	int32_t milliseconds {0};
	uint32_t referenceKey {0};
	uint32_t clipB {0};
	int32_t millisecondsB {0};
	float blend {0.0f};
};

/// One particle of the puff, HelpDude +0x2C10 (16 x 0x20 bytes, made by the first draw of a puff, 0x5C0B24..0x5C0C06)
struct PuffParticle
{
	glm::vec3 velocity {0.0f}; ///< +0 / +4 / +8: Random(-0.8, 0.8), Random(-1.9, 1.5), Random(-1, 1) (+8 unused)
	float sizeBase {0.0f};     ///< +0xC Random(4, 8)
	float age {0.0f};          ///< +0x10, in the puff's time units (ms x 0.0032)
	float spin {0.0f};         ///< +0x14 Random(-2, 2)
	float k {0.0f};            ///< +0x18 (1 - vy) + 1
	uint32_t grey {0};         ///< +0x1C (g, g, g), g = ftol(Random(116, 250)), the first draw of the six
	/// This frame's draw (fn_005C0700 0x5C0C74..0x5C0E6C): ftol(clamp(ftol(255 - 32 age) fade, 0, 255)) with the age
	/// before its step; <= 0 is not drawn (0x5C0CDA)
	int32_t drawAlpha {0};
	/// vy before this frame's drift (0x5C0E38..0x5C0E52 comes after the sprite's position, 0x5C0DEE)
	float drawVelocityY {0.0f};
};
constexpr size_t k_PuffParticles = 16; ///< 0x200 bytes (new at 0x5C0B48), the loops 0x5C1DD3 / 0x5C0E7C

/// HelpDude::PlaySoundFX 0x5C2800(anim, phase, InGame bank): the call, not its effect ((pending) the crossing rule)
struct AnimSound
{
	uint32_t anim {0};
	float phase {0.0f};
};

/// The inputs of one frame
struct FrameInput
{
	float dt {0.0f};               ///< g_delta_time * 0.001 (Draw3D 0x5C59A0 -> Process 0x5C3A30)
	int32_t frameMs {0};           ///< fn_005557E0 (alpha fade, puff): g_delta_time (<= 500) / g_game_time_inc
	Screen screen {};
	glm::ivec2 mouse {0};          ///< [0xE852C0] / [0xE852C4]
	uint32_t tickMs {0};           ///< GetTickCount (the GoInvisible flicker 0x5C0827)
	bool wideScreen {false};       ///< HelpSystem+0x45E8 (Feel 0x5B9ADE, the mouse zone 0x5BDCF8)
};

/// What ApplyLipSync 0x5BCD00 left for the mouth (HelpDude +0x2F60 / +0x2F70)
struct LipSyncFrame
{
	/// +0x2F70: (GetTickCount - [0xD15A98]) x 0.001 (0x5BCD2A..0x5BCD4E), then the play position x 0.001 when it is >= 0
	/// (0x5BCD74..0x5BCD82). The tag walker fn_005BCBC0 runs on it in both cases (0x5BCDFC..0x5BCE07), so the tags
	/// fire on the tick time while the play position is still -1 (Say's delay of up to 500 ms)
	float time {0.0f};
	/// The play position was >= 0 (0x5BCD6C..0x5BCD72): the vowels fn_005BF810 run (0x5BCDD2..0x5BCDDB)
	bool playing {false};
	/// +0x2F64..+0x2F6C: the key the last CalcKey left (stale when CalcKey was skipped, no PCM: 0x5BCD88..0x5BCD98)
	std::array<float, 3> weights {};
};

/// What the logic reads from the rest of the game; unset gives the value written next to each
struct Queries
{
	/// GRand::LocalRand 0x6DE570. Unset: game_random::LocalRand
	std::function<uint32_t(int32_t n)> localRand;
	/// GRand::LocalFloatRand 0x6DE590. Unset: game_random::LocalFloatRand
	std::function<float(float x)> localFloatRand;
	/// ?Random@@YAMMM@Z 0x81D180 (the CRT rand stream). Unset: game_random::crt::Random
	std::function<float(float a, float b)> random;

	/// HelpDude::IsTalking 0x5BB760 (audio::advisor::IsTalking). Unset: false
	std::function<bool(int dude)> isTalking;
	/// fn_005BB730 (audio::advisor::TalkingOrJustStopped). Unset: isTalking
	std::function<bool(int dude)> talkedRecently;
	/// HelpDudeControl+0x74 (audio::advisor::Active). Unset: false
	std::function<bool(int dude)> sayActive;
	/// ApplyLipSync 0x5BCD00's sound part this frame (audio::advisor::LipSyncThisFrame): nullopt when it stopped at
	/// IsTalking or at no sentence ([0xD15A9C] = 0, 0x5BCD18 / 0x5BCD24), the only case where the tag walker does not
	/// run. Unset: nullopt (no vowels, no tags)
	std::function<std::optional<LipSyncFrame>(int dude)> lipSync;

	/// LH3DTech::Get3DPointFromScreen 0x81B370(px, camera depth). Unset: (px.x, px.y, depth)
	std::function<glm::vec3(glm::vec2 pixel, float depth)> pointFromScreen;
	/// fn_0081B450 (+ fn_0081B5F0 when forced) in pixels, nullopt on failure (Convert3DToHover 0x5BD390). Unset: (p.x, p.y)
	std::function<std::optional<glm::vec2>(const glm::vec3& p, bool force)> worldToPixel;
	/// LH3DTech::ProjectPoint 0x819390, nullopt when it fails. Unset: (p.x, p.y, p.z)
	std::function<std::optional<ProjectedPoint>(const glm::vec3& p)> projectPoint;
	/// [0xE839E0], the near clip. Unset: 0
	std::function<float()> nearClip;
	/// The rows of inverse([0xEA1D28]) with the translation zeroed (fn_007FB3F0, 0x5BE2F5): the camera's world axes
	/// (inferred). Every glm::mat3 here holds the original's row i in [i]. Unset: identity
	std::function<glm::mat3()> cameraAxes;
	/// g_camera 0xEA1DB8 (look mode 2). Unset: (0, 0, 0)
	std::function<glm::vec3()> cameraPosition;
	/// LH3DIsland::GetAltitude at world (x, z). Unset: 0
	std::function<float(float x, float z)> altitude;
	/// CalcHeadPos 0x5BFE00 for the look target: the yaw / pitch already clamped to +-1.0472 and x 0.477465 (+0x34BC /
	/// +0x34C0, [-0.5, 0.5]); nullopt when the target is behind the head (local y < 0, 0x5BFFA5), where CalcHeadPos
	/// writes nothing and the previous values stay. Needs the head bone, so it is the renderer's. Unset: (0, 0)
	std::function<std::optional<glm::vec2>(int dude, const glm::mat3& rows, const glm::vec3& position,
	                                       const glm::vec3& target)>
	    headAngles;
	/// The fingertip of the point arm (bone 0 x M + r0 (+0x35C4 x size) + r2 (+0x35C8 x size), 0x5BF1DC..0x5BF2FB);
	/// needs the pose. Unset: the model position
	std::function<glm::vec3(int dude, const glm::mat3& rows, const glm::vec3& position)> fingertip;
	/// A script's game thing (IsAvailable vt+0x2C, position, GetHeight vt+0x42C). Unset: never available
	std::function<std::optional<ObjectInfo>(uint32_t object)> object;
};

class HelpDudeControl;

/// One HelpDude (0x37F0 bytes, vtable 0x900D04): its motion, its states and its anim stack
class HelpDude
{
public:
	/// fn_005C17D0(isEvil, 0) (0x5C2B06 / 0x5C2BAF), then SetState(4)
	HelpDude(int index, const DudeData& data, HelpDudeControl& control);
	/// It keeps references to its data and its control: neither copied nor moved
	HelpDude(const HelpDude&) = delete;
	HelpDude(HelpDude&&) = delete;
	HelpDude& operator=(const HelpDude&) = delete;
	HelpDude& operator=(HelpDude&&) = delete;
	~HelpDude() = default;

	// --- small members of the original, by address ---
	/// fn_005BBBD0: the hover snapped to a pixel (both Zoomers SetPosition), the trail reset, SetState(4, 0)
	void SetPosition(glm::ivec2 pixel);
	/// fn_005BBDD0: Sethoverx / Sethovery to a pixel in `seconds`, +0x34EC/F0 = the target, fn_005BBD20, SetState(4, 0)
	void FlyTo(glm::ivec2 pixel, float seconds, bool clamp);
	/// Sethoverx 0x5B96D0: ignored while the puff runs; clamp to [-0.75, 0.75] (0x900C44 / 0x8AB274); then
	/// Zoomer::SetDestinationWithSpeedAndTime(target, 0, T) inline (T < 0.001 snaps)
	void SetHoverX(float target, float seconds, bool clamp);
	/// Sethovery 0x5B98D0: the same on y, clamp [-0.6, 0.6] (0x900C48 / 0x8C7BDC)
	void SetHoverY(float target, float seconds, bool clamp);
	/// fn_005BD210(emotion, peak): target +0x2C30, peak +0x2C34 = max(peak, 0), +0x3488 = 0
	void SetEmotion(uint32_t emotion, float peak);
	/// fn_005BBCD0: the 32 trail points reset to the hover (the trail is drawn by the renderer: a counter here)
	void ResetTrail() { ++_trailResets; }
	/// fn_005BCC20: every anim slot's mode +0x2F7C = 0
	void ClearAnims();
	/// fn_005BBE70(hx, hy, fromHome): +0x34EC/F0 = +0x34FC/+0x3500 = (hx, hy), fn_005BBD20, from home a snap to the
	/// edge's off-screen point (fn_005BD440) and a trail reset, then SetState(0x100, 0)
	void Cling(float hx, float hy, bool fromHome);
	/// fn_005BBD20: not in 0x120; |x| > 1.28205 |y| -> right (x = 1.04) or left (-1.04), else bottom (y = 0.78) or top
	/// (-0.78) (0x900C64, 0x3F851EB8, 0x3F47AE14)
	void SnapClingEdge();
	/// fn_005BBEF0: 0x80 (current or queued) heading to 0x200, or 0x200 current or queued
	[[nodiscard]] bool IsPlayingAnim() const;
	/// fn_005BBF30(hx, hy, anim, speed): anim 0 while playing -> SetState(4, 0); else +0x34FC/+0x3500 = target,
	/// +0x350C = anim, +0x3508 = speed, +0x3510 = 0x200, SetState(0x80, 0)
	void PlayAnim(float hx, float hy, uint32_t anim, float speed);
	/// fn_005BBFA0(pixel): +0x35A4 = 0, +0x35AC/B0 = the pixel in hover space, SetState(8, 0)
	void ScreenPoint(glm::ivec2 pixel);
	/// fn_005BC4A0(pos, inWorld, side 8.0, height 5.0): world Zoomers (snap out of the world, else 0.5 s), +0x3670 /
	/// +0x3674 / +0x3678, the projection test, the hover target or (0, 1) off screen, SetState(8, 0), the look target
	void PointAt(const glm::vec3& position, bool inWorld, float side, float height);
	/// SetState 0x5BD4A0
	void SetState(uint32_t next, bool force);
	/// fn_005C1D20: the puff starts (+0x2C18 = 1, +0x2C14 = 0), the 16 particles drawn again with Random if the draw has
	/// made them (+0x2C10, 0x5C1D2E), alpha target = alpha > 0.5 ? 0 : 1
	void StartPuff();

	// --- per frame ---
	/// HelpDude::Update1 0x5BDDA0(dt, focus, zMin, sfx, a5)
	void Update1(float dt, bool focus, float zMin, bool sfx);
	/// fn_005BC0A0(dt, engaged, partnerOut, talkOrPoint, lookPos): the look mode and target
	void UpdateLookTarget(float dt, bool engaged, bool partnerOut, bool talkOrPoint, const glm::vec3* lookPosition);
	/// fn_005BF620(dt, focus, 0, 1, finish): head angles, their smoothing and the look layers 18 / 19 or 70 / 71
	void UpdateHead(float dt);
	/// The draw-side part of fn_005C0700: the alpha byte (with the GoInvisible flicker), the fade +3/s / -2/s, the puff
	/// (its particles made on its first draw, their ages, alphas and drift)
	void UpdateDraw(int32_t frameMs, uint32_t tickMs);
	/// fn_0042ACC0(tag, dude, resolve 1, apply 1)
	void FireTag(const AudioTag& tag, bool resolve, bool apply);

	// --- state, read by the renderer and the tests ---
	[[nodiscard]] int Index() const { return _index; }
	[[nodiscard]] uint32_t State() const { return _state; }
	[[nodiscard]] uint32_t QueuedState() const { return _queuedState; }
	[[nodiscard]] float StateTime() const { return _stateTime; }
	[[nodiscard]] glm::vec2 Hover() const { return {_hoverX.value, _hoverY.value}; }
	[[nodiscard]] const Zoomer& HoverX() const { return _hoverX; }
	[[nodiscard]] const Zoomer& HoverY() const { return _hoverY; }
	[[nodiscard]] const Zoomer& DepthChannel() const { return _depth; }
	[[nodiscard]] float Closeness() const { return _closeness; }
	[[nodiscard]] float ModelScale() const { return _modelScale; }
	void SetModelScale(float scale) { _modelScale = scale; }
	[[nodiscard]] float Alpha() const { return _alpha; }
	[[nodiscard]] float AlphaTarget() const { return _alphaTarget; }
	void SetAlpha(float alpha) { _alpha = alpha; }
	void SetAlphaTarget(float alpha) { _alphaTarget = alpha; }
	[[nodiscard]] int32_t AlphaByte() const { return _alphaByte; }
	[[nodiscard]] float InWorld() const { return _inWorld; }
	void SetInWorld(float blend) { _inWorld = blend; }
	[[nodiscard]] float InWorldTarget() const { return _inWorldTarget; }
	void SetInWorldTarget(float target) { _inWorldTarget = target; }
	[[nodiscard]] glm::vec3 WorldTarget() const { return _worldTarget.GetCurrentValue(); }
	[[nodiscard]] float WorldSide() const { return _worldSide; }
	[[nodiscard]] float WorldHeight() const { return _worldHeight; }
	[[nodiscard]] const glm::mat3& Rows() const { return _rows; }
	[[nodiscard]] const glm::vec3& Position() const { return _position; }
	[[nodiscard]] float Roll() const { return _roll; }
	[[nodiscard]] Edge ClingEdge() const { return _edge; }
	[[nodiscard]] glm::vec2 ClingTarget() const { return {_clingX, _clingY}; }
	[[nodiscard]] glm::vec2 PointTarget() const { return {_pointX, _pointY}; }
	[[nodiscard]] bool PointOffScreen() const { return _pointOffScreen; }
	[[nodiscard]] bool PuffRunning() const { return _puffRunning; }
	/// +0x2C14 (the draw side of the puff, 0x5C0AE5..0x5C0E95)
	[[nodiscard]] float PuffTime() const { return _puffTime; }
	/// clamp(+0x2C14 x 4, 0, 1) of this frame's draw (0x5C0C2A..0x5C0C64)
	[[nodiscard]] float PuffFade() const { return _puffFade; }
	/// +0x2C10: nullopt until the first draw of a puff makes them
	[[nodiscard]] const std::optional<std::array<PuffParticle, k_PuffParticles>>& PuffParticles() const
	{
		return _puffParticles;
	}
	[[nodiscard]] uint32_t TrailResets() const { return _trailResets; }
	[[nodiscard]] uint32_t Emotion() const { return _emotion; }
	[[nodiscard]] uint32_t EmotionTarget() const { return _emotionTarget; }
	[[nodiscard]] float EmotionWeight() const { return _emotionWeight; }
	[[nodiscard]] float EmotionPeak() const { return _emotionPeak; }
	[[nodiscard]] int32_t LookMode() const { return _lookMode; }
	[[nodiscard]] int32_t TagLookMode() const { return _tagLookMode; }
	[[nodiscard]] const std::optional<glm::vec3>& LookTarget() const { return _lookTarget; }
	[[nodiscard]] glm::vec2 HeadAngles() const { return {_head.x, _head.y}; }
	[[nodiscard]] const std::array<float, 16>& Face() const { return _face; }
	[[nodiscard]] uint32_t ActiveFlags() const { return _activeFlags; }
	[[nodiscard]] int32_t SlotMode(size_t i) const { return _slotMode[i]; }
	[[nodiscard]] float SlotPhase(size_t i) const { return _slotPhase[i]; }
	void SetSlot(size_t i, int32_t mode, float phase)
	{
		_slotMode[i] = mode;
		_slotPhase[i] = phase;
	}
	[[nodiscard]] const std::vector<AnimLayer>& Layers() const { return _layers; }
	[[nodiscard]] const std::vector<AnimSound>& Sounds() const { return _sounds; }
	/// +0x8 during this frame's GoInvisible windows (fn_005BB8F0)
	[[nodiscard]] bool Flicker() const { return _flicker; }
	[[nodiscard]] float PartnerDistance() const { return _partnerDistance; }
	[[nodiscard]] float HoverZoneStrength(size_t zone) const { return _zones[zone].strength; }

	/// HoverZone::Feel 0x5B9620 summed over the 6 zones minus 250 e^2 (HelpDude::Feel 0x5B9AD0)
	[[nodiscard]] float Feel(float x, float y) const;
	/// fn_005BD2A0(hx, hy, k, flag): the hover point in 3D, at depth flag ? 2 near : lerp(+0x35B8, +0x35BC,
	/// smooth(+0x34D0)) (1 + 0.3 k)
	[[nodiscard]] glm::vec3 HoverTo3D(float hx, float hy, float k, bool nearFlag) const;
	/// Convert3DToHover 0x5BD390
	[[nodiscard]] std::optional<glm::vec2> WorldToHover(const glm::vec3& p, bool force) const;

	HelpDude* partner {nullptr}; ///< +0x3470 (ctrl Process 0x5C3C46)

private:
	friend class HelpDudeControl;

	struct Zone
	{
		float strength {0.0f}; ///< +0
		float x {0.0f};        ///< +4
		float y {0.0f};        ///< +8
		float inner {0.0f};    ///< +0xC
		float outer {0.0f};    ///< +0x10
		[[nodiscard]] float Feel(float px, float py) const;
	};

	[[nodiscard]] const CAnim* Clip(uint32_t i) const { return i < k_AnimSlots ? _data.clips[i] : nullptr; }
	/// ApplyAnim 0x5BB980(anim, phase, referencePhase, wrap): the layer and the root move added through the rows
	void ApplyAnim(uint32_t anim, float phase, float referencePhase, bool wrap);
	/// fn_00861EE0 called directly at ms = clamp(ftol(dur * w), 0, dur - 1) with the key `referenceKey`
	void AddAt(uint32_t anim, float weight, uint32_t referenceKey);
	void Sound(uint32_t anim, float phase, bool sfx);
	/// UpdateHoverPos 0x5BA350(probability)
	void UpdateHoverPosition(float probability);
	/// fn_005BA010(zMin)
	void UpdateDepthSpacing(float zMin);
	/// fn_005BDAF0(dt, focus)
	void UpdateZones(float dt, bool focus);
	/// fn_005BC7D0(stable, rate, sfx)
	void UpdateBasePose(bool stable, float rate, bool sfx);
	/// fn_005BD0B0(dt): face record, blink, lids, emotion clip
	void UpdateFace(float dt);
	/// ApplyLipSync 0x5BCD00(dt, sfx): vowels, the tag walker fn_005BCBC0 and the 80 slots
	void UpdateAnimStack(float dt, bool sfx);
	/// The model matrix of Update1 step 8 and the in-world pose of step 9 (0x5BE2F5..0x5BE9B6)
	void UpdateMatrix(float dt);
	/// The six Random draws of one puff particle in the exe's order (fn_005C1D20 0x5C1D3E..0x5C1DCD, the creation
	/// 0x5C0B62..0x5C0BEC): grey, vx, vy, vz, size, spin; the age 0
	void RandomisePuffParticle(PuffParticle& particle) const;

	int _index;
	const DudeData& _data;
	HelpDudeControl& _control;

	uint32_t _state {0};            ///< +0x3490
	uint32_t _queuedState {0};      ///< +0x3494
	float _totalTime {0.0f};        ///< +0x3480
	float _stateTime {0.0f};        ///< +0x3484
	float _emotionTime {0.0f};      ///< +0x3488
	float _stableBlend {0.0f};      ///< +0x348C
	float _hoverClock {0.0f};       ///< +0x3478
	float _hoverClock2 {0.0f};      ///< +0x347C
	Zoomer _hoverX;                 ///< +0x3514
	Zoomer _hoverY;                 ///< +0x3544
	Zoomer _depth;                  ///< +0x3574
	Zoomer3d _worldTarget;          ///< +0x35E0 / +0x3610 / +0x3640
	float _closeness {0.0f};        ///< +0x34D0
	float _closenessTarget {1.0f};  ///< +0x34D4 (init 1, 0x5C1B0B)
	float _closenessRate {1.0f};    ///< +0x34D8
	float _modelScale {1.0f};       ///< +0x37E4
	bool _gimme {false};            ///< +0x37E8
	float _alpha {1.0f};            ///< +0x2C1C
	float _alphaTarget {1.0f};      ///< +0x2C20
	int32_t _alphaByte {255};
	bool _flicker {false};          ///< +0x8
	bool _puffRunning {false};      ///< +0x2C18
	float _puffTime {0.0f};         ///< +0x2C14
	float _puffFade {0.0f};         ///< this frame's clamp(+0x2C14 x 4, 0, 1)
	std::optional<std::array<PuffParticle, k_PuffParticles>> _puffParticles; ///< +0x2C10
	float _inWorld {0.0f};          ///< +0x35DC
	float _inWorldTarget {0.0f};    ///< +0x3670
	float _worldSide {0.0f};        ///< +0x3674
	float _worldHeight {0.0f};      ///< +0x3678
	float _clingX {0.0f};           ///< +0x34EC
	float _clingY {0.0f};           ///< +0x34F0
	Edge _edge {Edge::Bottom};      ///< +0x34F4
	float _clingClock {0.0f};       ///< +0x34F8
	float _targetX {0.0f};          ///< +0x34FC
	float _targetY {0.0f};          ///< +0x3500
	float _roll {0.0f};             ///< +0x3504
	float _animSpeed {0.0f};        ///< +0x3508
	uint32_t _scriptAnim {0};       ///< +0x350C
	uint32_t _afterFly {0};         ///< +0x3510
	bool _pointOffScreen {false};   ///< +0x35A4
	float _pointBlend {0.0f};       ///< +0x35A8
	float _pointX {0.0f};           ///< +0x35AC
	float _pointY {0.0f};           ///< +0x35B0
	uint32_t _avoidDir {0};         ///< +0x35CC
	uint32_t _activeFlags {0};      ///< +0x35D0
	float _partnerDistance {100.0f}; ///< +0x3474 (init 0x42C80000)
	std::array<Zone, k_Zones> _zones {}; ///< +0x3380
	float _mouseSpeed {0.0f};       ///< +0x3340
	glm::ivec2 _lastMouse {0};      ///< +0x3468 / +0x346C
	float _mouseInterest {0.0f};    ///< +0x3344
	bool _lookToggle {false};       ///< +0x3348
	float _lookTimer {0.0f};        ///< +0x334C
	int32_t _lookMode {0};          ///< the mode fn_005BC0A0 chose this frame
	int32_t _tagLookMode {0};       ///< +0x2F78
	int32_t _savedLookMode {0};     ///< +0x2F74
	std::optional<glm::vec3> _lookTarget; ///< +0x3498 / +0x34C8
	glm::vec3 _head {0.0f};         ///< +0x34B0
	glm::vec3 _headTarget {0.0f};   ///< +0x34BC
	uint32_t _emotion {0};          ///< +0x2C28
	float _emotionWeight {0.0f};    ///< +0x2C2C
	uint32_t _emotionTarget {0};    ///< +0x2C30
	float _emotionPeak {0.0f};      ///< +0x2C34
	std::array<float, 16> _face {}; ///< +0x2E38
	float _blinkTimer {0.0f};       ///< +0x2E78
	std::array<int32_t, k_AnimSlots> _slotMode {};  ///< +0x2F7C
	std::array<float, k_AnimSlots> _slotPhase {};   ///< +0x30BC
	std::array<float, k_AnimSlots> _slotLast {};    ///< +0x31FC (init -1, 0x5C1959)
	std::vector<AudioTag> _tags;    ///< +0x2F08
	size_t _nextTag {0};
	glm::mat3 _rows {1.0f};         ///< +0x3350 (rows r0, r1, r2)
	glm::vec3 _position {0.0f};     ///< +0x3374
	uint32_t _trailResets {0};
	std::vector<AnimLayer> _layers;
	std::vector<AnimSound> _sounds;
};

/// HelpDudeControl (0x88 bytes) and the two HelpSpirits around it. The CHL-facing calls take the help spirit type
/// (1 good, 2 evil: ConvertScriptSpiritToHelpSpirit of HelpSystem.h first).
class HelpDudeControl
{
public:
	/// fn_005C2A40: both dudes (fn_005C17D0(0 / 1)), then both placed at their home point (0x5C2CEA..0x5C2D16), state 0
	HelpDudeControl(const DudeData& good, const DudeData& evil, Queries queries, Screen screen);
	/// The dudes hold a reference to it: neither copied nor moved
	HelpDudeControl(const HelpDudeControl&) = delete;
	HelpDudeControl(HelpDudeControl&&) = delete;
	HelpDudeControl& operator=(const HelpDudeControl&) = delete;
	HelpDudeControl& operator=(HelpDudeControl&&) = delete;
	~HelpDudeControl() = default;

	/// fn_005C5250 / fn_005C5260: type 1 -> dude 0, any other -> 1
	[[nodiscard]] static int DudeOf(int32_t helpSpirit) { return helpSpirit != 1 ? 1 : 0; }

	// --- HelpDudeControl, by dude ---
	/// fn_005C2E90: the nearest of the four off-screen anchors, or (3W/2, -H/2) / (-W/2, -H/2) when within 8 px
	[[nodiscard]] glm::ivec2 HomePoint(int dude) const;
	/// fn_005BD440(edge): (W/2, 3H/2), (-W/2, H/2), (W/2, -H/2), (3W/2, H/2)
	[[nodiscard]] glm::ivec2 Anchor(int edge) const;
	/// fn_005C32C0
	void Eject(int dude);
	/// fn_005C3400
	void Appear(int dude);
	/// fn_005C3590
	void Home(int dude);
	/// fn_005C3540
	void Vanish(int dude);
	/// fn_005C2FB0(dude, px, py)
	void Cling(int dude, float px, float py);
	/// fn_005C3250(dude, px, py): only out of home, FlyTo(ftol(px), ftol(py), 1 s, clamp)
	void Fly(int dude, float px, float py);
	/// fn_005C31B0(dude, px, py, anim, speed)
	void PlayAnim(int dude, float px, float py, uint32_t anim, float speed);
	/// fn_005C3960(dude, pos, inWorld, side, height): point mode 1
	void PointAtPosition(int dude, const glm::vec3& position, bool inWorld, float side, float height);
	/// fn_005C39B0(dude, pixel): point mode 2
	void PointAtPixel(int dude, glm::ivec2 pixel);
	/// fn_005C39E0: point mode 0
	void StopPointing(int dude) { _pointMode[dude] = 0; }
	/// fn_005C39F0
	void LookAt(int dude, const glm::vec3& position);
	/// fn_005C3A20
	void StopLooking(int dude) { _lookOn[dude] = false; }
	/// fn_005C32A0: state 0 or 1
	[[nodiscard]] bool IsHome(int dude) const;
	/// The delay HelpDudeControl::Say 0x5C36D0 gives SaySentence: |hx| - 0.95 (double 0x915438) < 0 ? 0 :
	/// min((v + 1) 250, 500) ms (for audio::advisor::Say, which has no hover)
	[[nodiscard]] uint32_t SayDelayMs(int dude) const;

	// --- HelpSpirit (type 1 / 2) as the CHL calls them ---
	/// fn_005C4BD0 (CHL 7 SPIRIT_EJECT with isHelp, 300 SPIRIT_APPEAR with 1)
	void SpiritEject(int32_t type, bool isHelp);
	/// fn_005C5200 (CHL 8 SPIRIT_HOME with isHelp, 301 SPIRIT_DISAPPEAR with 1, END_DIALOGUE)
	void SpiritHome(int32_t type, bool isHelp);
	/// fn_005C4EF0 (CHL 9 SPIRIT_POINT_POS)
	void SpiritPointPosition(int32_t type, const glm::vec3& position, bool inWorld);
	/// fn_005C4FA0 (CHL 10 SPIRIT_POINT_GAME_THING): nothing for an unavailable object
	void SpiritPointObject(int32_t type, uint32_t object, bool inWorld);
	/// fn_005C4F50 (CHL 418 SPIRIT_SCREEN_POINT): `pixel` = (x W, y H) as the GScript computes it
	void SpiritScreenPoint(int32_t type, glm::ivec2 pixel);
	/// fn_005C5060 (CHL 137 STOP_POINTING)
	void SpiritStopPointing(int32_t type);
	/// fn_005C4E00 (CHL 139 LOOK_AT_POSITION)
	void SpiritLookAtPosition(int32_t type, const glm::vec3& position);
	/// fn_005C4E50 (CHL 100 LOOK_GAME_THING): nothing for an unavailable object
	void SpiritLookObject(int32_t type, uint32_t object);
	/// fn_005C5090 (CHL 138 STOP_LOOKING)
	void SpiritStopLooking(int32_t type);
	/// fn_005C4C80 (CHL 140 PLAY_SPIRIT_ANIM, after the GScript's checks): x, y in 0..1
	void SpiritPlayAnim(int32_t type, float x, float y, uint32_t anim, float speed);
	/// fn_005C4D10 (CHL 165 SPIRIT_PLAYED pushes the negation)
	[[nodiscard]] bool SpiritPlayingAnim(int32_t type) const;
	/// fn_005C4D40 (CHL 166 CLING_SPIRIT): x, y in 0..1
	void SpiritCling(int32_t type, float x, float y);
	/// fn_005C4DA0 (CHL 167 FLY_SPIRIT): x, y in 0..1
	void SpiritFly(int32_t type, float x, float y);
	/// HelpSpirit::Process 0x5C5270 for both, once a game turn: 0x5C50C0 (point object) then 0x5C5170 (look object)
	void ProcessTurn();

	// --- per frame ---
	/// HelpDudeControl::Process 0x5C3A30(dt, 0.4) from HelpSystem::Draw3D, then the draw-side update of every dude
	/// the original draws (state != 0: fn_005C3920 / Draw3D 0x5C5B26)
	void Update(const FrameInput& input);
	/// The sentence's tags (+0x2F08) when SaySentence starts it (from the WAV's cue labels, ParseAudioTags)
	void SetSentenceTags(int dude, std::vector<AudioTag> tags);

	[[nodiscard]] HelpDude& Dude(int dude) { return *_dudes[dude]; }
	[[nodiscard]] const HelpDude& Dude(int dude) const { return *_dudes[dude]; }
	[[nodiscard]] ControlState State(int dude) const { return _state[dude]; }
	[[nodiscard]] float Timer(int dude) const { return _timer[dude]; }
	[[nodiscard]] int Focus() const { return _focus; }
	[[nodiscard]] int32_t PointMode(int dude) const { return _pointMode[dude]; }
	[[nodiscard]] bool LookOn(int dude) const { return _lookOn[dude]; }
	[[nodiscard]] const Screen& GetScreen() const { return _frame.screen; }
	[[nodiscard]] const FrameInput& Frame() const { return _frame; }
	[[nodiscard]] const Queries& GetQueries() const { return _queries; }
	/// [0xD15AA4]: the in-world bob, one static for both dudes
	float worldBob {0.0f};

	// The random streams of the original call sites
	[[nodiscard]] uint32_t LocalRand(int32_t n) const;
	[[nodiscard]] float LocalFloatRand(float x) const;
	[[nodiscard]] float Random(float a, float b) const;

private:
	/// Process 0x5C3A30
	void Process(float dt, float focusBias);

	struct Spirit
	{
		int32_t type {1};          ///< +0x54
		uint32_t pointObject {0};  ///< +0x58
		uint32_t lookObject {0};   ///< +0x5C
		bool pointInWorld {false}; ///< +0x60
	};

	Queries _queries;
	FrameInput _frame;
	std::array<std::unique_ptr<HelpDude>, k_Dudes> _dudes;
	std::array<ControlState, k_Dudes> _state {};     ///< +0xC
	std::array<float, k_Dudes> _timer {};            ///< +0x14
	std::array<float, k_Dudes> _pointSide {};        ///< +0x1C (8.0)
	std::array<float, k_Dudes> _pointHeight {};      ///< +0x24 (5.0)
	std::array<bool, k_Dudes> _pointInWorld {};      ///< +0x2C
	int _focus {0};                                  ///< +0x30
	std::array<glm::vec3, k_Dudes> _pointPosition {}; ///< +0x34
	std::array<int32_t, k_Dudes> _pointMode {};      ///< +0x4C
	std::array<glm::vec3, k_Dudes> _lookPosition {}; ///< +0x54
	std::array<bool, k_Dudes> _lookOn {};            ///< +0x6C
	std::array<Spirit, k_Dudes> _spirits {};         ///< HelpSystem+0xC (good) / +8 (evil), by dude
	uint32_t _flickerNext {0};                       ///< [0xD15AAC]
	int32_t _flickerMultiplier {0};                  ///< [0xD15AA8]

	friend class HelpDude;
};

} // namespace openblack::help::spirits
