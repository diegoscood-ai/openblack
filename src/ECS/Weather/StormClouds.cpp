/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormClouds.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/Mists.h"
#include "Locator.h"
#include "Storms.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
std::unordered_map<storms::StormId, std::vector<storm_clouds::Puff>> g_Puffs;

/// ?Random@@YAMMM@Z 0x81D180: a + (b - a) x rand() x (1 / 32767) (0x9A3700) on the CRT generator. (aproximado) the
/// original's rand() is the program's shared CRT sequence; this is a private generator of the same kind (MSVC's LCG)
uint32_t g_Rand = 1;
float Random(float a, float b)
{
	g_Rand = g_Rand * 214013u + 2531011u;
	const auto r = static_cast<float>((g_Rand >> 16u) & 0x7FFFu);
	return r * 3.05185e-05f * (b - a) + a;
}

/// fn_007FEB30 on the colour (the "light" argument): with the haze on and the view depth z >= near, each RGB byte x
/// (256 - trunc((256 - k) t)) >> 8 with t = clamp((z - near) / (far - near), 0, 1); the specular it returns (the haze
/// colour x t) is not drawn by the mists' effect branch (fn_007FA300)
uint32_t Haze(uint32_t argb, const glm::vec3& position)
{
	if (!Locator::config::has_value() || !graphics::GetDetailLevel(Locator::config::value().detailLevel).fog || !Locator::camera::has_value())
	{
		return argb;
	}
	const auto& haze = LandLightTable::Current().GetHaze();
	const auto view = Locator::camera::value().GetViewMatrix(Camera::Interpolation::Current);
	const float depth = (view * glm::vec4(position, 1.0f)).z;
	if (depth < haze.nearDistance)
	{
		return argb;
	}
	const float t = std::clamp((depth - haze.nearDistance) / (haze.farDistance - haze.nearDistance), 0.0f, 1.0f);
	const auto f = static_cast<uint32_t>(256.0f - std::trunc((256.0f - haze.k) * t));
	uint32_t out = argb & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		out |= ((((argb >> shift) & 0xFFu) * f) >> 8u) << shift;
	}
	return out;
}
} // namespace

uint32_t storm_clouds::PuffColour(uint32_t baseArgb, float blackness, float fade)
{
	uint32_t rgb = baseArgb & 0x00FFFFFFu;
	if (blackness > 0.0f)
	{
		// 0x83FF70: each byte x (1 - blackness x 0.5) (0x8AA3B4), ftol
		const float mul = 1.0f - blackness * 0.5f;
		rgb = 0;
		for (const uint32_t shift : {16u, 8u, 0u})
		{
			const auto c = static_cast<uint32_t>(static_cast<int>(static_cast<float>((baseArgb >> shift) & 0xFFu) * mul)) & 0xFFu;
			rgb |= c << shift;
		}
	}
	// 0x83FFCC: alpha = ftol(fade x 0.75 (0x8AB274) x the base's alpha byte)
	const auto alpha = static_cast<uint32_t>(static_cast<int>(fade * 0.75f * static_cast<float>(baseArgb >> 24u))) & 0xFFu;
	return (alpha << 24u) | rgb;
}

namespace
{
/// OPENBLACK_TEST_STORM_CLOUDS="x,z,radius[,clouds[,blackness[,elevation]]]": a storm made as LH3DStorm's ctor
/// (fn_0083F3F0: 8 clouds, blackness 0.5, elevation 160, inner 100, outer 300) with that radius as its inner one and 3 x
/// it as the outer one, a fade of 1 s, an almost endless life and rain 100, the first frame a land exists (a test helper,
/// not in the original: the climate storms and the weather things make such storms in a game)
void RunDebugHook()
{
	static bool done = false;
	const char* text = std::getenv("OPENBLACK_TEST_STORM_CLOUDS");
	if (done || text == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	done = true;
	float x = 0.0f;
	float z = 0.0f;
	float radius = 100.0f;
	int clouds = 8;
	float blackness = 0.5f;
	float elevation = 160.0f;
	if (std::sscanf(text, "%f,%f,%f,%d,%f,%f", &x, &z, &radius, &clouds, &blackness, &elevation) < 3)
	{
		return;
	}
	storms::StormDescriptor d;
	d.position = glm::vec3(x, LandHeightAt(x, z), z);
	d.innerRadius = radius;
	d.outerRadius = radius * 3.0f;
	d.fadeInTime = 1.0f;
	d.lifeTime = 1e9f;
	d.numClouds = clouds;
	d.blackness = blackness;
	d.elevation = elevation;
	const auto id = storms::Create(d);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm clouds test: storm {} at ({:.1f}, {:.1f}) radius {:.0f}/{:.0f}, {} clouds, "
	                   "blackness {:.2f}, elevation {:.0f}", id, x, z, d.innerRadius, d.outerRadius, clouds, blackness, elevation);
}
} // namespace

void storm_clouds::DrawFrame(float milliseconds)
{
	RunDebugHook();
	// the storms that went take their puffs with them (fn_0083F630 deletes the mists)
	std::erase_if(g_Puffs, [](const auto& entry) {
		bool alive = false;
		storms::ForEach([&](const storms::Storm& storm) { alive = alive || storm.id == entry.first; });
		return !alive;
	});
	const uint32_t base = LandLightTable::Current().GetRawBase();
	storms::ForEachMutable([&](storms::Storm& storm) {
		auto& d = storm.descriptor;
		// fn_0083F8B0 calls every storm's DrawClouds, marked or not
		if (d.numClouds <= 0)
		{
			return;
		}
		if (d.numClouds > 16) // 0x83FCA4: the count itself is capped
		{
			d.numClouds = 16;
		}
		auto& puffs = g_Puffs[storm.id];
		// one new puff a frame up to the count: an LH3DMist (type 7) in the effect branch (+0x80 |= 2), k Random(2.5,
		// 5); size 2 x Random(0.01, 0.015); at (Random(-1, 1), Random(-10, 10) + elevation, Random(-1, 1))
		if (static_cast<int>(puffs.size()) < d.numClouds)
		{
			Puff puff;
			puff.k = Random(2.5f, 5.0f);
			puff.size = Random(0.01f, 0.015f) * 2.0f;
			puff.offset.x = Random(-1.0f, 1.0f);
			puff.offset.y = Random(-10.0f, 10.0f) + d.elevation;
			puff.offset.z = Random(-1.0f, 1.0f);
			puff.frames = 0;
			puffs.push_back(puff);
		}
		const float spread = storm.outerRadius + storm.innerRadius; // +0xB0 + +0xAC
		for (auto& puff : puffs)
		{
			// a new target every 400 frames (0x190), reached in steps of 0.0025 of the way (0x9A3B18). (aproximado:
			// counted in frames, as the original; openblack's frame rate is not the original's)
			if (puff.frames != 0)
			{
				--puff.frames;
				puff.offset += puff.step;
			}
			else
			{
				puff.target.x = Random(-1.0f, 1.0f);
				puff.target.y = Random(0.0f, 20.0f) + d.elevation;
				puff.target.z = Random(-1.0f, 1.0f);
				puff.step = (puff.target - puff.offset) * 0.0025f;
				puff.frames = 400;
			}
			glm::vec3 position;
			position.x = spread * puff.offset.x * 0.5f + storm.drawPosition.x;
			position.z = spread * puff.offset.z * 0.5f + storm.drawPosition.z;
			position.y = LandHeightAt(position.x, position.z) + puff.offset.y;
			const uint32_t colour = Haze(PuffColour(base, d.blackness, storm.fade), position);
			// fn_007FA300: the mist's counter += ftol(g_game_time_inc x 0.255) while drawn, modulo 900. (aproximado)
			// advanced for every drawn puff here (mists::Submit culls without telling); the fraction kept as the map
			// mists do
			puff.counterRemainder += milliseconds * 0.255f;
			const int step = static_cast<int>(puff.counterRemainder);
			puff.counterRemainder -= static_cast<float>(step);
			puff.counter += step;
			if (puff.counter > 900)
			{
				puff.counter %= 900;
			}
			// vt 0x100 (AddDrawing) only above alpha 5
			if ((colour >> 24u) > 5u)
			{
				mists::MistDesc mist {};
				mist.position = position;
				mist.size = spread * puff.size; // +0x88
				mist.colour = colour;
				mist.edgeShrink = true;
				mist.k = puff.k;
				mist.counter = puff.counter;
				mists::Submit(mist);
			}
		}
		// 0x840069: the storm's shadow, s = (blackness + 0.7) x fade capped at 1, stamped above 0.01 with the bitmap
		// 0xEE9D3C (40, mode 2) by fn_0086CFF0. (pendiente) no dynamic land light texture in openblack: not drawn
	});
}

void storm_clouds::Clear()
{
	g_Puffs.clear();
}

size_t storm_clouds::PuffCount(uint32_t stormId)
{
	const auto it = g_Puffs.find(stormId);
	return it == g_Puffs.end() ? 0 : it->second.size();
}
