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
#include <vector>

#include "3D/LandColourStamps.h"
#include "3D/LandLight.h"
#include "3D/Lightning.h"

using namespace openblack;
using namespace openblack::weather;

void flash::Start(storms::Storm::Flash& flash, const glm::vec3& position, float radius, float intensity)
{
	static_cast<lightning::Flash&>(flash) = lightning::Strike(position, radius, intensity);
	flash.f1 = 0.0f;
	flash.f3 = 0.0f;
}

void flash::Age(storms::Storm::Flash& flash, float seconds)
{
	static_cast<lightning::Flash&>(flash) = lightning::Advance(flash, seconds);
}

float flash::Frame(storms::Storm::Flash& flash)
{
	if (!flash.active)
	{
		flash.f1 = 0.0f;
		flash.f3 = 0.0f;
		return 0.0f;
	}
	flash.f1 = lightning::Brightness(flash);
	flash.f3 = lightning::GlowStrength(flash);
	// the land light stamp, centred, 64 wide, mode 1 (land_light::AddStamp), of the flash's glow
	static const std::vector<uint8_t> k_Bitmap = land_colour_stamps::LightningImage();
	land_light::AddStamp(flash.position, k_Bitmap.data(), land_colour_stamps::k_LightningSide, true, flash.f3, 1);
	return flash.f3;
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
		// the first one, or a strictly nearer one (d2 < best)
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
	return lightning::LandLightFlash(nearest->flash.f1);
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
	// with this frame's f1 (flash::UpdateFrame)
	return flash::AtCamera(camera);
}
