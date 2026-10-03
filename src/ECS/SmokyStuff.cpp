/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SmokyStuff.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"

namespace openblack::ecs::smoky_stuff
{
namespace
{
std::vector<Cloud> g_clouds;
} // namespace

void Create(const glm::vec3& position, int32_t mode, float size, uint32_t colour)
{
	Cloud cloud;
	cloud.life = 1.0f;
	cloud.mode = mode;
	cloud.size = size;
	cloud.colour = colour;
	for (auto& puff : cloud.puffs)
	{
		// the order of the Random calls (0x81D180) of 0x823D39..0x823D80
		const float a = game_random::crt::Random(-size, size);
		const float b = game_random::crt::Random(-size, size);
		const float c = game_random::crt::Random(-size, size);
		puff.position = glm::vec3(c, b, a);
		puff.angle = game_random::crt::Random(0.0f, 6.2831855f);
		const float d = game_random::crt::Random(-size, size);
		const float e = game_random::crt::Random(-size, size);
		puff.velocity = glm::vec3(e, size, d);
		puff.cell = 0x10;
		if (mode == 0)
		{
			// 0x823E8A: Random(0.3, 1) x size along the direction (left as it is when it is 0)
			const float speed = game_random::crt::Random(0.3f, 1.0f) * size;
			const float length = glm::length(puff.velocity);
			if (length > 0.0f)
			{
				puff.velocity *= speed / length;
			}
		}
		else if (const float length = glm::length(puff.velocity); length > 0.0f)
		{
			// 0x823DA7..0x823E18: mode != 0 (ground_marks, RootsPile 0x8252EB): 1.5 x size along the direction ([0x8AB24C])
			puff.velocity *= 1.5f * size / length;
		}
		puff.position += position;
	}
	g_clouds.push_back(cloud);
}

void Update(float seconds)
{
	for (auto& cloud : g_clouds)
	{
		// fn_00823F70
		cloud.life -= seconds * (cloud.mode != 0 ? 0.666667f : 0.333333f);
		if (cloud.life <= 0.0f)
		{
			continue;
		}
		uint32_t argb = 0;
		if (cloud.mode != 0)
		{
			const float a = cloud.life < 0.7f ? cloud.life * 1.42857f * 255.0f : 255.0f;
			argb = (static_cast<uint32_t>(static_cast<int32_t>(a)) << 24) | 0x68503Du;
		}
		else
		{
			argb = (static_cast<uint32_t>(static_cast<int32_t>(cloud.life * 100.0f)) << 24) | 0x808080u;
		}
		if (cloud.colour != 0xFFFFFFFFu)
		{
			argb = (argb & 0xFF000000u) | (cloud.colour & 0x00FFFFFFu);
		}
		for (auto& puff : cloud.puffs)
		{
			puff.argb = argb;
			const float sign = puff.velocity.x > puff.velocity.z ? -1.0f : 1.0f;
			puff.angle = sign * cloud.life * 5.0f + puff.velocity.x;
			puff.half = std::max(((1.0f - cloud.life) * 2.0f + 1.0f) * cloud.size * 0.5f, 0.0001f);
			puff.position += puff.velocity * seconds;
			puff.cell = graphics::frame_anim::SmokyStuffCell(cloud.life); // fn_00823F70 0x8240F9..0x824115
		}
	}
	// fn_00824140: freed when life < 0
	std::erase_if(g_clouds, [](const Cloud& cloud) { return cloud.life < 0.0f; });
}

const std::vector<Cloud>& Get()
{
	return g_clouds;
}

void Clear()
{
	g_clouds.clear();
}

} // namespace openblack::ecs::smoky_stuff
