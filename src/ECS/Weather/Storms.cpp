/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Storms.h"

#include <cmath>

#include <list>

#include "WeatherLand.h"

using namespace openblack::weather;
using namespace openblack::weather::storms;

namespace
{
/// 0xEEA37C, newest first (the ctors put the new one at the head, +0x3BC is the next)
std::list<Storm> g_storms;
StormId g_nextId = 1;
LightningCallback g_forkCallback;
LightningCallback g_sheetCallback;

/// 0xEEA380 = 0x5E1CE0 (set by GLandAlignement::Open): min + GameFloatRand(max - min)
float RandomRange(float min, float max)
{
	return GameFloatRand(max - min) + min;
}

/// GWeather::Update 0x83F900
void Update(Storm& storm, float seconds)
{
	auto& d = storm.descriptor;
	storm.age += seconds;
	if (!(storm.age < d.lifeTime))
	{
		MarkForDeletion(storm.id);
		return;
	}
	// fade in and out over fadeInTime, the inner radius with it
	if (storm.age < d.fadeInTime)
	{
		storm.fade = storm.age * d.strength / d.fadeInTime;
		storm.innerRadius = storm.age * d.innerRadius / d.fadeInTime;
	}
	else if (d.lifeTime - d.fadeInTime < storm.age)
	{
		const float left = d.lifeTime - storm.age;
		storm.fade = left * d.strength / d.fadeInTime;
		storm.innerRadius = left * d.innerRadius / d.fadeInTime;
	}
	else
	{
		storm.fade = d.strength;
		storm.innerRadius = d.innerRadius;
	}

	// towards the target at `speed` metres per second (2D; the height stays)
	storm.drawPosition = d.position;
	if (storm.speed != 0.0f)
	{
		const float step = seconds * storm.speed;
		const float dx = storm.drawPosition.x - storm.target.x;
		const float dz = storm.drawPosition.z - storm.target.z;
		const float distance = std::sqrt(dx * dx + dz * dz);
		if (distance > 0.001f)
		{
			const float f = step < distance ? step / distance : 1.0f;
			storm.drawPosition.x += (storm.target.x - storm.drawPosition.x) * f;
			storm.drawPosition.z += (storm.target.z - storm.drawPosition.z) * f;
			d.position = storm.drawPosition;
			storm.arrived = false;
		}
		else
		{
			storm.drawPosition = storm.target;
			d.position = storm.target;
			storm.arrived = true;
		}
	}

	storm.outerRadius = d.outerRadius;
	// ftol(snow x snowCoverRate x fade): with SnowCover (0xEDC344) fn_0086C2D0 lays snow in the inner/outer radius at
	// amount x seconds x 0.03. SnowCover is not ported.

	// the lightning, once faded in (and while the engine's random callback exists, always in a game)
	if (storm.age > d.fadeInTime)
	{
		const glm::vec3 point(d.position.x, LandHeightAt(d.position.x, d.position.z) + d.elevation, d.position.z);
		if (d.forkMax != 0.0f)
		{
			storm.forkTimer -= seconds;
			if (storm.forkTimer <= 0.0f)
			{
				storm.forkTimer = RandomRange(d.forkMin, d.forkMax);
				// fn_00837290(+0x70, pos, outer, 0.5): the light flash (not ported)
				if (g_forkCallback)
				{
					g_forkCallback(storm, point, d.outerRadius);
				}
			}
		}
		if (d.sheetMax != 0.0f)
		{
			storm.sheetTimer -= seconds;
			if (storm.sheetTimer <= 0.0f)
			{
				storm.sheetTimer = RandomRange(d.sheetMin, d.sheetMax);
				// fn_00837290(+0x70, pos, outer, 1.0): the flash (not ported); then the thunder
				if (g_sheetCallback)
				{
					g_sheetCallback(storm, point, d.outerRadius);
				}
			}
		}
	}
	// fn_008372D0(+0x70, seconds): the flash object's own step (not ported)
}
} // namespace

StormId storms::Create(const StormDescriptor& descriptor)
{
	// fn_0083F590: the descriptor, the position as the draw position and the target, speed 1, age and timers 0
	Storm storm;
	storm.id = g_nextId++;
	storm.descriptor = descriptor;
	storm.drawPosition = descriptor.position;
	storm.target = descriptor.position;
	storm.speed = 1.0f;
	storm.arrived = true;
	g_storms.push_front(storm);
	return storm.id;
}

Storm* storms::Find(StormId id)
{
	if (id == k_NoStorm)
	{
		return nullptr;
	}
	for (auto& storm : g_storms)
	{
		if (storm.id == id)
		{
			return storm.deleteCounter == 0 ? &storm : nullptr;
		}
	}
	return nullptr;
}

void storms::MarkForDeletion(StormId id)
{
	for (auto& storm : g_storms)
	{
		if (storm.id == id && storm.deleteCounter == 0)
		{
			storm.deleteCounter = 1;
		}
	}
}

void storms::Destroy(StormId id)
{
	g_storms.remove_if([id](const Storm& storm) { return storm.id == id; });
}

void storms::KillStormsInArea(const glm::vec3& position, float radius)
{
	for (auto& storm : g_storms)
	{
		const float dx = storm.drawPosition.x - position.x;
		const float dz = storm.drawPosition.z - position.z;
		if (radius + storm.descriptor.outerRadius > std::sqrt(dx * dx + dz * dz))
		{
			MarkForDeletion(storm.id);
		}
	}
}

void storms::UpdateAll(float seconds)
{
	for (auto it = g_storms.begin(); it != g_storms.end();)
	{
		Update(*it, seconds);
		if (it->deleteCounter != 0 && ++it->deleteCounter > 2)
		{
			it = g_storms.erase(it); // vt 0x10, the deleting destructor (fn_0083F630 unlinks it)
			continue;
		}
		++it;
	}
}

void storms::CalcAtmos(const Storm& storm, const glm::vec3& point, WeatherInfo& weather)
{
	const float r = storm.outerRadius;
	const auto& centre = storm.drawPosition;
	if (!(centre.x - r <= point.x) || centre.x + r < point.x || !(centre.z - r <= point.z) || centre.z + r < point.z)
	{
		return;
	}
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	const float d2 = dx * dx + dz * dz;
	if (d2 > r * r)
	{
		return;
	}
	const float inner = storm.innerRadius;
	const float f = d2 <= inner * inner ? 1.0f : 1.0f - (std::sqrt(d2) - inner) / (r - inner);
	const auto w = static_cast<int32_t>(f * storm.fade * 256.0f);
	if (w == 0)
	{
		return;
	}
	const auto& s = storm.descriptor.weather;
	// the temperature moves towards the storm's (a wrapping byte), the rest is added
	weather.temperature = LerpByte(weather.temperature, s.temperature, w);
	weather.rain = ClampAdd(weather.rain, (s.rain * w) >> 8);
	weather.snow = ClampAdd(weather.snow, (s.snow * w) >> 8);
	weather.overcast = ClampAdd(weather.overcast, (s.overcast * w) >> 8);
	weather.windX = ClampAdd(weather.windX, (s.windX * w) >> 8);
	weather.windZ = ClampAdd(weather.windZ, (s.windZ * w) >> 8);
}

void storms::CalcAtmosAll(const glm::vec3& point, WeatherInfo& weather)
{
	for (const auto& storm : g_storms)
	{
		if (storm.deleteCounter == 0)
		{
			CalcAtmos(storm, point, weather);
		}
	}
}

void storms::ForEach(const std::function<void(const Storm&)>& function)
{
	for (const auto& storm : g_storms)
	{
		function(storm);
	}
}

void storms::Clear()
{
	g_storms.clear();
}

void storms::SetForkCallback(LightningCallback callback)
{
	g_forkCallback = std::move(callback);
}

void storms::SetSheetCallback(LightningCallback callback)
{
	g_sheetCallback = std::move(callback);
}
