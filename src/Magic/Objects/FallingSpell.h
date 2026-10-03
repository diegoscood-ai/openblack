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
#include <optional>
#include <vector>

#include <glm/mat3x4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/Billboard.h"

/// What runblack.exe W120's FallingSpell (fallingspell.cpp, 0x525CB0..0x527290) draws besides the film fall.bik, which
/// session asistente's video::FallingSpellVideo plays (Video/FallingSpellVideo.h: KickOff 0x5539A0, End 0x553A10, the
/// update 0x526E00, the film's part of Init 0x526060 / Close 0x5264A0). Wiki: miracles.md, "La caída del hechizo".
///
/// - The 16 sparks: an LH3DSprite array (LH3DSprite::Create(0x10, 1) 0x52631E, FallingSpell +0x34) and their 0x20-byte
///   records (new(0x200) 0x526335, +0x38), screen anchored (Get3DPointFromScreen 0x81B370 at a random depth), drawn by
///   FallingSpell::Draw 0x526BFD..0x526DCA while +0x1C is set (the update sets it once at 13.45 s, 0x527047; Draw
///   rewrites it with "a spark was still seen", 0x526DCA).
/// - The light bursts: one LightBurst (new(0x400) 0x52642E, +0x3C, Init 0x525D30 twice: 0x52643E and 0x52644C), four
///   64-spoke additive fans a frame drawn by the finish frame callback 0x526480 -> 0x526530 from state 2 (37.75 s), round
///   the falling creature's projected centre. openblack has no CreatureFalling: the centre comes from a hook that has
///   none, so the bursts neither move nor draw (pendiente: the creature).
/// - The camera path data\spells\fall\fall.cm2 (LHLoadData 0x52609C, fn_0086D4A0 0x5260A2, +0x08), sampled at the
///   film's ms by fn_0086D760 and given to fn_00819F50 with the position and focus x 0.8 (Init 0x526259..0x5262BF, the
///   update 0x526E9B..0x526F1C, with ChangeFov(pi / 4) 0x526EB3). fn_00819F50 0x819F50..0x81A74B is
///   LH3DTech::UpdateCamera 0x819920 with the path's rotation: the drawn camera (g_camera 0xEA1DB8, its focus 0xEA1DC4,
///   the world to camera 0xEA1D28, see WorldToCamera) for the frame. GCamera::Update leaves it alone in mode 2 (with
///   g_game+0x205A28 != 0 no ChangeFov 0x4424F0 and no UpdateCamera 0x442602) and its zoomers are not touched, so
///   nothing puts the game camera back: the first frame in mode 0 draws it again (ChangeFov 0x4425D3, UpdateCamera
///   0x442622). Applied through Hooks::applyCamera every update, cleared at Close. In mode 2 no land or model is drawn,
///   so only the creature would show it (pendiente: the creature).
/// - The model light: Init keeps [0xEA9E90] at +0x10 (0x5262E0..0x526311), Draw puts it at (0, 0, 1000) for the
///   creature (fn_0081E1F0 0x526873..0x526895), Close puts the kept one back (0x5264E8..0x5264F4).
///
/// Not ported (they need the creature, which openblack does not have): the CreatureFalling (new(0x57B8) 0x5260D1, an
/// LH3DCreature copy of the player's creature, ctor 0x47F490, vtable 0x8D8BD8) with its UpdateTime / ResetLook /
/// StartIndividualAction(0xD7, 0) and HandGlows 0 and 2 (Init 0x526105..0x5261EB), its draw and tint in
/// FallingSpell::Draw (0x5268A4..0x526BFB: hidden from 19 550 to 27 350 ms, a random flicker colour scaled by the film
/// time, DrawNow 0x526A42, the hand glows' SetScalePowerTime windows), its advance in the update (0x526E83..0x526E8B)
/// and the debug keys 0xE85376..0xE85379 that turn it (0x5267FA..0x526870).
namespace openblack::magic::falling_spell
{

/// LH3DSprite::Create(0x10, 1) 0x526314..0x52631E and new(0x200) 0x526335 (16 records of 0x20 bytes, 0x52634D)
inline constexpr int k_SparkCount = 0x10;
/// LightBurst 0x400 bytes: four arrays of 0x40 floats (+0x000, +0x100, +0x200, +0x300; Init 0x525D34 `mov edi, 0x40`)
inline constexpr int k_BurstSpokes = 0x40;
/// LightBurst::Draw 0x526042 `mov ecx, 0x80`: the fan's vertices, the centre and the rim point of each spoke
inline constexpr int k_BurstVertices = 0x80;
/// The update 0x526EAE `push 0x3F490FDB`: ChangeFov(pi / 4) before every Draw
inline constexpr float k_FallFov = 0.7853981852531433f;
/// Init 0x526262 / the update 0x526EBC: [0x8C4A04] = 0.8, the path's position and focus scaled before fn_00819F50
inline constexpr float k_PathScale = 0.800000011920929f;
/// Draw 0x52687D..0x52688D: fn_0081E1F0(0, 0, 1000), the model light while the creature is drawn
inline constexpr glm::vec3 k_CreatureLight {0.0f, 0.0f, 1000.0f};

/// A spark's record, FallingSpell +0x38 (0x20 bytes; Init 0x526352..0x526400)
struct Spark
{
	float x {0.0f};      ///< +0x00 Random(-0.1, 1.1) (0x52639D), a fraction of the screen width [0xE85058]
	float y {0.0f};      ///< +0x04 Random(-0.1, 1.1) (0x5263AF), of the height [0xE8505A]; Draw moves it towards 1
	float depth {0.0f};  ///< +0x08 Random(5, 25) (0x5263C0), the depth of Get3DPointFromScreen
	float size {0.0f};   ///< +0x0C Random(4, 8) (0x5263E3)
	float age {0.0f};    ///< +0x10 0 (0x5263FD); + g_delta_time x 0.0013 each Draw
	float spin {0.0f};   ///< +0x14 Random(-2, 2) (0x5263F5)
	float shrink {0.0f}; ///< +0x18 2 - y (0x5263C8..0x5263E0)
	uint32_t rgb {0};    ///< +0x1C (min(2 v, 255), v, v / 3) with v = ftol(Random(16, 100)) (0x526352..0x52639A)
};

/// A LightBurst (0x400 bytes): its 64 spokes (Init 0x525D30)
struct LightBurst
{
	std::array<float, k_BurstSpokes> radius {}; ///< +0x000 LocalFloatRand(1) + 0.5, x 1.4 (1/8) and x 0.714286 (1/16)
	std::array<float, k_BurstSpokes> phase {};  ///< +0x100 Random(0, 2 pi)
	std::array<float, k_BurstSpokes> rate {};   ///< +0x200 Random(2, 20), negated when Random(0, 1) < 0.5
	std::array<float, k_BurstSpokes> wobble {}; ///< +0x300 Random(-0.8, 0.8)

	/// LightBurst::Init 0x525D30
	void Init();
};

/// A vertex in screen pixels (x right, y down), as Draw3DWorldTriangle / LH3DSprite::Draw project them
struct ScreenVertex
{
	glm::vec2 pixel {0.0f};
	glm::vec2 uv {0.0f};
	uint32_t argb {0};
};

/// One LightBurst::Draw call: 0x80 vertices and 0x40 triangles (2i, 2i + 1, 2 ((i + 1) & 63) + 1)
struct BurstFan
{
	std::array<ScreenVertex, k_BurstVertices> vertices {};
};
/// LightBurst::Draw 0x525ED7..0x525EED: the fan's index list
[[nodiscard]] std::array<int, 3 * k_BurstSpokes> BurstIndices();

/// LightBurst::Draw 0x525DF0 (ecx = the burst, 7 stack arguments, `ret 0x1C`): at the screen point (x, y) (ftol'd,
/// 0x525E0E / 0x525E1B) a fan whose spoke i reaches r = radius x (R[i] + W[i] sin(a5 B[i] + P[i])) x (4 s^2 k + 1 - k),
/// s = |sin((i + 1) a6^2 pi / 4)|, k = clamp(1 - a6^2, 0, 1), at the angle i pi / 32 + a5 (sin to x, cos to y); the
/// centre has `argb`, the rim the same alpha and no colour. `depth` only places the points in the world for the Z
/// (Get3DPointFromScreen 0x525E31 / 0x525FA1 projects back to the same pixels), so it is not an argument here.
/// Material LH3DAtmos::AdditiveMaterial [0xEDC364] (atmos.raw, mode 13), Draw3DWorldTriangle 0x526047
[[nodiscard]] BurstFan DrawBurst(const LightBurst& burst, float x, float y, float radius, float a5, float a6,
                                 uint32_t argb);

/// fall.cm2 as fn_0086D4A0 copies it (0x52607F..0x5260B6): +0 the size in bytes, +4 the duration (ms), +8 the count of
/// keys, then the keys (0x48 bytes): position, focus and 12 floats (a camera matrix given to fn_00819F50)
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
	/// fn_0086D760 (ecx = the path, edx = NULL so 0x86DA26 skips the transform), at `ms`: see the .cpp for its fraction
	[[nodiscard]] Key At(uint32_t ms) const;
};

/// The camera fn_00819F50 gets: the path's key at the film ms, position and focus x 0.8, and the FOV pi / 4
struct Camera
{
	glm::vec3 position {0.0f};
	glm::vec3 focus {0.0f};
	std::array<float, 12> matrix {};
	float fov {k_FallFov};
};

/// fn_00819F50 0x81A075..0x81A0F8 and 0x81A112..0x81A22F: the world to camera matrix 0xEA1D28 (LH3D rows,
/// x' = m0 x + m3 y + m6 z + m9) from the path's first nine floats a0..a8: (a0, a3, -a6, a1, a4, -a7, a2, a5, -a8),
/// each row of three normalised by fn_007FB5C0, then m9..m11 = -(the columns . position). The camera's right, up and
/// forward are so the path matrix's rows 0, 1 and -2; the focus does not turn it (UpdateWorldToCamera 0x81A10D's
/// look-at is overwritten). As a glm (column) matrix with the same memory, like openblack's glm::lookAt view
[[nodiscard]] glm::mat4 WorldToCamera(const Camera& camera);

/// FallingSpell's state that is not the film's (the object 0x40 bytes, GGame::FallingSpellVideo 0xCD3B10)
class FallingSpell
{
public:
	struct Hooks
	{
		/// The callback 0x526530's centre: the falling creature's point (fn_004813D0 0x526553 and 0x48F180 0x526580,
		/// their middle) projected by fn_008190D0 0x5265F0, nothing when it is off screen. openblack: none (no
		/// CreatureFalling), unless the test hook OPENBLACK_TEST_FALL_BURST_AT gives a screen fraction
		std::function<std::optional<glm::ivec2>(int width, int height)> burstCentre;
		/// The model light [0xEA9E90] (model_light::Light / SetLight)
		std::function<glm::vec3()> light;
		std::function<void(const glm::vec3&)> setLight;
		/// fn_00819F50 and ChangeFov on the drawn camera: the fall's camera each update, nothing to give back the game's
		/// camera (what GCamera::Update does in mode 0 every frame)
		std::function<void(const std::optional<Camera>&)> applyCamera;
	};
	explicit FallingSpell(Hooks hooks);

	/// FallingSpell::Init 0x526060 without the film (0x5262DA..0x52646B) and the camera path (0x526075..0x5260B6),
	/// `path` = fall.cm2 when it loaded
	void Init(std::optional<CameraPath> path);
	/// FallingSpell::Close 0x5264A0 without the film: the light put back, the sprites, records and burst freed
	void Close();
	[[nodiscard]] bool IsActive() const { return _active; }

	/// The update 0x526E00's camera (0x526E9B..0x526F1C): the path at the film's ms, given to Hooks::applyCamera
	void UpdateCamera(int32_t filmMs);
	/// The update's +0x1C = 1 (0x527047, once, with the state 0 -> 1 at 13.45 s)
	void StartSparks() { _sparksOn = true; }

	/// FallingSpell::Draw 0x5267D0 without the film and the creature: the light (0x526873..0x526895) and the sparks
	/// (0x526BFD..0x526DCA) with g_delta_time = `deltaMs` on a `width` x `height` screen ([0xE85058] / [0xE8505A])
	void Draw(uint32_t deltaMs, int width, int height);
	/// The finish frame callback 0x526480 -> 0x526530 (from state 2, `state` = +0x20): the four bursts
	void FinishFrame(uint32_t deltaMs, int32_t state, int width, int height);

	/// The sparks queued this frame (LH3DSprite::AddDrawing 0x526DA6), as LH3DSprite::Draw 0x840530 draws them in mode
	/// A (flag 0x40 clear) from the Z-sorter, far to near (key |pos - g_camera|^2, LH3DSprite::AddDrawing 0x840C70),
	/// none at or before the near plane `nearZ` [0xE839E0]
	[[nodiscard]] std::vector<std::array<ScreenVertex, 4>> SparkQuads(int width, int height, float nearZ) const;
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

private:
	Hooks _hooks;
	bool _active {false};                                       ///< +0x00
	std::optional<CameraPath> _path;                            ///< +0x08
	glm::vec3 _keptLight {0.0f};                                ///< +0x10 [0xEA9E90] at Init
	bool _sparksOn {false};                                     ///< +0x1C
	float _burstShape {0.0f};                                   ///< +0x28
	float _burstAlpha {0.0f};                                   ///< +0x2C
	float _burstGrow {1.0f};                                    ///< +0x30
	std::array<graphics::billboard::Sprite, k_SparkCount> _sprites {}; ///< +0x34
	std::array<bool, k_SparkCount> _queued {};                  ///< AddDrawing called this frame
	std::array<Spark, k_SparkCount> _sparks {};                 ///< +0x38
	LightBurst _burst;                                          ///< +0x3C
	std::vector<BurstFan> _bursts;
	std::optional<Camera> _camera;
};

/// The game's one; FrameUpdate drives it from session asistente's video::GetFallingSpell()
[[nodiscard]] FallingSpell& Get();
/// Once a frame after video::GetFallingSpell().ProcessFrame (Game.cpp): Init when the film's FallingSpell started
/// (KickOff), Close when it ended (EndFallingSpellVideo), else the camera (the update's part), Draw and the finish frame
/// callback, with g_delta_time = game_clock::FrameRealMs() on the window's size (MagicLoop.cpp)
void FrameUpdate();

} // namespace openblack::magic::falling_spell
