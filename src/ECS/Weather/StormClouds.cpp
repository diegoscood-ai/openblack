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

#include "3D/FrameAnim.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/DetailLevel.h"
#include "Graphics/Haze.h"
#include "Graphics/Mists.h"
#include "Locator.h"
#include "Storms.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;

namespace
{
std::unordered_map<storms::StormId, std::vector<storm_clouds::Puff>> g_Puffs;

/// ?Random@@YAMMM@Z 0x81D180 (GWeather::DrawClouds): the CRT rand (game_random::crt)
float Random(float a, float b)
{
	return game_random::crt::Random(a, b);
}

/// 0x83FFEE..0x840027: fn_007FEB30(pos, specular, &colour) (graphics::haze::ApplyObject) with the specular a grey
/// min(255, ftol(flash f3 [storm +0x8C] x 127 [0x8C4A00])) in all four bytes; it darkens the colour and returns the
/// specular SetColorSpecular puts in +0x50 (0x84002F..0x840039), drawn by the mists' effect branch (fn_0080DB30 0x80DEF5)
uint32_t Haze(uint32_t& argb, const glm::vec3& position, float flash)
{
	const auto grey = std::min(static_cast<uint32_t>(static_cast<int32_t>(flash * 127.0f)), 0xFFu);
	const uint32_t specular = grey << 24 | grey << 16 | grey << 8 | grey;
	if (!Locator::camera::has_value())
	{
		return specular;
	}
	const auto view = Locator::camera::value().GetViewMatrix(Camera::Interpolation::Current);
	return graphics::haze::ApplyObject(graphics::haze::Frame(), graphics::haze::Depth(view, position), specular, &argb);
}

/// sstorm.raw (40 x 40 grey, 0x640 bytes) that fn_00835AD0 0x835E15 loads into 0xEE9D3C: the storms' land shadow
const std::vector<uint8_t>& StormShadowImage()
{
	static const std::vector<uint8_t> k_Image = [] {
		std::vector<uint8_t> image;
		if (!Locator::filesystem::has_value())
		{
			return image;
		}
		try
		{
			auto& fileSystem = Locator::filesystem::value();
			image = fileSystem.ReadAll(fileSystem.FindPath(fileSystem.GetPath<filesystem::Path::Textures>() / "sstorm.raw"));
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "No storm shadows (sstorm.raw): {}", e.what());
		}
		return image;
	}();
	return k_Image;
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
			uint32_t colour = PuffColour(base, d.blackness, storm.fade);
			const uint32_t specular = Haze(colour, position, storm.flash.f3);
			// vt 0x100 (AddDrawing) only above alpha 5
			if ((colour >> 24u) > 5u)
			{
				// fn_007FA300: the mist's counter += ftol(g_game_time_inc x 0.255), modulo 900, run only for a mist that
				// AddDrawing 0x7FA7F0 sent to the Z-sorter (its sphere on screen: mists::InView); the fraction kept as
				// the map mists do (frame_anim::MistAdvance)
				if (mists::InView(position, spread * puff.size))
				{
					graphics::frame_anim::MistClock clock {puff.counter, puff.counterRemainder};
					graphics::frame_anim::MistAdvance(clock, milliseconds);
					puff.counter = clock.counter;
					puff.counterRemainder = clock.remainder;
				}
				mists::MistDesc mist {};
				mist.position = position;
				mist.size = spread * puff.size; // +0x88
				mist.colour = colour;
				mist.edgeShrink = true;
				mist.k = puff.k;
				mist.counter = puff.counter;
				mist.specular = specular;
				mists::Submit(mist);
			}
		}
		// 0x840069..0x8400C6: the storm's shadow, s = (blackness + 0.7 [0x8AB238]) x fade; at 1 or more stamped with 1,
		// else only above 0.01 ([0x8C5840]); with "CloudShadows" ([0xC381F4]): fn_0086CFF0(+0xA0, 0xEE9D3C, 40, 1, s, 2, 0)
		float shadow = (d.blackness + 0.7f) * storm.fade;
		const bool cloudShadows = Locator::config::has_value() && graphics::GetDetailLevel(Locator::config::value().detailLevel).clouds;
		if (shadow > 1.0f)
		{
			shadow = 1.0f;
		}
		const auto& image = StormShadowImage();
		if (shadow > 0.01f && cloudShadows && image.size() == 40u * 40u)
		{
			land_light::AddStamp(storm.drawPosition, image.data(), 40, true, shadow, 2);
		}
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
