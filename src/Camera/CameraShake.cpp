/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CameraShake.h"

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>

#include "Common/GameRandom.h"

namespace openblack::camera_shake
{
namespace
{
std::vector<Checker>& List()
{
	static std::vector<Checker> list; // g_first [0xEB99A8] and the next pointers, newest first
	return list;
}

/// fn_004C2B90 (a - b, in place) then fn_004A1BA0: sqrt((z z + y y) + x x) (0x4A1BA8..0x4A1BB8)
float Distance(const glm::vec3& a, const glm::vec3& b)
{
	const auto d = a - b;
	return std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
}
} // namespace

void Create(float maxDistance, const glm::vec3& point, float amplitude, int32_t ms, bool yOnly)
{
	Checker checker;
	checker.maxDistance = maxDistance; // +0x04 (0x82107E)
	checker.point = point;             // +0x08 (0x821085..0x821095)
	checker.amplitude = amplitude;     // +0x14 (0x8210A0)
	checker.totalMs = ms;              // +0x18 (0x8210A7)
	checker.remainingMs = ms;          // +0x1C (0x8210AA)
	checker.yOnly = yOnly;             // +0x20 (0x8210AD)
	auto& list = List();
	list.insert(list.begin(), checker); // the head of g_first (0x82106D..0x821078)
}

void StartCameraShake(const glm::vec3& point, float radius, float amplitude, float seconds)
{
	// 0x68F406..0x68F41E: fld seconds; fmul [0x8AB228]; fstp a float; fld; fistp
	const float ms = seconds * k_MsPerSecond;
	Create(radius, point, amplitude, static_cast<int32_t>(std::lrint(ms)), false); // push 0 (0x68F426)
}

void Adjust(const glm::vec3& lastDrawn, glm::vec3& position, glm::vec3& target)
{
	const auto& list = List();
	if (list.empty()) // 0x8210CB; [0xC383B8] (0x8210D3) is 1 and never written
	{
		return;
	}
	// 0x8210E0..0x821195: the nearest to g_camera; a later one only when strictly nearer (fcom; test ah, 1)
	const Checker* nearest = &list.front();
	float best = Distance(nearest->point, lastDrawn);
	for (auto it = std::next(list.begin()); it != list.end(); ++it)
	{
		if (const float d = Distance(it->point, lastDrawn); d < best)
		{
			best = d;
			nearest = &*it;
		}
	}
	if (!(best < nearest->maxDistance)) // 0x821197..0x8211A3
	{
		return;
	}
	if (nearest->totalMs == 0)
	{
		// Not original: a shake of 0 ms divides 0 by 0 (fidiv 0x8211B1) and draws a NaN camera until Tick frees it; the
		// original's frame is not reproduced here
		return;
	}
	// fild +0x1C; fidiv +0x18; fmul +0x14
	const float a = static_cast<float>(nearest->remainingMs) / static_cast<float>(nearest->totalMs) * nearest->amplitude;
	if (nearest->yOnly) // 0x8211AC..0x8211CF
	{
		position.y += game_random::crt::Random(-a, a); // 0x8211D1..0x8211DF
		target.y += game_random::crt::Random(-a, a);   // 0x8211E2..0x8211F0
		return;
	}
	// Six draws, kept in this order (0x8211F7..0x821264)
	const float positionZ = game_random::crt::Random(-a, a);
	const float positionY = game_random::crt::Random(-a, a);
	const float positionX = game_random::crt::Random(-a, a);
	position.x += positionX;
	position.y += positionY;
	position.z += positionZ;
	const float targetZ = game_random::crt::Random(-a, a);
	const float targetY = game_random::crt::Random(-a, a);
	const float targetX = game_random::crt::Random(-a, a);
	target.x += targetX;
	target.y += targetY;
	target.z += targetZ;
}

void Tick(uint32_t frameMs)
{
	auto& list = List();
	// 0x821287..0x8212CE: sub ecx, g_delta_time; jg keeps it
	for (auto& checker : list)
	{
		checker.remainingMs -= static_cast<int32_t>(frameMs);
	}
	list.erase(std::remove_if(list.begin(), list.end(), [](const Checker& c) { return c.remainingMs <= 0; }), list.end());
}

void Reset()
{
	List().clear();
}

const std::vector<Checker>& Checkers()
{
	return List();
}

} // namespace openblack::camera_shake
