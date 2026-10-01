/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LightningFlash.h"

#include <cmath>

#include <algorithm>

using namespace openblack;
using namespace openblack::weather;

void flash::Start(storms::Storm::Flash& flash, const glm::vec3& position, float radius, float intensity)
{
	flash.intensity = intensity;
	flash.age = 0.0f;
	flash.f1 = 0.0f;
	flash.f3 = 0.0f;
	flash.active = true;
	flash.position = position;
	flash.radius = radius;
}

void flash::Age(storms::Storm::Flash& flash, float seconds)
{
	flash.age += seconds;
	// 0x8372D7: `dec eax; jne` — only an active (== 1) flash is tested; test ah, 0x41 -> off when age > 0.8
	if (flash.active && flash.age > 0.8f)
	{
		flash.active = false;
	}
}

float flash::Frame(storms::Storm::Flash& flash)
{
	if (!flash.active)
	{
		flash.f1 = 0.0f;
		flash.f3 = 0.0f;
		return 0.0f;
	}
	const float s = 1.0f - flash.age;
	flash.f1 = s;
	flash.f3 = s * s * s;
	// 0x837221: age < 0.5 and not <= 0.2 -> the dip
	if (flash.age < 0.5f && flash.age > 0.2f)
	{
		flash.f3 = 0.1f;
		flash.f1 = 0.1f;
	}
	flash.f3 *= flash.intensity;
	flash.f1 *= flash.intensity;
	// fn_0086CFF0(pos, 0xED92F0, [0xEDC2F0] = 64, 1, f3, 1, 0): the land light stamp. (pendiente) openblack has no
	// dynamic land light texture (the light maps are drawn as ground quads, PSys/Creators/LightMap.h): not drawn
	return flash.f3;
}

uint8_t flash::BitmapTexel(int x, int y)
{
	const float d = std::sqrt(static_cast<float>(x * x + y * y));
	if (!(d < 32.0f))
	{
		return 0;
	}
	const int value = static_cast<int>((32.0f - d) * 9.0f);
	return static_cast<uint8_t>(std::min(value, 255));
}

uint8_t flash::AtCamera(const glm::vec3& camera)
{
	const storms::Storm* nearest = nullptr;
	float best = 0.0f;
	bool inside = false;
	storms::ForEach([&](const storms::Storm& storm) {
		if (storm.deleteCounter != 0)
		{
			return;
		}
		const float dx = storm.descriptor.position.x - camera.x;
		const float dz = storm.descriptor.position.z - camera.z;
		const float d2 = dx * dx + dz * dz;
		// 0x835803: the first one, or a strictly nearer one (test ah, 1: d2 < best)
		if (nearest != nullptr && !(d2 < best))
		{
			return;
		}
		best = d2;
		const float m = (storm.descriptor.outerRadius + storm.descriptor.innerRadius) * 0.5f;
		if (d2 < m * m)
		{
			inside = true;
		}
		nearest = &storm;
	});
	if (nearest == nullptr || !inside)
	{
		return 0;
	}
	const float f = std::clamp(nearest->flash.f1, 0.0f, 1.0f);
	return static_cast<uint8_t>(static_cast<int>(f * 255.0f));
}

void flash::UpdateFrame()
{
	storms::ForEachMutable([](storms::Storm& storm) {
		if (storm.deleteCounter == 0)
		{
			Frame(storm.flash);
		}
	});
}

uint8_t weather::LightningFlashAtCamera(const glm::vec3& camera)
{
	// 0x83587C..0x835903 with this frame's f1 (flash::UpdateFrame)
	return flash::AtCamera(camera);
}
