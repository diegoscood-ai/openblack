/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PSysWaterRings.h"

#include "ECS/SeaCells.h"
#include "ECS/WaterRings.h"

namespace openblack::psys::water_rings
{
namespace
{
/// The fields both creators write: +0x10 age 0, +0x18 growth, +0x20 angle 0, +0x24 = +0x28 aspect = +0x2C rate = 1,
/// +0x30 cell 0x30, +0x34 0xFFFFFFFF. +0x1C (the drift) is left as the slot had it, see WaterRing.
bool AddRing(const glm::vec3& position, float growth)
{
	ecs::WaterRing ring;
	ring.position = position;
	ring.age = 0;
	ring.growth = growth;
	ring.angle = 0.0f;
	ring.aspect = 1.0f;
	ring.rate = 1.0f;
	ring.cell = 0x30;
	ring.argb = 0xFFFFFFFFu;
	return ecs::AddWaterRing(ring);
}
} // namespace

bool AddExplosionRings(const glm::vec3& point)
{
	if (ecs::sea_cells::IsDryLand(point))
	{
		return false;
	}
	AddRing(point, k_ExplosionRingGrowth * 0.5f); // 0x67E39F
	AddRing(point, k_ExplosionRingGrowth * 0.7f); // 0x67E43A
	AddRing(point, k_ExplosionRingGrowth);        // 0x67E4D1
	return true;
}

bool AddParticleRipple(const glm::vec3& position, float atomRadius, glm::vec3& lastRipple, float minDistance)
{
	if (!ecs::sea_cells::IsWater(position))
	{
		return false;
	}
	// 0x6A176F..0x6A179B: minDistance^2 < dx^2 + dz^2
	const float dx = lastRipple.x - position.x;
	const float dz = lastRipple.z - position.z;
	if (!(minDistance * minDistance < dx * dx + dz * dz))
	{
		return false;
	}
	lastRipple = position;
	return AddRing(position, atomRadius * 4.0f); // 0x6A17CE
}

} // namespace openblack::psys::water_rings
