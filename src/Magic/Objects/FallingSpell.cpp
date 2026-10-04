/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpell.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <exception>
#include <utility>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>

#include "Camera/Camera.h"
#include "Camera/ScreenPoint.h"
#include "Common/GameRandom.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/ModelLight.h"
#include "Graphics/ZSorter.h"
#include "Locator.h"
#include "Video/FallingSpellVideo.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::magic::falling_spell;

namespace
{
// __ftol 0x7A1400: towards zero
int32_t Ftol(float value)
{
	return static_cast<int32_t>(value);
}

// 0x525CD0 / 0x525D00: frac(|x|) x 0.25 + 0.25 ([0x8AB3D4]) or + 0.5 ([0x8AA3B4]); frac by x - ftol(x) (0x525CD8..0x525CE1)
float BurstV(float value)
{
	const float a = std::fabs(value);
	return (a - static_cast<float>(Ftol(a))) * 0.25f + 0.25f;
}
float BurstU(float value)
{
	const float a = std::fabs(value);
	return (a - static_cast<float>(Ftol(a))) * 0.25f + 0.5f;
}

// The screen of the sparks: ChangeFov(pi / 4) 0x8195B0 (the update 0x526EB3, before every Draw), the lens of
// Camera/ScreenPoint.h (Get3DPointFromScreen 0x81B370 and LH3DSprite::Draw's projection in camera space)
screen_point::Lens LensOf(int width, int height)
{
	return screen_point::LensOf(width, height, k_FallFov);
}
} // namespace

void LightBurst::Init()
{
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		// 0x525D39..0x525D53: LocalFloatRand(1) + 0.5
		radius.at(i) = game_random::LocalFloatRand(1.0f) + 0.5f;
		// 0x525D4C..0x525D5F: Random(0, 2 pi) ([0x40C90FDB])
		phase.at(i) = game_random::crt::Random(0.0f, 6.2831855f);
		// 0x525D5A..0x525D6F: Random(2, 20)
		rate.at(i) = game_random::crt::Random(2.0f, 20.0f);
		// 0x525D75..0x525D96: Random(0, 1) < 0.5 negates it
		if (game_random::crt::Random(0.0f, 1.0f) < 0.5f)
		{
			rate.at(i) = -rate.at(i);
		}
		// 0x525D9C..0x525DAB: Random(-0.8, 0.8)
		wobble.at(i) = game_random::crt::Random(-0.8f, 0.8f);
		// 0x525DB1..0x525DC8: LocalRand(16) < 2: x 1.4 ([0x8C7E18])
		if (game_random::LocalRand(0x10) < 2)
		{
			radius.at(i) *= 1.4f;
		}
		// 0x525DCA..0x525DE1: LocalRand(16) < 1: x 0.714286 ([0x8D8BCC])
		if (game_random::LocalRand(0x10) < 1)
		{
			radius.at(i) *= 0.71428573f;
		}
	}
}

std::array<int, 3 * k_BurstSpokes> falling_spell::BurstIndices()
{
	std::array<int, 3 * k_BurstSpokes> indices {};
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		// 0x525ED4..0x525EED: 2i, 2i + 1, 2 ((i + 1) & 0x3F) + 1
		indices.at(3 * i) = 2 * i;
		indices.at(3 * i + 1) = 2 * i + 1;
		indices.at(3 * i + 2) = 2 * ((i + 1) & 0x3F) + 1;
	}
	return indices;
}

BurstFan falling_spell::DrawBurst(const LightBurst& burst, float x, float y, float radius, float a5, float a6,
                                  uint32_t argb)
{
	BurstFan fan;
	// 0x525E0E / 0x525E1B: the centre ftol'd; the rim colour keeps only the alpha (0x525DFF `and 0xFF000000`)
	const glm::vec2 centre(static_cast<float>(Ftol(x)), static_cast<float>(Ftol(y)));
	const uint32_t rim = argb & 0xFF000000u;
	// 0x525E36..0x525E58: w = a6^2 pi / 4 ([0x8C6C9C]); k = 1 - a6^2 clamped to [0, 1] (0x525E70..0x525EA4)
	const float a6Squared = a6 * a6;
	const float w = a6Squared * 0.7853982f;
	float k = 1.0f - a6Squared;
	if (k <= 0.0f)
	{
		k = 0.0f;
	}
	else if (!(k < 1.0f))
	{
		k = 1.0f;
	}
	const float rest = 1.0f - k;
	// 0x525EB8..0x525ECC: v of the centre 0.4 a5 ([0x8C7A44]), of the rim 1 + 0.4 a5
	const float vCentre = a5 * 0.4f;
	const float vRim = vCentre + 1.0f;
	for (int i = 0; i < k_BurstSpokes; ++i)
	{
		const auto spoke = static_cast<size_t>(i & 0x3F);
		// 0x525ED0..0x525F30: (R + W sin(a5 B + P)) x radius
		float r = (burst.radius.at(spoke) + burst.wobble.at(spoke) * std::sin(a5 * burst.rate.at(spoke) +
		                                                                     burst.phase.at(spoke))) *
		          radius;
		// 0x525F34..0x525F5A: s = |sin((i + 1) w)|, r (4 s^2 k) + (1 - k) r ([0x8AB418] = 4)
		const float s = std::fabs(std::sin(static_cast<float>(i + 1) * w));
		r = s * s * 4.0f * r * k + rest * r;
		// 0x525F5C..0x525F92: the angle i pi / 32 ([0x8C7C94]) + a5, sin to x, cos to y, each ftol'd
		const float theta = static_cast<float>(i) * 0.09817477f + a5;
		const glm::vec2 point(static_cast<float>(Ftol(r * std::sin(theta) + x)),
		                      static_cast<float>(Ftol(r * std::cos(theta) + y)));
		// 0x525FA6..0x526004: u = frac(i / 64) / 4 + 0.5 for both, v = frac(0.4 a5) / 4 + 0.25 (centre),
		// frac(1 + 0.4 a5) / 4 + 0.25 (rim)
		const float u = BurstU(static_cast<float>(i) * 0.015625f);
		fan.vertices.at(static_cast<size_t>(2 * i)) = {centre, {u, BurstV(vCentre)}, argb};
		fan.vertices.at(static_cast<size_t>(2 * i + 1)) = {point, {u, BurstV(vRim)}, rim};
	}
	return fan;
}

std::optional<CameraPath> CameraPath::Parse(const std::vector<uint8_t>& bytes)
{
	constexpr size_t k_Header = 0xC;
	constexpr size_t k_KeySize = 0x48; // fn_0086D760 0x86D809 `lea eax, [esi + esi*8]` x 8
	if (bytes.size() < k_Header)
	{
		return std::nullopt;
	}
	const auto u32 = [&bytes](size_t at) {
		uint32_t v = 0;
		std::memcpy(&v, bytes.data() + at, sizeof(v));
		return v;
	};
	const uint32_t count = u32(8);
	// (openblack) a guard: fn_0086D4A0 / fn_0086D760 read the keys unchecked (fall.cm2: 1449 keys, 104 340 bytes)
	if (count == 0 || bytes.size() < k_Header + static_cast<size_t>(count) * k_KeySize)
	{
		return std::nullopt;
	}
	CameraPath path;
	path.duration = u32(4);
	path.keys.resize(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		std::array<float, 18> floats {};
		std::memcpy(floats.data(), bytes.data() + k_Header + static_cast<size_t>(i) * k_KeySize, k_KeySize);
		auto& key = path.keys.at(i);
		key.position = {floats[0], floats[1], floats[2]};
		key.focus = {floats[3], floats[4], floats[5]};
		std::copy(floats.begin() + 6, floats.end(), key.matrix.begin());
	}
	return path;
}

CameraPath::Key CameraPath::At(uint32_t ms) const
{
	const auto n = static_cast<uint32_t>(keys.size());
	// (openblack) a guard: the original divides by n - 1 and by the duration unchecked (0x86D78A, 0x86D7B1)
	if (n < 2 || duration == 0)
	{
		return keys.empty() ? Key {} : keys.front();
	}
	// 0x86D76E..0x86D77A: past the duration, duration - 1 (unsigned `jbe`)
	uint32_t t = ms;
	if (t > duration)
	{
		t = duration - 1;
	}
	// 0x86D780..0x86D799: the key (n - 1) t / duration, unsigned, wrapped into n
	uint32_t key = static_cast<uint32_t>((static_cast<uint64_t>(n - 1) * t) / duration);
	if (key >= n)
	{
		key %= n;
	}
	// 0x86D79B..0x86D7F1: step = duration / (n - 1) as a float, the fraction (t - ftol(t / step) ftol(step)) / step:
	// the remainder takes the step's integer part only, so the fraction grows past 1 along the path (in fall.cm2, step
	// 33.379 and 33 x k, above 1 from about 2.9 s): the original extrapolates from the key, and so does this
	const float step = static_cast<float>(duration) / static_cast<float>(n - 1);
	const int32_t k = Ftol(static_cast<float>(t) / step);
	const int32_t whole = Ftol(step);
	const auto remainder = static_cast<uint32_t>(static_cast<int32_t>(t) - k * whole);
	const float f = static_cast<float>(remainder) / step;
	// 0x86D7E2..0x86D7F5: the next key, the same one at the end
	uint32_t next = key + 1;
	if (next >= n)
	{
		next = key;
	}
	const float g = 1.0f - f;
	const auto& a = keys.at(key);
	const auto& b = keys.at(next);
	// 0x86D809..0x86D891 (position), 0x86D894..0x86D90D (focus): a (1 - f) + b f; 0x86D910..0x86DA1B the matrix the
	// same way (a's twelve x (1 - f), fn_0086DAC0 b x f, fn_0086DA50 the sum)
	Key out;
	out.position = a.position * g + b.position * f;
	out.focus = a.focus * g + b.focus * f;
	for (size_t i = 0; i < out.matrix.size(); ++i)
	{
		out.matrix.at(i) = a.matrix.at(i) * g + b.matrix.at(i) * f;
	}
	return out;
}

glm::mat4 falling_spell::WorldToCamera(const Camera& camera)
{
	const auto& a = camera.matrix;
	// 0x81A075..0x81A0F5: the path's 3x3 transposed with its third row negated, in the caller's matrix
	std::array<float, 9> m {a[0], a[3], -a[6], a[1], a[4], -a[7], a[2], a[5], -a[8]};
	// 0x81A0F8 fn_007FB5C0: each row of three times InverseSquareRoot 0x841170 of its length squared. (aproximado) an
	// exact 1 / sqrt, not LH3DMath's table g_inverse_sqrt_lookup_table [0xEEA394]
	for (size_t row = 0; row < 3; ++row)
	{
		const glm::vec3 v(m.at(3 * row), m.at(3 * row + 1), m.at(3 * row + 2));
		const float inverse = 1.0f / std::sqrt(glm::dot(v, v));
		for (size_t k = 0; k < 3; ++k)
		{
			m.at(3 * row + k) *= inverse;
		}
	}
	// 0x81A112..0x81A22F: copied to 0xEA1D28..0xEA1D48, then 0xEA1D4C..0xEA1D54 = -(m0 p.x + m3 p.y + m6 p.z),
	// -(m1 p.x + m4 p.y + m7 p.z), -(m2 p.x + m5 p.y + m8 p.z)
	const glm::vec3& p = camera.position;
	const glm::vec3 c0(m[0], m[3], m[6]);
	const glm::vec3 c1(m[1], m[4], m[7]);
	const glm::vec3 c2(m[2], m[5], m[8]);
	return {glm::vec4(m[0], m[1], m[2], 0.0f), glm::vec4(m[3], m[4], m[5], 0.0f), glm::vec4(m[6], m[7], m[8], 0.0f),
	        glm::vec4(-glm::dot(c0, p), -glm::dot(c1, p), -glm::dot(c2, p), 1.0f)};
}

FallingSpell::FallingSpell(Hooks hooks)
    : _hooks(std::move(hooks))
{
}

void FallingSpell::Init(std::optional<CameraPath> path)
{
	// 0x52606A..0x526070: Close first when it is open
	if (_active)
	{
		Close();
	}
	// 0x5260A2..0x5260B6: +0x08 the path
	_path = std::move(path);
	_camera.reset();
	// 0x5262DA: +0x00 = 1; 0x5262E0..0x526311: +0x10 = [0xEA9E90], the model light
	_active = true;
	_keptLight = _hooks.light ? _hooks.light() : glm::vec3(0.0f);
	// 0x526314..0x52631E: LH3DSprite::Create(0x10, 1): SetToZero 0x8404F0 (billboard::Sprite's defaults) and the flag
	// 0x80 (Draw sets the material, here the renderer's smoke one)
	_sprites.fill(graphics::billboard::Sprite {});
	_queued.fill(false);
	// 0x526349..0x526404: the 16 records
	for (auto& spark : _sparks)
	{
		// 0x526352..0x52639A: v = ftol(Random(16, 100)), rgb = (min(2 v, 255) << 16) | (v << 8) | v / 3 (0x55555556)
		const int32_t v = Ftol(game_random::crt::Random(16.0f, 100.0f));
		int32_t red = v + v;
		if (red > 0xFF)
		{
			red = 0xFF;
		}
		spark.rgb = static_cast<uint32_t>((((red << 8) | v) << 8) | (v / 3));
		// 0x52638E..0x5263AC, 0x5263A2..0x5263B9: Random(-0.1, 1.1) twice ([0xBDCCCCCD], [0x3F8CCCCD])
		spark.x = game_random::crt::Random(-0.1f, 1.1f);
		spark.y = game_random::crt::Random(-0.1f, 1.1f);
		// 0x5263B4..0x5263C5: Random(5, 25)
		spark.depth = game_random::crt::Random(5.0f, 25.0f);
		// 0x5263C8..0x5263E0: 2 - y
		spark.shrink = (1.0f - spark.y) + 1.0f;
		// 0x5263CA..0x5263ED: Random(4, 8)
		spark.size = game_random::crt::Random(4.0f, 8.0f);
		// 0x5263E8..0x5263FA: Random(-2, 2)
		spark.spin = game_random::crt::Random(-2.0f, 2.0f);
		// 0x5263FD
		spark.age = 0.0f;
	}
	// 0x526422..0x52642B: +0x1C = +0x20 = +0x24 = 0 (the states are the film's, video::FallingSpellVideo)
	_sparksOn = false;
	// 0x52642E..0x52644C: new(0x400) with LightBurst::Init (0x52643E), then Init again (0x52644C)
	_burst.Init();
	_burst.Init();
	// 0x526459..0x526463: +0x28 = 0, +0x30 = 1.0, +0x2C = 0; RegisterFinishFrameCallback(0, 0, 0x526480, this)
	_burstShape = 0.0f;
	_burstGrow = 1.0f;
	_burstAlpha = 0.0f;
	_bursts.clear();
}

void FallingSpell::Close()
{
	// 0x5264A7..0x5264AD: nothing when it is not open
	if (!_active)
	{
		return;
	}
	// 0x5264B0..0x5264BB: RemoveFinishFrameCallback, +0x00 = 0
	_active = false;
	// 0x5264DD..0x5264F1: the path freed (fn_0086D4D0)
	_path.reset();
	_camera.reset();
	// (openblack) the game camera back: Close does not touch the camera, the next frame's GCamera::Update in mode 0 draws
	// it again (ChangeFov 0x4425D3 with its FOV, UpdateCamera 0x442622 with its zoomers, which mode 2 left alone)
	if (_hooks.applyCamera)
	{
		_hooks.applyCamera(std::nullopt);
	}
	// 0x5264E8..0x5264F4: fn_0081E1F0(+0x10): the model light put back
	if (_hooks.setLight)
	{
		_hooks.setLight(_keptLight);
	}
	// 0x5264F9..0x52651F: the records, the sprites (LH3DSprite::Release) and the burst freed
	_queued.fill(false);
	_bursts.clear();
	_sparksOn = false;
}

void FallingSpell::UpdateCamera(int32_t filmMs)
{
	if (!_active || !_path.has_value())
	{
		return;
	}
	// 0x526E9B..0x526F1C: fn_0086D760(path, NULL, t, &position, &focus, &matrix), ChangeFov(pi / 4), position and focus
	// x 0.8 ([0x8C4A04]), fn_00819F50(&position, &focus, &matrix). Init 0x526259..0x5262BF does the same at the film's
	// first frame without ChangeFov; here both come in the first frame (aproximado: see FrameUpdate)
	const auto key = _path->At(static_cast<uint32_t>(filmMs));
	_camera = Camera {key.position * k_PathScale, key.focus * k_PathScale, key.matrix, k_FallFov};
	if (_hooks.applyCamera)
	{
		_hooks.applyCamera(_camera);
	}
}

void FallingSpell::Draw(uint32_t deltaMs, int width, int height)
{
	_queued.fill(false);
	// 0x5267D6: nothing when +0x00 is 0 (and 0x5267F4: nothing without a film, which ends the spell anyway)
	if (!_active)
	{
		return;
	}
	// 0x526873..0x526895: fn_0081E1F0(0, 0, 1000) for the creature (not ported) drawn just after
	if (_hooks.setLight)
	{
		_hooks.setLight(k_CreatureLight);
	}
	// 0x526BFD..0x526C04: the sparks only while +0x1C is set
	if (!_sparksOn)
	{
		return;
	}
	const auto lens = LensOf(width, height);
	// 0x526C0A..0x526C20: g_delta_time x 0.0013 ([0x8D8C08])
	const float dt = static_cast<float>(deltaMs) * 0.0013f;
	bool seen = false;
	for (int i = 0; i < k_SparkCount; ++i)
	{
		auto& spark = _sparks.at(static_cast<size_t>(i));
		auto& sprite = _sprites.at(static_cast<size_t>(i));
		// 0x526C27..0x526C49: the alpha 255 - 32 age ([0x8CF134]) of the age before this frame's step
		int32_t alpha = Ftol(255.0f - spark.age * 32.0f);
		spark.age += dt;
		// 0x526C4C: none left: not drawn
		if (alpha <= 0)
		{
			continue;
		}
		if (alpha >= 0xFF)
		{
			alpha = 0xFF;
		}
		// 0x526C5E..0x526C6D: the colour +0x20 = rgb + alpha << 24; 0x526C7F: seen
		sprite.argb = spark.rgb + (static_cast<uint32_t>(alpha) << 24);
		seen = true;
		// 0x526C71..0x526CB0: the half width ((size - shrink age 0.5) + 1) 0.75 ([0x8AB274]), at least 1e-4 ([0x8BF518])
		float half = ((spark.size - spark.shrink * spark.age * 0.5f) + 1.0f) * 0.75f;
		if (half < 0.0001f)
		{
			half = 0.0001f;
		}
		// 0x526CB6..0x526CCB: the origin +0x18 / +0x1C times half / size, then +0x0C = half
		const float ratio = half / sprite.size;
		sprite.origin *= ratio;
		sprite.size = half;
		// 0x526CCE..0x526CE3: the angle +0x14 = spin age + i (fiadd of the counter)
		sprite.angle = spark.spin * spark.age + static_cast<float>(i);
		// 0x526CE7..0x526D09: the cell ftol(age x 8) & 15 ([0x8C2C70])
		sprite.cell = static_cast<uint8_t>(Ftol(spark.age * 8.0f) & 0xF);
		// 0x526D0C..0x526D65: Get3DPointFromScreen((ftol(W x), ftol(H y)), depth) with W [0xE85058], H [0xE8505A]
		const int32_t sx = Ftol(static_cast<float>(width) * spark.x);
		const int32_t sy = Ftol(static_cast<float>(height) * spark.y);
		sprite.position = screen_point::CameraPointFromScreen(lens, sx, sy, spark.depth);
		// 0x526D6A..0x526D94: y += (spin + 1) (1 - y) dt 0.1 ([0x8AB22C])
		spark.y += (spark.spin + 1.0f) * (1.0f - spark.y) * dt * 0.1f;
		// 0x526D97..0x526DA6: the material [0xEA1ABC] (smoke.raw, mode 6) and LH3DSprite::AddDrawing 0x840C70
		_queued.at(static_cast<size_t>(i)) = true;
	}
	// 0x526DC6..0x526DCA: +0x1C = a spark was seen
	_sparksOn = seen;
}

void FallingSpell::FinishFrame(uint32_t deltaMs, int32_t state, int width, int height)
{
	_bursts.clear();
	// 0x526484 (the callback's NULL test), 0x526536..0x52653A: from state 2 only
	if (!_active || state < 2)
	{
		return;
	}
	// 0x526546..0x5265F7: the creature's centre on the screen; nothing (and no step) when fn_008190D0 gives 0 (at or
	// before the near plane)
	const auto centre = _hooks.burstCentre ? _hooks.burstCentre(width, height) : std::nullopt;
	if (!centre.has_value())
	{
		return;
	}
	// 0x526540..0x52654F: g_delta_time x 0.001; 0x5265FD..0x52666A: d = 0.7 dt ([0x8AB238]); +0x2C += 0.1 d, the alpha
	// min(ftol(255 +0x2C), 255); +0x28 += 0.25 d, and past 0.5 +0x30 += 5 d ([0x8AB6E4])
	const float d = static_cast<float>(deltaMs) * 0.001f * 0.7f;
	_burstAlpha = d * 0.1f + _burstAlpha;
	int32_t alpha = Ftol(_burstAlpha * 255.0f);
	if (alpha > 0xFF)
	{
		alpha = 0xFF;
	}
	_burstShape = d * 0.25f + _burstShape;
	if (_burstShape > 0.5f)
	{
		_burstGrow = d * 5.0f + _burstGrow;
	}
	const auto a = static_cast<uint32_t>(alpha) << 24;
	const float x = static_cast<float>(centre->x);
	const float y = static_cast<float>(centre->y);
	const float g = _burstGrow;
	// (the depth [0xE839E0] x 1.1, 0x52666C..0x526676, only places them for the Z)
	// 0x526681..0x5266CC: 10 g^3 ([0x8AB414]), a5 = +0x2C, the colour bytes 0x40, 0xFF, 0x20 (B, G, R)
	_bursts.push_back(DrawBurst(_burst, x, y, g * g * g * 10.0f, _burstAlpha, _burstShape, a | 0x0020FF40u));
	// 0x5266D1..0x526717: 25 g ([0x8C7BD0]), a5 = -+0x2C, 0xFF, 0x40, 0xFF
	_bursts.push_back(DrawBurst(_burst, x, y, g * 25.0f, -_burstAlpha, _burstShape, a | 0x00FF40FFu));
	// 0x52671C..0x526766: 50 g ([0x8C6CA4]), a5 = -2 +0x2C ([0x8C7CE0]), 0xFF, 0x80, 0x40
	_bursts.push_back(DrawBurst(_burst, x, y, g * 50.0f, _burstAlpha * -2.0f, _burstShape, a | 0x004080FFu));
	// 0x52676B..0x5267B7: 75 g^2 ([0x8D8C04]), a5 = 2 +0x2C, 0x40, 0x40, 0xFF
	_bursts.push_back(DrawBurst(_burst, x, y, g * g * 75.0f, _burstAlpha + _burstAlpha, _burstShape, a | 0x00FF4040u));
}

std::vector<std::array<ScreenVertex, 4>> FallingSpell::SparkQuads(int width, int height, float nearZ) const
{
	std::vector<std::array<ScreenVertex, 4>> quads;
	if (!_active || width <= 0 || height <= 0)
	{
		return quads;
	}
	const auto lens = LensOf(width, height);
	// LH3DSprite::AddDrawing 0x840C70: the Z-sorter's key |pos - g_camera|^2, far to near (NewZObject 0x83F310).
	// (aproximado) the key from the camera space position (the original's world sum can differ in the last bit)
	graphics::zsorter::Queue<int> queue;
	queue.Begin();
	const graphics::billboard::CameraFrame frame; // the camera at the origin, looking down +z (camera space)
	for (int i = 0; i < k_SparkCount; ++i)
	{
		if (_queued.at(static_cast<size_t>(i)))
		{
			queue.Submit(i, graphics::zsorter::Key(_sprites.at(static_cast<size_t>(i)).position, glm::vec3(0.0f)));
		}
	}
	for (const auto& entry : queue.Drain())
	{
		const auto& sprite = _sprites.at(static_cast<size_t>(*entry.item));
		// LH3DSprite::Draw 0x84055D..0x840585: nothing at or before the near plane
		if (!(sprite.position.z > nearZ))
		{
			continue;
		}
		// mode A (0x84071D..0x8408CF): billboard::Screen in camera space, then the projection
		const auto quad = graphics::billboard::Screen(sprite, frame);
		std::array<ScreenVertex, 4> out {};
		for (size_t k = 0; k < out.size(); ++k)
		{
			out.at(k) = {screen_point::ProjectCamera(lens, quad.corners.at(k)), quad.uv.at(k), sprite.argb};
		}
		quads.push_back(out);
	}
	return quads;
}

FallingSpell& falling_spell::Get()
{
	static FallingSpell spell([]() {
		FallingSpell::Hooks hooks;
		// (pendiente: the creature) openblack has no CreatureFalling, so no centre. OPENBLACK_TEST_FALL_BURST_AT="fx,fy"
		// (openblack test hook) puts it at that fraction of the screen, to see the bursts
		hooks.burstCentre = [](int width, int height) -> std::optional<glm::ivec2> {
			const char* at = std::getenv("OPENBLACK_TEST_FALL_BURST_AT");
			glm::vec2 fraction(0.5f);
			if (at == nullptr || std::sscanf(at, "%f,%f", &fraction.x, &fraction.y) != 2)
			{
				return std::nullopt;
			}
			return glm::ivec2(glm::vec2(static_cast<float>(width), static_cast<float>(height)) * fraction);
		};
		hooks.light = []() { return model_light::Light(); };
		hooks.setLight = [](const glm::vec3& position) { model_light::SetLight(position); };
		// fn_00819F50 and ChangeFov 0x8195B0 on openblack's camera. ChangeFov takes the horizontal angle (0x4424C6..
		// 0x4424E2: [0xC3812C] = tan(fov / 2) near, [0xC38130] = that / aspect), as the config's cameraXFov; the
		// config keeps GCamera's own FOV, which the clear puts back. (inferido) the near plane stays openblack's
		// (GetNearClipping 0x4424AF runs in every mode, from GCamera's camera). Not ported: the debug camera overrides
		// [0xEA9EC8] / [0xEA9ECC] and the shake fn_008210C0 (0x819FAA..0x81A032) on the fall's camera, and the readers
		// of g_camera in mode 2 (GCamera::Update 0x442347..0x442395 takes it as its drawn camera: GetWeatherSmooth
		// 0x4426BA, +0x74)
		hooks.applyCamera = [](const std::optional<Camera>& fall) {
			if (!Locator::camera::has_value() || !Locator::config::has_value() || !Locator::windowing::has_value())
			{
				return;
			}
			auto& camera = Locator::camera::value();
			const auto& config = Locator::config::value();
			const float xFov = fall.has_value() ? glm::degrees(fall->fov) : config.cameraXFov;
			camera.SetProjectionMatrixPerspective(xFov, Locator::windowing::value().GetAspectRatio(), config.cameraNearClip,
			                                      config.cameraFarClip);
			// the drawn view, g_camera and the world to camera of fn_00819F50, over the game camera's look-at without
			// touching its zoomers
			camera.SetDrawnView(fall.has_value() ? std::optional(WorldToCamera(*fall)) : std::nullopt);
		};
		return hooks;
	}());
	return spell;
}

namespace
{
// FallingSpell::Init 0x52607A..0x5260A2: LHFileLength / LHLoadData("data\spells\fall\fall.cm2" 0xBE9C78)
std::optional<CameraPath> LoadFallPath()
{
	if (!Locator::filesystem::has_value())
	{
		return std::nullopt;
	}
	try
	{
		return CameraPath::Parse(Locator::filesystem::value().ReadAll("Data/Spells/fall/fall.cm2"));
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
}
} // namespace

void falling_spell::FrameUpdate()
{
	auto& film = video::GetFallingSpell();
	auto& spell = Get();
	static bool s_sparksSeen = false;
	if (!film.IsActive())
	{
		// EndFallingSpellVideo 0x553A10 -> FallingSpell::Close 0x5264A0
		spell.Close();
		return;
	}
	// (aproximado) a KickOff over a running one (EndFallingSpellVideo 0x5539C4, then a new Init) is only seen once the
	// old one had its sparks: the film's +0x1C is back to 0
	if (!spell.IsActive() || (s_sparksSeen && !film.SparklesOn()))
	{
		// KickOffFallingSpellVideo 0x5539A0 -> FallingSpell::Init 0x526060. (aproximado) here at the first frame of the
		// film, not in the turn of CHL 203: the CRT and local random draws come a little later in their streams
		spell.Init(LoadFallPath());
		s_sparksSeen = false;
	}
	// the update 0x526E00's part: +0x1C = 1 once with the state 0 -> 1 (0x527047), the camera with the film's ms
	if (film.SparklesOn() && !s_sparksSeen)
	{
		spell.StartSparks();
		s_sparksSeen = true;
	}
	spell.UpdateCamera(film.LastMs());
	glm::ivec2 size(0);
	if (Locator::windowing::has_value())
	{
		size = Locator::windowing::value().GetSize();
	}
	const uint32_t deltaMs = game_clock::FrameRealMs();
	// Process3dEngine case 2 0x54DDE0: FallingSpell::Draw with g_delta_time; FinishFrame's callback 0x526480 after it
	spell.Draw(deltaMs, size.x, size.y);
	spell.FinishFrame(deltaMs, film.State(), size.x, size.y);
	// (openblack test hook) OPENBLACK_TEST_FALL_LOG=1: one line a second of film, to place the screenshots in time
	static int32_t s_loggedSecond = -1;
	if (std::getenv("OPENBLACK_TEST_FALL_LOG") != nullptr && film.LastMs() / 1000 != s_loggedSecond)
	{
		s_loggedSecond = film.LastMs() / 1000;
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "FallingSpell: film {} ms, state {}, sparks {} ({} quads), bursts {}",
		                   film.LastMs(), film.State(), spell.SparksOn(), spell.SparkQuads(size.x, size.y, 0.0f).size(),
		                   spell.Bursts().size());
	}
}
