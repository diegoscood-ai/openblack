/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "DispenserRules.h"

#include <cmath>

#include <algorithm>
#include <array>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "InfoConstants.h"

using namespace openblack::magic;

namespace
{
constexpr std::array k_DispensableMiracles {
    openblack::MagicType::Fireball,
    openblack::MagicType::FireballPowerUpOne,
    openblack::MagicType::FireballPowerUpTwo,
    openblack::MagicType::LightningBolt,
    openblack::MagicType::LightningBoltPowerUpOne,
    openblack::MagicType::LightningBoltPowerUpTwo,
    openblack::MagicType::ExplosionOne,
    openblack::MagicType::ExplosionOnePuOne,
    openblack::MagicType::ExplosionOnePuTwo,
    openblack::MagicType::Heal,
    openblack::MagicType::HealPowerUpOne,
    openblack::MagicType::Teleport,
    openblack::MagicType::Forest,
    openblack::MagicType::Food,
    openblack::MagicType::FoodPowerUpOne,
    openblack::MagicType::StormWindRain,
    openblack::MagicType::StormWindRainLightning,
    openblack::MagicType::Tornado,
    openblack::MagicType::Shield,
    openblack::MagicType::PhysicalShield,
    openblack::MagicType::Wood,
    openblack::MagicType::Water,
    openblack::MagicType::WaterPowerUpOne,
    openblack::MagicType::FlockFlying,
    openblack::MagicType::FlockGround,
    openblack::MagicType::CreatureSpellFreeze,
    openblack::MagicType::CreatureSpellSmall,
    openblack::MagicType::CreatureSpellBig,
    openblack::MagicType::CreatureSpellWeak,
    openblack::MagicType::CreatureSpellStrong,
    openblack::MagicType::CreatureSpellInvisible,
    openblack::MagicType::CreatureSpellCompassion,
    openblack::MagicType::CreatureSpellAngry,
    openblack::MagicType::CreatureSpellItchy,
};
} // namespace

std::span<const openblack::MagicType> openblack::magic::DispensableMiracles()
{
	return k_DispensableMiracles;
}

DispenserStep openblack::magic::StepDispenser(DispenserTimer& timer, bool hasOrb, bool orbStillThere, bool hasMagic)
{
	// A bubble still on it: wait. Taken: forget it, and count from 0 on the next turn
	if (hasOrb)
	{
		if (orbStillThere)
		{
			return DispenserStep::Wait;
		}
		timer.tick = 0;
		return DispenserStep::OrbTaken;
	}
	// Active and with a miracle (built and repaired too, which openblack's abodes always are)
	if (!timer.active || !hasMagic)
	{
		return DispenserStep::Wait;
	}
	// One more turn; at the period a bubble
	if (++timer.tick >= timer.period)
	{
		return DispenserStep::MakeOrb;
	}
	return DispenserStep::Wait;
}

glm::mat3 openblack::magic::FaceTowards(const glm::vec3& direction)
{
	const float length = glm::length(direction);
	if (length <= 0.0f)
	{
		return glm::mat3(1.0f);
	}
	const auto up = direction / length;
	// Any axis not along the direction to build the other two from
	const auto reference = std::abs(up.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
	const auto right = glm::normalize(glm::cross(reference, up));
	const auto forward = glm::cross(right, up);
	return {right, up, forward};
}

glm::vec3 openblack::magic::OrbPosition(const glm::vec3& base, float height)
{
	return base + glm::vec3(0.0f, height * k_OrbHeightShare, 0.0f);
}

bool openblack::magic::OrbStillThere(const glm::vec3& orb, const glm::vec3& made)
{
	return glm::distance(glm::vec2(orb.x, orb.z), glm::vec2(made.x, made.z)) <= k_OrbStillThereDistance;
}

std::vector<openblack::AbodeInfo> openblack::magic::DispenserAbodes(const InfoConstants& info)
{
	std::vector<AbodeInfo> abodes;
	for (size_t i = 0; i < info.abode.size(); ++i)
	{
		if (info.abode.at(i).abodeType == AbodeType::SpellDispenser)
		{
			abodes.push_back(static_cast<AbodeInfo>(i));
		}
	}
	return abodes;
}

std::optional<openblack::AbodeInfo> openblack::magic::DefaultDispenserAbode(std::span<const AbodeInfo> abodes)
{
	if (abodes.empty())
	{
		return std::nullopt;
	}
	if (std::ranges::find(abodes, AbodeInfo::NorseSpellDispenser) != abodes.end())
	{
		return AbodeInfo::NorseSpellDispenser;
	}
	return abodes.front();
}
