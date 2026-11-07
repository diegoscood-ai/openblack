/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"
#include "Magic/Objects/FallingSpellCreature.h"

/// What the falling spell scene draws besides the film fall.bik, which video::FallingSpellVideo plays
/// (Video/FallingSpellVideo.h). Wiki: docs/wiki/magic/falling-spell.md#the-falling-spell-what-fallingspell-draws.
///
/// - The 16 sparks: sprites with their records, anchored to the screen at a random depth, drawn while the sparks flag
///   is set (the update sets it once at 13.45 s; Draw rewrites it with "a spark was still seen").
/// - The light bursts: one LightBurst, initialised twice, drawn as four 64-spoke additive fans a frame by the finish
///   frame callback from state 2 (37.75 s), round the falling creature's projected centre. openblack has no falling
///   creature: the centre comes from a hook that has none, so the bursts neither move nor draw (pending: the creature).
/// - The camera path data\spells\fall\fall.cm2, sampled at the film's ms and given to the camera with the position and
///   focus x 0.8 and a FOV of pi / 4 (at Init and every update). It sets the drawn camera with the path's rotation (see
///   WorldToCamera) for the frame. The game camera leaves it alone in mode 2 and its zoomers are not touched, so nothing
///   puts the game camera back: the first frame in mode 0 draws it again. Applied through Hooks::applyCamera every
///   update, cleared at Close. In mode 2 no land or model is drawn, so only the creature would show it (pending: the
///   creature).
/// - The model light: Init keeps it, Draw puts it at (0, 0, 1000) for the creature, Close puts the kept one back.
/// - The falling creature: a copy of the local player's creature made at Init (Hooks::makeCreature), with its first
///   step of time, look reset and start action; its clock starts at 100 ms of film, and each update advances it by the
///   film's ms since the last update when that is more than 0 (AdvanceCreature); Close frees it. Each Draw makes it
///   ready to be drawn (hidden or not), sets whether it is the body that sparkles (not while hidden) and places its
///   eyes when it is shown.
/// - The falling creature's two hand glows: lit at Init with a power of 1, placed on its body each Draw that shows it,
///   then given their power of the film time in their windows (GlowPower), which the next draw shows.
///
/// Not ported yet: the debug keys that turn the falling creature. The rules of the film time are in
/// Magic/Objects/FallingSpellCreature.h.
namespace openblack::magic::falling_spell
{

/// 16 sprites and 16 spark records
inline constexpr int k_SparkCount = 0x10;
/// A LightBurst holds four arrays of 64 floats
inline constexpr int k_BurstSpokes = 0x40;
/// The fan's vertices: the centre and the rim point of each spoke
inline constexpr int k_BurstVertices = 0x80;
/// The FOV set before every Draw: pi / 4
inline constexpr float k_FallFov = 0.7853981852531433f;
/// 0.8: the path's position and focus are scaled by it before they reach the camera
inline constexpr float k_PathScale = 0.800000011920929f;
/// The model light while the creature is drawn
inline constexpr glm::vec3 k_CreatureLight {0.0f, 0.0f, 1000.0f};

/// A spark's record (set at Init)
struct Spark
{
	float x {0.0f};      ///< Random(-0.1, 1.1), a fraction of the screen width
	float y {0.0f};      ///< Random(-0.1, 1.1), of the height; Draw moves it towards 1
	float depth {0.0f};  ///< Random(5, 25), the depth of the screen to world projection
	float size {0.0f};   ///< Random(4, 8)
	float age {0.0f};    ///< 0; + the frame's delta time x 0.0013 each Draw
	float spin {0.0f};   ///< Random(-2, 2)
	float shrink {0.0f}; ///< 2 - y
	uint32_t rgb {0};    ///< (min(2 v, 255), v, v / 3) with v = the integer part of Random(16, 100)
};

/// A LightBurst: its 64 spokes
struct LightBurst
{
	std::array<float, k_BurstSpokes> radius {}; ///< Random(0, 1) (local) + 0.5, x 1.4 (1/8) and x 0.714286 (1/16)
	std::array<float, k_BurstSpokes> phase {};  ///< Random(0, 2 pi)
	std::array<float, k_BurstSpokes> rate {};   ///< Random(2, 20), negated when Random(0, 1) < 0.5
	std::array<float, k_BurstSpokes> wobble {}; ///< Random(-0.8, 0.8)

	/// Fills the spokes at random
	void Init();
};

/// A vertex in screen pixels (x right, y down), as the world triangles and sprites are projected
struct ScreenVertex
{
	glm::vec2 pixel {0.0f};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};

/// A spark as it is queued for the frame's end: its Z-sorter key (|pos - camera|^2) and its quad on the screen
struct KeyedSpark
{
	float key {0.0f};
	std::array<ScreenVertex, 4> quad {};
};

/// One thing the frame's end queue draws in the falling film: one of the falling creature's glows or one of the sparks,
/// by its place in the list it was queued from
struct FrameEndDraw
{
	bool glow {false};
	size_t index {0};

	bool operator==(const FrameEndDraw&) const = default;
};
/// The order the frame's end queue draws the falling creature's glows and the sparks in, from their keys: far to near,
/// and with equal keys the one queued first first. The glows are queued first, as the creature's draw queues them
/// before the sparks are queued, each list in its order (zsort::Queue)
[[nodiscard]] std::vector<FrameEndDraw> FrameEndOrder(std::span<const float> glowKeys, std::span<const float> sparkKeys);

/// One burst draw: 0x80 vertices and 0x40 triangles (2i, 2i + 1, 2 ((i + 1) & 63) + 1)
struct BurstFan
{
	std::array<ScreenVertex, k_BurstVertices> vertices {};
};
/// The fan's index list
[[nodiscard]] std::array<int, 3 * k_BurstSpokes> BurstIndices();

/// A burst at the screen point (x, y) (truncated to integers): a fan whose spoke i reaches
/// r = radius x (R[i] + W[i] sin(phase B[i] + P[i])) x (4 s^2 k + 1 - k),
/// s = |sin((i + 1) shape^2 pi / 4)|, k = clamp(1 - shape^2, 0, 1), at the angle i pi / 32 + phase (sin to x, cos to
/// y); the centre has `argb`, the rim the same alpha and no colour. The original's depth only places the points in the
/// world for the Z (they project back to the same pixels), so it is not an argument here.
/// Drawn with the additive atmosphere material (atmos.raw, mode 13) as world triangles
[[nodiscard]] BurstFan DrawBurst(const LightBurst& burst, float x, float y, float radius, float phase, float shape,
                                 uint32_t argb);

/// fall.cm2: +0 the size in bytes, +4 the duration (ms), +8 the count of keys, then the keys (0x48 bytes): position,
/// focus and 12 floats (a camera matrix)
struct CameraPath
{
	struct Key
	{
		glm::vec3 position {0.0f};
		glm::vec3 focus {0.0f};
		std::array<float, 12> matrix {};
	};
	uint32_t duration {0};
	std::vector<Key> keys;

	/// The file's bytes; nothing when they do not hold the header's count of keys
	[[nodiscard]] static std::optional<CameraPath> Parse(const std::vector<uint8_t>& bytes);
	/// The path at `ms`, untransformed: see the .cpp for its fraction
	[[nodiscard]] Key At(uint32_t ms) const;
};

/// The camera the fall gives: the path's key at the film ms, position and focus x 0.8, and the FOV pi / 4
struct Camera
{
	glm::vec3 position {0.0f};
	glm::vec3 focus {0.0f};
	std::array<float, 12> matrix {};
	float fov {k_FallFov};
};

/// The world to camera matrix (rows, x' = m0 x + m3 y + m6 z + m9) from the path's first nine floats a0..a8:
/// (a0, a3, -a6, a1, a4, -a7, a2, a5, -a8), each row of three normalised, then m9..m11 = -(the columns . position).
/// The camera's right, up and forward are so the path matrix's rows 0, 1 and -2; the focus does not turn it (the
/// look-at is overwritten). As a glm (column) matrix with the same memory, like openblack's glm::lookAt view
[[nodiscard]] glm::mat4 WorldToCamera(const Camera& camera);

/// The falling spell's state that is not the film's
class FallingSpell
{
public:
	struct Hooks
	{
		/// The bursts' centre: the falling creature's middle point projected to the screen, nothing when it is off
		/// screen. openblack: none (no falling creature), unless the test hook OPENBLACK_TEST_FALL_BURST_AT gives a
		/// screen fraction
		std::function<std::optional<glm::ivec2>(int width, int height)> burstCentre;
		/// The model light (model_light::Light / SetLight)
		std::function<glm::vec3()> light;
		std::function<void(const glm::vec3&)> setLight;
		/// Sets the drawn camera and FOV: the fall's camera each update, nothing to give back the game's camera (what
		/// the game camera does in mode 0 every frame)
		std::function<void(const std::optional<Camera>&)> applyCamera;
		/// The falling creature: made at Init, a copy of the local player's creature started on its falling action
		/// (nothing without one); moved on by some ms of film in the update; freed at Close
		std::function<void()> makeCreature;
		std::function<void(int32_t)> advanceCreature;
		std::function<void()> freeCreature;
		/// Each Draw, hidden or not: the falling creature made ready to be drawn (its body between its last two poses)
		std::function<void()> prepareCreature;
		/// Each Draw, hidden or not, once its body is made ready: the falling creature's hair moved on by the frame
		/// (CreatureHairSeconds)
		std::function<void()> stepCreatureHair;
		/// Each Draw: whether the falling creature is the body that sparkles, which its next step of time reads (false
		/// while it is hidden, true otherwise)
		std::function<void(bool)> setCreatureSparkling;
		/// Each Draw in which the falling creature is drawn (not while it is hidden): its eyes placed on its body
		std::function<void()> placeCreatureEyes;
		/// Each Draw in which the falling creature is drawn, as its draw starts: its hand glows placed on its body, their
		/// textures sliding on with each draw
		std::function<void()> placeCreatureGlows;
		/// One of the falling creature's two hand glows given a power, at a scale of 1 (creature_hand_glow::Set): at
		/// Init, and in Draw after it is drawn
		std::function<void(FallGlow, float)> glowCreature;
		/// Whether the falling creature is there to be drawn this frame
		std::function<bool()> hasCreature;
		/// The local random stream (game_random::LocalRand when not given): a range, gives 0..range - 1
		std::function<uint32_t(int32_t)> localRand;
	};
	explicit FallingSpell(Hooks hooks);

	/// Init without the film and with the camera path: `path` = fall.cm2 when it loaded
	void Init(std::optional<CameraPath> path);
	/// Close without the film: the light put back, the sprites, records and burst freed
	void Close();
	[[nodiscard]] bool IsActive() const { return _active; }

	/// The update's creature: the falling creature moved on by the film's ms less the last update's (100 after Init),
	/// when that is more than 0 (CreatureAdvance), through Hooks::advanceCreature; the last update's ms become these
	void AdvanceCreature(int32_t filmMs);
	/// The update's camera: the path at the film's ms, given to Hooks::applyCamera
	void UpdateCamera(int32_t filmMs);
	/// Turns the sparks on (once, with the state 0 -> 1 at 13.45 s)
	void StartSparks() { _sparksOn = true; }

	/// Draw without the film: the light; the falling creature made ready to be drawn (Hooks::prepareCreature) and its
	/// hair moved on (Hooks::stepCreatureHair) every frame, then, by its last update's film ms (CreatureMs), whether it is the
	/// body that sparkles (Hooks::setCreatureSparkling: not while it is hidden, CreatureHidden) and, when it is shown, its hand
	/// glows (Hooks::placeCreatureGlows) and its eyes (Hooks::placeCreatureEyes), then each hand glow's power of the film time
	/// (GlowPower, Hooks::glowCreature); then the sparks, with a frame delta of `deltaMs` on a `width` x `height`
	/// screen
	void Draw(uint32_t deltaMs, int width, int height);
	/// The finish frame callback (from state 2): the four bursts
	void FinishFrame(uint32_t deltaMs, int32_t state, int width, int height);

	/// The sparks queued this frame, as the sprites are drawn in mode A from the Z-sorter, far to near (key
	/// |pos - camera|^2), none at or before the near plane `nearZ`
	[[nodiscard]] std::vector<std::array<ScreenVertex, 4>> SparkQuads(int width, int height, float nearZ) const;
	/// The sparks queued this frame in the order they are queued, each with its key, none at or before the near plane
	/// `nearZ`: SparkQuads is these far to near
	[[nodiscard]] std::vector<KeyedSpark> KeyedSparkQuads(int width, int height, float nearZ) const;
	/// The bursts of the last FinishFrame (four, or none)
	[[nodiscard]] const std::vector<BurstFan>& Bursts() const { return _bursts; }

	[[nodiscard]] bool SparksOn() const { return _sparksOn; }
	[[nodiscard]] const std::array<Spark, k_SparkCount>& Sparks() const { return _sparks; }
	[[nodiscard]] const std::array<graphics::billboard::Sprite, k_SparkCount>& Sprites() const { return _sprites; }
	[[nodiscard]] const LightBurst& Burst() const { return _burst; }
	[[nodiscard]] float BurstShape() const { return _burstShape; }
	[[nodiscard]] float BurstAlpha() const { return _burstAlpha; }
	[[nodiscard]] float BurstGrow() const { return _burstGrow; }
	[[nodiscard]] const std::optional<CameraPath>& Path() const { return _path; }
	[[nodiscard]] const std::optional<Camera>& CameraNow() const { return _camera; }
	/// The film ms of the falling creature's last update
	[[nodiscard]] int32_t CreatureMs() const { return _creatureMs; }
	/// The colours the falling creature is drawn with this frame (CreatureTintAt at its last update's film ms, made by the
	/// draw with the local random stream); none while it is hidden, without one, or with the spell closed
	[[nodiscard]] const std::optional<CreatureTint>& CreatureTintNow() const { return _creatureTint; }

private:
	Hooks _hooks;
	bool _active {false};
	std::optional<CameraPath> _path;
	int32_t _creatureMs {0};
	std::optional<CreatureTint> _creatureTint;
	glm::vec3 _keptLight {0.0f}; ///< The model light at Init
	bool _sparksOn {false};
	float _burstShape {0.0f};
	float _burstAlpha {0.0f};
	float _burstGrow {1.0f};
	std::array<graphics::billboard::Sprite, k_SparkCount> _sprites {};
	std::array<bool, k_SparkCount> _queued {}; ///< AddDrawing called this frame
	std::array<Spark, k_SparkCount> _sparks {};
	LightBurst _burst;
	std::vector<BurstFan> _bursts;
	std::optional<Camera> _camera;
};

/// The game's one; FrameUpdate drives it from video::GetFallingSpell()
[[nodiscard]] FallingSpell& Get();
/// Once a frame after video::GetFallingSpell().ProcessFrame (Game.cpp): Init when the film's falling spell started,
/// Close when it ended, else the creature and the camera (the update's part), Draw and the finish frame callback, with
/// the frame delta game_clock::FrameRealMs() on the window's size (MagicLoop.cpp)
void FrameUpdate();

} // namespace openblack::magic::falling_spell
